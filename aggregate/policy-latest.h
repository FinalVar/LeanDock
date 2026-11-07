#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>

namespace Capsule {
// Accepted 0.8.78 geometry: 64 DIP host = 60 DIP surface + 4 DIP gap.
// See baseline/settings-latest.json and baseline/styles-latest.json.
inline constexpr float Height=60, Radius=30, BottomGap=4, HoldBand=92, ShadowPad=32;
inline constexpr float IconSize=40, AppPitch=52, AppPadding=12, DotSize=4, DotBottom=3;
// User-selected A: a short stadium, not a widened pill or rounded rectangle.
inline constexpr float TimeWidth=72, TimeHeight=46, TimeInset=(Height-TimeHeight)/2;
inline constexpr float HiddenOffset=Height+BottomGap+8, PopupGap=8, DetailGap=10;
// Port of the accepted dashboard's width steps and hysteresis; leave the
// information region intact and compact app buttons only when necessary.
inline float FitAppPitch(float budget,unsigned count,float previous=AppPitch) {
    if(!count||!std::isfinite(budget))return AppPitch;
    for(float width:{52.f,48.f,44.f,40.f})
        if(count*width+(width>previous?8.f:0.f)<=budget)return width;
    return 40;
}
inline constexpr uint64_t HideDelay=500, HoverDelay=400, HoverDismiss=250;
// A successful command is only a pending request. Keep the Dock during Start
// startup, then for its observed visible lifetime; a failed launch expires.
struct StartHold {
    bool requested=false,observed=false;
    uint64_t until=0;
    void Request(uint64_t now){requested=true;observed=false;until=now+1500;}
    void Clear(){requested=observed=false;until=0;}
    bool Update(uint64_t now,bool visible){
        if(!requested)return false;
        if(visible){observed=true;return true;}
        if(observed||now>=until){Clear();return false;}
        return true;
    }
};
struct Rect {
    double x{},y{},w{},h{};
    bool Contains(double px,double py,double pad=0) const {
        return px>=x-pad && py>=y-pad && px<x+w+pad && py<y+h+pad;
    }
};
inline bool CapsuleHit(Rect r,double px,double py) {
    if(!r.Contains(px,py))return false;
    const double radius=r.h/2;
    const double cx=std::clamp(px,r.x+radius,r.x+r.w-radius);
    const double dy=py-r.y-radius,dx=px-cx;
    return dx*dx+dy*dy<=radius*radius;
}
struct Input {
    uint64_t now{};
    bool edge{},band{},detail{},menu{},buttons{},fullscreen{},outsidePress{};
};
enum class Change { None, Show, Hide };
// Monotonic, UI-thread-owned state. No UI calls or callbacks in the policy.
struct Reveal {
    bool visible=false,pinned=false,armed=true;
    uint64_t leaveAt=0;
    bool leaving=false;
    Change Show(bool keyboard=false) {
        pinned=keyboard;leaving=false;
        if(visible)return Change::None;
        visible=true;return Change::Show;
    }
    Change Hide() {
        pinned=false;leaving=false;armed=false;
        if(!visible)return Change::None;
        visible=false;return Change::Hide;
    }
    Change Toggle() { return pinned ? Hide() : Show(true); }
    Change Tick(Input s) {
        if(!s.edge)armed=true;
        if(pinned&&s.outsidePress)return Hide();
        if(!visible && armed && s.edge&&!s.buttons&&!s.fullscreen)return Show();
        if(!visible)return Change::None;
        if(pinned||s.band||s.detail||s.menu){leaving=false;return Change::None;}
        if(!leaving){leaveAt=s.now;leaving=true;}
        if(s.now-leaveAt>=HideDelay)return Hide();
        return Change::None;
    }
};
// The hover panel leaves shortly after the pointer does. A click in it asks it
// to stay: from then on only a press elsewhere closes it. Typing stays too.
struct PanelStay {
    bool clicked=false;
    uint64_t outside=0;
    // inside: on the key or the panel, or dragging. pressed: a button went
    // down since the last tick, not on the panel.
    bool Close(bool inside,bool pressed,bool typing,uint64_t now) {
        if(inside){outside=0;return false;}
        if(pressed)return true;
        if(clicked||typing){outside=0;return false;}
        if(!outside){outside=now;return false;}
        return now-outside>=HoverDismiss;
    }
};
struct RestartBudget {
    std::deque<uint64_t> attempts;
    bool Take(uint64_t now) {
        while(!attempts.empty()&&now-attempts.front()>=60000)attempts.pop_front();
        if(attempts.size()>=3)return false;
        attempts.push_back(now);return true;
    }
};
struct Curve {double x1,y1,x2,y2;};
inline constexpr Curve Spring{.34,1.42,.55,1}, Sink{.5,0,.75,0}, Ease{.25,.1,.25,1};
// Panel reveal: Lift carries a ~1% overshoot for the glass, Settle is a long
// deceleration for the content that follows it in.
inline constexpr Curve Lift{.22,1.18,.36,1}, Settle{.1,.9,.2,1};
inline double Bezier(double t,double p1,double p2) {
    const double u=1-t;return 3*u*u*t*p1+3*u*t*t*p2+t*t*t;
}
// Used only for hit geometry, not for rendering: rendering runs in Composition.
inline double Progress(double elapsed,double duration,Curve c) {
    const double x=std::clamp(elapsed/duration,0.0,1.0);
    double lo=0,hi=1;
    for(int i=0;i<24;i++){double m=(lo+hi)/2;if(Bezier(m,c.x1,c.x2)<x)lo=m;else hi=m;}
    return Bezier((lo+hi)/2,c.y1,c.y2);
}
inline double Mix(double a,double b,double t){return a+(b-a)*t;}
struct Motion {
    uint64_t at=0;
    bool showing=false,enabled=true;
    double widthFrom=120,yFrom=HiddenOffset,scaleFrom=.96,alphaFrom=0;
    double full=400,base=120;
    uint64_t resizeAt=0;
    double resizeFrom=0;
    bool resizing=false;
    struct Frame{double width,y,scale,alpha;};
    Frame At(uint64_t now) const {
        if(!enabled)return showing?Frame{full,0,1,1}:Frame{base,HiddenOffset,.96,0};
        const double ms=double(now-at);
        const auto c=showing?Spring:Sink;
        const double d=showing?420:260;
        const double widthT=Progress(std::max(0.0,ms-(showing?40:0)),showing?460:260,c);
        const double alphaT=std::clamp((ms-(showing?0:80))/(showing?140:180),0.0,1.0);
        return {resizing&&showing?Mix(resizeFrom,full,Progress(double(now-resizeAt),300,Ease)):
                Mix(widthFrom,showing?full:base,widthT),
                Mix(yFrom,showing?0:HiddenOffset,Progress(ms,d,c)),
                Mix(scaleFrom,showing?1:.96,Progress(ms,d,c)),
                Mix(alphaFrom,showing?1:0,alphaT)};
    }
    void Begin(bool show,uint64_t now,bool animate) {
        auto f=At(now);widthFrom=f.width;yFrom=f.y;scaleFrom=f.scale;alphaFrom=f.alpha;
        at=now;showing=show;enabled=animate;resizing=false;
    }
    void Resize(double width,double minimum,uint64_t now) {
        auto f=At(now);full=width;base=minimum;
        resizing=showing&&enabled;resizeAt=now;resizeFrom=f.width;
    }
    bool Hidden(uint64_t now)const{return !showing&&(!enabled||now-at>=260);}
};
struct Position {
    double from=0,to=0;uint64_t at=0;bool enabled=false;
    double At(uint64_t now)const{return enabled?Mix(from,to,Progress(double(now-at),300,Ease)):to;}
    void Move(double value,uint64_t now,bool animate){from=At(now);to=value;at=now;enabled=animate;}
};
}
