#pragma once
#include "../core/latest.h"
namespace nhc {
enum class StartGateResult { Idle, Wait, Execute, Cancel };
struct StartGate {
    Action action=None;int corner=-1;uint64_t deadline=0;
    void Clear(){action=None;corner=-1;deadline=0;}
    void Arm(Action next,int c,uint64_t now){action=next;corner=c;deadline=now+700;}
    StartGateResult Poll(uint64_t now,bool visible,bool foregroundStillStart,bool interrupted)const{
        if(action==None)return StartGateResult::Idle;
        if(interrupted||now>=deadline)return StartGateResult::Cancel;
        return visible||foregroundStillStart?StartGateResult::Wait:StartGateResult::Execute;
    }
};
}
