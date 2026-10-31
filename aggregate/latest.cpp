#include "popup-latest.h"
#include "control-panel-latest.h"
#include "wifi-page-latest.h"
#include "control-tests-latest.h"
#include "../include/aggregate-bridge-latest.h"
#include <cwchar>
#include <imm.h>

// Reuses the accepted 0.9.8 detail painter and device workers, but not its Dock,
// app list, tray receiver, taskbar hiding, guard or startup mechanism.
namespace Capsule {
namespace Bridge=LeanDockAggregate;
struct Panel {
    HWND bar{},control{};HANDLE owner{};uintptr_t cookie{};
    Drawing draw;Popup popup;Metrics metrics;Power power;
    PanelGesture gesture;PanelPart hover=PanelPart::None;PanelMotion motion;
    PanelValueRequest volumeRequest,brightnessRequest;
    // Second-level page state; page credentials are wiped on every exit.
    PanelPage page=PanelPage::Main;WifiView wifi;PageTarget pageHover,pagePressed;Tween pageFade;bool escapeHeld=false;
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
        closing=true;wifi.Reset();if(control)for(auto timer:{TickTimer,FrameTimer,LeaveTimer})KillTimer(control,timer);
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
        motion.Update(display,page==PanelPage::Main?hover:PanelPart::None,gesture,now,revealed&&Animations());
        if(page==PanelPage::Main)
            popup.Paint([&](ID2D1DeviceContext* dc){PaintControlPanel(draw,dc,display,power,clock,calendarDate,calendarLunar,layout,hover,gesture,motion,now);},
                layout.Cuts());
        else{
            // A page keeps the main page's height, so going back never resizes.
            WifiLayout rows(display.wifiList,wifi,popup.height);wifi.scroll=rows.Clamp(wifi.scroll);
            popup.PaintPage([&](ID2D1DeviceContext* dc){
                PaintWifiPage(draw,dc,display,rows,wifi,pageHover,pageFade.At(now),motion.on[0].At(now));});
        }
        // Frames run only while a hover, toggle or slider transition does.
        if(const bool active=motion.Active(now)||pageFade.Active(now);active!=animating&&control){
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
    void Navigate(PanelPage to){
        if(to==page||closing)return;
        if(GetCapture()==popup.window)ReleaseCapture();
        Cancel();wifi.Reset();pageHover=pagePressed={};page=to;metrics.Page(to);
        Paint();if(closing)return;
        popup.scene.Navigate(to!=PanelPage::Main,revealed&&Animations());
    }
    // Typing needs keyboard focus, which this panel otherwise never takes.
    // The click that leads here was our own input, so Windows allows it.
    void Focus(WifiView::Focus field){
        wifi.focus=field;SetForegroundWindow(popup.window);SetFocus(popup.window);
        ImmAssociateContext(popup.window,nullptr); // Passwords are typed without an IME.
    }
    void Submit(Sample const& data,WifiRow const* row){
        if(!row||!data.wifiList.connecting.empty())return;
        if(row->net.connected){metrics.WifiDisconnect();return;}
        if(!WifiFormReady(*row,wifi))return;
        metrics.WifiConnect(row->net.ssid,row->user?wifi.user:std::wstring(),wifi.password);
        wifi.focus=WifiView::Focus::None;wifi.reveal=false;
    }
    static void Paste(std::wstring& text,size_t limit){
        if(!OpenClipboard(nullptr))return;
        if(HANDLE data=GetClipboardData(CF_UNICODETEXT))if(auto p=static_cast<const wchar_t*>(GlobalLock(data))){
            for(;*p&&text.size()<limit;++p)if(*p>=0x20)text.push_back(*p);
            GlobalUnlock(data);}
        CloseClipboard();
    }
    // Characters arrive as WM_CHAR; Escape leaves the field, not the panel.
    bool Key(UINT message,WPARAM key){
        if(page!=PanelPage::Wifi||wifi.focus==WifiView::Focus::None)return false;
        if(message==WM_KEYDOWN){
            if(key!=VK_ESCAPE)return false;
            wifi.focus=WifiView::Focus::None;escapeHeld=true;Paint();return true;
        }
        const bool user=wifi.focus==WifiView::Focus::User;auto& text=user?wifi.user:wifi.password;
        const size_t limit=user?128:63;
        if(key==0x0D||key==0x09){
            const auto data=metrics.Read();WifiLayout rows(data.wifiList,wifi,popup.height);auto row=rows.Find(wifi.expanded);
            if(key==0x0D)Submit(data,row);
            else if(row&&row->user)wifi.focus=user?WifiView::Focus::Password:WifiView::Focus::User;
        }
        else if(key==0x08){if(!text.empty())text.pop_back();}
        else if(key==0x16)Paste(text,limit);
        else if(key>=0x20&&key!=0x7F&&text.size()<limit)text.push_back(wchar_t(key));
        Paint();return true;
    }
    void PageInput(UINT message,float x,float y){
        const auto data=metrics.Read();WifiLayout rows(data.wifiList,wifi,popup.height);
        const auto target=WifiHit(rows,wifi,x,y);const auto now=GetTickCount64();
        if(message==WM_MOUSEMOVE){
            popup.scene.Light(x,y);
            if(!(target==pageHover)){pageHover=target;pageFade.Snap(0);if(target.hit!=PageHit::None)pageFade.Go(1,now,120);Paint();}
            return;
        }
        if(message==WM_LBUTTONDOWN){pagePressed=target;SetCapture(popup.window);return;}
        if(message!=WM_LBUTTONUP)return;
        const auto pressed=pagePressed;pagePressed={};
        if(GetCapture()==popup.window)ReleaseCapture();
        if(!(pressed==target))return;
        switch(target.hit){
        case PageHit::Back:Navigate(PanelPage::Main);return;
        case PageHit::Toggle:if(PanelEnabled(PanelPart::Wifi,data))metrics.Set(Control::Wifi,data.wifi==1?0:1);break;
        case PageHit::Row:
            if(wifi.expanded==target.key){wifi.Wipe();wifi.expanded.clear();break;}
            wifi.Wipe();wifi.expanded=target.key;
            if(WifiLayout open(data.wifiList,wifi,popup.height);auto row=open.Find(target.key)){
                wifi.scroll=open.Reveal(*row,wifi.scroll);
                if(row->form)Focus(row->user?WifiView::Focus::User:WifiView::Focus::Password);
            }
            break;
        case PageHit::User:Focus(WifiView::Focus::User);break;
        case PageHit::Field:Focus(WifiView::Focus::Password);break;
        case PageHit::Reveal:wifi.reveal=!wifi.reveal;break;
        case PageHit::Primary:Submit(data,rows.Find(target.key));break;
        default:wifi.focus=WifiView::Focus::None;break;
        }
        Paint();
    }
    void Input(UINT message,float x,float y){
        if(page!=PanelPage::Main){PageInput(message,x,y);return;}
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
            case PanelPart::WifiMore:Navigate(PanelPage::Wifi);break;
            default:break;
        }
        Cancel();if(GetCapture()==popup.window)ReleaseCapture();Paint();
    }
    void Tick(){
        if(closing)return;
        if(!Valid()||!IsWindowVisible(bar)||std::abs(GetDpiForWindow(bar)/96.f-dpi)>.01f){Quit();return;}
        // Escape closes the panel unless it was spent leaving a text field.
        const bool typing=wifi.focus!=WifiView::Focus::None;
        if(!(GetAsyncKeyState(VK_ESCAPE)&0x8000))escapeHeld=false;
        else if(!escapeHeld&&!typing){Quit(true);return;}
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
        if(typing&&GetForegroundWindow()!=popup.window){wifi.focus=WifiView::Focus::None;Paint();}
        if(inside)outside=0;
        else if(GetAsyncKeyState(VK_LBUTTON)&0x8000||GetAsyncKeyState(VK_RBUTTON)&0x8000){Quit(true);return;}
        // While typing, a pointer drifting off the panel does not close it.
        else if(wifi.focus!=WifiView::Focus::None)outside=0;
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
        popup.pointerCancel=[this]{Cancel();pagePressed={};};popup.cancel=[this]{Quit();};
        popup.key=[this](UINT m,WPARAM k){return Key(m,k);};
        popup.wheel=[this](int delta){if(page==PanelPage::Main)return;wifi.scroll-=float(delta)/WHEEL_DELTA*WifiRowHeight;Paint();};
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
             Capsule::RenderControlPanel(draw);Capsule::RenderWifiPage(draw);
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
