#pragma once
#include <windows.h>
#include <cwchar>
#include <iterator>
#include "start-host-latest.h"
#include "quick-host-latest.h"

// Discovery only. Both native UI threads adjust pending placement themselves;
// no SHOW -> timer -> second move, and no restoration during HIDE/CLOAKED.
namespace LeanDockPanels {
inline HWINEVENTHOOK hooks[3]{};
inline bool active;
inline unsigned failures;
inline int Kind(HWND w) {
    if(!w || GetAncestor(w,GA_ROOT)!=w)return -1;
    wchar_t cls[96]{};GetClassNameW(w,cls,std::size(cls));
    const bool quick=!wcscmp(cls,L"ControlCenterWindow");
    if(!quick && wcscmp(cls,L"Windows.UI.Core.CoreWindow"))return -1;
    DWORD pid=0;GetWindowThreadProcessId(w,&pid);
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return -1;
    wchar_t path[32768]{},expected[32768]{};DWORD length=std::size(path);
    bool ok=QueryFullProcessImageNameW(p,0,path,&length);CloseHandle(p);
    GetWindowsDirectoryW(expected,std::size(expected));
    wcscat_s(expected,quick?L"\\System32\\ShellHost.exe":L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\StartMenuExperienceHost.exe");
    return ok && !_wcsicmp(path,expected)?(quick?1:0):-1;
}
inline void Discover(HWND w) {
    if(!active)return;
    switch(Kind(w)){case 0:LeanDockStart::Attach(w);break;case 1:LeanDockQuickHost::Attach(w);break;}
}
inline void CALLBACK Event(HWINEVENTHOOK,DWORD event,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(!active || object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
    if(event==EVENT_OBJECT_CREATE || event==EVENT_OBJECT_SHOW || event==EVENT_OBJECT_UNCLOAKED)Discover(w);
}
inline void Stop() {
    // Close admission before unhooking: already queued events become no-ops.
    active=false;
    for(auto& hook:hooks)if(hook){UnhookWinEvent(hook);hook=nullptr;}
}
inline bool Start(HWND) {
    Stop();active=true;
    const DWORD events[]={EVENT_OBJECT_CREATE,EVENT_OBJECT_SHOW,EVENT_OBJECT_UNCLOAKED};
    for(int i=0;i<3;++i){
        hooks[i]=SetWinEventHook(events[i],events[i],nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        if(!hooks[i]){++failures;Stop();return false;}
    }
    HWND w=nullptr;while((w=FindWindowExW(nullptr,w,nullptr,nullptr)))Discover(w);
    return true;
}
}
