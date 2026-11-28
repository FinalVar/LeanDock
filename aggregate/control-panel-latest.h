#pragma once
#include "metrics-latest.h"
#include "drawing-latest.h"
#include <array>

namespace Capsule {
// Vertical layout stack: accumulates y positions and inter-section band cuts
// from top to bottom so painting, hit testing and reveal cuts share one truth.
struct Stack {
    float y=0;
    constexpr explicit Stack(float start=0):y(start){}
    constexpr float Next(float h,float gap=0){const float top=y;y+=h+gap;return top;}
    constexpr float Cut(float before,float after){y+=before;const float cut=y;y+=after;return cut;}
};
struct PanelZones {
    static constexpr float clockH=42,tileH=44,sliderH=36,infoRowH=22;
    float clockY,dateY,lunarY,section0;
    float tile1Y,section1,tile2Y,section2;
    float volumeHitTop,volumeHitBottom,volumeY,volumeMoreTop;
    float brightnessHitTop,brightnessHitBottom,brightnessY;
    float section3,ruleY;
    float cpuY,memY,netY,section4,diskY,batteryY,gpuBaseY;
    consteval PanelZones(){
        Stack s(5);
        clockY=s.Next(clockH);dateY=clockY+4;lunarY=clockY+23;
        section0=s.Cut(8,5);
        tile1Y=s.Next(tileH);section1=s.Cut(2,4);
        tile2Y=s.Next(tileH);section2=s.Cut(3,7);
        volumeHitTop=s.Next(sliderH,6);volumeHitBottom=volumeHitTop+sliderH;
        volumeY=volumeHitTop+sliderH/2;volumeMoreTop=volumeHitTop+2;
        brightnessHitTop=s.Next(sliderH);brightnessHitBottom=brightnessHitTop+sliderH;
        brightnessY=brightnessHitTop+sliderH/2;
        section3=s.Cut(8,2);ruleY=s.Next(0,12);
        cpuY=s.Next(infoRowH);memY=s.Next(infoRowH);netY=s.Next(infoRowH);
        section4=s.y-2; // Cut in the 2 DIP gap between net row text (h=20) and disk row.
        diskY=s.Next(infoRowH);batteryY=s.Next(infoRowH);
        gpuBaseY=s.y;
    }
};
inline constexpr PanelZones PanelStatic{};
static_assert(PanelStatic.tile1Y==60&&PanelStatic.tile2Y==110&&PanelStatic.volumeY==182&&
              PanelStatic.brightnessY==224&&PanelStatic.ruleY==252&&PanelStatic.cpuY==264&&
              PanelStatic.gpuBaseY==374);
// The arrows open the second pages; the theme tile has none and toggles
// across its whole width. Nothing here opens system UI.
enum class PanelPart { None, Volume, Mute, Brightness, BrightnessToggle, Wifi, WifiMore, Bluetooth, BluetoothMore,
    DarkTheme, Hotspot, HotspotMore, VolumeMore, MediaPrev, MediaToggle, MediaNext };
// MediaNextX places U+E893's right vertical bar (xMax=1920/2048 at 10.5 DIP ->
// +16.59375 DIP inside the 24 DIP centered box) right on the 286 DIP content edge.
inline constexpr float PanelWidth=300,SliderLeft=60,SliderRight=218,
    MediaBtnW=24,MediaPrevX=217.4f,MediaToggleX=243.4f,MediaNextX=269.4f;
inline PanelPart PanelHit(float x,float y,float mediaY=-1.f){
    auto const& S=PanelStatic;
    if(y>=S.tile1Y&&y<S.tile1Y+S.tileH){
        if(x>=14&&x<146)return x>=114?PanelPart::WifiMore:PanelPart::Wifi;
        if(x>=154&&x<286)return x>=254?PanelPart::BluetoothMore:PanelPart::Bluetooth;
    }
    if(y>=S.tile2Y&&y<S.tile2Y+S.tileH){
        if(x>=14&&x<146)return PanelPart::DarkTheme;
        if(x>=154&&x<286)return x>=254?PanelPart::HotspotMore:PanelPart::Hotspot;
    }
    if(y>=S.volumeHitTop&&y<S.volumeHitBottom){if(x>=14&&x<48)return PanelPart::Mute;
        if(x>=52&&x<224)return PanelPart::Volume;
        if(x>=256&&x<288)return PanelPart::VolumeMore;}
    if(y>=S.brightnessHitTop&&y<S.brightnessHitBottom){if(x>=14&&x<48)return PanelPart::BrightnessToggle;
        if(x>=52&&x<224)return PanelPart::Brightness;}
    if(mediaY>=0.f&&y>=mediaY-2.f&&y<mediaY+22.f){
        if(x>=MediaPrevX&&x<MediaPrevX+MediaBtnW)return PanelPart::MediaPrev;
        if(x>=MediaToggleX&&x<MediaToggleX+MediaBtnW)return PanelPart::MediaToggle;
        if(x>=MediaNextX&&x<MediaNextX+MediaBtnW)return PanelPart::MediaNext;
    }
    return PanelPart::None;
}
inline bool PanelSlider(PanelPart p){return p==PanelPart::Volume||p==PanelPart::Brightness;}
inline int SliderValue(float x){return int(std::lround(100*std::clamp((x-SliderLeft)/(SliderRight-SliderLeft),0.f,1.f)));}
inline int ToggleBrightness(int current,int& saved){
    if(current>0){saved=std::clamp(current,1,100);return 0;}
    return std::clamp(saved>0?saved:50,1,100);
}
inline bool PanelEnabled(PanelPart p,Controls const& s){
    switch(p){
    case PanelPart::Volume:case PanelPart::Mute:return s.volume>=0;
    case PanelPart::Brightness:case PanelPart::BrightnessToggle:return s.brightness>=0;
    case PanelPart::Wifi:return RadioControllable(s.wifi)&&!s.wifiBusy&&!s.bluetoothBusy&&!s.hotspotBusy;
    case PanelPart::Bluetooth:return RadioControllable(s.bluetooth)&&!s.wifiBusy&&!s.bluetoothBusy&&!s.hotspotBusy;
    case PanelPart::DarkTheme:return s.darkTheme>=0&&s.themeAvailable&&!s.themeBusy;
    case PanelPart::Hotspot:return s.hotspot>=0&&!s.hotspotBusy&&!s.wifiBusy&&!s.bluetoothBusy;
    case PanelPart::WifiMore:case PanelPart::BluetoothMore:case PanelPart::HotspotMore:case PanelPart::VolumeMore:return true;
    case PanelPart::MediaPrev:case PanelPart::MediaToggle:case PanelPart::MediaNext:return s.mediaActive;
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
    float mediaY=-1,temperatureY=-1,errorY=-1,trayY=-1,gpuY=PanelStatic.gpuBaseY,height=406;
    explicit PanelLayout(Sample const& data,size_t trayCount){
        Stack s(PanelStatic.gpuBaseY);
        gpuY=s.Next(PanelStatic.infoRowH*float(std::max(size_t(1),data.gpus.size())));
        if(data.temperature!=L"CPU —　GPU —"&&!data.temperature.empty())temperatureY=s.Next(PanelStatic.infoRowH);
        if(data.media!=L"没有在播放"&&!data.media.empty())mediaY=s.Next(26);
        if(!data.error.empty())errorY=s.Next(24);
        if(trayCount){s.Next(9);trayY=s.Next(6+30*float((trayCount+5)/6));}
        s.Next(12);height=s.y;
    }
    // Reveal bands, cut only in the gaps between sections and rows.
    std::vector<float> Cuts()const{
        auto const& S=PanelStatic;
        return {0,S.section0,S.section1,S.section2,S.section3,S.section4,height};
    }
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
inline constexpr unsigned PanelParts=unsigned(PanelPart::MediaNext)+1;
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
    constexpr float left=202,top=PanelStatic.cpuY+2,width=84,height=16;
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
    dc->DrawGeometry(path(false).get(),draw.Brush(dc,color),1.5f,draw.roundStroke.get());
    dot(points.back());
}
// A value slider: track, accent fill and thumb. position is 0-100; lift
// (0-1) grows the thumb's dot under the pointer; dim greys a muted one.
inline void PaintSlider(Drawing& draw,ID2D1DeviceContext* dc,float left,float right,float y,float position,bool enabled,float lift,bool dim=false){
    auto& c=draw.colors;
    draw.Round(dc,{left,y-2,right-left,4},2,c.track);
    if(!enabled)return;
    const auto tone=dim?c.secondary:c.accent;
    const float x=left+(right-left)*std::clamp(position,0.f,100.f)/100.f;
    if(x>left+.5f){
        auto fill=draw.Gradient(dc,{left,y},{right,y},{{0,Alpha(tone,.82f)},{1,tone}});
        draw.Fill(dc,{left,y-2,x-left,4},2,fill.get());
    }
    draw.Round(dc,{x-9.5f,y-8.5f,19,19},9.5f,Ink(0x000000,c.light?.10f:.35f));
    draw.Round(dc,{x-9,y-9,18,18},9,c.thumb);
    auto ring=draw.Gradient(dc,{0,y-9},{0,y+9},{{0,Ink(0xFFFFFF,c.light?.9f:.18f)},{1,Ink(c.light?0x000000:0xFFFFFF,c.light?.10f:.05f)}});
    dc->DrawEllipse({{x,y},8.5f,8.5f},ring.get(),1);
    const float dot=4.5f+1.5f*lift;
    draw.Round(dc,{x-dot,y-dot,2*dot,2*dot},dot,tone);
}
inline void PanelGlyph(Drawing& draw,ID2D1DeviceContext* dc,const wchar_t* text,Rect rect,D2D1_COLOR_F color,float size=17){
    auto layout=draw.Layout(text,size,float(rect.w),float(rect.h),false,false,L"Segoe Fluent Icons");
    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    dc->DrawTextLayout({float(rect.x),float(rect.y)},layout.get(),draw.Brush(dc,color),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
inline float MeasurePanelText(Drawing& draw,std::wstring const& text,float size,bool bold=false){
    DWRITE_TEXT_METRICS tm{};
    draw.Layout(text,size,4096.f,64.f,bold,false)->GetMetrics(&tm);
    return tm.widthIncludingTrailingWhitespace;
}
inline float BatteryBadgeWidth(Drawing& draw,int battery,float fontSize){
    return std::ceil(MeasurePanelText(draw,std::to_wstring(battery)+L"%",fontSize,false));
}
inline D2D1_COLOR_F BatteryBadgeColor(D2D1_COLOR_F danger,D2D1_COLOR_F normal,int battery,bool onAccent){
    if(battery>=20)return normal;
    if(!onAccent)return danger;
    const float lum=normal.r*.299f+normal.g*.587f+normal.b*.114f;
    return lum>.5f?Ink(0xFFD66B):Ink(0xA81E18);
}
inline void PaintBatteryBadge(Drawing& draw,ID2D1DeviceContext* dc,Rect rect,int battery,D2D1_COLOR_F color,float fontSize){
    draw.Text(dc,std::to_wstring(battery)+L"%",{rect.x,rect.y,rect.w+2.f,rect.h},fontSize,color);
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
    dc->DrawTextLayout({x,y},big.get(),draw.Brush(dc,c.text));
    const float lift=Drawing::Baseline(big.get())-Drawing::Baseline(small.get());
    dc->DrawTextLayout({x+m.widthIncludingTrailingWhitespace+1,y+lift},small.get(),draw.Brush(dc,c.secondary));
    return m.widthIncludingTrailingWhitespace+1+draw.Measure(rest,16,true,face);
}
// The same painter is used by the real popup and the offscreen render checks.
// It has no system writes, device queries, window activation or input synthesis.
inline void PaintControlPanel(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,
    Power const& power,std::wstring const& time,std::wstring const& date,std::wstring const& lunar,
    PanelLayout const& layout,PanelPart hover,PanelGesture const& gesture,PanelMotion const& motion,uint64_t now){
    auto& c=draw.colors;
    auto const& S=PanelStatic;
    PaintClock(draw,dc,time,14,S.clockY,S.clockH);
    draw.Text(dc,date,{138,S.dateY,148,18},12.5f,c.text,true,true);
    draw.Text(dc,lunar,{128,S.lunarY,158,18},12,c.secondary,false,true);
    auto tile=[&](unsigned index,float x,float y,const wchar_t* name,int state,bool busy,PanelPart body,PanelPart more,const wchar_t* glyph,std::wstring label,int battery=-1){
        const float on=motion.on[index].At(now);const Rect box{x,y,132,44};
        draw.Round(dc,box,12,Mix(c.control,Alpha(c.control,.0f),on));
        if(on>0){
            auto fill=draw.Gradient(dc,{0,y},{0,y+44},{{0,Alpha(Lighter(c.accent,.12f),on)},{1,Alpha(c.accent,on)}});
            draw.Fill(dc,box,12,fill.get());
        }
        if(c.light){
            if(on<1)draw.Stroke(dc,{x+.5,y+.5,131,43},11.5f,draw.Brush(dc,Alpha(c.controlEdge,1-on)));
            if(on>0){
                auto topRim=draw.Gradient(dc,{0,y},{0,y+22},{{0,Ink(0xFFFFFF,.38f*on)},{1,Ink(0xFFFFFF,0)}});
                draw.Stroke(dc,{x+.5,y+.5,131,43},11.5f,topRim.get());
            }
        }else{
            // Lit top edge: the control reads as a raised pane of the same glass.
            auto edge=draw.Gradient(dc,{0,y},{0,y+22},{{0,Alpha(c.controlEdge,1-.55f*on)},{1,Alpha(c.controlEdge,0)}});
            draw.Stroke(dc,{x+.5,y+.5,131,43},11.5f,edge.get());
        }
        const auto wash=Mix(c.hover,Ink(0xFFFFFF,.14f),on);
        const bool split=more!=PanelPart::None;
        const float hb=motion.hover[unsigned(body)].At(now),hm=split?motion.hover[unsigned(more)].At(now):0;
        if(hb>0){dc->PushAxisAlignedClip({x,y,x+(split?100:132),y+44},D2D1_ANTIALIAS_MODE_ALIASED);draw.Round(dc,box,12,Alpha(wash,hb));dc->PopAxisAlignedClip();}
        if(hm>0){dc->PushAxisAlignedClip({x+100,y,x+132,y+44},D2D1_ANTIALIAS_MODE_ALIASED);draw.Round(dc,box,12,Alpha(wash,hm));dc->PopAxisAlignedClip();}
        const auto ink=Mix(state<0?c.secondary:c.text,c.onAccent,on);
        const auto sub=Mix(c.secondary,Alpha(c.onAccent,.78f),on);
        PanelGlyph(draw,dc,glyph,{x+6,y+8,26,28},ink,18);
        draw.Text(dc,name,{x+37,y+5,split?62.f:90.f,20},13,ink,true);
        if(split&&!busy&&battery>=0&&battery<=100){
            const float badgeW=BatteryBadgeWidth(draw,battery,9.5f);
            const float badgeX=std::round(x+97.f-badgeW);
            const float nameW=std::max(0.f,badgeX-(x+37.f)-3.f);
            draw.Text(dc,label,{x+37,y+24,nameW,16},9.5f,sub);
            const D2D1_COLOR_F badgeColor=BatteryBadgeColor(c.danger,sub,battery,on>.5f);
            PaintBatteryBadge(draw,dc,{badgeX,y+24,badgeW,16},battery,badgeColor,9.5f);
        }else{
            draw.Text(dc,busy?L"正在切换":label,{x+37,y+24,split?60.f:90.f,16},10.5f,sub);
        }
        if(!split)return;
        draw.Line(dc,x+100,y+12,x+100,y+32,Alpha(ink,.16f));
        PanelGlyph(draw,dc,L"\xE76C",{x+100,y+6,32,32},sub,10);
    };
    tile(0,14,S.tile1Y,L"Wi-Fi",s.wifi,s.wifiBusy,PanelPart::Wifi,PanelPart::WifiMore,L"\xE701",TileRadioLabel(s.wifi,s.wifiLabel));
    tile(1,154,S.tile1Y,L"蓝牙",s.bluetooth,s.bluetoothBusy,PanelPart::Bluetooth,PanelPart::BluetoothMore,L"\xE702",
        TileRadioLabel(s.bluetooth,s.bluetoothLabel),TileBluetoothBattery(s.bluetooth,s.bluetoothBusy,s.bluetoothBattery));
    tile(2,14,S.tile2Y,L"深色主题",s.darkTheme,s.themeBusy,PanelPart::DarkTheme,PanelPart::None,L"\xE708",ThemeControl::Label(s.darkTheme));
    tile(3,154,S.tile2Y,L"移动热点",s.hotspot,s.hotspotBusy,PanelPart::Hotspot,PanelPart::HotspotMore,L"\xEC15",s.hotspotLabel);
    auto slider=[&](float y,int actual,Tween const& eased,PanelPart part,PanelPart iconPart,const wchar_t* glyph,const wchar_t* lowGlyph=nullptr){
        const bool enabled=actual>=0;auto ink=enabled?c.text:c.secondary;
        const float position=gesture.pressed==part?float(gesture.preview):eased.At(now);
        const int shown=gesture.pressed==part?gesture.preview:enabled?int(std::lround(position)):-1;
        if(enabled){const float h=motion.hover[unsigned(iconPart)].At(now);
            if(h>0)draw.Round(dc,{14,y-18,34,36},10,Alpha(c.hover,h));}
        PanelGlyph(draw,dc,(lowGlyph&&shown==0)?lowGlyph:glyph,{14,y-18,34,36},ink);
        const float lift=std::max(motion.hover[unsigned(part)].At(now),gesture.pressed==part?1.f:0.f);
        PaintSlider(draw,dc,SliderLeft,SliderRight,y,position,enabled,lift);
        draw.Text(dc,shown>=0?std::to_wstring(shown):L"—",{222,y-10,32,20},12.5f,ink,false,true);
    };
    slider(S.volumeY,s.volume,motion.volume,PanelPart::Volume,PanelPart::Mute,s.mute?L"\xE74F":L"\xE767");
    slider(S.brightnessY,s.brightness,motion.brightness,PanelPart::Brightness,PanelPart::BrightnessToggle,L"\xE706",L"\xEC8A");
    // Output devices and app volumes: the sound page.
    if(const float h=motion.hover[unsigned(PanelPart::VolumeMore)].At(now);h>0)draw.Round(dc,{256,S.volumeMoreTop,30,32},8,Alpha(c.hover,h));
    PanelGlyph(draw,dc,L"\xE76C",{256,S.volumeMoreTop,30,32},c.secondary,10);
    draw.Rule(dc,14,S.ruleY,286,c.hair);
    auto row=[&](float y,const wchar_t* label,std::wstring main,std::wstring secondary=L"",D2D1_COLOR_F const* tone=nullptr){
        draw.Text(dc,label,{14,y,40,20},12,c.secondary);
        const float sub=secondary.empty()?0:draw.Measure(secondary,12)+8;
        draw.Text(dc,main,{56,y,230-sub,20},12.5f,tone?*tone:c.text);
        if(!secondary.empty())draw.Text(dc,secondary,{56,y,230,20},12,c.secondary,false,true);
    };
    auto percent=[](int n){return n<0?std::wstring(L"—"):std::to_wstring(n)+L"%";};
    auto load=[&](int n)->D2D1_COLOR_F const*{return n>=90?&c.danger:n>=75?&c.warn:nullptr;};
    row(S.cpuY,L"CPU",percent(s.cpu),L"",load(s.cpu));
    PaintCpuTrend(draw,dc,s.history);
    row(S.memY,L"内存",percent(s.memory),s.capacity,s.memory>=90?&c.warn:nullptr);
    row(S.netY,L"网速",s.network);
    row(S.diskY,L"磁盘",s.disks,L"可用");
    row(S.batteryY,L"电池",power.Present()?power.Percent():L"—",s.battery,
        power.Present()&&!power.AC()&&power.value.BatteryLifePercent<20?&c.warn:nullptr);
    if(s.gpus.empty())row(layout.gpuY,L"GPU",L"—");
    for(size_t i=0;i<s.gpus.size();++i){
        const auto& gpu=s.gpus[i];const float y=layout.gpuY+S.infoRowH*float(i);
        const auto value=L"3D "+percent(gpu.percent);
        const float w=draw.Measure(value,12)+8;
        draw.Text(dc,L"GPU",{14,y,40,20},12,c.secondary);
        draw.Text(dc,gpu.name,{56,y,230-w,20},12.5f,c.text);
        draw.Text(dc,value,{56,y,230,20},12,load(gpu.percent)?*load(gpu.percent):c.secondary,false,true);
    }
    if(layout.temperatureY>=0)row(layout.temperatureY,L"温度",s.temperature);
    if(layout.mediaY>=0){
        const float y=layout.mediaY;
        draw.Text(dc,L"媒体",{14,y,40,20},12,c.secondary);
        draw.Text(dc,s.media,{56,y,158,20},12.5f,c.text);
        const bool playing=s.mediaPlaying;
        auto mediaBtn=[&](float bx,PanelPart part,const wchar_t* glyph,bool primary=false){
            const Rect box{bx,y-1,MediaBtnW,22};
            if(const float h=motion.hover[unsigned(part)].At(now);h>0)draw.Round(dc,box,6,Alpha(c.hover,h));
            PanelGlyph(draw,dc,glyph,box,primary?c.text:c.secondary,primary?11.5f:10.5f);
        };
        mediaBtn(MediaPrevX,PanelPart::MediaPrev,L"\xE892");
        mediaBtn(MediaToggleX,PanelPart::MediaToggle,playing?L"\xE769":L"\xE768",true);
        mediaBtn(MediaNextX,PanelPart::MediaNext,L"\xE893");
    }
    if(layout.errorY>=0)draw.Text(dc,s.error,{14,layout.errorY,272,22},11,c.danger);
    if(layout.trayY>=0){draw.Rule(dc,14,layout.trayY-5,286,c.hair);
        draw.Text(dc,L"托盘",{14,layout.trayY,34,24},12,c.secondary);}
}
}
