#pragma once
#include <windows.h>
// A popup, not a taskbar receiver. The native XAML owner publishes the PID of
// its exact child; the child may publish only its own two window handles.
namespace LeanDockAggregate {
inline constexpr wchar_t PopupClass[]=L"LeanDock.Aggregate.Popup.v1";
inline constexpr wchar_t ControlClass[]=L"LeanDock.Aggregate.Control.v1";
inline constexpr wchar_t PidProp[]=L"LeanDock.Aggregate.PanelPid";
inline constexpr wchar_t WindowProp[]=L"LeanDock.Aggregate.PanelWindow";
inline constexpr wchar_t ControlProp[]=L"LeanDock.Aggregate.ControlWindow";
inline constexpr wchar_t CookieProp[]=L"LeanDock.Aggregate.Cookie";
inline constexpr wchar_t PendingProp[]=L"LeanDock.Aggregate.PendingUntil";
// The child starts warming up while the pointer rests on the key and stays
// hidden until the owner publishes this cookie at the real hover delay.
inline constexpr wchar_t RevealProp[]=L"LeanDock.Aggregate.Reveal";
inline constexpr UINT RevealMessage=WM_APP+29;
inline constexpr wchar_t BoundsProps[][36]={L"LeanDock.Aggregate.Left",L"LeanDock.Aggregate.Top",L"LeanDock.Aggregate.Right",L"LeanDock.Aggregate.Bottom"};
inline UINT RefreshMessage(){return RegisterWindowMessageW(L"Windhawk_UpdateTaskbarState_NativeTaskbarDock");}
inline void Refresh(HWND bar){if(bar)PostMessageW(bar,RefreshMessage(),0,0);}
inline bool Owned(HWND bar,HWND window,const wchar_t* expected){
    if(!bar||!window||!IsWindow(window))return false;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(!pid||pid!=DWORD(reinterpret_cast<uintptr_t>(GetPropW(bar,PidProp))))return false;
    wchar_t cls[96]{};GetClassNameW(window,cls,96);return !wcscmp(cls,expected);
}
inline bool Revealed(HWND bar,uintptr_t cookie){
    return cookie&&GetPropW(bar,CookieProp)==HANDLE(cookie)&&GetPropW(bar,RevealProp)==HANDLE(cookie);
}
inline bool Holding(HWND bar){
    if(!GetPropW(bar,CookieProp))return false;
    const auto pending=reinterpret_cast<uintptr_t>(GetPropW(bar,PendingProp));
    if(pending&&GetTickCount64()<pending)return true;
    auto popup=reinterpret_cast<HWND>(GetPropW(bar,WindowProp));
    return Owned(bar,popup,PopupClass)&&IsWindowVisible(popup);
}
inline bool Anchor(HWND bar,RECT& r){
    if(!GetPropW(bar,CookieProp))return false;
    r={int(reinterpret_cast<intptr_t>(GetPropW(bar,BoundsProps[0]))),int(reinterpret_cast<intptr_t>(GetPropW(bar,BoundsProps[1]))),
       int(reinterpret_cast<intptr_t>(GetPropW(bar,BoundsProps[2]))),int(reinterpret_cast<intptr_t>(GetPropW(bar,BoundsProps[3])))};
    return r.right>r.left&&r.bottom>r.top;
}
}
