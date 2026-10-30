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
// D3DKMT_ADAPTERTYPE: RenderSupported is bit 0, SoftwareDevice bit 2. An
// indirect-display alias of a real GPU reports engines but no render bit.
inline bool GpuRenders(unsigned type){return (type&1)&&!(type&4);}
// Busy share of one engine node from its cumulative running time (100 ns),
// the definition Task Manager and PDH "GPU Engine" use.
inline double NodeBusy(int64_t before,int64_t after,double elapsed){
    if(!(elapsed>0)||after<before)return -1;
    return std::clamp(100.0*double(after-before)/elapsed,0.,100.);
}
// An adapter's 3D load is its busiest 3D node; never a sum across nodes.
struct GpuBusy {
    double peak=-1;
    void Add(double value){if(value>=0&&std::isfinite(value))peak=std::max(peak,value);}
    int Percent()const{return peak<0?-1:int(std::lround(std::clamp(peak,0.,100.)));}
};
struct GpuSample {uint64_t luid{};std::wstring name;int percent=-1;};
}
