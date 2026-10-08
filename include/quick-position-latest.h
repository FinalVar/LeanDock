#pragma once
#include "dock-geometry-latest.h"

namespace LeanDockQuick {
inline constexpr UINT_PTR SubclassId=0x4c445151;
inline constexpr wchar_t Active[]=L"LeanDock.Quick.Active";
inline constexpr wchar_t Owner[]=L"LeanDock.Quick.Owner";
inline constexpr wchar_t Adjustments[]=L"LeanDock.Quick.Adjustments";
inline UINT Signal(){static const UINT value=RegisterWindowMessageW(L"LeanDock.QuickPosition.v1");return value;}
enum Command:WPARAM { Enable=1,Disable,Query,OwnerGone };

// Change only position, before Windows commits it. Preserve native dimensions,
// visibility, activation and z-order; a hidden window keeps its Dock placement.
inline bool Adjust(WINDOWPOS& pos,const RECT& current,const RECT& dock,
                   const RECT& monitor,UINT dpi,RECT* requested=nullptr) {
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
    auto target=LeanDockGeometry::HostPosition(dock,monitor,width,height,dpi,true);
    // Retain access to the panel on a short monitor or a tall media flyout.
    target.y=std::clamp(target.y,monitor.top,monitor.bottom-height);
    if(x==target.x&&y==target.y)return false;
    pos.x=target.x;pos.y=target.y;pos.flags&=~SWP_NOMOVE;return true;
}
}
