#pragma once
#include <windows.h>
#include <appmodel.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <array>
#include <iterator>
#include "client-edge-policy-latest.h"

// A maximized frameless Codex/ChatGPT window can be classified as fullscreen
// and put above the auto-hide taskbar. Use Microsoft's NonRudeHWND contract,
// then refresh that classification and the native taskbar's z-order.
// No input synthesis, app resizing, overlay, private hook or periodic polling.
namespace LeanDockClientEdge {
inline constexpr UINT Recheck=WM_APP+421;
struct OwnedWindow {HWND window{};DWORD pid{};};
inline std::array<OwnedWindow,32> owned{};
inline std::array<HWINEVENTHOOK,5> hooks{};
inline HWND host;
inline ITaskbarList2* shell;
inline bool active,pending,comOwned,evaluating,again,stopping;
inline unsigned generation;
inline unsigned repairs,failures;
inline HANDLE Token(){return reinterpret_cast<HANDLE>(host);}
inline DWORD Pid(HWND w){DWORD pid=0;GetWindowThreadProcessId(w,&pid);return pid;}
inline bool Maximized(HWND w) {
    if(!w || GetAncestor(w,GA_ROOT)!=w)return false;
    RECT r{};MONITORINFO m{sizeof(m)};
    if(!GetWindowRect(w,&r) || !GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONULL),&m))return false;
    return LeanDockClientEdgePolicy::Eligible(IsWindowVisible(w),IsIconic(w),IsZoomed(w),GetWindowLongPtrW(w,GWL_STYLE),r,m.rcMonitor);
}
inline bool Client(HWND w) {
    return LeanDockClientEdgePolicy::ClientWindow(w);
}
inline bool AutoHide() {APPBARDATA data{sizeof(data)};return (SHAppBarMessage(ABM_GETSTATE,&data)&ABS_AUTOHIDE)!=0;}
inline bool Tracked(HWND w){for(const auto& e:owned)if(e.window==w)return true;return false;}
inline void Forget(OwnedWindow& e) {
    // Properties die with the HWND. If the numeric handle was reused, do not
    // remove a new window's property or send it a shell notification.
    const OwnedWindow old=e;e={};
    if(Pid(old.window)==old.pid && LeanDockClientEdgePolicy::Release(old.window,Token()) && shell) {
        ITaskbarList2* current=shell;current->AddRef();
        current->MarkFullscreenWindow(old.window,FALSE);current->Release();
    }
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
    for(auto& e:owned)if(e.window && (!autoHide || Pid(e.window)!=e.pid ||
        GetPropW(e.window,LeanDockClientEdgePolicy::Owner)!=Token() || !Maximized(e.window)))Forget(e);
    if(!active || generation!=epoch)return;
    HWND foreground=GetForegroundWindow();
    if(!autoHide || !Maximized(foreground) || !Client(foreground))return;
    bool acquired=false;
    if(!GetPropW(foreground,LeanDockClientEdgePolicy::NonRude)) {
        OwnedWindow* slot=nullptr;for(auto& e:owned)if(!e.window){slot=&e;break;}
        if(!slot){++failures;return;}
        if(!LeanDockClientEdgePolicy::Acquire(foreground,Token())){++failures;return;}
        *slot={foreground,Pid(foreground)};acquired=true;
    }
    // A shell restart / later reclassification may demote the native taskbar
    // while this HWND still carries NonRude. Recheck only on native events.
    const bool needsRefresh=acquired || TaskbarZOrder(foreground,false);
    if(needsRefresh) {
        // Keep the COM object alive across a reentrant shutdown notification.
        ITaskbarList2* current=shell;current->AddRef();
        const HRESULT result=current->MarkFullscreenWindow(foreground,FALSE);current->Release();
        if(!active || generation!=epoch)return;
        if(FAILED(result)){
            ++failures;if(acquired)for(auto& e:owned)if(e.window==foreground)Forget(e);
            return;
        }
        if(GetForegroundWindow()==foreground && Maximized(foreground) &&
           (TaskbarZOrder(foreground,true) || acquired))++repairs;
    }
}
inline void Queue() {
    if(!active || pending)return;
    pending=PostMessageW(host,Recheck,0,0)!=FALSE;
    if(!pending)++failures;
}
inline void CALLBACK Event(HWINEVENTHOOK,DWORD event,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(!active || !w || object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
    // Re-read current identity instead of erasing by a delayed numeric HWND:
    // Windows may have reused that value before an out-of-context event arrives.
    if(event==EVENT_OBJECT_DESTROY){if(Tracked(w))Queue();return;}
    // High-volume accessibility children never queue a full application scan.
    if(event==EVENT_SYSTEM_FOREGROUND || Tracked(w) || w==GetForegroundWindow())Queue();
}
inline bool Dispatch(UINT message) {
    if(message!=Recheck)return false;
    pending=false;Evaluate();return true;
}
inline void Stop() {
    if(stopping)return;
    stopping=true;
    active=false;pending=false;again=false;++generation;
    for(auto& h:hooks)if(h){UnhookWinEvent(h);h=nullptr;}
    for(auto& e:owned)if(e.window)Forget(e);
    if(shell){shell->Release();shell=nullptr;}
    if(comOwned){CoUninitialize();comOwned=false;}
    host=nullptr;
    stopping=false;
}
inline bool Start(HWND owner) {
    Stop();host=owner;
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    comOwned=SUCCEEDED(hr);
    if(FAILED(hr) && hr!=RPC_E_CHANGED_MODE){++failures;Stop();return false;}
    hr=CoCreateInstance(CLSID_TaskbarList,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&shell));
    if(FAILED(hr) || FAILED(shell->HrInit())){++failures;Stop();return false;}
    const DWORD first[]={EVENT_SYSTEM_FOREGROUND,EVENT_OBJECT_CREATE,EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_STATECHANGE,EVENT_OBJECT_UNCLOAKED};
    const DWORD last[]={EVENT_SYSTEM_FOREGROUND,EVENT_OBJECT_HIDE,EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_STATECHANGE,EVENT_OBJECT_UNCLOAKED};
    active=true;
    for(size_t i=0;i<hooks.size();++i){
        hooks[i]=SetWinEventHook(first[i],last[i],nullptr,Event,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
        if(!hooks[i]){++failures;Stop();return false;}
    }
    Queue();return true;
}
}
