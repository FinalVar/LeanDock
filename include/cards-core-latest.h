#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <cstdio>
#include <string>

namespace LeanDockCards {
// UI-thread owned. Invalidate before cancellation, so both synchronous and
// delayed completions from the previous media session are harmless.
struct CommandGate {
    uint64_t serial=0;bool busy=false;
    uint64_t Begin(){if(busy)return 0;busy=true;return ++serial;}
    void Invalidate(){++serial;busy=false;}
    bool Finish(uint64_t ticket){if(!ticket||!busy||ticket!=serial)return false;busy=false;return true;}
};
inline constexpr double CardWidth=104, CardHeight=44, SlotWidth=CardWidth+24;
inline constexpr double RingSize=32, RingStroke=2.5, RingRadius=(RingSize-RingStroke)/2;
struct RingArc {double x=RingSize/2,y=RingStroke/2;bool visible=false,full=false,large=false;};
inline RingArc GaugeArc(std::optional<unsigned> percent) {
    RingArc arc;
    if(!percent||!*percent)return arc;
    const unsigned value=std::min(*percent,100u);
    arc.visible=true;arc.full=value==100;arc.large=value>50;
    // A full circle needs its own ellipse geometry: a single arc whose start
    // and end coincide would otherwise disappear exactly at 100 percent.
    const double angle=6.2831853071795864769*value/100;
    arc.x=RingSize/2+RingRadius*std::sin(angle);
    arc.y=RingSize/2-RingRadius*std::cos(angle);
    return arc;
}
enum class Page : unsigned { Metrics, Media, Power, Clock, Count };
inline Page Cycle(Page page,int direction) {
    constexpr auto count=static_cast<int>(Page::Count);
    return static_cast<Page>((static_cast<int>(page)+(direction<0?count-1:direction>0?1:0))%count);
}
struct ClockLabels {std::wstring time,date;};
inline ClockLabels ClockText(unsigned hour,unsigned minute,unsigned month,unsigned day) {
    if(hour>23||minute>59||month<1||month>12||day<1||day>31)return {L"—",L"—"};
    wchar_t time[8]{},date[16]{};
    std::swprintf(time,8,L"%02u:%02u",hour,minute);
    std::swprintf(date,16,L"%02u月%02u日",month,day);
    return {time,date};
}
inline unsigned NextMinuteMs(unsigned second,unsigned millis) {
    if(second>59||millis>999)return 60000;
    // A little past the boundary avoids an early DispatcherTimer tick showing
    // the preceding minute. The minimum prevents a tight loop at rollover.
    return std::max(250u,60000-second*1000-millis+20);
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
inline constexpr uint64_t CpuWarmupMs=200, CpuRefreshMs=2000;
struct CpuSampler {
    std::optional<CpuTimes> baseline;
    std::optional<unsigned> value;
    uint64_t baselineAt=0;
    bool refreshing=false;
    void Resume(std::optional<CpuTimes> current,uint64_t now) {
        baseline=current;baselineAt=now;refreshing=bool(current);
        if(!current)value.reset();
    }
    // Keep the last measured value for display, but exclude the hidden period
    // from the next sample. The UI labels retained values as refreshing.
    void Pause(){baseline.reset();refreshing=false;}
    bool Sample(std::optional<CpuTimes> current,uint64_t now) {
        if(!current){baseline.reset();value.reset();refreshing=false;return true;}
        if(!baseline||now<baselineAt){Resume(current,now);return false;}
        // A queued tick from before Pause must not publish an almost-zero
        // interval after Resume. Use the same guard for the initial sample.
        if(now-baselineAt<CpuWarmupMs)return false;
        value=CpuPercent(*baseline,*current);baseline=current;baselineAt=now;refreshing=false;
        return true;
    }
    uint64_t IntervalMs()const{return refreshing?CpuWarmupMs:CpuRefreshMs;}
};
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
