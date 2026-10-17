#pragma once
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <dwmapi.h>
#include <cwchar>
#include <iterator>
#include "verify-latest.h"
#include "theme-notify-latest.h"

// The system theme manager ABI is private. It is called only in a short-lived
// --theme worker, with the implementation hash pinned, never inside Explorer
// or ShellHost. ABI reference: Auto Theme Switcher (GPL-3.0); see NOTICE.md.
namespace LeanDockTheme {
inline constexpr wchar_t Personalize[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
inline constexpr wchar_t Themes[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes";
inline bool ReadModes(DWORD& apps,DWORD& system) {
    DWORD n=sizeof(DWORD);
    if(RegGetValueW(HKEY_CURRENT_USER,Personalize,L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&apps,&n)!=ERROR_SUCCESS)return false;
    n=sizeof(DWORD);
    if(RegGetValueW(HKEY_CURRENT_USER,Personalize,L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&system,&n)!=ERROR_SUCCESS||apps>1||system>1)return false;
    return true;
}
inline bool ReadDark(bool& dark) {
    DWORD apps=2,system=2;if(!ReadModes(apps,system))return false;
    dark=apps==0&&system==0;return true;
}
inline bool Paths(bool dark,wchar_t (&theme)[MAX_PATH],wchar_t (&wallpaper)[MAX_PATH]) {
    wchar_t windows[MAX_PATH]{};if(!GetWindowsDirectoryW(windows,std::size(windows)))return false;
    if(_snwprintf_s(theme,_TRUNCATE,L"%ls\\Resources\\Themes\\%ls.theme",windows,dark?L"dark":L"aero")<0)return false;
    if(_snwprintf_s(wallpaper,_TRUNCATE,L"%ls\\Web\\Wallpaper\\Windows\\%ls.jpg",windows,dark?L"img19":L"img0")<0)return false;
    return GetFileAttributesW(theme)!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(wallpaper)!=INVALID_FILE_ATTRIBUTES;
}
inline bool Verified(bool dark,const wchar_t* theme,const wchar_t* wallpaper) {
    DWORD apps=2,system=2;if(!ReadModes(apps,system)||apps!=DWORD(!dark)||system!=DWORD(!dark))return false;
    wchar_t path[32768]{};DWORD n=sizeof(path);
    if(RegGetValueW(HKEY_CURRENT_USER,Themes,L"CurrentTheme",RRF_RT_REG_SZ,nullptr,path,&n)!=ERROR_SUCCESS||_wcsicmp(path,theme))return false;
    n=sizeof(path);
    return RegGetValueW(HKEY_CURRENT_USER,L"Control Panel\\Desktop",L"WallPaper",RRF_RT_REG_SZ,nullptr,path,&n)==ERROR_SUCCESS&&!_wcsicmp(path,wallpaper);
}
struct Manager: IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Reserved1()=0;
    virtual HRESULT STDMETHODCALLTYPE ApplyTheme(BSTR)=0;
};
inline DWORD WINAPI NotifyVisible(void*) {
    NotifyTaskbars();
    const auto started=GetTickCount64();
    EnumWindows([](HWND window,LPARAM context)->BOOL {
        if(GetTickCount64()-*reinterpret_cast<const ULONGLONG*>(context)>=75)return FALSE;
        DWORD cloaked=0;
        if(IsWindowVisible(window)&&!IsIconic(window)&&
           SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&!cloaked)
            SendMessageTimeoutW(window,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),SMTO_ABORTIFHUNG,20,nullptr);
        return TRUE;
    },reinterpret_cast<LPARAM>(&started));
    return 0;
}
inline bool WriteMode(const wchar_t* name,DWORD value) {
    return RegSetKeyValueW(HKEY_CURRENT_USER,Personalize,name,REG_DWORD,&value,sizeof(value))==ERROR_SUCCESS;
}
// Make the real appearance preference available immediately. A bounded helper
// notifies visible windows in parallel with full theme application; the system
// theme manager still applies wallpaper, styles and sounds. A full native
// appearance broadcast follows verified completion, including hidden listeners
// skipped here. No optimistic success, timer, or resident helper is involved.
struct Appearance {
    DWORD apps=2,system=2,target=2;bool changed=false;HANDLE notify=nullptr;
    wchar_t previousTheme[32768]{};
    explicit Appearance(bool dark):target(dark?0:1) {
        DWORD size=sizeof(previousTheme);
        if(!ReadModes(apps,system)||RegGetValueW(HKEY_CURRENT_USER,Themes,L"CurrentTheme",RRF_RT_REG_SZ,nullptr,previousTheme,&size)!=ERROR_SUCCESS)return;
        if(apps==target&&system==target)return;
        if(!WriteMode(L"AppsUseLightTheme",target))return;
        if(!WriteMode(L"SystemUsesLightTheme",target)){WriteMode(L"AppsUseLightTheme",apps);return;}
        changed=true;notify=CreateThread(nullptr,0,NotifyVisible,nullptr,0,nullptr);
    }
    void Join(){if(notify){WaitForSingleObject(notify,1000);CloseHandle(notify);notify=nullptr;}}
    void RestoreOnFailure() {
        Join();if(!changed)return;
        DWORD currentApps=2,currentSystem=2;wchar_t currentTheme[32768]{};DWORD size=sizeof(currentTheme);
        // Do not overwrite a newer theme selected elsewhere while this worker
        // was applying. Roll back only the pre-applied preferences we still own.
        if(ReadModes(currentApps,currentSystem)&&currentApps==target&&currentSystem==target&&
           RegGetValueW(HKEY_CURRENT_USER,Themes,L"CurrentTheme",RRF_RT_REG_SZ,nullptr,currentTheme,&size)==ERROR_SUCCESS&&
           !_wcsicmp(currentTheme,previousTheme)){
            WriteMode(L"AppsUseLightTheme",apps);WriteMode(L"SystemUsesLightTheme",system);BroadcastAppearance();
        }
    }
    ~Appearance(){Join();}
};
inline int Apply(bool dark) {
    HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\LeanDock.ThemeSwitch.v1");
    if(!mutex)return 70;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mutex);return 70;}
    struct Lock {HANDLE h;~Lock(){ReleaseMutex(h);CloseHandle(h);}} lock{mutex};
    wchar_t theme[MAX_PATH]{},wallpaper[MAX_PATH]{},library[MAX_PATH]{},hash[65]{};
    if(!Paths(dark,theme,wallpaper))return 71;
    GetSystemDirectoryW(library,std::size(library));wcscat_s(library,L"\\themeui.dll");
    if(!NativeHashFile(library,hash)||_wcsicmp(hash,L"2AA99876194E8EE59D806A92DD740348279B6F1B704B4AE5A4E6BDB89552CD51"))return 72;
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 73;
    struct Apartment {~Apartment(){CoUninitialize();}} apartment;
    constexpr CLSID clsid={0xC04B329E,0x5823,0x4415,{0x9C,0x93,0xBA,0x44,0x68,0x89,0x47,0xB0}};
    constexpr IID iid={0x0646EBBE,0xC1B7,0x4045,{0x8F,0xD0,0xFF,0xD6,0x5D,0x3F,0xC7,0x92}};
    HMODULE module=LoadLibraryExW(library,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)return 74;
    struct Library {HMODULE h;~Library(){FreeLibrary(h);}} lib{module};
    auto factoryFunction=reinterpret_cast<HRESULT (WINAPI*)(REFCLSID,REFIID,void**)>(GetProcAddress(module,"DllGetClassObject"));
    IClassFactory* factory=nullptr;
    if(!factoryFunction||FAILED(factoryFunction(clsid,IID_IClassFactory,reinterpret_cast<void**>(&factory))))return 75;
    Manager* manager=nullptr;HRESULT hr=factory->CreateInstance(nullptr,iid,reinterpret_cast<void**>(&manager));factory->Release();
    if(FAILED(hr)||!manager)return 76;
    BSTR path=SysAllocString(theme);if(!path){manager->Release();return 77;}
    Appearance appearance(dark);
    hr=manager->ApplyTheme(path);SysFreeString(path);manager->Release();
    if(FAILED(hr)){appearance.RestoreOnFailure();return 78;}
    // A bounded completion check for the one requested transition, not a
    // resident monitor. Pre-applying mode values can make the theme manager
    // skip a redundant appearance broadcast. Resync ALL top-level listeners
    // AFTER the full theme is committed and the fast pass has finished. Hidden
    // shell/app listener windows must not retain the previous appearance; a
    // taskbar-only refresh was observed to recover and then revert to Dark.
    for(int i=0;i<40;++i){
        if(Verified(dark,theme,wallpaper)){
            appearance.Join();BroadcastAppearance();return 0;
        }
        Sleep(50);
    }
    return 79;
}
}
