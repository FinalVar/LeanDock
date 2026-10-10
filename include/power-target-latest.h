#pragma once
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>

namespace LeanDockPower {
namespace x=winrt::Windows::UI::Xaml;
struct NativeContent {
    unsigned icons{},loaded{},visible{};
    x::FrameworkElement target{nullptr};
};
inline void Inspect(x::DependencyObject const& node,NativeContent& out,unsigned& visited,unsigned depth=0,bool visible=true) {
    if(!node||depth>24||++visited>128)return;
    if(auto fe=node.try_as<x::FrameworkElement>()){
        visible=visible&&fe.Visibility()==x::Visibility::Visible;
        if(winrt::get_class_name(fe)==L"SystemTray.IconView"){
            ++out.icons;
            if(fe.IsLoaded()){
                ++out.loaded;
                if(visible){++out.visible;out.target=fe;}
            }
            return;
        }
    }
    const int count=x::Media::VisualTreeHelper::GetChildrenCount(node);
    for(int i=0;i<count&&visited<128;++i)Inspect(x::Media::VisualTreeHelper::GetChild(node,i),out,visited,depth+1,visible);
}
inline NativeContent Inspect(x::DependencyObject const& node) {
    NativeContent out;unsigned visited=0;Inspect(node,out,visited);return out;
}
inline bool DescendsFrom(x::DependencyObject node,x::DependencyObject const& ancestor) {
    for(unsigned depth=0;node&&depth<32;++depth,node=x::Media::VisualTreeHelper::GetParent(node))
        if(node==ancestor)return true;
    return false;
}
// Only the topmost object receives pointer input. An object merely appearing
// somewhere in the hit stack does not prove that it owns that point.
inline bool TopHit(x::UIElement const& root,x::FrameworkElement const& target,
                   winrt::Windows::Foundation::Point point) {
    auto hits=x::Media::VisualTreeHelper::FindElementsInHostCoordinates(point,root,false).First();
    return hits.HasCurrent()&&DescendsFrom(hits.Current(),target);
}
}
