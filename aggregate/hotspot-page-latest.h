#pragma once
#include "wifi-page-latest.h"

// The hotspot page: the network phones join (name, key, band), where the
// connection comes from, and who is connected. The settings can be edited
// while the hotspot is off. The key is shown only on request and, like the
// Wi-Fi password, lives only in this view's reserved buffers.
namespace Capsule {
struct HotspotView {
    enum class Focus { None, Name, Password };
    bool editing=false,reveal=false,saving=false;Focus focus=Focus::None;int band=-1;uint64_t serial=0;
    std::wstring name,password,shown; // password: the key being edited; shown: the current key while revealed
    HotspotView(){password.reserve(64);shown.reserve(64);}
    void Hide(){SecureZeroMemory(shown.data(),shown.size()*sizeof(wchar_t));shown.clear();reveal=false;}
    void Wipe(){SecureZeroMemory(password.data(),password.size()*sizeof(wchar_t));password.clear();Hide();focus=Focus::None;}
    void Reset(){Wipe();editing=saving=false;name.clear();band=-1;serial=0;}
};
inline const wchar_t* BandLabel(int band){
    return band==1?L"2.4 GHz":band==2?L"5 GHz":band==3?L"6 GHz":band==0?L"任何可用频段":L"—";
}
// Windows' own limits: an SSID is at most 32 bytes, a WPA2 key 8-63
// printable ASCII characters.
inline const wchar_t* HotspotProblem(std::wstring const& name,std::wstring const& key){
    if(name.empty())return L"请输入网络名称";
    if(WideCharToMultiByte(CP_UTF8,0,name.c_str(),int(name.size()),nullptr,0,nullptr,nullptr)>32)return L"网络名称最多 32 字节（约 10 个汉字）";
    if(key.size()<8)return L"密码至少需要 8 个字符";
    for(auto ch:key)if(ch<0x20||ch>0x7E)return L"密码只能用英文字母、数字和符号";
    return nullptr;
}
inline bool HotspotEditable(Sample const& s){
    return s.hotspot==0&&!s.hotspotBusy&&s.hotspotInfo.ready&&!s.hotspotInfo.ssid.empty()&&!s.hotspotInfo.saving;
}
// Fixed places: the details (view) or the form (editing), then clients.
inline constexpr Rect HotspotEye{254,91,28,28},HotspotEdit{188,186,96,32},
    HotspotNameBox{16,78,268,34},HotspotKeyBox{16,136,268,34},HotspotKeyEye{252,139,28,28},
    HotspotCancel{84,256,96,32},HotspotSave{188,256,96,32};
inline constexpr float HotspotClientsTop=262,ClientRowHeight=30;
inline Rect HotspotBand(int band){return {16+91.0*band,194,86,30};}
inline PageTarget HotspotHit(HotspotView const& v,float x,float y){
    if(Rect{8,8,36,36}.Contains(x,y))return {PageHit::Back};
    if(Rect{232,12,58,32}.Contains(x,y))return {PageHit::Toggle};
    if(!v.editing){
        if(HotspotEye.Contains(x,y))return {PageHit::Reveal};
        if(HotspotEdit.Contains(x,y))return {PageHit::Secondary};
        return {};
    }
    if(HotspotNameBox.Contains(x,y))return {PageHit::Name};
    if(HotspotKeyEye.Contains(x,y))return {PageHit::Reveal};
    if(HotspotKeyBox.Contains(x,y))return {PageHit::Field};
    for(int band=0;band<3;++band)if(HotspotBand(band).Contains(x,y))return {PageHit::Option,std::to_wstring(band)};
    if(HotspotCancel.Contains(x,y))return {PageHit::Secondary};
    if(HotspotSave.Contains(x,y))return {PageHit::Primary};
    return {};
}
inline void PaintHotspotPage(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,HotspotView const& v,
    PageTarget const& hover,float hoverAmount,float on,float height,bool footer){
    auto& c=draw.colors;auto const& h=s.hotspotInfo;
    auto lit=[&](PageHit hit,std::wstring const& key={}){return hover.hit==hit&&hover.key==key?hoverAmount:0.f;};
    PaintPageHeader(draw,dc,L"移动热点",lit(PageHit::Back));
    PaintSwitch(draw,dc,246,18,on,PanelEnabled(PanelPart::Hotspot,s),lit(PageHit::Toggle));
    if(!h.ready){CenterText(draw,dc,L"正在读取热点设置…",{14,PageListTop+40,272,24},13,c.secondary);return;}
    if(h.ssid.empty()){
        CenterText(draw,dc,s.hotspotLabel==L"未连接网络"?L"没有可共享的网络连接":L"这台电脑现在不能开热点",{14,PageListTop+40,272,24},13,c.secondary);
        return;
    }
    if(v.editing){
        auto label=[&](float y,const wchar_t* text){draw.Text(dc,text,{16,y,200,18},11.5f,c.secondary);};
        label(60,L"网络名称");
        PaintField(draw,dc,HotspotNameBox,v.name,false,false,v.focus==HotspotView::Focus::Name,L"网络名称",0);
        label(118,L"密码");
        PaintField(draw,dc,HotspotKeyBox,v.password,true,v.reveal,v.focus==HotspotView::Focus::Password,
            L"8–63 个字符",lit(PageHit::Reveal));
        label(176,L"频段");
        for(int band=0;band<3;++band){
            const auto r=HotspotBand(band);const bool chosen=v.band==band,allowed=h.bands[size_t(band)];
            if(chosen){auto fill=draw.Gradient(dc,{0,float(r.y)},{0,float(r.y+r.h)},{{0,Lighter(c.accent,.12f)},{1,c.accent}});draw.Fill(dc,r,8,fill.get());}
            else{draw.Round(dc,r,8,c.control);draw.Stroke(dc,{r.x+.5,r.y+.5,r.w-1,r.h-1},7.5f,Solid(dc,c.hair).get());}
            if(const float a=allowed&&!chosen?lit(PageHit::Option,std::to_wstring(band)):0;a>0)draw.Round(dc,r,8,Alpha(c.hover,a));
            const auto ink=chosen?c.onAccent:allowed?c.text:Alpha(c.text,.35f);
            CenterText(draw,dc,band?BandLabel(band):L"任何可用",r,12,ink,chosen);
        }
        // What blocks saving, or how the last attempt ended.
        const auto problem=HotspotProblem(v.name,v.password);
        const std::wstring note=v.saving?L"正在保存…":v.serial&&!h.failure.empty()?h.failure:problem?problem:L"";
        draw.Text(dc,note,{16,230,268,20},11.5f,v.saving?c.secondary:c.danger);
        PaintButton(draw,dc,HotspotCancel,L"取消",false,!v.saving,lit(PageHit::Secondary));
        PaintButton(draw,dc,HotspotSave,v.saving?L"正在保存":L"保存",true,!v.saving&&!problem&&HotspotEditable(s),lit(PageHit::Primary));
        return;
    }
    auto row=[&](float y,const wchar_t* label,std::wstring const& value,D2D1_COLOR_F ink){
        draw.Text(dc,label,{14,y,64,30},12,c.secondary);
        draw.Text(dc,value,{82,y,166,30},13,ink);
    };
    row(60,L"网络名称",h.ssid,c.text);
    row(90,L"密码",v.reveal?v.shown:std::wstring(std::max<size_t>(h.keyLength,8),L'\x25CF'),c.text);
    if(const float a=lit(PageHit::Reveal);a>0)draw.Round(dc,HotspotEye,6,Alpha(c.hover,a));
    PanelGlyph(draw,dc,L"\xE7B3",HotspotEye,v.reveal?c.text:c.secondary,13);
    row(120,L"频段",BandLabel(h.band),c.text);
    row(150,L"共享来源",h.source.empty()?L"—":h.source,c.text);
    const bool editable=HotspotEditable(s);
    if(!editable)draw.Text(dc,s.hotspot==1?L"关闭热点后可以修改":h.saving?L"正在保存…":L"",{14,HotspotEdit.y,170,HotspotEdit.h},11.5f,c.secondary);
    PaintButton(draw,dc,HotspotEdit,L"编辑",false,editable,lit(PageHit::Secondary));
    draw.Rule(dc,14,230,286,c.hair);
    draw.Text(dc,L"已连接的设备",{14,236,160,22},12,c.secondary);
    if(s.hotspot==1)draw.Text(dc,std::to_wstring(h.clients.size())+L" / "+std::to_wstring(h.maxClients),{186,236,100,22},12,c.secondary,false,true);
    const float bottom=height-8-(footer?PageFooter:0);
    if(s.hotspot!=1||h.clients.empty()){
        CenterText(draw,dc,s.hotspot==1?L"还没有设备连接":L"开启后，连上的设备会显示在这里",{14,HotspotClientsTop+8,272,22},12,Alpha(c.secondary,.85f));
        return;
    }
    const size_t fit=size_t(std::max(1.f,std::floor((bottom-HotspotClientsTop)/ClientRowHeight)));
    for(size_t i=0;i<h.clients.size()&&i<fit;++i){
        const float y=HotspotClientsTop+ClientRowHeight*float(i);
        if(i+1==fit&&h.clients.size()>fit){
            draw.Text(dc,L"还有 "+std::to_wstring(h.clients.size()-i)+L" 台",{44,y,200,ClientRowHeight},12,c.secondary);break;}
        auto const& client=h.clients[i];
        PanelGlyph(draw,dc,L"\xE8EA",{14,y+1,24,28},c.text,14);
        const float w=draw.Measure(client.address,11.5f)+8;
        draw.Text(dc,client.name.empty()?L"未命名设备":client.name,{44,y,242-w,ClientRowHeight},12.5f,c.text);
        draw.Text(dc,client.address,{44,y,242,ClientRowHeight},11.5f,c.secondary,false,true);
    }
}
}
