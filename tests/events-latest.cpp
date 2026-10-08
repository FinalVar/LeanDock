#include "event-loop-latest.h"
#include <cstdio>
#include <atomic>
struct Worker {HANDLE stop,ready;UINT_PTR timer{};std::atomic<unsigned> ticks{0},phase{0};};
static Worker* running;
static DWORD WINAPI Run(void* context){
    auto& worker=*static_cast<Worker*>(context);running=&worker;
    worker.timer=SetTimer(nullptr,0,10,[](HWND,UINT,UINT_PTR,DWORD){++running->ticks;});
    SetEvent(worker.ready);
    worker.phase=1;auto result=LeanDockEvents::Pump(worker.stop);worker.phase=2;
    LeanDockEvents::ClearTimer(worker.timer);worker.phase=3;return result;
}
int main(){
    int checks=0,failed=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::printf("FAIL %s\n",name);}};
    Worker worker{};worker.stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);worker.ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    // Stop before the worker has any Windows message queue, then repeated
    // immediate Stop/Start cycles. The worker owns and clears its timer.
    for(int i=0;i<50;++i){
        ResetEvent(worker.stop);ResetEvent(worker.ready);worker.ticks=0;worker.phase=0;
        if(i==0)SetEvent(worker.stop);
        HANDLE thread=CreateThread(nullptr,0,Run,&worker,0,nullptr);
        check(thread!=nullptr,"event worker created");
        if(!thread)break;
        if(i%2){check(WaitForSingleObject(worker.ready,1000)==WAIT_OBJECT_0,"worker readiness");Sleep(35);}
        LeanDockEvents::RequestStop(worker.stop,GetThreadId(thread));
        auto exited=LeanDockEvents::WaitForExit(thread,3000)?WAIT_OBJECT_0:WAIT_TIMEOUT;
        check(exited==WAIT_OBJECT_0,"stop delivered even before message queue exists");
        if(exited!=WAIT_OBJECT_0){std::printf("iteration=%d ready=%lu timer=%llu ticks=%u phase=%u stop=%lu wait=%lu\n",i,WaitForSingleObject(worker.ready,0),(unsigned long long)worker.timer,worker.ticks.load(),worker.phase.load(),WaitForSingleObject(worker.stop,0),exited);return 2;} // never reuse handles under a live worker
        check(worker.timer==0,"pending timer cleared before next run");
        if(i%2)check(worker.ticks>0,"subsequent runs still dispatch timers");
        CloseHandle(thread);
    }
    CloseHandle(worker.stop);CloseHandle(worker.ready);
    std::printf("Event lifecycle: %d checks, %d failures\n",checks,failed);return failed?1:0;
}
