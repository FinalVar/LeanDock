#pragma once
#include "control-panel-latest.h"

// The Wi-Fi page: networks, connect, disconnect and password entry inside
// the panel. Like the main page it is one painted surface; the painter has no
// device queries, system writes or input synthesis.
namespace Capsule {
enum class PageHit { None, Back, Toggle, Row, User, Field, Reveal, Primary };
struct PageTarget {
    PageHit hit=PageHit::None;std::wstring key; // key: the row's SSID
    bool operator==(PageTarget const&)const=default;
};
struct WifiView {
    enum class Focus { None, User, Password };
    std::wstring expanded,user,password; // credentials stay on this page and are wiped when done
    Focus focus=Focus::None;bool reveal=false;float scroll=0;
    // Room for the longest key up front: growing would leave copies behind.
    WifiView(){password.reserve(64);}
    void Wipe(){
        SecureZeroMemory(password.data(),password.size()*sizeof(wchar_t));password.clear();
        user.clear();focus=Focus::None;reveal=false;
    }
    void Reset(){Wipe();expanded.clear();scroll=0;}
};
inline constexpr float PageListTop=58,WifiRowHeight=48;
struct WifiRow {
    WifiNetwork net;float y=0,h=WifiRowHeight;
    bool open=false,status=false,form=false,user=false;
    Rect userBox,passwordBox,reveal,button; // list coordinates, before scrolling
};
struct WifiLayout {
    std::vector<WifiRow> rows;float top=PageListTop,bottom=0,view=0,content=0;
    WifiLayout(WifiList const& list,WifiView const& v,float height){
        bottom=height-8;view=bottom-top;float y=0;
        for(auto const& net:list.networks){
            WifiRow r;r.net=net;r.y=y;r.open=net.ssid==v.expanded;
            if(r.open){
                float at=y+WifiRowHeight;
                const bool connecting=list.connecting==net.ssid,failed=list.failed==net.ssid&&!list.failure.empty();
                r.status=connecting||failed;if(r.status)at+=20;
                r.form=!net.connected&&!connecting&&net.secure&&(!net.saved||(failed&&list.wrongPassword));
                r.user=r.form&&net.enterprise;
                if(r.user){r.userBox={16,at,268,34};at+=42;}
                if(r.form){r.passwordBox={16,at,268,34};r.reveal={252,at+3,28,28};at+=42;}
                r.button={188,at,96,32};at+=44;r.h=at-y;
            }
            rows.push_back(std::move(r));y+=rows.back().h+2;
        }
        content=y;
    }
    float Clamp(float scroll)const{return std::clamp(scroll,0.f,std::max(0.f,content-view));}
    WifiRow const* Find(std::wstring const& ssid)const{
        for(auto const& r:rows)if(r.net.ssid==ssid)return &r;return nullptr;
    }
    // The scroll that shows the whole row, or as much of it from the top.
    float Reveal(WifiRow const& r,float scroll)const{
        if(r.y+r.h>scroll+view)scroll=r.y+r.h-view;
        if(r.y<scroll)scroll=r.y;
        return Clamp(scroll);
    }
};
inline bool WifiFormReady(WifiRow const& r,WifiView const& v){
    return !r.form||(!v.password.empty()&&(!r.user||!v.user.empty()));
}
inline PageTarget WifiHit(WifiLayout const& l,WifiView const& v,float x,float y){
    if(Rect{8,8,36,36}.Contains(x,y))return {PageHit::Back};
    if(Rect{232,12,58,32}.Contains(x,y))return {PageHit::Toggle};
    if(y<l.top||y>=l.bottom||x<8||x>=292)return {};
    const float at=y-l.top+v.scroll;
    for(auto const& r:l.rows){
        if(at<r.y||at>=r.y+r.h)continue;
        if(r.open&&at>=r.y+WifiRowHeight){
            if(r.user&&r.userBox.Contains(x,at))return {PageHit::User,r.net.ssid};
            if(r.form&&r.reveal.Contains(x,at))return {PageHit::Reveal,r.net.ssid};
            if(r.form&&r.passwordBox.Contains(x,at))return {PageHit::Field,r.net.ssid};
            if(r.button.Contains(x,at))return {PageHit::Primary,r.net.ssid};
            return {};
        }
        return {PageHit::Row,r.net.ssid};
    }
    return {};
}
inline std::wstring WifiSubtitle(WifiNetwork const& n,WifiList const& list){
    if(list.connecting==n.ssid)return L"正在连接…";
    if(n.connected)return n.secure?L"已连接，安全":L"已连接，开放";
    return std::wstring(n.saved?L"已保存，":L"")+(n.enterprise?L"企业网络":n.secure?L"安全":L"开放");
}
inline winrt::com_ptr<ID2D1SolidColorBrush> Solid(ID2D1DeviceContext* dc,D2D1_COLOR_F color){
    winrt::com_ptr<ID2D1SolidColorBrush> brush;winrt::check_hresult(dc->CreateSolidColorBrush(color,brush.put()));return brush;
}
inline void CenterText(Drawing& draw,ID2D1DeviceContext* dc,std::wstring const& text,Rect r,float size,D2D1_COLOR_F color,bool semi=false){
    auto layout=draw.Layout(text,size,float(r.w),float(r.h),semi);
    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    dc->DrawTextLayout({float(r.x),float(r.y)},layout.get(),Solid(dc,color).get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
// Windows' Wi-Fi mark: a dot and three arcs, lit up to the signal strength.
inline void PaintSignal(Drawing& draw,ID2D1DeviceContext* dc,float cx,float cy,int bars,D2D1_COLOR_F on,D2D1_COLOR_F off){
    winrt::com_ptr<ID2D1StrokeStyle> round;
    D2D1_STROKE_STYLE_PROPERTIES style{D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND,D2D1_CAP_STYLE_ROUND,D2D1_LINE_JOIN_ROUND};
    winrt::check_hresult(draw.factory->CreateStrokeStyle(style,nullptr,0,round.put()));
    for(int i=0;i<3;++i){
        const float r=4.5f+4.5f*float(i),k=.7071f;
        winrt::com_ptr<ID2D1PathGeometry> arc;winrt::check_hresult(draw.factory->CreatePathGeometry(arc.put()));
        winrt::com_ptr<ID2D1GeometrySink> sink;winrt::check_hresult(arc->Open(sink.put()));
        sink->BeginFigure({cx-r*k,cy-r*k},D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc({{cx+r*k,cy-r*k},{r,r},0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
        sink->EndFigure(D2D1_FIGURE_END_OPEN);winrt::check_hresult(sink->Close());
        dc->DrawGeometry(arc.get(),Solid(dc,bars>=i+2?on:off).get(),1.7f,round.get());
    }
    dc->FillEllipse({{cx,cy},1.7f,1.7f},Solid(dc,bars>=1?on:off).get());
}
inline void PaintSwitch(Drawing& draw,ID2D1DeviceContext* dc,float x,float y,float on,bool enabled,float hover){
    auto& c=draw.colors;const auto ink=Alpha(c.text,enabled?.62f:.3f);
    if(on<1)draw.Stroke(dc,{x+.5,y+.5,39,19},9.5f,Solid(dc,Alpha(ink,1-on)).get());
    if(on>0)draw.Round(dc,{x,y,40,20},10,Alpha(enabled?c.accent:Alpha(c.accent,.45f),on));
    const float r=6+(enabled?hover:0),cx=x+10+20*on;
    dc->FillEllipse({{cx,y+10},r,r},Solid(dc,Mix(ink,c.onAccent,on)).get());
}
inline void PaintButton(Drawing& draw,ID2D1DeviceContext* dc,Rect r,std::wstring const& label,bool primary,bool enabled,float hover){
    auto& c=draw.colors;const float a=enabled?1:.45f;
    if(primary){
        auto fill=draw.Gradient(dc,{0,float(r.y)},{0,float(r.y+r.h)},{{0,Alpha(Lighter(c.accent,.12f),a)},{1,Alpha(c.accent,a)}});
        draw.Fill(dc,r,8,fill.get());
    }else{
        draw.Round(dc,r,8,c.control);
        auto edge=draw.Gradient(dc,{0,float(r.y)},{0,float(r.y+r.h/2)},{{0,c.controlEdge},{1,Alpha(c.controlEdge,0)}});
        draw.Stroke(dc,{r.x+.5,r.y+.5,r.w-1,r.h-1},7.5f,edge.get());
    }
    if(enabled&&hover>0)draw.Round(dc,r,8,Alpha(primary?Ink(0xFFFFFF,.14f):c.hover,hover));
    CenterText(draw,dc,label,{r.x,r.y,r.w,r.h},12.5f,Alpha(primary?c.onAccent:c.text,a),true);
}
inline void PaintField(Drawing& draw,ID2D1DeviceContext* dc,Rect r,std::wstring const& text,bool secret,bool reveal,
    bool focused,const wchar_t* placeholder,float revealHover){
    auto& c=draw.colors;
    draw.Round(dc,r,6,c.control);
    draw.Stroke(dc,{r.x+.5,r.y+.5,r.w-1,r.h-1},5.5f,Solid(dc,focused?Alpha(c.accent,.7f):c.hair).get());
    if(focused)draw.Round(dc,{r.x+2,r.y+r.h-2,r.w-4,2},1,c.accent);
    const float left=float(r.x)+10,inner=float(r.w)-(secret?44:20);
    const std::wstring shown=secret&&!reveal?std::wstring(text.size(),L'\x25CF'):text;
    const float width=shown.empty()?0:draw.Measure(shown,13),shift=std::max(0.f,width-inner+2);
    dc->PushAxisAlignedClip({left,float(r.y),left+inner,float(r.y+r.h)},D2D1_ANTIALIAS_MODE_ALIASED);
    if(shown.empty())draw.Text(dc,placeholder,{left,r.y,inner,r.h},13,c.secondary);
    else draw.Text(dc,shown,{left-shift,r.y,width+8,r.h},13,c.text);
    if(focused){const float x=left-shift+width+.5f;draw.Line(dc,x,float(r.y)+9,x,float(r.y+r.h)-9,c.text);}
    dc->PopAxisAlignedClip();
    if(secret){
        const Rect eye{r.x+r.w-32,r.y+3,28,28};
        if(revealHover>0)draw.Round(dc,eye,6,Alpha(c.hover,revealHover));
        PanelGlyph(draw,dc,L"\xE7B3",eye,reveal?c.text:c.secondary,13);
    }
}
inline void PaintPageHeader(Drawing& draw,ID2D1DeviceContext* dc,const wchar_t* title,float backHover){
    auto& c=draw.colors;
    if(backHover>0)draw.Round(dc,{8,8,36,36},8,Alpha(c.hover,backHover));
    PanelGlyph(draw,dc,L"\xE72B",{8,8,36,36},c.text,15);
    draw.Text(dc,title,{50,8,170,36},15,c.text,true);
    draw.Rule(dc,14,52,286,c.hair);
}
inline void PaintWifiPage(Drawing& draw,ID2D1DeviceContext* dc,Sample const& s,WifiLayout const& l,WifiView const& v,
    PageTarget const& hover,float hoverAmount,float on){
    auto& c=draw.colors;auto const& list=s.wifiList;
    auto lit=[&](PageHit hit,std::wstring const& key={}){return hover.hit==hit&&hover.key==key?hoverAmount:0.f;};
    PaintPageHeader(draw,dc,L"Wi-Fi",lit(PageHit::Back));
    PaintSwitch(draw,dc,246,18,on,PanelEnabled(PanelPart::Wifi,s),lit(PageHit::Toggle));
    const Rect middle{14,l.top+40,272,24};
    std::wstring empty;
    if(s.wifi==0)empty=L"Wi-Fi 已关闭";
    else if(s.wifi==3)empty=L"无线网卡已被硬件开关关闭";
    else if(!list.problem.empty())empty=list.problem;
    else if(!list.ready||(list.networks.empty()&&list.scanning))empty=L"正在查找网络…";
    else if(list.networks.empty())empty=L"附近没有可用的网络";
    if(!empty.empty()){CenterText(draw,dc,empty,middle,13,c.secondary);return;}
    dc->PushAxisAlignedClip({8,l.top,292,l.bottom},D2D1_ANTIALIAS_MODE_ALIASED);
    const float shift=l.top-v.scroll;
    auto at=[&](Rect r){r.y+=shift;return r;};
    for(auto const& r:l.rows){
        const float y=r.y+shift;
        if(y+r.h<l.top||y>l.bottom)continue;
        const Rect box{8,y,284,r.h};
        if(r.open){
            draw.Round(dc,box,10,c.control);
            auto edge=draw.Gradient(dc,{0,y},{0,y+24},{{0,c.controlEdge},{1,Alpha(c.controlEdge,0)}});
            draw.Stroke(dc,{8.5,y+.5,283,r.h-1},9.5f,edge.get());
        }
        if(const float h=lit(PageHit::Row,r.net.ssid);h>0)
            draw.Round(dc,{8,y,284,WifiRowHeight},10,Alpha(c.hover,h));
        const bool connecting=list.connecting==r.net.ssid;
        PaintSignal(draw,dc,30,y+30,r.net.bars,c.text,Alpha(c.text,.22f));
        if(r.net.secure)PanelGlyph(draw,dc,L"\xE72E",{33,y+25,13,13},c.text,8);
        draw.Text(dc,r.net.ssid,{52,y+6,228,20},13,c.text,r.net.connected);
        draw.Text(dc,WifiSubtitle(r.net,list),{52,y+25,228,16},11,connecting?c.accent:c.secondary);
        if(!r.open)continue;
        if(r.status)draw.Text(dc,connecting?L"正在连接，请稍候":list.failure,{20,y+WifiRowHeight,260,18},11.5f,connecting?c.secondary:c.danger);
        if(r.user)PaintField(draw,dc,at(r.userBox),v.user,false,false,v.focus==WifiView::Focus::User,L"用户名",0);
        if(r.form)PaintField(draw,dc,at(r.passwordBox),v.password,true,v.reveal,v.focus==WifiView::Focus::Password,
            L"网络安全密钥",lit(PageHit::Reveal,r.net.ssid));
        const bool busy=!list.connecting.empty();
        if(r.net.connected)PaintButton(draw,dc,at(r.button),L"断开连接",false,!busy,lit(PageHit::Primary,r.net.ssid));
        else PaintButton(draw,dc,at(r.button),connecting?L"正在连接":L"连接",true,!busy&&WifiFormReady(r,v),lit(PageHit::Primary,r.net.ssid));
    }
    dc->PopAxisAlignedClip();
    if(l.content>l.view){
        const float thumb=std::max(24.f,l.view*l.view/l.content),y=l.top+(l.view-thumb)*v.scroll/(l.content-l.view);
        draw.Round(dc,{294,y,2.5,thumb},1.25f,Alpha(c.text,.24f));
    }
}
}
