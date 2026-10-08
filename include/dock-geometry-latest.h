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
inline UINT ChangedMessage() {static const UINT value=RegisterWindowMessageW(L"LeanDock.Geometry.v1.Changed");return value;}
inline int Scalar(HWND window,const wchar_t* key) {
    return static_cast<int>(reinterpret_cast<intptr_t>(GetPropW(window,key)))-1;
}
inline uintptr_t Revision(HWND window) {return reinterpret_cast<uintptr_t>(GetPropW(window,Ready));}
inline void Clear(HWND window) {
    for(auto key:{Ready,Left,Right,Height})RemovePropW(window,key);
}
inline bool ValidSpan(int left,int right,int height,int width) {
    return left>=0 && right>left && right<=width && height>=24 && height<=400;
}
inline void Publish(HWND window,int left,int right,int height) {
    RECT r{};if(!GetWindowRect(window,&r) || !ValidSpan(left,right,height,r.right-r.left))return;
    if(Revision(window) && !(Revision(window)&1) && Scalar(window,Left)==left && Scalar(window,Right)==right && Scalar(window,Height)==height)return;
    // Single UI-thread writer. Odd revisions are in progress; readers must
    // observe the same completed revision on both sides of the scalar reads.
    // A boolean ready flag cannot detect a whole publication between reads.
    auto revision=(Revision(window)&~uintptr_t(1))+2;
    if(!revision)revision=2;
    if(!SetPropW(window,Ready,reinterpret_cast<HANDLE>(revision-1)))return;
    if(SetPropW(window,Left,reinterpret_cast<HANDLE>(static_cast<intptr_t>(left+1))) &&
       SetPropW(window,Right,reinterpret_cast<HANDLE>(static_cast<intptr_t>(right+1))) &&
       SetPropW(window,Height,reinterpret_cast<HANDLE>(static_cast<intptr_t>(height+1))))
        if(SetPropW(window,Ready,reinterpret_cast<HANDLE>(revision)))
            PostMessageW(window,ChangedMessage(),0,0);
}
inline bool Read(HWND window,RECT& dock,MONITORINFO* info=nullptr) {
    const auto revision=Revision(window);if(!revision || (revision&1))return false;
    RECT r{};if(!GetWindowRect(window,&r))return false;
    int left=Scalar(window,Left),right=Scalar(window,Right),height=Scalar(window,Height);
    if(Revision(window)!=revision || !ValidSpan(left,right,height,r.right-r.left))return false;
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return false;
    // Project the fully shown Dock, never the current slide-animation position.
    dock={r.left+left,monitor.rcMonitor.bottom-height,r.left+right,monitor.rcMonitor.bottom};
    if(dock.left<monitor.rcMonitor.left || dock.right>monitor.rcMonitor.right)return false;
    if(info)*info=monitor;
    return true;
}
inline bool InSpan(const RECT& dock,LONG x) {return x>=dock.left && x<dock.right;}
// Use one complete, fully shown snapshot. Never derive the obstacle from the
// animated/hidden window position, which would create a hide/show feedback loop.
// Unknown or wrong-monitor geometry leaves the native fallback intact.
inline bool OcclusionRect(HWND window,HMONITOR expected,RECT& nativeRect) {
    RECT dock{};
    if(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST)!=expected || !Read(window,dock))return false;
    nativeRect=dock;return true;
}
inline bool Overlaps(const RECT& window,const RECT& dock) {
    return window.left<window.right && window.top<window.bottom &&
           dock.left<dock.right && dock.top<dock.bottom &&
           window.left<dock.right && window.right>dock.left &&
           window.top<dock.bottom && window.bottom>dock.top;
}
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
