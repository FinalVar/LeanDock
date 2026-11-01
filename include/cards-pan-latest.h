#pragma once
#include <algorithm>
#include <cmath>
namespace LeanDockCards {
struct PanCommand {int delta{};bool horizontal=false;};
// DirectManipulation content offsets are physical motion, not WHEEL_DELTA.
// Card paging (the default): a deliberate 48 DIP pan is one step for the
// entire contact sequence. Reset at the gesture boundary; distance and
// direction changes cannot rearm it. With repeat (the time key's levels),
// every further threshold of travel on the locked axis is another step, and
// the remainder carries over; each step is one WHEEL_DELTA in the result.
struct CardPan {
    float lastX{},lastY{},pending{};unsigned axis{};bool pinch=false,committed=false;
    float threshold=48;bool repeat=false;
    void Reset(){lastX=lastY=pending=0;axis=0;pinch=committed=false;}
    PanCommand Step(float scale,float x,float y){
        if(!std::isfinite(scale)||!std::isfinite(x)||!std::isfinite(y)||scale<=0){pinch=true;return {};}
        if(std::fabs(scale-1.f)>.01f)pinch=true;
        const auto dx=x-lastX,dy=y-lastY;lastX=x;lastY=y;
        if(pinch||committed)return {};
        if(!axis){
            if(std::max(std::fabs(x),std::fabs(y))<5)return {};
            axis=std::fabs(x)>std::fabs(y)?2:1;pending=axis==2?x:y;
        }else pending+=axis==2?dx:dy;
        if(!(threshold>0)||std::fabs(pending)<threshold)return {};
        const int direction=pending>0?1:-1;
        int steps=1;
        if(repeat){steps=int(std::fabs(pending)/threshold);pending-=direction*steps*threshold;}
        else{committed=true;pending=0;}
        return {direction*steps*(axis==2?-120:120),axis==2};
    }
};
}
