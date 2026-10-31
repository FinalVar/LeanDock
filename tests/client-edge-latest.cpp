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
    {
        // Fullscreen applications other than games and remote clients keep the auto-hide Dock.
        using namespace LeanDockFullscreen;
        Check(Covers(WS_POPUP,exact,monitor)&&Covers(WS_POPUP|WS_THICKFRAME,overscan,monitor),"captionless full-monitor frames are fullscreen");
        Check(!Covers(WS_OVERLAPPEDWINDOW,exact,monitor),"a captioned window is never fullscreen");
        Check(!Covers(WS_POPUP,gap,monitor)&&!Covers(WS_POPUP,exact,RECT{0,0,0,0}),"exposed edge or invalid monitor is not fullscreen");
        Check(Quiet(L"D:\\SteamLibrary\\steamapps\\common\\Hades\\Hades.exe",L"",false),"Steam library is a game");
        Check(Quiet(L"C:\\Program Files\\Epic Games\\Fortnite\\FortniteClient.exe",L"",false),"Epic library is a game");
        Check(Quiet(L"C:\\XboxGames\\Forza\\Content\\forza.exe",L"",false),"Xbox app library is a game");
        Check(Quiet(L"C:\\STEAM\\STEAMAPPS\\COMMON\\x\\y.exe",L"",false),"library match ignores case");
        Check(Quiet(L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe",L"",true),"exclusive Direct3D fullscreen is a game");
        Check(Quiet(nullptr,L"",false),"an unidentified process counts as a game");
        Check(!Quiet(L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe",L"",false),"a player is not a game");
        Check(!Quiet(L"C:\\Program Files\\WindowsApps\\Microsoft.WindowsTerminal_1.23.0_x64__8wekyb3d8bbwe\\WindowsTerminal.exe",L"",false),"Windows Terminal is not a game");
        Check(!Quiet(L"C:\\Program Files\\WindowsApps\\OpenAI.Codex_26.1007.2314.0_x64__2p2nqsd0c76g0\\app\\ChatGPT.exe",L"",false),"ChatGPT is not a game");
        Check(!Quiet(L"C:\\Games\\notes\\epic games.exe",L"",false),"a library name must be a folder");
        Check(Quiet(L"D:\\Games\\osu!\\osu!.exe",L"vlc.exe; osu!.exe",false),"listed file name is a game");
        Check(!Quiet(L"D:\\Games\\osu!\\osu!.exe",L"su!.exe;osu",false),"partial names do not match");
        Check(Listed(L"C:\\a\\Game.EXE",L" game.exe ")&&Listed(L"game.exe",L";;game.exe;"),"entries trimmed, case-insensitive, empty entries skipped");
        Check(!Listed(L"C:\\a\\game.exe",L"")&&!Listed(L"C:\\a\\game.exe",L" ; ")&&!Listed(L"C:\\a\\",L";"),"empty list or name matches nothing");
        // Remote-control clients pass the screen edges to the remote computer.
        Check(Quiet(L"C:\\Windows\\System32\\mstsc.exe",L"",false),"Remote Desktop is quiet");
        Check(Quiet(L"C:\\Program Files\\Netease\\GameViewer\\bin\\GameViewer.exe",L"",false),"UU Remote is quiet");
        Check(Quiet(L"C:\\Program Files\\ToDesk\\TODESK.EXE",L"",false)&&Quiet(L"C:\\x\\rustdesk.exe",L"",false),"other remote clients, any case");
        Check(!Quiet(L"C:\\Program Files\\DAUM\\PotPlayer\\PotPlayerMini64.exe",L"",false),"PotPlayer is not quiet");
        // The Dock is wanted over a fullscreen application only on demand.
        const RECT hidden{0,1598,2560,1694},out{0,1504,2560,1600},sliding{0,1580,2560,1676},strip{940,0,1620,96};
        Check(!Wanted(hidden,strip,POINT{1200,800},monitor),"hidden Dock, pointer elsewhere: not wanted");
        Check(Wanted(hidden,strip,POINT{1200,1599},monitor)&&Wanted(hidden,strip,POINT{940,1598},monitor),"pointer on the hidden strip");
        Check(!Wanted(hidden,strip,POINT{300,1599},monitor)&&!Wanted(hidden,strip,POINT{1620,1599},monitor),"bottom edge outside the Dock's region");
        Check(!Wanted(hidden,strip,POINT{1200,1597},monitor),"just above the strip");
        Check(Wanted(out,strip,POINT{100,100},monitor)&&Wanted(sliding,strip,POINT{100,100},monitor),"Dock out or sliding stays wanted");
        Check(Wanted(hidden,RECT{0,0,2560,96},POINT{10,1599},monitor),"no region: the whole window");
        Check(!Wanted(RECT{0,0,0,0},strip,POINT{1200,1599},monitor)&&!Wanted(hidden,strip,POINT{1200,1599},RECT{0,0,0,0}),"empty geometry is not wanted");
    }

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
        {
            // Hand-back after the Dock hides, release, and takeover of a mark
            // an earlier controller left behind (a hidden test window only).
            HWND t=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,10,10,nullptr,nullptr,nullptr,nullptr);
            Check(t&&Acquire(t,Token()),"revealed fullscreen window owned");
            OwnedWindow& e=owned[0];e={t,Pid(t),false,true,false};
            const unsigned holds=fullscreenHolds;Hold(e);
            Check(e.held&&GetPropW(t,Held)==Token()&&!GetPropW(t,NonRude)&&!GetPropW(t,Owner)&&fullscreenHolds==holds+1,"hand-back swaps NonRudeHWND for the held mark");
            Check(Owns(e)&&Tracked(t)&&!Revealed(t),"held window stays owned but not revealed");
            Forget(e);Check(!e.window&&!GetPropW(t,Held),"release clears the held mark");
            SetPropW(t,Held,reinterpret_cast<HANDLE>(0x1234));Adopt(t,true);
            Check(Tracked(t)&&GetPropW(t,Held)==Token()&&!Revealed(t),"a fullscreen window's stale mark is taken over");
            for(auto& x:owned)if(x.window==t)Forget(x);
            SetPropW(t,Held,reinterpret_cast<HANDLE>(0x1234));Adopt(t,false);
            Check(!Tracked(t)&&!GetPropW(t,Held),"a stale mark on a window no longer fullscreen is cleared");
            SetPropW(t,Held,Token());Adopt(t,false);
            Check(!Tracked(t)&&GetPropW(t,Held)==Token(),"own mark is not treated as stale");
            OwnedWindow reused{t,Pid(t)+1,false,true,true};Forget(reused);
            Check(GetPropW(t,Held)==Token(),"a reused handle is never cleaned up");
            RemovePropW(t,Held);DestroyWindow(t);
        }
        Stop();Check(!shell&&!comOwned&&!retryTimer,"actual resources released without pumping desktop events");
        DestroyWindow(w);Check(!Release(w,a),"destroyed HWND cleanup is harmless");
    }
    LeanDockClientEdge::Stop();LeanDockClientEdge::Stop();
    Check(!LeanDockClientEdge::active&&!LeanDockClientEdge::shell,"stop before start is idempotent");
    Check(!LeanDockClientEdge::Dispatch(WM_USER),"unrelated messages not swallowed");
    Check(LeanDockClientEdge::Dispatch(LeanDockClientEdge::Recheck)&&!LeanDockClientEdge::active,"late queued message stays inactive after stop");
    std::printf("Client edge: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
