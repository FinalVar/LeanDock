// Ownership/lifecycle regression with hidden, non-activating fixture windows.
#include "../include/switcher-visibility-latest.h"
#include <cstdio>
using namespace LeanDockSwitcherVisibility;
static unsigned checks,failures;
static void Check(bool value,const char* text){++checks;if(!value){++failures;std::printf("FAIL: %s\n",text);}}
int main(){
    HWND first=CreateWindowExW(0,L"STATIC",L"LeanDock hidden marker test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND second=CreateWindowExW(0,L"STATIC",L"LeanDock hidden marker test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(first && second,"hidden fixture windows created");
    if(!first || !second) return 1;
    Check(!IsWindowVisible(first) && !IsWindowVisible(second),"fixture never shown");
    HWND foreground=GetForegroundWindow();
    {
        WindowMark older,newer;
        Check(older.Set(first) && GetPropW(first,WindowProperty),"loaded root marks its host");
        HANDLE original=GetPropW(first,WindowProperty);
        Check(older.Set(first) && GetPropW(first,WindowProperty)==original,"unchanged layout preserves owner");
        Check(newer.Set(first) && GetPropW(first,WindowProperty)!=original,"replacement root owns new mark");
        HANDLE replacement=GetPropW(first,WindowProperty);
        older.Clear();
        Check(GetPropW(first,WindowProperty)==replacement,"late old-root unload does not remove new root mark");
        Check(newer.Set(second),"root can move to replacement host");
        Check(!GetPropW(first,WindowProperty) && GetPropW(second,WindowProperty),"old host is unmarked on replacement");
        Check(ActiveWindow()!=first && ActiveWindow()!=second,"hidden or non-shell marked window is not active switcher");
        newer.Clear();newer.Clear();
        Check(!GetPropW(second,WindowProperty),"repeated unload is harmless");
        Check(!newer.Set(nullptr),"null host cannot acquire mark");
        Check(newer.Set(first),"reopen can acquire mark after unload");
    }
    Check(!GetPropW(first,WindowProperty),"root destruction clears owned mark");
    {
        WindowMark destroyed;Check(destroyed.Set(second),"mark before host destruction");
        Check(DestroyWindow(second)!=FALSE,"hidden host destroyed");second=nullptr;
        destroyed.Clear();
    }
    Check(GetForegroundWindow()==foreground,"fixture did not activate a window");
    DestroyWindow(first);
    std::printf("%u visibility checks, %u failures (no window shown or input sent)\n",checks,failures);
    return failures?1:0;
}
