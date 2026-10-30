#pragma once
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <psapi.h>
#include <wtsapi32.h>
#include <winrt/base.h>
#include <string>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <bcrypt.h>
#include "policy-latest.h"
#include "../include/dock-geometry-latest.h"
#include "../include/start-menu-state-latest.h"
namespace Capsule {
inline std::wstring Sha256(std::wstring const& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    DWORD length=0,returned=0;std::wstring result;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&
       BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&returned,0)>=0){
        std::vector<BYTE> object(length);
        if(BCryptCreateHash(algorithm,&hash,object.data(),length,nullptr,0,0)>=0){
            BYTE bytes[32768];DWORD n=0;bool ok=true;
            while(ok){
                if(!ReadFile(file,bytes,sizeof(bytes),&n,nullptr)){ok=false;break;}
                if(!n)break;
                ok=BCryptHashData(hash,bytes,n,0)>=0;
            }
            BYTE digest[32]{};
            if(ok&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0){
                for(auto c:digest){wchar_t b[3];swprintf_s(b,L"%02X",c);result+=b;}
            }
            BCryptDestroyHash(hash);
        }
    }
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);return result;
}
inline constexpr wchar_t WindowClass[]=L"NativeTaskbarDock.Capsule.v1";
inline constexpr wchar_t GuardClass[]=L"NativeTaskbarDock.Capsule.Guard.v1";
inline constexpr UINT ToggleMessage=WM_APP+20, EscapeMessage=WM_APP+21,
    TaskbarMessage=WM_APP+22, QuitMessage=WM_APP+23;
