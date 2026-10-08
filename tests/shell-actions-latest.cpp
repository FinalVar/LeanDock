#include "shell-actions-latest.h"
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
    std::printf("Shell actions: %d checks, %d failures (no input sent)\n",count,failures);
    return failures?1:0;
}
