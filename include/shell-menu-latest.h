#pragma once
#include "shell-actions-latest.h"
#include "dock-geometry-latest.h"
#include <vector>

// The 26H2 Power User menu is a windowed XAML popup. Keep its native
// invocation and contents, and move only the new popup belonging to this
// real Dock gesture. No idle polling or cross-process command endpoint.
namespace LeanDockMenu {
inline POINT Position(POINT click,const RECT& dock,const RECT& monitor,LONG width,LONG height,int dpi) {
    const LONG gap=MulDiv(8,dpi,96);
    return {std::clamp(click.x-width/2,monitor.left,std::max(monitor.left,monitor.right-width)),
            std::clamp(dock.top-gap-height,monitor.top,std::max(monitor.top,monitor.bottom-height))};
}
inline bool Popup(HWND window,DWORD process) {
    DWORD pid=0;wchar_t type[64]{};
    return window && GetWindowThreadProcessId(window,&pid) && pid==process &&
        IsWindowVisible(window) && GetClassNameW(window,type,64) && !wcscmp(type,L"Xaml_WindowedPopupClass");
}
struct Pending {
    HWND taskbar{},popup{};POINT click{};UINT_PTR timer{};ULONGLONG start{},stable{};
    RECT applied{};unsigned attempts{};std::vector<HWND> before;
};
extern thread_local Pending pending;
inline void Put(HWND window,const wchar_t* name,ULONG_PTR value) {
    if(window)SetPropW(window,name,reinterpret_cast<HANDLE>(value+1));
}
inline void Cancel() {
    if(pending.timer)KillTimer(nullptr,pending.timer);
    pending={};
}
inline void CALLBACK Tick(HWND,UINT,UINT_PTR timer,DWORD) noexcept {
    if(timer!=pending.timer)return;
    try {
        auto& p=pending;const auto now=GetTickCount64();
        if(now-p.start>1200 || !LeanDockShell::IsTaskbar(p.taskbar,GetCurrentProcessId())){Cancel();return;}
        if(!p.popup){
            std::vector<HWND> candidates;
            HWND window=nullptr;
            while((window=FindWindowExW(nullptr,window,L"Xaml_WindowedPopupClass",nullptr))){
                if(!Popup(window,GetCurrentProcessId()) || std::find(p.before.begin(),p.before.end(),window)!=p.before.end())continue;
                RECT rect{};if(!GetWindowRect(window,&rect))continue;
                const int dpi=GetDpiForWindow(window);
                if(rect.right-rect.left<MulDiv(120,dpi,96)||rect.bottom-rect.top<MulDiv(200,dpi,96))continue;
                candidates.push_back(window);
            }
            if(candidates.size()!=1)return;
            p.popup=candidates.front();Put(p.taskbar,L"LeanDock.Menu.Seen",1);
        }
        if(!Popup(p.popup,GetCurrentProcessId())){Cancel();return;}
        RECT rect{},dock{};MONITORINFO monitor{};
        if(!GetWindowRect(p.popup,&rect)||!LeanDockGeometry::Read(p.taskbar,dock,&monitor))return;
        const LONG width=rect.right-rect.left,height=rect.bottom-rect.top;
        if(width>monitor.rcMonitor.right-monitor.rcMonitor.left || height>monitor.rcMonitor.bottom-monitor.rcMonitor.top){Cancel();return;}
        const int dpi=GetDpiForWindow(p.taskbar);
        const auto target=Position(p.click,dock,monitor.rcMonitor,width,height,dpi?dpi:96);
        if(rect.left==target.x&&rect.top==target.y){
            Put(p.taskbar,L"LeanDock.Menu.Placed",1);
            if(!p.stable)p.stable=now;
            if(now-p.stable>=180)Cancel();
            return;
        }
        p.stable=0;if(p.attempts>=4){Put(p.taskbar,L"LeanDock.Menu.Placed",0);Cancel();return;}
        ++p.attempts;
        if(SetWindowPos(p.popup,nullptr,target.x,target.y,0,0,
            SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_ASYNCWINDOWPOS)){
            Put(p.taskbar,L"LeanDock.Menu.X",target.x);Put(p.taskbar,L"LeanDock.Menu.Y",target.y);
            Put(p.taskbar,L"LeanDock.Menu.Window",reinterpret_cast<ULONG_PTR>(p.popup));
        }else{Put(p.taskbar,L"LeanDock.Menu.Error",GetLastError());Cancel();}
    }catch(...){Cancel();}
}
inline bool Request(HWND taskbar) {
    if(!LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))return false;
    // Routed and window context notifications may describe the same gesture.
    if(pending.timer&&pending.taskbar==taskbar&&GetTickCount64()-pending.start<200)return true;
    Cancel();pending.taskbar=taskbar;pending.start=GetTickCount64();GetCursorPos(&pending.click);
    HWND window=nullptr;while((window=FindWindowExW(nullptr,window,L"Xaml_WindowedPopupClass",nullptr)))
        if(Popup(window,GetCurrentProcessId()))pending.before.push_back(window);
    for(auto key:{L"LeanDock.Menu.Seen",L"LeanDock.Menu.Placed",L"LeanDock.Menu.Error"})Put(taskbar,key,0);
    pending.timer=SetTimer(nullptr,0,30,Tick);
    // Even if positioning cannot be armed, preserve the usable native menu.
    if(!LeanDockShell::Request(taskbar,LeanDockShell::Action::PowerUser)){Cancel();return false;}
    return true;
}
}
