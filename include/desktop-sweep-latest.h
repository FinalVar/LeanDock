#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <array>
#include <cstdint>
#include <cwchar>
#include "fullscreen-app-policy-latest.h"
#include "desktop-sweep-mark-latest.h"

// Win+D ("show desktop") minimizes the windows that have a minimize box and
// only covers the others, by raising the desktop over them: a caption-less
// or fullscreen terminal, for example. Activating any window lowers the
// desktop again and those windows reappear, so after the desktop hot corner,
// opening one window seemed to bring everything back. The desktop stays the
// bottom-most window, so nothing can be kept under it; after the corner's
// Win+D the covered windows are minimized like the others instead, and
// tagged (desktop-sweep-mark-latest.h). The taskbar and Alt+Tab would bring
// them back with SC_RESTORE, which takes Windows Terminal out of fullscreen;
// for a tagged window Explorer only activates it, and this controller, which
// runs at the window's integrity level where Explorer may not, shows it again
// with ShowWindow when it becomes the foreground while still minimized.
// Leaving the desktop with the same corner before another window was used
// returns them, as Win+D returns its own windows.
namespace LeanDockDesktopSweep {
struct Facts {
    bool visible=false,iconic=false,cloaked=false,owned=false,tool=false,topmost=false,disabled=false,
         shell=false,own=false,game=false,belowDesktop=false,onScreen=false;
};
// Pure: an application window Win+D left covered under the raised desktop,
// whatever its size or caption. Topmost, owned and tool windows stay as
// Win+D leaves them; games are never minimized from outside.
inline bool Sweepable(const Facts& f) {
    return f.visible && !f.iconic && !f.cloaked && !f.owned && !f.tool && !f.topmost && !f.disabled &&
        !f.shell && !f.own && !f.game && f.belowDesktop && f.onScreen;
}
// Pure: the desktop session survives the desktop itself and a taskbar (a
// press on the Dock); any other foreground means another window was used.
inline bool Keeps(const wchar_t* cls) {
    for(auto name:{L"Progman",L"WorkerW",L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"})if(!wcscmp(cls,name))return true;
    return false;
}
// Pure: what a foreground change means for the tagged windows. A listed
// window activated while still minimized and tagged is shown here; a listed
// window that is back, or whose value is not ours, leaves the list.
enum class Activation { Ignore, Return, Drop };
inline Activation OnActivate(bool listed,bool iconic,HANDLE tag) {
    if(!listed)return Activation::Ignore;
    return LeanDockDesktopSweepMark::RestoresHere(iconic,tag)?Activation::Return:Activation::Drop;
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

inline constexpr UINT_PTR TimerId=0x4453;
inline constexpr unsigned Capacity=16,OrderCapacity=64;
inline constexpr uint64_t Settle=120,ShowDeadline=1500,RestoreDeadline=600,RestoreSettle=150;
enum class Phase { Idle, Show, Restore };
struct Entry { HWND window{}; DWORD pid{}; };
inline std::array<Entry,Capacity> swept{},marked{};
inline std::array<HWND,OrderCapacity> order{};
inline unsigned count=0,orderCount=0,markedCount=0;
inline Phase phase=Phase::Idle;
inline HWND source=nullptr,owner=nullptr;
inline HWINEVENTHOOK hook=nullptr;
inline bool open=false;
inline uint64_t started=0,since=0;
inline uint64_t sweeps=0,minimized=0,restored=0,returned=0,ended=0;
inline void (*log)(const char*,unsigned)=nullptr;

inline DWORD Pid(HWND w) {DWORD p=0;GetWindowThreadProcessId(w,&p);return p;}
inline bool Valid(const Entry& e) {return IsWindow(e.window) && Pid(e.window)==e.pid;}
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
    wchar_t cls[64]{};GetClassNameW(w,cls,64);
    f.shell=Keeps(cls) || w==GetShellWindow() || !wcscmp(cls,L"Windows.UI.Core.CoreWindow");
    f.own=Pid(w)==GetCurrentProcessId();
    RECT frame{};MONITORINFO m{sizeof(m)};
    if(GetWindowRect(w,&frame) && GetMonitorInfoW(MonitorFromRect(&frame,MONITOR_DEFAULTTONULL),&m)) {
        RECT overlap{};f.onScreen=IntersectRect(&overlap,&frame,&m.rcMonitor)!=FALSE;
    }
    // Only a window that would otherwise be minimized pays for the process query.
    f.game=false;if(Sweepable(f))f.game=Game(w);
    return f;
}
// The tag is ours only while it carries our value; a window that is no
// longer minimized (returned some other way) loses it.
inline void Unmark(HWND w) {
    if(IsWindow(w) && GetPropW(w,LeanDockDesktopSweepMark::Name)==LeanDockDesktopSweepMark::Value)
        RemovePropW(w,LeanDockDesktopSweepMark::Name);
}
inline void Mark(const Entry& e) {
    if(markedCount>=Capacity)return;
    for(unsigned i=0;i<markedCount;++i)if(marked[i].window==e.window)return;
    if(GetPropW(e.window,LeanDockDesktopSweepMark::Name)!=LeanDockDesktopSweepMark::Value &&
       !SetPropW(e.window,LeanDockDesktopSweepMark::Name,LeanDockDesktopSweepMark::Value))return;
    marked[markedCount++]=e;
}
inline void Drop(HWND w) {
    unsigned kept=0;
    for(unsigned i=0;i<markedCount;++i)if(marked[i].window!=w)marked[kept++]=marked[i];
    for(unsigned i=kept;i<markedCount;++i)marked[i]={};
    markedCount=kept;
}
// Drops closed windows and untags windows that are back on screen.
inline void Prune(bool all) {
    unsigned kept=0;
    for(unsigned i=0;i<markedCount;++i) {
        const auto e=marked[i];
        if(!Valid(e))continue;
        if(all || !IsIconic(e.window)){Unmark(e.window);continue;}
        marked[kept++]=e;
    }
    for(unsigned i=kept;i<markedCount;++i)marked[i]={};
    markedCount=kept;
}
inline void CALLBACK Foreground(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
// One foreground hook, held while the desktop session is open or a tagged
// window may still be minimized; event-driven and released when neither is.
inline void Watch() {
    const bool need=open || markedCount;
    if(need && !hook)hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,Foreground,0,0,
        WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
    else if(!need && hook){UnhookWinEvent(hook);hook=nullptr;}
}
inline void Forget() {open=false;count=0;swept={};}
inline void CALLBACK Foreground(HWINEVENTHOOK,DWORD,HWND w,LONG object,LONG child,DWORD,DWORD) {
    if(object!=OBJID_WINDOW || child!=CHILDID_SELF)return;
    if(w) {
        bool listed=false;
        for(unsigned i=0;i<markedCount;++i)if(marked[i].window==w && Valid(marked[i]))listed=true;
        switch(OnActivate(listed,IsIconic(w)!=FALSE,GetPropW(w,LeanDockDesktopSweepMark::Name))) {
        // Already active; ShowWindow, not SC_RESTORE, so it returns as it was minimized.
        case Activation::Return:Unmark(w);Drop(w);ShowWindowAsync(w,SW_RESTORE);++returned;if(log)log("desktop-sweep-returned",1);break;
        case Activation::Drop:Unmark(w);Drop(w);break;
        case Activation::Ignore:break;
        }
    }
    wchar_t cls[64]{};
    if(open && !(w && GetClassNameW(w,cls,64) && Keeps(cls))){++ended;Forget();if(log)log("desktop-sweep-ended",0);}
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
        if(!s.below || !Sweepable(Read(w,true)))return TRUE;
        swept[count]={w,Pid(w)};Mark(swept[count++]);
        ShowWindowAsync(w,SW_SHOWMINNOACTIVE);++minimized;
        return count<Capacity;
    },reinterpret_cast<LPARAM>(&scan));
    ++sweeps;
    if(!count)return;
    open=true;Watch();open=hook!=nullptr;
    if(log)log(open?"desktop-sweep":"desktop-sweep-unwatched",count);
    if(!open)count=0;
}
inline void Restore() {
    HWND front=nullptr;unsigned n=0;
    // Back to front, each under the nearest window that was above it; the
    // window that was in front before the desktop returns last and active.
    // ShowWindow, not SC_RESTORE: they come back as they were minimized.
    for(unsigned i=count;i-->0;) {
        const auto e=swept[i];
        if(!Valid(e) || !IsIconic(e.window))continue;
        Unmark(e.window);Drop(e.window);
        if(e.window==source){front=e.window;continue;}
        ShowWindowAsync(e.window,SW_SHOWNOACTIVATE);
        if(HWND above=Above(order.data(),orderCount,e.window,[](HWND w){return Shown(w);}))
            SetWindowPos(e.window,above,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_ASYNCWINDOWPOS);
        ++n;
    }
    if(front){ShowWindowAsync(front,SW_RESTORE);SetForegroundWindow(front);++n;}
    restored+=n;count=0;swept={};Prune(false);Watch();
    if(log)log("desktop-sweep-restored",n);
}
inline void Cancel() {if(phase!=Phase::Idle && owner)KillTimer(owner,TimerId);phase=Phase::Idle;}
// Before the desktop corner sends Win+D. On the desktop with windows this
// corner minimized, Win+D leaves the desktop and they return with it.
inline void Prepare(HWND window) {
    Cancel();owner=window;
    if(open && count && Desktop(GetForegroundWindow())){open=false;phase=Phase::Restore;return;}
    Forget();Prune(false);Watch();source=GetForegroundWindow();Remember();phase=Phase::Show;
}
inline void Sent(bool ok) {
    if(phase==Phase::Idle)return;
    if(!ok || !owner || !SetTimer(owner,TimerId,30,nullptr)){
        if(phase==Phase::Restore){count=0;swept={};Watch();}
        phase=Phase::Idle;return;
    }
    started=GetTickCount64();since=0;
}
// Bounded to one Win+D: at most ShowDeadline / RestoreDeadline of 30 ms ticks.
inline bool Timer(HWND window,WPARAM id) {
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
// Minimized windows stay minimized; their tags and the hook go with the
// controller, and Explorer then takes the system path for them.
inline void Stop() {Cancel();Forget();Prune(true);Watch();owner=nullptr;}
}
