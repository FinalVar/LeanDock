#pragma once
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <vector>

namespace LeanDockCards {
// InputSite WndProc discovery/detour adapted from m417z's Taskbar Volume
// Control 1.3.1 (GPL-3.0). Full source, commit and local differences: NOTICE.md.
// Never subclass InputSite: it checks the identity of its window procedure.
class CardInputRoute {
public:
    using HitTest=bool(*)(void*,HWND,POINT) noexcept;
    using Wheel=void(*)(void*,int,bool) noexcept;
    using Contact=bool(*)(void*,HWND,UINT32) noexcept;
    using Install=bool(*)(WNDPROC,WNDPROC,WNDPROC*) noexcept;
    struct Packet {
        HWND window{};UINT message{};WPARAM wp{};LPARAM lp{};
        POINT point{};POINTER_INPUT_TYPE type{};bool control=false,pointValid=false;
    };
private:
    static auto& Routes(){static thread_local std::vector<CardInputRoute*> routes;return routes;}
    inline static WNDPROC original{};
    void* context{};HitTest hit{};Wheel wheel{};Contact contact{};
    DWORD thread{};HWND inputWindow{};
    bool attached=false;
    static bool IsWheel(UINT m){return m==WM_MOUSEWHEEL||m==WM_MOUSEHWHEEL||m==WM_POINTERWHEEL||m==WM_POINTERHWHEEL;}
    enum class Action {Pass,Contact,Wheel};
    Action Classify(Packet const& p) noexcept {
        if(!attached||!inputWindow||p.window!=inputWindow)return Action::Pass;
        if(p.message==DM_POINTERHITTEST){
            // DM_POINTERHITTEST parameters are not a portable pointer contract.
            // The pinned InputHost build was observed to provide this ID. An
            // unknown/invalid type keeps all native behavior unchanged.
            if(p.type!=PT_TOUCHPAD||p.control||!p.pointValid||!hit(context,p.window,p.point))return Action::Pass;
            return contact?Action::Contact:Action::Pass;
        }
        if(IsWheel(p.message)){
            if(p.control||!p.pointValid||!hit(context,p.window,p.point))return Action::Pass;
            return Action::Wheel;
        }
        return Action::Pass;
    }
    static LRESULT CALLBACK Procedure(HWND w,UINT message,WPARAM wp,LPARAM lp) noexcept {
        if(message==DM_POINTERHITTEST||IsWheel(message)){
            Packet p{w,message,wp,lp};
            p.control=(GetKeyState(VK_CONTROL)&0x8000)!=0;
            if(message==DM_POINTERHITTEST){
                GetPointerType(GET_POINTERID_WPARAM(wp),&p.type);p.pointValid=GetCursorPos(&p.point);
            }else if(IsWheel(message)){
                p.point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};p.pointValid=true;
                if(message==WM_MOUSEWHEEL||message==WM_MOUSEHWHEEL)p.control|=(GET_KEYSTATE_WPARAM(wp)&MK_CONTROL)!=0;
            }
            LRESULT result{};if(Dispatch(p,result))return result;
        }
        return original(w,message,wp,lp);
    }
public:
    CardInputRoute()=default;
    CardInputRoute(CardInputRoute const&)=delete;
    CardInputRoute& operator=(CardInputRoute const&)=delete;
    ~CardInputRoute(){Stop();}
    bool Start(void* owner,HitTest test,Wheel action,Contact begin=nullptr){
        if(attached)return true;
        if(!owner||!test||!action)return false;
        Routes().push_back(this);context=owner;hit=test;wheel=action;contact=begin;
        thread=GetCurrentThreadId();attached=true;return true;
    }
    bool Bind(HWND taskbar,Install install) noexcept {
        if(!attached||GetCurrentThreadId()!=thread||GetWindowThreadProcessId(taskbar,nullptr)!=thread)return false;
        if(Ready()&&GetAncestor(inputWindow,GA_ROOT)==taskbar)return true;
        inputWindow=nullptr;
        struct Search {HWND taskbar,found;DWORD thread;} search{taskbar,nullptr,thread};
        EnumChildWindows(taskbar,[](HWND child,LPARAM context)->BOOL{
            auto& s=*reinterpret_cast<Search*>(context);wchar_t name[96]{};
            if(GetWindowThreadProcessId(child,nullptr)!=s.thread)return TRUE;
            GetClassNameW(child,name,96);if(wcscmp(name,L"Windows.UI.Input.InputSite.WindowClass"))return TRUE;
            auto bridge=GetParent(child);GetClassNameW(bridge,name,96);
            if(wcscmp(name,L"Windows.UI.Composition.DesktopWindowContentBridge")||GetParent(bridge)!=s.taskbar)return TRUE;
            s.found=child;return FALSE;
        },reinterpret_cast<LPARAM>(&search));
        if(!search.found)return false;
        auto candidate=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(search.found,GWLP_WNDPROC));
        if(!candidate||!install||!install(candidate,Procedure,&original))return false;
        inputWindow=search.found;return true;
    }
    // Hidden-window fixtures use explicitly decoded packets, not invented
    // WM_POINTER handles or claimed hardware acceptance.
    static bool Dispatch(Packet const& p,LRESULT& result) noexcept {
        for(auto* route:Routes()){
            const auto action=route->Classify(p);
            if(action==Action::Pass)continue;
            // Copy callbacks before invoking: Render/Stop may retire the Board.
            auto owner=route->context;auto begin=route->contact;auto scroll=route->wheel;
            if(action==Action::Wheel){
                const bool horizontal=p.message==WM_MOUSEHWHEEL||p.message==WM_POINTERHWHEEL;
                scroll(owner,GET_WHEEL_DELTA_WPARAM(p.wp),horizontal);result=0;
            }else{
                if(!begin(owner,p.window,GET_POINTERID_WPARAM(p.wp)))return false;
                result=0;
            }
            return true;
        }
        return false;
    }
    void Stop() noexcept {
        if(!attached||GetCurrentThreadId()!=thread)return;
        attached=false;inputWindow=nullptr;std::erase(Routes(),this);
        // The shared runtime owns the detour/trampoline. Empty routes pass to
        // original; it stays mapped through shutdown and late callbacks.
    }
    bool Ready()const noexcept{return attached&&inputWindow&&IsWindow(inputWindow);}
    HWND Window()const noexcept{return inputWindow;}
};
}
