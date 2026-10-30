#pragma once
#include <string_view>

namespace LeanDockContext {
enum class Target { Unknown, Background, Card, Native };
enum class Action { Pass, Suppress, WinX };

// Walk the actual hit/source ancestry. A card owns its child controls, while
// native controls outside the card retain their own menu. Frame boundaries
// are checked before Control: the frames themselves are native controls too.
struct Classifier {
    bool native{};
    Target Visit(std::wstring_view name,std::wstring_view type,bool control,
                 bool boundary,bool nativePowerMapped) {
        if(name==L"LeanDockPowerCard"||(name==L"ControlCenterButton"&&nativePowerMapped))return Target::Native;
        if(name==L"LeanDockCard")return Target::Card;
        if(boundary)return native?Target::Native:Target::Background;
        if(type==L"Taskbar.TaskbarBackground")return Target::Unknown;
        native=native||control||type==L"Taskbar.TaskListButton"||
            type==L"Taskbar.TaskListButtonPanel"||type==L"Taskbar.TaskListLabeledButtonPanel"||
            type==L"SystemTray.NotifyIconView"||type==L"SystemTray.IconView"||name==L"NotifyItemIcon";
        return Target::Unknown;
    }
};
inline Action Route(Target target,bool pointer=true,bool handled=false) {
    if(!pointer||handled)return Action::Pass;
    if(target==Target::Card)return Action::WinX;
    if(target==Target::Background)return Action::Suppress;
    return Action::Pass;
}
// A direct subscription on our own card must retain exactly the same native
// battery, keyboard and already-handled exclusions as the ancestor fallback.
inline Action RouteCard(bool nativePower,bool pointer,bool handled) {
    return Route(nativePower?Target::Native:Target::Card,pointer,handled);
}
}
