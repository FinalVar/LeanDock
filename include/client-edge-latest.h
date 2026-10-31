#pragma once
#include <windows.h>
#include <appmodel.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <array>
#include <iterator>
#include "client-edge-policy-latest.h"
#include "fullscreen-app-policy-latest.h"
#include "dock-geometry-latest.h"
#include "start-attach-policy-latest.h"
#include "appbar-query-policy-latest.h"

// A maximized frameless Codex/ChatGPT window can be classified as fullscreen
// and put above the auto-hide taskbar. Use Microsoft's NonRudeHWND contract,
// then refresh that classification and the native taskbar's z-order.
// Other maximized windows need a measured resize obstruction or a full client
// under the active AppBar compatibility policy. True fullscreen applications
// that are not games or remote clients (fullscreen-app-policy-latest.h) get
// the same contract on demand only: while the pointer is on the hidden Dock's
// strip or the Dock is out. Windows re-detects fullscreen only on activation,
// so when the Dock is no longer wanted LeanDock hands the window back with
// MarkFullscreenWindow(TRUE) (Windows' fullscreen do-not-disturb and taskbar
// order return at once). That is held only while the window stays in front:
// once another window (the desktop after "show desktop", another app) takes
// the foreground it is lifted, and Windows judges the window itself on its
// next activation. No resizing or input synthesis.
namespace LeanDockClientEdge {
inline constexpr UINT Recheck=WM_APP+421;
// held: a fullscreen application handed back with MarkFullscreenWindow(TRUE)
// and the Held property instead of NonRudeHWND.
struct OwnedWindow {HWND window{};DWORD pid{};bool generic{},fullscreen{},held{};};
inline std::array<OwnedWindow,32> owned{};
inline std::array<HWINEVENTHOOK,6> hooks{};
inline HWND host;
inline ITaskbarList2* shell;
inline bool active,pending,comOwned,evaluating,again,stopping,enabled,initializing;
inline unsigned generation;
inline unsigned repairs,failures,genericRepairs,fullscreenRepairs,fullscreenHolds,initAttempts;
inline HWND candidate; // the fullscreen application in front at the last evaluation
// Set when the hot corners' raw input reaches Pointer(). Without it a
// fullscreen application stays marked while in front, as before 0.8.91.
inline bool pointerFeed;
inline HRESULT lastError;
inline const char* lastStage="not-started";
inline LeanDockStart::AttachPolicy initPolicy;
inline UINT_PTR retryTimer;
inline ULONGLONG retryDue;
inline HANDLE Token(){return reinterpret_cast<HANDLE>(host);}
inline DWORD Pid(HWND w){DWORD pid=0;GetWindowThreadProcessId(w,&pid);return pid;}
inline bool Maximized(HWND w) {
    if(!w || GetAncestor(w,GA_ROOT)!=w || (GetWindowLongPtrW(w,GWL_EXSTYLE)&(WS_EX_TOPMOST|WS_EX_TOOLWINDOW)))return false;
    RECT r{};MONITORINFO m{sizeof(m)};
    if(!GetWindowRect(w,&r) || !GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONULL),&m))return false;
    return LeanDockClientEdgePolicy::Eligible(IsWindowVisible(w),IsIconic(w),IsZoomed(w),GetWindowLongPtrW(w,GWL_STYLE),r,m.rcMonitor);
}
// A fullscreen window that is not a game or remote client. Explorer's own
// surfaces (desktop, taskbar, Task View) and shell experiences such as Start
// are never marked.
inline bool FullscreenApp(HWND w) {
    if(!w || GetAncestor(w,GA_ROOT)!=w || !IsWindowVisible(w) || IsIconic(w) || w==GetShellWindow() ||
       (GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW) || Pid(w)==Pid(FindWindowW(L"Shell_TrayWnd",nullptr)))return false;
    wchar_t cls[64]{};GetClassNameW(w,cls,std::size(cls));
    if(!wcscmp(cls,L"Windows.UI.Core.CoreWindow"))return false;
    RECT frame{};MONITORINFO m{sizeof(m)};
    if(!GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONULL),&m) ||
       (FAILED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))) && !GetWindowRect(w,&frame)))return false;
    return LeanDockFullscreen::Covers(GetWindowLongPtrW(w,GWL_STYLE),frame,m.rcMonitor) && !LeanDockFullscreen::Quiet(w);
}
// The pointer on the hidden Dock's on-screen strip of this window's monitor,
// or the Dock out: the only times a fullscreen application is marked. Any
// taskbar on that monitor counts (a native secondary one too); this only
// reads its position, and z-order repair stays limited to LeanDock's own.
inline bool DockWanted(HWND w) {
    if(!pointerFeed)return true;
    POINT p{};MONITORINFO m{sizeof(m)};
    const HMONITOR monitor=MonitorFromWindow(w,MONITOR_DEFAULTTONULL);
    if(!monitor || !GetCursorPos(&p) || !GetMonitorInfoW(monitor,&m))return false;
    for(const wchar_t* cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}) {
        HWND tray=nullptr;
        while((tray=FindWindowExW(nullptr,tray,cls,nullptr))) {
            RECT r{},region{};
            if(MonitorFromWindow(tray,MONITOR_DEFAULTTONULL)!=monitor || !GetWindowRect(tray,&r))continue;
            if(GetWindowRgnBox(tray,&region)==ERROR)region={0,0,r.right-r.left,r.bottom-r.top};
            if(LeanDockFullscreen::Wanted(r,region,p,m.rcMonitor))return true;
        }
    }
    return false;
}
inline bool Client(HWND w) {
    return LeanDockClientEdgePolicy::ClientWindow(w);
}
inline bool Tray(HWND w) {
    wchar_t cls[64]{};GetClassNameW(w,cls,std::size(cls));
    return !wcscmp(cls,L"Shell_TrayWnd") || !wcscmp(cls,L"Shell_SecondaryTrayWnd");
}
inline ULONG_PTR NativeBarQuery(UINT query,APPBARDATA& data) {
    HWND tray=FindWindowW(L"Shell_TrayWnd",nullptr);ULONG_PTR result=0;
    if(host && GetPropW(tray,LeanDockAppBar::HookReady)==reinterpret_cast<HANDLE>(uintptr_t(Pid(tray))) &&
        LeanDockAppBar::NativeWireQuery(tray,host,query,data.uEdge,data.rc,result))return result;
    return SHAppBarMessage(query,&data);
}
inline bool CompatibleFullClient(HWND w) {
    RECT client{},dock{};MONITORINFO monitor{sizeof(monitor)};
    const auto screen=MonitorFromWindow(w,MONITOR_DEFAULTTONULL);
    if(!screen || !GetMonitorInfoW(screen,&monitor) || !GetClientRect(w,&client))return false;
    SetLastError(0);
    if(!MapWindowPoints(w,nullptr,reinterpret_cast<POINT*>(&client),2) && GetLastError())return false;
    if(!LeanDockAppBar::FullMaximizedClient(GetWindowLongPtrW(w,GWL_STYLE),client,monitor.rcMonitor))return false;
    for(const wchar_t* cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}) {
        HWND tray=nullptr;
        while((tray=FindWindowExW(nullptr,tray,cls,nullptr))) {
            if(MonitorFromWindow(tray,MONITOR_DEFAULTTONULL)==screen &&
               GetPropW(tray,LeanDockAppBar::HookReady)==reinterpret_cast<HANDLE>(uintptr_t(Pid(tray))) &&
               LeanDockGeometry::Read(tray,dock))return true;
        }
    }
    return false;
}
inline bool GenericFrame(HWND w) {
    RECT frame{};MONITORINFO m{sizeof(m)};
    return CompatibleFullClient(w) || (GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONULL),&m) &&
        SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))) &&
        LeanDockClientEdgePolicy::BorderInset(GetWindowLongPtrW(w,GWL_EXSTYLE),frame,m.rcMonitor,GetDpiForWindow(w)));
}
inline bool ResizeObstruction(HWND w) {
    const auto monitor=MonitorFromWindow(w,MONITOR_DEFAULTTONULL);
    for(const wchar_t* cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}) {
        HWND tray=nullptr;
        while((tray=FindWindowExW(nullptr,tray,cls,nullptr))) {
            RECT dock{},r{},frame{},region{};MONITORINFO m{};
            if(MonitorFromWindow(tray,MONITOR_DEFAULTTONULL)!=monitor ||
               !LeanDockGeometry::Read(tray,dock,&m) || !GetWindowRect(tray,&r))continue;
            APPBARDATA bar{sizeof(bar)};bar.uEdge=ABE_BOTTOM;bar.rc=m.rcMonitor;
            if(reinterpret_cast<HWND>(NativeBarQuery(ABM_GETAUTOHIDEBAREX,bar))!=tray)continue;
            const POINT point{dock.left+(dock.right-dock.left)/2,m.rcMonitor.bottom-1};
            // A missing/off-screen clipped strip is a different bug. Never
            // compensate for it by changing unrelated application properties.
            const POINT local{point.x-r.left,point.y-r.top};
            if(!PtInRect(&r,point) || GetWindowRgnBox(tray,&region)==ERROR || !PtInRect(&region,local) ||
               GetAncestor(WindowFromPoint(point),GA_ROOT)!=w ||
               FAILED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))))continue;
            DWORD_PTR hit=0;
            if(!SendMessageTimeoutW(w,WM_NCHITTEST,0,MAKELPARAM(point.x,point.y),
                                   SMTO_ABORTIFHUNG|SMTO_BLOCK,50,&hit))continue;
            if(LeanDockClientEdgePolicy::ResizeObstruction(true,static_cast<LRESULT>(hit),
                GetWindowLongPtrW(w,GWL_EXSTYLE),frame,m.rcMonitor,GetDpiForWindow(w)))return true;
        }
    }
    return false;
}
inline bool AutoHide() {APPBARDATA data{sizeof(data)};return (NativeBarQuery(ABM_GETSTATE,data)&ABS_AUTOHIDE)!=0;}
inline bool Kept(const OwnedWindow& e) {
    // A fullscreen window behind the desktop or another app must not keep
    // Windows treating the screen as fullscreen.
    if(e.held)return e.window==GetForegroundWindow() && FullscreenApp(e.window);
    return e.fullscreen?FullscreenApp(e.window) && DockWanted(e.window):
        Maximized(e.window) && (!e.generic || GenericFrame(e.window));
}
inline bool Tracked(HWND w){for(const auto& e:owned)if(e.window==w)return true;return false;}
inline bool Revealed(HWND w){for(const auto& e:owned)if(e.window==w && !e.held)return true;return false;}
inline bool Owns(const OwnedWindow& e) {
    return Pid(e.window)==e.pid &&
        GetPropW(e.window,e.held?LeanDockClientEdgePolicy::Held:LeanDockClientEdgePolicy::Owner)==Token();
}
inline HRESULT MarkFullscreen(HWND w,BOOL fullscreen) {
    if(!shell)return E_UNEXPECTED;
    // Keep the COM object alive across a reentrant shutdown notification.
    ITaskbarList2* current=shell;current->AddRef();
    const HRESULT result=current->MarkFullscreenWindow(w,fullscreen);current->Release();
    return result;
}
inline void Forget(OwnedWindow& e) {
    // Properties die with the HWND. If the numeric handle was reused, do not
    // remove a new window's property or send it a shell notification.
    const OwnedWindow old=e;e={};
    if(Pid(old.window)!=old.pid)return;
    if(old.held) {
        if(GetPropW(old.window,LeanDockClientEdgePolicy::Held)!=Token())return;
        RemovePropW(old.window,LeanDockClientEdgePolicy::Held);
        MarkFullscreen(old.window,FALSE);return;
    }
    if(LeanDockClientEdgePolicy::Release(old.window,Token()))MarkFullscreen(old.window,FALSE);
}
// The Dock is no longer wanted over a revealed fullscreen application: give
// Windows its fullscreen treatment back now instead of at the next activation.
inline void Hold(OwnedWindow& e) {
    if(!LeanDockClientEdgePolicy::Release(e.window,Token()) || !SetPropW(e.window,LeanDockClientEdgePolicy::Held,Token())){
        ++failures;Forget(e);return;
    }
    e.held=true;
    if(FAILED(MarkFullscreen(e.window,TRUE))){++failures;Forget(e);return;}
    ++fullscreenHolds;
}
// A Held property from an earlier controller that ended without cleanup.
inline void Adopt(HWND w,bool fullscreen) {
    const HANDLE previous=GetPropW(w,LeanDockClientEdgePolicy::Held);
    if(!previous || previous==Token() || Tracked(w))return;
    if(fullscreen) {
        for(auto& e:owned)if(!e.window) {
            if(!SetPropW(w,LeanDockClientEdgePolicy::Held,Token()))break;
            e={w,Pid(w),false,true,true};return;
        }
    }
    RemovePropW(w,LeanDockClientEdgePolicy::Held);MarkFullscreen(w,FALSE);
}
inline bool TaskbarZOrder(HWND foreground,bool repair) {
    bool changed=false;
    const HMONITOR monitor=MonitorFromWindow(foreground,MONITOR_DEFAULTTONULL);
    for(const wchar_t* cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}) {
        HWND tray=nullptr;
        while((tray=FindWindowExW(nullptr,tray,cls,nullptr))) {
            // Only the taskbar already owned by the running LeanDock backend.
            if(MonitorFromWindow(tray,MONITOR_DEFAULTTONULL)!=monitor ||
               !GetPropW(tray,L"LeanDock.Geometry.v1.Ready") ||
               (GetWindowLongPtrW(tray,GWL_EXSTYLE)&WS_EX_TOPMOST))continue;
            if(!repair)return true;
            if(SetWindowPos(tray,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER))changed=true;
            else ++failures;
        }
    }
    return changed;
}
inline void Queue();
inline void Evaluate() {
    if(!active)return;
    if(evaluating){again=true;return;}
    evaluating=true;
    struct Guard {~Guard(){evaluating=false;if(again){again=false;Queue();}}} guard;
    const unsigned epoch=generation;
    const bool autoHide=AutoHide();
    const HWND front=GetForegroundWindow();
    for(auto& e:owned) {
        if(!e.window)continue;
        if(!autoHide || !Owns(e)){Forget(e);continue;}
        if(e.fullscreen && !e.held && e.window==front && FullscreenApp(e.window) && !DockWanted(e.window))Hold(e);
        else if(!Kept(e))Forget(e);
        if(!active || generation!=epoch)return;
    }
    HWND foreground=GetForegroundWindow();
    const bool maximized=Maximized(foreground),fullscreen=!maximized && FullscreenApp(foreground);
    Adopt(foreground,autoHide && fullscreen);
    if(!active || generation!=epoch || !autoHide)return;
    candidate=fullscreen?foreground:nullptr;
    if(!maximized && !fullscreen)return;
    if(fullscreen && !DockWanted(foreground))return;
    const bool knownClient=maximized && Client(foreground);
    if(maximized && !knownClient && !Tracked(foreground) && !CompatibleFullClient(foreground) && !ResizeObstruction(foreground))return;
    auto still=[&]{return fullscreen?FullscreenApp(foreground) && DockWanted(foreground):Maximized(foreground);};
    if(!active || generation!=epoch || GetForegroundWindow()!=foreground || !still())return;
    bool acquired=false;
    if(!GetPropW(foreground,LeanDockClientEdgePolicy::NonRude)) {
        // A held window switches back to NonRudeHWND in its own slot; the
        // MarkFullscreenWindow(FALSE) below also lifts the TRUE mark.
        OwnedWindow* slot=nullptr;
        for(auto& e:owned)if(e.window==foreground && e.held){slot=&e;break;}
        const bool held=slot!=nullptr;
        if(held)RemovePropW(foreground,LeanDockClientEdgePolicy::Held);
        else for(auto& e:owned)if(!e.window){slot=&e;break;}
        if(!slot){++failures;return;}
        if(!LeanDockClientEdgePolicy::Acquire(foreground,Token())){
            ++failures;*slot={};if(held)MarkFullscreen(foreground,FALSE);return;
        }
        *slot={foreground,Pid(foreground),!knownClient,fullscreen,false};acquired=true;
    }
    // A shell restart / later reclassification may demote the native taskbar
    // while this HWND still carries NonRude. Recheck only on native events.
    const bool needsRefresh=acquired || TaskbarZOrder(foreground,false);
    if(needsRefresh) {
        const HRESULT result=MarkFullscreen(foreground,FALSE);
        if(!active || generation!=epoch)return;
        if(FAILED(result)){
            ++failures;if(acquired)for(auto& e:owned)if(e.window==foreground)Forget(e);
            return;
        }
        if(GetForegroundWindow()==foreground && still() &&
           (TaskbarZOrder(foreground,true) || acquired)){
            ++repairs;if(fullscreen)++fullscreenRepairs;else if(!knownClient)++genericRepairs;
        }
    }
}
inline void Queue() {
    if(!active || pending)return;
    pending=PostMessageW(host,Recheck,0,0)!=FALSE;
    if(!pending)++failures;
}
inline void CALLBACK Event(HWINEVENTHOOK,DWORD event,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(!active || !w || child!=CHILDID_SELF)return;
    const bool tray=Tray(w);
    if(object!=OBJID_WINDOW && !(tray && event==EVENT_OBJECT_REORDER && object==OBJID_CLIENT))return;
    // Re-read current identity instead of erasing by a delayed numeric HWND:
    // Windows may have reused that value before an out-of-context event arrives.
    if(event==EVENT_OBJECT_DESTROY){if(Tracked(w))Queue();return;}
    // High-volume accessibility children never queue a full application scan.
    if(event==EVENT_SYSTEM_FOREGROUND || tray || Tracked(w) || w==GetForegroundWindow())Queue();
}
// Pointer movement, coalesced from the hot corners' raw input. Only a change
// in whether the fullscreen application in front wants the Dock queues an
// evaluation; otherwise this returns after one cursor and taskbar read.
inline void Pointer() {
    pointerFeed=true;
    if(active && candidate && DockWanted(candidate)!=Revealed(candidate))Queue();
}
inline bool Dispatch(UINT message) {
    if(message!=Recheck)return false;
    pending=false;Evaluate();return true;
}
inline void Stop() {
    if(stopping)return;
    stopping=true;
    enabled=false;active=false;pending=false;again=false;candidate=nullptr;++generation;
    if(host)RemovePropW(host,LeanDockAppBar::ControllerReady);
    if(HWND tray=FindWindowW(L"Shell_TrayWnd",nullptr))PostMessageW(tray,LeanDockAppBar::RefreshMessage(),0,0);
    if(retryTimer)KillTimer(nullptr,retryTimer);
    retryTimer=0;retryDue=0;
    for(auto& h:hooks)if(h){UnhookWinEvent(h);h=nullptr;}
    for(auto& e:owned)if(e.window)Forget(e);
    if(shell){shell->Release();shell=nullptr;}
    if(comOwned){CoUninitialize();comOwned=false;}
    host=nullptr;
    stopping=false;
}
inline HRESULT InitializeNative(unsigned epoch) {
    // Publish only a complete connection. COM can dispatch messages while it
    // initializes: a reentrant Stop must not let this older attempt go live.
    struct Pending {
        bool com{};ITaskbarList2* object{};std::array<HWINEVENTHOOK,6> events{};
        ~Pending(){for(auto h:events)if(h)UnhookWinEvent(h);if(object)object->Release();if(com)CoUninitialize();}
    } next;
    lastStage="com-initialize";
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);next.com=SUCCEEDED(hr);
    if(FAILED(hr) && hr!=RPC_E_CHANGED_MODE)return hr;
    lastStage="taskbar-create";
    hr=CoCreateInstance(CLSID_TaskbarList,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&next.object));
    if(FAILED(hr))return hr;
    if(!enabled || generation!=epoch)return E_ABORT;
    lastStage="taskbar-ready";
    hr=next.object->HrInit();if(FAILED(hr))return hr;
    if(!enabled || generation!=epoch)return E_ABORT;
    lastStage="event-hooks";
    const DWORD first[]={EVENT_SYSTEM_FOREGROUND,EVENT_OBJECT_CREATE,EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_STATECHANGE,EVENT_OBJECT_UNCLOAKED,EVENT_OBJECT_REORDER};
    const DWORD last[]={EVENT_SYSTEM_FOREGROUND,EVENT_OBJECT_HIDE,EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_STATECHANGE,EVENT_OBJECT_UNCLOAKED,EVENT_OBJECT_REORDER};
    for(size_t i=0;i<next.events.size();++i){
        next.events[i]=SetWinEventHook(first[i],last[i],nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        if(!next.events[i]){const DWORD error=GetLastError();return HRESULT_FROM_WIN32(error?error:ERROR_GEN_FAILURE);}
    }
    if(!enabled || generation!=epoch)return E_ABORT;
    shell=next.object;next.object=nullptr;hooks=next.events;next.events={};comOwned=next.com;next.com=false;
    return S_OK;
}
#ifdef LEANDOCK_CLIENT_EDGE_TESTING
// Test builds replace initialization only, never the window-policy path.
inline HRESULT (*initializeForTest)(unsigned)=InitializeNative;
#endif
inline bool Attempt(bool nativeEvent);
inline void CALLBACK Retry(HWND,UINT,UINT_PTR timer,DWORD) {
    if(!enabled || timer!=retryTimer || GetTickCount64()<retryDue)return;
    KillTimer(nullptr,retryTimer);retryTimer=0;retryDue=0;Attempt(false);
}
inline bool Attempt(bool nativeEvent) {
    if(!enabled || initializing || active || !initPolicy.Begin(GetTickCount64(),nativeEvent))return active;
    initializing=true;const unsigned epoch=generation;++initAttempts;
#ifdef LEANDOCK_CLIENT_EDGE_TESTING
    const HRESULT result=initializeForTest(epoch);
#else
    const HRESULT result=InitializeNative(epoch);
#endif
    initializing=false;
    if(!enabled || generation!=epoch)return false;
    lastError=result;
    if(SUCCEEDED(result)){
        active=true;lastStage="ready";initPolicy.Success();
        // The backend preserves native query responses until this event-driven
        // public Shell adapter can protect ordinary full-client maximization.
        if(!SetPropW(host,LeanDockAppBar::ControllerReady,reinterpret_cast<HANDLE>(uintptr_t(GetCurrentProcessId()))))++failures;
        if(HWND tray=FindWindowW(L"Shell_TrayWnd",nullptr))PostMessageW(tray,LeanDockAppBar::RefreshMessage(),0,0);
        if(retryTimer)KillTimer(nullptr,retryTimer);
        retryTimer=0;retryDue=0;Queue();return true;
    }
    ++failures;
    // Refusals and permanent registration failures are not a readiness delay.
    if(result==E_ACCESSDENIED || result==REGDB_E_CLASSNOTREG || result==E_NOINTERFACE)initPolicy.Stop();
    if(initPolicy.Retryable() && !retryTimer){
        retryDue=GetTickCount64()+LeanDockStart::AttachPolicy::DelayMs;
        retryTimer=SetTimer(nullptr,0,static_cast<UINT>(LeanDockStart::AttachPolicy::DelayMs),Retry);
        if(!retryTimer){lastStage="retry-timer";lastError=HRESULT_FROM_WIN32(GetLastError());initPolicy.Stop();retryDue=0;}
    }
    return false;
}
inline bool Start(HWND owner) {
    Stop();host=owner;enabled=true;initPolicy={};return Attempt(true);
}
inline void Ensure(HWND owner) {
    if(!enabled || host!=owner){Start(owner);return;}
    if(active)Queue();else Attempt(true);
}
}
