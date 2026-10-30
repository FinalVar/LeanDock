// Real DWM ownership/watchdog checks. Fixtures are never presented/activated.
// Reuse checks set WS_VISIBLE only while DWM-cloaked, then clear it before
// allowing native uncloak. No input, ShowWindow(SW_SHOW) or foreground call.
#include "../include/verify-latest.h"
#include "../include/switcher-confirm-policy-latest.h"
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <string>
static NativeSwitcherPolicy::Request request;
extern "C" unsigned long long Native_SwitcherPeek(){return request.Peek(GetTickCount64());}
extern "C" bool Native_SwitcherConsume(unsigned long long t){return request.Consume(t,GetTickCount64());}
BOOL InternalWh_SetStringValue(void*,PCWSTR,PCWSTR){return TRUE;}
BOOL InternalWh_SetFunctionHook(void*,void*,void*,void**){return FALSE;}
#include "../src/switcher-preshow-latest.inc"
static unsigned checks,failed;
static void Check(bool ok,const char* text){++checks;if(!ok){++failed;printf("FAIL: %s\n",text);}}
static bool Cloaked(HWND w){DWORD state=0;DwmGetWindowAttribute(w,DWMWA_CLOAKED,&state,sizeof(state));return state&DWM_CLOAKED_APP;}
static unsigned actualUncloaks{};
static bool failCloak{};
static HRESULT WINAPI TrackDwm(HWND w,DWORD attribute,LPCVOID value,DWORD size){
    if(attribute==DWMWA_CLOAK&&value&&size==sizeof(BOOL)){
        if(!*static_cast<const BOOL*>(value))++actualUncloaks;
        else if(failCloak)return E_FAIL;
    }
    return DwmSetWindowAttribute(w,attribute,value,size);
}
static void NativeUncloak(HWND w){
    ++NativeSwitcherPreshow::displayDepth;BOOL no=FALSE;
    NativeSwitcherPreshow::DwmHook(w,DWMWA_CLOAK,&no,sizeof(no));
    --NativeSwitcherPreshow::displayDepth;
}
int main(){
    using namespace NativeSwitcherPreshow;
    HWND before=GetForegroundWindow();
    WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"XamlExplorerHostIslandWindow";
    Check(RegisterClassW(&cls)!=0,"fixture class registered in test process only");
    HWND w=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,L"Hidden gate fixture",WS_POPUP,-32000,-32000,8,8,nullptr,nullptr,cls.hInstance,nullptr);
    HWND unrelated=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"",WS_POPUP,-32000,-32000,8,8,nullptr,nullptr,cls.hInstance,nullptr);
    Check(w&&unrelated,"hidden fixtures exist");if(!w||!unrelated)return 1;
    supported=true;Start();showOriginal=ShowWindow;dwmOriginal=TrackDwm;
    Begin(w);Check(!Cloaked(w)&&!window,"manual invocation has no gate");
    auto t=request.Arm(GetTickCount64());Begin(unrelated);Check(!window,"unrelated window class bypassed");
    Begin(w);Check(window==w&&ticket==t&&Cloaked(w),"pre-show cloak acquired");
    BOOL no=FALSE;DwmHook(w,DWMWA_CLOAK,&no,sizeof(no));Check(Cloaked(w)&&uncloakDeferred==1,"native uncloak deferred while request held");
    DwmHook(unrelated,DWMWA_CLOAK,&no,sizeof(no));Check(!Cloaked(unrelated),"unrelated native calls pass through");
    Reveal(t+1);Check(Cloaked(w),"stale request cannot reveal new gate");
    Reveal(t);Check(!Cloaked(w)&&!GetPropW(w,kProperty)&&!window,"non-two/error release restores original host");
    t=request.Arm(GetTickCount64());Begin(w);ShowHook(w,SW_HIDE);Check(!Cloaked(w)&&!window,"native hide releases without showing fixture");
    t=request.Arm(GetTickCount64());Begin(w);Sleep(350);Check(!Cloaked(w)&&!window&&!Native_SwitcherPeek()&&timedOut==1,"watchdog restores host and cancels late confirmation");
    BOOL yes=TRUE;dwmOriginal(w,DWMWA_CLOAK,&yes,sizeof(yes));request.Arm(GetTickCount64());Begin(w);
    Check(!window&&Cloaked(w),"preexisting application cloak is not claimed");dwmOriginal(w,DWMWA_CLOAK,&no,sizeof(no));
    request.Cancel();dwmOriginal(w,DWMWA_CLOAK,&yes,sizeof(yes));NativeUncloak(w);
    Check(!Cloaked(w)&&!window,"manual native display without corner ticket remains visible");
    request.Arm(GetTickCount64());dwmOriginal(w,DWMWA_CLOAK,&yes,sizeof(yes));DwmHook(w,DWMWA_CLOAK,&no,sizeof(no));
    Check(!Cloaked(w)&&!window,"uncloak outside exact display scope is not acquired");
    auto reuseBefore=fromUncloak;
    for(unsigned i=0;i<8;++i){
        dwmOriginal(w,DWMWA_CLOAK,&yes,sizeof(yes));
        SetWindowLongPtrW(w,GWL_STYLE,WS_POPUP|WS_VISIBLE);
        t=request.Arm(GetTickCount64());Begin(w);
        Check(IsWindowVisible(w)&&Cloaked(w)&&!window,"visible-style reused host not claimed by Show alone");
        const auto uncloaks=actualUncloaks;NativeUncloak(w);
        Check(window==w&&ticket==t&&Cloaked(w)&&actualUncloaks==uncloaks,"native uncloak establishes gate on reused host without exposure");
        DwmHook(w,DWMWA_CLOAK,&yes,sizeof(yes));
        Check(!window&&!GetPropW(w,kProperty)&&Cloaked(w)&&actualUncloaks==uncloaks&&!Native_SwitcherPeek(),"native close retains cloak and clears owned request");
        Reveal(t);Timeout(nullptr,nullptr,nullptr);
        Check(Cloaked(w)&&actualUncloaks==uncloaks,"late reveal and timer cannot uncloak dismissed host");
        SetWindowLongPtrW(w,GWL_STYLE,WS_POPUP);
    }
    Check(fromUncloak-reuseBefore==8&&nativeClosed==8,"every repeated native display acquires and releases gate");
    Stop();Check(Cloaked(w)&&!watchdog,"stop preserves native close cloak");Start();
    t=request.Arm(GetTickCount64());NativeUncloak(w);auto newer=request.Arm(GetTickCount64());
    auto uncloaks=actualUncloaks;NativeUncloak(w);
    Check(ticket==newer&&Cloaked(w)&&actualUncloaks==uncloaks,"new ticket inherits owned cloak without an exposure gap");
    Reveal(t);Timeout(nullptr,nullptr,nullptr);
    Check(ticket==newer&&Cloaked(w),"old callback observes newer deadline and cannot release new request");
    Reveal(newer);Check(!Cloaked(w)&&!window,"non-two fallback reveals reused host");
    dwmOriginal(w,DWMWA_CLOAK,&yes,sizeof(yes));t=request.Arm(GetTickCount64());NativeUncloak(w);
    failCloak=true;auto failedClose=DwmHook(w,DWMWA_CLOAK,&yes,sizeof(yes));failCloak=false;
    Check(FAILED(failedClose)&&window==w&&Cloaked(w),"failed native close keeps watchdog and ownership intact");
    Reveal(t);Check(!Cloaked(w)&&!window,"failed native close still permits safe fallback");
    dwmOriginal(w,DWMWA_CLOAK,&yes,sizeof(yes));t=request.Arm(GetTickCount64());NativeUncloak(w);Sleep(350);
    Check(!Cloaked(w)&&!window&&!Native_SwitcherPeek()&&timedOut==2,"reused host watchdog reveals and cancels stale confirmation");
    t=request.Arm(GetTickCount64());Begin(w);Stop();Check(!Cloaked(w)&&!window&&!watchdog&&!enabled,"stop restores property and releases timer");
    Start();t=request.Arm(GetTickCount64());Begin(w);Check(Cloaked(w)&&watchdog,"resume recreates timer and gate");
    DestroyWindow(w);Reveal(t);Check(!window,"destroyed HWND safe for late release");
    Stop();Stop();Check(!watchdog,"repeated stop harmless");
    Check(!IsWindowVisible(unrelated)&&GetForegroundWindow()==before,"no fixture shown and foreground unchanged");
    DestroyWindow(unrelated);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    printf("%u pre-show checks, %u failures; no desktop input or visible window\n",checks,failed);return failed?1:0;
}
