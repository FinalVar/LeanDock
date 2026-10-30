#include <windows.h>
#include <cstdio>
#define LEANDOCK_CLIENT_EDGE_TESTING
#include "../include/client-edge-latest.h"
static unsigned checks,failures;
static void Check(bool condition,const char* label){++checks;if(!condition){++failures;std::printf("FAIL %s\n",label);}}
static unsigned initCalls;
static HRESULT Transient(unsigned){++initCalls;LeanDockClientEdge::lastStage="test-taskbar-ready";return initCalls<3?E_FAIL:S_OK;}
static HRESULT Denied(unsigned){++initCalls;return E_ACCESSDENIED;}
static HRESULT AlwaysFails(unsigned){++initCalls;return E_FAIL;}
static HRESULT StopsDuringInit(unsigned){++initCalls;LeanDockClientEdge::Stop();return S_OK;}
int main() {
    using namespace LeanDockClientEdgePolicy;
    const RECT monitor{0,0,2560,1600},exact=monitor,overscan{-11,-11,2571,1611},gap{0,0,2560,1598};
    auto eligible=[&](RECT r){return Eligible(true,false,true,WS_THICKFRAME,r,monitor);};
    Check(eligible(exact),"frameless native maximization qualifies");
    Check(eligible(overscan),"standard oversized maximized frame qualifies");
    Check(!eligible(gap),"a window that already exposes the edge is unchanged");
    Check(!Eligible(true,false,false,WS_THICKFRAME,exact,monitor),"manually screen-sized normal window is excluded");
    Check(!Eligible(true,false,true,0,exact,monitor),"non-resizable fullscreen excluded");
    Check(!Eligible(false,false,true,WS_THICKFRAME,exact,monitor),"hidden excluded");
    Check(!Eligible(true,true,true,WS_THICKFRAME,exact,monitor),"minimized excluded");
    for(RECT r: {RECT{1,0,2560,1600},RECT{0,1,2560,1600},RECT{0,0,2559,1600},RECT{0,0,2560,1599}})
        Check(!eligible(r),"any exposed monitor edge avoids unnecessary marking");
    Check(Eligible(true,false,true,WS_THICKFRAME,RECT{-1920,-1080,0,0},RECT{-1920,-1080,0,0}),"negative monitor coordinates");
    Check(!Eligible(true,false,true,WS_THICKFRAME,exact,RECT{0,0,0,0}),"invalid monitor rejected");
    Check(Identity(L"OpenAI.Codex_2p2nqsd0c76g0",L"C:\\Program Files\\WindowsApps\\OpenAI.Codex_26.1007.2314.0_x64__2p2nqsd0c76g0\\app\\ChatGPT.exe"),"verified package and executable");
    Check(Identity(L"openai.codex_2p2nqsd0c76g0",L"C:\\WindowsApps\\new-version\\APP\\CHATGPT.EXE"),"case insensitive and not version path pinned");
    Check(!Identity(L"Unrelated_2p2nqsd0c76g0",L"C:\\other\\app\\ChatGPT.exe"),"same executable name from unrelated package rejected");
    Check(!Identity(L"OpenAI.Codex_2p2nqsd0c76g0",L"C:\\other\\ChatGPT.exe"),"non-client package executable rejected");
    Check(!Identity(L"OpenAI.Codex_2p2nqsd0c76g0",L"C:\\other\\app\\ChatGPT.exe.bak"),"suffix extension rejected");
    Check(!Identity(nullptr,L"ChatGPT.exe")&&!Identity(L"OpenAI.Codex_2p2nqsd0c76g0",nullptr),"missing process identity rejected");
    Check(ResizeObstruction(true,HTBOTTOM,0,gap,monitor,144),"other applications need measured resize obstruction");
    for(auto hit:{HTBOTTOM,HTBOTTOMLEFT,HTBOTTOMRIGHT})Check(BottomResize(hit),"bottom sizing borders accepted");
    for(auto hit:{HTCLIENT,HTNOWHERE,HTTRANSPARENT,HTCAPTION,HTLEFT,HTRIGHT,HTTOP})Check(!BottomResize(hit),"ordinary client/top/side hits excluded");
    Check(!ResizeObstruction(false,HTBOTTOM,0,gap,monitor,144),"not owning the activation point is excluded");
    Check(!ResizeObstruction(true,HTCLIENT,0,gap,monitor,144),"visible gap alone cannot change other applications");
    Check(!ResizeObstruction(true,HTBOTTOM,WS_EX_TOPMOST,gap,monitor,144),"topmost fullscreen/overlay excluded");
    Check(!ResizeObstruction(true,HTBOTTOM,WS_EX_TOOLWINDOW,gap,monitor,144),"tool windows excluded");
    Check(!ResizeObstruction(true,HTBOTTOM,0,exact,monitor,144),"true full-monitor frame excluded");
    Check(!ResizeObstruction(true,HTBOTTOM,0,RECT{0,0,2560,1500},monitor,144),"large unrelated gap excluded");
    Check(!ResizeObstruction(true,HTBOTTOM,0,gap,monitor,0),"unknown DPI is not proof");
    Check(ResizeObstruction(true,HTBOTTOM,0,RECT{-1920,-1080,0,-2},RECT{-1920,-1080,0,0},96),"negative monitor border evidence");
    Check(BorderInset(0,RECT{0,0,2560,1576},monitor,144),"144 DPI boundary");
    Check(!BorderInset(0,RECT{0,0,2560,1575},monitor,144),"144 DPI excessive border");

    // Native property lifecycle tests affect only this hidden test-owned HWND.
    HWND w=CreateWindowExW(0,L"STATIC",L"LeanDock edge property fixture",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(w!=nullptr,"hidden owned window created");
    if(w) {
        HANDLE a=reinterpret_cast<HANDLE>(101),b=reinterpret_cast<HANDLE>(102);
        Check(!Acquire(w,nullptr),"no zero ownership token");
        Check(Acquire(w,a),"acquire only absent property");
        Check(GetPropW(w,NonRude)==reinterpret_cast<HANDLE>(1),"real shell property written");
        Check(!Acquire(w,a),"duplicate acquire does not claim another original value");
        Check(!Release(w,b)&&GetPropW(w,Owner)==a,"wrong owner cannot clean up");
        Check(Release(w,a)&&!GetPropW(w,Owner)&&!GetPropW(w,NonRude),"own properties removed");
        SetPropW(w,NonRude,reinterpret_cast<HANDLE>(7));
        Check(!Acquire(w,a)&&!GetPropW(w,Owner),"preexisting external property retained");
        Check(!Release(w,a)&&GetPropW(w,NonRude)==reinterpret_cast<HANDLE>(7),"foreign property never removed");
        RemovePropW(w,NonRude);Check(Acquire(w,a),"new lifetime acquires");
        SetPropW(w,NonRude,reinterpret_cast<HANDLE>(7));
        Check(!Release(w,a)&&GetPropW(w,NonRude)==reinterpret_cast<HANDLE>(7)&&!GetPropW(w,Owner),"external replacement survives release");
        RemovePropW(w,NonRude);Check(Acquire(w,a),"second lifetime acquires");
        SetPropW(w,Owner,b);Check(!Release(w,a)&&GetPropW(w,NonRude),"replacement owner survives stale cleanup");
        Check(Release(w,b),"new owner releases");
        Check(Acquire(w,a),"property present at destruction");
        // Exercise the actual finite retry orchestration with no desktop hook,
        // shell mutation, message pump, visible window or simulated input.
        using namespace LeanDockClientEdge;
        initializeForTest=Transient;initCalls=0;
        Check(!Start(w)&&retryTimer&&!active&&enabled,"initial readiness failure schedules recovery");
        Check(lastError==E_FAIL&&initAttempts==1,"failure stage and error retained");
        const auto timer=retryTimer;
        Retry(nullptr,WM_TIMER,timer,0);Check(initCalls==1&&retryTimer==timer,"early stale timer cannot retry");
        Sleep(LeanDockStart::AttachPolicy::DelayMs+10);Retry(nullptr,WM_TIMER,timer,0);
        Check(initCalls==2&&retryTimer&&!active,"second transient failure remains bounded");
        Sleep(LeanDockStart::AttachPolicy::DelayMs+10);Retry(nullptr,WM_TIMER,retryTimer,0);
        Check(initCalls==3&&active&&!retryTimer&&initPolicy.recoveries==1,"third attempt recovers without resident timer");
        Stop();Check(!active&&!enabled&&!retryTimer,"stop closes retry admission");
        Retry(nullptr,WM_TIMER,timer,0);Check(initCalls==3,"late retry after stop is inert");
        initializeForTest=Denied;initCalls=0;
        Check(!Start(w)&&!retryTimer&&initPolicy.stopped&&initCalls==1,"access refusal never repeatedly retried");
        Ensure(w);Check(initCalls==1,"native reconnect call respects terminal refusal");
        initializeForTest=AlwaysFails;initCalls=0;Start(w);
        for(unsigned i=1;i<3;++i){Sleep(LeanDockStart::AttachPolicy::DelayMs+10);Retry(nullptr,WM_TIMER,retryTimer,0);}
        Check(initCalls==3&&!active&&!retryTimer,"retry budget exhausts without polling");
        initializeForTest=StopsDuringInit;initCalls=0;Start(w);
        Check(initCalls==1&&!active&&!enabled&&!retryTimer,"reentrant stop cannot publish late success");
        initializeForTest=InitializeNative;
        Check(Start(w)&&active&&shell,"real public COM and event initialization succeeds when shell ready");
        Stop();Check(!shell&&!comOwned&&!retryTimer,"actual resources released without pumping desktop events");
        DestroyWindow(w);Check(!Release(w,a),"destroyed HWND cleanup is harmless");
    }
    LeanDockClientEdge::Stop();LeanDockClientEdge::Stop();
    Check(!LeanDockClientEdge::active&&!LeanDockClientEdge::shell,"stop before start is idempotent");
    Check(!LeanDockClientEdge::Dispatch(WM_USER),"unrelated messages not swallowed");
    Check(LeanDockClientEdge::Dispatch(LeanDockClientEdge::Recheck)&&!LeanDockClientEdge::active,"late queued message stays inactive after stop");
    std::printf("Client edge: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
