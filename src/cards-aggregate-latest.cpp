#include "cards-latest.h"
#include "cards-compact-apps-latest.h"
#include "cards-aggregate-geometry-latest.h"
#include "aggregate-bridge-latest.h"
#include "aggregate-panel-pin-generated.h"
#include "verify-latest.h"
#include "shell-menu-latest.h"
#include <filesystem>
#include <atomic>
#include <chrono>
#include <winrt/Windows.System.Power.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Xaml.Automation.h>

namespace LeanDockMenu {std::mutex gestureMutex;Gesture gesture;}
extern "C" void Native_tray_refresh_cards();
extern "C" size_t InternalWh_GetModStoragePath(void*,PWSTR,size_t);
namespace LeanDockCards {
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace a=LeanDockAggregate;
using Power=winrt::Windows::System::Power::PowerManager;
using winrt::Windows::Foundation::IInspectable;
#include "cards-aggregate-xaml-latest.inc"
static constexpr const wchar_t* Properties[]={
    L"LeanDock.Card.Ready",L"LeanDock.Card.Visible",L"LeanDock.Card.Fits",L"LeanDock.Card.PowerReady",L"LeanDock.Card.PowerActive",
    L"LeanDock.Card.Left100",L"LeanDock.Card.Top100",L"LeanDock.Card.Right100",L"LeanDock.Card.Bottom100",
    L"LeanDock.Card.BatteryPercent",L"LeanDock.Card.Charging",L"LeanDock.Card.Plugged",L"LeanDock.Card.PowerUpdates",
    L"LeanDock.Card.ClockHourMinute",L"LeanDock.Card.ClockMonthDay",L"LeanDock.Card.ClockUpdates",L"LeanDock.Card.AppExtent100",
    L"LeanDock.Card.Dashboard",L"LeanDock.Card.Dashboard.Width100",L"LeanDock.Card.Errors",L"LeanDock.Card.PanelError",
    L"LeanDock.Card.Sampling",L"LeanDock.Card.ClockActive",L"LeanDock.Card.NetworkActive",L"LeanDock.Card.AggregateVersion",
    L"LeanDock.Card.SurfaceCenterY100",L"LeanDock.Card.CenterDelta100",L"LeanDock.Card.OffsetY100"};
struct Board final:Host,std::enable_shared_from_this<Board>{
    c::Grid root{nullptr};x::FrameworkElement background{nullptr},apps{nullptr};
    IInspectable oldPadding{nullptr},oldMargin{nullptr};
    c::Border card{nullptr};c::Button key{nullptr};c::TextBlock time{nullptr},date{nullptr};
    x::Shapes::Path normal{nullptr},low{nullptr},critical{nullptr};
    x::DispatcherTimer clock{nullptr},hover{nullptr};
    winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
    Power::RemainingChargePercentChanged_revoker chargeChanged;Power::BatteryStatusChanged_revoker batteryChanged;
    Power::PowerSupplyStatusChanged_revoker supplyChanged;
    x::FrameworkElement::Unloaded_revoker unloaded;x::UIElement::ContextRequested_revoker context;
    CompactApps compact;std::wstring directory;HWND hwnd{};HANDLE child{};uintptr_t cookie{};
    uint64_t enteredAt=0,launchedAt=0;
    bool stopped=false,busy=false,fits=false,shown=false,inside=false,ready=false,attached=false,layoutOwned=false;
    unsigned errors=0,clockUpdates=0,powerUpdates=0;int percent=-2;std::atomic<bool> queued=false;
    ~Board(){Stop();}
    void Put(wchar_t const* name,intptr_t value){if(hwnd&&GetPropW(hwnd,name)!=HANDLE(value+1))SetPropW(hwnd,name,HANDLE(value+1));}
    void Fail()noexcept{Put(L"LeanDock.Card.Errors",++errors);}
    bool Visible()const{
        if(!hwnd||!fits||!IsWindowVisible(hwnd)||IsIconic(hwnd))return false;
        RECT r{};MONITORINFO m{sizeof(m)};
        return GetWindowRect(hwnd,&r)&&GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&m)&&
            r.top<m.rcMonitor.bottom-int(a::Height*GetDpiForWindow(hwnd)/96.0);
    }
    void ClosePanel()noexcept{
        if(!hwnd)return;
        auto control=reinterpret_cast<HWND>(GetPropW(hwnd,a::ControlProp));
        if(a::Owned(hwnd,control,a::ControlClass))PostMessageW(control,WM_CLOSE,0,0);
        for(auto prop:{a::CookieProp,a::PendingProp,a::RevealProp,a::WindowProp,a::ControlProp,a::PidProp})RemovePropW(hwnd,prop);
        cookie=0;a::Refresh(hwnd);
    }
    void AlignToSurface(){
        if(!fits||!card.IsLoaded()||!background.IsLoaded()||background.ActualHeight()<=0)return;
        // BackgroundControl is top-aligned and has its own height. Centering
        // in RootGrid alone can differ from the visible Dock's center.
        const auto target=background.TransformToVisual(root).TransformPoint({0,float(background.ActualHeight()/2)});
        const auto current=card.TransformToVisual(root).TransformPoint({0,float(card.ActualHeight()/2)});
        auto shift=card.RenderTransform().as<x::Media::TranslateTransform>();
        const double delta=target.Y-current.Y;
        if(std::isfinite(delta)&&std::abs(delta)>.01)shift.Y(shift.Y()+delta);
        const auto aligned=card.TransformToVisual(root).TransformPoint({0,float(card.ActualHeight()/2)});
        Put(L"LeanDock.Card.SurfaceCenterY100",std::lround(target.Y*100));
        Put(L"LeanDock.Card.CenterDelta100",std::lround((aligned.Y-target.Y)*100));
        Put(L"LeanDock.Card.OffsetY100",std::lround(shift.Y()*100));
    }
    void PublishAnchor(){
        if(!hwnd||!fits||!card.IsLoaded())return;
        const auto pos=card.TransformToVisual(root.XamlRoot().Content()).TransformPoint({0,0});
        Put(L"LeanDock.Card.Left100",std::lround(pos.X*100));Put(L"LeanDock.Card.Top100",std::lround(pos.Y*100));
        Put(L"LeanDock.Card.Right100",std::lround((pos.X+card.ActualWidth())*100));Put(L"LeanDock.Card.Bottom100",std::lround((pos.Y+card.ActualHeight())*100));
        // XAML coordinates belong to the content bridge, not the full taskbar.
        struct Search{DWORD thread;HWND island;};Search search{GetCurrentThreadId(),nullptr};
        EnumChildWindows(hwnd,[](HWND w,LPARAM data)->BOOL{
            auto& s=*reinterpret_cast<Search*>(data);wchar_t cls[128]{};GetClassNameW(w,cls,128);
            if(GetWindowThreadProcessId(w,nullptr)==s.thread&&!wcscmp(cls,L"Windows.UI.Composition.DesktopWindowContentBridge")){s.island=w;return FALSE;}return TRUE;
        },reinterpret_cast<LPARAM>(&search));
        if(!search.island)return;
        POINT origin{};if(!ClientToScreen(search.island,&origin))return;
        const auto islandDpi=GetDpiForWindow(search.island);
        const double scale=islandDpi?islandDpi/96.0:root.XamlRoot().RasterizationScale();
        const LONG values[]={origin.x+LONG(std::lround(pos.X*scale)),origin.y+LONG(std::lround(pos.Y*scale)),
            origin.x+LONG(std::lround((pos.X+card.ActualWidth())*scale)),origin.y+LONG(std::lround((pos.Y+card.ActualHeight())*scale))};
        for(unsigned i=0;i<4;++i)SetPropW(hwnd,a::BoundsProps[i],HANDLE(intptr_t(values[i])));
    }
    // Hover is two-staged: after a short rest the child starts hidden and
    // samples, then the real hover delay reveals it with values already read.
    // A pass-over shorter than PrimeDelay starts nothing.
    static constexpr uint64_t PrimeDelay=120,RevealDelay=400;
    void Hover(){
        enteredAt=GetTickCount64();hover.Stop();
        hover.Interval(std::chrono::milliseconds(child&&WaitForSingleObject(child,0)==WAIT_TIMEOUT?RevealDelay:PrimeDelay));hover.Start();
    }
    void HoverTick(){
        hover.Stop();if(stopped||!inside||!Visible())return;
        if(child&&WaitForSingleObject(child,0)!=WAIT_TIMEOUT){CloseHandle(child);child=nullptr;}
        if(!child&&!Launch())return;
        const auto now=GetTickCount64(),due=enteredAt+RevealDelay;
        // A predecessor that already withdrew its control window is closing;
        // poll briefly until it exits, then start and reveal a fresh child.
        if(now>=due&&Reveal())return;
        hover.Interval(std::chrono::milliseconds(now<due?due-now:50));hover.Start();
    }
    bool Reveal(){
        if(!cookie||!child||WaitForSingleObject(child,0)!=WAIT_TIMEOUT)return false;
        auto control=reinterpret_cast<HWND>(GetPropW(hwnd,a::ControlProp));
        const bool published=a::Owned(hwnd,control,a::ControlClass);
        if(!published&&launchedAt<enteredAt)return false;
        // Property first: a child publishing its control window afterwards
        // checks it, so the reveal is never lost between the two processes.
        SetPropW(hwnd,a::RevealProp,HANDLE(cookie));
        if(published)PostMessageW(control,a::RevealMessage,0,0);
        return true;
    }
    bool Launch(){
        if(stopped||!inside||!Visible()||directory.empty())return false;
        PublishAnchor();ClosePanel();
        const auto path=directory+L"\\aggregate\\latest.exe";wchar_t hash[65]{};
        if(!NativeHashFile(path.c_str(),hash)||_wcsicmp(hash,AggregatePanelHash)){Put(L"LeanDock.Card.PanelError",ERROR_INVALID_IMAGE_HASH);return false;}
        cookie=(GetTickCount64()<<16)^uintptr_t(this);if(!cookie)cookie=1;
        SetPropW(hwnd,a::CookieProp,HANDLE(cookie));RECT anchor{};
        if(!a::Anchor(hwnd,anchor)){ClosePanel();Put(L"LeanDock.Card.PanelError",ERROR_INVALID_DATA);return false;}
        std::wstring command=L"\""+path+L"\" --owner "+std::to_wstring(uintptr_t(hwnd))+L" "+std::to_wstring(cookie);
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        // Hover is an in-place interaction. GUI process creation otherwise
        // enables Windows' app-starting cursor even with SW_HIDE.
        startup.dwFlags=STARTF_USESHOWWINDOW|STARTF_FORCEOFFFEEDBACK;startup.wShowWindow=SW_HIDE;
        // Inherit Explorer's ordinary token, never request UAC. Suspend until the
        // PID/cookie have been published so the child can reject stale owners.
        if(!CreateProcessW(path.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT,
                nullptr,(directory+L"\\aggregate").c_str(),&startup,&process)){
            const auto error=GetLastError();ClosePanel();Put(L"LeanDock.Card.PanelError",error);return false;
        }
        child=process.hProcess;launchedAt=GetTickCount64();
        SetPropW(hwnd,a::PidProp,HANDLE(uintptr_t(process.dwProcessId)));
        SetPropW(hwnd,a::PendingProp,HANDLE(uintptr_t(GetTickCount64()+2500)));a::Refresh(hwnd);
        const bool resumed=ResumeThread(process.hThread)!=DWORD(-1);
        if(!resumed){TerminateProcess(child,ERROR_CANCELLED);ClosePanel();Put(L"LeanDock.Card.PanelError",ERROR_CANCELLED);}
        else Put(L"LeanDock.Card.PanelError",0);
        CloseHandle(process.hThread);return resumed;
    }
    void Clock(){
        if(stopped)return;SYSTEMTIME local{};GetLocalTime(&local);wchar_t t[16]{},d[16]{};
        swprintf_s(t,L"%u:%02u",local.wHour,local.wMinute);swprintf_s(d,L"%u/%02u",local.wMonth,local.wDay);
        time.Text(t);date.Text(d);Put(L"LeanDock.Card.ClockHourMinute",local.wHour*100+local.wMinute);
        Put(L"LeanDock.Card.ClockMonthDay",local.wMonth*100+local.wDay);Put(L"LeanDock.Card.ClockUpdates",++clockUpdates);
        clock.Stop();if(shown){clock.Interval(std::chrono::milliseconds(60000-local.wSecond*1000-local.wMilliseconds+20));clock.Start();}
    }
    void ReadPower(){
        if(stopped)return;SYSTEM_POWER_STATUS p{};const bool valid=GetSystemPowerStatus(&p)&&!(p.BatteryFlag&128)&&p.BatteryLifePercent<=100;
        const int next=valid?p.BatteryLifePercent:-1;
        if(next!=percent){
            // UWP Geometry has one parent; sharing it between paths fails with
            // E_INVALIDARG (unlike shareable theme brushes). Own each instance.
            for(auto const& path:{normal,low,critical}){path.Data(a::Geometry(std::max(0,next)));path.Visibility(x::Visibility::Collapsed);}
            percent=next;
        }
        const auto tone=a::BatteryTone(percent,p.ACLineStatus==1);
        unsigned index=0;for(auto const& path:{normal,low,critical}){
            const auto visibility=percent>0&&index++==unsigned(tone)?x::Visibility::Visible:x::Visibility::Collapsed;
            if(path.Visibility()!=visibility)path.Visibility(visibility);
        }
        Put(L"LeanDock.Card.BatteryPercent",percent);Put(L"LeanDock.Card.Plugged",p.ACLineStatus==1);
        Put(L"LeanDock.Card.Charging",p.BatteryFlag!=255&&(p.BatteryFlag&8));Put(L"LeanDock.Card.PowerUpdates",++powerUpdates);
        x::Automation::AutomationProperties::SetName(key,L"时间 "+time.Text()+L"，日期 "+date.Text()+
            (valid?L"，电量 "+winrt::to_hstring(percent)+L"%":L"，电量未知")+L"，打开开始菜单");
    }
    static void Queue(std::weak_ptr<Board> weak)noexcept{
        try{if(auto p=weak.lock();p&&!p->queued.exchange(true))p->dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[weak]{
            if(auto p=weak.lock()){p->queued=false;if(!p->stopped)try{p->ReadPower();}catch(...){p->Fail();}}
        });}catch(...){if(auto p=weak.lock())p->queued=false;}
    }
    void Init(c::Grid const& grid){
        root=grid;dispatcher=root.Dispatcher();wchar_t storage[32768]{};
        if(InternalWh_GetModStoragePath(nullptr,storage,std::size(storage)))directory=storage;
        for(auto const& child:root.Children())if(auto fe=child.try_as<x::FrameworkElement>()){
            if(fe.Name()==L"BackgroundControl")background=fe;if(fe.Name()==L"TaskbarFrameRepeater")apps=fe;
        }
        if(!background||!apps)throw winrt::hresult_error(E_PENDING);
        oldPadding=root.ReadLocalValue(c::Grid::PaddingProperty());oldMargin=background.ReadLocalValue(x::FrameworkElement::MarginProperty());
        card=x::Markup::XamlReader::Load(AggregateXaml).as<c::Border>();card.Margin({0,0,-a::Width,0});
        key=card.FindName(L"TimeKey").as<c::Button>();time=card.FindName(L"ClockTime").as<c::TextBlock>();date=card.FindName(L"ClockDate").as<c::TextBlock>();
        normal=card.FindName(L"RingNormal").as<x::Shapes::Path>();low=card.FindName(L"RingLow").as<x::Shapes::Path>();critical=card.FindName(L"RingCritical").as<x::Shapes::Path>();
        c::Canvas::SetZIndex(card,20);card.Visibility(x::Visibility::Collapsed);
        auto weak=weak_from_this();clock=x::DispatcherTimer();hover=x::DispatcherTimer();
        clock.Tick([weak](auto&&,auto&&){if(auto p=weak.lock())try{p->Clock();}catch(...){p->Fail();}});
        hover.Tick([weak](auto&&,auto&&){if(auto p=weak.lock())try{p->HoverTick();}catch(...){p->hover.Stop();p->ClosePanel();p->Fail();}});
        key.PointerEntered([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped){p->inside=true;p->Hover();}});
        key.PointerExited([weak](auto&&,auto&&){if(auto p=weak.lock()){p->inside=false;p->hover.Stop();}});
        key.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped&&LeanDockShell::IsTaskbar(p->hwnd,GetCurrentProcessId())){
            p->hover.Stop();p->ClosePanel();PostMessageW(p->hwnd,WM_SYSCOMMAND,SC_TASKLIST,0);
        }});
        context=card.ContextRequested(winrt::auto_revoke,[weak](auto&&,x::Input::ContextRequestedEventArgs const& e)noexcept{
            if(auto p=weak.lock();p&&!p->stopped&&p->fits&&p->Visible()&&!e.Handled())try{
                e.Handled(true);p->hover.Stop();p->ClosePanel();
                LeanDockMenu::HandleContext(p->hwnd,LeanDockContext::Target::Card,LeanDockMenu::ContextSource::DirectCard);
            }catch(...){p->Fail();}
        });
        unloaded=root.Unloaded(winrt::auto_revoke,[weak](auto&&,auto&&){if(auto p=weak.lock())p->Stop();});
        Clock();ReadPower();
        try{
            chargeChanged=Power::RemainingChargePercentChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
            batteryChanged=Power::BatteryStatusChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
            supplyChanged=Power::PowerSupplyStatusChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
        }catch(...){Fail();}
        // Publish only after construction succeeds. A failed candidate must
        // not repeatedly add/remove children from a live LayoutUpdated call.
        root.Children().Append(card);attached=true;
    }
    void Update(HWND taskbar,double available,double tray)override{
        if(stopped||busy)return;busy=true;
        try{
            hwnd=taskbar;const double budget=available-tray-(a::SlotWidth-12)-57;
            fits=std::isfinite(budget)&&budget>=40;if(fits)compact.Apply(apps,budget);else compact.Restore();
            const double slot=fits?a::SlotWidth-12:0;
            layoutOwned=true;
            const auto padding=root.Padding();if(padding.Left!=12||padding.Right!=12+slot||padding.Top||padding.Bottom)root.Padding({12,0,12+slot,0});
            const auto margin=background.Margin();if(margin.Left!=-13||margin.Right!=-12-slot||margin.Top!=-1||margin.Bottom!=-1)background.Margin({-13,-1,-12-slot,-1});
            const auto visibility=fits?x::Visibility::Visible:x::Visibility::Collapsed;
            if(card.Visibility()!=visibility)card.Visibility(visibility);
            const bool changed=shown!=Visible();shown=Visible();if(changed){Clock();ReadPower();if(!shown){hover.Stop();ClosePanel();}}
            const bool nextReady=fits&&card.IsLoaded()&&card.ActualWidth()>0;
            Put(L"LeanDock.Card.Ready",1);Put(L"LeanDock.Card.Fits",fits);Put(L"LeanDock.Card.Visible",shown);
            Put(L"LeanDock.Card.Errors",errors);
            Put(L"LeanDock.Card.PowerReady",nextReady);Put(L"LeanDock.Card.PowerActive",0);
            Put(L"LeanDock.Card.Dashboard",1);Put(L"LeanDock.Card.Dashboard.Width100",7200);Put(L"LeanDock.Card.AggregateVersion",1);
            Put(L"LeanDock.Card.Sampling",0);Put(L"LeanDock.Card.NetworkActive",0);Put(L"LeanDock.Card.ClockActive",shown);
            Put(L"LeanDock.Card.AppExtent100",std::lround(compact.Width()*100));AlignToSurface();PublishAnchor();
            if(ready!=nextReady){ready=nextReady;Native_tray_refresh_cards();}
        }catch(...){Fail();}busy=false;
    }
    void TimeChanged()noexcept override{try{Clock();ReadPower();}catch(...){Fail();}}
    void ReloadPreferences()noexcept override{TimeChanged();}
    bool Alive()const noexcept override{return !stopped;}
    x::FrameworkElement MenuTarget()const noexcept override{try{if(!stopped&&fits&&card&&card.IsLoaded())return card;}catch(...){}return nullptr;}
    void Stop()noexcept override{
        if(stopped)return;stopped=true;ClosePanel();if(child){CloseHandle(child);child=nullptr;}
        try{if(clock)clock.Stop();if(hover)hover.Stop();}catch(...){}
        chargeChanged.revoke();batteryChanged.revoke();supplyChanged.revoke();unloaded.revoke();context.revoke();compact.Restore();
        try{if(root){uint32_t index=0;if(attached&&card&&root.Children().IndexOf(card,index))root.Children().RemoveAt(index);
            if(layoutOwned){if(oldPadding==x::DependencyProperty::UnsetValue())root.ClearValue(c::Grid::PaddingProperty());else root.SetValue(c::Grid::PaddingProperty(),oldPadding);}}}
        catch(...){}
        try{if(layoutOwned&&background){if(oldMargin==x::DependencyProperty::UnsetValue())background.ClearValue(x::FrameworkElement::MarginProperty());else background.SetValue(x::FrameworkElement::MarginProperty(),oldMargin);}}catch(...){}
        if(hwnd){for(auto prop:Properties)RemovePropW(hwnd,prop);for(auto prop:a::BoundsProps)RemovePropW(hwnd,prop);}
    }
};
std::shared_ptr<Host> Create(c::Grid const& root){auto p=std::make_shared<Board>();p->Init(root);return p;}
}
