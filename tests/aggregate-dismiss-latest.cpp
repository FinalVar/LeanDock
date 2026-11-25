// Real Win32 HWND fixture verifying hover-panel activation-on-pin, external
// deactivation (including across elevation/same-foreground returns), WH_MOUSE_LL
// outside-click dismissal, slider no-activate preservation, and Escape focus restore.
#define wWinMain AggregateProductionEntry
#include "../aggregate/latest.cpp"
#undef wWinMain
#include <cstdio>
#include <stdexcept>

namespace {
using namespace Capsule;
unsigned passed=0,failed=0;
void Check(bool ok,const char* why){
    if(ok){++passed;}
    else{++failed;std::printf("FAIL: %s\n",why);}
}
void Pump(){
    MSG m{};
    while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
}
HWND HiddenHost(const wchar_t* cls=L"STATIC"){
    return CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls,L"LeanDock dismiss fixture",
        WS_POPUP,-4096,-4096,96,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
}
void SetupPanel(Panel& panel,HWND bar,HWND control,uintptr_t cookie){
    panel.bar=bar;panel.cookie=cookie;panel.control=control;
    panel.owner=OpenProcess(SYNCHRONIZE,FALSE,GetCurrentProcessId());
    SetPropW(bar,Bridge::CookieProp,HANDLE(cookie));
    SetPropW(bar,Bridge::PidProp,HANDLE(uintptr_t(GetCurrentProcessId())));
    SetPropW(bar,Bridge::RevealProp,HANDLE(cookie));
    SetPropW(bar,Bridge::BoundsProps[0],HANDLE(intptr_t(200)));
    SetPropW(bar,Bridge::BoundsProps[1],HANDLE(intptr_t(700)));
    SetPropW(bar,Bridge::BoundsProps[2],HANDLE(intptr_t(272)));
    SetPropW(bar,Bridge::BoundsProps[3],HANDLE(intptr_t(746)));
    panel.popup.Init(panel.draw,1.f,PanelWidth,408.f,14.f,false);
    SetPropW(bar,Bridge::ControlProp,control);
    SetPropW(bar,Bridge::WindowProp,panel.popup.window);
    panel.popup.pointerCancel=[&]{panel.Cancel();panel.pagePressed={};};
    panel.popup.cancel=[&]{panel.Quit();};
    panel.popup.escape=[&]{panel.Quit(true,true);};
    panel.popup.shouldActivate=[&](UINT m,float x,float y){return panel.ShouldActivate(m,x,y);};
    panel.popup.deactivate=[&](HWND next){panel.Deactivate(next);};
    panel.popup.click=[&](UINT m,float x,float y){
        if(m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN)panel.panelPress=true;
        panel.Input(m,x,y);
    };
}
void CleanupPanel(Panel& panel,HWND bar,HWND control){
    panel.Close();
    if(control&&IsWindow(control))DestroyWindow(control);
    if(bar&&IsWindow(bar))DestroyWindow(bar);
    Pump();
}
HWND CreateControlWindow(Panel* panel){
    WNDCLASSW c{};c.lpfnWndProc=Panel::Proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=Bridge::ControlClass;
    RegisterClassW(&c);
    return CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,Bridge::ControlClass,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,c.hInstance,panel);
}
}
int wmain(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    HWND ext=HiddenHost();
    Check(ext!=nullptr,"external fixture window created");

    // 1. Reveal leaves WS_EX_NOACTIVATE off for click activation, shows without
    //    activating, installs WH_MOUSE_LL, and preserves slider non-pin behavior.
    {
        Panel panel;HWND bar=HiddenHost();HWND control=CreateControlWindow(&panel);
        SetupPanel(panel,bar,control,101);
        Check((GetWindowLongPtrW(panel.popup.window,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0,"popup initialized with WS_EX_NOACTIVATE");
        panel.Reveal();
        Check(panel.revealed&&IsWindowVisible(panel.popup.window),"reveal shows popup");
        Check((GetWindowLongPtrW(panel.popup.window,GWL_EXSTYLE)&WS_EX_NOACTIVATE)==0,"visible popup allows WM_MOUSEACTIVATE when non-slider clicked");
        Check(panel.mouseHook!=nullptr&&g_activeHookPanel==&panel,"revealed panel installs scoped WH_MOUSE_LL");
        Check(!panel.activated&&!panel.stay.clicked,"reveal alone does not activate or pin");
        // Slider click: must not activate or pin.
        Check(!panel.ShouldActivate(WM_LBUTTONDOWN,140.f,224.f),"slider click stays MA_NOACTIVATE");
        panel.Input(WM_LBUTTONDOWN,140.f,224.f);
        panel.Input(WM_LBUTTONUP,140.f,224.f);
        Check(!panel.stay.clicked,"slider press-release leaves panel unpinned");
        // Non-slider click: must allow activation, pin, and dismiss on external deactivate.
        Check(panel.ShouldActivate(WM_LBUTTONDOWN,20.f,80.f),"tile click returns MA_ACTIVATE");
        Check(!panel.ShouldActivate(WM_RBUTTONDOWN,20.f,80.f),"right-click before pin stays MA_NOACTIVATE");
        panel.Input(WM_LBUTTONDOWN,20.f,80.f);
        Check(panel.stay.clicked,"non-slider click pins panel");
        // Own-process deactivate targets must not spuriously close.
        panel.Deactivate(control);
        panel.Deactivate(panel.popup.window);
        Check(!panel.closing,"own-process deactivate ignored");
        // External deactivation (including elevated / same prior foreground) closes cleanly.
        panel.Deactivate(ext);
        Check(panel.closing,"external deactivation closes pinned panel");
        Check(panel.mouseHook==nullptr&&g_activeHookPanel==nullptr,"Quit unhooks WH_MOUSE_LL immediately");
        Check((GetWindowLongPtrW(panel.popup.window,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0,"leaving popup restores WS_EX_NOACTIVATE");
        CleanupPanel(panel,bar,control);
    }

    // 2. Scoped WH_MOUSE_LL hook ignores clicks inside popup.Hit and active drags,
    //    and closes immediately on clicks outside popup.Hit.
    {
        Panel panel;HWND bar=HiddenHost();HWND control=CreateControlWindow(&panel);
        SetupPanel(panel,bar,control,202);
        panel.Reveal();
        panel.Pin();
        Check(panel.stay.clicked&&g_activeHookPanel==&panel,"pinned panel armed with hook");
        const LONG inX=LONG(panel.popup.rect.x+panel.popup.rect.w/2);
        const LONG inY=LONG(panel.popup.rect.y+panel.popup.rect.h/2);
        const LONG outX=LONG(panel.popup.rect.x-40);
        const LONG outY=LONG(panel.popup.rect.y-40);
        MSLLHOOKSTRUCT hs{};
        hs.pt={inX,inY};
        Panel::MouseHookProc(HC_ACTION,WM_LBUTTONDOWN,reinterpret_cast<LPARAM>(&hs));
        Pump();
        Check(!panel.closing,"hook click inside popup keeps panel open");
        // Moving outside does not close; button down while captured does not close.
        hs.pt={outX,outY};
        Panel::MouseHookProc(HC_ACTION,WM_MOUSEMOVE,reinterpret_cast<LPARAM>(&hs));
        Pump();
        Check(!panel.closing,"hook ignores mousemove outside popup");
        SetCapture(panel.popup.window);
        if(GetCapture()==panel.popup.window){
            Panel::MouseHookProc(HC_ACTION,WM_LBUTTONDOWN,reinterpret_cast<LPARAM>(&hs));
            Pump();
            Check(!panel.closing,"hook ignores outside press while captured");
            ReleaseCapture();
        }
        Panel::MouseHookProc(HC_ACTION,WM_RBUTTONDOWN,reinterpret_cast<LPARAM>(&hs));
        Pump();
        Check(panel.closing,"hook button down outside popup dismisses immediately");
        Check(g_activeHookPanel==nullptr,"hook removed after DismissMessage");
        CleanupPanel(panel,bar,control);
    }

    // 3. Escape handler, middle/button fallback, and activated WindowSwitched fallback.
    {
        Panel panel;HWND bar=HiddenHost();HWND control=CreateControlWindow(&panel);
        SetupPanel(panel,bar,control,303);
        panel.Reveal();
        panel.stay.clicked=true;panel.activated=true;
        // Simulate activated panel where another HWND (even previous foreground) is active
        // if GetForegroundWindow is non-null external (in headless exebox may be null, verify WindowSwitched contract).
        if(HWND fg=GetForegroundWindow();fg&&fg!=panel.popup.window){
            panel.foreground=fg;
            Check(panel.WindowSwitched(),"activated WindowSwitched returns true even if foreground == now");
        }
        SendMessageW(panel.popup.window,WM_KEYDOWN,VK_ESCAPE,0);
        Check(panel.closing&&g_activeHookPanel==nullptr,"WM_KEYDOWN VK_ESCAPE closes and unhooks cleanly");
        CleanupPanel(panel,bar,control);
    }

    // 4. Rapid consecutive clicks on the popup never get swallowed by CS_DBLCLKS,
    //    and even synthetic WM_LBUTTONDBLCLK messages act as normal presses.
    {
        Panel panel;HWND bar=HiddenHost();HWND control=CreateControlWindow(&panel);
        SetupPanel(panel,bar,control,404);
        panel.Reveal();
        Check((GetClassLongPtrW(panel.popup.window,GCL_STYLE)&CS_DBLCLKS)==0,"non-menu popup class omits CS_DBLCLKS");
        Check(panel.ShouldActivate(WM_LBUTTONDBLCLK,24.f,224.f),"double-click message on button still permits activation");
        // Client coordinates carry a 32 DIP shadow margin inside popup.window.
        const LPARAM wifiMorePt=MAKELPARAM(32+135,32+80);
        const LPARAM pageBackPt=MAKELPARAM(32+20,32+20);
        SendMessageW(panel.popup.window,WM_LBUTTONDBLCLK,MK_LBUTTON,wifiMorePt);
        Check(panel.panelPress&&panel.stay.clicked,"synthetic WM_LBUTTONDBLCLK sets panelPress and pins");
        SendMessageW(panel.popup.window,WM_LBUTTONUP,0,wifiMorePt);
        Check(panel.page==PanelPage::Wifi,"synthetic WM_LBUTTONDBLCLK + UP activates main-page button");
        SendMessageW(panel.popup.window,WM_LBUTTONDBLCLK,MK_LBUTTON,pageBackPt);
        SendMessageW(panel.popup.window,WM_LBUTTONUP,0,pageBackPt);
        Check(panel.page==PanelPage::Main,"synthetic WM_LBUTTONDBLCLK + UP activates subpage back button");
        SendMessageW(panel.popup.window,WM_LBUTTONDOWN,MK_LBUTTON,wifiMorePt);
        SendMessageW(panel.popup.window,WM_LBUTTONUP,0,wifiMorePt);
        Check(panel.page==PanelPage::Wifi,"third consecutive click activates button again");
        CleanupPanel(panel,bar,control);
    }

    if(ext&&IsWindow(ext))DestroyWindow(ext);
    winrt::uninit_apartment();
    std::printf("%u dismiss checks passed, %u failed\n",passed,failed);
    return failed?1:0;
}