inline std::wstring Directory() {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    return std::filesystem::path(path).parent_path().wstring();
}
inline std::wstring Executable() {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return path;
}
inline void Log(wchar_t const* event,long code=0) {
    auto path=Directory()+L"\\latest.log";
    FILE* f=_wfopen(path.c_str(),L"a");if(!f)return;
    SYSTEMTIME t{};GetLocalTime(&t);
    fwprintf(f,L"%04u-%02u-%02u %02u:%02u:%02u pid=%lu %ls code=%ld\n",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId(),event,code);
    fclose(f);
}
inline bool Elevated() {
    HANDLE t=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t))return true;
    TOKEN_ELEVATION value{};DWORD bytes{};
    bool ok=GetTokenInformation(t,TokenElevation,&value,sizeof(value),&bytes);
    CloseHandle(t);return !ok||value.TokenIsElevated;
}
inline bool ExactProcess(HWND w,wchar_t const* expected) {
    DWORD pid{};GetWindowThreadProcessId(w,&pid);
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
    wchar_t path[32768]{};DWORD size=32768;
    bool ok=QueryFullProcessImageNameW(p,0,path,&size);CloseHandle(p);
    return ok&&_wcsicmp(path,expected)==0;
}
inline void ShowWithoutActivation(HWND window) {
    // A background launcher can set STARTF_USESHOWWINDOW / SW_HIDE. The first
    // ShowWindow call would obey that value instead of our explicit reveal.
    // This changes only our own window visibility and never its focus.
    if(!SetWindowPos(window,HWND_TOPMOST,0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW))winrt::throw_last_error();
}
inline std::vector<HWND> ExplorerBars() {
    wchar_t explorer[MAX_PATH]{};GetWindowsDirectoryW(explorer,MAX_PATH);
    wcscat_s(explorer,L"\\explorer.exe");std::vector<HWND> result;
    for(auto cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}){
        HWND w=nullptr;
        while((w=FindWindowExW(nullptr,w,cls,nullptr)))
            if(ExactProcess(w,explorer))result.push_back(w);
    }
    return result;
}
// Only called by a real click on the clock. SC_TASKLIST is the public system
// command for Start; it does not inject a Win key into an elevated foreground
// application. Resolve the real Explorer every time (not our tray relay).
// Dispatch success is deliberately distinct from visible Start acceptance.
inline bool RequestStartMenu(HWND owner) {
    auto count=reinterpret_cast<uintptr_t>(GetPropW(owner,L"Capsule.StartRequests"));
    SetPropW(owner,L"Capsule.StartRequests",HANDLE(count+1));
    DWORD error=ERROR_NOT_FOUND;
    for(auto bar:ExplorerBars()) {
        wchar_t cls[64]{};GetClassNameW(bar,cls,64);
        if(wcscmp(cls,L"Shell_TrayWnd"))continue;
        SetLastError(ERROR_SUCCESS);
        bool sent=PostMessageW(bar,WM_SYSCOMMAND,SC_TASKLIST,0)!=FALSE;
        error=sent?ERROR_SUCCESS:GetLastError();
        SetPropW(owner,L"Capsule.StartDispatch",HANDLE(uintptr_t(sent?1:2)));
        SetPropW(owner,L"Capsule.StartError",HANDLE(uintptr_t(error)));
        Log(sent?L"start-command-dispatched":L"start-command-failed",error);return sent;
    }
    SetPropW(owner,L"Capsule.StartDispatch",HANDLE(2));
    SetPropW(owner,L"Capsule.StartError",HANDLE(uintptr_t(error)));
    Log(L"start-shell-unavailable",error);return false;
}
inline UINT AppBarState() {
    APPBARDATA a{sizeof(a)};auto bars=ExplorerBars();
    if(!bars.empty())a.hWnd=bars.front();
    return static_cast<UINT>(SHAppBarMessage(ABM_GETSTATE,&a));
}
inline void SetAppBarState(UINT state) {
    APPBARDATA a{sizeof(a)};auto bars=ExplorerBars();
    if(!bars.empty())a.hWnd=bars.front();a.lParam=state;
    SHAppBarMessage(ABM_SETSTATE,&a);
}
inline void RestoreTaskbar(UINT state) {
    for(auto w:ExplorerBars()){
        if(GetPropW(w,L"Capsule.GeometryOwner")){
            LeanDockGeometry::Clear(w);RemovePropW(w,L"Capsule.GeometryOwner");
        }
    }
    SetAppBarState(state);
    for(auto w:ExplorerBars())ShowWindowAsync(w,SW_SHOWNOACTIVATE);
    Log(L"taskbar-restored",state);
}
inline void HideTaskbar(UINT state) {
    SetAppBarState(state|ABS_AUTOHIDE);
    for(auto w:ExplorerBars())ShowWindowAsync(w,SW_HIDE);
}
inline void Journal(bool active,UINT state) {
    auto p=Directory()+L"\\recovery-latest.ini";
    if(active)WritePrivateProfileStringW(L"restore",L"state",std::to_wstring(state).c_str(),p.c_str());
    WritePrivateProfileStringW(L"restore",L"active",active?L"1":L"0",p.c_str());
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,p.c_str());
}
inline bool Animations() {
    BOOL enabled=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);return enabled;
}
inline bool Fullscreen() {
    QUERY_USER_NOTIFICATION_STATE state{};
    if(SUCCEEDED(SHQueryUserNotificationState(&state))&&
       (state==QUNS_BUSY||state==QUNS_RUNNING_D3D_FULL_SCREEN||state==QUNS_PRESENTATION_MODE))return true;
    auto w=GetForegroundWindow();if(!w||IsZoomed(w))return false;
    wchar_t cls[128]{};GetClassNameW(w,cls,128);
    if(!wcscmp(cls,L"Progman")||!wcscmp(cls,L"WorkerW")||
       !wcscmp(cls,WindowClass)||!wcscmp(cls,L"Shell_TrayWnd"))return false;
    MONITORINFO m{sizeof(m)};GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST),&m);
    RECT r{};GetWindowRect(w,&r);
    return r.left<=m.rcMonitor.left&&r.top<=m.rcMonitor.top&&
           r.right>=m.rcMonitor.right&&r.bottom>=m.rcMonitor.bottom;
}
inline PROCESS_INFORMATION Spawn(std::wstring args) {
    auto exe=Executable();std::wstring cmd=L"\""+exe+L"\" "+args;
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION p{};
    startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT,
                       nullptr,Directory().c_str(),&startup,&p))Log(L"create-process-failed",GetLastError());
    if(p.hThread){CloseHandle(p.hThread);p.hThread=nullptr;}
    return p;
}
inline HWND FindOwnedWindow(wchar_t const* cls,DWORD pid) {
    HWND w=nullptr;while((w=FindWindowExW(nullptr,w,cls,nullptr))){
        DWORD owner{};GetWindowThreadProcessId(w,&owner);if(owner==pid)return w;
    }return nullptr;
}
}

