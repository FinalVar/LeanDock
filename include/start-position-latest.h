#pragma once
#include "panel-position-latest.h"

namespace LeanDockStartPlacement {
// Start owns a responsive CoreWindow which can be as tall as the work area.
// Merely clamping its top to zero then leaves its bottom over the Dock.
// Reserve the shown Dock + 8 DIP before the native move/size is committed;
// retain native width, activation, visibility and z-order. Small displays use
// Start's existing responsive/scrolling layout, never a cropped screenshot.
inline bool Adjust(WINDOWPOS& pos,const RECT& current,const RECT& dock,
                   const RECT& monitor,UINT dpi,RECT* requested=nullptr) {
    if((pos.flags&(SWP_NOMOVE|SWP_NOSIZE))==(SWP_NOMOVE|SWP_NOSIZE)&&
       !(pos.flags&SWP_SHOWWINDOW))return false;
    const LONG width=(pos.flags&SWP_NOSIZE)?current.right-current.left:pos.cx;
    const LONG nativeHeight=(pos.flags&SWP_NOSIZE)?current.bottom-current.top:pos.cy;
    if(width<100||nativeHeight<100||width>monitor.right-monitor.left||
       nativeHeight>monitor.bottom-monitor.top||dock.left<monitor.left||
       dock.right>monitor.right||dock.left>=dock.right||dock.top<monitor.top||
       dock.bottom>monitor.bottom||dpi<48||dpi>768)return false;
    const LONG bottom=dock.top-MulDiv(8,dpi,96);
    const LONG height=std::min(nativeHeight,bottom-monitor.top);
    if(height<100)return false;
    const LONG x=(pos.flags&SWP_NOMOVE)?current.left:pos.x;
    const LONG y=(pos.flags&SWP_NOMOVE)?current.top:pos.y;
    if(requested)*requested={x,y,x+width,y+nativeHeight};
    const LONG targetX=std::clamp(dock.left+(dock.right-dock.left-width)/2,
        monitor.left,monitor.right-width);
    const LONG targetY=bottom-height;
    if(x==targetX&&y==targetY&&nativeHeight==height)return false;
    pos.x=targetX;pos.y=targetY;pos.flags&=~SWP_NOMOVE;
    if(nativeHeight!=height){pos.cx=width;pos.cy=height;pos.flags&=~SWP_NOSIZE;}
    return true;
}
}
