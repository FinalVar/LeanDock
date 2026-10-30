#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace Capsule {
struct CpuPoint {uint64_t at{};double value{};};
inline void AppendCpu(std::deque<CpuPoint>& history,uint64_t at,double value){
    if(!std::isfinite(value))return;
    if(!history.empty()&&at<=history.back().at)return;
    history.push_back({at,std::clamp(value,0.,100.)});
    while(history.size()>60||(!history.empty()&&at-history.front().at>59000))history.pop_front();
}
struct TrendPoint {float x{},y{};};
inline std::vector<TrendPoint> CpuTrend(std::deque<CpuPoint> const& history,float width,float height){
    std::vector<TrendPoint> result;
    if(history.empty()||width<=0||height<=0)return result;
    double ceiling=50;for(auto p:history)ceiling=std::max(ceiling,p.value);
    const auto end=history.back().at;
    for(auto p:history){
        if(p.at>end||end-p.at>59000)continue;
        result.push_back({width*(1.f-float(end-p.at)/59000),height*float(1-p.value/ceiling)});
    }
    return result;
}
inline std::wstring PanelClock(unsigned hour,unsigned minute,unsigned second){
    wchar_t text[16]{};swprintf_s(text,L"%02u:%02u:%02u",hour,minute,second);return text;
}
struct GpuEngine {
    uint32_t high{},low{},physical{},engine{};
    auto operator<=>(GpuEngine const&)const=default;
    uint64_t Adapter()const{return (uint64_t(high)<<32)|low;}
};
inline std::optional<GpuEngine> ParseGpuEngine(wchar_t const* instance){
    if(!instance)return {};
    auto start=wcsstr(instance,L"_luid_");if(!start)return {};
    unsigned hi=0,lo=0,physical=0,engine=0;int used=0;
    if(swscanf(start,L"_luid_0x%x_0x%x_phys_%u_eng_%u_engtype_3D%n",&hi,&lo,&physical,&engine,&used)!=4||used<=0)return {};
    // PDH may suffix duplicate instances with #N. No other engine type is 3D.
    if(start[used]&&start[used]!=L'#')return {};
    return GpuEngine{hi,lo,physical,engine};
}
struct GpuLoads {
    std::map<GpuEngine,double> engines;
    void Add(wchar_t const* instance,double percent){
        auto key=ParseGpuEngine(instance);
        if(key&&std::isfinite(percent)&&percent>=0)engines[*key]+=percent;
    }
    int ForAdapter(uint64_t luid)const{
        double peak=-1;
        for(auto const& [engine,value]:engines)if(engine.Adapter()==luid)peak=std::max(peak,value);
        return peak<0?-1:int(std::lround(std::clamp(peak,0.,100.)));
    }
};
struct GpuSample {uint64_t luid{};std::wstring name;int percent=-1;};
}
