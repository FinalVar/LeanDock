#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace LeanDockAggregate {
inline constexpr double Width=72,Height=46,Inset=1.6,Stroke=1.4;
// Native buttons and tray icons already own their inner padding. Reserve only
// the key and a small leading gap; do not inherit the old dashboard gutters.
inline constexpr double KeyOffset=6,SlotWidth=Width+KeyOffset;
inline constexpr double Pi=3.14159265358979323846;
// Plugging in can change warning color without changing the percentage.
inline int BatteryTone(int percent,bool plugged){return percent<0||plugged||percent>=20?0:percent<10?2:1;}
struct Segment {bool arc=false;double x{},y{},radius{},length{};};
// Exact same stadium and arc-length mapping as capsule/time-key-latest.h.
// No raster upscaling: the native card builds XAML line/arc geometry.
inline std::vector<Segment> Ring(double percent){
    const double radius=Height/2-Inset,left=Inset+radius,right=Width-left;
    const double straight=right-left;
    double remaining=(2*straight+2*Pi*radius)*std::clamp(percent,0.,100.);
    remaining/=100;double x=Width/2;
    std::vector<Segment> result;
    auto line=[&](double end,double y,double length){if(remaining<=0)return;
        const double take=std::min(remaining,length);x+=(end-x)*take/length;
        result.push_back({false,x,y,0,take});remaining=std::max(0.,remaining-take);};
    auto arc=[&](double center,double start){if(remaining<=0)return;
        const double take=std::min(remaining,Pi*radius),angle=start+take/radius;
        x=center+radius*std::cos(angle);result.push_back({true,x,Height/2+radius*std::sin(angle),radius,take});
        remaining=std::max(0.,remaining-take);};
    line(right,Inset,straight/2);arc(right,-Pi/2);line(left,Height-Inset,straight);arc(left,Pi/2);line(Width/2,Inset,straight/2);
    return result;
}
}
