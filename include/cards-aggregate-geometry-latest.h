#pragma once
#include "cards-aggregate-policy-latest.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Media.h>
namespace LeanDockAggregate {
inline winrt::Windows::UI::Xaml::Media::PathGeometry Geometry(int percent){
    using namespace winrt::Windows::UI::Xaml::Media;
    PathGeometry geometry;PathFigure figure;figure.StartPoint({float(Width/2),float(Inset)});
    figure.IsClosed(percent==100);figure.IsFilled(false);
    for(auto const& part:Ring(percent)){
        if(part.arc){ArcSegment arc;arc.Point({float(part.x),float(part.y)});arc.Size({float(part.radius),float(part.radius)});
            arc.SweepDirection(SweepDirection::Clockwise);arc.IsLargeArc(false);figure.Segments().Append(arc);}
        else{LineSegment line;line.Point({float(part.x),float(part.y)});figure.Segments().Append(line);}
    }
    geometry.Figures().Append(figure);return geometry;
}
}
