#include "../include/cards-input-latest.h"
#include <cstdio>
#include <thread>
using Route=LeanDockCards::CardInputRoute;
static LRESULT CALLBACK Window(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_APP+1){LRESULT result{};return Route::Dispatch(*reinterpret_cast<Route::Packet*>(lp),result)?result:91;}
    return DefWindowProcW(w,msg,wp,lp);
}
static bool installOK=true;
static bool Install(WNDPROC target,WNDPROC hook,WNDPROC* original) noexcept {
    if(!installOK||target!=Window||!hook)return false;
    *original=target;return true;
}
struct Probe {
    Route route;HWND window{};int wheels{},delta{},starts{};UINT32 pointer{};
    bool horizontal=false,visible=true,stopOnWheel=false,stopOnStart=false,accept=true;
    bool Start(){
        return route.Start(this,[](void* owner,HWND w,POINT p) noexcept {
            auto& self=*static_cast<Probe*>(owner);
            return self.visible&&w==self.window&&p.x>=10&&p.x<114&&p.y>=10&&p.y<54;
        },[](void* owner,int delta,bool horizontal) noexcept {
            auto& self=*static_cast<Probe*>(owner);++self.wheels;self.delta=delta;self.horizontal=horizontal;
            if(self.stopOnWheel)self.route.Stop();
        },[](void* owner,HWND,UINT32 pointer) noexcept {
            auto& self=*static_cast<Probe*>(owner);++self.starts;self.pointer=pointer;
            if(self.stopOnStart)self.route.Stop();
            return self.accept;
        });
    }
};
int main(){
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    const wchar_t* names[]={L"LeanDockHiddenCardFixture",L"Windows.UI.Composition.DesktopWindowContentBridge",L"Windows.UI.Input.InputSite.WindowClass"};
    for(auto name:names){WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=Window;cls.lpszClassName=name;check(RegisterClassW(&cls)!=0,"register fixture class");}
    auto make=[&](PCWSTR name,HWND parent){return CreateWindowExW(0,name,L"",parent?WS_CHILD:WS_POPUP,0,0,200,100,parent,nullptr,GetModuleHandleW(nullptr),nullptr);};
    HWND first=make(names[0],nullptr),second=make(names[0],nullptr);
    HWND bridgeA=make(names[1],first),bridgeB=make(names[1],second);
    Probe a,b;a.window=make(names[2],bridgeA);b.window=make(names[2],bridgeB);
    check(first&&second&&a.window&&b.window&&!IsWindowVisible(first)&&!IsWindowVisible(second),"fixtures are never shown");
    check(a.Start()&&b.Start(),"routes start on owning thread");
    check(!a.route.Ready(),"no readiness before verified procedure binding");
    installOK=false;check(!a.route.Bind(first,Install)&&!a.route.Ready(),"installer rejection leaves route inactive");
    installOK=true;check(a.route.Bind(first,Install)&&b.route.Bind(second,Install),"bind exact hierarchy");
    check(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(a.window,GWLP_WNDPROC))==Window,"WndProc identity is unchanged");
    Route::Packet p{a.window,DM_POINTERHITTEST,42,0,{30,30},PT_TOUCHPAD,false,true};
    auto send=[&](){return SendMessageW(first,WM_APP+1,0,reinterpret_cast<LPARAM>(&p));};
    check(send()==0&&a.starts==1&&a.pointer==42&&b.starts==0,"synchronous hit test delivers exact contact once");
    a.accept=false;check(send()==91&&a.starts==2,"failed contact keeps native handling");a.accept=true;
    p.message=WM_POINTERDOWN;check(send()==91,"pointer stream remains with DirectManipulation");
    p.message=DM_POINTERHITTEST;p.type=PT_TOUCH;check(send()==91,"touchscreen remains native");
    p.type=PT_PEN;check(send()==91,"pen remains native");
    p.type=PT_POINTER;check(send()==91,"unknown pointer remains native");
    p.type=PT_TOUCHPAD;p.pointValid=false;check(send()==91,"unknown cursor remains native");
    p.pointValid=true;p.control=true;check(send()==91,"control gesture is not claimed");
    p.control=false;p.point.x=9;check(send()==91,"outside left boundary remains native");
    p.point.x=114;check(send()==91,"right boundary is exclusive");
    p.point={30,54};check(send()==91,"bottom boundary is exclusive");
    p.point={30,30};a.visible=false;check(send()==91,"hidden card remains native");a.visible=true;
    p.window=b.window;check(send()==0&&b.starts==1&&a.starts==2,"second card contact is isolated");
    p.window=a.window;p.message=WM_MOUSEWHEEL;p.wp=MAKEWPARAM(0,30);
    check(send()==0&&a.wheels==1&&a.delta==30&&!a.horizontal,"vertical fractional wheel delivered once");
    p.message=WM_MOUSEHWHEEL;p.wp=MAKEWPARAM(0,static_cast<WORD>(-60));
    check(send()==0&&a.wheels==2&&a.delta==-60&&a.horizontal,"horizontal wheel retains signed fraction");
    p.message=WM_POINTERWHEEL;p.wp=MAKEWPARAM(42,120);
    check(send()==0&&a.wheels==3&&!a.horizontal,"pointer vertical wheel handled");
    p.message=WM_POINTERHWHEEL;check(send()==0&&a.wheels==4&&a.horizontal,"pointer horizontal wheel handled");
    p.control=true;check(send()==91&&a.wheels==4,"pinch wheel not consumed");
    p.control=false;p.message=WM_LBUTTONDOWN;check(send()==91,"battery click remains native");
    p.message=WM_RBUTTONDOWN;check(send()==91,"context menu remains native");
    p.message=WM_POINTERHWHEEL;bool otherThreadHandled=true;
    std::thread t([&]{LRESULT r{};otherThreadHandled=Route::Dispatch(p,r);});t.join();
    check(!otherThreadHandled,"other threads have no routes");
    a.stopOnWheel=true;check(send()==0&&!a.route.Ready()&&a.wheels==5,"callback can retire its route");
    check(send()==91&&a.wheels==5,"stopped route cannot consume input");
    check(a.Start()&&a.route.Bind(first,Install),"route can restart");
    p.message=DM_POINTERHITTEST;p.wp=42;a.stopOnStart=true;
    check(send()==0&&!a.route.Ready(),"contact callback can retire route");
    p.window=b.window;p.message=WM_MOUSEWHEEL;
    check(send()==0&&b.wheels==1,"second route survives first retirement");
    DestroyWindow(second);check(!b.route.Ready(),"destroyed host no longer ready");b.route.Stop();
    DestroyWindow(first);for(auto name:names)UnregisterClassW(name,GetModuleHandleW(nullptr));
    std::printf("%d checks, %d failures; hidden synchronous-message/decoded-packet fixtures, not hardware acceptance\n",checks,failures);
    return failures?1:0;
}
