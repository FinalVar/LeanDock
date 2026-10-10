#pragma once
#include <windows.h>
#include <cstdint>
#include <cwchar>

namespace LeanDockBackdrop {
enum class Reason : unsigned { Desktop=1, Expansion=2, BeforeShow=3 };
inline UINT Message() {
    static const UINT message=RegisterWindowMessageW(L"LeanDock.Backdrop.Resume.1");
    return message;
}
inline bool IsDesktop(HWND window) {
    wchar_t type[64]{};
    return window && GetClassNameW(window,type,64) &&
        (!wcscmp(type,L"Progman")||!wcscmp(type,L"WorkerW"));
}
inline void Notify(HWND taskbar,Reason reason) {
    if(taskbar&&Message())PostMessageW(taskbar,Message(),static_cast<WPARAM>(reason),GetTickCount64());
}
// Called immediately before the retained native Unhide routine. Send only on
// the taskbar's own UI thread, so preparation completes before visibility is
// changed; never block another process/thread or synthesize user input.
inline void Prepare(HWND taskbar) {
    if(taskbar&&GetWindowThreadProcessId(taskbar,nullptr)==GetCurrentThreadId()&&Message())
        SendMessageW(taskbar,Message(),static_cast<WPARAM>(Reason::BeforeShow),GetTickCount64());
}
inline bool Fresh(std::uint64_t at,std::uint64_t now) {
    return at&&now>=at&&now-at<=1000;
}
// Two finite commits per latest transition. A late timer from an earlier
// transition cannot refresh the new scene or stop its timer. No idle polling.
struct Plan {
    static constexpr std::uint64_t FirstDelay=16,SettleDelay=160;
    std::uint64_t generation{},due{};
    unsigned remaining{};
    std::uint64_t Begin(std::uint64_t now) {
        ++generation;remaining=2;due=now+FirstDelay;return generation;
    }
    void Cancel(){++generation;remaining=0;due=0;}
    bool Current(std::uint64_t token)const{return remaining&&token==generation;}
    bool Ready(std::uint64_t token,std::uint64_t now)const{return Current(token)&&now>=due;}
    bool Advance(std::uint64_t token,std::uint64_t now) {
        if(!Ready(token,now))return false;
        if(--remaining){due=now+SettleDelay;return true;}
        due=0;return false;
    }
};
inline void Put(HWND window,const wchar_t* key,std::uint64_t value) {
    if(window)SetPropW(window,key,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(value)+1));
}
inline void Count(HWND window,const wchar_t* key) {
    const auto previous=reinterpret_cast<ULONG_PTR>(GetPropW(window,key));
    Put(window,key,previous?previous:1);
}
}
