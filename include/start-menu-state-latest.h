#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <cwchar>

// Read-only discovery, factored from the existing native panel host check.
// A CoreWindow's WS_VISIBLE survives closure; the DWM cloak must be checked.
// Neither a title nor the generic CoreWindow class establishes shell identity.
// Search is a separate host: it must not be mistaken for a general application
// when retaining the Dock or dismissing a shell panel before a corner action.
namespace LeanDockStartState {
enum class Kind { None, Start, Search };
inline Kind Identify(HWND window) {
    if(!window || GetAncestor(window,GA_ROOT)!=window)return Kind::None;
    wchar_t cls[96]{};GetClassNameW(window,cls,96);
    if(wcscmp(cls,L"Windows.UI.Core.CoreWindow"))return Kind::None;
    DWORD pid{};GetWindowThreadProcessId(window,&pid);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return Kind::None;
    wchar_t path[32768]{},expected[32768]{};DWORD length=32768;
    const bool ok=QueryFullProcessImageNameW(process,0,path,&length)!=FALSE;
    CloseHandle(process);
    GetWindowsDirectoryW(expected,32768);
    wcscat_s(expected,L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\StartMenuExperienceHost.exe");
    if(ok&&!_wcsicmp(path,expected))return Kind::Start;
    GetWindowsDirectoryW(expected,32768);
    wcscat_s(expected,L"\\SystemApps\\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\\SearchHost.exe");
    return ok&&!_wcsicmp(path,expected)?Kind::Search:Kind::None;
}
inline bool Host(HWND window) {return Identify(window)!=Kind::None;}
inline bool Shown(HWND window) {
    if(!window||!IsWindowVisible(window)||IsIconic(window))return false;
    DWORD cloaked=0;RECT r{};
    return SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&
        !cloaked&&GetWindowRect(window,&r)&&r.right-r.left>1&&r.bottom-r.top>1;
}
inline HWND Visible() {
    HWND w=nullptr;
    while((w=FindWindowExW(nullptr,w,L"Windows.UI.Core.CoreWindow",nullptr)))
        if(Shown(w)&&Host(w))return w;
    return nullptr;
}
inline HWND Foreground() {
    HWND w=GetForegroundWindow();
    return Shown(w)&&Host(w)?w:nullptr;
}
}
