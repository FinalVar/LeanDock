#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <cwchar>
#include <iterator>
#include "dock-geometry-latest.h"
#include "start-host-latest.h"
#include "quick-host-latest.h"

// Native Start and Quick Settings remain in their own system processes.
// Start retains its established position/layout path. Quick Settings adjusts
// pending native placement inside its host, never after SHOW/UNCLOAKED.
namespace LeanDockPanels {
inline constexpr UINT Message=WM_APP+224, Timer=3;
struct Panel { HWND window{}; RECT original{},applied{}; bool moved{},pending{}; };
inline Panel panels[2];
inline HWINEVENTHOOK hooks[4]{};
inline HWND owner;
inline unsigned moves[2]{},failures;
inline bool queued;
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
inline bool Visible(HWND w) {
    DWORD cloak=0;return IsWindowVisible(w) &&
        SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloak,sizeof(cloak))) && !cloak;
}
inline HWND TaskbarFor(HWND panel) {
    const auto monitor=MonitorFromWindow(panel,MONITOR_DEFAULTTONEAREST);
    for(auto cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}) {
        HWND w=nullptr;
        while((w=FindWindowExW(nullptr,w,cls,nullptr)))
            if(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST)==monitor)return w;
    }
    return nullptr;
}
inline void Position(int kind) {
    if(kind!=0)return;
    auto& panel=panels[kind];panel.pending=false;
    HWND w=panel.window;
    if(Kind(w)!=kind || !Visible(w))return;
    RECT dock{},r{};MONITORINFO monitor{};
    HWND bar=TaskbarFor(w);
    if(!bar || !LeanDockGeometry::Read(bar,dock,&monitor) || !GetWindowRect(w,&r))return;
    const LONG width=r.right-r.left,height=r.bottom-r.top;
    if(width<100 || height<100 || width>monitor.rcMonitor.right-monitor.rcMonitor.left ||
       height>monitor.rcMonitor.bottom-monitor.rcMonitor.top)return;
    const UINT dpi=GetDpiForWindow(w);
    // Both hosts include the native 12-DIP panel inset. Positioning their
    // bottom four DIPs below the Dock top leaves an eight-DIP visible gap.
    auto point=LeanDockGeometry::HostPosition(dock,monitor.rcMonitor,width,height,dpi?dpi:96,kind!=0);
    LONG x=point.x,y=point.y;
    if(r.left==x && r.top==y)return;
    if(!panel.moved)panel.original=r;
    if(SetWindowPos(w,nullptr,x,y,0,0,SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE|SWP_NOOWNERZORDER)) {
        panel.applied={x,y,x+width,y+height};panel.moved=true;++moves[kind];
    } else ++failures;
}
inline void Restore(Panel& panel) {
    RECT r{};
    if(panel.moved && Kind(panel.window)>=0 && GetWindowRect(panel.window,&r) && EqualRect(&r,&panel.applied))
        SetWindowPos(panel.window,nullptr,panel.original.left,panel.original.top,0,0,
                     SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE|SWP_NOOWNERZORDER);
    panel={};
}
inline void CALLBACK Event(HWINEVENTHOOK,DWORD event,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
    int kind=Kind(w);if(kind<0)return;
    if(kind==1){
        if(event==EVENT_OBJECT_CREATE||event==EVENT_OBJECT_SHOW||event==EVENT_OBJECT_UNCLOAKED)
            LeanDockQuickHost::Attach(w);
        return;
    }
    if(kind==0 && (event==EVENT_OBJECT_SHOW || event==EVENT_OBJECT_UNCLOAKED))LeanDockStart::Attach(w);
    auto& panel=panels[kind];
    if(event==EVENT_OBJECT_HIDE || event==EVENT_OBJECT_CLOAKED){Restore(panel);return;}
    if(panel.window!=w){Restore(panel);panel.window=w;}
    // Ignore our own location notification. Native size/placement changes still
    // schedule one update; there is no permanent timer or mouse polling.
    RECT r{};if(event==EVENT_OBJECT_LOCATIONCHANGE && panel.moved && GetWindowRect(w,&r) && EqualRect(&r,&panel.applied))return;
    panel.pending=true;
    if(!queued){queued=PostMessageW(owner,Message,0,0)!=FALSE;}
}
inline bool Start(HWND window) {
    owner=window;
    hooks[0]=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_HIDE,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    hooks[1]=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    hooks[2]=SetWinEventHook(EVENT_OBJECT_CLOAKED,EVENT_OBJECT_UNCLOAKED,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    hooks[3]=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_CREATE,nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    for(int i=0;i<4;i++)if(!hooks[i])return false;
    HWND w=nullptr;while((w=FindWindowExW(nullptr,w,nullptr,nullptr))){int k=Kind(w);if(k==0){panels[k].window=w;Position(k);}else if(k==1)LeanDockQuickHost::Attach(w);}
    return true;
}
inline bool Dispatch(UINT message,WPARAM p) {
    if(message==Message){queued=false;SetTimer(owner,Timer,30,nullptr);return true;}
    if(message!=WM_TIMER || p!=Timer)return false;
    KillTimer(owner,Timer);
    for(int i=0;i<2;i++)if(panels[i].pending)Position(i);
    return true;
}
inline void Stop() {
    for(auto& hook:hooks)if(hook){UnhookWinEvent(hook);hook=nullptr;}
    if(owner)KillTimer(owner,Timer);queued=false;
    for(auto& panel:panels)Restore(panel);owner=nullptr;
}
}
