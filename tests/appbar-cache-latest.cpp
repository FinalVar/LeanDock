#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <array>
#include <atomic>
#include <mutex>
#include <iterator>
#include "../include/appbar-query-policy-latest.h"
#include "../include/dock-geometry-latest.h"
#include "../include/protocol-latest.h"
#include "../include/verify-latest.h"
#define Wh_Log(...) ((void)0)
namespace WindhawkUtils {template<class T>bool SetFunctionHook(T,T,T*){return false;}}
static HWND testTray;
static std::atomic<bool> g_eventStopping{true};
static bool g_wasAutoHideProcessed;
static bool IsTaskbarWindow(HWND window){return window && window==testTray;}
// Compile the production compatibility functions. Only hook registration is
// replaced; no live process is hooked and no input desktop is ever switched.
#include "../src/hide-appbar-latest.inc"
static unsigned checks,failures,nativeCalls;
static void Check(bool value,const char* text){++checks;if(!value){++failures;printf("FAIL: %s\n",text);}}
static LRESULT WINAPI Original(void*,const COPYDATASTRUCT* copy){
    ++nativeCalls;LeanDockAppBar::WireRequest data{};
    if(!LeanDockAppBar::ReadRequest(copy,data))return 0;
    if(data.message==ABM_GETSTATE)return ABS_AUTOHIDE;
    return data.edge==ABE_BOTTOM?reinterpret_cast<LRESULT>(testTray):0;
}
static LRESULT CALLBACK Probe(HWND window,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_COPYDATA)return AppBarMessage_Hook(nullptr,reinterpret_cast<const COPYDATASTRUCT*>(l));
    return DefWindowProcW(window,msg,w,l);
}
static DWORD WINAPI Run(void*){
    const HDESK previous=GetThreadDesktop(GetCurrentThreadId());
    wchar_t desktopName[80];swprintf_s(desktopName,L"LeanDockCacheTest-%lu",GetCurrentProcessId());
    HDESK isolated=CreateDesktopW(desktopName,nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if(!isolated || !SetThreadDesktop(isolated)){if(isolated)CloseDesktop(isolated);return 1;}
    const auto instance=GetModuleHandleW(nullptr);
    const wchar_t* names[]={L"Shell_TrayWnd",NativeControllerClass,NativeBackendClass};
    std::array<HWND,3> windows{};
    for(unsigned i=0;i<3;++i){WNDCLASSW wc{};wc.lpfnWndProc=Probe;wc.hInstance=instance;wc.lpszClassName=names[i];RegisterClassW(&wc);windows[i]=CreateWindowExW(0,names[i],L"",WS_POPUP,0,0,2560,96,nullptr,nullptr,instance,nullptr);}
    testTray=windows[0];const auto controller=windows[1];
    if(!testTray || !controller || !windows[2])return 2;
    Check(!IsWindowVisible(testTray)&&!IsWindowVisible(controller)&&!IsWindowVisible(windows[2]),"all test windows remain hidden");
    AppBarMessage_Original=Original;AppBarSetProp_Original=SetPropW;AppBarPublic_Original=SHAppBarMessage;
    g_appBarHookInstalled=true;g_wasAutoHideProcessed=true;g_eventStopping=false;
    LeanDockGeometry::Publish(testTray,0,200,96);
    SetPropW(controller,LeanDockAppBar::ControllerReady,reinterpret_cast<HANDLE>(uintptr_t(GetCurrentProcessId())));
    Check(AppBarReady()&&OwnedAppBar(testTray),"production readiness and ownership work on private desktop");
    auto monitor=MonitorFromWindow(testTray,MONITOR_DEFAULTTOPRIMARY);
    SetPropW(testTray,L"LastAutoHideBarStuckMonitor",reinterpret_cast<HANDLE>(monitor));
    wchar_t key[24];swprintf_s(key,L"WindowOnEdge:%08x:3",static_cast<unsigned>(reinterpret_cast<uintptr_t>(monitor)));
    APPBARDATA bar{sizeof(bar)};bar.uEdge=ABE_BOTTOM;MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(monitor,&mi);bar.rc=mi.rcMonitor;
    Check(LeanDockAppBar::BottomCacheKey(key),"observed monitor cache key recognized");
    for(auto invalid:{L"",L"WindowOnEdge:",L"WindowOnEdge:00010001:2",L"WindowOnEdge:00010001:33",L"WindowOnEdge:0001000Z:3",L"AnotherProperty"})Check(!LeanDockAppBar::BottomCacheKey(invalid),"other or malformed property preserved");
    Check(!LeanDockAppBar::BottomCacheKey(reinterpret_cast<LPCWSTR>(123)),"integer atom never dereferenced");
    SetPropW(testTray,key,reinterpret_cast<HANDLE>(testTray));
    Check(SHAppBarMessage(ABM_GETAUTOHIDEBAR,&bar)==reinterpret_cast<ULONG_PTR>(testTray),"unmodified real shell cache returns native taskbar");
    SyncAppBarCache();
    Check(GetPropW(testTray,key)==reinterpret_cast<HANDLE>(uintptr_t(1)),"production synchronization marks external cache empty");
    const auto before=nativeCalls;
    Check(SHAppBarMessage(ABM_GETAUTOHIDEBAR,&bar)==0 && SHAppBarMessage(ABM_GETAUTOHIDEBAREX,&bar)==0,"both public API cache paths return no reserved bottom edge");
    Check(nativeCalls==before,"cache result actually bypasses message handler");
    Check(SHAppBarMessage(ABM_GETSTATE,&bar)==0,"public state query filtered despite receiver PID matching Explorer");
    Check(AppBarPublic_Hook(ABM_GETSTATE,&bar)==ABS_AUTOHIDE,"Explorer internal state stays enabled");
    Check(AppBarPublic_Hook(ABM_GETAUTOHIDEBAREX,&bar)==reinterpret_cast<ULONG_PTR>(testTray),"Explorer internal edge ignores external cache");
    ULONG_PTR result=0;
    Check(LeanDockAppBar::NativeWireQuery(testTray,controller,ABM_GETAUTOHIDEBAREX,ABE_BOTTOM,mi.rcMonitor,result)&&result==reinterpret_cast<ULONG_PTR>(testTray),"controller uses original native registration");
    Check(!LeanDockAppBar::NativeWireQuery(testTray,controller,ABM_SETSTATE,ABE_BOTTOM,mi.rcMonitor,result),"internal helper refuses write operations");
    RemovePropW(controller,LeanDockAppBar::ControllerReady);SyncAppBarCache();
    Check(GetPropW(testTray,key)==reinterpret_cast<HANDLE>(testTray),"controller not ready restores native cache");
    Check(SHAppBarMessage(ABM_GETSTATE,&bar)==ABS_AUTOHIDE,"unready public state is native");
    SetPropW(controller,LeanDockAppBar::ControllerReady,reinterpret_cast<HANDLE>(uintptr_t(GetCurrentProcessId())));SyncAppBarCache();
    Check(AppBarSetProp_Hook(testTray,key,reinterpret_cast<HANDLE>(testTray))!=0 && GetPropW(testTray,key)==reinterpret_cast<HANDLE>(uintptr_t(1)),"later native cache rewrite remains adapted");
    Check(AppBarSetProp_Hook(testTray,key,reinterpret_cast<HANDLE>(windows[2]))!=0,"other appbar write succeeds unchanged");
    RestoreAppBarCache();Check(GetPropW(testTray,key)==reinterpret_cast<HANDLE>(windows[2]),"restore never overwrites later unowned appbar value");
    AppBarSetProp_Hook(testTray,key,reinterpret_cast<HANDLE>(testTray));
    g_eventStopping=true;RestoreAppBarCache();
    Check(GetPropW(testTray,key)==reinterpret_cast<HANDLE>(testTray),"normal unload restores last native cache value");
    Check(SHAppBarMessage(ABM_GETAUTOHIDEBAR,&bar)==reinterpret_cast<ULONG_PTR>(testTray),"public API works natively after stop");
    RemovePropW(testTray,key);RemovePropW(testTray,L"LastAutoHideBarStuckMonitor");
    for(auto window:windows)DestroyWindow(window);for(auto name:names)UnregisterClassW(name,instance);
    Check(SetThreadDesktop(previous)!=FALSE,"thread desktop restored");Check(CloseDesktop(isolated)!=FALSE,"private desktop released");return 0;
}
int main(){
    HWND foreground=GetForegroundWindow();HANDLE thread=CreateThread(nullptr,0,Run,nullptr,0,nullptr);Check(thread!=nullptr,"isolated worker created");
    if(thread){WaitForSingleObject(thread,15000);DWORD code=1;GetExitCodeThread(thread,&code);Check(code==0,"production cache integration completed");CloseHandle(thread);}
    Check(GetForegroundWindow()==foreground,"foreground unchanged");printf("AppBar cache: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
