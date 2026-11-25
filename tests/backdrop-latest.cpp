#include <cstdio>
#include <vector>
#include "backdrop-resume-latest.h"
static int checks,failures;
static void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}}
static unsigned prepared;
static LRESULT CALLBACK FixtureProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
    if(message==LeanDockBackdrop::Message()&&wParam==static_cast<WPARAM>(LeanDockBackdrop::Reason::BeforeShow)){
        ++prepared;return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
int main(){
    using namespace LeanDockBackdrop;
    Plan plan;
    auto old=plan.Begin(1000),current=plan.Begin(1008);
    Check(!plan.Ready(old,2000),"old queued callback cannot mutate newest scene");
    Check(!plan.Advance(old,2000)&&plan.remaining==3,"old callback cannot consume new budget");
    Check(!plan.Ready(current,1023),"early timer waits for latest transition");
    Check(plan.Ready(current,1024)&&!plan.IsFinal(current),"current transition first pass runs without input");
    Check(plan.Advance(current,1024)&&plan.due==1134&&plan.NextDelay()==Plan::SettleDelay,"mid settling pass scheduled at +110ms");
    Check(!plan.Ready(current,1133),"mid settling pass waits for due time");
    Check(plan.Ready(current,1134)&&!plan.IsFinal(current),"mid settling pass is not final");
    Check(plan.Advance(current,1134)&&plan.due==1374&&plan.NextDelay()==Plan::FinalDelay,"final pass scheduled at +240ms");
    Check(!plan.Ready(current,1373),"final pass waits for post-animation due time");
    Check(plan.Ready(current,1374)&&plan.IsFinal(current)&&!plan.Advance(current,1374),"final pass terminates");
    Check(!plan.Ready(current,999999)&&!plan.remaining,"idle never refreshes again");
    current=plan.Begin(2000);plan.Cancel();
    Check(!plan.Ready(current,999999),"disconnect cancels queued callback");
    auto reconnected=plan.Begin(3000);
    Check(reconnected!=current&&!plan.Advance(current,999999),"reconnect rejects old lifetime");
    Check(plan.Ready(reconnected,3016),"reconnected brush has live finite plan");
    // Adversarial burst: every old event runs after the final request, then a
    // delayed UI thread services only the final three passes. No catch-up loop.
    plan={};std::vector<std::uint64_t> oldTickets;
    for(unsigned i=0;i<64;++i)oldTickets.push_back(plan.Begin(10000+i*3));
    current=oldTickets.back();oldTickets.pop_back();
    for(auto ticket:oldTickets)Check(!plan.Advance(ticket,20000),"burst rejects superseded timer");
    Check(plan.Ready(current,20000)&&plan.Advance(current,20000),"delayed burst refreshes latest once");
    Check(!plan.Ready(current,20001),"late first pass does not immediately replay second");
    Check(plan.Ready(current,20110)&&plan.Advance(current,20110),"burst advances to final post-animation pass");
    Check(!plan.Ready(current,20111),"late second pass does not immediately replay final");
    Check(plan.Ready(current,20350)&&plan.IsFinal(current)&&!plan.Advance(current,20350),"burst still ends after three finite passes");
    Check(!Fresh(0,1000)&&!Fresh(2000,1999)&&!Fresh(1,2000),"bad or expired queued message rejected");
    Check(Fresh(1000,1000)&&Fresh(1000,2000)&&!Fresh(1000,2001),"queued message age boundary");
    // Test-owned message-only window: verifies the before-Unhide entry really
    // completes synchronously, and refuses cross-thread calls without blocking.
    WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LeanDockBackdropFixture";wc.lpfnWndProc=FixtureProc;
    Check(RegisterClassW(&wc)!=0,"register hidden fixture class");
    HWND fixture=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
    Check(fixture!=nullptr,"create test-owned message-only window");
    if(fixture){
        Prepare(fixture);Check(prepared==1,"preparation completes before caller continues");
        HANDLE thread=CreateThread(nullptr,0,[](void* p)->DWORD{LeanDockBackdrop::Prepare(static_cast<HWND>(p));return 0;},fixture,0,nullptr);
        Check(thread!=nullptr,"create test worker");
        if(thread){
            const bool exited=WaitForSingleObject(thread,1000)==WAIT_OBJECT_0;
            Check(exited,"cross-thread preparation does not block");CloseHandle(thread);
            if(!exited)return 1; // terminate only this test process, never Explorer
        }
        Check(prepared==1,"cross-thread preparation sends no message");
        Check(!IsDesktop(fixture),"ordinary app is not a desktop transition");
        DestroyWindow(fixture);
    }
    UnregisterClassW(wc.lpszClassName,wc.hInstance);
    std::printf("Backdrop transitions: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
