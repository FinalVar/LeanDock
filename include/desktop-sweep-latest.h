#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <array>
#include <cstdint>
#include <cwchar>
#include "fullscreen-app-policy-latest.h"

// Win+D ("show desktop") minimizes the windows that have a minimize box and
// only covers the others, by raising the desktop over them: a caption-less
// fullscreen terminal or player, for example. Activating any window lowers
// the desktop again and those windows reappear, so after the desktop hot
// corner, one click on the Dock seemed to bring everything back.
// Such a window is not minimized here: Windows Terminal, restored from the
// taskbar after a minimize, leaves fullscreen (its SC_RESTORE handling), and
// Win+D and "minimize all" never minimize a window without a minimize box
// either. When another window is used, it goes under the desktop instead
// ("buried") and stays there until it is activated again, which brings it
// back unchanged. A covered window that does have a minimize box is
// minimized. Leaving the desktop with the same corner before another window
// was used returns all of them, as Win+D returns its own windows.
namespace LeanDockDesktopSweep {
struct Facts {
    bool visible=false,iconic=false,cloaked=false,owned=false,tool=false,topmost=false,disabled=false,
         shell=false,own=false,game=false,belowDesktop=false,onScreen=false,minimizeBox=false,covers=false;
};
// Pure: an application window Win+D left covered under the raised desktop.
// Topmost, owned and tool windows stay as Win+D leaves them; games are never
// minimized from outside.
inline bool Sweepable(const Facts& f) {
    return f.visible && !f.iconic && !f.cloaked && !f.owned && !f.tool && !f.topmost && !f.disabled &&
        !f.shell && !f.own && !f.game && f.belowDesktop && f.onScreen && (f.minimizeBox || f.covers);
}
// Pure: covered windows with a minimize box are minimized; the others are
// only buried once another window is used.
inline bool Minimizes(const Facts& f) {return Sweepable(f) && f.minimizeBox;}
// Pure: the desktop session survives the desktop itself and a taskbar (a
// press on the Dock); any other foreground means another window was used.
inline bool Keeps(const wchar_t* cls) {
    for(auto name:{L"Progman",L"WorkerW",L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"})if(!wcscmp(cls,name))return true;
    return false;
}
// Pure: when a minimized window returns, the nearest window that was above
// it before the desktop and is shown again, to be stacked under; null when
// none (it was in front, or everything above it is still minimized).
template<class Shown>
inline HWND Above(const HWND* order,unsigned count,HWND target,Shown shown) {
    unsigned index=0;
    while(index<count && order[index]!=target)++index;
    if(index==count)return nullptr;
    while(index-->0)if(shown(order[index]))return order[index];
    return nullptr;
}

inline constexpr UINT_PTR TimerId=0x4453,BuryTimer=0x4454;
inline constexpr unsigned Capacity=16,OrderCapacity=64;
inline constexpr uint64_t Settle=120,ShowDeadline=1500,RestoreDeadline=600,RestoreSettle=150,KeepDown=600;
enum class Phase { Idle, Show, Restore };
struct Entry { HWND window{}; DWORD pid{}; bool minimized=false; };
inline std::array<Entry,Capacity> swept{},buried{};
inline std::array<HWND,OrderCapacity> order{};
inline unsigned count=0,orderCount=0,buriedCount=0;
inline Phase phase=Phase::Idle;
inline HWND source=nullptr,owner=nullptr;
inline HWINEVENTHOOK hook=nullptr;
inline bool open=false;
inline uint64_t started=0,since=0,buryUntil=0;
inline uint64_t sweeps=0,minimized=0,restored=0,ended=0,burials=0;
inline void (*log)(const char*,unsigned)=nullptr;

inline DWORD Pid(HWND w) {DWORD p=0;GetWindowThreadProcessId(w,&p);return p;}
inline bool Desktop(HWND w) {
    wchar_t cls[32]{};
    return w && GetClassNameW(w,cls,32) && (!wcscmp(cls,L"Progman") || !wcscmp(cls,L"WorkerW")) &&
        (w==GetShellWindow() || FindWindowExW(w,nullptr,L"SHELLDLL_DefView",nullptr));
}
inline bool Shown(HWND w) {return IsWindow(w) && IsWindowVisible(w) && !IsIconic(w);}
// The game half of LeanDockFullscreen::Quiet: remote-control clients may be
// minimized like any other window. An unidentified process counts as a game.
inline bool Game(HWND w) {
    QUERY_USER_NOTIFICATION_STATE state{};
    if(SUCCEEDED(SHQueryUserNotificationState(&state)) && state==QUNS_RUNNING_D3D_FULL_SCREEN)return true;
    wchar_t image[1024]{};DWORD size=std::size(image);bool known=false;
    if(HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,Pid(w))) {
        known=QueryFullProcessImageNameW(p,0,image,&size)!=FALSE;CloseHandle(p);
    }
    if(!known)return true;
    for(auto library:LeanDockFullscreen::Libraries)if(LeanDockFullscreen::Contains(image,library))return true;
    return LeanDockFullscreen::Listed(image,LeanDockFullscreen::listed);
}
inline Facts Read(HWND w,bool below) {
    Facts f;f.belowDesktop=below;
    f.visible=IsWindowVisible(w)!=FALSE;f.iconic=IsIconic(w)!=FALSE;
    if(!f.visible || f.iconic)return f;
    DWORD cloak=0;f.cloaked=SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloak,sizeof(cloak))) && cloak;
    f.owned=GetWindow(w,GW_OWNER)!=nullptr;
    const LONG_PTR style=GetWindowLongPtrW(w,GWL_STYLE),ex=GetWindowLongPtrW(w,GWL_EXSTYLE);
    f.tool=(ex&WS_EX_TOOLWINDOW)!=0;f.topmost=(ex&WS_EX_TOPMOST)!=0;f.disabled=(style&WS_DISABLED)!=0;
    f.minimizeBox=(style&WS_MINIMIZEBOX)!=0;
    wchar_t cls[64]{};GetClassNameW(w,cls,64);
    f.shell=Keeps(cls) || w==GetShellWindow() || !wcscmp(cls,L"Windows.UI.Core.CoreWindow");
    f.own=Pid(w)==GetCurrentProcessId();
    RECT frame{};MONITORINFO m{sizeof(m)};
    if((SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))) || GetWindowRect(w,&frame)) &&
       GetMonitorInfoW(MonitorFromRect(&frame,MONITOR_DEFAULTTONULL),&m)) {
        RECT overlap{};f.onScreen=IntersectRect(&overlap,&frame,&m.rcMonitor)!=FALSE;
        f.covers=LeanDockFullscreen::Covers(style,frame,m.rcMonitor);
    }
    // Only a window that would otherwise be minimized pays for the process query.
    f.game=false;if(Sweepable(f))f.game=Game(w);
    return f;
}
inline void CALLBACK Foreground(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
// One foreground hook while a desktop session is open or a window is buried.
inline void Watch() {
    if((open || buriedCount) && !hook)
        hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,Foreground,0,0,
            WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    else if(!open && !buriedCount && hook){UnhookWinEvent(hook);hook=nullptr;}
}
inline void Forget() {open=false;count=0;swept={};Watch();}
inline bool Valid(const Entry& e) {return IsWindow(e.window) && Pid(e.window)==e.pid;}
// Whether the desktop window is below this window in the z-order.
inline bool AboveDesktop(HWND w) {
    const HWND desktop=GetShellWindow();if(!desktop)return false;
    unsigned steps=0;
    for(HWND next=GetWindow(w,GW_HWNDNEXT);next && steps<4096;next=GetWindow(next,GW_HWNDNEXT),++steps)if(next==desktop)return true;
    return false;
}
inline void Bury(HWND w) {
    SetWindowPos(w,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_ASYNCWINDOWPOS);++burials;
}
inline void Unbury(HWND w) {
    unsigned kept=0;
    for(unsigned i=0;i<buriedCount;++i)if(buried[i].window!=w && Valid(buried[i]))buried[kept++]=buried[i];
    for(unsigned i=kept;i<buriedCount;++i)buried[i]={};
    buriedCount=kept;
}
// A buried window stays under the desktop until it is activated, also when
// a later Win+D lowers the desktop beneath it again.
inline void Rebury(HWND front) {
    Unbury(front);
    for(unsigned i=0;i<buriedCount;++i)if(Shown(buried[i].window) && AboveDesktop(buried[i].window))Bury(buried[i].window);
}
// Windows may lower the desktop just after the foreground changes, beneath a
// window buried a moment earlier: check again for a short while.
inline void Recheck() {
    if(!owner || !buriedCount)return;
    buryUntil=GetTickCount64()+KeepDown;SetTimer(owner,BuryTimer,50,nullptr);
}
inline void CALLBACK Foreground(HWINEVENTHOOK,DWORD,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
    wchar_t cls[64]{};
    const bool keeps=w && GetClassNameW(w,cls,64) && Keeps(cls);
    if(open && !keeps) {
        // Another window was used: the desktop session ends and the windows
        // the desktop only covered go under it instead of reappearing.
        unsigned n=0;
        for(unsigned i=0;i<count;++i) {
            const auto& e=swept[i];
            if(e.minimized || e.window==w || !Valid(e) || !Shown(e.window))continue;
            Bury(e.window);++n;
            bool known=false;for(unsigned j=0;j<buriedCount;++j)known|=buried[j].window==e.window;
            if(!known && buriedCount<Capacity)buried[buriedCount++]=e;
        }
        ++ended;open=false;count=0;swept={};
        if(log)log("desktop-sweep-ended",n);
    }
    if(!keeps){Rebury(w);Recheck();}
    Watch();
}
// The application windows in front-to-back order before Win+D, to stack the
// returned windows where they were.
inline void Remember() {
    orderCount=0;
    EnumWindows([](HWND w,LPARAM)->BOOL{
        if(orderCount>=OrderCapacity)return FALSE;
        if(Shown(w) && !GetWindow(w,GW_OWNER) && !(GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW))order[orderCount++]=w;
        return TRUE;
    },0);
}
inline void Sweep(HWND desktop) {
    struct Scan {HWND desktop;bool below;} scan{desktop,false};
    EnumWindows([](HWND w,LPARAM data)->BOOL{
        auto& s=*reinterpret_cast<Scan*>(data);
        if(w==s.desktop){s.below=true;return TRUE;}
        if(!s.below)return TRUE;
        const Facts f=Read(w,true);
        if(!Sweepable(f))return TRUE;
        const bool minimize=Minimizes(f);
        swept[count++]={w,Pid(w),minimize};
        if(minimize){ShowWindowAsync(w,SW_SHOWMINNOACTIVE);++minimized;}
        return count<Capacity;
    },reinterpret_cast<LPARAM>(&scan));
    ++sweeps;
    if(!count)return;
    open=true;Watch();
    if(!hook){open=false;count=0;swept={};if(log)log("desktop-sweep-unwatched",0);return;}
    if(log)log("desktop-sweep",count);
}
inline void Restore() {
    HWND front=nullptr;unsigned n=0;
    // Back to front, each under the nearest window that was above it; the
    // window that was in front before the desktop returns last and active.
    for(unsigned i=count;i-->0;) {
        const auto e=swept[i];
        if(!Valid(e))continue;
        if(e.window==source){front=e.window;continue;}
        // A covered window returns with the desktop, where it was.
        if(!e.minimized || !IsIconic(e.window))continue;
        ShowWindowAsync(e.window,SW_SHOWNOACTIVATE);
        if(HWND above=Above(order.data(),orderCount,e.window,[](HWND w){return Shown(w);}))
            SetWindowPos(e.window,above,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_ASYNCWINDOWPOS);
        ++n;
    }
    if(front) {
        if(IsIconic(front))ShowWindowAsync(front,SW_RESTORE);
        SetForegroundWindow(front);++n;
    }
    restored+=n;count=0;swept={};
    if(log)log("desktop-sweep-restored",n);
}
inline void Cancel() {if(phase!=Phase::Idle && owner)KillTimer(owner,TimerId);phase=Phase::Idle;}
// Before the desktop corner sends Win+D. On the desktop with windows this
// corner minimized, Win+D leaves the desktop and they return with it.
inline void Prepare(HWND window) {
    Cancel();owner=window;
    if(open && count && Desktop(GetForegroundWindow())){open=false;Watch();phase=Phase::Restore;return;}
    Forget();source=GetForegroundWindow();Remember();phase=Phase::Show;
}
inline void Sent(bool ok) {
    if(phase==Phase::Idle)return;
    if(!ok || !owner || !SetTimer(owner,TimerId,30,nullptr)){
        if(phase==Phase::Restore){count=0;swept={};}
        phase=Phase::Idle;return;
    }
    started=GetTickCount64();since=0;
}
// Bounded to one Win+D: at most ShowDeadline / RestoreDeadline of 30 ms ticks.
inline bool Timer(HWND window,WPARAM id) {
    if(id==BuryTimer) {
        for(unsigned i=0;i<buriedCount;++i)if(Shown(buried[i].window) && AboveDesktop(buried[i].window))Bury(buried[i].window);
        if(!buriedCount || GetTickCount64()>=buryUntil)KillTimer(window,BuryTimer);
        return true;
    }
    if(id!=TimerId)return false;
    const uint64_t now=GetTickCount64(),elapsed=now-started;
    const bool desktop=Desktop(GetForegroundWindow());
    bool done=true;
    if(phase==Phase::Show) {
        // Win+D has put the desktop in front and it stays there.
        if(desktop){if(!since)since=now;if(now-since>=Settle)Sweep(GetForegroundWindow());else done=elapsed>=ShowDeadline;}
        else{since=0;done=elapsed>=ShowDeadline;}
    }else if(phase==Phase::Restore) {
        // Win+D first returns its own windows, which takes the desktop out of front.
        if(!desktop && !since)since=now;
        if(elapsed<RestoreDeadline && (!since || now-since<RestoreSettle))done=false;else Restore();
    }
    if(done){KillTimer(window,TimerId);phase=Phase::Idle;}
    return true;
}
// Buried windows stay where they are; the next activation brings them back.
inline void Stop() {Cancel();if(owner)KillTimer(owner,BuryTimer);buriedCount=0;buried={};Forget();owner=nullptr;}
}
