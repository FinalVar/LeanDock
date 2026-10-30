#include "../include/aggregate-bridge-latest.h"
#include <iostream>
namespace a=LeanDockAggregate;
int main(){
    unsigned checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("aggregate owner/hold boundary");};
    WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=a::ControlClass;RegisterClassW(&cls);
    HWND owner=CreateWindowW(L"STATIC",L"hidden owner",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);
    HWND panel=CreateWindowW(cls.lpszClassName,L"hidden child",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);
    try{
        check(owner&&panel);check(!a::Holding(owner));
        SetPropW(owner,a::PendingProp,HANDLE(GetTickCount64()+1000));check(!a::Holding(owner));
        SetPropW(owner,a::CookieProp,HANDLE(123));check(a::Holding(owner));
        SetPropW(owner,a::PendingProp,HANDLE(GetTickCount64()-1));check(!a::Holding(owner));
        SetPropW(owner,a::PidProp,HANDLE(uintptr_t(GetCurrentProcessId()+1)));check(!a::Owned(owner,panel,a::ControlClass));
        SetPropW(owner,a::PidProp,HANDLE(uintptr_t(GetCurrentProcessId())));check(a::Owned(owner,panel,a::ControlClass));
        check(!a::Owned(owner,panel,a::PopupClass));
        LONG values[]={-1900,1200,-1792,1269};for(unsigned i=0;i<4;++i)SetPropW(owner,a::BoundsProps[i],HANDLE(intptr_t(values[i])));
        RECT rect{};check(a::Anchor(owner,rect)&&rect.left==-1900&&rect.right==-1792); // secondary monitor negative coordinate
        RemovePropW(owner,a::CookieProp);check(!a::Anchor(owner,rect)&&!a::Holding(owner));
        check(!IsWindowVisible(owner)&&!IsWindowVisible(panel));DestroyWindow(panel);panel=nullptr;check(!a::Owned(owner,panel,a::ControlClass));
        DestroyWindow(owner);std::cout<<checks<<" aggregate ownership/expiry/hidden lifecycle checks passed\n";return 0;
    }catch(...){if(panel)DestroyWindow(panel);DestroyWindow(owner);return 1;}
}
