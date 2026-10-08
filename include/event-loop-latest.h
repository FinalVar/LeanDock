#pragma once
#include <windows.h>
namespace LeanDockEvents {
// A stop event exists before the worker starts. It cannot be lost like a
// PostThreadMessage sent before Windows has created that thread's queue.
inline DWORD Pump(HANDLE stop) {
    MSG message{};
    // Establish the queue before testing the stop event. If RequestStop ran
    // before the queue existed, the event carries that request. After this
    // point its posted wake-up cannot be lost between the check and GetMessage.
    PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
    while(WaitForSingleObject(stop,0)==WAIT_TIMEOUT){
        auto result=GetMessageW(&message,nullptr,0,0);
        if(result<=0)return result<0?GetLastError():0;
        if(WaitForSingleObject(stop,0)!=WAIT_TIMEOUT || (!message.hwnd && message.message==WM_APP))return 0;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    return 0;
}
inline void RequestStop(HANDLE stop,DWORD threadId) {
    SetEvent(stop);PostThreadMessageW(threadId,WM_APP,0,0);
}
inline bool WaitForExit(HANDLE thread,DWORD timeout) {
    const auto deadline=GetTickCount64()+timeout;
    for(;;){
        if(WaitForSingleObject(thread,0)==WAIT_OBJECT_0)return true;
        const auto now=GetTickCount64();if(now>=deadline)return false;
        const auto result=MsgWaitForMultipleObjectsEx(1,&thread,DWORD(deadline-now),QS_SENDMESSAGE,MWMO_INPUTAVAILABLE);
        if(result==WAIT_OBJECT_0)return true;
        if(result!=WAIT_OBJECT_0+1)return false;
        // Deliver only synchronous native callbacks required for worker exit.
        // Do not dispatch posted start/stop commands inside this join. Owners
        // must still gate reentrant sent requests before starting teardown.
        MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
    }
}
inline void ClearTimer(UINT_PTR& timer) {
    if(timer)KillTimer(nullptr,timer);
    timer=0;
}
}
