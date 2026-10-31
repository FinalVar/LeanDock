#pragma once
#include "wifi-page-latest.h"
#include <functional>

// The sound page: master volume, the output device, and each app's volume
// and mute, like the Windows volume mixer but inside the panel.
namespace Capsule {
struct SoundView {
    float scroll=0;
    void Reset(){scroll=0;}
};
// A slider drag on this page: the master (empty key) or one app's.
struct SoundDrag {
    std::wstring key;bool active=false;float left=0,right=0;int preview=-1,lastSent=-1;uint64_t sentAt=0;
    int Value(float x)const{return int(std::lround(100*std::clamp((x-left)/(right-left),0.f,1.f)));}
    void Begin(std::wstring k,float l,float r,float x){key=std::move(k);active=true;left=l;right=r;lastSent=-1;sentAt=0;preview=Value(x);}
    // Same pacing as the main page: at most one change per 75 ms, and the
    // value under the pointer on release.
    bool Send(float x,uint64_t now,bool release=false){
        if(!active)return false;preview=Value(x);
        if(preview==lastSent)return false;
        if(!release&&sentAt&&now-sentAt<75)return false;
        lastSent=preview;sentAt=now;return true;
    }
    void Cancel(){key.clear();active=false;preview=lastSent=-1;sentAt=0;}
};
inline constexpr float MasterY=78,SoundTop=104,SoundRight=244,AppSliderLeft=46,OutputRowHeight=44,AppRowHeight=52,SoundLabelHeight=26;
struct SoundRow {
    enum class Kind { Label, Note, Output, App } kind=Kind::Label;
    float y=0,h=SoundLabelHeight;std::wstring text;AudioOutput output;AudioApp app;
};
struct SoundLayout : PageList {
    std::vector<SoundRow> rows;
    SoundLayout(SoundList const& list,float height,bool footer=false):PageList(height,footer,SoundTop){
        float y=0;
        auto add=[&](SoundRow r){r.y=y;y+=r.h;rows.push_back(std::move(r));};
        using Kind=SoundRow::Kind;
        add({Kind::Label,0,SoundLabelHeight,L"输出设备"});
        for(auto const& o:list.outputs)add({Kind::Output,0,OutputRowHeight,{},o});
        y+=4;add({Kind::Label,0,SoundLabelHeight,L"应用"});
        if(list.apps.empty())add({Kind::Note,0,30,L"没有应用在使用声音"});
        for(auto const& a:list.apps)add({Kind::App,0,AppRowHeight,{},{},a});
        content=y;
    }
};
inline PageTarget SoundHit(SoundLayout const& l,SoundView const& v,float x,float y){
    if(Rect{8,8,36,36}.Contains(x,y))return {PageHit::Back};
    if(y>=MasterY-18&&y<MasterY+18){
        if(x>=14&&x<48)return {PageHit::Mute};
        if(x>=52&&x<SoundRight+8)return {PageHit::Slider};
        return {};
    }
    if(!l.Inside(x,y))return {};
    const float at=y-l.top+v.scroll;
    for(auto const& r:l.rows){
        if(at<r.y||at>=r.y+r.h)continue;
        if(r.kind==SoundRow::Kind::Output)return r.output.selected?PageTarget{}:PageTarget{PageHit::Row,r.output.id};
        if(r.kind==SoundRow::Kind::App){
            if(x>=12&&x<42&&at<r.y+32)return {PageHit::Mute,r.app.key};
            if(at>=r.y+24&&x>=AppSliderLeft-8&&x<SoundRight+8)return {PageHit::Slider,r.app.key};
        }
        return {};
    }
    return {};
}
inline const wchar_t* OutputGlyph(int form){
    switch(form){ // EndpointFormFactor
    case 3:return L"\xE7F6";            // Headphones
    case 5:return L"\xE95B";            // Headset
    case 6:return L"\xE717";            // Handset
    case 7:case 8:return L"\xE7F4";     // HDMI / S/PDIF display
    default:return L"\xE7F5";
    }
}
using IconLookup=std::function<IWICBitmapSource*(std::wstring const&)>;
inline void PaintSoundPage(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,SoundLayout const& l,SoundView const& v,
    PageTarget const& hover,float hoverAmount,SoundDrag const& drag,IconLookup const& icon){
    auto& c=draw.colors;auto const& list=s.sound;
    auto lit=[&](PageHit hit,std::wstring const& key={}){return hover.hit==hit&&hover.key==key?hoverAmount:0.f;};
    auto lift=[&](std::wstring const& key){return drag.active&&drag.key==key?1.f:lit(PageHit::Slider,key);};
    PaintPageHeader(draw,dc,L"声音",lit(PageHit::Back));
    // Master volume, as on the main page.
    {
        const bool enabled=s.volume>=0;const auto ink=enabled?c.text:c.secondary;
        const int value=drag.active&&drag.key.empty()?drag.preview:s.volume;
        if(const float h=enabled?lit(PageHit::Mute):0;h>0)draw.Round(dc,{14,MasterY-18,34,36},10,Alpha(c.hover,h));
        PanelGlyph(draw,dc,s.mute?L"\xE74F":L"\xE767",{14,MasterY-18,34,36},ink);
        PaintSlider(draw,dc,SliderLeft,SoundRight,MasterY,float(std::max(0,value)),enabled,lift({}));
        draw.Text(dc,value>=0?std::to_wstring(value):L"—",{252,MasterY-10,34,20},12.5f,ink,false,true);
    }
    if(!list.ready){CenterText(draw,dc,L"正在读取音频设备…",{14,SoundTop+30,272,24},13,c.secondary);return;}
    dc->PushAxisAlignedClip({8,l.top,292,l.bottom},D2D1_ANTIALIAS_MODE_ALIASED);
    const float shift=l.top-v.scroll;
    for(auto const& r:l.rows){
        const float y=r.y+shift;
        if(y+r.h<l.top||y>l.bottom)continue;
        switch(r.kind){
        case SoundRow::Kind::Label:draw.Text(dc,r.text,{14,y+4,200,20},11.5f,c.secondary);break;
        case SoundRow::Kind::Note:draw.Text(dc,r.text,{16,y+2,260,22},12,Alpha(c.secondary,.85f));break;
        case SoundRow::Kind::Output:{
            auto const& o=r.output;
            if(const float h=lit(PageHit::Row,o.id);h>0)draw.Round(dc,{8,y,284,r.h-2},10,Alpha(c.hover,h));
            PanelGlyph(draw,dc,OutputGlyph(o.form),{14,y+7,30,28},o.selected?c.text:c.secondary,16);
            const float title=o.detail.empty()?y+11:y+4;
            draw.Text(dc,o.name,{52,title,196,20},13,c.text,o.selected);
            if(!o.detail.empty())draw.Text(dc,o.detail,{52,y+23,196,16},11,c.secondary);
            if(o.selected)PanelGlyph(draw,dc,L"\xE73E",{252,y+7,30,28},c.accent,13);
            break;
        }
        case SoundRow::Kind::App:{
            auto const& a=r.app;const bool enabled=a.volume>=0;
            const float mh=lit(PageHit::Mute,a.key);
            if(mh>0)draw.Round(dc,{12,y+2,30,30},8,Alpha(c.hover,mh));
            const Rect mark{17,y+7,20,20};
            if(auto image=a.system?nullptr:icon(a.path)){
                if(a.mute)dc->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),nullptr,D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                    D2D1::IdentityMatrix(),.4f),nullptr);
                draw.Image(dc,image,mark);
                if(a.mute)dc->PopLayer();
            }else PanelGlyph(draw,dc,a.system?L"\xE7F5":L"\xECAA",mark,a.mute?Alpha(c.text,.4f):c.text,15);
            if(a.mute){draw.Round(dc,{28,y+18,13,13},6.5f,c.control);PanelGlyph(draw,dc,L"\xE74F",{28,y+18,13,13},c.text,8);}
            draw.Text(dc,a.name,{AppSliderLeft,y+5,200,20},12.5f,a.mute?c.secondary:c.text);
            const int value=drag.active&&drag.key==a.key?drag.preview:a.volume;
            const float sy=y+37;
            PaintSlider(draw,dc,AppSliderLeft,SoundRight,sy,float(std::max(0,value)),enabled,lift(a.key),a.mute);
            draw.Text(dc,value>=0?std::to_wstring(value):L"—",{252,sy-10,34,20},12.5f,a.mute?c.secondary:c.text,false,true);
            break;
        }
        }
    }
    dc->PopAxisAlignedClip();
    PaintScrollbar(draw,dc,l,v.scroll);
}
}
