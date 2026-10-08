#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace LeanDockCards {
// UI-thread owned. Invalidate before cancellation, so both synchronous and
// delayed completions from the previous media session are harmless.
struct CommandGate {
    uint64_t serial=0;bool busy=false;
    uint64_t Begin(){if(busy)return 0;busy=true;return ++serial;}
    void Invalidate(){++serial;busy=false;}
    bool Finish(uint64_t ticket){if(!ticket||!busy||ticket!=serial)return false;busy=false;return true;}
};
inline constexpr double CardWidth=160, CardHeight=44, SlotWidth=184;
enum class Page : unsigned { Metrics, Media, Power };
inline Page Cycle(Page page,int direction) {
    return static_cast<Page>((static_cast<int>(page)+(direction<0?2:direction>0?1:0))%3);
}
// One wheel event advances at most one page. Keep high-resolution residuals;
// positive wheel movement goes backwards, negative movement goes forwards.
inline int WheelStep(int delta,int& remainder) {
    remainder+=std::clamp(delta,-120,120);
    if(remainder>=120){remainder-=120;return -1;}
    if(remainder<=-120){remainder+=120;return 1;}
    return 0;
}
struct Battery {
    int percent=-1;
    bool present=false,presentKnown=false,charging=false,plugged=false,supplyKnown=false,low=false,critical=false;
};
enum class BatteryMode : unsigned { Unknown, NoBattery, OnBattery, Low, Critical, Charging, Plugged, Full };
inline Battery BatteryState(bool valid,unsigned percent,unsigned flags,unsigned ac) {
    Battery result;
    if(!valid)return result;
    result.plugged=ac==1;result.supplyKnown=ac<=1;
    if(flags==255)return result;
    result.presentKnown=true;result.present=(flags&128)==0;
    if(result.present){
        result.percent=percent<=100?int(percent):-1;
        result.charging=(flags&8)!=0;
        result.low=(flags&2)!=0||(result.percent>=0&&result.percent<=20);
        result.critical=(flags&4)!=0||(result.percent>=0&&result.percent<=10);
    }
    return result;
}
inline BatteryMode Mode(Battery const& battery) {
    if(!battery.presentKnown)return BatteryMode::Unknown;
    if(!battery.present)return BatteryMode::NoBattery;
    if(battery.charging)return BatteryMode::Charging;
    if(battery.plugged)return battery.percent==100?BatteryMode::Full:BatteryMode::Plugged;
    if(!battery.supplyKnown)return BatteryMode::Unknown;
    if(battery.critical)return BatteryMode::Critical;
    if(battery.low)return BatteryMode::Low;
    return BatteryMode::OnBattery;
}
inline const wchar_t* BatteryCaption(Battery const& battery) {
    switch(Mode(battery)) {
    case BatteryMode::NoBattery:return battery.plugged?L"已接通电源 · 无电池":L"未检测到电池";
    case BatteryMode::Charging:return L"正在充电";
    case BatteryMode::Plugged:return L"已插电 · 未充电";
    case BatteryMode::Full:return L"已充满";
    case BatteryMode::Critical:return L"电量极低";
    case BatteryMode::Low:return L"电量偏低";
    case BatteryMode::OnBattery:return L"使用电池";
    default:return battery.plugged?L"已插电 · 电量未知":L"电源状态未知";
    }
}
struct CpuTimes { uint64_t idle, kernel, user; };
// GetSystemTimes reports idle time as part of kernel time. Reject counter
// resets and zero-length intervals instead of presenting an invented sample.
inline std::optional<unsigned> CpuPercent(CpuTimes a, CpuTimes b) {
    if(b.idle<a.idle || b.kernel<a.kernel || b.user<a.user)return {};
    const auto idle=b.idle-a.idle, kernel=b.kernel-a.kernel, user=b.user-a.user;
    const double total=double(kernel)+double(user);
    if(total<=0 || double(idle)>total)return {};
    return unsigned(std::clamp(std::lround(100.0*(total-double(idle))/total),0l,100l));
}
inline bool Fits(double available,double apps,double tray,bool alreadyShown,double slot=SlotWidth) {
    if(!std::isfinite(available)||!std::isfinite(apps)||!std::isfinite(tray) ||
       !std::isfinite(slot)||available<=0||apps<=0||tray<0||slot<=0)return false;
    // Space for the original root padding/border and outer 12-DIP margins.
    // DesiredSize can be capped by the parent's measure constraint. Collapse
    // before that cap rather than mistaking clipped app content for a fit.
    // Four additional DIP on entry prevent fractional-DPI oscillation.
    return available>=apps+tray+25+slot+24+(alreadyShown?4:8);
}
inline bool ShouldSample(bool shown,bool metrics,bool sessionActive) {
    return shown && metrics && sessionActive;
}
}
