#pragma once
#include "drawing-latest.h"

namespace Capsule {
// Same 72 x 46 outline, 1.4 DIP stroke and text hierarchy as preview A.
// Coordinates are DIPs. Drawing::Surface rasterizes at the actual monitor DPI.
inline constexpr float TimeRingInset=1.6f,TimeRingStroke=1.4f;
inline constexpr double RingPi=3.14159265358979323846;

inline winrt::com_ptr<ID2D1PathGeometry> TimeRingPath(Drawing& draw,int percent) {
    const float top=TimeRingInset,bottom=TimeHeight-TimeRingInset;
    const float radius=(bottom-top)/2,left=TimeRingInset+radius,right=TimeWidth-left;
    const double straight=right-left;
    double remaining=(2*straight+2*RingPi*radius)*std::clamp(percent,0,100)/100;
    winrt::com_ptr<ID2D1PathGeometry> path;
    winrt::check_hresult(draw.factory->CreatePathGeometry(path.put()));
    winrt::com_ptr<ID2D1GeometrySink> sink;
    winrt::check_hresult(path->Open(sink.put()));
    D2D1_POINT_2F current{TimeWidth/2,top};
    sink->BeginFigure(current,D2D1_FIGURE_BEGIN_HOLLOW);
    auto line=[&](float endX,float y,double length) {
        if(remaining<=0)return;
        const double take=std::min(remaining,length);
        current={float(current.x+(endX-current.x)*take/length),y};
        sink->AddLine(current);remaining-=take;
    };
    auto arc=[&](float center,double start) {
        if(remaining<=0)return;
        const double take=std::min(remaining,RingPi*radius),angle=start+take/radius;
        current={float(center+radius*std::cos(angle)),float(TimeHeight/2+radius*std::sin(angle))};
        sink->AddArc({current,{radius,radius},0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL});
        remaining-=take;
    };
    line(right,top,straight/2);
    arc(right,-RingPi/2);
    line(left,bottom,straight);
    arc(left,RingPi/2);
    line(TimeWidth/2,top,straight/2);
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    winrt::check_hresult(sink->Close());
    return path;
}

inline void PaintTimeKey(Drawing& draw,ID2D1DeviceContext* dc,
                         std::wstring const& time,std::wstring const& date,int percent) {
    const bool known=percent>=0&&percent<=100;
    auto track=Ink(draw.colors.light?0x1E3E34:0xD0E9DD,draw.colors.light?.15f:.16f);
    auto color=percent<=10?Ink(draw.colors.light?0xC45A51:0xED8E86):
        percent<20?Ink(draw.colors.light?0xB88024:0xE1B25C):
        Ink(draw.colors.light?0x368775:0x89C7B3);
    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(dc->CreateSolidColorBrush(track,brush.put()));
    const float radius=TimeHeight/2-TimeRingInset;
    const D2D1_ROUNDED_RECT outline{{TimeRingInset,TimeRingInset,
        TimeWidth-TimeRingInset,TimeHeight-TimeRingInset},radius,radius};
    dc->DrawRoundedRectangle(outline,brush.get(),TimeRingStroke);
    if(known&&percent>0){
        brush->SetColor(color);
        if(percent==100){
            // Closed geometry prevents a seam or two overlapping round caps.
            dc->DrawRoundedRectangle(outline,brush.get(),TimeRingStroke);
        }else{
            auto path=TimeRingPath(draw,percent);
            D2D1_STROKE_STYLE_PROPERTIES props{};
            props.startCap=props.endCap=props.dashCap=D2D1_CAP_STYLE_ROUND;
            props.lineJoin=D2D1_LINE_JOIN_ROUND;props.miterLimit=10;
            winrt::com_ptr<ID2D1StrokeStyle> stroke;
            winrt::check_hresult(draw.factory->CreateStrokeStyle(props,nullptr,0,stroke.put()));
            dc->DrawGeometry(path.get(),brush.get(),TimeRingStroke,stroke.get());
        }
    }
    // No permanent fill. The existing container adds feedback only on hover.
    auto text=[&](std::wstring const& value,float top,float height,float size,
                  bool semi,D2D1_COLOR_F ink) {
        auto layout=draw.Layout(value,size,TimeWidth,height,semi,false,L"Segoe UI");
        layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        brush->SetColor(ink);
        dc->DrawTextLayout({0,top},layout.get(),brush.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    text(time,5,22,18,true,draw.colors.text);
    text(date,28,13,11,false,draw.colors.secondary);
}
}
