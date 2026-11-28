#include "popup-latest.h"
#include "control-panel-latest.h"
#include "wifi-page-latest.h"
#include "bluetooth-page-latest.h"
#include "hotspot-page-latest.h"
#include "sound-page-latest.h"
#include "control-tests-latest.h"
#include "../include/aggregate-bridge-latest.h"
#include "../include/dock-geometry-latest.h"
#include <cwchar>
#include <imm.h>

// Reuses the accepted 0.9.8 detail painter and device workers, but not its Dock,
// app list, tray receiver, taskbar hiding, guard or startup mechanism.
namespace Capsule {
namespace Bridge=LeanDockAggregate;
struct Panel;
inline Panel* g_activeHookPanel=nullptr;
struct Panel {
    HWND bar{},control{};HANDLE owner{};uintptr_t cookie{};
    Drawing draw;Popup popup;Metrics metrics;Power power;
    PanelGesture gesture;PanelPart hover=PanelPart::None;PanelMotion motion;
    PanelValueRequest volumeRequest,brightnessRequest;int savedBrightness=0;
    // Second-level page state; page credentials are wiped on every exit.
    PanelPage page=PanelPage::Main;WifiView wifi;BluetoothView bluetooth;HotspotView hotspot;SoundView sound;
    PageTarget pageHover,pagePressed;Tween pageFade;bool escapeHeld=false;
    SoundDrag drag;PanelValueRequest appRequest;std::wstring appRequestKey;
    std::vector<std::pair<std::wstring,winrt::com_ptr<IWICBitmap>>> icons; // app icons by program path, this panel only
    uint64_t startedAt=0;bool closing=false,started=false,revealed=false,animating=false;float dpi=1;
    PanelStay stay;HWND foreground{},previousForeground{};bool buttonDown[3]{},panelPress=false,activated=false;
    HHOOK mouseHook{};
    int paintedSecond=-1,calendarDay=-1;
    std::wstring calendarDate,calendarLunar;
    // Control-window timers: the visible-only 50 ms tick, frames while a
    // painted transition runs, leave fade-out, and quiet keepalive expiry.
    enum Timer:UINT_PTR{TickTimer=1,FrameTimer=2,LeaveTimer=3,KeepaliveTimer=4,PrimeTimer=5};
    static constexpr UINT DismissMessage=WM_APP+30;
    // A hidden standby that is never revealed (owner gone quiet) gives up.
    static constexpr uint64_t StandbyLimit=4000;
    static constexpr UINT KeepaliveMs=4000,PrimeHoldMs=600;
    bool keepalive=false,allowKeepalive=false;
    ~Panel(){Close();if(owner)CloseHandle(owner);}
    bool Valid()const{return bar&&IsWindow(bar)&&GetPropW(bar,Bridge::CookieProp)==HANDLE(cookie)&&
        GetPropW(bar,Bridge::PidProp)==HANDLE(uintptr_t(GetCurrentProcessId()))&&
        owner&&WaitForSingleObject(owner,0)==WAIT_TIMEOUT;}
    void Cancel(){gesture.Cancel();hover=PanelPart::None;drag.Cancel();}
    void Forget(){wifi.Reset();bluetooth.Reset();hotspot.Reset();sound.Reset();}
    static LRESULT CALLBACK MouseHookProc(int code,WPARAM wParam,LPARAM lParam);
    void InstallMouseHook(){
        if(mouseHook||!control)return;
        g_activeHookPanel=this;
        mouseHook=SetWindowsHookExW(WH_MOUSE_LL,MouseHookProc,nullptr,0);
        if(!mouseHook&&g_activeHookPanel==this)g_activeHookPanel=nullptr;
    }
    void UninstallMouseHook(){
        if(g_activeHookPanel==this)g_activeHookPanel=nullptr;
        if(mouseHook){UnhookWindowsHookEx(mouseHook);mouseHook=nullptr;}
    }
    // animate: a dismissal the user caused, faded out unless motion is off.
    // Lock, suspend, owner loss and WM_CLOSE end at once.
    void Quit(bool animate=false,bool restoreFocus=false){
        if(closing)return;closing=true;
        allowKeepalive=animate&&revealed;
        UninstallMouseHook();
        if(restoreFocus&&GetForegroundWindow()==popup.window&&previousForeground&&
           IsWindow(previousForeground)&&previousForeground!=popup.window){
            SetForegroundWindow(previousForeground);
        }
        // Withdraw the control window and clear the consumed reveal token first:
        // the owner reveals only a child whose control window is still published.
        if(Valid()){
            if(GetPropW(bar,Bridge::ControlProp)==control)RemovePropW(bar,Bridge::ControlProp);
            if(GetPropW(bar,Bridge::RevealProp)==HANDLE(cookie))RemovePropW(bar,Bridge::RevealProp);
        }
        const int delay=animate&&revealed&&popup.visible&&control?popup.Leave():0;
        if(delay>0&&SetTimer(control,LeaveTimer,UINT(delay),nullptr))return;
        Finish();
    }
    void Finish(){
        if(control)KillTimer(control,LeaveTimer);
        const bool park=allowKeepalive;allowKeepalive=false;
        if(park&&Valid()&&control){
            // Park warm for KeepaliveMs with all timers and background sampling stopped.
            keepalive=true;closing=false;revealed=false;animating=false;
            for(auto timer:{TickTimer,FrameTimer,PrimeTimer})KillTimer(control,timer);
            Unfocus();if(popup.window)ImmAssociateContext(popup.window,nullptr);
            popup.Hide();popup.scene.Navigate(false,false);
            metrics.Page(PanelPage::Main);metrics.Visible(false);
            Cancel();Forget();icons.clear();page=PanelPage::Main;pageHover=pagePressed={};pageFade.Snap(0);escapeHeld=false;
            volumeRequest={};brightnessRequest={};appRequest={};appRequestKey.clear();
            stay={};foreground={};previousForeground={};activated=false;
            for(bool& down:buttonDown)down=false;panelPress=false;
            paintedSecond=-1;motion={};
            if(Valid()){
                SetPropW(bar,Bridge::ControlProp,control);
                Bridge::Refresh(bar);
                if(SetTimer(control,KeepaliveTimer,KeepaliveMs,nullptr))return;
                RemovePropW(bar,Bridge::ControlProp);
            }
            keepalive=false;closing=true;
        }
        if(popup.window)popup.Hide();PostQuitMessage(0);
    }
    void Prime(){
        if(!keepalive||closing||!control||!Valid())return;
        SetTimer(control,KeepaliveTimer,KeepaliveMs,nullptr);
        SetTimer(control,PrimeTimer,PrimeHoldMs,nullptr);
        metrics.Visible(true);metrics.Urgent();
    }
    void ExpireKeepalive(){
        keepalive=false;closing=true;
        if(control)for(auto timer:{KeepaliveTimer,PrimeTimer})KillTimer(control,timer);
        metrics.Visible(false);
        if(Valid()&&GetPropW(bar,Bridge::ControlProp)==control)RemovePropW(bar,Bridge::ControlProp);
        if(popup.window)popup.Hide();PostQuitMessage(0);
    }
    void Close(){
        closing=true;keepalive=false;allowKeepalive=false;UninstallMouseHook();Forget();icons.clear();
        if(control)for(auto timer:{TickTimer,FrameTimer,LeaveTimer,KeepaliveTimer,PrimeTimer})KillTimer(control,timer);
        popup.Hide();metrics.Visible(false);metrics.Stop();
        if(Valid()){
            for(auto name:{Bridge::WindowProp,Bridge::ControlProp,Bridge::PendingProp})RemovePropW(bar,name);
            Bridge::Refresh(bar);
        }
        popup.Destroy();if(control){DestroyWindow(control);control=nullptr;}
    }
    void Position(){
        RECT anchor{},dock{};if(!Bridge::Anchor(bar,anchor)){Quit();return;}
        MONITORINFO mon{sizeof(mon)};
        if(!LeanDockGeometry::Read(bar,dock,&mon)){
            // A new Dock size is being published: stay put and read it next tick.
            // With no position yet, the key alone is the reference.
            if(popup.rect.w>0)return;
            GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&mon);dock=anchor;
        }
        const auto at=LeanDockGeometry::PopupFromPart(anchor,dock,mon.rcWork,std::lround(PanelWidth*dpi),std::lround(popup.height*dpi),std::lround(dpi*96));
        if(popup.rect.x!=at.x||popup.rect.y!=at.y||popup.rect.h!=popup.height*dpi)popup.Position(at.x,at.y);
    }
    void Paint(){
        if(closing||keepalive||!popup.window)return;
        power.Read();auto data=metrics.Read();SYSTEMTIME time{};GetLocalTime(&time);
        paintedSecond=time.wHour*3600+time.wMinute*60+time.wSecond;
        const auto day=time.wYear*10000+time.wMonth*100+time.wDay;
        if(day!=calendarDay){calendarDay=day;
            calendarDate=std::to_wstring(time.wMonth)+L"月"+std::to_wstring(time.wDay)+L"日 周"+(L"日一二三四五六"[time.wDayOfWeek%7]);
            calendarLunar=Lunar(time);
        }
        const auto clock=PanelClock(time.wHour,time.wMinute,time.wSecond);
        PanelLayout layout(data,0); // The tray remains in native Explorer, always.
        const float height=gesture.Dragging()||drag.active?popup.height:layout.height;
        if(height!=popup.height){popup.height=height;popup.scene.Dimensions(PanelWidth,PanelWidth,height,popup.visible&&Animations());}
        Position();if(closing)return;
        auto display=data;const auto now=GetTickCount64();
        display.volume=volumeRequest.Display(data.volume,data.completed[unsigned(Control::Volume)],now);
        display.brightness=brightnessRequest.Display(data.brightness,data.completed[unsigned(Control::Brightness)],now);
        for(auto& app:display.sound.apps)
            if(app.key==appRequestKey)app.volume=appRequest.Display(app.volume,data.completed[unsigned(Control::AppVolume)],now);
        Settle(data);
        motion.Update(display,page==PanelPage::Main?hover:PanelPart::None,gesture,now,revealed&&Animations());
        if(page==PanelPage::Main)
            popup.Paint([&](ID2D1DeviceContext* dc){PaintControlPanel(draw,dc,display,power,clock,calendarDate,calendarLunar,layout,hover,gesture,motion,now);},
                layout.Cuts());
        // A page keeps the main page's height, so going back never resizes.
        else popup.PaintPage([&](ID2D1DeviceContext* dc){PaintPage(dc,display,now);});
        // Frames run only while a hover, toggle or slider transition does.
        if(const bool active=motion.Active(now)||pageFade.Active(now);active!=animating&&control){
            animating=active;if(active)SetTimer(control,FrameTimer,16,nullptr);else KillTimer(control,FrameTimer);
        }
    }
    void PaintPage(ID2D1DeviceContext* dc,Sample const& s,uint64_t now){
        const bool footer=!s.error.empty();const float fade=pageFade.At(now);
        switch(page){
        case PanelPage::Wifi:{WifiLayout rows(s.wifiList,wifi,popup.height,footer);wifi.scroll=rows.Clamp(wifi.scroll);
            PaintWifiPage(draw,dc,s,rows,wifi,pageHover,fade,motion.on[0].At(now));break;}
        case PanelPage::Bluetooth:{BluetoothLayout rows(s.bluetoothList,bluetooth,popup.height,footer);bluetooth.scroll=rows.Clamp(bluetooth.scroll);
            PaintBluetoothPage(draw,dc,s,rows,bluetooth,pageHover,fade,motion.on[1].At(now));break;}
        case PanelPage::Hotspot:PaintHotspotPage(draw,dc,s,hotspot,pageHover,fade,motion.on[3].At(now),popup.height,footer);break;
        case PanelPage::Sound:{SoundLayout rows(s.sound,popup.height,footer);sound.scroll=rows.Clamp(sound.scroll);
            PaintSoundPage(draw,dc,s,rows,sound,pageHover,fade,drag,[this](std::wstring const& path){return Icon(path);});break;}
        default:break;
        }
        PaintPageFooter(draw,dc,popup.height,s.error);
    }
    // The program's icon at this panel's DPI, loaded once per panel.
    IWICBitmapSource* Icon(std::wstring const& path){
        for(auto const& [p,bitmap]:icons)if(p==path)return bitmap.get();
        winrt::com_ptr<IWICBitmap> bitmap;HICON icon=nullptr;const UINT size=UINT(std::lround(20*dpi));
        if(!path.empty()&&SHDefExtractIconW(path.c_str(),0,0,&icon,nullptr,size)==S_OK&&icon){
            try{if(auto art=draw.Artwork(draw.Icon(icon));art.image)bitmap=draw.RasterArtwork(art,size,size);}catch(...){}
            DestroyIcon(icon);
        }
        icons.emplace_back(path,bitmap);return bitmap.get();
    }
    // A hotspot save has finished: back to the details, or the form again
    // with the old key to retry.
    void Settle(Sample const& data){
        if(!hotspot.saving||data.completed[unsigned(Control::HotspotConfigure)]<hotspot.serial||data.hotspotInfo.saving)return;
        hotspot.saving=false;
        if(data.hotspotInfo.failure.empty())hotspot.Reset();else metrics.HotspotKey(hotspot.password);
    }
    void Light(POINT p){popup.scene.Light(float((p.x-popup.rect.x)/dpi),float((p.y-popup.rect.y)/dpi));}
    // Shown only once the owner publishes this cookie as revealed: the real
    // hover delay has passed on the key. Values sampled hidden are ready.
    void Reveal(){
        if(revealed||closing||!popup.window||!Valid()||!Bridge::Revealed(bar,cookie))return;
        // Waking from keepalive: cancel expiry, restart the visible tick and drain stale button bits.
        if(keepalive){
            if(!IsWindowVisible(bar)||std::abs(GetDpiForWindow(bar)/96.f-dpi)>.01f){ExpireKeepalive();return;}
            keepalive=false;
            if(control){
                KillTimer(control,KeepaliveTimer);KillTimer(control,PrimeTimer);
                if(!SetTimer(control,TickTimer,50,nullptr)){ExpireKeepalive();return;}
            }
            draw.colors.Refresh();popup.scene.Theme();
            if(const int saved=Bridge::SavedBrightness(bar);saved>0)savedBrightness=saved;
            metrics.Visible(true);
        }
        // Drain sticky GetAsyncKeyState bits on both cold launch and keepalive wake so
        // an older click (e.g. inside a maximized app or on the lock screen) never triggers
        // an immediate outside-press dismissal on the first visible tick.
        (void)PressedElsewhere();
        escapeHeld=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
        revealed=true;metrics.Urgent();Paint();if(closing)return;
        if(HWND fg=GetForegroundWindow();fg&&fg!=popup.window){foreground=fg;previousForeground=fg;}
        POINT cursor{};if(GetCursorPos(&cursor))Light(cursor);
        popup.Show(true);InstallMouseHook();RemovePropW(bar,Bridge::PendingProp);Bridge::Refresh(bar);
        Log(L"panel-open");
    }
    void Set(Control target,int value){
        const auto serial=metrics.Set(target,value);
        if(serial)(target==Control::Volume?volumeRequest:brightnessRequest)={serial,GetTickCount64(),value};
    }
    void Navigate(PanelPage to){
        if(to==page||closing)return;
        if(GetCapture()==popup.window)ReleaseCapture();
        Cancel();Forget();pageHover=pagePressed={};page=to;metrics.Page(to);
        Paint();if(closing)return;
        popup.scene.Navigate(to!=PanelPage::Main,revealed&&Animations());
    }
    // Typing needs keyboard focus, which this panel otherwise never takes.
    // The click that leads here was our own input, so Windows allows it.
    // Keys and user names are typed without an IME; a hotspot name may be
    // Chinese, so that field gets the default one, composing under the box.
    void TakeKeyboard(Rect const* ime=nullptr){
        if(HWND fg=GetForegroundWindow();fg&&fg!=popup.window&&!previousForeground)previousForeground=fg;
        SetForegroundWindow(popup.window);SetFocus(popup.window);
        if(GetForegroundWindow()==popup.window)activated=true;
        if(!ime){ImmAssociateContext(popup.window,nullptr);return;}
        ImmAssociateContextEx(popup.window,nullptr,IACE_DEFAULT);
        if(HIMC context=ImmGetContext(popup.window)){
            // Client pixels; the panel draws 32 DIP inside its window.
            COMPOSITIONFORM at{CFS_POINT,{LONG(std::lround((32+ime->x+10)*popup.dpi)),LONG(std::lround((32+ime->y+ime->h+4)*popup.dpi))},{}};
            ImmSetCompositionWindow(context,&at);ImmReleaseContext(popup.window,context);
        }
    }
    void Focus(WifiView::Focus field){if(wifi.focus!=field)wifi.selectAll=false;wifi.focus=field;TakeKeyboard();}
    void Focus(HotspotView::Focus field){
        if(hotspot.focus!=field)hotspot.selectAll=false;
        hotspot.focus=field;TakeKeyboard(field==HotspotView::Focus::Name?&HotspotNameBox:nullptr);
    }
    bool Typing()const{
        return (page==PanelPage::Wifi&&wifi.focus!=WifiView::Focus::None)||
            (page==PanelPage::Hotspot&&hotspot.focus!=HotspotView::Focus::None);
    }
    void Unfocus(){wifi.focus=WifiView::Focus::None;wifi.selectAll=false;hotspot.focus=HotspotView::Focus::None;hotspot.selectAll=false;}
    void Submit(Sample const& data,WifiRow const* row){
        if(!row||!data.wifiList.connecting.empty())return;
        if(row->net.connected){metrics.Send(Control::WifiDisconnect,0);return;}
        if(!WifiFormReady(*row,wifi))return;
        metrics.WifiConnect(row->net.ssid,row->user?wifi.user:std::wstring(),wifi.password);
        wifi.focus=WifiView::Focus::None;wifi.reveal=wifi.selectAll=false;
    }
    void EditHotspot(Sample const& data){
        if(!HotspotEditable(data))return;
        hotspot.Wipe();hotspot.editing=true;hotspot.saving=false;hotspot.serial=0;
        hotspot.name=data.hotspotInfo.ssid;hotspot.band=data.hotspotInfo.band;metrics.HotspotKey(hotspot.password);
        Focus(HotspotView::Focus::Name);
    }
    void SaveHotspot(Sample const& data){
        if(hotspot.saving||HotspotProblem(hotspot.name,hotspot.password)||!HotspotEditable(data))return;
        const int band=hotspot.band>=0&&hotspot.band<3&&data.hotspotInfo.bands[size_t(hotspot.band)]?hotspot.band:-1;
        hotspot.serial=metrics.HotspotConfigure(hotspot.name,band,hotspot.password); // wipes the key
        hotspot.saving=hotspot.serial!=0;hotspot.focus=HotspotView::Focus::None;hotspot.reveal=hotspot.selectAll=false;
    }
    static void Paste(std::wstring& text,size_t limit,bool ascii){
        if(!OpenClipboard(nullptr))return;
        if(HANDLE data=GetClipboardData(CF_UNICODETEXT))if(auto p=static_cast<const wchar_t*>(GlobalLock(data))){
            for(;*p&&text.size()<limit;++p)if(*p>=0x20&&(!ascii||*p<0x7F))text.push_back(*p);
            GlobalUnlock(data);}
        CloseClipboard();
    }
    // Backspace clears the character it removes; Ctrl+Backspace / Ctrl+U /
    // Ctrl+A+Delete securely wipe the whole reserved buffer.
    static void Edit(std::wstring& text,bool& selectAll,WPARAM key,size_t limit,bool ascii){
        EditFieldWith(text,selectAll,key,limit,ascii,Paste);
    }
    // Characters arrive as WM_CHAR; Escape leaves the field, not the panel.
    bool Key(UINT message,WPARAM key){
        if(!Typing())return false;
        if(message==WM_KEYDOWN){
            if(key==VK_ESCAPE){Unfocus();escapeHeld=true;Paint();return true;}
            if(key==VK_DELETE){
                auto& text=page==PanelPage::Wifi?(wifi.focus==WifiView::Focus::User?wifi.user:wifi.password):
                    (hotspot.focus==HotspotView::Focus::Name?hotspot.name:hotspot.password);
                auto& sel=page==PanelPage::Wifi?wifi.selectAll:hotspot.selectAll;
                if(sel||(GetKeyState(VK_CONTROL)&0x8000)!=0){WipeField(text);sel=false;Paint();}
                return true;
            }
            return false;
        }
        if(page==PanelPage::Wifi){
            const bool user=wifi.focus==WifiView::Focus::User;
            if(key==0x0D||key==0x09){
                const auto data=metrics.Read();WifiLayout rows(data.wifiList,wifi,popup.height);auto row=rows.Find(wifi.expanded);
                if(key==0x0D)Submit(data,row);
                else if(row&&row->user){wifi.selectAll=false;wifi.focus=user?WifiView::Focus::Password:WifiView::Focus::User;}
            }
            else Edit(user?wifi.user:wifi.password,wifi.selectAll,key,user?128:63,false);
        }else{
            const bool name=hotspot.focus==HotspotView::Focus::Name;
            if(key==0x0D)SaveHotspot(metrics.Read());
            else if(key==0x09)Focus(name?HotspotView::Focus::Password:HotspotView::Focus::Name);
            // The SSID limit is 32 bytes; HotspotProblem checks the encoded length.
            else Edit(name?hotspot.name:hotspot.password,hotspot.selectAll,key,name?32:63,!name);
        }
        Paint();return true;
    }
    PageTarget PageHitAt(Sample const& data,float x,float y){
        const bool footer=!data.error.empty();
        switch(page){
        case PanelPage::Wifi:return WifiHit(WifiLayout(data.wifiList,wifi,popup.height,footer),wifi,x,y);
        case PanelPage::Bluetooth:return BluetoothHit(BluetoothLayout(data.bluetoothList,bluetooth,popup.height,footer),bluetooth,x,y);
        case PanelPage::Hotspot:return HotspotHit(hotspot,x,y);
        case PanelPage::Sound:return SoundHit(SoundLayout(data.sound,popup.height,footer),sound,x,y);
        default:return {};
        }
    }
    void SendDrag(){
        if(drag.key.empty()){Set(Control::Volume,drag.preview);return;}
        if(const auto serial=metrics.Send(Control::AppVolume,drag.preview,drag.key)){
            appRequest={serial,GetTickCount64(),drag.preview};appRequestKey=drag.key;}
    }
    void WifiClick(Sample const& data,PageTarget const& target){
        WifiLayout rows(data.wifiList,wifi,popup.height,!data.error.empty());
        switch(target.hit){
        case PageHit::Toggle:if(PanelEnabled(PanelPart::Wifi,data))metrics.Set(Control::Wifi,data.wifi==1?0:1);break;
        case PageHit::Row:
            if(wifi.expanded==target.key){wifi.Wipe();wifi.expanded.clear();break;}
            wifi.Wipe();wifi.expanded=target.key;
            if(WifiLayout open(data.wifiList,wifi,popup.height,!data.error.empty());auto row=open.Find(target.key)){
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
    }
    void BluetoothClick(Sample const& data,PageTarget const& target){
        switch(target.hit){
        case PageHit::Toggle:if(PanelEnabled(PanelPart::Bluetooth,data))metrics.Set(Control::Bluetooth,data.bluetooth==1?0:1);break;
        case PageHit::Row:
            bluetooth.expanded=bluetooth.expanded==target.key?std::wstring():target.key;
            if(BluetoothLayout open(data.bluetoothList,bluetooth,popup.height,!data.error.empty());auto row=open.Find(target.key))
                bluetooth.scroll=open.Reveal(row->y,row->h,bluetooth.scroll);
            break;
        case PageHit::Primary:
            if(!data.bluetoothList.busy.empty())break;
            for(auto const& d:data.bluetoothList.devices)if(d.id==target.key)metrics.Send(Control::BluetoothConnect,d.connected?0:1,d.id);
            break;
        default:break;
        }
    }
    void HotspotClick(Sample const& data,PageTarget const& target){
        switch(target.hit){
        case PageHit::Toggle:if(PanelEnabled(PanelPart::Hotspot,data))metrics.Set(Control::Hotspot,data.hotspot==1?0:1);break;
        case PageHit::Reveal:
            if(hotspot.editing)hotspot.reveal=!hotspot.reveal;
            else if(data.hotspotInfo.ssid.empty())break;
            else if(hotspot.reveal)hotspot.Hide();
            else{metrics.HotspotKey(hotspot.shown);hotspot.reveal=true;}
            break;
        case PageHit::Secondary:if(hotspot.editing){if(!hotspot.saving)hotspot.Reset();}else EditHotspot(data);break;
        case PageHit::Name:Focus(HotspotView::Focus::Name);break;
        case PageHit::Field:Focus(HotspotView::Focus::Password);break;
        case PageHit::Option:if(const int band=target.key[0]-L'0';data.hotspotInfo.bands[size_t(band)])hotspot.band=band;break;
        case PageHit::Primary:SaveHotspot(data);break;
        default:hotspot.focus=HotspotView::Focus::None;break;
        }
    }
    void SoundClick(Sample const& data,PageTarget const& target){
        switch(target.hit){
        case PageHit::Mute:
            if(target.key.empty()){if(data.volume>=0)metrics.Set(Control::Mute,!data.mute);break;}
            for(auto const& app:data.sound.apps)if(app.key==target.key)metrics.Send(Control::AppMute,app.mute?0:1,app.key);
            break;
        case PageHit::Row:metrics.Send(Control::OutputDevice,0,target.key);break;
        default:break;
        }
    }
    void PageInput(UINT message,float x,float y){
        const auto data=metrics.Read();const auto now=GetTickCount64();
        if(drag.active){
            if(message==WM_MOUSEMOVE){popup.scene.Light(x,y);if(drag.Send(x,now))SendDrag();Paint();}
            else if(message==WM_LBUTTONUP){
                if(drag.Send(x,now,true))SendDrag();
                drag.Cancel();if(GetCapture()==popup.window)ReleaseCapture();Paint();
            }
            return;
        }
        const auto target=PageHitAt(data,x,y);
        if(message==WM_MOUSEMOVE){
            popup.scene.Light(x,y);
            if(!(target==pageHover)){pageHover=target;pageFade.Snap(0);if(target.hit!=PageHit::None)pageFade.Go(1,now,120);Paint();}
            return;
        }
        if(message==WM_LBUTTONDOWN){
            // Sliders follow the pointer from the press, as on the main page.
            if(page==PanelPage::Sound&&target.hit==PageHit::Slider){
                const bool master=target.key.empty();
                const bool enabled=master?data.volume>=0:std::any_of(data.sound.apps.begin(),data.sound.apps.end(),
                    [&](AudioApp const& a){return a.key==target.key&&a.volume>=0;});
                if(!enabled)return;
                drag.Begin(target.key,master?SliderLeft:AppSliderLeft,SoundRight,x);SetCapture(popup.window);
                if(drag.Send(x,now))SendDrag();Paint();return;
            }
            Pin();pagePressed=target;SetCapture(popup.window);return;
        }
        if(message!=WM_LBUTTONUP)return;
        const auto pressed=pagePressed;pagePressed={};
        if(GetCapture()==popup.window)ReleaseCapture();
        if(!(pressed==target))return;
        if(target.hit==PageHit::Back){Navigate(PanelPage::Main);return;}
        switch(page){
        case PanelPage::Wifi:WifiClick(data,target);break;
        case PanelPage::Bluetooth:BluetoothClick(data,target);break;
        case PanelPage::Hotspot:HotspotClick(data,target);break;
        case PanelPage::Sound:SoundClick(data,target);break;
        default:break;
        }
        Paint();
    }
    void Input(UINT message,float x,float y){
        if(page!=PanelPage::Main){PageInput(message,x,y);return;}
        const auto data=metrics.Read();const auto now=GetTickCount64();
        auto effective=data;
        effective.volume=volumeRequest.Display(data.volume,data.completed[unsigned(Control::Volume)],now);
        effective.brightness=brightnessRequest.Display(data.brightness,data.completed[unsigned(Control::Brightness)],now);
        const auto hit=PanelHit(x,y,PanelLayout(data,0).mediaY);
        if(message==WM_MOUSEMOVE){
            popup.scene.Light(x,y);
            if(gesture.Dragging()){
                if(gesture.Send(x,now))Set(gesture.pressed==PanelPart::Volume?Control::Volume:Control::Brightness,gesture.preview);
                Paint();
            }else if(hover!=hit){hover=hit;Paint();}return;
        }
        // Dragging a slider is not a click: adjust and move away still closes.
        if(message==WM_LBUTTONDOWN&&!PanelSlider(hit))Pin();
        if(message==WM_LBUTTONDOWN&&PanelEnabled(hit,effective)){
            gesture.Begin(hit,x);SetCapture(popup.window);
            if(gesture.Send(x,now))Set(hit==PanelPart::Volume?Control::Volume:Control::Brightness,gesture.preview);
            Paint();return;
        }
        if(message!=WM_LBUTTONUP||gesture.pressed==PanelPart::None)return;
        const auto pressed=gesture.pressed;
        if(gesture.Dragging()){
            if(gesture.Send(x,now,true))Set(pressed==PanelPart::Volume?Control::Volume:Control::Brightness,gesture.preview);
        }else if(hit==pressed&&PanelEnabled(hit,effective))switch(hit){
            case PanelPart::Mute:metrics.Set(Control::Mute,!data.mute);break;
            case PanelPart::BrightnessToggle:{
                const int next=ToggleBrightness(effective.brightness,savedBrightness);
                Bridge::SetSavedBrightness(bar,savedBrightness);
                Set(Control::Brightness,next);break;
            }
            case PanelPart::Wifi:metrics.Set(Control::Wifi,data.wifi==1?0:1);break;
            case PanelPart::Bluetooth:metrics.Set(Control::Bluetooth,data.bluetooth==1?0:1);break;
            case PanelPart::DarkTheme:metrics.Set(Control::DarkTheme,data.darkTheme==1?0:1);break;
            case PanelPart::Hotspot:metrics.Set(Control::Hotspot,data.hotspot==1?0:1);break;
            case PanelPart::WifiMore:Navigate(PanelPage::Wifi);break;
            case PanelPart::BluetoothMore:Navigate(PanelPage::Bluetooth);break;
            case PanelPart::HotspotMore:Navigate(PanelPage::Hotspot);break;
            case PanelPart::VolumeMore:Navigate(PanelPage::Sound);break;
            case PanelPart::MediaPrev:metrics.Send(Control::MediaPrev,0);break;
            case PanelPart::MediaToggle:metrics.Send(Control::MediaToggle,0);break;
            case PanelPart::MediaNext:metrics.Send(Control::MediaNext,0);break;
            default:break;
        }
        Cancel();if(GetCapture()==popup.window)ReleaseCapture();Paint();
    }
    bool ShouldActivate(UINT triggerMsg,float x,float y){
        if(closing||!revealed)return false;
        if(triggerMsg!=WM_LBUTTONDOWN&&triggerMsg!=WM_LBUTTONDBLCLK)return stay.clicked;
        if(!stay.clicked&&page==PanelPage::Main&&PanelSlider(PanelHit(x,y)))return false;
        if(HWND fg=GetForegroundWindow();fg&&fg!=popup.window){
            previousForeground=fg;
            if(!foreground)foreground=fg;
        }
        return true;
    }
    void Pin(){
        stay.clicked=true;
        InstallMouseHook();
        if(GetForegroundWindow()!=popup.window){
            if(HWND fg=GetForegroundWindow();fg&&fg!=popup.window){
                previousForeground=fg;
                if(!foreground)foreground=fg;
            }
            SetForegroundWindow(popup.window);
        }
        if(GetForegroundWindow()==popup.window)activated=true;
    }
    void Deactivate(HWND next){
        if(!stay.clicked||closing)return;
        if(next==popup.window||(control&&next==control))return;
        Quit(true);
    }
    // A press since the last tick that the panel did not receive. The low bit
    // catches a tap between ticks, so this runs every tick: otherwise a press
    // made on the panel would still be set when the pointer has left it.
    bool PressedElsewhere(){
        bool pressed=false;
        for(int i=0;i<3;++i){
            const int vk=i==0?VK_LBUTTON:(i==1?VK_RBUTTON:VK_MBUTTON);
            const auto state=GetAsyncKeyState(vk);
            const bool down=(state&0x8000)!=0;
            if((state&1)||(down&&!buttonDown[i]))pressed=true;
            buttonDown[i]=down;
        }
        const bool own=panelPress;panelPress=false;
        return pressed&&!own;
    }
    // A panel that stays must also go when another window comes forward: a
    // button read can miss a press aimed at a window of higher integrity.
    bool WindowSwitched(){
        const HWND now=GetForegroundWindow();
        if(!stay.clicked||!now||now==popup.window)return false;
        if(activated)return true;
        if(!foreground)foreground=now;
        return now!=foreground;
    }
    void Tick(){
        if(closing||keepalive)return;
        if(!Valid()||!IsWindowVisible(bar)||std::abs(GetDpiForWindow(bar)/96.f-dpi)>.01f){Quit();return;}
        // Escape closes the panel unless it was spent leaving a text field.
        const bool typing=Typing();
        if(!(GetAsyncKeyState(VK_ESCAPE)&0x8000))escapeHeld=false;
        else if(!escapeHeld&&!typing){Quit(true,true);return;}
        if(!revealed){
            if(Bridge::Revealed(bar,cookie))Reveal();
            else if(GetTickCount64()-startedAt>=StandbyLimit){Quit();return;}
        }
        RECT anchor{};POINT cursor{};
        if(!Bridge::Anchor(bar,anchor)||!GetCursorPos(&cursor)){Quit();return;}
        Position();popup.Hover(cursor);
        // Inside the panel WM_MOUSEMOVE moves the light more finely.
        if(!popup.Hit(cursor.x,cursor.y))Light(cursor);
        // Before the reveal only the key counts: a pass-over ends the standby, with
        // bottom-edge slack while an auto-hidden Dock finishes sliding up.
        // Once shown, the way up from the key to the panel counts as the key.
        RECT reach=anchor;
        if(revealed)reach.top=std::min(reach.top,LONG(popup.rect.y+popup.rect.h));
        else reach.bottom+=LONG(std::lround(16*dpi));
        const bool inside=PtInRect(&reach,cursor)||popup.Hit(cursor.x,cursor.y)||GetCapture()==popup.window;
        if(typing&&GetForegroundWindow()!=popup.window){Unfocus();Paint();}
        const bool pressed=PressedElsewhere();
        const bool pressOutside=revealed&&pressed&&!popup.Hit(cursor.x,cursor.y)&&GetCapture()!=popup.window;
        if(WindowSwitched()||pressOutside||stay.Close(inside,pressed,typing,GetTickCount64())){Quit(true);return;}
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
                if(a==KeepaliveTimer)self->ExpireKeepalive();
                else if(a==PrimeTimer){KillTimer(window,PrimeTimer);if(self->keepalive)self->metrics.Visible(false);}
                else if(a==LeaveTimer)self->Finish();else if(a==FrameTimer)self->Paint();else self->Tick();
                return 0;
            }
            if(message==WM_APP+28){self->Paint();return 0;}
            if(message==Bridge::PrimeMessage){self->Prime();return 0;}
            if(message==Bridge::RevealMessage){self->Reveal();return 0;}
            if(message==DismissMessage){if(self->revealed&&!self->closing&&GetCapture()!=self->popup.window)self->Quit(true);return 0;}
            if(message==WM_SETTINGCHANGE||message==WM_THEMECHANGED){
                self->draw.colors.Refresh();self->popup.scene.Theme();
                if(self->keepalive)self->metrics.RefreshControls();
                else self->Paint();
                return 0;
            }
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
        savedBrightness=Bridge::SavedBrightness(bar);
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
        popup.click=[this](UINT m,float x,float y){if(m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN)panelPress=true;Input(m,x,y);};
        popup.pointerCancel=[this]{Cancel();pagePressed={};};popup.cancel=[this]{Quit();};
        popup.escape=[this]{Quit(true,true);};
        popup.shouldActivate=[this](UINT m,float x,float y){return ShouldActivate(m,x,y);};
        popup.deactivate=[this](HWND next){Deactivate(next);};
        popup.key=[this](UINT m,WPARAM k){return Key(m,k);};
        popup.wheel=[this](int delta){
            const float step=float(delta)/WHEEL_DELTA*48;
            switch(page){
            case PanelPage::Wifi:wifi.scroll-=step;break;
            case PanelPage::Bluetooth:bluetooth.scroll-=step;break;
            case PanelPage::Sound:sound.scroll-=step;break;
            default:return;
            }
            Paint();};
        if(!Valid())return 22;
        SetPropW(bar,Bridge::ControlProp,control);SetPropW(bar,Bridge::WindowProp,popup.window);
        // Drain sticky GetAsyncKeyState low bits before the first standby Tick runs:
        // after long maximized-app idle or PC wake, cold launch would otherwise read
        // a stale click bit from minutes ago and quit before RevealDelay elapses.
        (void)PressedElsewhere();
        escapeHeld=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
        Paint();if(closing)return 23;
        if(!SetTimer(control,TickTimer,50,nullptr))winrt::throw_last_error();
        // The owner sets the reveal property before looking for this control
        // window, so checking after publishing it cannot miss a reveal.
        Reveal();MSG msg{};
        while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
        WTSUnRegisterSessionNotification(control);Close();Log(L"panel-closed");return 0;
    }
};
inline LRESULT CALLBACK Panel::MouseHookProc(int code,WPARAM wParam,LPARAM lParam){
    if(code==HC_ACTION&&g_activeHookPanel&&
       (wParam==WM_LBUTTONDOWN||wParam==WM_RBUTTONDOWN||wParam==WM_MBUTTONDOWN||wParam==WM_XBUTTONDOWN)){
        auto const* info=reinterpret_cast<MSLLHOOKSTRUCT const*>(lParam);
        auto* self=g_activeHookPanel;
        if(self->revealed&&!self->closing&&!self->popup.Hit(info->pt.x,info->pt.y)&&
           GetCapture()!=self->popup.window){
            PostMessageW(self->control,Panel::DismissMessage,0,0);
        }
    }
    return CallNextHookEx(nullptr,code,wParam,lParam);
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try{
        int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);int result=2;
        if(count==2&&!wcscmp(args[1],L"--hidden-probe")){
            LocalFree(args);winrt::init_apartment(winrt::apartment_type::single_threaded);
            {Capsule::Drawing draw;Capsule::Popup popup;popup.Init(draw,1,300,408,14,false);
             Capsule::RenderControlPanel(draw);Capsule::RenderWifiPage(draw);Capsule::RenderPages(draw);
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
