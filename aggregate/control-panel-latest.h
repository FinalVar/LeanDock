#pragma once
#include "metrics-latest.h"
#include "drawing-latest.h"
#include <array>

namespace Capsule {
// Tiles without a second page (theme; Bluetooth and hotspot until theirs
// exist) toggle across their whole width and never open system UI.
enum class PanelPart { None, Volume, Mute, Brightness, Wifi, WifiMore, Bluetooth, BluetoothMore,
    DarkTheme, Hotspot, HotspotMore };
inline constexpr float PanelWidth=300,SliderLeft=60,SliderRight=244;
inline PanelPart PanelHit(float x,float y){
    if(y>=60&&y<104){
        if(x>=14&&x<146)return x>=114?PanelPart::WifiMore:PanelPart::Wifi;
        if(x>=154&&x<286)return PanelPart::Bluetooth;
    }
    if(y>=110&&y<154){
        if(x>=14&&x<146)return PanelPart::DarkTheme;
        if(x>=154&&x<286)return PanelPart::Hotspot;
    }
    if(y>=164&&y<200){if(x>=14&&x<48)return PanelPart::Mute;
        if(x>=52&&x<250)return PanelPart::Volume;}
    if(y>=206&&y<242&&x>=52&&x<250)return PanelPart::Brightness;
    return PanelPart::None;
}
inline bool PanelSlider(PanelPart p){return p==PanelPart::Volume||p==PanelPart::Brightness;}
inline int SliderValue(float x){return int(std::lround(100*std::clamp((x-SliderLeft)/(SliderRight-SliderLeft),0.f,1.f)));}
inline bool PanelEnabled(PanelPart p,Controls const& s){
    switch(p){
    case PanelPart::Volume:case PanelPart::Mute:return s.volume>=0;
    case PanelPart::Brightness:return s.brightness>=0;
    case PanelPart::Wifi:return RadioControllable(s.wifi)&&!s.wifiBusy&&!s.bluetoothBusy&&!s.hotspotBusy;
    case PanelPart::Bluetooth:return RadioControllable(s.bluetooth)&&!s.wifiBusy&&!s.bluetoothBusy&&!s.hotspotBusy;
    case PanelPart::DarkTheme:return s.darkTheme>=0&&s.themeAvailable&&!s.themeBusy;
    case PanelPart::Hotspot:return s.hotspot>=0&&!s.hotspotBusy&&!s.wifiBusy&&!s.bluetoothBusy;
    case PanelPart::WifiMore:case PanelPart::BluetoothMore:case PanelPart::HotspotMore:return true;
    default:return false;
    }
}
struct PanelGesture {
    PanelPart pressed=PanelPart::None;
    int preview=-1,lastSent=-1;
    uint64_t sentAt=0;
    void Begin(PanelPart p,float x){pressed=p;preview=PanelSlider(p)?SliderValue(x):-1;lastSent=-1;sentAt=0;}
    bool Dragging()const{return PanelSlider(pressed);}
    bool Send(float x,uint64_t now,bool release=false){
        if(!Dragging())return false;preview=SliderValue(x);
        if(preview==lastSent)return false;
        if(!release&&sentAt&&now-sentAt<75)return false;
        lastSent=preview;sentAt=now;return true;
    }
    void Cancel(){pressed=PanelPart::None;preview=lastSent=-1;sentAt=0;}
};
struct PanelValueRequest {
    uint64_t serial=0,at=0;int value=-1;
    int Display(int actual,uint64_t completed,uint64_t now)const{
        return serial>completed&&now>=at&&now-at<4000?value:actual;
    }
};
struct PanelLayout {
    float mediaY=-1,temperatureY=-1,errorY=-1,trayY=-1,gpuY=374,height=406;
    explicit PanelLayout(Sample const& data,size_t trayCount){
        float y=gpuY+22*float(std::max(size_t(1),data.gpus.size()));
        if(data.temperature!=L"CPU —　GPU —"&&!data.temperature.empty()){temperatureY=y;y+=22;}
        if(data.media!=L"没有在播放"&&!data.media.empty()){mediaY=y;y+=26;}
        if(!data.error.empty()){errorY=y;y+=24;}
        if(trayCount){trayY=y+9;y+=15+30*float((trayCount+5)/6);}
        height=y+12;
    }
    // Reveal bands, cut only in the gaps between sections and rows.
    std::vector<float> Cuts()const{return {0,55,106,157,250,328,height};}
};
// Short transitions painted by the panel: hover washes, toggle fills and
// slider moves not driven by the pointer. Settled values are exact.
struct Tween {
    float from=0,to=0;uint64_t at=0;uint32_t ms=0;
    float At(uint64_t now)const{
        if(!ms||now>=at+ms)return to;
        const float t=float(now-at)/float(ms),u=1-t;return from+(to-from)*(1-u*u*u);
    }
    bool Active(uint64_t now)const{return ms&&now<at+ms;}
    void Go(float value,uint64_t now,uint32_t duration){if(value!=to){from=At(now);to=value;at=now;ms=duration;}}
    void Snap(float value){from=to=value;ms=0;}
};
inline constexpr unsigned PanelParts=unsigned(PanelPart::HotspotMore)+1;
struct PanelMotion {
    std::array<Tween,PanelParts> hover;std::array<Tween,4> on;Tween volume,brightness;
    bool ready=false;
    void Update(Controls const& s,PanelPart hit,PanelGesture const& gesture,uint64_t now,bool animate){
        animate=animate&&ready;ready=true;
        auto go=[&](Tween& t,float value,uint32_t ms){if(animate)t.Go(value,now,ms);else t.Snap(value);};
        for(unsigned i=0;i<PanelParts;++i)go(hover[i],hit==PanelPart(i)?1.f:0.f,120);
        const int states[]={s.wifi,s.bluetooth,s.darkTheme,s.hotspot};
        for(unsigned i=0;i<4;++i)go(on[i],states[i]==1?1.f:0.f,180);
        // The pointer drives a dragged slider directly; never ease behind it.
        auto slide=[&](Tween& t,int value,PanelPart part){
            if(gesture.pressed==part)t.Snap(float(gesture.preview));
            else if(value<0)t.Snap(0);else go(t,float(value),240);
        };
        slide(volume,s.volume,PanelPart::Volume);slide(brightness,s.brightness,PanelPart::Brightness);
    }
    bool Active(uint64_t now)const{
        for(auto const& t:hover)if(t.Active(now))return true;
        for(auto const& t:on)if(t.Active(now))return true;
        return volume.Active(now)||brightness.Active(now);
    }
};
inline D2D1_COLOR_F Mix(D2D1_COLOR_F a,D2D1_COLOR_F b,float t){
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t};
}
inline D2D1_COLOR_F Alpha(D2D1_COLOR_F c,float a){c.a*=a;return c;}
inline D2D1_COLOR_F Lighter(D2D1_COLOR_F c,float t){return {c.r+(1-c.r)*t,c.g+(1-c.g)*t,c.b+(1-c.b)*t,c.a};}
inline void PaintCpuTrend(Drawing& draw,ID2D1DeviceContext* dc,std::deque<CpuPoint> const& history){
    constexpr float left=202,top=266,width=84,height=16;
    const auto points=CpuTrend(history,width,height);if(points.empty())return;
    auto color=draw.colors.accent;
    auto dot=[&](TrendPoint p){
        draw.Round(dc,{left+p.x-4,top+p.y-4,8,8},4,Alpha(color,.22f));
        draw.Round(dc,{left+p.x-2,top+p.y-2,4,4},2,color);
    };
    if(points.size()==1){dot(points[0]);return;}
    auto path=[&](bool area){
        winrt::com_ptr<ID2D1PathGeometry> shape;
        winrt::check_hresult(draw.factory->CreatePathGeometry(shape.put()));
        winrt::com_ptr<ID2D1GeometrySink> sink;winrt::check_hresult(shape->Open(sink.put()));
        sink->BeginFigure(area?D2D1_POINT_2F{left+points.front().x,top+height}:D2D1_POINT_2F{left+points.front().x,top+points.front().y},
            area?D2D1_FIGURE_BEGIN_FILLED:D2D1_FIGURE_BEGIN_HOLLOW);
        for(auto p:points)sink->AddLine({left+p.x,top+p.y});
        if(area)sink->AddLine({left+points.back().x,top+height});
        sink->EndFigure(area?D2D1_FIGURE_END_CLOSED:D2D1_FIGURE_END_OPEN);
        winrt::check_hresult(sink->Close());return shape;
    };
    auto fill=draw.Gradient(dc,{0,top},{0,top+height},{{0,Alpha(color,.32f)},{1,Alpha(color,0)}});
    dc->FillGeometry(path(true).get(),fill.get());
    winrt::com_ptr<ID2D1StrokeStyle> round;
    D2D1_STROKE_STYLE_PROPERTIES style{D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND,D2D1_LINE_JOIN_ROUND};
    winrt::check_hresult(draw.factory->CreateStrokeStyle(style,nullptr,0,round.put()));
    winrt::com_ptr<ID2D1SolidColorBrush> line;winrt::check_hresult(dc->CreateSolidColorBrush(color,line.put()));
    dc->DrawGeometry(path(false).get(),line.get(),1.5f,round.get());
    dot(points.back());
}
inline void PanelGlyph(Drawing& draw,ID2D1DeviceContext* dc,const wchar_t* text,Rect rect,D2D1_COLOR_F color,float size=17){
    auto layout=draw.Layout(text,size,float(rect.w),float(rect.h),false,false,L"Segoe Fluent Icons");
    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    winrt::com_ptr<ID2D1SolidColorBrush> brush;winrt::check_hresult(dc->CreateSolidColorBrush(color,brush.put()));
    dc->DrawTextLayout({float(rect.x),float(rect.y)},layout.get(),brush.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
inline float ClockWidth(Drawing& draw,std::wstring const& clock){
    const auto face=draw.figures.c_str();
    return draw.Measure(clock.substr(0,std::min<size_t>(5,clock.size())),28,true,face)+1+
        (clock.size()>5?draw.Measure(clock.substr(5),16,true,face):0);
}
// Hours and minutes in display figures, seconds smaller on the same baseline.
inline float PaintClock(Drawing& draw,ID2D1DeviceContext* dc,std::wstring const& clock,float x,float y,float h){
    auto& c=draw.colors;const auto face=draw.figures.c_str();
    const auto main=clock.substr(0,std::min<size_t>(5,clock.size())),rest=clock.size()>5?clock.substr(5):std::wstring();
    auto big=draw.Layout(main,28,200,h,true,false,face),small=draw.Layout(rest,16,80,h,true,false,face);
    DWRITE_TEXT_METRICS m{};big->GetMetrics(&m);
    winrt::com_ptr<ID2D1SolidColorBrush> ink,dim;
    winrt::check_hresult(dc->CreateSolidColorBrush(c.text,ink.put()));winrt::check_hresult(dc->CreateSolidColorBrush(c.secondary,dim.put()));
    dc->DrawTextLayout({x,y},big.get(),ink.get());
    const float lift=Drawing::Baseline(big.get())-Drawing::Baseline(small.get());
    dc->DrawTextLayout({x+m.widthIncludingTrailingWhitespace+1,y+lift},small.get(),dim.get());
    return m.widthIncludingTrailingWhitespace+1+draw.Measure(rest,16,true,face);
}
// The same painter is used by the real popup and the offscreen render checks.
// It has no system writes, device queries, window activation or input synthesis.
inline void PaintControlPanel(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,
    Power const& power,std::wstring const& time,std::wstring const& date,std::wstring const& lunar,
    PanelLayout const& layout,PanelPart hover,PanelGesture const& gesture,PanelMotion const& motion,uint64_t now){
    auto& c=draw.colors;
    PaintClock(draw,dc,time,14,5,42);
    draw.Text(dc,date,{138,9,148,18},12.5f,c.text,true,true);
    draw.Text(dc,lunar,{128,28,158,18},12,c.secondary,false,true);
    auto tile=[&](unsigned index,float x,float y,const wchar_t* name,int state,bool busy,PanelPart body,PanelPart more,const wchar_t* glyph,std::wstring label){
        const float on=motion.on[index].At(now);const Rect box{x,y,132,44};
        draw.Round(dc,box,12,Mix(c.control,Alpha(c.control,.0f),on));
        if(on>0){
            auto fill=draw.Gradient(dc,{0,y},{0,y+44},{{0,Alpha(Lighter(c.accent,.12f),on)},{1,Alpha(c.accent,on)}});
            draw.Fill(dc,box,12,fill.get());
        }
        // Lit top edge: the control reads as a raised pane of the same glass.
        auto edge=draw.Gradient(dc,{0,y},{0,y+22},{{0,Alpha(c.controlEdge,1-.55f*on)},{1,Alpha(c.controlEdge,0)}});
        draw.Stroke(dc,{x+.5,y+.5,131,43},11.5f,edge.get());
        const auto wash=Mix(c.hover,Ink(0xFFFFFF,.14f),on);
        const bool split=more!=PanelPart::None;
        const float hb=motion.hover[unsigned(body)].At(now),hm=split?motion.hover[unsigned(more)].At(now):0;
        if(hb>0){dc->PushAxisAlignedClip({x,y,x+(split?100:132),y+44},D2D1_ANTIALIAS_MODE_ALIASED);draw.Round(dc,box,12,Alpha(wash,hb));dc->PopAxisAlignedClip();}
        if(hm>0){dc->PushAxisAlignedClip({x+100,y,x+132,y+44},D2D1_ANTIALIAS_MODE_ALIASED);draw.Round(dc,box,12,Alpha(wash,hm));dc->PopAxisAlignedClip();}
        const auto ink=Mix(state<0?c.secondary:c.text,c.onAccent,on);
        const auto sub=Mix(c.secondary,Alpha(c.onAccent,.78f),on);
        PanelGlyph(draw,dc,glyph,{x+6,y+8,26,28},ink,18);
        draw.Text(dc,name,{x+37,y+5,split?62.f:90.f,20},13,ink,true);
        draw.Text(dc,busy?L"正在切换":label,{x+37,y+24,split?64.f:90.f,16},10.5f,sub);
        if(!split)return;
        draw.Line(dc,x+100,y+12,x+100,y+32,Alpha(ink,.16f));
        PanelGlyph(draw,dc,L"\xE76C",{x+100,y+6,32,32},sub,10);
    };
    tile(0,14,60,L"Wi-Fi",s.wifi,s.wifiBusy,PanelPart::Wifi,PanelPart::WifiMore,L"\xE701",RadioLabel(s.wifi));
    tile(1,154,60,L"蓝牙",s.bluetooth,s.bluetoothBusy,PanelPart::Bluetooth,PanelPart::None,L"\xE702",RadioLabel(s.bluetooth));
    tile(2,14,110,L"深色主题",s.darkTheme,s.themeBusy,PanelPart::DarkTheme,PanelPart::None,L"\xE708",ThemeControl::Label(s.darkTheme));
    tile(3,154,110,L"移动热点",s.hotspot,s.hotspotBusy,PanelPart::Hotspot,PanelPart::None,L"\xEC15",s.hotspotLabel);
    auto slider=[&](float y,int actual,Tween const& eased,PanelPart part,const wchar_t* glyph){
        const bool enabled=actual>=0;auto ink=enabled?c.text:c.secondary;
        const float position=gesture.pressed==part?float(gesture.preview):eased.At(now);
        if(part==PanelPart::Volume&&enabled){const float h=motion.hover[unsigned(PanelPart::Mute)].At(now);
            if(h>0)draw.Round(dc,{14,y-18,34,36},10,Alpha(c.hover,h));}
        PanelGlyph(draw,dc,glyph,{14,y-18,34,36},ink);
        draw.Round(dc,{SliderLeft,y-2,SliderRight-SliderLeft,4},2,c.track);
        if(enabled){
            const float x=SliderLeft+(SliderRight-SliderLeft)*std::clamp(position,0.f,100.f)/100.f;
            if(x>SliderLeft+.5f){
                auto fill=draw.Gradient(dc,{SliderLeft,y},{SliderRight,y},{{0,Alpha(c.accent,.82f)},{1,c.accent}});
                draw.Fill(dc,{SliderLeft,y-2,x-SliderLeft,4},2,fill.get());
            }
            const float lift=std::max(motion.hover[unsigned(part)].At(now),gesture.pressed==part?1.f:0.f);
            draw.Round(dc,{x-9.5f,y-8.5f,19,19},9.5f,Ink(0x000000,c.light?.10f:.35f));
            draw.Round(dc,{x-9,y-9,18,18},9,c.thumb);
            auto ring=draw.Gradient(dc,{0,y-9},{0,y+9},{{0,Ink(0xFFFFFF,c.light?.9f:.18f)},{1,Ink(c.light?0x000000:0xFFFFFF,c.light?.10f:.05f)}});
            dc->DrawEllipse({{x,y},8.5f,8.5f},ring.get(),1);
            const float dot=4.5f+1.5f*lift;
            draw.Round(dc,{x-dot,y-dot,2*dot,2*dot},dot,c.accent);
        }
        const int shown=gesture.pressed==part?gesture.preview:enabled?int(std::lround(position)):-1;
        draw.Text(dc,shown>=0?std::to_wstring(shown):L"—",{252,y-10,34,20},12.5f,ink,false,true);
    };
    slider(182,s.volume,motion.volume,PanelPart::Volume,s.mute?L"\xE74F":L"\xE767");
    slider(224,s.brightness,motion.brightness,PanelPart::Brightness,L"\xE706");
    draw.Rule(dc,14,252,286,c.hair);
    auto row=[&](float y,const wchar_t* label,std::wstring main,std::wstring secondary=L"",D2D1_COLOR_F const* tone=nullptr){
        draw.Text(dc,label,{14,y,40,20},12,c.secondary);
        const float sub=secondary.empty()?0:draw.Measure(secondary,12)+8;
        draw.Text(dc,main,{56,y,230-sub,20},12.5f,tone?*tone:c.text);
        if(!secondary.empty())draw.Text(dc,secondary,{56,y,230,20},12,c.secondary,false,true);
    };
    auto percent=[](int n){return n<0?std::wstring(L"—"):std::to_wstring(n)+L"%";};
    auto load=[&](int n)->D2D1_COLOR_F const*{return n>=90?&c.danger:n>=75?&c.warn:nullptr;};
    row(264,L"CPU",percent(s.cpu),L"",load(s.cpu));
    PaintCpuTrend(draw,dc,s.history);
    row(286,L"内存",percent(s.memory),s.capacity,s.memory>=90?&c.warn:nullptr);
    row(308,L"网速",s.network);
    row(330,L"磁盘",s.disks,L"可用");
    row(352,L"电池",power.Present()?power.Percent():L"—",s.battery,
        power.Present()&&!power.AC()&&power.value.BatteryLifePercent<20?&c.warn:nullptr);
    if(s.gpus.empty())row(layout.gpuY,L"GPU",L"—");
    for(size_t i=0;i<s.gpus.size();++i){
        const auto& gpu=s.gpus[i];const float y=layout.gpuY+22*float(i);
        const auto value=L"3D "+percent(gpu.percent);
        const float w=draw.Measure(value,12)+8;
        draw.Text(dc,L"GPU",{14,y,40,20},12,c.secondary);
        draw.Text(dc,gpu.name,{56,y,230-w,20},12.5f,c.text);
        draw.Text(dc,value,{56,y,230,20},12,load(gpu.percent)?*load(gpu.percent):c.secondary,false,true);
    }
    if(layout.temperatureY>=0)row(layout.temperatureY,L"温度",s.temperature);
    if(layout.mediaY>=0)row(layout.mediaY,L"媒体",s.media);
    if(layout.errorY>=0)draw.Text(dc,s.error,{14,layout.errorY,272,22},11,c.danger);
    if(layout.trayY>=0){draw.Rule(dc,14,layout.trayY-5,286,c.hair);
        draw.Text(dc,L"托盘",{14,layout.trayY,34,24},12,c.secondary);}
}
}
