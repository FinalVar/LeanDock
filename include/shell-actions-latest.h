#pragma once
#include <windows.h>
#include <array>
#include <cwchar>

namespace LeanDockShell {
enum class Action : unsigned { QuickSettings=1, PowerUser=2 };
constexpr WORD Key(Action action) {
    return action==Action::QuickSettings?'A':action==Action::PowerUser?'X':0;
}
inline std::array<INPUT,4> Input(Action action) {
    std::array<INPUT,4> input{};
    const WORD keys[]={VK_LWIN,Key(action),Key(action),VK_LWIN};
    for(unsigned i=0;i<4;++i){
        input[i].type=INPUT_KEYBOARD;input[i].ki.wVk=keys[i];
        input[i].ki.dwFlags=(i>=2?KEYEVENTF_KEYUP:0)|(keys[i]==VK_LWIN?KEYEVENTF_EXTENDEDKEY:0);
    }
    return input;
}
inline bool IsTaskbar(HWND window,DWORD explorer) {
    DWORD pid=0;wchar_t type[64]{};
    return window && GetWindowThreadProcessId(window,&pid) && pid==explorer &&
        GetClassNameW(window,type,64) &&
        (!wcscmp(type,L"Shell_TrayWnd")||!wcscmp(type,L"Shell_SecondaryTrayWnd"));
}
inline bool Invoke(Action action) {
    if(!Key(action))return false;
    for(int key:{VK_SHIFT,VK_CONTROL,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(key)&0x8000)return false;
    auto input=Input(action);const UINT sent=SendInput(4,input.data(),sizeof(INPUT));
    if(sent==4)return true;
    // Release only keys whose down transition could have been submitted.
    INPUT release[2]{};UINT count=0;
    if(sent>=2)release[count++]=input[2];
    if(sent>=1)release[count++]=input[3];
    if(count)SendInput(count,release,sizeof(INPUT));
    return false;
}
// Called only from the Explorer UI thread handling a genuine Dock gesture.
// No cross-process command interface and no elevated controller involvement.
inline bool Request(HWND taskbar,Action action) {
    if(!Key(action)||!IsTaskbar(taskbar,GetCurrentProcessId()))return false;
    const wchar_t* prop=L"LeanDock.Shell.LastRequest";
    const ULONG_PTR now=GetTickCount64();
    const ULONG_PTR previous=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,prop));
    if(previous&&now-previous<200)return true;
    DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    // A background right-click need not activate the taskbar by itself. The
    // process receiving this real user gesture may foreground its own window.
    if(foreground!=GetCurrentProcessId()&&!SetForegroundWindow(taskbar))return false;
    if(!Invoke(action))return false;
    SetPropW(taskbar,prop,reinterpret_cast<HANDLE>(now));
    const wchar_t* counter=action==Action::PowerUser?L"LeanDock.Shell.PowerUser":L"LeanDock.Shell.QuickSettings";
    const ULONG_PTR count=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,counter));
    SetPropW(taskbar,counter,reinterpret_cast<HANDLE>(count+1));return true;
}
}
