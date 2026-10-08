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
    using LeanDockMenu::Position;
    for(int dpi:{96,144,192}){
        const RECT screen{0,0,2560,1600},dock{600,1504,1960,1600};
        auto middle=Position({1500,1560},dock,screen,360,900,dpi);
        check(middle.x==1320);check(middle.y+900==dock.top-MulDiv(8,dpi,96));
        auto left=Position({0,1560},dock,screen,360,900,dpi);check(left.x==0);
        auto right=Position({2559,1560},dock,screen,360,900,dpi);check(right.x==2200);
        auto tall=Position({1500,1560},dock,screen,360,1590,dpi);check(tall.y==0);
        const RECT negative{-1920,-200,0,880},negativeDock{-1600,784,-320,880};
        auto other=Position({-1919,800},negativeDock,negative,360,700,dpi);
        check(other.x==-1920);check(other.y+700==negativeDock.top-MulDiv(8,dpi,96));
        check(!LeanDockMenu::Popup(nullptr,GetCurrentProcessId()));
    }
    std::printf("Shell actions: %d checks, %d failures (no input sent)\n",count,failures);
    return failures?1:0;
}
