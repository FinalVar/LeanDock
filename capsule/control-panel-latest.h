#pragma once
#include "metrics-latest.h"
#include "drawing-latest.h"

namespace Capsule {
enum class PanelPart { None, Volume, Mute, Brightness, Wifi, WifiMore, Bluetooth, BluetoothMore,
    DarkTheme, DarkThemeMore, Hotspot, HotspotMore };
inline constexpr float PanelWidth=300,SliderLeft=60,SliderRight=244;
inline PanelPart PanelHit(float x,float y){
    if(y>=60&&y<104){
        if(x>=14&&x<146)return x>=114?PanelPart::WifiMore:PanelPart::Wifi;
        if(x>=154&&x<286)return x>=254?PanelPart::BluetoothMore:PanelPart::Bluetooth;
    }
    if(y>=110&&y<154){
        if(x>=14&&x<146)return x>=114?PanelPart::DarkThemeMore:PanelPart::DarkTheme;
        if(x>=154&&x<286)return x>=254?PanelPart::HotspotMore:PanelPart::Hotspot;
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
    case PanelPart::WifiMore:case PanelPart::BluetoothMore:case PanelPart::DarkThemeMore:case PanelPart::HotspotMore:return true;
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
    float mediaY=-1,temperatureY=-1,errorY=-1,trayY=-1,height=406;
    explicit PanelLayout(Sample const& data,size_t trayCount){
        float y=396;
        if(data.temperature!=L"CPU —　GPU —"&&!data.temperature.empty()){temperatureY=y;y+=22;}
        if(data.media!=L"没有在播放"&&!data.media.empty()){mediaY=y;y+=26;}
        if(!data.error.empty()){errorY=y;y+=24;}
        if(trayCount){trayY=y+9;y+=15+30*float((trayCount+5)/6);}
        height=y+12;
    }
};
inline void PanelGlyph(Drawing& draw,ID2D1DeviceContext* dc,const wchar_t* text,Rect rect,D2D1_COLOR_F color,float size=17){
    auto layout=draw.Layout(text,size,float(rect.w),float(rect.h),false,false,L"Segoe Fluent Icons");
    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    winrt::com_ptr<ID2D1SolidColorBrush> brush;winrt::check_hresult(dc->CreateSolidColorBrush(color,brush.put()));
    dc->DrawTextLayout({float(rect.x),float(rect.y)},layout.get(),brush.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
// The same painter is used by the real popup and the offscreen render checks.
// It has no system writes, device queries, window activation or input synthesis.
inline void PaintControlPanel(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,
    Power const& power,std::wstring const& time,std::wstring const& date,std::wstring const& lunar,
    PanelLayout const& layout,PanelPart hover,PanelGesture const& gesture){
    auto& c=draw.colors;
    draw.Text(dc,time,{14,10,116,36},23,c.text,true);
    draw.Text(dc,date,{138,10,148,18},12,c.secondary,false,true);
    draw.Text(dc,lunar,{128,29,158,18},12,c.secondary,false,true);
    auto tile=[&](float x,float y,const wchar_t* name,int state,bool busy,PanelPart body,PanelPart more,const wchar_t* glyph,std::wstring label){
        auto fill=state==1?c.accent:c.hover;fill.a=state==1?.13f:c.hover.a;
        draw.Round(dc,{x,y,132,44},10,fill);
        if(hover==body||hover==more)draw.Round(dc,{x+(hover==more?100:0),y,hover==more?32.:100.,44},10,c.hover);
        auto ink=state==1?c.accent:c.text;
        PanelGlyph(draw,dc,glyph,{x+6,y+8,26,28},ink,18);
        draw.Text(dc,name,{x+37,y+4,62,20},13,c.text,true);
        draw.Text(dc,busy?L"正在切换":label,{x+37,y+24,64,16},10.5f,c.secondary);
        PanelGlyph(draw,dc,L"\xE76C",{x+100,y+6,30,32},c.secondary,10);
    };
    tile(14,60,L"Wi-Fi",s.wifi,s.wifiBusy,PanelPart::Wifi,PanelPart::WifiMore,L"\xE701",RadioLabel(s.wifi));
    tile(154,60,L"蓝牙",s.bluetooth,s.bluetoothBusy,PanelPart::Bluetooth,PanelPart::BluetoothMore,L"\xE702",RadioLabel(s.bluetooth));
    tile(14,110,L"深色主题",s.darkTheme,s.themeBusy,PanelPart::DarkTheme,PanelPart::DarkThemeMore,L"\xE708",ThemeControl::Label(s.darkTheme));
    tile(154,110,L"移动热点",s.hotspot,s.hotspotBusy,PanelPart::Hotspot,PanelPart::HotspotMore,L"\xEC15",s.hotspotLabel);
    auto slider=[&](float y,int actual,PanelPart part,const wchar_t* glyph){
        int value=gesture.pressed==part?gesture.preview:actual;
        const bool enabled=actual>=0;auto ink=enabled?c.text:c.secondary;
        if(part==PanelPart::Volume&&hover==PanelPart::Mute&&enabled)draw.Round(dc,{14,y-18,34,36},8,c.hover);
        PanelGlyph(draw,dc,glyph,{14,y-18,34,36},ink);
        draw.Round(dc,{SliderLeft,y-2,SliderRight-SliderLeft,4},2,c.hair);
        if(value>=0){const float x=SliderLeft+(SliderRight-SliderLeft)*value/100.f;
            if(value>0)draw.Round(dc,{SliderLeft,y-2,x-SliderLeft,4},2,enabled?c.accent:c.secondary);
            draw.Round(dc,{x-5,y-5,10,10},5,enabled?c.accent:c.secondary);}
        draw.Text(dc,value>=0?std::to_wstring(value):L"—",{252,y-10,34,20},12,ink,false,true);
    };
    slider(182,s.volume,PanelPart::Volume,s.mute?L"\xE74F":L"\xE767");
    slider(224,s.brightness,PanelPart::Brightness,L"\xE706");
    draw.Line(dc,14,252,286,252,c.hair);
    auto row=[&](float y,const wchar_t* label,std::wstring main,std::wstring secondary=L"",bool warn=false){
        draw.Text(dc,label,{14,y,34,20},12,c.secondary);
        const float sub=secondary.empty()?0:draw.Measure(secondary,12)+8;
        draw.Text(dc,main,{56,y,230-sub,20},12,warn?c.warn:c.text);
        if(!secondary.empty())draw.Text(dc,secondary,{56,y,230,20},12,c.secondary,false,true);
    };
    auto percent=[](int n){return n<0?std::wstring(L"—"):std::to_wstring(n)+L"%";};
    row(264,L"CPU",percent(s.cpu));
    draw.Text(dc,L"GPU",{174,264,34,20},12,c.secondary);
    draw.Text(dc,percent(s.gpu),{212,264,74,20},12,c.text,false,true);
    row(286,L"内存",percent(s.memory),s.capacity,s.memory>=90);
    row(308,L"网速",s.network);
    row(330,L"磁盘",s.disks,L"可用");
    row(352,L"电池",power.Present()?power.Percent():L"—",s.battery,
        power.Present()&&!power.AC()&&power.value.BatteryLifePercent<20);
    // GPU identity is useful detail, rather than a decorative chart.
    draw.Text(dc,s.gpuName,{56,374,230,18},10.5f,c.secondary,false,true);
    if(layout.temperatureY>=0)row(layout.temperatureY,L"温度",s.temperature);
    if(layout.mediaY>=0)row(layout.mediaY,L"媒体",s.media);
    if(layout.errorY>=0)draw.Text(dc,s.error,{14,layout.errorY,272,22},11,c.danger);
    if(layout.trayY>=0){draw.Line(dc,14,layout.trayY-5,286,layout.trayY-5,c.hair);
        draw.Text(dc,L"托盘",{14,layout.trayY,34,24},12,c.secondary);}
}
}
