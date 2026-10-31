#include "../include/desktop-sweep-latest.h"
#include <cstdio>
namespace S=LeanDockDesktopSweep;
static int failures=0,checks=0;
static void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
static S::Facts Covered(){S::Facts f;f.visible=true;f.belowDesktop=true;f.onScreen=true;f.covers=true;return f;}
int main(){
    // What Win+D leaves covered and the corner minimizes.
    Check(S::Sweepable(Covered()),"caption-less fullscreen window under the raised desktop");
    {auto f=Covered();f.covers=false;f.minimizeBox=true;Check(S::Sweepable(f),"window with a minimize box Win+D left under the desktop");}
    {auto f=Covered();f.covers=false;Check(!S::Sweepable(f),"small window without a minimize box stays as Win+D left it");}
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
    // Minimize only what has a minimize box; a fullscreen window without one
    // (Windows Terminal leaves fullscreen when restored from the taskbar) is
    // buried under the desktop instead, once another window is used.
    Check(!S::Minimizes(Covered()),"fullscreen window without a minimize box is not minimized");
    {auto f=Covered();f.minimizeBox=true;Check(S::Minimizes(f),"covered window with a minimize box is minimized");}
    {auto f=Covered();f.minimizeBox=true;f.topmost=true;Check(!S::Minimizes(f),"never minimizes what it would not sweep");}
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
    // Buried windows: activation or destruction releases them; hidden ones are not moved.
    HWND second=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,10,10,nullptr,nullptr,wc.hInstance,nullptr);
    S::buried[0]={host,S::Pid(host),false};S::buried[1]={second,S::Pid(second),false};S::buriedCount=2;
    S::Unbury(host);
    Check(S::buriedCount==1 && S::buried[0].window==second,"an activated window is no longer kept under the desktop");
    const auto burialsBefore=S::burials;S::Rebury(nullptr);
    Check(S::buriedCount==1 && S::burials==burialsBefore,"a hidden window is not moved");
    DestroyWindow(second);S::Unbury(nullptr);
    Check(S::buriedCount==0,"a closed window is dropped");
    // Timer lifecycle: one Win+D, bounded, and only its own timer id.
    S::Prepare(host);
    Check(S::phase==S::Phase::Show && !S::open && S::count==0,"desktop corner prepares a sweep");
    S::Sent(false);
    Check(S::phase==S::Phase::Idle,"failed input leaves nothing armed");
    S::Prepare(host);S::Sent(true);
    Check(S::phase==S::Phase::Show,"sent input arms the timer");
    Check(!S::Timer(host,S::TimerId+2),"other timer ids pass through");
    Check(S::Timer(host,S::BuryTimer),"burial re-check timer is handled and ends with nothing buried");
    S::started=GetTickCount64()-S::ShowDeadline-1;
    if(!S::Desktop(GetForegroundWindow())){Check(S::Timer(host,S::TimerId) && S::phase==S::Phase::Idle,"gives up after the deadline without the desktop");}
    S::buried[0]={host,S::Pid(host),false};S::buriedCount=1;S::Watch();
    Check(S::hook!=nullptr,"a buried window keeps the foreground hook");
    S::Stop();
    Check(S::phase==S::Phase::Idle && !S::open && !S::hook && S::count==0 && S::buriedCount==0,"stop releases everything");
    DestroyWindow(host);
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
