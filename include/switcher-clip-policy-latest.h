#pragma once
#include <cmath>

namespace NativeSwitcherClip {
struct Size { double width{},height{}; };
struct Rect { double x{},y{},width{},height{}; };
enum class Action { Ignore, Synchronized, RepairPreviousSize };
inline bool Close(double a,double b) noexcept {
    return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=0.75;
}
inline bool Valid(Size s) noexcept {
    return std::isfinite(s.width)&&std::isfinite(s.height)&&s.width>0&&s.height>0;
}
inline bool Same(Size a,Size b) noexcept {
    return Close(a.width,b.width)&&Close(a.height,b.height);
}
struct State {
    Size confirmed{};
    bool known{};
    Action Observe(Rect clip,Size actual,Size viewport) noexcept {
        if(!Valid(actual)||!Valid(viewport)||!Same(actual,viewport)||
           !Close(clip.x,0)||!Close(clip.y,0)||!Valid({clip.width,clip.height})) return Action::Ignore;
        const Size size{clip.width,clip.height};
        if(Same(size,actual)) { confirmed=actual;known=true;return Action::Synchronized; }
        // Only a rectangle previously observed as the native viewport is ours
        // to reconcile. An unexplained/intentional clip is never expanded.
        return known&&Same(size,confirmed)?Action::RepairPreviousSize:Action::Ignore;
    }
};
} // namespace NativeSwitcherClip
