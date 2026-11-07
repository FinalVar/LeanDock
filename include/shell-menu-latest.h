#pragma once
#include "shell-actions-latest.h"
#include "dock-geometry-latest.h"
#include "shell-context-latest.h"
#include <mutex>
namespace LeanDockMenu {
struct Gesture {HWND taskbar{};POINT click{};ULONGLONG at{};};
extern std::mutex gestureMutex;
extern Gesture gesture;
inline bool Fresh(const Gesture& value,ULONGLONG now) {return value.taskbar&&now>=value.at&&now-value.at<=1500;}
inline bool Take(Gesture& result) {
    std::lock_guard lock(gestureMutex);result=gesture;gesture={};return Fresh(result,GetTickCount64());
}
inline void Put(HWND window,const wchar_t* name,ULONG_PTR value) {
    if(window)SetPropW(window,name,reinterpret_cast<HANDLE>(value+1));
}
inline POINT AnchorPoint(POINT click,const RECT& dock,int dpi) {
    return {std::clamp(click.x,dock.left,dock.right-1),LeanDockGeometry::PopupBottom(dock,dpi>0?dpi:96)};
}
inline bool CardAnchor(const RECT& card,const RECT& dock,int dpi,POINT& point) {
    // Vertical card coordinates can be offscreen during auto-hide. Use the
    // fully shown Dock for height, but require a real card inside its span.
    if(dock.right<=dock.left||dock.bottom<=dock.top||card.right<=card.left||
       card.bottom<=card.top||card.left<dock.left||card.right>dock.right)return false;
    point=AnchorPoint({card.left+(card.right-card.left)/2,0},dock,dpi);
    return true;
}
inline bool Popup(HWND window,DWORD process) {
    DWORD pid=0;wchar_t type[64]{};
    return window&&GetWindowThreadProcessId(window,&pid)&&pid==process&&IsWindowVisible(window)&&
        GetClassNameW(window,type,64)&&!wcscmp(type,L"Xaml_WindowedPopupClass");
}
inline void Cancel() {std::lock_guard lock(gestureMutex);gesture={};}
// The native invocation consumes this one-use gesture in its XAML menu entry.
// Do not move/subclass the resulting HWND: XAML owns placement and hit testing.
enum class Reopen { NotOpen, Pending, Failed };
Reopen ReopenVisible(const Gesture& next);
inline bool ClaimReopen(HWND taskbar,ULONG_PTR ticket) {
    if(!ticket||reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.ReopenPending"))!=ticket)return false;
    RemovePropW(taskbar,L"LeanDock.Menu.ReopenPending");RemovePropW(taskbar,L"LeanDock.Menu.ReopenAt");return true;
}
inline bool ReopenPending(HWND taskbar,ULONGLONG now) {
    const auto ticket=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.ReopenPending"));
    if(!ticket)return false;
    const auto at=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.ReopenAt"));
    if(at&&Fresh({taskbar,{},at},now))return true;
    ClaimReopen(taskbar,ticket);return false; // a dropped dispatcher cannot block future clicks
}
inline bool Submit(const Gesture& next,bool coalesce=true) {
    const HWND taskbar=next.taskbar;
    if(!Fresh(next,GetTickCount64())||!LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))return false;
    {std::lock_guard lock(gestureMutex);gesture=next;}
    for(auto key:{L"LeanDock.Menu.NativeAnchor",L"LeanDock.Menu.Error",L"LeanDock.Menu.FocusArmed",L"LeanDock.Menu.PointerFocus",L"LeanDock.Menu.FocusBefore",L"LeanDock.Menu.FocusAfter",L"LeanDock.Menu.FocusError",L"LeanDock.Menu.FocusOpened",L"LeanDock.Menu.FocusGot",L"LeanDock.Menu.FocusItemCount"})Put(taskbar,key,0);
    if(LeanDockShell::Request(taskbar,LeanDockShell::Action::PowerUser,coalesce))return true;
    Cancel();return false;
}
inline bool Request(HWND taskbar) {
    if(!LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))return false;
    Gesture next{taskbar,{},GetTickCount64()};if(!GetCursorPos(&next.click))return false;
    // Win+X toggles. A visible native menu must finish closing before this
    // gesture submits one opening, including when the pointer is on battery.
    switch(ReopenVisible(next)){
    case Reopen::Pending:return true;
    case Reopen::Failed:return false;
    default:break;
    }
    const auto previous=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Shell.LastRequest"));
    if(LeanDockShell::Recent(LeanDockShell::Action::PowerUser,
        reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Shell.LastAction")),GetTickCount64(),previous))return true;
    return Submit(next);
}
enum class ContextSource : unsigned { WindowMessage=1, XamlBackground=2, XamlCard=3, NativePower=4, DirectCard=5 };
// Ownership and successful command submission are separate. An owned context
// request must not fall through to a second native taskbar menu on failure.
inline bool Consume(HWND taskbar,ContextSource source) {
    Put(taskbar,L"LeanDock.Menu.ContextSource",static_cast<unsigned>(source));
    Put(taskbar,L"LeanDock.Menu.ContextCount",std::max<ULONG_PTR>(1,reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.ContextCount"))));
    Put(taskbar,L"LeanDock.Menu.ContextSubmitted",Request(taskbar));
    return true;
}
inline bool HandleContext(HWND taskbar,LeanDockContext::Target target,ContextSource source) {
    Put(taskbar,L"LeanDock.Menu.ContextTarget",static_cast<unsigned>(target));
    switch(LeanDockContext::Route(target)) {
    case LeanDockContext::Action::WinX:return Consume(taskbar,source);
    case LeanDockContext::Action::Suppress:
        Put(taskbar,L"LeanDock.Menu.BlankSuppressed",
            std::max<ULONG_PTR>(1,reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.BlankSuppressed"))));
        return true;
    default:return false;
    }
}
}
