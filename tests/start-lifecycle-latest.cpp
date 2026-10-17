#include "../include/start-host-latest.h"
// Compile-time tests exercise the exact shared policy used by Start and Quick
// Settings. They do not claim native process retirement or hook acceptance.
constexpr bool NormalRetirement() {
    LeanDockQuick::RestartPolicy p;
    return p.Retired(true,0,1000)&&p.recoveries==1&&!p.blocked;
}
constexpr bool CrashStops(unsigned code,bool known=true) {
    LeanDockQuick::RestartPolicy p;
    return !p.Retired(known,code,1000)&&p.blocked&&
        !p.Retired(true,0,70000)&&p.recoveries==0;
}
constexpr bool BurstStops() {
    LeanDockQuick::RestartPolicy p;
    return p.Retired(true,0,1000)&&p.Retired(true,0,1500)&&
        !p.Retired(true,0,2000)&&p.recoveries==2&&p.blocked;
}
constexpr bool BudgetResets() {
    LeanDockQuick::RestartPolicy p;
    return p.Retired(true,0,1000)&&p.Retired(true,0,2000)&&
        p.Retired(true,0,61000)&&p.recent==1&&p.recoveries==3&&!p.blocked;
}
static_assert(NormalRetirement());
static_assert(CrashStops(0,false));
static_assert(CrashStops(1));
static_assert(CrashStops(259));
static_assert(CrashStops(0xc0000005));
static_assert(CrashStops(0xc0000409));
static_assert(BurstStops());
static_assert(BudgetResets());

constexpr bool InitRetryRecovers() {
    LeanDockStart::AttachPolicy p;
    if(!p.Begin(1000,true)||p.Begin(1001,true)||!p.Retryable())return false;
    if(!p.Begin(1750,false))return false;
    p.Success();
    return p.attempts==0&&p.recoveries==1&&!p.stopped;
}
constexpr bool InitBudgetIsFinite() {
    LeanDockStart::AttachPolicy p;
    return p.Begin(1000,true)&&p.Begin(1750,false)&&p.Begin(2500,false)&&
        !p.Retryable()&&!p.Begin(3250,false)&&!p.Begin(61000,false);
}
constexpr bool OnlyNativeEventRenews() {
    LeanDockStart::AttachPolicy p;
    return p.Begin(1000,true)&&p.Begin(1750,false)&&p.Begin(2500,false)&&
        !p.Begin(60999,true)&&p.Begin(61000,true)&&p.attempts==1;
}
constexpr bool TransportFailureStaysStopped() {
    LeanDockStart::AttachPolicy p;
    p.Begin(1000,true);p.Stop();
    return !p.Retryable()&&!p.Begin(2000,false)&&!p.Begin(61000,true);
}
constexpr bool AlreadyActiveAdoption() {
    LeanDockStart::AttachPolicy p;
    p.Begin(1000,true);p.Stop();p.Success();
    return p.attempts==0&&!p.stopped&&p.recoveries==0;
}
constexpr bool HostLifetimeResetsReadinessOnly() {
    LeanDockStart::AttachPolicy p;
    LeanDockQuick::RestartPolicy retirement;
    p.Begin(1000,true);p.Stop();
    if(!retirement.Retired(true,0,2000))return false;
    p={};
    return p.Begin(2000,true)&&retirement.recoveries==1;
}
static_assert(InitRetryRecovers());
static_assert(InitBudgetIsFinite());
static_assert(OnlyNativeEventRenews());
static_assert(TransportFailureStaysStopped());
static_assert(AlreadyActiveAdoption());
static_assert(HostLifetimeResetsReadinessOnly());
