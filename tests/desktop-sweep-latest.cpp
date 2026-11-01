#include "../include/desktop-sweep-latest.h"
#include <cstdio>
namespace S=LeanDockDesktopSweep;
namespace M=LeanDockDesktopSweepMark;
static int failures=0,checks=0;
static void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
static S::Facts Covered(){S::Facts f;f.visible=true;f.belowDesktop=true;f.onScreen=true;return f;}
int main(){
    // What Win+D leaves covered under the raised desktop and the corner
    // minimizes, whatever its size or caption.
    Check(S::Sweepable(Covered()),"application window under the raised desktop");
    {auto f=Covered();f.belowDesktop=false;Check(!S::Sweepable(f),"window above the desktop (topmost band) untouched");}
    {auto f=Covered();f.iconic=true;Check(!S::Sweepable(f),"already minimized");}
    {auto f=Covered();f.visible=false;Check(!S::Sweepable(f),"hidden");}
    {auto f=Covered();f.cloaked=true;Check(!S::Sweepable(f),"cloaked (other virtual desktop)");}
    {auto f=Covered();f.owned=true;Check(!S::Sweepable(f),"owned window follows its owner");}
    {auto f=Covered();f.tool=true;Check(!S::Sweepable(f),"tool window");}
    {auto f=Covered();f.topmost=true;Check(!S::Sweepable(f),"topmost window");}
    {auto f=Covered();f.disabled=true;Check(!S::Sweepable(f),"disabled behind a modal dialog");}
    {auto f=Covered();f.shell=true;Check(!S::Sweepable(f),"shell window");}
    {auto f=Covered();f.own=true;Check(!S::Sweepable(f),"own process");}
    {auto f=Covered();f.game=true;Check(!S::Sweepable(f),"game never minimized from outside");}
    {auto f=Covered();f.onScreen=false;Check(!S::Sweepable(f),"off-screen window");}
    // The Explorer side restores only a tagged window that is still minimized.
    Check(M::RestoresHere(true,M::Value),"tagged minimized window restored without SC_RESTORE");
    Check(!M::RestoresHere(false,M::Value),"tagged window already on screen takes the system path");
    Check(!M::RestoresHere(true,nullptr),"untagged window takes the system path");
    Check(!M::RestoresHere(true,reinterpret_cast<HANDLE>(2)),"someone else's value is not our tag");
    // The controller returns a listed window activated while minimized.
    Check(S::OnActivate(true,true,M::Value)==S::Activation::Return,"tagged window activated while minimized is shown");
    Check(S::OnActivate(true,false,M::Value)==S::Activation::Drop,"listed window already back leaves the list");
    Check(S::OnActivate(true,true,reinterpret_cast<HANDLE>(2))==S::Activation::Drop,"listed window with another value leaves the list");
    Check(S::OnActivate(false,true,M::Value)==S::Activation::Ignore,"unlisted window is not ours to show");
    // Which foregrounds end the desktop session.
    for(auto keep:{L"Progman",L"WorkerW",L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"})Check(S::Keeps(keep),"desktop and taskbar keep the session");
    for(auto end:{L"CabinetWClass",L"CASCADIA_HOSTING_WINDOW_CLASS",L"XamlExplorerHostIslandWindow",L"Chrome_WidgetWin_1",L""})
        Check(!S::Keeps(end),"any other window ends the session");
    // Stacking on return.
    HWND a=reinterpret_cast<HWND>(0x10),b=reinterpret_cast<HWND>(0x20),c=reinterpret_cast<HWND>(0x30),d=reinterpret_cast<HWND>(0x40);
    const HWND order[]={a,b,c,d};
    auto all=[](HWND){return true;};
    Check(S::Above(order,4,c,all)==b,"returns under the window directly above it");
    Check(S::Above(order,4,c,[&](HWND w){return w!=b;})==a,"skips a window that is still minimized");
    Check(S::Above(order,4,a,all)==nullptr,"front window has nothing above");
    Check(S::Above(order,4,c,[](HWND){return false;})==nullptr,"nothing above shown");
    Check(S::Above(order,4,reinterpret_cast<HWND>(0x50),all)==nullptr,"window opened after the desktop");
    Check(S::Above(order,0,a,all)==nullptr,"empty order");
    // Real calls: a hidden window of this process is never minimized.
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LeanDockSweepTest";
    RegisterClassW(&wc);
    HWND host=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,10,10,nullptr,nullptr,wc.hInstance,nullptr);
    Check(host!=nullptr,"test window");
    {auto f=S::Read(host,true);Check(!f.visible && !S::Sweepable(f),"hidden window read as not visible");}
    if(HWND shell=GetShellWindow())Check(S::Desktop(shell),"shell window is the desktop");
    Check(!S::Desktop(host) && !S::Desktop(nullptr),"other windows are not the desktop");
    // Tags: set once, adopted, removed when the window is back or on stop,
    // never removed when the value is not ours; closed windows are dropped.
    HWND second=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,10,10,nullptr,nullptr,wc.hInstance,nullptr);
    S::Mark({host,S::Pid(host)});S::Mark({host,S::Pid(host)});
    Check(S::markedCount==1 && GetPropW(host,M::Name)==M::Value,"window tagged once");
    S::Prune(false);
    Check(S::markedCount==0 && !GetPropW(host,M::Name),"a window that is not minimized loses the tag");
    SetPropW(second,M::Name,reinterpret_cast<HANDLE>(2));S::Unmark(second);
    Check(GetPropW(second,M::Name)==reinterpret_cast<HANDLE>(2),"a value that is not ours stays");
    RemovePropW(second,M::Name);
    S::Mark({second,S::Pid(second)});DestroyWindow(second);S::Prune(false);
    Check(S::markedCount==0,"a closed window is dropped");
    S::Mark({host,S::Pid(host)});S::Prune(true);
    Check(S::markedCount==0 && !GetPropW(host,M::Name),"stop removes every tag");
    HWND third=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,10,10,nullptr,nullptr,wc.hInstance,nullptr);
    S::Mark({host,S::Pid(host)});S::Mark({third,S::Pid(third)});S::Drop(host);
    Check(S::markedCount==1 && S::marked[0].window==third,"drop removes one window only");
    S::Prune(true);DestroyWindow(third);
    // The hook is held while a tagged window may still be minimized. An
    // off-screen tool window, minimized without activation, stands in.
    HWND parked=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,-20000,-20000,10,10,nullptr,nullptr,wc.hInstance,nullptr);
    ShowWindow(parked,SW_SHOWMINNOACTIVE);
    Check(IsIconic(parked)!=FALSE,"stand-in window minimized");
    S::Mark({parked,S::Pid(parked)});S::Watch();
    Check(S::hook!=nullptr && !S::open,"tagged minimized window keeps the hook without a session");
    S::Prepare(host);S::Cancel();
    Check(S::markedCount==1 && S::hook!=nullptr,"next corner keeps a window that is still minimized");
    S::Foreground(nullptr,EVENT_SYSTEM_FOREGROUND,host,OBJID_WINDOW,CHILDID_SELF,0,0);
    Check(S::markedCount==1 && S::hook!=nullptr && IsIconic(parked),"another window in front leaves it alone");
    S::Mark({host,S::Pid(host)});
    S::Foreground(nullptr,EVENT_SYSTEM_FOREGROUND,host,OBJID_WINDOW,CHILDID_SELF,0,0);
    Check(S::markedCount==1 && !GetPropW(host,M::Name) && S::returned==0,"listed window in front and not minimized is dropped and untagged");
    S::Prune(true);S::Watch();
    Check(S::markedCount==0 && !S::hook && !GetPropW(parked,M::Name) && IsIconic(parked),"no tags left releases the hook; the window stays minimized");
    DestroyWindow(parked);
    // Timer lifecycle: one Win+D, bounded, and only its own timer id.
    S::Prepare(host);
    Check(S::phase==S::Phase::Show && !S::open && S::count==0,"desktop corner prepares a sweep");
    S::Sent(false);
    Check(S::phase==S::Phase::Idle,"failed input leaves nothing armed");
    S::Prepare(host);S::Sent(true);
    Check(S::phase==S::Phase::Show,"sent input arms the timer");
    Check(!S::Timer(host,S::TimerId+1),"other timer ids pass through");
    S::started=GetTickCount64()-S::ShowDeadline-1;
    if(!S::Desktop(GetForegroundWindow())){Check(S::Timer(host,S::TimerId) && S::phase==S::Phase::Idle,"gives up after the deadline without the desktop");}
    S::Mark({host,S::Pid(host)});
    S::Stop();
    Check(S::phase==S::Phase::Idle && !S::open && !S::hook && S::count==0 && S::markedCount==0 && !GetPropW(host,M::Name),"stop releases everything");
    DestroyWindow(host);
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
