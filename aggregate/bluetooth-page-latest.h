#pragma once
#include "wifi-page-latest.h"

// The Bluetooth page: paired devices with their connection and battery, and
// connect or disconnect for audio devices. Pairing new devices stays in
// Windows Settings; the page says so rather than linking there.
namespace Capsule {
struct BluetoothView {
    std::wstring expanded;float scroll=0;
    void Reset(){expanded.clear();scroll=0;}
};
inline constexpr float DeviceRowHeight=48;
struct BluetoothRow {
    BluetoothDevice device;float y=0,h=DeviceRowHeight;
    bool open=false,status=false;Rect button; // list coordinates, before scrolling
};
struct BluetoothLayout : PageList {
    std::vector<BluetoothRow> rows;
    BluetoothLayout(BluetoothList const& list,BluetoothView const& v,float height,bool footer=false):PageList(height,footer){
        float y=0;
        for(auto const& d:list.devices){
            BluetoothRow r;r.device=d;r.y=y;r.open=d.audio&&d.id==v.expanded;
            if(r.open){
                float at=y+DeviceRowHeight;
                r.status=list.failed==d.id&&!list.failure.empty();if(r.status)at+=20;
                r.button={188,at,96,32};at+=44;r.h=at-y;
            }
            rows.push_back(std::move(r));y+=rows.back().h+2;
        }
        content=y;
    }
    BluetoothRow const* Find(std::wstring const& id)const{
        for(auto const& r:rows)if(r.device.id==id)return &r;return nullptr;
    }
};
inline PageTarget BluetoothHit(BluetoothLayout const& l,BluetoothView const& v,float x,float y){
    if(Rect{8,8,36,36}.Contains(x,y))return {PageHit::Back};
    if(Rect{232,12,58,32}.Contains(x,y))return {PageHit::Toggle};
    if(!l.Inside(x,y))return {};
    const float at=y-l.top+v.scroll;
    for(auto const& r:l.rows){
        if(at<r.y||at>=r.y+r.h)continue;
        if(r.open&&at>=r.y+DeviceRowHeight)return r.button.Contains(x,at)?PageTarget{PageHit::Primary,r.device.id}:PageTarget{};
        // Only audio devices open: Windows reconnects the others by itself.
        return r.device.audio?PageTarget{PageHit::Row,r.device.id}:PageTarget{};
    }
    return {};
}
inline const wchar_t* BluetoothGlyph(BluetoothKind kind){
    switch(kind){
    case BluetoothKind::Headphones:return L"\xE7F6";
    case BluetoothKind::Speaker:return L"\xE7F5";
    case BluetoothKind::Mouse:return L"\xE962";
    case BluetoothKind::Keyboard:return L"\xE765";
    case BluetoothKind::Gamepad:return L"\xE7FC";
    case BluetoothKind::Phone:return L"\xE8EA";
    case BluetoothKind::Computer:return L"\xE977";
    default:return L"\xE702";
    }
}
inline std::wstring BluetoothSubtitle(BluetoothDevice const& d,BluetoothList const& list){
    if(list.busy==d.id)return list.connecting?L"正在连接…":L"正在断开…";
    if(!d.connected)return L"未连接";
    return d.battery>=0?L"已连接 · 电量 "+std::to_wstring(d.battery)+L"%":std::wstring(L"已连接");
}
inline void PaintBluetoothPage(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,BluetoothLayout const& l,BluetoothView const& v,
    PageTarget const& hover,float hoverAmount,float on){
    auto& c=draw.colors;auto const& list=s.bluetoothList;
    auto lit=[&](PageHit hit,std::wstring const& key={}){return hover.hit==hit&&hover.key==key?hoverAmount:0.f;};
    PaintPageHeader(draw,dc,L"蓝牙",lit(PageHit::Back));
    PaintSwitch(draw,dc,246,18,on,PanelEnabled(PanelPart::Bluetooth,s),lit(PageHit::Toggle));
    std::wstring empty,hint;
    if(s.bluetooth==0)empty=L"蓝牙已关闭";
    else if(s.bluetooth==3)empty=L"蓝牙已被硬件开关关闭";
    else if(s.bluetooth<0)empty=L"没有找到蓝牙适配器";
    else if(!list.ready)empty=L"正在读取设备…";
    else if(list.devices.empty()){empty=L"没有已配对的设备";hint=L"新设备请在 Windows 设置中配对";}
    if(!empty.empty()){
        CenterText(draw,dc,empty,{14,l.top+40,272,24},13,c.secondary);
        if(!hint.empty())CenterText(draw,dc,hint,{14,l.top+64,272,20},11.5f,Alpha(c.secondary,.8f));
        return;
    }
    dc->PushAxisAlignedClip({8,l.top,292,l.bottom},D2D1_ANTIALIAS_MODE_ALIASED);
    const float shift=l.top-v.scroll;
    for(auto const& r:l.rows){
        const float y=r.y+shift;auto const& d=r.device;
        if(y+r.h<l.top||y>l.bottom)continue;
        if(r.open){
            draw.Round(dc,{8,y,284,r.h},10,c.control);
            auto edge=draw.Gradient(dc,{0,y},{0,y+24},{{0,c.controlEdge},{1,Alpha(c.controlEdge,0)}});
            draw.Stroke(dc,{8.5,y+.5,283,r.h-1},9.5f,edge.get());
        }
        if(const float h=lit(PageHit::Row,d.id);h>0)draw.Round(dc,{8,y,284,DeviceRowHeight},10,Alpha(c.hover,h));
        const bool busy=list.busy==d.id;
        PanelGlyph(draw,dc,BluetoothGlyph(d.kind),{14,y+10,30,28},d.connected?c.text:c.secondary,17);
        draw.Text(dc,d.name,{52,y+6,228,20},13,c.text,d.connected);
        draw.Text(dc,BluetoothSubtitle(d,list),{52,y+25,228,16},11,busy?c.accent:c.secondary);
        if(!r.open)continue;
        if(r.status)draw.Text(dc,list.failure,{20,y+DeviceRowHeight,260,18},11.5f,c.danger);
        const Rect button{r.button.x,r.button.y+shift,r.button.w,r.button.h};
        const bool idle=list.busy.empty();
        if(d.connected)PaintButton(draw,dc,button,busy?L"正在断开":L"断开连接",false,idle,lit(PageHit::Primary,d.id));
        else PaintButton(draw,dc,button,busy?L"正在连接":L"连接",true,idle,lit(PageHit::Primary,d.id));
    }
    dc->PopAxisAlignedClip();
    PaintScrollbar(draw,dc,l,v.scroll);
}
}
