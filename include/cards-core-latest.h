#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <cstdio>
#include <string>
#include <vector>
#include <string_view>

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
enum class Page : unsigned { Metrics, Media, Power, Clock, Network, Count };
inline Page Cycle(Page page,int direction) {
    constexpr auto count=static_cast<int>(Page::Count);
    return static_cast<Page>((static_cast<int>(page)+(direction<0?count-1:direction>0?1:0))%count);
}
inline constexpr Page DefaultOrder[]={Page::Clock,Page::Power,Page::Media,Page::Metrics,Page::Network};
inline const wchar_t* PageKey(Page page) {
    switch(page){case Page::Clock:return L"clock";case Page::Power:return L"power";
    case Page::Media:return L"media";case Page::Metrics:return L"metrics";case Page::Network:return L"network";default:return L"";}
}
inline const wchar_t* PageLabel(Page page) {
    switch(page){case Page::Clock:return L"时间与日期";case Page::Power:return L"电池";
    case Page::Media:return L"媒体";case Page::Metrics:return L"CPU / 内存";case Page::Network:return L"上传 / 下载";default:return L"";}
}
inline std::optional<Page> ParsePage(std::wstring_view text) {
    for(auto page:DefaultOrder)if(text==PageKey(page))return page;
    return {};
}
struct PageOrder {
    std::vector<Page> pages{std::begin(DefaultOrder),std::end(DefaultOrder)};
    bool Has(Page page)const{return std::find(pages.begin(),pages.end(),page)!=pages.end();}
    Page Select(Page page)const{return Has(page)?page:pages.front();}
    Page Step(Page page,int direction)const {
        auto item=std::find(pages.begin(),pages.end(),page);
        if(item==pages.end())return pages.front();
        const int count=int(pages.size()),index=int(item-pages.begin());
        return pages[(index+(direction<0?count-1:direction>0?1:0))%count];
    }
    std::wstring Text()const {
        std::wstring result;for(auto page:pages){if(!result.empty())result+=L',';result+=PageKey(page);}return result;
    }
};
inline std::optional<PageOrder> ParseOrder(std::wstring_view text) {
    if(text.empty()||text.size()>128)return {};
    PageOrder order;order.pages.clear();
    while(!text.empty()){
        auto end=text.find(L',');auto part=text.substr(0,end);
        auto first=part.find_first_not_of(L" \t"),last=part.find_last_not_of(L" \t");
        if(first==std::wstring_view::npos)return {};
        auto page=ParsePage(part.substr(first,last-first+1));
        if(!page||order.Has(*page))return {};
        order.pages.push_back(*page);
        if(end==std::wstring_view::npos)break;
        text.remove_prefix(end+1);if(text.empty())return {};
    }
    return order;
}
struct NetworkCounter {uint64_t id,received,sent;};
using NetworkSnapshot=std::vector<NetworkCounter>;
struct NetworkRate {double received=0,sent=0;};
struct NetworkSampler {
    std::optional<NetworkSnapshot> baseline;
    std::optional<NetworkRate> value;
    uint64_t at=0;bool connected=false;
    void Pause(){baseline.reset();value.reset();connected=false;}
    bool Sample(std::optional<NetworkSnapshot> current,uint64_t now) {
        if(!current){Pause();return true;}
        connected=!current->empty();
        if(!baseline||now<at||now-at>10000){baseline=std::move(current);at=now;value.reset();return false;}
        if(now-at<200)return false;
        NetworkRate delta;bool complete=true;
        for(auto const& item:*current){
            auto previous=std::find_if(baseline->begin(),baseline->end(),[&](auto const& row){return row.id==item.id;});
            if(previous==baseline->end()||item.received<previous->received||item.sent<previous->sent){complete=false;break;}
            delta.received+=double(item.received-previous->received);
            delta.sent+=double(item.sent-previous->sent);
        }
        if(complete){const double scale=1000.0/(now-at);value=NetworkRate{delta.received*scale,delta.sent*scale};}
        else value.reset();
        baseline=std::move(current);at=now;return complete;
    }
    uint64_t IntervalMs()const{return connected&&!value?200:1000;}
};
struct RateLabels {
    std::wstring number=L"—",unit;
    std::wstring CompactText()const{return unit.empty()?number:number+L" "+unit;}
    std::wstring PerSecondText()const{return unit.empty()?number:CompactText()+L"/s";}
};
inline RateLabels FormatRate(std::optional<double> rate) {
    if(!rate||!std::isfinite(*rate)||*rate<0)return {};
    double number=*rate;unsigned unit=0;const wchar_t* units[]={L"B",L"KB",L"MB",L"GB"};
    while(number>=1000&&unit<3){number/=1000;++unit;}
    if(number>9999)return {L">9999",L"GB"};
    wchar_t buffer[32]{};
    std::swprintf(buffer,32,unit&&number<100?L"%.1f":L"%.0f",number);return {buffer,units[unit]};
}
inline std::wstring RateText(std::optional<double> rate) {return FormatRate(rate).PerSecondText();}
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
struct CardWheel {
    int vertical{},horizontal{};uint64_t last{};bool active=false;
    void Reset(){vertical=horizontal=0;last=0;active=false;}
    int Step(int delta,bool sideways,uint64_t now){
        if(!delta)return 0;
        if(active&&(now<last||now-last>500))Reset();
        active=true;last=now;
        // Never add a horizontal fraction to a vertical fraction. A positive
        // horizontal wheel delta scrolls right (next), unlike positive vertical.
        return sideways?-WheelStep(delta,horizontal):WheelStep(delta,vertical);
    }
};
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
