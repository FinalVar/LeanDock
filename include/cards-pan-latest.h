#pragma once
#include <algorithm>
#include <cmath>
namespace LeanDockCards {
struct PanCommand {int delta{};bool horizontal=false;};
// DirectManipulation content offsets are physical motion, not WHEEL_DELTA.
// Convert a deliberate 48 DIP pan to one step for the entire contact sequence.
// Reset at the gesture boundary; distance and direction changes cannot rearm it.
struct CardPan {
    float lastX{},lastY{},pending{};unsigned axis{};bool pinch=false,committed=false;
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
        if(std::fabs(pending)<48)return {};
        const int direction=pending>0?1:-1;
        committed=true;pending=0;
        return {direction*(axis==2?-120:120),axis==2};
    }
};
}
