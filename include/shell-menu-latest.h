#pragma once
#include "shell-actions-latest.h"
#include "dock-geometry-latest.h"
#include <mutex>
namespace LeanDockMenu {
struct Gesture {HWND taskbar{};POINT click{};ULONGLONG at{};};
extern std::mutex gestureMutex;
extern Gesture gesture;
inline bool Fresh(const Gesture& value,ULONGLONG now) {return value.taskbar&&now>=value.at&&now-value.at<=1500;}
inline bool Take(Gesture& result) {
    std::lock_guard lock(gestureMutex);result=gesture;gesture={};return Fresh(result,GetTickCount64());
}
inline void Put(HWND window,const wchar_t* name,ULONG_PTR value) {
    if(window)SetPropW(window,name,reinterpret_cast<HANDLE>(value+1));
}
inline POINT AnchorPoint(POINT click,const RECT& dock,int dpi) {
    return {std::clamp(click.x,dock.left,dock.right-1),dock.top-MulDiv(8,dpi>0?dpi:96,96)};
}
inline bool Popup(HWND window,DWORD process) {
    DWORD pid=0;wchar_t type[64]{};
    return window&&GetWindowThreadProcessId(window,&pid)&&pid==process&&IsWindowVisible(window)&&
        GetClassNameW(window,type,64)&&!wcscmp(type,L"Xaml_WindowedPopupClass");
}
inline void Cancel() {std::lock_guard lock(gestureMutex);gesture={};}
// The native invocation consumes this one-use gesture in its XAML menu entry.
// Do not move/subclass the resulting HWND: XAML owns placement and hit testing.
inline bool Request(HWND taskbar) {
    if(!LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))return false;
    const auto previous=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Shell.LastRequest"));
    if(previous&&GetTickCount64()-previous<200)return true;
    Gesture next{taskbar,{},GetTickCount64()};if(!GetCursorPos(&next.click))return false;
    {std::lock_guard lock(gestureMutex);gesture=next;}
    for(auto key:{L"LeanDock.Menu.NativeAnchor",L"LeanDock.Menu.Error"})Put(taskbar,key,0);
    if(LeanDockShell::Request(taskbar,LeanDockShell::Action::PowerUser))return true;
    Cancel();return false;
}
}
