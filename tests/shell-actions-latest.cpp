#include "shell-actions-latest.h"
#include "shell-menu-latest.h"
#include <cstdio>
int main(){
    using namespace LeanDockShell;int count=0,failures=0;
    auto check=[&](bool condition){++count;if(!condition)++failures;};
    check(Key(Action::QuickSettings)=='A');check(Key(Action::PowerUser)=='X');
    check(Key(Action::Notifications)=='N');check(!Key(static_cast<Action>(0)));check(!Key(static_cast<Action>(5)));
    check(Key(Action::Start)==VK_LWIN);check(InputCount(Action::Start)==2);
    auto start=Input(Action::Start);
    check(start[0].ki.wVk==VK_LWIN&&start[1].ki.wVk==VK_LWIN);
    check(start[0].ki.dwFlags==KEYEVENTF_EXTENDEDKEY);
    check(start[1].ki.dwFlags==(KEYEVENTF_EXTENDEDKEY|KEYEVENTF_KEYUP));
    for(auto action:{Action::QuickSettings,Action::PowerUser,Action::Notifications}){
        auto input=Input(action);
        check(input[0].ki.wVk==VK_LWIN);check(input[1].ki.wVk==Key(action));
        check(input[2].ki.wVk==Key(action));check(input[3].ki.wVk==VK_LWIN);
        for(unsigned i=0;i<4;++i){check(input[i].type==INPUT_KEYBOARD);
            check(bool(input[i].ki.dwFlags&KEYEVENTF_KEYUP)==(i>=2));
            check(bool(input[i].ki.dwFlags&KEYEVENTF_EXTENDEDKEY)==(i==0||i==3));}
    }
    check(!IsTaskbar(nullptr,GetCurrentProcessId()));
    // One gesture can produce multiple context notifications. Coalesce only
    // the same action; opening Quick Settings must not consume an immediate
    // Power User request, nor may a stale/wrapped timestamp suppress input.
    check(Recent(Action::PowerUser,2,1199,1000));
    check(!Recent(Action::PowerUser,2,1200,1000));
    check(!Recent(Action::PowerUser,1,1100,1000));
    check(!Recent(Action::QuickSettings,2,1100,1000));
    check(Recent(Action::QuickSettings,1,1100,1000));
    check(!Recent(Action::PowerUser,2,999,1000));
    check(!Recent(Action::PowerUser,2,100,0));
    check(!Recent(Action::PowerUser,0,1100,1000));
    using LeanDockMenu::AnchorPoint;
    for(int dpi:{96,144,192}){
        const RECT screen{0,0,2560,1600},dock{600,1504,1960,1600};
        auto middle=AnchorPoint({1500,1560},dock,dpi);
        check(middle.x==1500);check(middle.y==dock.top-MulDiv(8,dpi,96));
        auto left=AnchorPoint({0,1560},dock,dpi);check(left.x==dock.left);
        auto right=AnchorPoint({2559,1560},dock,dpi);check(right.x==dock.right-1);
        auto edge=AnchorPoint({1959,1560},dock,dpi);check(edge.x==1959);
        const RECT negative{-1920,-200,0,880},negativeDock{-1600,784,-320,880};
        auto other=AnchorPoint({-1500,800},negativeDock,dpi);
        check(other.x==-1500);check(other.y==negativeDock.top-MulDiv(8,dpi,96));
        check(!LeanDockMenu::Popup(nullptr,GetCurrentProcessId()));
    }
    LeanDockMenu::Gesture gesture{};check(!LeanDockMenu::Fresh(gesture,1000));
    gesture.taskbar=reinterpret_cast<HWND>(1);gesture.at=1000;
    check(LeanDockMenu::Fresh(gesture,1000));check(LeanDockMenu::Fresh(gesture,2500));
    check(!LeanDockMenu::Fresh(gesture,2501));check(!LeanDockMenu::Fresh(gesture,999));
    const RECT dock{600,1504,1960,1600};check(AnchorPoint({1500,1560},dock,0).y==1496);
    // Keyboard menus follow the live card, even with the taskbar translated
    // below the monitor. Collapsed or wrong-taskbar cards keep native placement.
    for(int dpi:{96,144,192}){
        POINT point{};
        check(LeanDockMenu::CardAnchor({1500,1518,1656,1584},dock,dpi,point));
        check(point.x==1578&&point.y==1504-MulDiv(8,dpi,96));
        const POINT shown=point;
        check(LeanDockMenu::CardAnchor({1500,1700,1656,1766},dock,dpi,point));
        check(point.x==shown.x&&point.y==shown.y);
        check(LeanDockMenu::CardAnchor({-800,-500,-644,-434},{-1600,-550,-320,-400},dpi,point));
        check(point.x==-722&&point.y==-550-MulDiv(8,dpi,96));
    }
    for(auto card:{RECT{600,1,600,40},RECT{600,40,700,40},RECT{500,1,700,40},
                  RECT{1900,1,2000,40},RECT{-800,1,-600,40}}){
        POINT point{123,456};check(!LeanDockMenu::CardAnchor(card,dock,96,point));
        check(point.x==123&&point.y==456);
    }
    POINT point{};
    check(!LeanDockMenu::CardAnchor({600,1,700,40},{600,1,600,40},96,point));
    check(!LeanDockMenu::CardAnchor({600,1,700,40},{600,40,800,40},96,point));
    check(LeanDockMenu::CardAnchor({600,1,700,40},dock,0,point)&&point.y==1496);
    // Real taskbar template ancestry, including the labeled panel which the
    // old ButtonBase/TaskListButtonPanel-only classifier treated as background.
    using Target=LeanDockContext::Target;
    using ContextAction=LeanDockContext::Action;
    struct Node {std::wstring_view name,type;bool control{},boundary{},mapped{};};
    const Node frame{L"TaskbarFrame",L"Taskbar.TaskbarFrame",true,true};
    const Node tray{L"SystemTrayFrame",L"SystemTray.SystemTrayFrame",true,true};
    const Node root{L"RootGrid",L"Windows.UI.Xaml.Controls.Grid"};
    const Node icon{L"Icon",L"Windows.UI.Xaml.Controls.Image"};
    auto classify=[](std::initializer_list<Node> nodes){
        LeanDockContext::Classifier classifier;
        for(const auto& n:nodes){
            auto target=classifier.Visit(n.name,n.type,n.control,n.boundary,n.mapped);
            if(target!=Target::Unknown)return target;
        }
        return Target::Unknown;
    };
    for(auto panel:{L"Windows.UI.Xaml.Controls.Grid",L"Taskbar.TaskListButtonPanel",L"Taskbar.TaskListLabeledButtonPanel"}){
        auto target=classify({icon,{L"IconPanel",panel},{L"TaskListButton",L"Taskbar.TaskListButton",true},root,frame});
        check(target==Target::Native);check(LeanDockContext::Route(target)==ContextAction::Pass);
    }
    // The exact task-list type is protected even if its interface cast fails;
    // other native Controls remain protected without a class-name allowlist.
    check(classify({icon,{L"TaskListButton",L"Taskbar.TaskListButton"},root,frame})==Target::Native);
    check(classify({icon,{L"App",L"Taskbar.FutureAppControl",true},root,frame})==Target::Native);
    for(auto type:{L"SystemTray.NotifyIconView",L"SystemTray.IconView"}){
        auto target=classify({icon,{L"IconView",type,true},root,tray});
        check(target==Target::Native);check(LeanDockContext::Route(target)==ContextAction::Pass);
    }
    for(auto page:{L"ClockCard",L"MetricsCard",L"NetworkCard",L"MediaPrevious",L"MediaPlay",L"MediaNext"}){
        auto target=classify({icon,{page,L"Windows.UI.Xaml.Controls.Button",true},
            {L"LeanDockCard",L"Windows.UI.Xaml.Controls.Border"},root,frame});
        check(target==Target::Card);check(LeanDockContext::Route(target)==ContextAction::WinX);
    }
    for(bool mapped:{false,true}){
        auto target=classify({{L"ContainerGrid",L"Windows.UI.Xaml.Controls.Grid"},
            {L"IconView",L"SystemTray.IconView",true},
            {L"ControlCenterButton",L"SystemTray.OmniButton",true,false,mapped},root,tray});
        check(target==Target::Native);
        check(LeanDockContext::Route(target)==ContextAction::Pass);
    }
    check(classify({icon,{L"LeanDockPowerCard",L"Windows.UI.Xaml.Controls.Button",true},{L"LeanDockCard",L"Windows.UI.Xaml.Controls.Border"},root,frame})==Target::Native);
    // Frame and TaskbarBackground are Controls too, but own no app menu.
    for(auto boundary:{frame,tray}){
        check(classify({root,boundary})==Target::Background);
        check(classify({boundary})==Target::Background);
    }
    auto blank=classify({{L"BackgroundFill",L"Windows.UI.Xaml.Shapes.Rectangle"},
        {L"",L"Taskbar.TaskbarBackground",true},root,frame});
    check(blank==Target::Background);check(LeanDockContext::Route(blank)==ContextAction::Suppress);
    check(classify({icon,root})==Target::Unknown); // detached/popup ancestry
    for(auto target:{Target::Unknown,Target::Background,Target::Card,Target::Native}){
        check(LeanDockContext::Route(target,false)==ContextAction::Pass);
        check(LeanDockContext::Route(target,true,true)==ContextAction::Pass);
    }
    check(LeanDockContext::Route(Target::Unknown)==ContextAction::Pass);
    // Direct card ownership does not turn the battery or keyboard context
    // request back into Win+X. A handled child/ancestor event is never replayed.
    for(bool nativePower:{false,true})for(bool pointer:{false,true})for(bool handled:{false,true}){
        check(LeanDockContext::RouteCard(nativePower,pointer,handled)==
            (!nativePower&&pointer&&!handled?ContextAction::WinX:ContextAction::Pass));
    }
    // Real window properties used by queued menu reopen callbacks. An old,
    // repeated, or cancelled callback must never consume a newer request.
    HWND pending=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(pending!=nullptr);
    if(pending){
        check(!IsWindowVisible(pending));
        check(!LeanDockMenu::ClaimReopen(pending,0));
        check(!LeanDockMenu::ClaimReopen(pending,1));
        SetPropW(pending,L"LeanDock.Menu.ReopenPending",reinterpret_cast<HANDLE>(1));
        check(!LeanDockMenu::ClaimReopen(pending,2));
        SetPropW(pending,L"LeanDock.Menu.ReopenPending",reinterpret_cast<HANDLE>(2));
        check(!LeanDockMenu::ClaimReopen(pending,1));
        check(LeanDockMenu::ClaimReopen(pending,2));
        check(!LeanDockMenu::ClaimReopen(pending,2));
        SetPropW(pending,L"LeanDock.Menu.ReopenPending",reinterpret_cast<HANDLE>(3));
        RemovePropW(pending,L"LeanDock.Menu.ReopenPending");
        check(!LeanDockMenu::ClaimReopen(pending,3));
        SetPropW(pending,L"LeanDock.Menu.ReopenPending",reinterpret_cast<HANDLE>(4));
        SetPropW(pending,L"LeanDock.Menu.ReopenAt",reinterpret_cast<HANDLE>(100));
        check(LeanDockMenu::ReopenPending(pending,101));
        check(!LeanDockMenu::ReopenPending(pending,1601));
        check(!GetPropW(pending,L"LeanDock.Menu.ReopenPending")&&!GetPropW(pending,L"LeanDock.Menu.ReopenAt"));
        check(!LeanDockMenu::ClaimReopen(pending,4));
        SetPropW(pending,L"LeanDock.Menu.ReopenPending",reinterpret_cast<HANDLE>(5));
        check(!LeanDockMenu::ReopenPending(pending,1700));
        DestroyWindow(pending);
    }
    std::printf("Shell actions: %d checks, %d failures (no input sent)\n",count,failures);
    return failures?1:0;
}
