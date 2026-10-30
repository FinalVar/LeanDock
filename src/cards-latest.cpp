#include "cards-network-latest.h"
#include "cards-latest.h"
#include "cards-core-latest.h"
#include "cards-dashboard-latest.h"
#include "cards-compact-apps-latest.h"
#include "verify-latest.h"
#include "cards-preferences-latest.h"
#include "shell-actions-latest.h"
#include "shell-menu-latest.h"
#include "power-target-latest.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <utility>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.System.Power.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>

namespace LeanDockMenu { std::mutex gestureMutex;Gesture gesture; }
// Called on the owning XAML thread after the battery module becomes available.
// The tray adapter also follows subsequent layout changes on that thread.
extern "C" void Native_tray_refresh_cards();
extern "C" int Native_hide_expanded(HWND window);
extern "C" BOOL InternalWh_SetStringValue(void*,PCWSTR,PCWSTR);
extern "C" size_t InternalWh_GetModStoragePath(void*,PWSTR,size_t);
extern "C" BOOL InternalWh_SetFunctionHook(void*,void*,void*,void**);
extern "C" BOOL InternalWh_ApplyHookOperations(void*);

namespace LeanDockCards {
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace m=winrt::Windows::Media::Control;
using Manager=m::GlobalSystemMediaTransportControlsSessionManager;
using Session=m::GlobalSystemMediaTransportControlsSession;
using Power=winrt::Windows::System::Power::PowerManager;
using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::UI::Core::CoreDispatcherPriority;

static constexpr const wchar_t* Props[]={L"LeanDock.Card.Ready",L"LeanDock.Card.Visible",
    L"LeanDock.Card.Sampling",L"LeanDock.Card.Samples",L"LeanDock.Card.Cpu",L"LeanDock.Card.Memory",
    L"LeanDock.Card.Media",L"LeanDock.Card.View",L"LeanDock.Card.Errors",L"LeanDock.Card.Commands",
    L"LeanDock.Card.Left100",L"LeanDock.Card.Top100",L"LeanDock.Card.Right100",L"LeanDock.Card.Bottom100",
    L"LeanDock.Card.AppRight100",L"LeanDock.Card.RootRight100",L"LeanDock.Card.Fits",
    L"LeanDock.Card.ControlsLeft100",L"LeanDock.Card.ControlsRight100",L"LeanDock.Card.ControlsTop100",L"LeanDock.Card.ControlsBottom100",
    L"LeanDock.Card.TitleLeft100",L"LeanDock.Card.TitleRight100",L"LeanDock.Card.ButtonWidth100",
    L"LeanDock.Card.PowerReady",L"LeanDock.Card.BatteryPercent",L"LeanDock.Card.Charging",L"LeanDock.Card.Plugged",
    L"LeanDock.Card.PowerLeft100",L"LeanDock.Card.PowerRight100",L"LeanDock.Card.PowerTop100",L"LeanDock.Card.PowerBottom100",
    L"LeanDock.Card.PowerActive",L"LeanDock.Card.VisiblePages",L"LeanDock.Card.BatteryMode",L"LeanDock.Card.PowerUpdates",L"LeanDock.Card.Switches",
    L"LeanDock.Card.CpuRefreshing",L"LeanDock.Card.CpuIntervalMs",L"LeanDock.Card.TitleFont100",
    L"LeanDock.Card.PowerContentLeft100",L"LeanDock.Card.PowerContentRight100",L"LeanDock.Card.PowerContentTop100",L"LeanDock.Card.PowerContentBottom100",
    L"LeanDock.Card.TitleHeight100",L"LeanDock.Card.TitleRowHeight100",L"LeanDock.Card.TitleTop100",L"LeanDock.Card.TitleBottom100",
    L"LeanDock.Card.ClockActive",L"LeanDock.Card.ClockHourMinute",L"LeanDock.Card.ClockMonthDay",L"LeanDock.Card.ClockUpdates",
    L"LeanDock.Card.AppWidth100",L"LeanDock.Card.RootWidth100",L"LeanDock.Card.RootMinWidth100",L"LeanDock.Card.FrameMinWidth100",L"LeanDock.Card.MediaPlaying",
    L"LeanDock.Card.NetworkActive",L"LeanDock.Card.NetworkSamples",L"LeanDock.Card.NetworkReceive",L"LeanDock.Card.NetworkSend",L"LeanDock.Card.PageCount",L"LeanDock.Card.PreferencesValid",L"LeanDock.Card.NetworkInterfaces"};
static uint64_t Bits(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
// Bounded, read-only diagnostics for genuine gestures inside the power card.
// No global input observation, coordinates outside this card, or titles.
static constexpr const wchar_t* InputProps[]={
    L"LeanDock.Card.Input.Presses",L"LeanDock.Card.Input.Releases",L"LeanDock.Card.Input.Taps",
    L"LeanDock.Card.Input.Kind",L"LeanDock.Card.Input.Source",L"LeanDock.Card.Input.Handled",
    L"LeanDock.Card.Input.At",L"LeanDock.Card.Input.NativeDx100",L"LeanDock.Card.Input.NativeDy100",
    L"LeanDock.Card.Input.NativeEnabled",L"LeanDock.Card.Input.NativeHit",L"LeanDock.Card.Input.NativeFocus",
    L"LeanDock.Card.Input.Fallbacks",L"LeanDock.Card.Input.NativeIcons",L"LeanDock.Card.Input.NativeLoadedIcons",
    L"LeanDock.Card.Input.TapEnabled"};
static void InputValue(HWND w,unsigned index,intptr_t value){if(w)SetPropW(w,InputProps[index],reinterpret_cast<HANDLE>(value+1));}
static void InputCount(HWND w,unsigned index){if(w)InputValue(w,index,std::max<intptr_t>(1,reinterpret_cast<intptr_t>(GetPropW(w,InputProps[index]))));}
static constexpr const wchar_t* WheelProps[]={L"LeanDock.Card.Wheel.Ready",L"LeanDock.Card.Wheel.Native",
    L"LeanDock.Card.Wheel.Xaml",L"LeanDock.Card.Wheel.TouchpadStarts",L"LeanDock.Card.Wheel.Fallbacks",L"LeanDock.Card.Wheel.Axis",
    L"LeanDock.Card.Wheel.DMReady",L"LeanDock.Card.Wheel.DMContacts",L"LeanDock.Card.Wheel.DMActive",
    L"LeanDock.Card.Wheel.DMUpdates",L"LeanDock.Card.Wheel.DMState",L"LeanDock.Card.Wheel.DMError",
    L"LeanDock.Card.Wheel.DMSwitches",L"LeanDock.Card.Wheel.DMFrames"};
static void WheelCount(HWND w,unsigned index){if(w)SetPropW(w,WheelProps[index],reinterpret_cast<HANDLE>(reinterpret_cast<uintptr_t>(GetPropW(w,WheelProps[index]))+1));}
static std::optional<CpuTimes> Times(){
    // This API is group-local on machines with more than 64 processors. Do
    // not label that partial value as whole-machine CPU utilization.
    if(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)>64)return {};
    FILETIME i{},k{},u{};if(!GetSystemTimes(&i,&k,&u))return {};return CpuTimes{Bits(i),Bits(k),Bits(u)};
}
static void Put(HWND w,unsigned i,uintptr_t value){if(w){auto v=reinterpret_cast<HANDLE>(value+1);if(GetPropW(w,Props[i])!=v)SetPropW(w,Props[i],v);}}
static winrt::hstring PercentText(std::optional<unsigned> value){return value?winrt::to_hstring(*value)+L"%":L"—";}
#include "cards-dashboard-xaml-latest.inc"
static x::FrameworkElement NativeQuickSettings(x::DependencyObject const& node,unsigned depth=0) {
    if(!node||depth>24)return nullptr;
    if(auto fe=node.try_as<x::FrameworkElement>();fe&&fe.Name()==L"ControlCenterButton")return fe;
    const auto count=x::Media::VisualTreeHelper::GetChildrenCount(node);
    for(int i=0;i<count;++i)if(auto result=NativeQuickSettings(x::Media::VisualTreeHelper::GetChild(node,i),depth+1))return result;
    return nullptr;
}

struct Board final:Host,std::enable_shared_from_this<Board> {
    c::Grid root{nullptr}; x::FrameworkElement background{nullptr}, apps{nullptr};
    IInspectable oldPadding{nullptr},oldMargin{nullptr};
    c::Border card{nullptr}; x::Shapes::Rectangle divider{nullptr};
    c::Button powerCard{nullptr};c::TextBlock powerText{nullptr},powerStatus{nullptr};c::StackPanel powerContent{nullptr};
    x::Shapes::Rectangle powerFill{nullptr};x::Shapes::Path powerBolt{nullptr};
    c::ColumnDefinition powerLevelColumn{nullptr},powerEmptyColumn{nullptr};
    x::UIElement wheelRoot{nullptr};IInspectable wheelHandler{nullptr},inputPress{nullptr},inputRelease{nullptr},inputTap{nullptr};
    winrt::weak_ref<x::FrameworkElement> inputNative;
    Power::RemainingChargePercentChanged_revoker chargeChanged;
    Power::BatteryStatusChanged_revoker batteryChanged;
    Power::PowerSupplyStatusChanged_revoker supplyChanged;
    c::Grid metrics{nullptr},media{nullptr}; c::StackPanel clock{nullptr};
    c::Grid metricsCard{nullptr},networkCard{nullptr};c::Button clockCard{nullptr},startEntry{nullptr},mediaToggle{nullptr};
    c::StackPanel modules{nullptr},clockGroup{nullptr};c::Grid mediaGroup{nullptr};
    c::TextBlock metricsSummary{nullptr},memoryUsage{nullptr};
    bool mediaAvailable=false,mediaExpanded=false;double dashboardWidth=0;std::wstring moduleKey;
    CompactApps compactApps;uint64_t cpuDue=0,clockDue=0,networkDue=0;
    c::TextBlock networkReceive{nullptr},networkSend{nullptr};
    c::TextBlock title{nullptr},clockTime{nullptr},clockDate{nullptr};
    c::StackPanel mediaControls{nullptr};winrt::hstring artistName;
    CpuSampler cpu;std::optional<unsigned> memoryPercent;
    c::Button previous{nullptr},play{nullptr},next{nullptr};c::TextBlock playGlyph{nullptr};
    x::DispatcherTimer timer{nullptr};winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
    Manager manager{nullptr};Session session{nullptr};
    Manager::CurrentSessionChanged_revoker managerChanged;
    Manager::SessionsChanged_revoker sessionsChanged;
    Session::MediaPropertiesChanged_revoker propertiesChanged;
    Session::PlaybackInfoChanged_revoker playbackChanged;
    x::FrameworkElement::Unloaded_revoker unloaded;
    x::UIElement::ContextRequested_revoker contextRequested;
    winrt::Windows::Foundation::IAsyncOperation<Manager> request{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<m::GlobalSystemMediaTransportControlsSessionMediaProperties> metadata{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<bool> command{nullptr};
    CommandGate commandGate;
    HWND hwnd=nullptr; bool stopped=false,shown=false,fits=false,busy=false,sampling=false,clocking=false;
    Page page=Page::Clock;PageOrder order;std::wstring directory;
    NetworkSampler network;bool networking=false,networkPending=false,middleArmed=false;
    unsigned networkGeneration=0,networkSamples=0;bool preferencesValid=true;
    unsigned generation=0,samples=0,errors=0,commands=0,powerUpdates=0,switches=0,clockUpdates=0;
    std::atomic<bool> refreshQueued=false;
    std::atomic<bool> powerQueued=false;
    Battery battery;
    std::wstring lastLayout;
    ~Board(){Stop();}

    void Init(c::Grid const& grid) {
        root=grid;dispatcher=root.Dispatcher();
        wchar_t storage[32768]{};
        if(InternalWh_GetModStoragePath(nullptr,storage,std::size(storage)))directory=storage;
        preferencesValid=!directory.empty()&&ReadPreferences(directory,order,page);
        for(auto const& child:root.Children())if(auto fe=child.try_as<x::FrameworkElement>()){
            if(fe.Name()==L"BackgroundControl")background=fe;
            if(fe.Name()==L"TaskbarFrameRepeater")apps=fe;
        }
        if(!background || !apps)throw winrt::hresult_error(E_PENDING);
        oldPadding=root.ReadLocalValue(c::Grid::PaddingProperty());
        oldMargin=background.ReadLocalValue(x::FrameworkElement::MarginProperty());
        // A fixed-width overlay inside owned root padding contributes zero
        // desired content width; even a one-application Dock stays compact.
        card=x::Markup::XamlReader::Load(DashboardXaml).as<c::Border>();
        modules=card.FindName(L"DashboardModules").as<c::StackPanel>();
        clockGroup=card.FindName(L"ClockGroup").as<c::StackPanel>();
        mediaGroup=card.FindName(L"MediaGroup").as<c::Grid>();
        metricsCard=card.FindName(L"MetricsCard").as<c::Grid>();metrics=metricsCard;
        networkCard=card.FindName(L"NetworkCard").as<c::Grid>();
        media=card.FindName(L"Media").as<c::Grid>();clock=card.FindName(L"Clock").as<c::StackPanel>();
        clockCard=card.FindName(L"ClockCard").as<c::Button>();
        startEntry=card.FindName(L"StartEntry").as<c::Button>();mediaToggle=card.FindName(L"MediaToggle").as<c::Button>();
        metricsSummary=card.FindName(L"MetricsSummary").as<c::TextBlock>();memoryUsage=card.FindName(L"MemoryUsage").as<c::TextBlock>();
        networkReceive=card.FindName(L"NetworkReceive").as<c::TextBlock>();networkSend=card.FindName(L"NetworkSend").as<c::TextBlock>();
        clockTime=card.FindName(L"ClockTime").as<c::TextBlock>();clockDate=card.FindName(L"ClockDate").as<c::TextBlock>();
        title=card.FindName(L"Title").as<c::TextBlock>();mediaControls=card.FindName(L"MediaControls").as<c::StackPanel>();
        previous=card.FindName(L"Previous").as<c::Button>();play=card.FindName(L"Play").as<c::Button>();next=card.FindName(L"Next").as<c::Button>();
        playGlyph=card.FindName(L"PlayGlyph").as<c::TextBlock>();
        powerCard=x::Markup::XamlReader::Load(DashboardPowerXaml).as<c::Button>();
        powerCard.Width(Dashboard::PowerWidth);
        powerText=powerCard.FindName(L"PowerText").as<c::TextBlock>();powerFill=powerCard.FindName(L"PowerFill").as<x::Shapes::Rectangle>();
        auto powerColumns=powerCard.FindName(L"PowerLevelHost").as<c::Grid>().ColumnDefinitions();
        powerLevelColumn=powerColumns.GetAt(0);powerEmptyColumn=powerColumns.GetAt(1);
        powerStatus=powerCard.FindName(L"PowerStatus").as<c::TextBlock>();powerBolt=powerCard.FindName(L"PowerBolt").as<x::Shapes::Path>();
        powerContent=powerCard.FindName(L"PowerContent").as<c::StackPanel>();
        divider=x::Markup::XamlReader::Load(LR"xaml(
<Rectangle xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
 <Rectangle.Fill><LinearGradientBrush StartPoint="0.5,0" EndPoint="0.5,1" Opacity="0.30">
  <GradientStop Offset="0" Color="Transparent"/><GradientStop Offset="0.18" Color="{ThemeResource SystemBaseHighColor}"/>
  <GradientStop Offset="0.82" Color="{ThemeResource SystemBaseHighColor}"/><GradientStop Offset="1" Color="Transparent"/>
 </LinearGradientBrush></Rectangle.Fill>
</Rectangle>)xaml").as<x::Shapes::Rectangle>();
        divider.Name(L"LeanDockCardDivider");divider.Width(1);divider.Height(40);
        divider.HorizontalAlignment(x::HorizontalAlignment::Right);divider.VerticalAlignment(x::VerticalAlignment::Center);
        divider.Margin({0,0,2,0});divider.IsHitTestVisible(false);
        x::Media::TranslateTransform shift;shift.X(12+Dashboard::SlotPadding);divider.RenderTransform(shift);
        c::Canvas::SetZIndex(card,20);c::Canvas::SetZIndex(powerCard,20);c::Canvas::SetZIndex(divider,10);
        card.Visibility(x::Visibility::Collapsed);divider.Visibility(x::Visibility::Collapsed);
        powerCard.Visibility(x::Visibility::Collapsed);
        modules.Children().Clear(); // Rebuilt from enabled modules by Render.

        root.Children().Append(card);root.Children().Append(divider);
        auto weak=weak_from_this();
        powerCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)p->OpenPower();});
        clockCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)LeanDockShell::Request(p->hwnd,LeanDockShell::Action::Notifications);});
        startEntry.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)p->OpenStart();});
        mediaToggle.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped&&p->mediaAvailable){p->mediaExpanded=!p->mediaExpanded;p->Render();}});
        // Own the request at the card, before any taskbar ancestor can handle
        // it. The ancestor subscription remains a fallback for window-message
        // and background routes, and sees Handled for this same gesture.
        contextRequested=card.ContextRequested(winrt::auto_revoke,
            [weak](x::UIElement const& sender,x::Input::ContextRequestedEventArgs const& e) noexcept {
                if(auto p=weak.lock();p&&!p->stopped&&p->fits&&p->Visible())try{
                    winrt::Windows::Foundation::Point point{};
                    const bool pointer=e.TryGetPosition(sender,point);
                    if(LeanDockContext::RouteCard(p->ContainsPower(point),pointer,e.Handled())!=LeanDockContext::Action::WinX||
                       !LeanDockShell::IsTaskbar(p->hwnd,GetCurrentProcessId()))return;
                    e.Handled(true);
                    LeanDockMenu::HandleContext(p->hwnd,LeanDockContext::Target::Card,LeanDockMenu::ContextSource::DirectCard);
                }catch(...){LeanDockMenu::Put(p->hwnd,L"LeanDock.Menu.ContextError",static_cast<ULONG_PTR>(winrt::to_hresult()));}
            });
        // Native power retains its original parent. Read-only input diagnostics
        // and the existing middle-click settings gesture use the common root.
        // Dashboard modules do not consume wheel/touchpad input or rotate pages.
        wheelRoot=root.XamlRoot().Content();
        auto tracePointer=[weak](unsigned kind,x::Input::PointerRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped)try{
                auto point=e.GetCurrentPoint(p->card);auto pos=point.Position();
                const bool inside=p->fits&&p->Visible()&&pos.X>=0&&pos.Y>=0&&pos.X<p->card.ActualWidth()&&pos.Y<p->card.ActualHeight();
                using Update=winrt::Windows::UI::Input::PointerUpdateKind;
                if(kind==0)p->middleArmed=inside&&point.Properties().PointerUpdateKind()==Update::MiddleButtonPressed;
                if(kind==1){const bool open=std::exchange(p->middleArmed,false)&&inside&&point.Properties().PointerUpdateKind()==Update::MiddleButtonReleased;
                    if(open){e.Handled(true);p->OpenPreferences();}}
                if(p->ContainsPower(pos))p->TracePowerInput(e.OriginalSource(),pos,kind,e.Handled());
            }catch(...){p->Fail();}
        };
        inputPress=winrt::box_value(x::Input::PointerEventHandler([tracePointer](auto&&,auto const& e){tracePointer(0,e);}));
        inputRelease=winrt::box_value(x::Input::PointerEventHandler([tracePointer](auto&&,auto const& e){tracePointer(1,e);}));
        inputTap=winrt::box_value(x::Input::TappedEventHandler([weak](auto&&,x::Input::TappedRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped)try{
                p->TracePowerInput(e.OriginalSource(),e.GetPosition(p->card),2,e.Handled());
            }catch(...){p->Fail();}
        }));
        wheelRoot.AddHandler(x::UIElement::PointerPressedEvent(),inputPress,true);
        wheelRoot.AddHandler(x::UIElement::PointerReleasedEvent(),inputRelease,true);
        wheelRoot.AddHandler(x::UIElement::TappedEvent(),inputTap,true);
        previous.Click([weak](auto&&,auto&&){Command(weak,0);});
        play.Click([weak](auto&&,auto&&){Command(weak,1);});
        next.Click([weak](auto&&,auto&&){Command(weak,2);});
        unloaded=root.Unloaded(winrt::auto_revoke,[weak](auto&&,auto&&){if(auto p=weak.lock())p->Stop();});
        timer=x::DispatcherTimer();timer.Interval(std::chrono::milliseconds(CpuRefreshMs));
        timer.Tick([weak](auto&&,auto&&){if(auto p=weak.lock())try{p->Tick();}catch(...){p->Fail();}});
        title.Text(L"");previous.IsEnabled(false);play.IsEnabled(false);next.IsEnabled(false);
        ReadPower();
        try{
            chargeChanged=Power::RemainingChargePercentChanged(winrt::auto_revoke,[weak](auto&&,auto&&){QueuePower(weak);});
            batteryChanged=Power::BatteryStatusChanged(winrt::auto_revoke,[weak](auto&&,auto&&){QueuePower(weak);});
            supplyChanged=Power::PowerSupplyStatusChanged(winrt::auto_revoke,[weak](auto&&,auto&&){QueuePower(weak);});
        }catch(...){Fail();} // Opening/revealing the Dock still refreshes power state.
        Render();Request(weak);
    }
    void Fail() noexcept{++errors;Put(hwnd,8,errors);}
    bool ContainsPower(winrt::Windows::Foundation::Point point) const {
        if(!order.Has(Page::Power)||powerCard.Visibility()!=x::Visibility::Visible)return false;
        auto local=card.TransformToVisual(powerCard).TransformPoint(point);
        return local.X>=0&&local.Y>=0&&local.X<powerCard.ActualWidth()&&local.Y<powerCard.ActualHeight();
    }
    bool Visible()const {
        if(!hwnd || !fits || !IsWindowVisible(hwnd) || IsIconic(hwnd))return false;
        RECT r{};MONITORINFO mi{sizeof(mi)};
        if(!GetWindowRect(hwnd,&r)||!GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi))return false;
        // A hidden auto-hide taskbar keeps a one/two-pixel activation strip.
        // Never continue the timer while the body is outside the monitor.
        return r.top<mi.rcMonitor.bottom-int(CardHeight*GetDpiForWindow(hwnd)/96.0);
    }
    void Update(HWND taskbar,double available,double tray) override {
        if(stopped||busy)return;
        busy=true;
        try{
            hwnd=taskbar;

            Put(hwnd,52,std::lround(apps.ActualWidth()*100));Put(hwnd,53,std::lround(root.ActualWidth()*100));
            Put(hwnd,54,std::lround(root.MinWidth()*100));
            if(auto frame=x::Media::VisualTreeHelper::GetParent(root).try_as<x::FrameworkElement>())Put(hwnd,55,std::lround(frame.MinWidth()*100));
            PublishLayout();
            const double requested=dashboardWidth+Dashboard::SlotPadding;
            const double appBudget=available-tray-requested-57;
            // Allocate information first, then compact only the app buttons.
            // The native parent still owns task overflow on very narrow screens.
            const bool nextFits=std::isfinite(appBudget)&&appBudget>=Dashboard::MinimumAppWidth;
            if(nextFits)compactApps.Apply(apps,appBudget);else compactApps.Restore();
            SetPropW(hwnd,L"LeanDock.Card.AppExtent100",reinterpret_cast<HANDLE>(uintptr_t(std::lround(compactApps.Width()*100))+1));
            const double slot=nextFits?requested:0;
            auto padding=root.Padding();if(padding.Left!=12||padding.Right!=12+slot||padding.Top||padding.Bottom)root.Padding({12,0,12+slot,0});
            auto margin=background.Margin();if(margin.Right!=-12-slot||margin.Left!=-13||margin.Top!=-1||margin.Bottom!=-1)background.Margin({-13,-1,-12-slot,-1});
            if(fits!=nextFits){fits=nextFits;card.Visibility(fits?x::Visibility::Visible:x::Visibility::Collapsed);divider.Visibility(card.Visibility());}
            auto lineShift=divider.RenderTransform().as<x::Media::TranslateTransform>();if(lineShift.X()!=12+slot)lineShift.X(12+slot);
            const bool wasShown=shown;shown=Visible();if(shown&&!wasShown)ReadPower();Sampling();Put(hwnd,0,1);Put(hwnd,1,shown);Publish();
            PublishDashboard();PublishSlot();
            if(fits&&order.Has(Page::Power)){auto p=powerCard.TransformToVisual(root.XamlRoot().Content()).TransformPoint({0,0});
                Put(hwnd,28,std::lround(p.X*100));Put(hwnd,29,std::lround((p.X+powerCard.ActualWidth())*100));
                Put(hwnd,30,std::lround(p.Y*100));Put(hwnd,31,std::lround((p.Y+powerCard.ActualHeight())*100));
                auto body=powerContent.TransformToVisual(root.XamlRoot().Content()).TransformPoint({0,0});
                Put(hwnd,40,std::lround(body.X*100));Put(hwnd,41,std::lround((body.X+powerContent.ActualWidth())*100));
                Put(hwnd,42,std::lround(body.Y*100));Put(hwnd,43,std::lround((body.Y+powerContent.ActualHeight())*100));}
            else{for(unsigned i=28;i<=31;++i)RemovePropW(hwnd,Props[i]);for(unsigned i=40;i<=43;++i)RemovePropW(hwnd,Props[i]);}
            if(fits){
                auto content=root.XamlRoot().Content();
                auto pos=card.TransformToVisual(content).TransformPoint({0,0});
                auto appPos=apps.TransformToVisual(content).TransformPoint({0,0});
                auto rootPos=root.TransformToVisual(content).TransformPoint({0,0});
                // Read-only geometry evidence, centi-DIP relative to the island.
                Put(hwnd,10,std::lround(pos.X*100));Put(hwnd,11,std::lround(pos.Y*100));
                Put(hwnd,12,std::lround((pos.X+card.ActualWidth())*100));Put(hwnd,13,std::lround((pos.Y+card.ActualHeight())*100));
                Put(hwnd,14,std::lround((appPos.X+apps.ActualWidth())*100));Put(hwnd,15,std::lround((rootPos.X+root.ActualWidth())*100));
                if(mediaAvailable&&mediaExpanded){
                    auto buttons=mediaControls.TransformToVisual(content).TransformPoint({0,0});
                    auto text=title.TransformToVisual(content).TransformPoint({0,0});
                    Put(hwnd,17,std::lround(buttons.X*100));Put(hwnd,18,std::lround((buttons.X+mediaControls.ActualWidth())*100));
                    Put(hwnd,19,std::lround(buttons.Y*100));Put(hwnd,20,std::lround((buttons.Y+mediaControls.ActualHeight())*100));
                    Put(hwnd,21,std::lround(text.X*100));Put(hwnd,22,std::lround((text.X+title.ActualWidth())*100));Put(hwnd,23,std::lround(play.ActualWidth()*100));
                    Put(hwnd,39,std::lround(title.FontSize()*100));
                    Put(hwnd,44,std::lround(title.ActualHeight()*100));Put(hwnd,45,std::lround(media.RowDefinitions().GetAt(0).ActualHeight()*100));
                    Put(hwnd,46,std::lround(text.Y*100));Put(hwnd,47,std::lround((text.Y+title.ActualHeight())*100));
                }else{for(unsigned i=17;i<=23;++i)RemovePropW(hwnd,Props[i]);RemovePropW(hwnd,Props[39]);for(unsigned i=44;i<=47;++i)RemovePropW(hwnd,Props[i]);}
            }
            Put(hwnd,16,fits);
        }catch(...){Fail();}
        busy=false;
    }
    void ScheduleTimer() {
        if(stopped||!shown||!fits){timer.Stop();return;}
        const auto delay=Dashboard::Delay(GetTickCount64(),sampling,cpuDue,
            clocking,clockDue,networking,networkPending,networkDue);
        if(!delay){timer.Stop();return;}
        const auto interval=std::chrono::milliseconds(delay);
        if(timer.Interval()!=interval)timer.Interval(interval);
        timer.Start();
    }
    void Sampling() {
        const bool active=shown&&fits&&!stopped;
        const bool want=active&&order.Has(Page::Metrics),wantClock=active&&order.Has(Page::Clock),wantNetwork=active&&order.Has(Page::Network);
        if(want==sampling&&wantClock==clocking&&wantNetwork==networking)return;
        timer.Stop();
        if(want!=sampling){sampling=want;cpu.Pause();
            if(want){const auto now=GetTickCount64();cpu.Resume(Times(),now);cpuDue=now+cpu.IntervalMs();UpdateCpuVisual();ReadMemory();}
            else{Put(hwnd,37,0);Put(hwnd,38,0);}}
        if(wantClock!=clocking){clocking=wantClock;if(wantClock)ReadClock();}
        if(wantNetwork!=networking){networking=wantNetwork;network.Pause();++networkGeneration;UpdateNetworkVisual();
            if(wantNetwork){networkDue=GetTickCount64();QueueNetwork();}}
        Put(hwnd,2,sampling);Put(hwnd,48,clocking);ScheduleTimer();
    }
    void ReloadPreferences() noexcept override {
        try{auto weak=weak_from_this();dispatcher.RunAsync(CoreDispatcherPriority::Normal,[weak]{
            if(auto p=weak.lock();p&&!p->stopped)try{
                PageOrder order;Page selected=p->page;
                if(!(p->preferencesValid=ReadPreferences(p->directory,order,selected))){p->Fail();p->Publish();return;}
                p->order=std::move(order);p->page=p->order.Select(p->page);
                p->ReadPower();p->Render();
            }catch(...){p->Fail();}
        });}catch(...){}
    }
    void OpenPreferences() {
        if(directory.empty()){Fail();return;}
        auto exe=directory+L"\\latest.exe";
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",exe.c_str(),L"--cards",directory.c_str(),SW_SHOWNORMAL))<=32)Fail();
    }
    void OpenStart() noexcept {
        // Use the original Start control's native Invoke path when exposed by
        // this template. The fixed Win-key fallback exists only for this click.
        try{
            std::vector<x::DependencyObject> pending{root.XamlRoot().Content()};unsigned visited=0;
            while(!pending.empty()&&visited++<1024){auto node=pending.back();pending.pop_back();
                if(auto fe=node.try_as<x::FrameworkElement>();fe&&x::Automation::AutomationProperties::GetAutomationId(fe)==L"StartButton"){
                    auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(fe);
                    if(peer)if(auto invoke=peer.GetPattern(x::Automation::Peers::PatternInterface::Invoke).try_as<x::Automation::Provider::IInvokeProvider>()){
                        invoke.Invoke();return;
                    }
                    break;
                }
                const int n=x::Media::VisualTreeHelper::GetChildrenCount(node);
                for(int i=0;i<n&&pending.size()<1024;++i)pending.push_back(x::Media::VisualTreeHelper::GetChild(node,i));
            }
        }catch(...){}
        LeanDockShell::Request(hwnd,LeanDockShell::Action::Start);
    }
    void UpdateNetworkVisual() {
        auto rx=FormatRate(network.value?std::optional(network.value->received):std::nullopt);
        auto tx=FormatRate(network.value?std::optional(network.value->sent):std::nullopt);
        networkReceive.Text(rx.CompactText());networkSend.Text(tx.CompactText());
        x::Automation::AutomationProperties::SetName(networkCard,L"网络总发送 "+tx.PerSecondText()+L"，总接收 "+rx.PerSecondText()+L"");
        Put(hwnd,57,networking);Put(hwnd,58,networkSamples);
        Put(hwnd,63,network.baseline?network.baseline->size():0);
        if(network.value){Put(hwnd,59,uintptr_t(std::min(network.value->received,double(UINT32_MAX))));Put(hwnd,60,uintptr_t(std::min(network.value->sent,double(UINT32_MAX))));}
        else if(hwnd){RemovePropW(hwnd,Props[59]);RemovePropW(hwnd,Props[60]);}
    }
    void QueueNetwork(){
        if(networkPending||!networking||stopped)return;networkPending=true;
        ReadNetworkAsync(weak_from_this(),dispatcher,networkGeneration);
    }
    static winrt::fire_and_forget ReadNetworkAsync(std::weak_ptr<Board> weak,winrt::Windows::UI::Core::CoreDispatcher dispatcher,unsigned ticket) {
        // Driver enumeration runs away from Explorer's input thread. One read
        // at a time; late completions after switching/closing cannot update UI.
        try{
            co_await winrt::resume_background();
            std::optional<NetworkSnapshot> snapshot;try{snapshot=ReadNetwork();}catch(...){}
            const auto measuredAt=GetTickCount64();
            co_await winrt::resume_foreground(dispatcher);
            if(auto p=weak.lock();p&&!p->stopped){p->networkPending=false;
                if(p->networking&&p->networkGeneration==ticket){
                    if(p->network.Sample(std::move(snapshot),measuredAt))++p->networkSamples;
                    p->networkDue=measuredAt+p->network.IntervalMs();p->UpdateNetworkVisual();
                }
                p->ScheduleTimer();
            }
        }catch(...){} // A destroyed dispatcher needs no retry or retained Board.
    }
    void PublishLayout() {
        // Small, read-only numeric snapshot on the existing layout event. No
        // app titles or content, and no full-tree walk or background polling.
        auto describe=[](x::FrameworkElement const& e){
            auto name=std::wstring(e.Name());
            for(auto& ch:name)if(!((ch>=L'a'&&ch<=L'z')||(ch>=L'A'&&ch<=L'Z')||(ch>=L'0'&&ch<=L'9')||ch==L'_'))ch=L'_';
            return L"{\"name\":\""+name+L"\",\"actual\":"+std::to_wstring(e.ActualWidth())+
                L",\"desired\":"+std::to_wstring(e.DesiredSize().Width)+L",\"min\":"+std::to_wstring(e.MinWidth())+
                L",\"visible\":"+std::to_wstring(e.Visibility()==x::Visibility::Visible)+L"}";
        };
        auto data=L"{\"root\":"+describe(root)+L",\"children\":[";
        bool first=true;for(auto const& child:root.Children())if(auto element=child.try_as<x::FrameworkElement>()){
            if(!first)data+=L",";first=false;data+=describe(element);
        }data+=L"],\"modules\":[";first=true;
        for(auto const& child:modules.Children())if(auto element=child.try_as<x::FrameworkElement>();element&&element.Visibility()==x::Visibility::Visible){
            if(!first)data+=L",";first=false;
            auto pos=element.TransformToVisual(card).TransformPoint({0,0});
            data+=L"{\"layout\":"+describe(element)+L",\"x\":"+std::to_wstring(pos.X)+L",\"y\":"+std::to_wstring(pos.Y)+L",\"height\":"+std::to_wstring(element.ActualHeight())+L"}";
        }
        data+=L"],\"dashboardWidth\":"+std::to_wstring(dashboardWidth)+L",\"appExtent\":"+std::to_wstring(compactApps.Width())+L"}";
        if(data!=lastLayout){InternalWh_SetStringValue(nullptr,L"cardsLayoutState",data.c_str());lastLayout=std::move(data);}
    }
    void ReadClock() {
        SYSTEMTIME now{};GetLocalTime(&now);
        auto text=ClockText(now.wHour,now.wMinute,now.wMonth,now.wDay);
        if(clockTime.Text()!=text.time)clockTime.Text(text.time);
        if(clockDate.Text()!=text.date)clockDate.Text(text.date);
        x::Automation::AutomationProperties::SetName(clockCard,text.date+L" "+text.time+L"，打开日历和通知");
        Put(hwnd,49,now.wHour*100+now.wMinute);Put(hwnd,50,now.wMonth*100+now.wDay);Put(hwnd,51,++clockUpdates);
        clockDue=GetTickCount64()+NextMinuteMs(now.wSecond,now.wMilliseconds);
    }
    void TimeChanged() noexcept override {
        try{auto weak=weak_from_this();dispatcher.RunAsync(CoreDispatcherPriority::Normal,[weak]{
            if(auto p=weak.lock();p&&!p->stopped&&p->clocking)try{p->ReadClock();p->ScheduleTimer();}catch(...){p->Fail();}
        });}catch(...){}
    }
    void UpdateCpuVisual(){
        UpdateMetricsText();
        if(cpu.value)Put(hwnd,4,*cpu.value);else RemovePropW(hwnd,Props[4]);
        Put(hwnd,37,cpu.refreshing);Put(hwnd,38,sampling?cpu.IntervalMs():0);
    }
    winrt::hstring CpuText()const{
        if(cpu.refreshing)return cpu.value?L"更新中（上次 "+PercentText(cpu.value)+L"）":L"采样中";
        return PercentText(cpu.value);
    }
    void UpdateMetricsText(){
        metricsSummary.Text(L"CPU "+PercentText(cpu.value)+L"  内存 "+PercentText(memoryPercent));
    }
    void ReadMemory(){MEMORYSTATUSEX s{sizeof(s)};if(GlobalMemoryStatusEx(&s)){
        memoryPercent=s.dwMemoryLoad;memoryUsage.Text(Dashboard::MemoryText(s.ullTotalPhys,s.ullAvailPhys));
        Put(hwnd,5,s.dwMemoryLoad);
    }else{memoryPercent.reset();memoryUsage.Text(L"— / — GB");RemovePropW(hwnd,Props[5]);}
        UpdateMetricsText();MetricsDescription();
    }
    void MetricsDescription() {
        x::Automation::AutomationProperties::SetName(metrics,L"CPU "+CpuText()+L"，内存 "+PercentText(memoryPercent)+L"，已用／总量 "+memoryUsage.Text());
    }
    void Tick(){
        if(stopped)return;shown=Visible();Put(hwnd,1,shown);Sampling();
        if(!shown||!fits)return;
        const auto now=GetTickCount64();
        if(clocking&&now>=clockDue)ReadClock();
        if(networking&&!networkPending&&now>=networkDue)QueueNetwork();
        if(sampling&&now>=cpuDue){
            const bool measured=cpu.Sample(Times(),now);cpuDue=now+cpu.IntervalMs();UpdateCpuVisual();ReadMemory();
            if(measured){++samples;Put(hwnd,3,samples);}
        }
        ScheduleTimer();
    }
    void Publish(){Put(hwnd,3,samples);Put(hwnd,6,bool(session));Put(hwnd,7,static_cast<unsigned>(Page::Count));Put(hwnd,8,errors);Put(hwnd,9,commands);
        if(battery.percent>=0)Put(hwnd,25,battery.percent);else if(hwnd)RemovePropW(hwnd,Props[25]);Put(hwnd,26,battery.charging);Put(hwnd,27,battery.plugged);
        Put(hwnd,34,static_cast<unsigned>(Mode(battery)));Put(hwnd,35,powerUpdates);Put(hwnd,36,switches);Put(hwnd,61,order.pages.size());Put(hwnd,62,preferencesValid);}
    void PublishSlot() {
        if(!hwnd)return;
        const bool ready=fits&&card.IsLoaded()&&card.ActualWidth()>0&&card.ActualHeight()>0;
        const bool active=ready&&order.Has(Page::Power);
        const bool changed=reinterpret_cast<uintptr_t>(GetPropW(hwnd,Props[24]))!=uintptr_t(ready+1)||
            reinterpret_cast<uintptr_t>(GetPropW(hwnd,Props[32]))!=uintptr_t(active+1);
        Put(hwnd,24,ready);Put(hwnd,32,active);
        Put(hwnd,33,fits?(metricsCard.Visibility()==x::Visibility::Visible)+
            (media.Visibility()==x::Visibility::Visible)+(powerCard.Visibility()==x::Visibility::Visible)+
            (clockCard.Visibility()==x::Visibility::Visible)+(networkCard.Visibility()==x::Visibility::Visible):0);
        if(changed)Native_tray_refresh_cards();
    }
    void ReadPower(){
        SYSTEM_POWER_STATUS status{};const bool ok=GetSystemPowerStatus(&status)!=FALSE;
        battery=BatteryState(ok,status.BatteryLifePercent,status.BatteryFlag,status.ACLineStatus);
        powerText.Text(battery.percent>=0?winrt::to_hstring(battery.percent)+L"%":battery.presentKnown&&!battery.present?L"电源":L"—");
        // Share the border's actual inner layout, including rounding. At 100%
        // the fill occupies that whole area; zero/unknown has no filled column.
        const double charge=std::clamp<double>(battery.percent,0,100);
        powerLevelColumn.Width({charge,x::GridUnitType::Star});
        powerEmptyColumn.Width({100.0-charge,x::GridUnitType::Star});
        powerBolt.Visibility(battery.charging?x::Visibility::Visible:x::Visibility::Collapsed);
        auto mode=Mode(battery);
        if(mode==BatteryMode::Charging||mode==BatteryMode::Full)
            powerFill.Fill(x::Media::SolidColorBrush({255,48,178,100}));
        else if(mode==BatteryMode::Critical)powerFill.Fill(x::Media::SolidColorBrush({255,223,78,65}));
        else if(mode==BatteryMode::Low)powerFill.Fill(x::Media::SolidColorBrush({255,213,153,42}));
        else powerFill.Fill(powerText.Foreground());
        const auto state=BatteryCaption(battery);
        powerStatus.Text(mode==BatteryMode::Plugged?L"插电未充电":mode==BatteryMode::NoBattery?L"未检测到电池":
            mode==BatteryMode::Unknown?(battery.plugged?L"已插电 · 未知":L"电源状态未知"):state);
        x::Automation::AutomationProperties::SetName(powerCard,L"电池 "+powerText.Text()+L"，"+state+L"，打开快速设置");
        ++powerUpdates;Publish();
    }
    void TracePowerInput(IInspectable const& source,winrt::Windows::Foundation::Point point,unsigned kind,bool handled) {
        if(!hwnd||!fits||!ContainsPower(point))return;
        Native_hide_expanded(hwnd);
        BOOL frozen=FALSE;
        if(SUCCEEDED(DwmGetWindowAttribute(hwnd,DWMWA_FREEZE_REPRESENTATION,&frozen,sizeof(frozen))))
            SetPropW(hwnd,L"LeanDock.Resume.FrozenAtInput",reinterpret_cast<HANDLE>(uintptr_t(frozen!=FALSE)+1));
        else RemovePropW(hwnd,L"LeanDock.Resume.FrozenAtInput");
        const auto xamlRoot=root.XamlRoot();
        SetPropW(hwnd,L"LeanDock.Resume.HostVisible",reinterpret_cast<HANDLE>(uintptr_t(xamlRoot.IsHostVisible())+1));
        SetPropW(hwnd,L"LeanDock.Resume.RootHit",reinterpret_cast<HANDLE>(uintptr_t(xamlRoot.Content().IsHitTestVisible())+1));
        const auto className=winrt::get_class_name(source);
        const unsigned sourceKind=className==L"SystemTray.IconView"?1:className==L"Windows.UI.Xaml.Controls.Grid"?2:
            className==L"Windows.UI.Xaml.Controls.Border"?3:className==L"Windows.UI.Xaml.Controls.ContentPresenter"?4:
            className==L"SystemTray.OmniButton"?5:0;
        SetPropW(hwnd,L"LeanDock.Resume.InputClass",reinterpret_cast<HANDLE>(uintptr_t(sourceKind)+1));
        unsigned origin=0;
        auto node=source.try_as<x::DependencyObject>();
        for(unsigned depth=0;node&&depth<32;++depth,node=x::Media::VisualTreeHelper::GetParent(node)){
            if(auto fe=node.try_as<x::FrameworkElement>()){
                if(fe.Name()==L"ControlCenterButton"){origin=1;break;}
                if(fe.Name()==L"LeanDockPowerCard"){origin=2;break;}
                if(fe.Name()==L"LeanDockCard"){origin=3;break;}
            }
        }
        InputCount(hwnd,kind);InputValue(hwnd,3,kind);InputValue(hwnd,4,origin);
        InputValue(hwnd,5,handled);InputValue(hwnd,6,GetTickCount64());
        auto native=inputNative.get();
        if(!native||!native.IsLoaded()){native=NativeQuickSettings(root.XamlRoot().Content());if(native)inputNative=winrt::make_weak(native);}
        if(native){
            const auto content=LeanDockPower::Inspect(native);
            const bool targetSource=content.visible==1&&LeanDockPower::DescendsFrom(source.try_as<x::DependencyObject>(),content.target);
            SetPropW(hwnd,L"LeanDock.Resume.InputIcon",reinterpret_cast<HANDLE>(uintptr_t(targetSource)+1));
            InputValue(hwnd,13,content.icons);InputValue(hwnd,14,content.loaded);InputValue(hwnd,15,native.IsTapEnabled());
            auto delta=native.TransformToVisual(card).TransformPoint({0,0});
            InputValue(hwnd,7,std::lround(delta.X*100));InputValue(hwnd,8,std::lround(delta.Y*100));
            InputValue(hwnd,10,native.IsHitTestVisible());
            if(auto control=native.try_as<c::Control>()){
                InputValue(hwnd,9,control.IsEnabled());InputValue(hwnd,11,static_cast<unsigned>(control.FocusState()));
            }
        }
    }
    void OpenPower() noexcept {
        InputCount(hwnd,12);
        try{
            if(auto native=NativeQuickSettings(root.XamlRoot().Content())){
                auto peer=x::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(native);
                if(peer)if(auto invoke=peer.GetPattern(x::Automation::Peers::PatternInterface::Invoke).try_as<x::Automation::Provider::IInvokeProvider>()){
                    invoke.Invoke();return;
                }
            }
        }catch(...){} // A Windows template without Invoke uses the native shortcut.
        LeanDockShell::Request(hwnd,LeanDockShell::Action::QuickSettings);
    }
    static void QueuePower(std::weak_ptr<Board> weak) noexcept {
        try{if(auto p=weak.lock();p&&!p->powerQueued.exchange(true))p->dispatcher.RunAsync(CoreDispatcherPriority::Normal,[weak]{
            if(auto q=weak.lock()){q->powerQueued=false;if(!q->stopped)try{q->ReadPower();}catch(...){q->Fail();}}
        });}catch(...){if(auto p=weak.lock())p->powerQueued=false;}
    }
    void Render(){
        if(stopped)return;
        if(!session){mediaAvailable=false;mediaExpanded=false;title.Text(L"");artistName=L"";
            previous.IsEnabled(false);play.IsEnabled(false);next.IsEnabled(false);playGlyph.Text(L"\xE768");}
        if(!mediaAvailable)mediaExpanded=false;
        clockCard.Visibility(order.Has(Page::Clock)?x::Visibility::Visible:x::Visibility::Collapsed);
        clockGroup.Width(order.Has(Page::Clock)?Dashboard::ClockWidth:Dashboard::StartWidth);
        metricsCard.Visibility(order.Has(Page::Metrics)?x::Visibility::Visible:x::Visibility::Collapsed);
        powerCard.Visibility(order.Has(Page::Power)?x::Visibility::Visible:x::Visibility::Collapsed);
        networkCard.Visibility(order.Has(Page::Network)?x::Visibility::Visible:x::Visibility::Collapsed);
        mediaGroup.Visibility(order.Has(Page::Media)&&mediaAvailable?x::Visibility::Visible:x::Visibility::Collapsed);
        ArrangeDashboardMedia(mediaGroup,media,mediaToggle,mediaAvailable,mediaExpanded);
        x::Automation::AutomationProperties::SetName(mediaToggle,mediaExpanded?L"收起媒体控制":L"展开媒体控制");
        auto key=order.Text()+(mediaAvailable?L"/media":L"/empty");
        if(key!=moduleKey){moduleKey=key;modules.Children().Clear();
            if(!order.Has(Page::Clock))modules.Children().Append(clockGroup);
            for(auto p:order.pages){switch(p){
                case Page::Clock:modules.Children().Append(clockGroup);break;
                case Page::Metrics:modules.Children().Append(metricsCard);break;
                case Page::Power:modules.Children().Append(powerCard);break;
                case Page::Network:modules.Children().Append(networkCard);break;
                case Page::Media:if(mediaAvailable)modules.Children().Append(mediaGroup);break;
                default:break;
            }}
        }
        const double width=Dashboard::Width(order,mediaAvailable,mediaExpanded);
        if(width!=dashboardWidth){dashboardWidth=width;card.Width(width);card.Margin({0,0,-width,0});root.InvalidateMeasure();}
        x::Automation::AutomationProperties::SetName(card,L"Dock 小组件");
        PublishDashboard();
        Sampling();Publish();PublishSlot();
    }
    void PutDashboard(const wchar_t* suffix,uintptr_t value){auto name=std::wstring(L"LeanDock.Card.Dashboard.")+suffix;SetPropW(hwnd,name.c_str(),reinterpret_cast<HANDLE>(value+1));}
    void PublishDashboard(){if(hwnd){SetPropW(hwnd,L"LeanDock.Card.Dashboard",reinterpret_cast<HANDLE>(1));
        PutDashboard(L"Width100",std::lround(dashboardWidth*100));PutDashboard(L"MediaAvailable",mediaAvailable);PutDashboard(L"MediaExpanded",mediaExpanded);}}
    static winrt::fire_and_forget Request(std::weak_ptr<Board> weak) {
        try{
            auto p=weak.lock();if(!p||p->stopped)co_return;
            auto op=Manager::RequestAsync();p->request=op;p.reset();
            auto result=co_await op;
            p=weak.lock();if(!p||p->stopped)co_return;
            p->request=nullptr;p->manager=result;
            p->managerChanged=result.CurrentSessionChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
            p->sessionsChanged=result.SessionsChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
            p->Refresh();
        }catch(...){if(auto p=weak.lock();p&&!p->stopped){p->Fail();try{p->Render();}catch(...){}}}
    }
    static void Queue(std::weak_ptr<Board> weak) noexcept {
        try{if(auto p=weak.lock();p && !p->refreshQueued.exchange(true)){
            // Media callbacks can run on a worker thread. Only queue weak
            // references; all session/XAML state remains UI-thread owned.
            p->dispatcher.RunAsync(CoreDispatcherPriority::Normal,[weak]{if(auto q=weak.lock()){
                q->refreshQueued=false;if(!q->stopped)try{q->Refresh();}catch(...){q->Fail();}
            }});
        }}catch(...){if(auto p=weak.lock())p->refreshQueued=false;}
    }
    void Refresh() noexcept try {
        if(stopped || !manager)return;
        auto current=manager.GetCurrentSession();
        if(current!=session){
            CancelCommand();
            propertiesChanged.revoke();playbackChanged.revoke();session=current;
            title.Text(L"");artistName=L"";mediaExpanded=false;
            if(session){auto weak=weak_from_this();
                propertiesChanged=session.MediaPropertiesChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
                playbackChanged=session.PlaybackInfoChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
            }
        }
        ++generation;if(metadata){metadata.Cancel();metadata=nullptr;}
        if(session){
            auto info=session.GetPlaybackInfo();auto controls=info.Controls();
            previous.IsEnabled(controls.IsPreviousEnabled()&&!commandGate.busy);next.IsEnabled(controls.IsNextEnabled()&&!commandGate.busy);
            play.IsEnabled(controls.IsPlayPauseToggleEnabled()&&!commandGate.busy);
            mediaAvailable=info.PlaybackStatus()!=m::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed&&
                (controls.IsPreviousEnabled()||controls.IsNextEnabled()||controls.IsPlayPauseToggleEnabled());
            const bool playing=info.PlaybackStatus()==m::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            Put(hwnd,56,playing);
            playGlyph.Text(playing?L"\xE769":L"\xE768");
            x::Automation::AutomationProperties::SetName(play,playing?L"暂停":L"播放");
            Metadata(weak_from_this(),session,generation);
        }else{Put(hwnd,56,0);previous.IsEnabled(false);play.IsEnabled(false);next.IsEnabled(false);playGlyph.Text(L"\xE768");}
        Render();
    }catch(...){
        Fail();if(hwnd)RemovePropW(hwnd,Props[56]);++generation;CancelCommand();
        propertiesChanged.revoke();playbackChanged.revoke();session=nullptr;
        try{if(metadata)metadata.Cancel();metadata=nullptr;Render();}catch(...){}
    }
    static winrt::fire_and_forget Metadata(std::weak_ptr<Board> weak,Session source,unsigned ticket){
        try{
            auto p=weak.lock();if(!p||p->stopped)co_return;
            auto op=source.TryGetMediaPropertiesAsync();p->metadata=op;p.reset();
            auto data=co_await op;
            p=weak.lock();if(!p||p->stopped||p->generation!=ticket||p->session!=source)co_return;
            p->metadata=nullptr;
            p->title.Text(data.Title());p->artistName=data.Artist();
            x::Automation::AutomationProperties::SetName(p->media,p->title.Text()+L" "+p->artistName);
            p->Render();
        }catch(winrt::hresult_canceled const&){}catch(...){if(auto p=weak.lock();p&&!p->stopped&&p->generation==ticket)p->Fail();}
    }
    static winrt::fire_and_forget Command(std::weak_ptr<Board> weak,int action){
        Session source{nullptr};uint64_t ticket=0;
        try{
            auto p=weak.lock();if(!p||p->stopped||!p->session||!(ticket=p->commandGate.Begin()))co_return;
            source=p->session;p->previous.IsEnabled(false);p->play.IsEnabled(false);p->next.IsEnabled(false);
            auto op=action==0?source.TrySkipPreviousAsync():action==1?source.TryTogglePlayPauseAsync():source.TrySkipNextAsync();
            p->command=op;p.reset();
            bool success=co_await op;
            if(auto q=weak.lock();q&&!q->stopped&&q->commandGate.Finish(ticket)){
                q->command=nullptr;if(success)++q->commands;else q->Fail();q->Refresh();
            }
        }catch(...){if(auto p=weak.lock();p&&!p->stopped&&p->commandGate.Finish(ticket)){
            p->command=nullptr;p->Fail();try{p->Refresh();}catch(...){}}
        }
    }
    void CancelCommand() noexcept {
        commandGate.Invalidate();auto pending=std::exchange(command,nullptr);
        try{if(pending)pending.Cancel();}catch(...){}
    }
    void Stop() noexcept override {
        if(stopped)return;stopped=true;++generation;++networkGeneration;
        compactApps.Restore();
        CancelCommand();
        try{if(timer)timer.Stop();}catch(...){}
        try{if(request)request.Cancel();if(metadata)metadata.Cancel();}catch(...){}
        managerChanged.revoke();sessionsChanged.revoke();propertiesChanged.revoke();playbackChanged.revoke();unloaded.revoke();contextRequested.revoke();
        chargeChanged.revoke();batteryChanged.revoke();supplyChanged.revoke();
        try{if(wheelRoot&&wheelHandler)wheelRoot.RemoveHandler(x::UIElement::PointerWheelChangedEvent(),wheelHandler);}catch(...){}
        try{if(wheelRoot&&inputPress)wheelRoot.RemoveHandler(x::UIElement::PointerPressedEvent(),inputPress);}catch(...){}
        try{if(wheelRoot&&inputRelease)wheelRoot.RemoveHandler(x::UIElement::PointerReleasedEvent(),inputRelease);}catch(...){}
        try{if(wheelRoot&&inputTap)wheelRoot.RemoveHandler(x::UIElement::TappedEvent(),inputTap);}catch(...){}
        inputPress=nullptr;inputRelease=nullptr;inputTap=nullptr;inputNative={};
        wheelHandler=nullptr;wheelRoot=nullptr;
        try{if(root){for(auto e:{card.try_as<x::UIElement>(),divider.try_as<x::UIElement>()}){uint32_t index=0;if(e&&root.Children().IndexOf(e,index))root.Children().RemoveAt(index);}
            if(oldPadding==x::DependencyProperty::UnsetValue())root.ClearValue(c::Grid::PaddingProperty());else root.SetValue(c::Grid::PaddingProperty(),oldPadding);
        }}catch(...){}
        try{if(background){if(oldMargin==x::DependencyProperty::UnsetValue())background.ClearValue(x::FrameworkElement::MarginProperty());else background.SetValue(x::FrameworkElement::MarginProperty(),oldMargin);}}catch(...){}
        for(auto key:Props)if(hwnd)RemovePropW(hwnd,key);
        for(auto key:InputProps)if(hwnd)RemovePropW(hwnd,key);
        for(auto key:WheelProps)if(hwnd)RemovePropW(hwnd,key);
        if(hwnd)for(auto name:{L"LeanDock.Shell.LastRequest",L"LeanDock.Shell.LastAction",L"LeanDock.Shell.LastResult",L"LeanDock.Shell.PowerUser",L"LeanDock.Shell.QuickSettings",L"LeanDock.Shell.Notifications",L"LeanDock.Shell.Start",L"LeanDock.Card.Dashboard",L"LeanDock.Card.Dashboard.Width100",L"LeanDock.Card.Dashboard.MediaAvailable",L"LeanDock.Card.Dashboard.MediaExpanded",L"LeanDock.Card.AppExtent100"})RemovePropW(hwnd,name);
        session=nullptr;manager=nullptr;
    }
    bool Alive()const noexcept override{return !stopped;}
    x::FrameworkElement MenuTarget()const noexcept override {
        try{
            if(!stopped&&fits&&card&&card.IsLoaded()&&card.Visibility()==x::Visibility::Visible&&
               card.ActualWidth()>0&&card.ActualHeight()>0)return card;
        }catch(...){}
        return nullptr;
    }
};
std::shared_ptr<Host> Create(c::Grid const& root){auto p=std::make_shared<Board>();p->Init(root);return p;}
}
