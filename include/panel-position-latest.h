#pragma once
#include "dock-geometry-latest.h"

namespace LeanDockPlacement {
// Called on the owning UI thread, after native size validation but before the
// window manager commits the move. No second move or visibility manipulation.
inline bool Adjust(WINDOWPOS& pos,const RECT& current,const RECT& dock,
                   const RECT& monitor,UINT dpi,bool rightAligned,RECT* requested=nullptr) {
    if((pos.flags&(SWP_NOMOVE|SWP_NOSIZE))==(SWP_NOMOVE|SWP_NOSIZE) &&
       !(pos.flags&SWP_SHOWWINDOW))return false;
    const LONG width=(pos.flags&SWP_NOSIZE)?current.right-current.left:pos.cx;
    const LONG height=(pos.flags&SWP_NOSIZE)?current.bottom-current.top:pos.cy;
    if(width<100||height<100||width>monitor.right-monitor.left||height>monitor.bottom-monitor.top||
       dock.left<monitor.left||dock.right>monitor.right||dock.left>=dock.right||
       dock.top<monitor.top||dock.bottom>monitor.bottom||dpi<48||dpi>768)return false;
    const LONG x=(pos.flags&SWP_NOMOVE)?current.left:pos.x;
    const LONG y=(pos.flags&SWP_NOMOVE)?current.top:pos.y;
    if(requested)*requested={x,y,x+width,y+height};
    auto target=LeanDockGeometry::HostPosition(dock,monitor,width,height,dpi,rightAligned);
    target.y=std::clamp(target.y,monitor.top,monitor.bottom-height);
    if(x==target.x&&y==target.y)return false;
    pos.x=target.x;pos.y=target.y;pos.flags&=~SWP_NOMOVE;return true;
}
inline bool DockFor(const RECT& proposed,RECT& dock,MONITORINFO& info) {
    const auto monitor=MonitorFromRect(&proposed,MONITOR_DEFAULTTONEAREST);
    for(auto cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}){
        HWND bar=nullptr;
        while((bar=FindWindowExW(nullptr,bar,cls,nullptr)))
            if(MonitorFromWindow(bar,MONITOR_DEFAULTTONEAREST)==monitor && LeanDockGeometry::Read(bar,dock,&info))return true;
    }
    return false;
}
}
