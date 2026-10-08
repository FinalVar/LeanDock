#include "shell-actions-latest.h"
#include "shell-menu-latest.h"
#include <cstdio>
int main(){
    using namespace LeanDockShell;int count=0,failures=0;
    auto check=[&](bool condition){++count;if(!condition)++failures;};
    check(Key(Action::QuickSettings)=='A');check(Key(Action::PowerUser)=='X');
    check(!Key(static_cast<Action>(0)));check(!Key(static_cast<Action>(3)));
    for(auto action:{Action::QuickSettings,Action::PowerUser}){
        auto input=Input(action);
        check(input[0].ki.wVk==VK_LWIN);check(input[1].ki.wVk==Key(action));
        check(input[2].ki.wVk==Key(action));check(input[3].ki.wVk==VK_LWIN);
        for(unsigned i=0;i<4;++i){check(input[i].type==INPUT_KEYBOARD);
            check(bool(input[i].ki.dwFlags&KEYEVENTF_KEYUP)==(i>=2));
            check(bool(input[i].ki.dwFlags&KEYEVENTF_EXTENDEDKEY)==(i==0||i==3));}
    }
    check(!IsTaskbar(nullptr,GetCurrentProcessId()));
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
    std::printf("Shell actions: %d checks, %d failures (no input sent)\n",count,failures);
    return failures?1:0;
}
