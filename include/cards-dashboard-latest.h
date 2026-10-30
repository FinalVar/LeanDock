#pragma once
#include "cards-core-latest.h"
#include <limits>

namespace LeanDockCards::Dashboard {
inline constexpr double ClockWidth=84, StartWidth=24, MetricsWidth=124,
    PowerWidth=64, NetworkWidth=68, MediaToggleWidth=24, MediaExpandedWidth=128,
    Gap=6, SlotPadding=16, Height=44;
inline constexpr double NormalAppWidth=52, MinimumAppWidth=40;
inline double ModuleWidth(Page page,bool mediaAvailable,bool expanded) {
    switch(page){
    case Page::Clock:return ClockWidth;case Page::Metrics:return MetricsWidth;
    case Page::Power:return PowerWidth;case Page::Network:return NetworkWidth;
    case Page::Media:return mediaAvailable?(expanded?MediaExpandedWidth:MediaToggleWidth):0;
    default:return 0;
    }
}
inline double Width(PageOrder const& order,bool mediaAvailable,bool expanded) {
    // Start remains available even when the clock module is disabled.
    double width=order.Has(Page::Clock)?0:StartWidth;unsigned count=width>0?1:0;
    for(auto page:order.pages){const auto part=ModuleWidth(page,mediaAvailable,expanded);
        if(part>0){width+=part;++count;}}
    return width+(count>1?(count-1)*Gap:0);
}
inline double AppWidth(double budget,double naturalWidth,unsigned buttons,double previous=NormalAppWidth) {
    if(!std::isfinite(budget)||!std::isfinite(naturalWidth)||budget<=0||!buttons)return NormalAppWidth;
    // Fixed steps and hysteresis avoid oscillation at fractional-DPI limits.
    for(double width:{52.0,48.0,44.0,40.0}){
        const double needed=naturalWidth-buttons*(NormalAppWidth-width);
        if(needed+(width>previous?8.0:0.0)<=budget)return width;
    }
    return MinimumAppWidth;
}
inline std::wstring MemoryText(uint64_t total,uint64_t available) {
    if(!total||available>total)return L"— / — GB";
    constexpr double gib=1024.0*1024*1024;
    wchar_t text[80]{};
    std::swprintf(text,std::size(text),L"%.1f / %.1f GB",(total-available)/gib,total/gib);
    return text;
}
// One timer services independent due times. A pending async read contributes
// no deadline; its completion schedules the next read, never a busy poll.
inline uint64_t Delay(uint64_t now,bool cpu,uint64_t cpuDue,bool clock,uint64_t clockDue,
                      bool network,bool networkPending,uint64_t networkDue) {
    uint64_t due=std::numeric_limits<uint64_t>::max();
    if(cpu)due=std::min(due,cpuDue);if(clock)due=std::min(due,clockDue);
    if(network&&!networkPending)due=std::min(due,networkDue);
    return due==std::numeric_limits<uint64_t>::max()?0:std::clamp<uint64_t>(due>now?due-now:1,50,60020);
}
}
