#include "cards-network-latest.h"
#include "cards-latest.h"
#include "cards-core-latest.h"
#include "cards-input-latest.h"
#include "cards-manipulation-latest.h"
#include "cards-input-pin-generated.h"
#include "verify-latest.h"
#include "cards-preferences-latest.h"
#include "shell-actions-latest.h"
#include "shell-menu-latest.h"
#include "power-target-latest.h"
#include <dwmapi.h>
#include <shellapi.h>
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
// Called on the owning XAML thread after the selected page changes. This
// retires the previous native hit target before another page accepts input.
extern "C" void Native_tray_refresh_cards();
extern "C" int Native_hide_expanded(HWND window);
extern "C" BOOL InternalWh_SetStringValue(void*,PCWSTR,PCWSTR);
extern "C" size_t InternalWh_GetModStoragePath(void*,PWSTR,size_t);
extern "C" BOOL InternalWh_SetFunctionHook(void*,void*,void*,void**);
extern "C" BOOL InternalWh_ApplyHookOperations(void*);

namespace LeanDockCards {
static bool InstallInputProcedure(WNDPROC candidate,WNDPROC hook,WNDPROC* original) noexcept {
    // Share the runtime's hook chain, shutdown gate and process-long trampoline.
    // Bind runs on a taskbar XAML thread, so installation must be idempotent
    // across secondary taskbars and later recreation of a Board.
    static std::mutex mutex;static WNDPROC target{};
    static bool checked=false,valid=false,registered=false,applied=false;
    std::lock_guard guard(mutex);
    if(target&&target!=candidate)return false;
    if(!checked){
        target=candidate;checked=true;
        MEMORY_BASIC_INFORMATION region{};HMODULE module{};wchar_t path[32768]{},hash[65]{};
        if(!VirtualQuery(reinterpret_cast<void*>(candidate),&region,sizeof(region))||region.Type!=MEM_IMAGE)return false;
        const auto access=region.Protect&0xff;
        if(access!=PAGE_EXECUTE&&access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)return false;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<PCWSTR>(candidate),&module)||region.AllocationBase!=module||
            !GetModuleFileNameW(module,path,std::size(path))||_wcsicmp(path,NativeCardsInputModule.path)||
            !NativeValidModule(module,NativeCardsInputModule)||!NativeHashFile(path,hash)||_wcsicmp(hash,NativeCardsInputModule.sha256))return false;
        // Keep the system module mapped for exactly as long as our trampoline.
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<PCWSTR>(candidate),&module))return false;
        valid=true;
    }
    if(!valid)return false;
    if(!registered)registered=InternalWh_SetFunctionHook(nullptr,reinterpret_cast<void*>(candidate),
        reinterpret_cast<void*>(hook),reinterpret_cast<void**>(original));
    if(registered&&!applied)applied=InternalWh_ApplyHookOperations(nullptr);
    return applied;
}
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
struct RingDrawing {
    x::Shapes::Path path{nullptr};x::Media::PathGeometry partial{nullptr};
    x::Media::ArcSegment segment{nullptr};x::Media::EllipseGeometry full{nullptr};
    void Init(x::Shapes::Path const& element) {
        path=element;partial=x::Media::PathGeometry();segment=x::Media::ArcSegment();
        segment.Size({float(RingRadius),float(RingRadius)});segment.SweepDirection(x::Media::SweepDirection::Clockwise);
        x::Media::PathFigure figure;figure.StartPoint({float(RingSize/2),float(RingStroke/2)});
        figure.IsClosed(false);figure.IsFilled(false);figure.Segments().Append(segment);partial.Figures().Append(figure);
        full=x::Media::EllipseGeometry();full.Center({float(RingSize/2),float(RingSize/2)});
        full.RadiusX(RingRadius);full.RadiusY(RingRadius);
    }
    void Set(std::optional<unsigned> percent) {
        auto arc=GaugeArc(percent);
        if(!arc.visible){path.Data(nullptr);return;}
        if(arc.full){path.Data(full);return;}
        segment.Point({float(arc.x),float(arc.y)});segment.IsLargeArc(arc.large);path.Data(partial);
    }
};
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
    x::UIElement wheelRoot{nullptr};IInspectable wheelHandler{nullptr},inputPress{nullptr},inputRelease{nullptr},inputTap{nullptr};
    winrt::weak_ref<x::FrameworkElement> inputNative;
    Power::RemainingChargePercentChanged_revoker chargeChanged;
    Power::BatteryStatusChanged_revoker batteryChanged;
    Power::PowerSupplyStatusChanged_revoker supplyChanged;
    c::Grid metrics{nullptr},media{nullptr}; c::StackPanel clock{nullptr};
    c::Button metricsCard{nullptr},clockCard{nullptr},networkCard{nullptr};
    c::TextBlock networkReceive{nullptr},networkSend{nullptr};
    c::TextBlock title{nullptr},clockTime{nullptr},clockDate{nullptr};
    c::StackPanel mediaControls{nullptr};winrt::hstring artistName;
    RingDrawing cpuRing,memoryRing;CpuSampler cpu;std::optional<unsigned> memoryPercent;
    c::Button previous{nullptr},play{nullptr},next{nullptr};c::TextBlock playGlyph{nullptr};
    x::DispatcherTimer timer{nullptr};winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
    Manager manager{nullptr};Session session{nullptr};
    Manager::CurrentSessionChanged_revoker managerChanged;
    Manager::SessionsChanged_revoker sessionsChanged;
    Session::MediaPropertiesChanged_revoker propertiesChanged;
    Session::PlaybackInfoChanged_revoker playbackChanged;
    x::FrameworkElement::Unloaded_revoker unloaded;
    winrt::Windows::Foundation::IAsyncOperation<Manager> request{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<m::GlobalSystemMediaTransportControlsSessionMediaProperties> metadata{nullptr};
    winrt::Windows::Foundation::IAsyncOperation<bool> command{nullptr};
    CommandGate commandGate;
    HWND hwnd=nullptr; bool stopped=false,shown=false,fits=false,busy=false,sampling=false,clocking=false;
    Page page=Page::Clock;PageOrder order;std::wstring directory;
    NetworkSampler network;bool networking=false,networkPending=false,middleArmed=false;
    unsigned networkGeneration=0,networkSamples=0;bool preferencesValid=true;
    unsigned generation=0,samples=0,errors=0,commands=0,powerUpdates=0,switches=0,clockUpdates=0;
    CardWheel wheel;CardInputRoute inputRoute;CardManipulation manipulation;
    winrt::event_token gestureRendering{};bool gestureFrames=false;uint64_t gestureDeadline{};HWND preparedInput{};
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
        card=x::Markup::XamlReader::Load(LR"xaml(
<Border xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 x:Name="LeanDockCard" Height="44" CornerRadius="10" Background="Transparent"
 HorizontalAlignment="Right" VerticalAlignment="Center">
 <Border.Resources>
  <ResourceDictionary><ResourceDictionary.ThemeDictionaries>
   <ResourceDictionary x:Key="Default">
    <SolidColorBrush x:Key="LeanDockCardSurfaceBrush" Color="#9E202732"/>
    <SolidColorBrush x:Key="LeanDockUploadBrush" Color="#FF8BE0BD"/>
    <SolidColorBrush x:Key="LeanDockDownloadBrush" Color="#FF91CFFF"/>
    <SolidColorBrush x:Key="LeanDockRingTrackBrush" Color="#42E4EDF6"/>
   </ResourceDictionary>
   <ResourceDictionary x:Key="Light">
    <SolidColorBrush x:Key="LeanDockCardSurfaceBrush" Color="#A3FFFFFF"/>
    <SolidColorBrush x:Key="LeanDockUploadBrush" Color="#FF197F69"/>
    <SolidColorBrush x:Key="LeanDockDownloadBrush" Color="#FF2D69A3"/>
    <SolidColorBrush x:Key="LeanDockRingTrackBrush" Color="#35343F4D"/>
   </ResourceDictionary>
   <ResourceDictionary x:Key="HighContrast">
    <SolidColorBrush x:Key="LeanDockCardSurfaceBrush" Color="{ThemeResource SystemColorWindowColor}"/>
    <SolidColorBrush x:Key="LeanDockUploadBrush" Color="{ThemeResource SystemColorWindowTextColor}"/>
    <SolidColorBrush x:Key="LeanDockDownloadBrush" Color="{ThemeResource SystemColorWindowTextColor}"/>
    <SolidColorBrush x:Key="LeanDockRingTrackBrush" Color="{ThemeResource SystemColorGrayTextColor}"/>
   </ResourceDictionary>
  </ResourceDictionary.ThemeDictionaries></ResourceDictionary>
 </Border.Resources>
 <Border.RenderTransform><TranslateTransform X="21"/></Border.RenderTransform>
 <Grid>
  <Grid.Resources>
   <Style TargetType="TextBlock"><Setter Property="Foreground" Value="{ThemeResource SystemControlForegroundBaseHighBrush}"/><Setter Property="FontFamily" Value="Segoe UI Variable"/></Style>
   <Style TargetType="Button">
    <Setter Property="Width" Value="24"/><Setter Property="Height" Value="24"/><Setter Property="MinWidth" Value="0"/><Setter Property="MinHeight" Value="0"/><Setter Property="Padding" Value="0"/>
    <Setter Property="Foreground" Value="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
    <Setter Property="Template"><Setter.Value><ControlTemplate TargetType="Button">
     <Grid Background="Transparent"><VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
      <VisualState x:Name="Normal"/><VisualState x:Name="Disabled"><VisualState.Setters><Setter Target="content.Opacity" Value="0.30"/></VisualState.Setters></VisualState>
      <VisualState x:Name="PointerOver"/><VisualState x:Name="Pressed"/>
     </VisualStateGroup></VisualStateManager.VisualStateGroups>
     <ContentPresenter x:Name="content" Content="{TemplateBinding Content}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
     </Grid></ControlTemplate></Setter.Value></Setter>
   </Style>
  </Grid.Resources>
   <!-- One persistent surface for every page; pointer states do not own it. -->
   <Border x:Name="CardSurface" CornerRadius="10" Background="{ThemeResource LeanDockCardSurfaceBrush}" IsHitTestVisible="False"/>
   <Button x:Name="MetricsCard" Width="104" Height="44" AutomationProperties.Name="CPU 与内存，打开任务管理器">
   <Grid x:Name="Metrics" Width="76" Height="32" HorizontalAlignment="Center" VerticalAlignment="Center"><Grid.ColumnDefinitions><ColumnDefinition Width="32"/><ColumnDefinition Width="12"/><ColumnDefinition Width="32"/></Grid.ColumnDefinitions>
   <Grid Width="32" Height="32">
    <Ellipse Stroke="{ThemeResource LeanDockRingTrackBrush}" StrokeThickness="2.5"/>
    <Path x:Name="CpuRing" Stroke="{ThemeResource LeanDockDownloadBrush}" StrokeThickness="2.5" StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
    <TextBlock Text="CPU" FontSize="9" HorizontalAlignment="Center" VerticalAlignment="Center"/>
   </Grid>
   <Grid Grid.Column="2" Width="32" Height="32">
    <Ellipse Stroke="{ThemeResource LeanDockRingTrackBrush}" StrokeThickness="2.5"/>
    <Path x:Name="MemoryRing" Stroke="{ThemeResource LeanDockUploadBrush}" StrokeThickness="2.5" StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
    <TextBlock Text="内存" FontSize="9" HorizontalAlignment="Center" VerticalAlignment="Center"/>
   </Grid>
  </Grid>
   </Button>
   <Grid x:Name="Media" Visibility="Collapsed" Margin="7,0"><Grid.RowDefinitions><RowDefinition Height="20"/><RowDefinition Height="24"/></Grid.RowDefinitions>
   <TextBlock x:Name="Title" FontSize="10" TextTrimming="CharacterEllipsis" TextAlignment="Center" HorizontalAlignment="Center" VerticalAlignment="Center"/>
    <StackPanel x:Name="MediaControls" Grid.Row="1" Orientation="Horizontal" Width="72" Height="24" HorizontalAlignment="Center" VerticalAlignment="Center">
     <Button x:Name="Previous" AutomationProperties.Name="上一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE892;"/></Button>
     <Button x:Name="Play" AutomationProperties.Name="播放或暂停"><TextBlock x:Name="PlayGlyph" FontFamily="Segoe Fluent Icons" FontSize="14" Text="&#xE768;"/></Button>
     <Button x:Name="Next" AutomationProperties.Name="下一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE893;"/></Button>
    </StackPanel>
  </Grid>
   <Button x:Name="ClockCard" Width="104" Height="44" Visibility="Collapsed" AutomationProperties.Name="时间与日期，打开日历和通知">
   <StackPanel x:Name="Clock" HorizontalAlignment="Center" VerticalAlignment="Center">
   <TextBlock x:Name="ClockTime" FontSize="20" FontWeight="SemiBold" Typography.NumeralAlignment="Tabular" TextAlignment="Center"/>
    <TextBlock x:Name="ClockDate" FontSize="10" Opacity="0.82" CharacterSpacing="100" TextAlignment="Center"/>
  </StackPanel>
   </Button>
   <Button x:Name="NetworkCard" Width="104" Height="44" Visibility="Collapsed" AutomationProperties.Name="上传与下载速度，打开网络设置">
     <Grid Width="60" Height="36" HorizontalAlignment="Center"><Grid.RowDefinitions><RowDefinition/><RowDefinition/></Grid.RowDefinitions><Grid.ColumnDefinitions><ColumnDefinition Width="10"/><ColumnDefinition Width="3"/><ColumnDefinition Width="47"/></Grid.ColumnDefinitions>
     <TextBlock Text="↑" FontSize="14" Foreground="{ThemeResource LeanDockUploadBrush}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
     <TextBlock Text="↓" FontSize="14" Foreground="{ThemeResource LeanDockDownloadBrush}" Grid.Row="1" HorizontalAlignment="Center" VerticalAlignment="Center"/>
      <Viewbox Grid.Column="2" MaxWidth="47" MaxHeight="18" Stretch="Uniform" StretchDirection="DownOnly" HorizontalAlignment="Right" VerticalAlignment="Center"><TextBlock x:Name="NetworkSend" Text="—" FontSize="12" Typography.NumeralAlignment="Tabular"/></Viewbox>
      <Viewbox Grid.Row="1" Grid.Column="2" MaxWidth="47" MaxHeight="18" Stretch="Uniform" StretchDirection="DownOnly" HorizontalAlignment="Right" VerticalAlignment="Center"><TextBlock x:Name="NetworkReceive" Text="—" FontSize="12" Typography.NumeralAlignment="Tabular"/></Viewbox>
    </Grid>
   </Button>
 </Grid>
</Border>)xaml").as<c::Border>();
        card.Width(CardWidth);card.Margin({0,0,-CardWidth,0});
        metrics=card.FindName(L"Metrics").as<c::Grid>();media=card.FindName(L"Media").as<c::Grid>();
        clock=card.FindName(L"Clock").as<c::StackPanel>();
        metricsCard=card.FindName(L"MetricsCard").as<c::Button>();clockCard=card.FindName(L"ClockCard").as<c::Button>();
        networkCard=card.FindName(L"NetworkCard").as<c::Button>();
        networkReceive=card.FindName(L"NetworkReceive").as<c::TextBlock>();networkSend=card.FindName(L"NetworkSend").as<c::TextBlock>();
        clockTime=card.FindName(L"ClockTime").as<c::TextBlock>();clockDate=card.FindName(L"ClockDate").as<c::TextBlock>();
        title=card.FindName(L"Title").as<c::TextBlock>();mediaControls=card.FindName(L"MediaControls").as<c::StackPanel>();
        title.Width(CardWidth-14);
        cpuRing.Init(card.FindName(L"CpuRing").as<x::Shapes::Path>());memoryRing.Init(card.FindName(L"MemoryRing").as<x::Shapes::Path>());
        previous=card.FindName(L"Previous").as<c::Button>();play=card.FindName(L"Play").as<c::Button>();next=card.FindName(L"Next").as<c::Button>();
        playGlyph=card.FindName(L"PlayGlyph").as<c::TextBlock>();
        powerCard=x::Markup::XamlReader::Load(LR"xaml(
<Button xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 x:Name="LeanDockPowerCard" Height="44" MinWidth="0" MinHeight="0" Padding="0"
 HorizontalAlignment="Center" VerticalAlignment="Center" AutomationProperties.Name="电池与快速设置">
 <Button.Template><ControlTemplate TargetType="Button">
  <Grid Background="Transparent"><VisualStateManager.VisualStateGroups><VisualStateGroup x:Name="CommonStates">
   <VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"/><VisualState x:Name="Pressed"/>
  </VisualStateGroup></VisualStateManager.VisualStateGroups>
  <ContentPresenter Content="{TemplateBinding Content}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
  </Grid>
 </ControlTemplate></Button.Template>
  <StackPanel x:Name="PowerContent" Orientation="Horizontal" Spacing="6" HorizontalAlignment="Center">
  <Viewbox Width="24" Height="18" VerticalAlignment="Center">
  <Grid Width="32" Height="20" VerticalAlignment="Center">
   <Border Width="28" Height="16" CornerRadius="3" BorderThickness="1" BorderBrush="{ThemeResource SystemControlForegroundBaseHighBrush}" HorizontalAlignment="Left" VerticalAlignment="Center"/>
   <Rectangle x:Name="PowerFill" Width="0" Height="10" RadiusX="1" RadiusY="1" Fill="{ThemeResource SystemControlForegroundBaseHighBrush}" HorizontalAlignment="Left" Margin="3,0,0,0"/>
   <Border Width="2" Height="6" CornerRadius="1" Background="{ThemeResource SystemControlForegroundBaseHighBrush}" HorizontalAlignment="Right" VerticalAlignment="Center"/>
   <Path x:Name="PowerBolt" Data="M 16,1 L 10,11 L 15,11 L 12,19 L 22,7 L 17,7 Z" Fill="{ThemeResource SystemControlForegroundBaseHighBrush}" Stroke="{ThemeResource SystemControlBackgroundChromeMediumLowBrush}" StrokeThickness="0.8" Visibility="Collapsed"/>
  </Grid>
  </Viewbox>
   <StackPanel MaxWidth="60" VerticalAlignment="Center">
   <TextBlock x:Name="PowerText" Text="—" FontFamily="Segoe UI Variable" FontSize="14" FontWeight="SemiBold" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
   <TextBlock x:Name="PowerStatus" Text="电源状态未知" FontFamily="Segoe UI Variable" FontSize="9" Opacity="0.82" TextTrimming="CharacterEllipsis" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
  </StackPanel>
 </StackPanel>
</Button>)xaml").as<c::Button>();
        powerCard.Width(CardWidth);
        powerText=powerCard.FindName(L"PowerText").as<c::TextBlock>();powerFill=powerCard.FindName(L"PowerFill").as<x::Shapes::Rectangle>();
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
        x::Media::TranslateTransform shift;shift.X(12+SlotWidth);divider.RenderTransform(shift);
        c::Canvas::SetZIndex(card,20);c::Canvas::SetZIndex(powerCard,20);c::Canvas::SetZIndex(divider,10);
        card.Visibility(x::Visibility::Collapsed);divider.Visibility(x::Visibility::Collapsed);
        powerCard.Visibility(x::Visibility::Collapsed);
        card.Child().as<c::Grid>().Children().Append(powerCard);
        root.Children().Append(card);root.Children().Append(divider);
        auto weak=weak_from_this();
        powerCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)p->OpenPower();});
        clockCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)LeanDockShell::Request(p->hwnd,LeanDockShell::Action::Notifications);});
        metricsCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)p->OpenSystem(false);});
        networkCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)p->OpenSystem(true);});
        // ContextRequested is handled by the owning RootGrid before it bubbles
        // to TaskbarFrame. RightTapped is a different gesture route and cannot
        // reliably replace the taskbar's ContextRequested menu.
        // The native battery hit target has its original parent in the tray,
        // not in this card. Observe wheel events on their shared XAML root,
        // including handled events, and restrict them to the measured card.
        wheelRoot=root.XamlRoot().Content();
        wheelHandler=winrt::box_value(x::Input::PointerEventHandler([weak](IInspectable const&,x::Input::PointerRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped&&p->fits&&p->Visible())try{
                auto point=e.GetCurrentPoint(p->card);auto pos=point.Position();
                if(pos.X<0||pos.Y<0||pos.X>=p->card.ActualWidth()||pos.Y>=p->card.ActualHeight()){p->wheel.Reset();return;}
                if((e.KeyModifiers()&winrt::Windows::System::VirtualKeyModifiers::Control)!=winrt::Windows::System::VirtualKeyModifiers::None)return;
                WheelCount(p->hwnd,2);
                p->Scroll(point.Properties().MouseWheelDelta(),point.Properties().IsHorizontalMouseWheel());
                e.Handled(true);
            }catch(...){p->Fail();}
        }));
        wheelRoot.AddHandler(x::UIElement::PointerWheelChangedEvent(),wheelHandler,true);
        if(!inputRoute.Start(this,[](void* owner,HWND target,POINT point) noexcept{
            auto p=static_cast<Board*>(owner)->weak_from_this().lock();
            return p&&p->InputHit(target,point);
        },[](void* owner,int delta,bool horizontal) noexcept{
            if(auto p=static_cast<Board*>(owner)->weak_from_this().lock();p&&!p->stopped)try{
                WheelCount(p->hwnd,1);p->Scroll(delta,horizontal);
            }catch(...){p->Fail();}
        },[](void* owner,HWND target,UINT32 pointer) noexcept{
            auto p=static_cast<Board*>(owner)->weak_from_this().lock();
            return p&&!p->stopped&&p->BeginTouchpad(target,pointer);
        }))Fail();
        auto tracePointer=[weak](unsigned kind,x::Input::PointerRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped)try{
                auto point=e.GetCurrentPoint(p->card);auto pos=point.Position();
                const bool inside=p->fits&&p->Visible()&&pos.X>=0&&pos.Y>=0&&pos.X<p->card.ActualWidth()&&pos.Y<p->card.ActualHeight();
                using Update=winrt::Windows::UI::Input::PointerUpdateKind;
                if(kind==0)p->middleArmed=inside&&point.Properties().PointerUpdateKind()==Update::MiddleButtonPressed;
                if(kind==1){const bool open=std::exchange(p->middleArmed,false)&&inside&&point.Properties().PointerUpdateKind()==Update::MiddleButtonReleased;
                    if(open){e.Handled(true);p->OpenPreferences();}}
                if(p->page==Page::Power)p->TracePowerInput(e.OriginalSource(),pos,kind,e.Handled());
            }catch(...){p->Fail();}
        };
        inputPress=winrt::box_value(x::Input::PointerEventHandler([tracePointer](auto&&,auto const& e){tracePointer(0,e);}));
        inputRelease=winrt::box_value(x::Input::PointerEventHandler([tracePointer](auto&&,auto const& e){tracePointer(1,e);}));
        inputTap=winrt::box_value(x::Input::TappedEventHandler([weak](auto&&,x::Input::TappedRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped&&p->page==Page::Power)try{
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
        title.Text(L"暂无播放");previous.IsEnabled(false);play.IsEnabled(false);next.IsEnabled(false);
        ReadPower();
        try{
            chargeChanged=Power::RemainingChargePercentChanged(winrt::auto_revoke,[weak](auto&&,auto&&){QueuePower(weak);});
            batteryChanged=Power::BatteryStatusChanged(winrt::auto_revoke,[weak](auto&&,auto&&){QueuePower(weak);});
            supplyChanged=Power::PowerSupplyStatusChanged(winrt::auto_revoke,[weak](auto&&,auto&&){QueuePower(weak);});
        }catch(...){Fail();} // Opening/revealing the Dock still refreshes power state.
        Render();Request(weak);
    }
    void Fail() noexcept{++errors;Put(hwnd,8,errors);}
    bool InputHit(HWND target,POINT point) noexcept {
        if(stopped||!fits||!Visible()||GetAncestor(target,GA_ROOT)!=hwnd)return false;
        wchar_t cls[64]{};if(!GetClassNameW(target,cls,std::size(cls))||wcscmp(cls,L"Windows.UI.Input.InputSite.WindowClass"))return false;
        try{
            auto xr=root.XamlRoot();const auto scale=xr.RasterizationScale();
            if(!xr.IsHostVisible()||scale<=0||!ScreenToClient(target,&point))return false;
            auto local=xr.Content().TransformToVisual(card).TransformPoint({float(point.x/scale),float(point.y/scale)});
            return local.X>=0&&local.Y>=0&&local.X<card.ActualWidth()&&local.Y<card.ActualHeight();
        }catch(...){return false;}
    }
    void GestureFrames(bool active) noexcept {
        if(active&&stopped)return;
        if(active==gestureFrames)return;
        try {
            if(active){
                auto weak=weak_from_this();
                gestureRendering=x::Media::CompositionTarget::Rendering([weak](auto&&,auto&&){
                    if(auto p=weak.lock();p&&!p->stopped){
                        if(!p->fits||!p->Visible()||GetTickCount64()>p->gestureDeadline){p->manipulation.Cancel();return;}
                        p->manipulation.Update();
                    }
                });
            }else x::Media::CompositionTarget::Rendering(gestureRendering);
            gestureFrames=active;
            if(hwnd)SetPropW(hwnd,WheelProps[8],reinterpret_cast<HANDLE>(active?1:0));
        }catch(...){Fail();}
    }
    HRESULT PrepareTouchpad(HWND target,RECT& rect,float& dpi) noexcept {
        try {
            const auto xr=root.XamlRoot();dpi=float(xr.RasterizationScale());
            auto origin=card.TransformToVisual(xr.Content()).TransformPoint({0,0});
            rect={LONG(std::floor(origin.X*dpi)),LONG(std::floor(origin.Y*dpi)),
                LONG(std::ceil((origin.X+card.ActualWidth())*dpi)),LONG(std::ceil((origin.Y+card.ActualHeight())*dpi))};
            auto weak=weak_from_this();
            auto hr=manipulation.Init(target,rect,[weak](int delta,bool horizontal){
                if(auto p=weak.lock();p&&!p->stopped)try{
                    auto before=p->switches;p->Scroll(delta,horizontal);
                    if(before!=p->switches)WheelCount(p->hwnd,12);
                }catch(...){p->Fail();}
            },[weak](bool active){if(auto p=weak.lock())p->GestureFrames(active);},
            [weak](unsigned index,intptr_t value){if(auto p=weak.lock();p&&!p->stopped){
                if(index==9)p->gestureDeadline=GetTickCount64()+30000;
                if(index==9||index==13)WheelCount(p->hwnd,index);
                else if(p->hwnd)SetPropW(p->hwnd,WheelProps[index],reinterpret_cast<HANDLE>(value));
            }});
            SetPropW(hwnd,WheelProps[6],reinterpret_cast<HANDLE>(SUCCEEDED(hr)?1:0));
            return hr;
        }catch(...){return winrt::to_hresult();}
    }
    bool BeginTouchpad(HWND target,UINT32 pointer) noexcept {
        try {
            WheelCount(hwnd,3);gestureDeadline=GetTickCount64()+30000;
            RECT rect{};float dpi{};auto hr=PrepareTouchpad(target,rect,dpi);
            if(SUCCEEDED(hr)){if(!gestureFrames)wheel.Reset();hr=manipulation.Begin(pointer,rect,dpi);}
            SetPropW(hwnd,WheelProps[11],reinterpret_cast<HANDLE>(intptr_t(hr)));
            if(FAILED(hr))return false;
            WheelCount(hwnd,7);return true;
        }catch(...){Fail();return false;}
    }
    void Scroll(int delta,bool horizontal) {
        if(stopped||!fits||!Visible()){wheel.Reset();return;}
        if(hwnd)SetPropW(hwnd,WheelProps[5],reinterpret_cast<HANDLE>(horizontal?2:1));
        if(auto step=wheel.Step(delta,horizontal,GetTickCount64())){
            auto selected=order.Step(page,step);
            if(selected!=page){page=selected;++switches;if(!SaveSelection(directory,page))Fail();}
            if(page==Page::Power)ReadPower();
            Render();
        }
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
            inputRoute.Bind(hwnd,InstallInputProcedure);
            SetPropW(hwnd,WheelProps[0],reinterpret_cast<HANDLE>(inputRoute.Ready()?1:0));
            const double appWidth=std::max(double(apps.ActualWidth()),double(apps.DesiredSize().Width));
            Put(hwnd,52,std::lround(apps.ActualWidth()*100));Put(hwnd,53,std::lround(root.ActualWidth()*100));
            Put(hwnd,54,std::lround(root.MinWidth()*100));
            if(auto frame=x::Media::VisualTreeHelper::GetParent(root).try_as<x::FrameworkElement>())Put(hwnd,55,std::lround(frame.MinWidth()*100));
            PublishLayout();
            bool nextFits=Fits(available,appWidth,tray,fits);
            const double slot=nextFits?SlotWidth:0;
            auto padding=root.Padding();if(padding.Left!=12||padding.Right!=12+slot||padding.Top||padding.Bottom)root.Padding({12,0,12+slot,0});
            auto margin=background.Margin();if(margin.Right!=-12-slot||margin.Left!=-13||margin.Top!=-1||margin.Bottom!=-1)background.Margin({-13,-1,-12-slot,-1});
            if(fits!=nextFits){fits=nextFits;card.Visibility(fits?x::Visibility::Visible:x::Visibility::Collapsed);divider.Visibility(card.Visibility());}
            auto lineShift=divider.RenderTransform().as<x::Media::TranslateTransform>();if(lineShift.X()!=12+slot)lineShift.X(12+slot);
            const bool wasShown=shown;shown=Visible();if(shown&&!wasShown)ReadPower();Sampling();Put(hwnd,0,1);Put(hwnd,1,shown);Publish();
            if(!shown){wheel.Reset();if(gestureFrames)manipulation.Cancel();}
            PublishSlot();
            if(inputRoute.Ready()&&preparedInput!=inputRoute.Window()&&card.ActualWidth()>0&&card.ActualHeight()>0){
                preparedInput=inputRoute.Window();RECT rect{};float dpi{};
                auto hr=PrepareTouchpad(preparedInput,rect,dpi);
                SetPropW(hwnd,WheelProps[11],reinterpret_cast<HANDLE>(intptr_t(hr)));
            }
            if(fits&&page==Page::Power){auto p=powerCard.TransformToVisual(root.XamlRoot().Content()).TransformPoint({0,0});
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
                if(page==Page::Media){
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
    void Sampling() {
        const bool want=ShouldSample(shown&&fits,page==Page::Metrics,!stopped);
        const bool wantClock=shown&&fits&&page==Page::Clock&&!stopped;
        const bool wantNetwork=shown&&fits&&page==Page::Network&&!stopped;
        if(want==sampling&&wantClock==clocking&&wantNetwork==networking)return;
        timer.Stop();cpu.Pause();sampling=want;clocking=wantClock;
        networking=wantNetwork;network.Pause();++networkGeneration;UpdateNetworkVisual();
        Put(hwnd,2,sampling);Put(hwnd,48,clocking);
        if(want){
            cpu.Resume(Times(),GetTickCount64());UpdateCpuVisual();ReadMemory();
            timer.Interval(std::chrono::milliseconds(cpu.IntervalMs()));timer.Start();
        }else{Put(hwnd,37,0);Put(hwnd,38,0);}
        if(wantClock){ReadClock();timer.Start();}
        if(wantNetwork){timer.Interval(std::chrono::milliseconds(200));timer.Start();QueueNetwork();}
    }
    void ReloadPreferences() noexcept override {
        try{auto weak=weak_from_this();dispatcher.RunAsync(CoreDispatcherPriority::Normal,[weak]{
            if(auto p=weak.lock();p&&!p->stopped)try{
                PageOrder order;Page selected=p->page;
                if(!(p->preferencesValid=ReadPreferences(p->directory,order,selected))){p->Fail();p->Publish();return;}
                p->order=std::move(order);p->page=p->order.Select(p->page);p->wheel.Reset();
                if(p->page==Page::Power)p->ReadPower();p->Render();
            }catch(...){p->Fail();}
        });}catch(...){}
    }
    void OpenPreferences() {
        if(directory.empty()){Fail();return;}
        auto exe=directory+L"\\latest.exe";
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",exe.c_str(),L"--cards",directory.c_str(),SW_SHOWNORMAL))<=32)Fail();
    }
    void OpenSystem(bool networkSettings) {
        wchar_t path[MAX_PATH]{};
        if(networkSettings)wcscpy_s(path,L"ms-settings:network-status");
        else {if(!GetSystemDirectoryW(path,MAX_PATH-16)){Fail();return;}wcscat_s(path,L"\\taskmgr.exe");}
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",path,nullptr,nullptr,SW_SHOWNORMAL))<=32)Fail();
    }
    void UpdateNetworkVisual() {
        auto rx=FormatRate(network.value?std::optional(network.value->received):std::nullopt);
        auto tx=FormatRate(network.value?std::optional(network.value->sent):std::nullopt);
        networkReceive.Text(rx.CompactText());networkSend.Text(tx.CompactText());
        x::Automation::AutomationProperties::SetName(networkCard,L"网络总发送 "+tx.PerSecondText()+L"，总接收 "+rx.PerSecondText()+L"，打开网络设置");
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
                    p->timer.Interval(std::chrono::milliseconds(p->network.IntervalMs()));p->UpdateNetworkVisual();
                }
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
        }data+=L"]}";
        if(data!=lastLayout){InternalWh_SetStringValue(nullptr,L"cardsLayoutState",data.c_str());lastLayout=std::move(data);}
    }
    void ReadClock() {
        SYSTEMTIME now{};GetLocalTime(&now);
        auto text=ClockText(now.wHour,now.wMinute,now.wMonth,now.wDay);
        if(clockTime.Text()!=text.time)clockTime.Text(text.time);
        if(clockDate.Text()!=text.date)clockDate.Text(text.date);
        x::Automation::AutomationProperties::SetName(card,text.date+L" "+text.time);
        Put(hwnd,49,now.wHour*100+now.wMinute);Put(hwnd,50,now.wMonth*100+now.wDay);Put(hwnd,51,++clockUpdates);
        timer.Interval(std::chrono::milliseconds(NextMinuteMs(now.wSecond,now.wMilliseconds)));
    }
    void TimeChanged() noexcept override {
        try{auto weak=weak_from_this();dispatcher.RunAsync(CoreDispatcherPriority::Normal,[weak]{
            if(auto p=weak.lock();p&&!p->stopped&&p->clocking)try{p->timer.Stop();p->ReadClock();p->timer.Start();}catch(...){p->Fail();}
        });}catch(...){}
    }
    void UpdateCpuVisual(){
        cpuRing.Set(cpu.value);
        if(cpu.value)Put(hwnd,4,*cpu.value);else RemovePropW(hwnd,Props[4]);
        Put(hwnd,37,cpu.refreshing);Put(hwnd,38,sampling?cpu.IntervalMs():0);
    }
    winrt::hstring CpuText()const{
        if(cpu.refreshing)return cpu.value?L"更新中（上次 "+PercentText(cpu.value)+L"）":L"采样中";
        return PercentText(cpu.value);
    }
    void ReadMemory(){MEMORYSTATUSEX s{sizeof(s)};if(GlobalMemoryStatusEx(&s)){
        memoryPercent=s.dwMemoryLoad;memoryRing.Set(memoryPercent);
        Put(hwnd,5,s.dwMemoryLoad);
    }else{memoryPercent.reset();memoryRing.Set({});RemovePropW(hwnd,Props[5]);}MetricsDescription();}
    void MetricsDescription() {
        auto description=L"CPU "+CpuText()+L"，内存 "+PercentText(memoryPercent);
        x::Automation::AutomationProperties::SetName(metrics,description);
        if(page==Page::Metrics){
            x::Automation::AutomationProperties::SetName(card,description);
        }
    }
    void Tick(){
        if(stopped)return;shown=Visible();Put(hwnd,1,shown);Sampling();
        if(clocking){ReadClock();return;}if(networking){QueueNetwork();return;}if(!sampling)return;
        const bool measured=cpu.Sample(Times(),GetTickCount64());
        timer.Interval(std::chrono::milliseconds(cpu.IntervalMs()));UpdateCpuVisual();
        ReadMemory();if(measured){++samples;Put(hwnd,3,samples);}
    }
    void Publish(){Put(hwnd,3,samples);Put(hwnd,6,bool(session));Put(hwnd,7,static_cast<unsigned>(page));Put(hwnd,8,errors);Put(hwnd,9,commands);
        if(battery.percent>=0)Put(hwnd,25,battery.percent);else if(hwnd)RemovePropW(hwnd,Props[25]);Put(hwnd,26,battery.charging);Put(hwnd,27,battery.plugged);
        Put(hwnd,34,static_cast<unsigned>(Mode(battery)));Put(hwnd,35,powerUpdates);Put(hwnd,36,switches);Put(hwnd,61,order.pages.size());Put(hwnd,62,preferencesValid);}
    void PublishSlot() {
        if(!hwnd)return;
        const bool ready=fits&&card.IsLoaded()&&card.ActualWidth()>0&&card.ActualHeight()>0;
        const bool active=ready&&page==Page::Power;
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
        powerFill.Width(battery.percent>=0?22.0*battery.percent/100:0);
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
        if(!hwnd||!fits||point.X<0||point.Y<0||point.X>=card.ActualWidth()||point.Y>=card.ActualHeight())return;
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
        const double titleSize=session?10:14;if(title.FontSize()!=titleSize)title.FontSize(titleSize);
        if(!session){title.Text(L"暂无播放");artistName=L"";previous.IsEnabled(false);play.IsEnabled(false);next.IsEnabled(false);playGlyph.Text(L"\xE768");}
        metricsCard.Visibility(page==Page::Metrics?x::Visibility::Visible:x::Visibility::Collapsed);
        media.Visibility(page==Page::Media?x::Visibility::Visible:x::Visibility::Collapsed);
        powerCard.Visibility(page==Page::Power?x::Visibility::Visible:x::Visibility::Collapsed);
        clockCard.Visibility(page==Page::Clock?x::Visibility::Visible:x::Visibility::Collapsed);
        networkCard.Visibility(page==Page::Network?x::Visibility::Visible:x::Visibility::Collapsed);
        x::Automation::AutomationProperties::SetName(card,page==Page::Metrics?L"CPU "+CpuText()+L"，内存 "+PercentText(memoryPercent):
            page==Page::Media?L"媒体 "+title.Text():page==Page::Power?L"电池 "+powerText.Text()+L"，"+powerStatus.Text():
            page==Page::Network?x::Automation::AutomationProperties::GetName(networkCard):clockDate.Text()+L" "+clockTime.Text());
        Sampling();Publish();PublishSlot();
    }
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
            title.Text(session?L"媒体":L"暂无播放");artistName=L"";
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
            p->title.Text(data.Title().empty()?L"媒体":data.Title());p->artistName=data.Artist();
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
        inputRoute.Stop();manipulation.Stop();GestureFrames(false);wheel.Reset();
        CancelCommand();
        try{if(timer)timer.Stop();}catch(...){}
        try{if(request)request.Cancel();if(metadata)metadata.Cancel();}catch(...){}
        managerChanged.revoke();sessionsChanged.revoke();propertiesChanged.revoke();playbackChanged.revoke();unloaded.revoke();
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
        if(hwnd)for(auto name:{L"LeanDock.Shell.LastRequest",L"LeanDock.Shell.LastAction",L"LeanDock.Shell.LastResult",L"LeanDock.Shell.PowerUser",L"LeanDock.Shell.QuickSettings",L"LeanDock.Shell.Notifications"})RemovePropW(hwnd,name);
        session=nullptr;manager=nullptr;
    }
    bool Alive()const noexcept override{return !stopped;}
};
std::shared_ptr<Host> Create(c::Grid const& root){auto p=std::make_shared<Board>();p->Init(root);return p;}
}
