#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>

// Bounded scalar properties published by Explorer's XAML layout. No pointers,
// files, polling, or cross-process executable messages are exchanged.
namespace LeanDockGeometry {
inline constexpr wchar_t Ready[] = L"LeanDock.Geometry.v1.Ready";
inline constexpr wchar_t Left[] = L"LeanDock.Geometry.v1.Left";
inline constexpr wchar_t Right[] = L"LeanDock.Geometry.v1.Right";
inline constexpr wchar_t Height[] = L"LeanDock.Geometry.v1.Height";
inline int Scalar(HWND window,const wchar_t* key) {
    return static_cast<int>(reinterpret_cast<intptr_t>(GetPropW(window,key)))-1;
}
inline void Clear(HWND window) {
    for(auto key:{Ready,Left,Right,Height})RemovePropW(window,key);
}
inline bool ValidSpan(int left,int right,int height,int width) {
    return left>=0 && right>left && right<=width && height>=24 && height<=400;
}
inline void Publish(HWND window,int left,int right,int height) {
    RECT r{};if(!GetWindowRect(window,&r) || !ValidSpan(left,right,height,r.right-r.left))return;
    if(GetPropW(window,Ready) && Scalar(window,Left)==left && Scalar(window,Right)==right && Scalar(window,Height)==height)return;
    RemovePropW(window,Ready);
    if(SetPropW(window,Left,reinterpret_cast<HANDLE>(static_cast<intptr_t>(left+1))) &&
       SetPropW(window,Right,reinterpret_cast<HANDLE>(static_cast<intptr_t>(right+1))) &&
       SetPropW(window,Height,reinterpret_cast<HANDLE>(static_cast<intptr_t>(height+1))))
        SetPropW(window,Ready,reinterpret_cast<HANDLE>(1));
}
inline bool Read(HWND window,RECT& dock,MONITORINFO* info=nullptr) {
    if(!GetPropW(window,Ready))return false;
    RECT r{};if(!GetWindowRect(window,&r))return false;
    int left=Scalar(window,Left),right=Scalar(window,Right),height=Scalar(window,Height);
    if(!GetPropW(window,Ready) || !ValidSpan(left,right,height,r.right-r.left))return false;
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return false;
    // Project the fully shown Dock, never the current slide-animation position.
    dock={r.left+left,monitor.rcMonitor.bottom-height,r.left+right,monitor.rcMonitor.bottom};
    if(dock.left<monitor.rcMonitor.left || dock.right>monitor.rcMonitor.right)return false;
    if(info)*info=monitor;
    return true;
}
inline bool InSpan(const RECT& dock,LONG x) {return x>=dock.left && x<dock.right;}
inline bool RejectPointerReveal(HWND window) {
    RECT dock{};MONITORINFO monitor{};POINT p{};
    if(!Read(window,dock,&monitor) || !GetCursorPos(&p))return false;
    return p.y>=monitor.rcMonitor.bottom-2 && p.y<monitor.rcMonitor.bottom &&
           p.x>=monitor.rcMonitor.left && p.x<monitor.rcMonitor.right && !InSpan(dock,p.x);
}
inline POINT HostPosition(const RECT& dock,const RECT& work,LONG width,LONG height,int dpi,bool rightAligned) {
    const LONG x=rightAligned?dock.right+MulDiv(12,dpi,96)-width:dock.left+(dock.right-dock.left-width)/2;
    return {std::clamp(x,work.left,std::max(work.left,work.right-width)),
            dock.top+MulDiv(4,dpi,96)-height};
}
}
