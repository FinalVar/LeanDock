#include "cards-core-latest.h"
#include <cstdio>
#include <limits>
int main(){using namespace LeanDockCards;int count=0,failed=0;
    auto check=[&](bool ok,const char* name){++count;if(!ok){++failed;std::printf("FAIL %s\n",name);}};
    check(CpuPercent({100,150,100},{180,250,200})==60,"kernel includes idle");
    check(CpuPercent({0,0,0},{200,200,0})==0,"all idle");
    check(CpuPercent({0,0,0},{0,100,100})==100,"all busy");
    check(!CpuPercent({20,30,40},{20,30,40}),"zero interval");
    check(!CpuPercent({20,30,40},{19,35,45}),"idle counter reset");
    check(!CpuPercent({20,30,40},{25,29,45}),"kernel counter reset");
    check(!CpuPercent({20,30,40},{25,35,39}),"user counter reset");
    check(!CpuPercent({0,0,0},{300,100,100}),"inconsistent counters");
    const double edge=520+132+25+SlotWidth+24;
    check(Fits(edge+8,520,132,false),"enter with hysteresis");
    check(!Fits(edge+7,520,132,false),"avoid entering on fractional edge");
    check(Fits(edge+4,520,132,true),"keep slot with measurement guard");
    check(!Fits(edge,520,132,true),"collapse when app measurement hits cap");
    check(!Fits(1920,0,132,false),"wait for apps geometry");
    check(!Fits(1920,520,0,false),"wait for tray geometry");
    check(!Fits(std::numeric_limits<double>::quiet_NaN(),520,132,false),"reject nan geometry");
    check(Fits(800,52,76,false),"one app stays compact");
    for(int a=0;a<2;++a)for(int b=0;b<2;++b)for(int c=0;c<2;++c)
        check(ShouldSample(a,b,c)==bool(a&&b&&c),"visible metrics active conjunction");
    int wheel=0;check(!AdvanceWheel(30,wheel),"small wheel delta");check(!AdvanceWheel(60,wheel),"accumulate partial wheel");
    check(AdvanceWheel(30,wheel),"one detent switches");check(wheel==0,"detent remainder reset");
    check(AdvanceWheel(-240,wheel),"large event switches only once");
    check(!AdvanceWheel(60,wheel)&&!AdvanceWheel(-60,wheel)&&wheel==0,"opposite partial deltas cancel");
    std::printf("Cards: %d checks, %d failures\n",count,failed);return failed?1:0;
}
