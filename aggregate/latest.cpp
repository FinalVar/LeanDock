#include "popup-latest.h"
#include "control-panel-latest.h"
#include "control-tests-latest.h"
#include "../include/aggregate-bridge-latest.h"
#include <cwchar>

// Reuses the accepted 0.9.8 detail painter and device workers, but not its Dock,
// app list, tray receiver, taskbar hiding, guard or startup mechanism.
namespace Capsule {
namespace Bridge=LeanDockAggregate;
struct Panel {
    HWND bar{},control{};HANDLE owner{};uintptr_t cookie{};
    Drawing draw;Popup popup;Metrics metrics;Power power;
    PanelGesture gesture;PanelPart hover=PanelPart::None;PanelMotion motion;
    PanelValueRequest volumeRequest,brightnessRequest;
    uint64_t outside=0,startedAt=0;bool closing=false,started=false,revealed=false,animating=false;float dpi=1;
    int paintedSecond=-1,calendarDay=-1;
    std::wstring calendarDate,calendarLunar;
    // Control-window timers: the visible-only 50 ms tick, frames while a
    // painted transition runs, and the end of the leave animation.
    enum Timer:UINT_PTR{TickTimer=1,FrameTimer=2,LeaveTimer=3};
    // A hidden standby that is never revealed (owner gone quiet) gives up.
    static constexpr uint64_t StandbyLimit=4000;
    ~Panel(){Close();if(owner)CloseHandle(owner);}
    bool Valid()const{return bar&&IsWindow(bar)&&GetPropW(bar,Bridge::CookieProp)==HANDLE(cookie)&&
        GetPropW(bar,Bridge::PidProp)==HANDLE(uintptr_t(GetCurrentProcessId()))&&
        owner&&WaitForSingleObject(owner,0)==WAIT_TIMEOUT;}
    void Cancel(){gesture.Cancel();hover=PanelPart::None;}
    // animate: a dismissal the user caused, faded out unless motion is off.
    // Lock, suspend, owner loss and WM_CLOSE end at once.
    void Quit(bool animate=false){
        if(closing)return;closing=true;
        // Withdraw the control window first: the owner reveals only a child
        // whose control window is still published, never a leaving one.
        if(Valid()&&GetPropW(bar,Bridge::ControlProp)==control)RemovePropW(bar,Bridge::ControlProp);
        const int delay=animate&&revealed&&popup.visible&&control?popup.Leave():0;
        if(delay>0&&SetTimer(control,LeaveTimer,UINT(delay),nullptr))return;
        Finish();
    }
    void Finish(){if(control)KillTimer(control,LeaveTimer);if(popup.window)popup.Hide();PostQuitMessage(0);}
    void Close(){
        closing=true;if(control)for(auto timer:{TickTimer,FrameTimer,LeaveTimer})KillTimer(control,timer);
        popup.Hide();metrics.Visible(false);metrics.Stop();
        if(Valid()){
            for(auto name:{Bridge::WindowProp,Bridge::ControlProp,Bridge::PendingProp})RemovePropW(bar,name);
            Bridge::Refresh(bar);
        }
        popup.Destroy();if(control){DestroyWindow(control);control=nullptr;}
    }
    void Position(){
        RECT anchor{};if(!Bridge::Anchor(bar,anchor)){Quit();return;}
        MONITORINFO mon{sizeof(mon)};GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&mon);
        const double left=std::clamp(double(anchor.right)-PanelWidth*dpi,double(mon.rcWork.left+8*dpi),
            std::max(double(mon.rcWork.left+8*dpi),double(mon.rcWork.right)-(PanelWidth+8)*dpi));
        const double top=std::max(double(mon.rcWork.top+8*dpi),double(anchor.top)-(10+popup.height)*dpi);
        if(popup.rect.x!=left||popup.rect.y!=top||popup.rect.h!=popup.height*dpi)popup.Position(left,top);
    }
    void Paint(){
        if(closing||!popup.window)return;
        power.Read();auto data=metrics.Read();SYSTEMTIME time{};GetLocalTime(&time);
        paintedSecond=time.wHour*3600+time.wMinute*60+time.wSecond;
        const auto day=time.wYear*10000+time.wMonth*100+time.wDay;
        if(day!=calendarDay){calendarDay=day;
            calendarDate=std::to_wstring(time.wMonth)+L"月"+std::to_wstring(time.wDay)+L"日";
            calendarLunar=Lunar(time);
        }
        const auto clock=PanelClock(time.wHour,time.wMinute,time.wSecond);
        PanelLayout layout(data,0); // The tray remains in native Explorer, always.
        const float height=gesture.Dragging()?popup.height:layout.height;
        if(height!=popup.height){popup.height=height;popup.scene.Dimensions(PanelWidth,PanelWidth,height);}
        Position();if(closing)return;
        auto display=data;const auto now=GetTickCount64();
        display.volume=volumeRequest.Display(data.volume,data.completed[unsigned(Control::Volume)],now);
        display.brightness=brightnessRequest.Display(data.brightness,data.completed[unsigned(Control::Brightness)],now);
        motion.Update(display,hover,gesture,now,revealed&&Animations());
        popup.Paint([&](ID2D1DeviceContext* dc){PaintControlPanel(draw,dc,display,power,clock,calendarDate,calendarLunar,layout,hover,gesture,motion,now);},
            layout.Cuts());
        // Frames run only while a hover, toggle or slider transition does.
        if(const bool active=motion.Active(now);active!=animating&&control){
            animating=active;if(active)SetTimer(control,FrameTimer,16,nullptr);else KillTimer(control,FrameTimer);
        }
    }
    void Light(POINT p){popup.scene.Light(float((p.x-popup.rect.x)/dpi),float((p.y-popup.rect.y)/dpi));}
    // Shown only once the owner publishes this cookie as revealed: the real
    // hover delay has passed on the key. Values sampled hidden are ready.
    void Reveal(){
        if(revealed||closing||!popup.window||!Valid()||!Bridge::Revealed(bar,cookie))return;
        revealed=true;metrics.Urgent();Paint();if(closing)return;
        POINT cursor{};if(GetCursorPos(&cursor))Light(cursor);
        popup.Show(true);RemovePropW(bar,Bridge::PendingProp);Bridge::Refresh(bar);
        Log(L"panel-open");
    }
    void Set(Control target,int value){
        const auto serial=metrics.Set(target,value);
        if(serial)(target==Control::Volume?volumeRequest:brightnessRequest)={serial,GetTickCount64(),value};
    }
    void Open(wchar_t const* uri){ShellExecuteW(popup.window,L"open",uri,nullptr,nullptr,SW_SHOWNORMAL);Quit(true);}
    // Where ShellHost runs Quick Settings (build 26300), ms-availablenetworks:
    // only shows their main page; ms-controlcenter:wifi opens the network
    // list itself (checked 2026-10-31). Older systems keep the old link.
    static wchar_t const* WifiList(){
        return RegGetValueW(HKEY_CLASSES_ROOT,L"ms-controlcenter\\shell\\open\\command",nullptr,RRF_RT_ANY,nullptr,nullptr,nullptr)?
            L"ms-availablenetworks:":L"ms-controlcenter:wifi";
    }
    void Input(UINT message,float x,float y){
        const auto hit=PanelHit(x,y);const auto data=metrics.Read();
        if(message==WM_MOUSEMOVE){
            popup.scene.Light(x,y);
            if(gesture.Dragging()){
                if(gesture.Send(x,GetTickCount64()))Set(gesture.pressed==PanelPart::Volume?Control::Volume:Control::Brightness,gesture.preview);
                Paint();
            }else if(hover!=hit){hover=hit;Paint();}return;
        }
        if(message==WM_LBUTTONDOWN&&PanelEnabled(hit,data)){
            gesture.Begin(hit,x);SetCapture(popup.window);
            if(gesture.Send(x,GetTickCount64()))Set(hit==PanelPart::Volume?Control::Volume:Control::Brightness,gesture.preview);
            Paint();return;
        }
        if(message!=WM_LBUTTONUP||gesture.pressed==PanelPart::None)return;
        const auto pressed=gesture.pressed;
        if(gesture.Dragging()){
            if(gesture.Send(x,GetTickCount64(),true))Set(pressed==PanelPart::Volume?Control::Volume:Control::Brightness,gesture.preview);
        }else if(hit==pressed&&PanelEnabled(hit,data))switch(hit){
            case PanelPart::Mute:metrics.Set(Control::Mute,!data.mute);break;
            case PanelPart::Wifi:metrics.Set(Control::Wifi,data.wifi==1?0:1);break;
            case PanelPart::Bluetooth:metrics.Set(Control::Bluetooth,data.bluetooth==1?0:1);break;
            case PanelPart::DarkTheme:metrics.Set(Control::DarkTheme,data.darkTheme==1?0:1);break;
            case PanelPart::Hotspot:metrics.Set(Control::Hotspot,data.hotspot==1?0:1);break;
            case PanelPart::WifiMore:Open(WifiList());break;
            case PanelPart::BluetoothMore:Open(L"ms-settings:bluetooth");break;
            case PanelPart::DarkThemeMore:Open(L"ms-settings:themes");break;
            case PanelPart::HotspotMore:Open(L"ms-settings:network-mobilehotspot");break;
            default:break;
        }
        Cancel();if(GetCapture()==popup.window)ReleaseCapture();Paint();
    }
    void Tick(){
        if(closing)return;
        if(!Valid()||!IsWindowVisible(bar)||std::abs(GetDpiForWindow(bar)/96.f-dpi)>.01f){Quit();return;}
        if(GetAsyncKeyState(VK_ESCAPE)&0x8000){Quit(true);return;}
        if(!revealed){
            if(Bridge::Revealed(bar,cookie))Reveal();
            else if(GetTickCount64()-startedAt>=StandbyLimit){Quit();return;}
        }
        RECT anchor{};POINT cursor{};
        if(!Bridge::Anchor(bar,anchor)||!GetCursorPos(&cursor)){Quit();return;}
        Position();popup.Hover(cursor);
        // Inside the panel WM_MOUSEMOVE moves the light more finely.
        if(!popup.Hit(cursor.x,cursor.y))Light(cursor);
        // Before the reveal only the key counts: a pass-over ends the standby.
        const bool inside=PtInRect(&anchor,cursor)||popup.Hit(cursor.x,cursor.y)||GetCapture()==popup.window;
        if(inside)outside=0;
        else if(GetAsyncKeyState(VK_LBUTTON)&0x8000||GetAsyncKeyState(VK_RBUTTON)&0x8000){Quit(true);return;}
        else if(!outside)outside=GetTickCount64();
        else if(GetTickCount64()-outside>=250)Quit(true);
        // Clock remains live even when a device query is slow. Reuse this
        // existing visible-only timer; never add a resident background clock.
        SYSTEMTIME time{};GetLocalTime(&time);
        if(!closing&&paintedSecond!=time.wHour*3600+time.wMinute*60+time.wSecond)Paint();
    }
    static LRESULT CALLBACK Proc(HWND window,UINT message,WPARAM a,LPARAM b){
        auto self=reinterpret_cast<Panel*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,LONG_PTR(self));}
        if(self)try{
            if(message==WM_CLOSE){self->Quit();return 0;}
            if(message==WM_TIMER){
                if(a==LeaveTimer)self->Finish();else if(a==FrameTimer)self->Paint();else self->Tick();
                return 0;
            }
            if(message==WM_APP+28){self->Paint();return 0;}
            if(message==Bridge::RevealMessage){self->Reveal();return 0;}
            if(message==WM_SETTINGCHANGE||message==WM_THEMECHANGED){self->draw.colors.Refresh();self->popup.scene.Theme();self->Paint();return 0;}
            if(message==WM_WTSSESSION_CHANGE&&a==WTS_SESSION_LOCK){self->Quit();return 0;}
            if(message==WM_POWERBROADCAST&&a==PBT_APMSUSPEND){self->Quit();return TRUE;}
        }catch(...){Log(L"panel-event-failed",winrt::to_hresult());self->Quit();return 0;}
        return DefWindowProcW(window,message,a,b);
    }
    int Run(HWND taskbar,uintptr_t token){
        bar=taskbar;cookie=token;DWORD pid=0;GetWindowThreadProcessId(bar,&pid);
        wchar_t explorer[MAX_PATH]{},cls[64]{};GetWindowsDirectoryW(explorer,MAX_PATH);wcscat_s(explorer,L"\\explorer.exe");
        GetClassNameW(bar,cls,64);
        if(!cookie||Elevated()||(wcscmp(cls,L"Shell_TrayWnd")&&wcscmp(cls,L"Shell_SecondaryTrayWnd"))||!ExactProcess(bar,explorer))return 20;
        owner=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!Valid())return 21;
        dpi=GetDpiForWindow(bar)/96.f;
        WNDCLASSW c{};c.lpfnWndProc=Proc;c.lpszClassName=Bridge::ControlClass;c.hInstance=GetModuleHandleW(nullptr);RegisterClassW(&c);
        // Hidden top-level control receives theme/session broadcasts; no input hook.
        control=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,c.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,c.hInstance,this);
        if(!control)winrt::throw_last_error();
        WTSRegisterSessionNotification(control,NOTIFY_FOR_THIS_SESSION);
        // Sampling starts before any graphics setup: the pointer is resting
        // on the key and the owner reveals this window at the hover delay.
        startedAt=GetTickCount64();metrics.Start(control);metrics.Visible(true);started=true;
        popup.Init(draw,dpi,PanelWidth,408,14,false);
        popup.click=[this](UINT m,float x,float y){Input(m,x,y);};
        popup.pointerCancel=[this]{Cancel();};popup.cancel=[this]{Quit();};
        if(!Valid())return 22;
        SetPropW(bar,Bridge::ControlProp,control);SetPropW(bar,Bridge::WindowProp,popup.window);
        Paint();if(closing)return 23;
        if(!SetTimer(control,TickTimer,50,nullptr))winrt::throw_last_error();
        // The owner sets the reveal property before looking for this control
        // window, so checking after publishing it cannot miss a reveal.
        Reveal();MSG msg{};
        while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
        WTSUnRegisterSessionNotification(control);Close();Log(L"panel-closed");return 0;
    }
};
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try{
        int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);int result=2;
        if(count==2&&!wcscmp(args[1],L"--hidden-probe")){
            LocalFree(args);winrt::init_apartment(winrt::apartment_type::single_threaded);
            {Capsule::Drawing draw;Capsule::Popup popup;popup.Init(draw,1,300,408,14,false);
             Capsule::RenderControlPanel(draw);
             // Visual fixture only; a host without visual capture still passes.
             try{Capsule::RenderMaterial(draw,popup.scene);}catch(...){Capsule::Log(L"material-probe-failed",winrt::to_hresult());}
             if(IsWindowVisible(popup.window))winrt::throw_hresult(E_UNEXPECTED);
             popup.Destroy();}
            winrt::uninit_apartment();return Capsule::ControlsProbe();
        }
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        if(count==4&&!wcscmp(args[1],L"--owner")){
            HWND bar=reinterpret_cast<HWND>(_wcstoui64(args[2],nullptr,10));auto cookie=_wcstoui64(args[3],nullptr,10);
            LocalFree(args);Capsule::Panel panel;result=panel.Run(bar,cookie);
        }else LocalFree(args);
        return result;
    }catch(...){Capsule::Log(L"panel-init-failed",winrt::to_hresult());return 24;}
}
