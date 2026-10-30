#include "../include/switcher-confirm-policy-latest.h"
#include <cstdio>
#include <cstdlib>
using namespace NativeSwitcherPolicy;
static unsigned checks;
static void check(bool value,const char* label) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}
}
int main() {
    Request request;
    check(request.Peek(100)==0,"manual invocation has no ticket");
    auto first=request.Arm(100);
    check(first!=0 && request.Peek(100)==first,"corner arms request");
    check(!request.Consume(0,110),"zero cannot consume request");
    auto second=request.Arm(100);
    check(second!=first,"same-tick activation has a new identity");
    check(!request.Consume(first,120),"old UI callback cannot consume newer request");
    check(request.Consume(second,120),"current request consumed");
    check(!request.Consume(second,121),"reentrant confirmation fires once");
    auto third=request.Arm(200);
    check(request.Peek(1700)==third,"deadline inclusive");
    check(request.Peek(1701)==0,"expired request cleared without timer");
    check(!request.Consume(third,1701),"late callback cannot switch windows");
    auto fourth=request.Arm(2000);request.Cancel();
    check(!request.Consume(fourth,2001),"failed input cancels automatic confirmation");
    check(Decide(false,2,1,true)==Decision::Wait,"background host cannot activate");
    check(Decide(true,0,-1,false)==Decision::Wait,"empty list not ready");
    check(Decide(true,2,-1,true)==Decision::Wait,"native selection must be ready");
    check(Decide(true,2,2,true)==Decision::Wait,"out-of-range selection rejected");
    check(Decide(true,1,0,true)==Decision::KeepOpen,"single window keeps native behavior");
    check(Decide(true,3,1,true)==Decision::KeepOpen,"three windows require user choice");
    check(Decide(true,20,1,true)==Decision::KeepOpen,"many windows require user choice");
    check(Decide(true,3,-1,false)==Decision::KeepOpen,"three unfocused items cancel auto request");
    check(Decide(true,1,-1,false)==Decision::KeepOpen,"single unfocused item cancels auto request");
    check(Decide(true,2,1,false)==Decision::Wait,"missing native provider is not replaced by Enter");
    check(Decide(true,2,1,true)==Decision::Confirm,"native second selection confirmed");
    check(Decide(true,2,0,true)==Decision::Confirm,"native first selection from desktop retained");
    std::printf("PASS: %u switcher confirmation checks; no desktop input sent\n",checks);
}
