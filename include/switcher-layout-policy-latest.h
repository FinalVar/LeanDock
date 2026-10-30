#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// Pure sizing policy; native DynamicFlowPanel retains ordering, thumbnails,
// navigation, hit testing and arrangement. Values are XAML DIPs, never pixels.
namespace NativeSwitcherLayout {
struct Item { double aspect{1.6}, chromeHeight{42}, chromeWidth{2}; };
struct Plan { bool valid{}; double cardHeight{}; unsigned rows{}; double width{}, height{}; };
constexpr double kGap=24; // Verified in the pinned native row-size calculation.
constexpr double kHeightStep=24;
constexpr double kComfortHeightFraction=0.75;

inline double ComfortHeight(double availableHeight) {
    if(!std::isfinite(availableHeight) || availableHeight<=0) return 0;
    // A sparse list may use most of the useful height, while retaining desktop
    // context. Scale with the display; a fixed DIP cap left large screens empty.
    return std::floor(availableHeight*kComfortHeightFraction/kHeightStep)*kHeightStep;
}

inline Plan AtHeight(double width, double height, std::vector<Item> const& items, double cardHeight) {
    Plan result{};
    if(items.empty() || items.size()>128 || !std::isfinite(width) || !std::isfinite(height) ||
       width<=0 || height<=0 || !std::isfinite(cardHeight) || cardHeight<=0) return result;
    unsigned rows=1;
    double rowWidth=0,maxWidth=0;
    for(auto const& item:items) {
        if(!std::isfinite(item.aspect) || item.aspect<=0 || !std::isfinite(item.chromeHeight) ||
           !std::isfinite(item.chromeWidth) || item.chromeHeight<0 || item.chromeWidth<0 ||
           cardHeight<=item.chromeHeight+4) return {};
        const double w=(cardHeight-item.chromeHeight)*item.aspect+item.chromeWidth+kGap;
        if(w>width) return {};
        if(rowWidth>0 && rowWidth+w>width) {maxWidth=std::max(maxWidth,rowWidth);rowWidth=0;++rows;}
        rowWidth+=w;
    }
    maxWidth=std::max(maxWidth,rowWidth);
    const double usedHeight=rows*(cardHeight+kGap);
    if(usedHeight>height) return {};
    return {true,cardHeight,rows,maxWidth,usedHeight};
}

inline Plan Choose(double width, double height, std::vector<Item> const& items,
                   double ceiling=std::numeric_limits<double>::max()) {
    if(!std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0 ||
       !std::isfinite(ceiling) || ceiling<=0 || items.empty() || items.size()>128) return {};
    // Search the same size ladder for every count. Fewer items can grow;
    // additional items reduce the size only when width or height requires it.
    const double top=std::floor(std::min(ComfortHeight(height),ceiling)/kHeightStep)*kHeightStep;
    for(double h=top;h>=48;h-=kHeightStep) {
        auto plan=AtHeight(width,height,items,h);
        if(plan.valid) return plan;
    }
    return {};
}
} // namespace NativeSwitcherLayout
