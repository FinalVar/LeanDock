#pragma once
#include "apps-latest.h"
#include "metrics-latest.h"
#include "popup-latest.h"
#include "tray-latest.h"
#include <powrprof.h>
#include <unordered_map>
#include "../include/start-menu-state-latest.h"
#include "icon-alpha-tests-latest.h"
#include "time-key-latest.h"
#include "control-panel-latest.h"

namespace Capsule {
inline void Shortcut(WORD key) {
    INPUT keys[4]{};for(auto& i:keys)i.type=INPUT_KEYBOARD;
    keys[0].ki.wVk=VK_LWIN;
    if(key){keys[1].ki.wVk=key;keys[2].ki.wVk=key;keys[2].ki.dwFlags=KEYEVENTF_KEYUP;
        keys[3].ki.wVk=VK_LWIN;keys[3].ki.dwFlags=KEYEVENTF_KEYUP;SendInput(4,keys,sizeof(INPUT));}
    else{keys[1]=keys[0];keys[1].ki.dwFlags=KEYEVENTF_KEYUP;SendInput(2,keys,sizeof(INPUT));}
}
struct AppVisual {
    std::wstring key;
    comp::ContainerVisual container{nullptr},icon{nullptr};
    comp::SpriteVisual dot{nullptr};
    float x=0;
};
class Dashboard {
    Scene& scene;
    Drawing& draw;
    HWND window{};
    float dpi=1,full=400,base=120,infoX=0,timeX=0,timeWidth=TimeWidth;
    float separatorX=-1;
    float appPitch=AppPitch,iconSize=IconSize;
    Rect dock{};
    MONITORINFO monitor{sizeof(monitor)};
    Apps apps;
    Power power;
    Metrics metrics;
    Tray tray;
    std::vector<AppVisual> visuals;
    std::vector<std::pair<comp::Visual,uint64_t>> retiring;
    std::unordered_map<std::wstring,AppArtwork> cache;
    comp::SpriteVisual timeVisual{nullptr};
    comp::ContainerVisual timeContainer{nullptr};
    comp::CompositionColorBrush timeTint{nullptr};
    std::unique_ptr<Popup> detail,menu,submenu,tooltip;
    std::vector<size_t> traySlots;
    PanelGesture panelGesture;
    PanelPart panelHover=PanelPart::None;
    PanelValueRequest volumeRequest,brightnessRequest;
    uint64_t hoverAt=0,detailLeave=0,iconAt=0,minute=0;
    bool shown=false,infoHover=false,timeHover=false,timePressed=false;
    StartHold startHold;
    HWND startWindow{};
    bool startHolding=false;
    int hovered=-1,pressed=-1,hoveredTray=-1;
    POINT downPoint{};
    HWND previousForeground{};
    std::optional<App> menuApp;
    std::wstring pressedKey;
    std::function<void(float,float)> resize;
    std::function<void()> hide;
    unsigned shellMessage=0;
    HPOWERNOTIFY acNotify{},batteryNotify{};
    static constexpr GUID AcGuid{0x5d3e9a59,0xe9d5,0x4b00,{0xa6,0xbd,0xff,0x34,0xff,0x51,0x65,0x48}};
    static constexpr GUID PercentGuid{0xa7ad8041,0xb45a,0x4cae,{0x87,0xa3,0xee,0xcb,0xb4,0x68,0xa9,0xe1}};
    static std::wstring Percent(int n){return n<0?L"—":std::to_wstring(n)+L"%";}
    std::wstring Time(bool seconds=false)const{
        SYSTEMTIME t{};GetLocalTime(&t);wchar_t b[32]{};
        if(seconds)swprintf_s(b,L"%02u:%02u:%02u",t.wHour,t.wMinute,t.wSecond);
        else swprintf_s(b,L"%02u:%02u",t.wHour,t.wMinute);return b;
    }
    std::wstring Date()const{
        SYSTEMTIME t{};GetLocalTime(&t);return std::to_wstring(t.wMonth)+L"/"+std::to_wstring(t.wDay);
    }
    void AnimateScalar(comp::CompositionObject const& target,const wchar_t* property,float end,int ms,
                       int delay=0,bool linear=false){
        if(!Animations()){if(auto visual=target.try_as<comp::Visual>();visual&&wcscmp(property,L"Opacity")==0)
            visual.Opacity(end);return;}
        auto a=scene.compositor.CreateScalarKeyFrameAnimation();
        a.InsertExpressionKeyFrame(0,L"this.StartingValue");
        comp::CompositionEasingFunction ease=linear?scene.compositor.CreateLinearEasingFunction().as<comp::CompositionEasingFunction>():
            scene.compositor.CreateCubicBezierEasingFunction({.34f,1.42f},{.55f,1}).as<comp::CompositionEasingFunction>();
        a.InsertKeyFrame(1,end,ease);a.Duration(std::chrono::milliseconds(ms));a.DelayTime(std::chrono::milliseconds(delay));
        a.DelayBehavior(comp::AnimationDelayBehavior::SetInitialValueBeforeDelay);target.StartAnimation(property,a);
    }
    void AnimateVector(comp::Visual const& target,const wchar_t* property,num::float3 end,int ms,int delay=0){
        if(!Animations()){if(!wcscmp(property,L"Scale"))target.Scale(end);else target.Offset(end);return;}
        auto a=scene.compositor.CreateVector3KeyFrameAnimation();a.InsertExpressionKeyFrame(0,L"this.StartingValue");
        auto ease=scene.compositor.CreateCubicBezierEasingFunction({.34f,1.42f},{.55f,1});
        a.InsertKeyFrame(1,end,ease);a.Duration(std::chrono::milliseconds(ms));a.DelayTime(std::chrono::milliseconds(delay));
        a.DelayBehavior(comp::AnimationDelayBehavior::SetInitialValueBeforeDelay);target.StartAnimation(property,a);
    }
    AppArtwork AppImage(App const& a) {
        auto it=cache.find(a.key);if(it!=cache.end())return it->second;
        winrt::com_ptr<IShellItemImageFactory> factory;
        HRESULT hr=E_FAIL;
        if(!a.aumid.empty()){
            auto parse=L"shell:AppsFolder\\"+a.aumid;
            hr=SHCreateItemFromParsingName(parse.c_str(),nullptr,__uuidof(IShellItemImageFactory),factory.put_void());
        }
        if(FAILED(hr)&&a.pidl)hr=SHCreateItemFromIDList(a.pidl.get(),__uuidof(IShellItemImageFactory),factory.put_void());
        if(FAILED(hr))hr=SHCreateItemFromParsingName(a.parse.c_str(),nullptr,__uuidof(IShellItemImageFactory),factory.put_void());
        winrt::com_ptr<IWICBitmap> result;HBITMAP bitmap{};
        // Shell icon artwork, never an executable thumbnail. Request the
        // standard 256 px resource tier and retain its actual returned size.
        // Do not synthesize detail by asking Shell to SCALEUP small resources.
        int n=std::max(256,int(std::ceil(IconSize*2*dpi)));
        if(SUCCEEDED(hr)&&SUCCEEDED(factory->GetImage({n,n},SIIGBF_ICONONLY|SIIGBF_BIGGERSIZEOK,&bitmap))&&bitmap){
            result=draw.Bitmap(bitmap);DeleteObject(bitmap);
        }
        if(!result&&a.recent){
            DWORD_PTR icon=0;SendMessageTimeoutW(a.recent,WM_GETICON,ICON_BIG,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,30,&icon);
            if(!icon)icon=GetClassLongPtrW(a.recent,GCLP_HICON);
            if(icon)result=draw.Icon(reinterpret_cast<HICON>(icon));
        }
        if(!result){
            // System file-type icon, never a synthetic/random program symbol.
            SHFILEINFOW file{};if(SHGetFileInfoW(a.exe.c_str(),FILE_ATTRIBUTE_NORMAL,&file,sizeof(file),
                SHGFI_ICON|SHGFI_LARGEICON|SHGFI_USEFILEATTRIBUTES)){result=draw.Icon(file.hIcon);DestroyIcon(file.hIcon);}
        }
        auto artwork=draw.Artwork(result);cache[a.key]=artwork;return artwork;
    }
    void SetSurface(comp::SpriteVisual const& v,comp::CompositionDrawingSurface const& surface) {
        auto brush=scene.compositor.CreateSurfaceBrush(surface);brush.Stretch(comp::CompositionStretch::Fill);v.Brush(brush);
    }
    void Rebuild(bool recreate=false) {
        auto previous=std::move(visuals);
        for(auto it=cache.begin();it!=cache.end();){
            if(std::none_of(apps.list.begin(),apps.list.end(),[&](auto const& a){return a.key==it->first;}))it=cache.erase(it);
            else ++it;
        }
        timeWidth=TimeWidth;
        const float infoWidth=timeWidth;
        base=AppPadding+infoWidth+TimeInset;float x=AppPadding;separatorX=-1;
        const float available=float(monitor.rcMonitor.right-monitor.rcMonitor.left)/dpi-16;
        const bool hasUnpinned=std::any_of(apps.list.begin(),apps.list.end(),[](auto const& a){return !a.pinned;});
        const float nextPitch=FitAppPitch(available-base-(apps.list.empty()?0:6)-(hasUnpinned?9:0),unsigned(apps.list.size()),appPitch);
        recreate=recreate||nextPitch!=appPitch;appPitch=nextPitch;iconSize=IconSize*appPitch/AppPitch;
        const float inset=(appPitch-iconSize)/2,iconTop=(Height-iconSize)/2;
        bool separated=false;
        std::vector<float> offsets;
        for(size_t i=0;i<apps.list.size();i++){
            if(!apps.list[i].pinned&&!separated){separatorX=x;separated=true;x+=9;}
            offsets.push_back(x);x+=appPitch;
        }
        infoX=x+(apps.list.empty()?0:6);full=infoX+infoWidth+TimeInset;timeX=full-TimeInset-timeWidth;
        if(full*dpi>monitor.rcMonitor.right-monitor.rcMonitor.left-16*dpi)
            winrt::throw_hresult(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
        scene.Dimensions(full,base);
        resize(full,base);
        scene.content.Children().RemoveAll();visuals.clear();
        for(auto const& [v,until]:retiring)scene.content.Children().InsertAtTop(v);
        if(separatorX>=0){
            auto surface=draw.Surface(1,Height/2,dpi,[&](ID2D1DeviceContext* dc){draw.Line(dc,.5f,0,.5f,Height/2,draw.colors.hair);});
            auto v=draw.Visual(scene.compositor,surface,1,Height/2);v.Offset({separatorX,Height/4,0});
            scene.content.Children().InsertAtTop(v);
        }
        for(size_t i=0;i<apps.list.size();i++){
            auto const& a=apps.list[i];auto image=AppImage(a);
            AppVisual v;v.key=a.key;v.x=offsets[i];
            auto old=std::find_if(previous.begin(),previous.end(),[&](auto const& p){return p.key==a.key;});
            bool keep=!recreate&&old!=previous.end();
            if(keep){v=*old;v.x=offsets[i];previous.erase(old);
                AnimateVector(v.container,L"Offset",{std::round((v.x+inset)*dpi)/dpi,std::round(iconTop*dpi)/dpi,0},300);
            }else{
                v.container=scene.compositor.CreateContainerVisual();v.container.Offset({std::round((v.x+inset)*dpi)/dpi,std::round(iconTop*dpi)/dpi,0});
                v.icon=draw.AppIcon(scene.compositor,image,iconSize,dpi);
                v.container.Children().InsertAtTop(v.icon);
                if(shown&&!recreate){
                    v.icon.Opacity(0);v.icon.Scale({.55f,.55f,1});
                    AnimateScalar(v.icon,L"Opacity",1,300,0,true);AnimateVector(v.icon,L"Scale",{1,1,1},300);
                }
            }
            auto dot=draw.Surface(DotSize,DotSize,dpi,[&](ID2D1DeviceContext* dc){auto c=draw.colors.text;c.a=.55f;draw.Round(dc,{0,0,DotSize,DotSize},DotSize/2,c);});
            if(!keep){v.dot=draw.Visual(scene.compositor,dot,DotSize,DotSize);v.dot.Offset({(iconSize-DotSize)/2,Height-DotBottom-DotSize-iconTop,0});
                v.dot.Opacity(shown?0:(a.windows.empty()?0:1));v.container.Children().InsertAtTop(v.dot);}
            else SetSurface(v.dot,dot);
            AnimateScalar(v.dot,L"Opacity",a.windows.empty()?0:1,300,0,true);
            scene.content.Children().InsertAtTop(v.container);
            visuals.push_back(v);
        }
        if(shown&&!recreate)for(auto& old:previous){
            scene.content.Children().InsertAtTop(old.container);
            AnimateScalar(old.container,L"Opacity",0,300,0,true);
            AnimateVector(old.icon,L"Scale",{.55f,.55f,1},300);
            retiring.push_back({old.container,GetTickCount64()+320});
        }
        auto blank=draw.Surface(timeWidth,TimeHeight,dpi,[](auto*){});
        timeVisual=draw.Visual(scene.compositor,blank,timeWidth,TimeHeight);
        timeContainer=scene.compositor.CreateContainerVisual();
        timeContainer.Offset({timeX,TimeInset,0});timeContainer.CenterPoint({timeWidth/2,TimeHeight/2,0});
        auto maskSurface=draw.Surface(timeWidth,TimeHeight,dpi,[&](auto* dc){draw.Round(dc,{0,0,timeWidth,TimeHeight},TimeHeight/2,{1,1,1,1});});
        auto mask=scene.compositor.CreateMaskBrush();
        auto maskBrush=scene.compositor.CreateSurfaceBrush(maskSurface);maskBrush.Stretch(comp::CompositionStretch::Fill);
        mask.Mask(maskBrush);
        timeTint=scene.compositor.CreateColorBrush();mask.Source(timeTint);
        auto background=scene.compositor.CreateSpriteVisual();background.Size({timeWidth,TimeHeight});background.Brush(mask);
        timeContainer.Children().InsertAtTop(background);timeContainer.Children().InsertAtTop(timeVisual);
        scene.content.Children().InsertAtTop(timeContainer);TintTime(false);
        PaintTime();
        SetPropW(window,L"Capsule.TimeKeyVersion",HANDLE(1));
        SetPropW(window,L"Capsule.TimeKeyWidthDip",HANDLE(uintptr_t(TimeWidth)));
        SetPropW(window,L"Capsule.TimeKeyHeightDip",HANDLE(uintptr_t(TimeHeight)));
        hovered=-1;if(tooltip)tooltip->Hide();
        SetPropW(window,L"Capsule.AppCount",HANDLE(uintptr_t(apps.list.size()+1)));
        SetPropW(window,L"Capsule.PinnedCount",HANDLE(uintptr_t(std::count_if(apps.list.begin(),apps.list.end(),
            [](auto const& a){return a.pinned;})+1)));
        SetPropW(window,L"Capsule.HeightDip",HANDLE(uintptr_t(Height)));
        SetPropW(window,L"Capsule.RadiusDip",HANDLE(uintptr_t(Radius)));
        SetPropW(window,L"Capsule.IconSize100",HANDLE(uintptr_t(std::lround(iconSize*100))));
        SetPropW(window,L"Capsule.AppPitchDip",HANDLE(uintptr_t(appPitch)));
        UINT smallest=UINT_MAX,largest=0;
        for(auto const& app:apps.list){auto const& image=cache[app.key];
            if(image.width&&image.height){smallest=std::min(smallest,std::min(image.width,image.height));largest=std::max(largest,std::max(image.width,image.height));}}
        SetPropW(window,L"Capsule.IconSourcePx",HANDLE(uintptr_t(smallest==UINT_MAX?0:smallest)));
        SetPropW(window,L"Capsule.IconSourceMaxPx",HANDLE(uintptr_t(largest)));
    }
    void PaintTime(){
        if(!timeVisual)return;
        const int percent=power.Present()?power.value.BatteryLifePercent:-1;
        auto surface=draw.Surface(timeWidth,TimeHeight,dpi,[&](ID2D1DeviceContext* dc){
            PaintTimeKey(draw,dc,Time(),Date(),percent);
        });SetSurface(timeVisual,surface);
        SetPropW(window,L"Capsule.TimeKeyBatteryPlusOne",HANDLE(uintptr_t(percent+1)));
    }
    void TintTime(bool animate=true){
        if(!timeTint)return;
        auto c=draw.colors.hover;c.a=timeHover?c.a:0;
        Color color{uint8_t(std::lround(c.a*255)),uint8_t(std::lround(c.r*255)),
            uint8_t(std::lround(c.g*255)),uint8_t(std::lround(c.b*255))};
        if(!animate||!Animations()){timeTint.StopAnimation(L"Color");timeTint.Color(color);return;}
        auto a=scene.compositor.CreateColorKeyFrameAnimation();a.InsertExpressionKeyFrame(0,L"this.StartingValue");
        a.InsertKeyFrame(1,color);a.Duration(std::chrono::milliseconds(150));timeTint.StartAnimation(L"Color",a);
    }
    bool PopupHit(POINT p)const{
        for(auto q:{detail.get(),menu.get(),submenu.get(),tooltip.get()})if(q&&q->Hit(p.x,p.y))return true;
        return false;
    }
    int AppAt(float x,float y)const{
        if(y<0||y>=Height)return -1;
        for(int i=0;i<int(visuals.size());i++)if(x>=visuals[i].x&&x<visuals[i].x+appPitch)return i;
        return -1;
    }
    void HideDetail(){if(detail)detail->Hide();metrics.Visible(false);infoHover=false;hoverAt=detailLeave=0;
        volumeRequest={};brightnessRequest={};}
    void CloseMenus(bool restoreFocus=true) {
        auto current=GetForegroundWindow();
        bool ours=(menu&&current==menu->window)||(submenu&&current==submenu->window);
        if(menu)menu->Hide();if(submenu)submenu->Hide();
        menuApp.reset();
        if(restoreFocus&&ours&&previousForeground&&IsWindow(previousForeground))SetForegroundWindow(previousForeground);
        previousForeground=nullptr;
    }
    void ShowMenu(std::vector<MenuItem> items,double left,double bottom,bool app=false) {
        HideDetail();if(tooltip)tooltip->Hide();
        CloseMenus(false);previousForeground=GetForegroundWindow();
        float width=190;
        for(auto const& i:items)width=std::max(width,draw.Measure(i.text,13)+30+(i.submenu?15:0));
        width=std::min(width,340.f);
        menu.reset();menu=std::make_unique<Popup>();
        menu->Init(scene,draw,dpi,width,MenuHeight(items),10,true);menu->items=std::move(items);
        double x=app?left-width*dpi/2:left-width*dpi;
        x=std::clamp(x,double(monitor.rcMonitor.left+8*dpi),double(monitor.rcMonitor.right-(width+8)*dpi));
        menu->Position(x,bottom-menu->height*dpi);
        menu->select=[this](int action){Execute(action);};
        menu->cancel=[this]{CloseMenus();};
        menu->hovering=[this](int index){
            if(index>=0&&menu->items[index].submenu)ShowPowerMenu();
        };
        menu->PaintMenu();menu->Show();
    }
    void ShowPowerMenu(){
        if(!menu||!menu->visible)return;
        if(submenu&&submenu->visible)return;
        std::vector<MenuItem> items={{L"注销",201},{L"睡眠",202},{L"关机",203},{L"重启",204}};
        submenu.reset();submenu=std::make_unique<Popup>();
        submenu->Init(scene,draw,dpi,190,MenuHeight(items),10,true);submenu->items=std::move(items);
        int index=0;for(int i=0;i<int(menu->items.size());i++)if(menu->items[i].submenu)index=i;
        auto y=menu->rect.y+(menu->ItemY(index)-5)*dpi;
        y=std::clamp(y,double(monitor.rcMonitor.top+8*dpi),double(monitor.rcMonitor.bottom-(submenu->height+8)*dpi));
        submenu->Position(std::max(double(monitor.rcMonitor.left+8*dpi),menu->rect.x-190*dpi-4*dpi),y);
        submenu->select=[this](int a){Execute(a);};submenu->cancel=[this]{submenu->Hide();if(menu)SetForegroundWindow(menu->window);};
        submenu->PaintMenu();submenu->Show();
    }
    bool Open(std::wstring file,std::wstring args=L"",bool admin=false){
        SHELLEXECUTEINFOW s{sizeof(s)};s.hwnd=window;s.lpVerb=admin?L"runas":L"open";
        s.lpFile=file.c_str();s.lpParameters=args.empty()?nullptr:args.c_str();s.nShow=SW_SHOWNORMAL;
        return ShellExecuteExW(&s)!=FALSE;
    }
    void Execute(int action){
        if(action==199){ShowPowerMenu();return;}
        auto selected=menuApp;CloseMenus();
        bool ok=true;
        if(action>=300&&selected){
            switch(action){
            case 301:ok=apps.Launch(*selected,window);break;
            case 303:apps.CloseWindows(*selected);break;
            case 304:ok=apps.End(*selected);break;
            }if(!ok)Log(L"app-action-failed",action);return;
        }
        switch(action){
        case 1:ok=Open(L"ms-settings:appsfeatures");break;
        case 2:ok=Open(L"mblctr.exe");break;
        case 3:ok=Open(L"ms-settings:powersleep");break;
        case 4:ok=Open(L"eventvwr.msc");break;
        case 5:ok=Open(L"ms-settings:about");break;
        case 6:ok=Open(L"devmgmt.msc");break;
        case 7:ok=Open(L"ms-settings:network");break;
        case 8:ok=Open(L"diskmgmt.msc");break;
        case 9:ok=Open(L"compmgmt.msc");break;
        case 10:case 11:{
            wchar_t path[32768]{};auto terminal=SearchPathW(nullptr,L"wt.exe",nullptr,32768,path,nullptr)?path:L"powershell.exe";
            ok=Open(terminal,L"",action==11);break;
        }
        case 12:ok=Open(L"taskmgr.exe");break;
        case 13:ok=Open(L"ms-settings:");break;
        case 14:ok=Open(L"explorer.exe");break;
        case 15:hide();Shortcut('S');break;
        case 16:hide();Shortcut('R');break;
        case 200:hide();Shortcut('D');break;
        case 201:ExitWindowsEx(EWX_LOGOFF,0);break;
        case 202:SetSuspendState(FALSE,FALSE,FALSE);break;
        case 203:ok=Open(L"shutdown.exe",L"/s /hybrid /t 0");break;
        case 204:ok=Open(L"shutdown.exe",L"/r /t 0");break;
        case 299:PostMessageW(window,WM_CLOSE,0,0);break;
        }
        if(!ok)Log(L"menu-action-failed",action);
    }
    void StartMenu(){
        std::vector<MenuItem> rows={{L"已安装的应用",1},{L"移动中心",2},{L"电源选项",3},{L"事件查看器",4},
            {L"系统",5},{L"设备管理器",6},{L"网络连接",7},{L"磁盘管理",8},{L"计算机管理",9},
            {L"终端",10},{L"终端（管理员）",11},{L"",0,true},
            {L"任务管理器",12},{L"设置",13},{L"文件资源管理器",14},{L"搜索",15},{L"运行",16},
            {L"",0,true},{L"关机或注销",199,false,false,false,true},{L"桌面",200},
            {L"",0,true},{L"退出胶囊任务栏",299}};
        ShowMenu(std::move(rows),dock.x+dock.w,dock.y-PopupGap*dpi);
    }
    void AppMenu(int i) {
        auto app=apps.list.at(i);
        std::vector<MenuItem> rows={{app.name,0,false,true},{L"打开新窗口",301},
            {app.pinned?L"从任务栏取消固定":L"固定到任务栏",302,false,!Apps::CanEditPins}};
        if(!app.windows.empty()){
            rows.push_back({L"",0,true});rows.push_back({L"关闭窗口",303});rows.push_back({L"结束任务",304,false,false,true});
        }
        ShowMenu(std::move(rows),dock.x+(visuals[i].x+appPitch/2)*dpi,dock.y-PopupGap*dpi,true);
        menuApp=app;
    }
    void Bounce(int index) {
        if(index<0||index>=int(visuals.size())||!Animations())return;
        auto& v=visuals[index];auto a=scene.compositor.CreateVector3KeyFrameAnimation();
        auto ease=scene.compositor.CreateCubicBezierEasingFunction({.34f,1.42f},{.55f,1});
        float y=hovered==index?-2:0;
        for(auto p:std::vector<std::pair<float,float>>{{0,0},{.3f,-9},{.55f,0},{.75f,-4},{1,0}})
            a.InsertKeyFrame(p.first,{0,y+p.second,0},ease);
        a.Duration(std::chrono::milliseconds(700));v.icon.StartAnimation(L"Offset",a);
    }
    void Tooltip(std::wstring const& text,double center,double bottom) {
        if(text.empty())return;
        float width=std::min(360.f,draw.Measure(text,13)+20);
        tooltip.reset();tooltip=std::make_unique<Popup>();
        tooltip->Init(scene,draw,dpi,width,28,10,false);
        double left=std::clamp(center-width*dpi/2,double(monitor.rcMonitor.left+8*dpi),
            double(monitor.rcMonitor.right-(width+8)*dpi));
        tooltip->Position(left,bottom-28*dpi);
        tooltip->Paint([&](auto* dc){draw.Text(dc,text,{10,0,width-20,28},13,draw.colors.text);});
        tooltip->Show();
    }
public:
    Dashboard(Scene& s,Drawing& d):scene(s),draw(d){}
    void Init(HWND w,float scale,MONITORINFO m,std::function<void(float,float)> size,std::function<void()> hideDock,bool isolated=false) {
        window=w;dpi=scale;monitor=m;resize=std::move(size);hide=std::move(hideDock);
        SetPropW(window,L"Capsule.ControlPanelVersion",HANDLE(3));
        power.Read();
        if(!apps.Refresh())winrt::throw_hresult(apps.pinResult);
        Rebuild();
        if(!isolated){
            apps.WatchPins();
            shellMessage=RegisterWindowMessageW(L"SHELLHOOK");RegisterShellHookWindow(window);
            acNotify=RegisterPowerSettingNotification(window,&AcGuid,DEVICE_NOTIFY_WINDOW_HANDLE);
            batteryNotify=RegisterPowerSettingNotification(window,&PercentGuid,DEVICE_NOTIFY_WINDOW_HANDLE);
            if(!acNotify||!batteryNotify)winrt::throw_last_error();
            metrics.Start(window);
            // Tray initialization is explicit after all windows/data are ready.
        }
    }
    bool StartTray(){bool ok=tray.Start(window);SetPropW(window,L"Capsule.TrayReady",HANDLE(uintptr_t(ok)));return ok;}
    // Own-surface verification only: no shell hooks, tray interception, input
    // or foreground changes. The same detail renderer reads real local data.
    void ProbeDetails(Rect r,MONITORINFO m) {
        Geometry(r,m);metrics.Start(window);ShowDetail();
    }
    void ProbeIdle(){apps.WatchPins();metrics.Start(window);}
    void ProbeIcons(){
        VerifyIconAlpha(draw);
        // Only file artifacts from application icon resources; no desktop
        // pixels, visible window, input registration, activation or tray use.
        const auto output=Directory()+L"\\icon-probe";std::filesystem::create_directories(output);
        auto save=[&](IWICBitmapSource* image,std::wstring const& name){
            winrt::com_ptr<IWICStream> stream;winrt::check_hresult(draw.wic->CreateStream(stream.put()));
            winrt::check_hresult(stream->InitializeFromFilename((output+L"\\"+name+L".png").c_str(),GENERIC_WRITE));
            winrt::com_ptr<IWICBitmapEncoder> encoder;
            winrt::check_hresult(draw.wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put()));
            winrt::check_hresult(encoder->Initialize(stream.get(),WICBitmapEncoderNoCache));
            winrt::com_ptr<IWICBitmapFrameEncode> frame;winrt::check_hresult(encoder->CreateNewFrame(frame.put(),nullptr));
            winrt::check_hresult(frame->Initialize(nullptr));winrt::check_hresult(frame->WriteSource(image,nullptr));
            winrt::check_hresult(frame->Commit());winrt::check_hresult(encoder->Commit());
        };
        FILE* report=_wfopen((output+L"\\latest.csv").c_str(),L"w");if(!report)winrt::throw_last_error();
        try{
            fprintf(report,"slot,sourceWidth,sourceHeight,scale,dpi,rasterWidth,rasterHeight,surfaceWidth,surfaceHeight,staticPixelAligned\n");
            for(size_t i=0;i<apps.list.size();++i){
                auto a=AppImage(apps.list[i]);if(!a.image||!a.width||!a.height)winrt::throw_hresult(E_FAIL);
                save(a.image.get(),std::to_wstring(i)+L"-source");
                const float factor=a.fit.valid?float(a.fit.scale):1,longest=float(std::max(a.width,a.height));
                for(float scale:{1.f,1.25f,1.5f,1.75f,2.f}){
                    const UINT pw=std::max(1u,UINT(std::lround(IconSize*factor*a.width/longest*scale)));
                    const UINT ph=std::max(1u,UINT(std::lround(IconSize*factor*a.height/longest*scale)));
                    auto raster=draw.RasterArtwork(a,pw,ph);
                    if(!ValidPremultiplied(draw,raster.get()))winrt::throw_hresult(E_UNEXPECTED);
                    auto icon=draw.AppIcon(scene.compositor,a,IconSize,scale);
                    auto visual=icon.Children().First().Current().as<comp::SpriteVisual>();
                    auto surface=visual.Brush().as<comp::CompositionSurfaceBrush>().Surface().as<comp::CompositionDrawingSurface>();
                    const auto pixels=surface.Size();const auto size=visual.Size();const auto offset=visual.Offset();
                    auto integer=[](float v){return std::abs(v-std::round(v))<.001f;};
                    const bool aligned=integer(offset.x*scale)&&integer(offset.y*scale)&&
                        std::abs(size.x*scale-pw)<.001f&&std::abs(size.y*scale-ph)<.001f;
                    fprintf(report,"%zu,%u,%u,%.5f,%.2f,%u,%u,%.0f,%.0f,%d\n",i,a.width,a.height,factor,scale,pw,ph,pixels.Width,pixels.Height,aligned);
                    if(pixels.Width!=pw||pixels.Height!=ph||!aligned)winrt::throw_hresult(E_UNEXPECTED);
                    if(scale==1.5f)save(raster.get(),std::to_wstring(i)+L"-raster-150");
                }
            }
            fclose(report);
        }catch(...){fclose(report);throw;}
    }
    Rect DetailRect()const{return detail?detail->rect:Rect{};}
    bool Shell(UINT message,WPARAM event) {
        if(message!=shellMessage||!shellMessage)return false;
        if(event==HSHELL_WINDOWCREATED||event==HSHELL_WINDOWDESTROYED||event==HSHELL_WINDOWACTIVATED||
           event==HSHELL_RUDEAPPACTIVATED||event==HSHELL_WINDOWREPLACED){
            SetTimer(window,3,120,nullptr);
        }return true;
    }
    void RefreshApps(){
        std::wstring old;
        for(auto const& a:apps.list)old+=a.key+(a.windows.empty()?L"0":L"1")+(a.pinned?L"P":L"R")+L"\n";
        if(!apps.Refresh()){Log(L"app-refresh-failed",apps.pinResult);return;}
        std::wstring now;
        for(auto const& a:apps.list)now+=a.key+(a.windows.empty()?L"0":L"1")+(a.pinned?L"P":L"R")+L"\n";
        if(old!=now)Rebuild();
    }
    void PowerChanged(){auto before=power.Percent();bool ac=power.AC();power.Read();
        // Geometry is fixed: battery updates must not rebuild/reanimate apps.
        if(before!=power.Percent()||ac!=power.AC()){
            if(shown)PaintTime();
            if(detail&&detail->visible)PaintDetail();
        }}
    void Geometry(Rect r,MONITORINFO m){
        dock=r;monitor=m;
        if(detail&&detail->visible){
            PositionDetail();
        }
    }
    void DPI(float scale){dpi=scale;cache.clear();CloseMenus(false);HideDetail();tooltip.reset();detail.reset();retiring.clear();Rebuild(true);}
    void Theme(){draw.colors.Refresh();scene.Theme();CloseMenus(false);HideDetail();tooltip.reset();detail.reset();retiring.clear();Rebuild(true);}
    void Shown(bool value){
        shown=value;
        if(!value){CancelInput();if(GetCapture()==window)ReleaseCapture();
            HideDetail();CloseMenus(false);if(tooltip)tooltip->Hide();return;}
        for(size_t i=0;i<visuals.size();i++){
            auto& v=visuals[i];v.icon.Opacity(Animations()?0:1);v.icon.Scale(Animations()?num::float3{.55f,.55f,1}:num::float3{1,1,1});
            int delay=60+int(visuals.size()-i)*16;
            AnimateScalar(v.icon,L"Opacity",1,300,delay,true);AnimateVector(v.icon,L"Scale",{1,1,1},300,delay);
        }
        // Refresh after hiding/sleep without a new polling timer.
        power.Read();PaintTime();
    }
    bool MenuOpen()const{return menu&&menu->visible;}
    bool DetailHold(POINT p)const{return detail&&detail->visible&&
        (GetCapture()==detail->window||detail->rect.Contains(p.x,p.y,8*dpi));}
    bool AnyHit(POINT p)const{return PopupHit(p);}
    bool TrayHold(){return tray.Holding();}
    bool StartHolding()const{return startHolding;}
    void Recreated(){tray.Recreated();apps.pins=nullptr;SetTimer(window,3,250,nullptr);}
    void TrayChanged(){if(detail&&detail->visible)PaintDetail();}
    void DevicesChanged(){metrics.RefreshControls();}
    void Poll(POINT p,bool buttonDownEdge) {
        if(startHold.requested){
            // Reuse the 20 ms input loop; no separate timer or process. A
            // cached HWND must still belong to the trusted shell host.
            if(startWindow&&(!IsWindow(startWindow)||!LeanDockStartState::Host(startWindow)))startWindow=nullptr;
            if(!startWindow)startWindow=LeanDockStartState::Visible();
            const bool next=startHold.Update(GetTickCount64(),LeanDockStartState::Shown(startWindow));
            if(next!=startHolding){startHolding=next;SetPropW(window,L"Capsule.StartHold",HANDLE(uintptr_t(next)));}
            if(!next)startWindow=nullptr;
        }
        if(apps.PinsChanged())SetTimer(window,3,150,nullptr);
        tray.Ensure();
        for(auto it=retiring.begin();it!=retiring.end();){
            if(GetTickCount64()>=it->second){scene.content.Children().Remove(it->first);it=retiring.erase(it);}
            else ++it;
        }
        if(timePressed&&GetCapture()!=window&&GetAsyncKeyState(VK_LBUTTON)>=0){
            AnimateVector(timeContainer,L"Scale",{1,1,1},200);timePressed=false;
        }
        for(auto q:{detail.get(),menu.get(),submenu.get(),tooltip.get()})if(q)q->Hover(p);
        if(buttonDownEdge&&MenuOpen()&&!((menu&&menu->Hit(p.x,p.y))||(submenu&&submenu->Hit(p.x,p.y)))){
            CloseMenus(false);if(!dock.Contains(p.x,p.y))return;
        }
        if(!shown)return;
        const auto unit=float(dock.w/full);
        float x=float(p.x-dock.x)/unit,y=float(p.y-dock.y)/unit;
        bool inInfo=dock.Contains(p.x,p.y)&&x>=infoX;
        bool onTime=inInfo&&x>=timeX&&x<full-TimeInset&&y>=TimeInset&&y<Height-TimeInset;
        if(onTime!=timeHover){timeHover=onTime;TintTime();}
        auto now=GetTickCount64();
        if(!MenuOpen()&&!startHolding){
            if(inInfo){
                detailLeave=0;if(!infoHover){infoHover=true;hoverAt=now;}
                if(now-hoverAt>=400&&(!detail||!detail->visible))ShowDetail();
            }else if(DetailHold(p)||tray.Holding()){detailLeave=0;}
            else{
                infoHover=false;hoverAt=0;
                if(detail&&detail->visible){if(!detailLeave)detailLeave=now;if(now-detailLeave>=250)HideDetail();}
            }
        }
        int icon=dock.Contains(p.x,p.y)?AppAt(x,y):-1;
        if(icon!=hovered){
            if(hovered>=0&&hovered<int(visuals.size()))AnimateVector(visuals[hovered].icon,L"Offset",{0,0,0},300);
            hovered=icon;iconAt=now;if(tooltip)tooltip->Hide();
            if(icon>=0)AnimateVector(visuals[icon].icon,L"Offset",{0,-2,0},300);
        }
        if(icon>=0&&!MenuOpen()&&now-iconAt>=600&&(!tooltip||!tooltip->visible))
            Tooltip(apps.list[icon].name,dock.x+(visuals[icon].x+appPitch/2)*dpi,dock.y-6*dpi);
        int trayHover=-1;
        if(detail&&detail->visible)for(auto index:traySlots)
            if(index<tray.icons.size()&&tray.icons[index].rect.Contains(p.x,p.y))trayHover=int(index);
        if(trayHover!=hoveredTray){hoveredTray=trayHover;iconAt=now;if(tooltip)tooltip->Hide();}
        if(trayHover>=0&&now-iconAt>=600&&(!tooltip||!tooltip->visible)){
            auto const& i=tray.icons[trayHover];Tooltip(i.tip,i.rect.x+i.rect.w/2,i.rect.y-6*dpi);
        }
    }
    void Mouse(UINT message,float x,float y) {
        int app=AppAt(x,y);bool time=x>=timeX&&x<full-TimeInset&&y>=TimeInset&&y<Height-TimeInset;
        if(message==WM_RBUTTONUP){
            if(app>=0)AppMenu(app);else if(x>=infoX)StartMenu();return;
        }
        if(message==WM_LBUTTONDOWN){
            pressed=app;pressedKey=app>=0?apps.list[app].key:L"";
            GetCursorPos(&downPoint);timePressed=time;SetCapture(window);
            if(time)AnimateVector(timeContainer,L"Scale",{.95f,.95f,1},200);
            return;
        }
        if(message==WM_LBUTTONUP){
            const bool wasTime=timePressed;
            const auto selected=pressedKey;pressedKey.clear();pressed=-1;
            POINT up{};GetCursorPos(&up);
            const bool dragged=std::abs(up.x-downPoint.x)>GetSystemMetrics(SM_CXDRAG)||
                std::abs(up.y-downPoint.y)>GetSystemMetrics(SM_CYDRAG);
            if(GetCapture()==window)ReleaseCapture();
            if(wasTime){
                AnimateVector(timeContainer,L"Scale",{1,1,1},200);timePressed=false;
                if(time&&!dragged&&RequestStartMenu(window)){
                    HideDetail();infoHover=false;hoverAt=0;
                    startHold.Request(GetTickCount64());startWindow=LeanDockStartState::Visible();startHolding=true;
                    startHold.Update(GetTickCount64(),LeanDockStartState::Shown(startWindow));
                    SetPropW(window,L"Capsule.StartHold",HANDLE(1));
                }return;
            }
            if(app>=0&&!dragged&&apps.list[app].key==selected){
                if(apps.list[app].windows.empty())Bounce(app);
                apps.Activate(apps.list[app],window);
            }
        }
    }
    void CancelInput(){
        pressed=-1;pressedKey.clear();
        if(timePressed&&timeContainer)AnimateVector(timeContainer,L"Scale",{1,1,1},200);
        timePressed=false;
    }
    void Tick(){
        SYSTEMTIME t{};GetLocalTime(&t);
        auto stamp=((uint64_t(t.wYear)*13+t.wMonth)*32+t.wDay)*1440+t.wHour*60+t.wMinute;
        if(stamp!=minute){minute=stamp;if(shown)PaintTime();}
        tray.Prune();
        if(detail&&detail->visible)PaintDetail();
    }
    void PositionDetail(){
        if(!detail)return;
        const double x=std::clamp(dock.x+dock.w-PanelWidth*dpi,double(monitor.rcWork.left+8*dpi),
            std::max(double(monitor.rcWork.left+8*dpi),double(monitor.rcWork.right)-(PanelWidth+8)*dpi));
        const double y=std::max(double(monitor.rcWork.top+8*dpi),dock.y-(DetailGap+detail->height)*dpi);
        if(std::abs(detail->rect.x-x)>=.5||std::abs(detail->rect.y-y)>=.5||detail->rect.h!=detail->height*dpi)
            detail->Position(x,y);
    }
    void PanelCancel(){panelGesture.Cancel();panelHover=PanelPart::None;}
    void PanelSet(Control target,int value){
        const auto id=metrics.Set(target,value);
        if(id){auto& pending=target==Control::Volume?volumeRequest:brightnessRequest;
            pending={id,GetTickCount64(),value};}
    }
    void PanelInput(UINT message,float x,float y){
        const auto hit=PanelHit(x,y);auto data=metrics.Read();
        if(message==WM_MOUSEMOVE){
            if(panelGesture.Dragging()){
                if(panelGesture.Send(x,GetTickCount64()))PanelSet(panelGesture.pressed==PanelPart::Volume?
                    Control::Volume:Control::Brightness,panelGesture.preview);
                PaintDetail();
            }else if(panelHover!=hit){panelHover=hit;PaintDetail();}
            return;
        }
        if(message==WM_LBUTTONDOWN&&PanelEnabled(hit,data)){
            panelGesture.Begin(hit,x);SetCapture(detail->window);
            if(panelGesture.Send(x,GetTickCount64()))PanelSet(hit==PanelPart::Volume?
                Control::Volume:Control::Brightness,panelGesture.preview);
            PaintDetail();return;
        }
        if(message==WM_LBUTTONUP&&panelGesture.pressed!=PanelPart::None){
            const auto pressedPart=panelGesture.pressed;
            if(panelGesture.Dragging()){
                if(panelGesture.Send(x,GetTickCount64(),true))PanelSet(pressedPart==PanelPart::Volume?
                    Control::Volume:Control::Brightness,panelGesture.preview);
            }else if(hit==pressedPart&&PanelEnabled(hit,data)){
                switch(hit){
                case PanelPart::Mute:metrics.Set(Control::Mute,!data.mute);break;
                case PanelPart::Wifi:metrics.Set(Control::Wifi,data.wifi==1?0:1);break;
                case PanelPart::Bluetooth:metrics.Set(Control::Bluetooth,data.bluetooth==1?0:1);break;
                case PanelPart::DarkTheme:metrics.Set(Control::DarkTheme,data.darkTheme==1?0:1);break;
                case PanelPart::Hotspot:metrics.Set(Control::Hotspot,data.hotspot==1?0:1);break;
                case PanelPart::WifiMore:Open(L"ms-availablenetworks:");HideDetail();break;
                case PanelPart::BluetoothMore:Open(L"ms-settings:bluetooth");HideDetail();break;
                case PanelPart::DarkThemeMore:Open(L"ms-settings:themes");HideDetail();break;
                case PanelPart::HotspotMore:Open(L"ms-settings:network-mobilehotspot");HideDetail();break;
                default:break;
                }
            }
            PanelCancel();if(GetCapture()==detail->window)ReleaseCapture();
            if(detail->visible)PaintDetail();return;
        }
        // Existing tray forwarding is kept, including its down/up ordering.
        POINT p{};GetCursorPos(&p);
        for(auto index:traySlots)if(index<tray.icons.size()&&tray.icons[index].rect.Contains(p.x,p.y)){
            tray.Click(index,message,p);break;}
    }
    void ShowDetail(){
        if(MenuOpen())return;
        if(!detail){detail=std::make_unique<Popup>();detail->Init(scene,draw,dpi,PanelWidth,408,14,false);
            detail->click=[this](UINT m,float x,float y){PanelInput(m,x,y);};
            detail->pointerCancel=[this]{PanelCancel();};
        }
        PositionDetail();metrics.Visible(true);PaintDetail();detail->Show(true);
    }
    void PaintDetail(){
        if(!detail)return;
        auto data=metrics.Read();SYSTEMTIME date{};GetLocalTime(&date);
        std::wstring day=std::to_wstring(date.wMonth)+L"月"+std::to_wstring(date.wDay)+L"日";
        auto lunar=Lunar(date);
        traySlots.clear();for(size_t i=0;i<tray.icons.size();i++)if(!tray.icons[i].hidden)traySlots.push_back(i);
        PanelLayout layout(data,traySlots.size());
        // Hold geometry while dragging: error/media/device updates must not
        // move the track out from under a captured pointer.
        float wanted=panelGesture.Dragging()?detail->height:layout.height;
        if(wanted!=detail->height){detail->height=wanted;detail->scene.Dimensions(PanelWidth,PanelWidth,wanted);}
        PositionDetail();
        detail->Paint([&](ID2D1DeviceContext* dc){
            // Keep the dragged thumb at its requested position only until the
            // worker acknowledges that command. Diagnostics remain readback.
            auto display=data;const auto now=GetTickCount64();
            display.volume=volumeRequest.Display(data.volume,data.completed[unsigned(Control::Volume)],now);
            display.brightness=brightnessRequest.Display(data.brightness,data.completed[unsigned(Control::Brightness)],now);
            PaintControlPanel(draw,dc,display,power,Time(),day,lunar,layout,panelHover,panelGesture);
            if(layout.trayY>=0){float x=56,y=layout.trayY;
                for(auto index:traySlots){
                    if(x+22>252){x=56;y+=30;}
                    auto& icon=tray.icons[index];
                    icon.rect={detail->rect.x+x*dpi,detail->rect.y+y*dpi,22*dpi,22*dpi};
                    auto bitmap=draw.Icon(icon.image.get());draw.Image(dc,bitmap.get(),{x+3,y+3,16,16});x+=30;
                }
            }
        });
        SetPropW(window,L"Capsule.ControlPanelVersion",HANDLE(3));
        SetPropW(window,L"Capsule.ControlDarkThemePlusTwo",HANDLE(INT_PTR(data.darkTheme+2)));
        SetPropW(window,L"Capsule.ControlThemeAvailable",HANDLE(INT_PTR(data.themeAvailable)));
        SetPropW(window,L"Capsule.ControlThemeBusy",HANDLE(INT_PTR(data.themeBusy)));
        SetPropW(window,L"Capsule.ControlThemeExitPlusOne",HANDLE(INT_PTR(data.themeLastExit)+1));
        SetPropW(window,L"Capsule.ControlHotspotPlusTwo",HANDLE(INT_PTR(data.hotspot+2)));
        SetPropW(window,L"Capsule.ControlHotspotClientsPlusOne",HANDLE(INT_PTR(data.hotspotClients+1)));
        SetPropW(window,L"Capsule.ControlReads",HANDLE(uintptr_t(data.reads+1)));
        SetPropW(window,L"Capsule.ControlWrites",HANDLE(uintptr_t(data.writes+1)));
        SetPropW(window,L"Capsule.Volume",HANDLE(intptr_t(data.volume+2)));
        SetPropW(window,L"Capsule.Brightness",HANDLE(intptr_t(data.brightness+2)));
        SetPropW(window,L"Capsule.Wifi",HANDLE(intptr_t(data.wifi+2)));
        SetPropW(window,L"Capsule.Bluetooth",HANDLE(intptr_t(data.bluetooth+2)));
        SetPropW(window,L"Capsule.CpuSamples",HANDLE(uintptr_t(data.history.size()+1)));
        SetPropW(window,L"Capsule.DetailReads",HANDLE(uintptr_t(data.detailReads+1)));
        SetPropW(window,L"Capsule.TrayCount",HANDLE(uintptr_t(traySlots.size()+1)));
    }
    void Stop(){
        CloseMenus(false);HideDetail();tooltip.reset();submenu.reset();menu.reset();detail.reset();
        apps.StopWatch();tray.Stop();metrics.Stop();
        if(acNotify){UnregisterPowerSettingNotification(acNotify);acNotify=nullptr;}
        if(batteryNotify){UnregisterPowerSettingNotification(batteryNotify);batteryNotify=nullptr;}
        if(shellMessage)DeregisterShellHookWindow(window);shellMessage=0;
        visuals.clear();cache.clear();timeVisual=nullptr;timeContainer=nullptr;timeTint=nullptr;
    }
    ~Dashboard(){Stop();}
};
}
