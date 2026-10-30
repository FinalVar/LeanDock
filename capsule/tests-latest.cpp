#include "policy-latest.h"
#include <cassert>
#include <cstdio>
using namespace Capsule;
int main(){
    int checks=0;
    auto check=[&](bool b){++checks;if(!b){printf("FAILED %d\n",checks);abort();}};
    Reveal r;
    check(r.Tick({1,true,true,false,false,true,false,false})==Change::None);
    check(r.Tick({2,true,true,false,false,false,true,false})==Change::None);
    check(r.Tick({3,true,true,false,false,false,false,false})==Change::Show);
    check(r.Tick({100,false,true})==Change::None);
    check(r.Tick({200,false,false})==Change::None);
    check(r.Tick({699,false,false})==Change::None);
    check(r.Tick({700,false,false})==Change::Hide);
    check(r.Tick({710,true,true})==Change::None); // must leave/re-enter
    r.Tick({720,false,true});
    check(r.Tick({730,true,true})==Change::Show);
    r.Tick({1000,false,false});
    check(r.Tick({1400,false,false,true})==Change::None);
    r.Tick({1500,false,false});
    check(r.Tick({1900,false,false,false,true})==Change::None);
    check(r.Toggle()==Change::None&&r.pinned);
    check(r.Tick({3000,false,false})==Change::None);
    check(r.Tick({4000,false,false,false,false,true,false,true})==Change::Hide);
    check(r.Toggle()==Change::Show&&r.pinned);
    check(r.Toggle()==Change::Hide&&!r.pinned);
    StartHold start;
    check(!start.Update(0,true)); // Unrelated shell windows cannot pin the Dock.
    start.Request(100);
    check(start.Update(150,false)); // Allow native Start its asynchronous open.
    check(start.Update(200,true));
    check(start.Update(9000,true)); // No timeout while actually visible.
    check(!start.Update(9001,false));
    check(!start.Update(9010,true)); // Closing ends this request's lifetime.
    start.Request(10000);
    check(start.Update(11499,false));
    check(!start.Update(11500,false)); // Failed dispatch must not stick forever.
    start.Request(12000);start.Clear();check(!start.Update(12001,true));
    r.Show();start.Request(13000);
    check(r.Tick({14000,false,false,false,start.Update(14000,true)})==Change::None);
    check(r.Tick({20000,false,false,false,start.Update(20000,true)})==Change::None&&r.visible);
    check(r.Tick({20001,false,false,false,start.Update(20001,false)})==Change::None);
    check(r.Tick({20501,false,false,false,false})==Change::Hide);
    RestartBudget b;
    check(b.Take(1));check(b.Take(2));check(b.Take(3));check(!b.Take(4));
    check(b.Take(60001));check(!b.Take(60001));check(b.Take(60002));
    Rect c{100,200,400,Height};
    check(!CapsuleHit(c,100,200));check(CapsuleHit(c,100,230));
    check(CapsuleHit(c,499,230));check(!CapsuleHit(c,500,230));
    check(!CapsuleHit(c,100,259));check(CapsuleHit(c,130,201));
    check(Height==2*Radius&&Height+BottomGap==64);
    check(HiddenOffset>Height+BottomGap&&HoldBand>Height+BottomGap);
    check((Height-IconSize)/2+IconSize<Height-DotBottom-DotSize);
    check(FitAppPitch(6*52,6)==52&&FitAppPitch(6*52-1,6)==48);
    check(FitAppPitch(6*48-1,6)==44&&FitAppPitch(6*44-1,6)==40);
    check(FitAppPitch(6*52,6,48)==48&&FitAppPitch(6*52+8,6,48)==52);
    check(FitAppPitch(0,0)==52);
    Motion m;m.Begin(true,1000,true);
    auto f=m.At(1500);check(std::abs(f.width-400)<.001&&std::abs(f.y)<.001);
    bool overshoot=false;
    for(int i=0;i<=500;i++)if(m.At(1000+i).width>400)overshoot=true;
    check(overshoot);
    auto before=m.At(1600);m.Begin(false,1600,true);
    check(std::abs(before.width-m.At(1600).width)<.001);
    check(!m.Hidden(1859));check(m.Hidden(1860));
    m.Begin(true,2000,false);check(m.At(2000).width==400);
    m.Begin(false,2001,false);check(m.Hidden(2001));
    m.Begin(true,3000,true);
    auto resizeStart=m.At(3200);m.Resize(560,150,3200);
    check(std::abs(m.At(3200).width-resizeStart.width)<.001);
    check(std::abs(m.At(3500).width-560)<.001);
    m.Resize(480,150,3600);auto interrupted=m.At(3700);m.Begin(false,3700,true);
    check(std::abs(m.At(3700).width-interrupted.width)<.001);
    check(std::abs(m.At(4000).width-150)<.001&&m.Hidden(4000));
    Position position;position.Move(500,1000,false);check(position.At(1000)==500);
    position.Move(350,2000,true);check(std::abs(position.At(2000)-500)<.001);
    check(std::abs(position.At(2300)-350)<.001);
    position.Move(600,2400,true);auto halfway=position.At(2500);
    position.Move(450,2500,true);check(std::abs(position.At(2500)-halfway)<.001);
    check(std::abs(position.At(2800)-450)<.001);
    for(auto scale:{1.0,1.25,1.5,1.75,2.0}){
        Rect scaled{c.x*scale,c.y*scale,c.w*scale,c.h*scale};
        check(CapsuleHit(scaled,300*scale,230*scale));
        check(!CapsuleHit(scaled,100*scale,200*scale));
        // Visible artwork stays centered and the dot never crosses the body.
        for(float pitch:{52.f,48.f,44.f,40.f}){
            const double icon=IconSize*pitch/AppPitch;
            check(std::abs(((Height-icon)/2+icon/2)*scale-Height*scale/2)<.001);
            check((Height-DotBottom)*scale<scaled.h);
        }
    }
    printf("PASS %d policy checks\n",checks);
}
