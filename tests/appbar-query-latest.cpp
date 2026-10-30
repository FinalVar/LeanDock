#include "../include/appbar-query-policy-latest.h"
#include <cstdio>
#include <initializer_list>
using namespace LeanDockAppBar;
static unsigned checks,failures;
static void Check(bool value,const char* text){++checks;if(!value){++failures;printf("FAIL: %s\n",text);}}
static bool livePolicy;
static unsigned nativeCalls;
static LRESULT CALLBACK ProbeProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg!=WM_COPYDATA)return DefWindowProcW(w,msg,wp,lp);
    WireRequest request{};
    const bool parsed=ReadRequest(reinterpret_cast<COPYDATASTRUCT*>(lp),request);
    ++nativeCalls;
    const ULONG_PTR raw=request.message==ABM_GETSTATE?ABS_AUTOHIDE|ABS_ALWAYSONTOP:0x123456;
    return parsed?Filter(request,raw,livePolicy,false,true,true):raw;
}
static DWORD WINAPI IsolatedWireTest(void*) {
    HDESK previous=GetThreadDesktop(GetCurrentThreadId());
    wchar_t name[80];swprintf_s(name,L"LeanDockAppBarTest-%lu",GetCurrentProcessId());
    HDESK isolated=CreateDesktopW(name,nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if(!isolated || !SetThreadDesktop(isolated)){if(isolated)CloseDesktop(isolated);return 1;}
    WNDCLASSW wc{};wc.lpfnWndProc=ProbeProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"Shell_TrayWnd";
    ATOM cls=RegisterClassW(&wc);
    HWND w=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,100,20,nullptr,nullptr,wc.hInstance,nullptr);
    if(!w || FindWindowW(L"Shell_TrayWnd",nullptr)!=w){if(w)DestroyWindow(w);if(cls)UnregisterClassW(wc.lpszClassName,wc.hInstance);SetThreadDesktop(previous);CloseDesktop(isolated);return 2;}
    Check(!IsWindowVisible(w),"test window always hidden; desktop never switched");
    for(bool enabled:{false,true,false}) {
        livePolicy=enabled;
        for(DWORD query:{ABM_GETSTATE,ABM_GETAUTOHIDEBAR,ABM_GETAUTOHIDEBAREX}) {
            APPBARDATA data{sizeof(data)};data.uEdge=ABE_BOTTOM;data.rc={0,0,2560,1600};
            const ULONG_PTR expected=query==ABM_GETSTATE?(enabled?ABS_ALWAYSONTOP:ABS_ALWAYSONTOP|ABS_AUTOHIDE):(enabled?0:0x123456);
            Check(SHAppBarMessage(query,&data)==expected,"real OS wire query filters and restores on private desktop");
        }
    }
    Check(nativeCalls==9,"every query reached original response path");
    DestroyWindow(w);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    Check(SetThreadDesktop(previous)!=FALSE,"original thread desktop restored");
    Check(CloseDesktop(isolated)!=FALSE,"private desktop released");return 0;
}
int main() {
    WireRequest r{40};r.message=ABM_GETSTATE;r.sharedProcess=100;r.edge=ABE_BOTTOM;
    COPYDATASTRUCT data{0,sizeof(r),&r};WireRequest parsed{};
    Check(ReadRequest(&data,parsed)&&parsed.sharedProcess==100&&parsed.message==4,"known native wire layout parsed");
    Check(!ReadRequest(nullptr,parsed),"missing copy data rejected");
    for(DWORD n:{0,28,40,44,60,63,65,128}){data.cbData=n;Check(!ReadRequest(&data,parsed),"unknown wire size unchanged");}data.cbData=64;
    data.dwData=1;Check(!ReadRequest(&data,parsed),"different IPC command not adapted");data.dwData=0;
    data.lpData=nullptr;Check(!ReadRequest(&data,parsed),"missing payload rejected");data.lpData=&r;
    r.size=48;Check(!ReadRequest(&data,parsed),"native APPBARDATA cannot be mistaken for wire data");r.size=40;
    r.sharedProcess=0;Check(!ReadRequest(&data,parsed),"unknown caller unchanged");r.sharedProcess=100;
    Check(Filter(r,3,false,false,true,true)==3,"unready compatibility leaves native response");
    Check(Filter(r,3,true,true,true,true)==3,"Explorer internal state preserved");
    Check(Filter(r,3,true,true,true,true)==3,"controller native state preserved");
    Check(Filter(r,3,false,false,true,true)==3,"missing adapter leaves native response");
    Check(Filter(r,3,true,false,false,true)==3,"unowned primary taskbar unchanged");
    Check(Filter(r,0x83,true,false,true,true)==0x82,"only autohide bit removed; other bits intact");
    for(auto query:{ABM_GETAUTOHIDEBAR,ABM_GETAUTOHIDEBAREX}) {
        r.message=query;Check(Filter(r,0x1234,true,false,true,true)==0,"owned bottom auto-hide registration hidden externally");
        Check(Filter(r,0x1234,true,false,true,false)==0x1234,"other appbar response retained");
        for(auto edge:{ABE_TOP,ABE_LEFT,ABE_RIGHT}){r.edge=edge;Check(Filter(r,0x1234,true,false,true,true)==0x1234,"other edge unchanged");}r.edge=ABE_BOTTOM;
    }
    for(auto query:{ABM_NEW,ABM_REMOVE,ABM_QUERYPOS,ABM_SETPOS,ABM_GETTASKBARPOS,ABM_ACTIVATE,ABM_SETAUTOHIDEBAR,ABM_WINDOWPOSCHANGED,ABM_SETSTATE,ABM_SETAUTOHIDEBAREX,99}){r.message=query;Check(Filter(r,0x1234,true,false,true,true)==0x1234,"mutating and unrelated operations unchanged");}
    constexpr LONG_PTR normal=WS_MAXIMIZE|WS_OVERLAPPEDWINDOW;
    RECT monitor{0,0,2560,1600};
    Check(FullMaximizedClient(normal,monitor,monitor),"ordinary full-client maximized window eligible");
    for(auto missing:{WS_MAXIMIZE,WS_THICKFRAME,WS_CAPTION,WS_MAXIMIZEBOX})Check(!FullMaximizedClient(normal&~missing,monitor,monitor),"fullscreen/restored style transition excluded");
    for(auto extra:{LONG_PTR(WS_CHILD),LONG_PTR(WS_POPUP)})Check(!FullMaximizedClient(normal|extra,monitor,monitor),"popup/child excluded");
    Check(!FullMaximizedClient(normal,RECT{0,0,2560,1599},monitor),"client with remaining gap not treated as full client");
    Check(!FullMaximizedClient(normal,RECT{-1,0,2560,1600},monitor),"off-monitor client not assumed compatible");
    RECT negative{-2560,0,0,1600};Check(FullMaximizedClient(normal,negative,negative),"negative monitor coordinates supported");
    HANDLE worker=CreateThread(nullptr,0,IsolatedWireTest,nullptr,0,nullptr);
    Check(worker!=nullptr,"private desktop test thread created");
    if(worker){WaitForSingleObject(worker,10000);DWORD code=1;GetExitCodeThread(worker,&code);Check(code==0,"isolated OS serialization integration passes");CloseHandle(worker);}
    printf("AppBar query: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
