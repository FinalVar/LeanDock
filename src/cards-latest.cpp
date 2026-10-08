#include "cards-latest.h"
#include "cards-core-latest.h"
#include "shell-actions-latest.h"
#include "shell-menu-latest.h"
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
extern "C" BOOL InternalWh_SetStringValue(void*,PCWSTR,PCWSTR);

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
    L"LeanDock.Card.AppWidth100",L"LeanDock.Card.RootWidth100",L"LeanDock.Card.RootMinWidth100",L"LeanDock.Card.FrameMinWidth100",L"LeanDock.Card.MediaPlaying"};
static uint64_t Bits(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
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
    x::UIElement wheelRoot{nullptr};IInspectable wheelHandler{nullptr};
    Power::RemainingChargePercentChanged_revoker chargeChanged;
    Power::BatteryStatusChanged_revoker batteryChanged;
    Power::PowerSupplyStatusChanged_revoker supplyChanged;
    c::Grid metrics{nullptr},media{nullptr}; c::StackPanel clock{nullptr};
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
    Page page=Page::Metrics;
    unsigned generation=0,samples=0,errors=0,commands=0,powerUpdates=0,switches=0,clockUpdates=0;
    int wheelRemainder=0;
    std::atomic<bool> refreshQueued=false;
    std::atomic<bool> powerQueued=false;
    Battery battery;
    std::wstring lastLayout;
    ~Board(){Stop();}

    void Init(c::Grid const& grid) {
        root=grid;dispatcher=root.Dispatcher();
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
 x:Name="LeanDockCard" Height="44" CornerRadius="10" Background="#0CFFFFFF"
 HorizontalAlignment="Right" VerticalAlignment="Center">
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
      <VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="hover.Opacity" Value="0.12"/></VisualState.Setters></VisualState>
      <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="hover.Opacity" Value="0.20"/></VisualState.Setters></VisualState>
     </VisualStateGroup></VisualStateManager.VisualStateGroups>
     <Border x:Name="hover" CornerRadius="6" Background="{ThemeResource SystemControlForegroundBaseHighBrush}" Opacity="0"/>
     <ContentPresenter x:Name="content" Content="{TemplateBinding Content}" HorizontalAlignment="Center" VerticalAlignment="Center"/>
     </Grid></ControlTemplate></Setter.Value></Setter>
   </Style>
  </Grid.Resources>
  <Grid x:Name="Metrics" Width="76" Height="32" HorizontalAlignment="Center" VerticalAlignment="Center"><Grid.ColumnDefinitions><ColumnDefinition Width="32"/><ColumnDefinition Width="12"/><ColumnDefinition Width="32"/></Grid.ColumnDefinitions>
   <Grid Width="32" Height="32">
    <Ellipse Stroke="#26808080" StrokeThickness="2.5"/>
    <Path x:Name="CpuRing" Stroke="#FF538ABA" StrokeThickness="2.5" StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
    <TextBlock Text="CPU" FontSize="9" HorizontalAlignment="Center" VerticalAlignment="Center"/>
   </Grid>
   <Grid Grid.Column="2" Width="32" Height="32">
    <Ellipse Stroke="#26808080" StrokeThickness="2.5"/>
    <Path x:Name="MemoryRing" Stroke="#FF4C9E8B" StrokeThickness="2.5" StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>
    <TextBlock Text="内存" FontSize="9" HorizontalAlignment="Center" VerticalAlignment="Center"/>
   </Grid>
  </Grid>
  <Grid x:Name="Media" Visibility="Collapsed" Margin="7,0"><Grid.RowDefinitions><RowDefinition Height="20"/><RowDefinition Height="24"/></Grid.RowDefinitions>
   <TextBlock x:Name="Title" FontSize="10" TextTrimming="CharacterEllipsis" TextAlignment="Center" HorizontalAlignment="Center" VerticalAlignment="Center"/>
    <StackPanel x:Name="MediaControls" Grid.Row="1" Orientation="Horizontal" Width="72" Height="24" HorizontalAlignment="Center" VerticalAlignment="Center">
     <Button x:Name="Previous" AutomationProperties.Name="上一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE892;"/></Button>
     <Button x:Name="Play" AutomationProperties.Name="播放或暂停"><TextBlock x:Name="PlayGlyph" FontFamily="Segoe Fluent Icons" FontSize="14" Text="&#xE768;"/></Button>
     <Button x:Name="Next" AutomationProperties.Name="下一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE893;"/></Button>
    </StackPanel>
  </Grid>
  <StackPanel x:Name="Clock" Visibility="Collapsed" HorizontalAlignment="Center" VerticalAlignment="Center">
   <TextBlock x:Name="ClockTime" FontSize="20" FontWeight="SemiBold" Typography.NumeralAlignment="Tabular" TextAlignment="Center"/>
   <TextBlock x:Name="ClockDate" FontSize="10" Opacity="0.72" CharacterSpacing="100" TextAlignment="Center"/>
  </StackPanel>
 </Grid>
</Border>)xaml").as<c::Border>();
        card.Width(CardWidth);card.Margin({0,0,-CardWidth,0});
        metrics=card.FindName(L"Metrics").as<c::Grid>();media=card.FindName(L"Media").as<c::Grid>();
        clock=card.FindName(L"Clock").as<c::StackPanel>();
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
   <VisualState x:Name="Normal"/><VisualState x:Name="PointerOver"><VisualState.Setters><Setter Target="hover.Opacity" Value="0.12"/></VisualState.Setters></VisualState>
   <VisualState x:Name="Pressed"><VisualState.Setters><Setter Target="hover.Opacity" Value="0.20"/></VisualState.Setters></VisualState>
  </VisualStateGroup></VisualStateManager.VisualStateGroups>
  <Border x:Name="hover" CornerRadius="10" Background="{ThemeResource SystemControlForegroundBaseHighBrush}" Opacity="0"/>
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
   <TextBlock x:Name="PowerStatus" Text="电源状态未知" FontFamily="Segoe UI Variable" FontSize="9" Opacity="0.75" TextTrimming="CharacterEllipsis" Foreground="{ThemeResource SystemControlForegroundBaseHighBrush}"/>
  </StackPanel>
 </StackPanel>
</Button>)xaml").as<c::Button>();
        powerCard.Width(CardWidth);
        powerText=powerCard.FindName(L"PowerText").as<c::TextBlock>();powerFill=powerCard.FindName(L"PowerFill").as<x::Shapes::Rectangle>();
        powerStatus=powerCard.FindName(L"PowerStatus").as<c::TextBlock>();powerBolt=powerCard.FindName(L"PowerBolt").as<x::Shapes::Path>();
        powerContent=powerCard.FindName(L"PowerContent").as<c::StackPanel>();
        divider=x::Shapes::Rectangle();divider.Name(L"LeanDockCardDivider");divider.Width(1);divider.Height(40);
        divider.HorizontalAlignment(x::HorizontalAlignment::Right);divider.VerticalAlignment(x::VerticalAlignment::Center);
        divider.Margin({0,0,2,0});divider.IsHitTestVisible(false);
        x::Media::TranslateTransform shift;shift.X(12+SlotWidth);divider.RenderTransform(shift);
        x::Media::LinearGradientBrush brush;brush.StartPoint({.5f,0});brush.EndPoint({.5f,1});
        for(auto [offset,alpha]:{std::pair{0.0,16},std::pair{0.18,70},std::pair{0.82,70},std::pair{1.0,16}}){
            x::Media::GradientStop s;s.Offset(offset);s.Color({uint8_t(alpha),100,110,125});brush.GradientStops().Append(s);
        }
        divider.Fill(brush);c::Canvas::SetZIndex(card,20);c::Canvas::SetZIndex(powerCard,20);c::Canvas::SetZIndex(divider,10);
        card.Visibility(x::Visibility::Collapsed);divider.Visibility(x::Visibility::Collapsed);
        powerCard.Visibility(x::Visibility::Collapsed);
        card.Child().as<c::Grid>().Children().Append(powerCard);
        root.Children().Append(card);root.Children().Append(divider);
        auto weak=weak_from_this();
        powerCard.Click([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->stopped)p->OpenPower();});
        auto context=[weak](IInspectable const&,x::Input::RightTappedRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped&&LeanDockMenu::Request(p->hwnd))e.Handled(true);
        };
        card.RightTapped(context);
        // The native battery hit target has its original parent in the tray,
        // not in this card. Observe wheel events on their shared XAML root,
        // including handled events, and restrict them to the measured card.
        wheelRoot=root.XamlRoot().Content();
        wheelHandler=winrt::box_value(x::Input::PointerEventHandler([weak](IInspectable const&,x::Input::PointerRoutedEventArgs const& e){
            if(auto p=weak.lock();p&&!p->stopped&&p->fits&&p->Visible())try{
                auto point=e.GetCurrentPoint(p->card);auto pos=point.Position();
                if(pos.X<0||pos.Y<0||pos.X>=p->card.ActualWidth()||pos.Y>=p->card.ActualHeight()||
                   point.Properties().IsHorizontalMouseWheel())return;
                if(auto step=WheelStep(point.Properties().MouseWheelDelta(),p->wheelRemainder)){
                    p->page=Cycle(p->page,step);++p->switches;
                    if(p->page==Page::Power)p->ReadPower();
                    p->Render();
                }
                e.Handled(true);
            }catch(...){p->Fail();}
        }));
        wheelRoot.AddHandler(x::UIElement::PointerWheelChangedEvent(),wheelHandler,true);
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
            PublishSlot();
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
        if(want==sampling&&wantClock==clocking)return;
        timer.Stop();cpu.Pause();sampling=want;clocking=wantClock;
        Put(hwnd,2,sampling);Put(hwnd,48,clocking);
        if(want){
            cpu.Resume(Times(),GetTickCount64());UpdateCpuVisual();ReadMemory();
            timer.Interval(std::chrono::milliseconds(cpu.IntervalMs()));timer.Start();
        }else{Put(hwnd,37,0);Put(hwnd,38,0);}
        if(wantClock){ReadClock();timer.Start();}
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
        if(clocking){ReadClock();return;}if(!sampling)return;
        const bool measured=cpu.Sample(Times(),GetTickCount64());
        timer.Interval(std::chrono::milliseconds(cpu.IntervalMs()));UpdateCpuVisual();
        ReadMemory();if(measured){++samples;Put(hwnd,3,samples);}
    }
    void Publish(){Put(hwnd,3,samples);Put(hwnd,6,bool(session));Put(hwnd,7,static_cast<unsigned>(page));Put(hwnd,8,errors);Put(hwnd,9,commands);
        if(battery.percent>=0)Put(hwnd,25,battery.percent);else if(hwnd)RemovePropW(hwnd,Props[25]);Put(hwnd,26,battery.charging);Put(hwnd,27,battery.plugged);
        Put(hwnd,34,static_cast<unsigned>(Mode(battery)));Put(hwnd,35,powerUpdates);Put(hwnd,36,switches);}
    void PublishSlot() {
        if(!hwnd)return;
        const bool ready=fits&&card.IsLoaded()&&card.ActualWidth()>0&&card.ActualHeight()>0;
        const bool active=ready&&page==Page::Power;
        const bool changed=reinterpret_cast<uintptr_t>(GetPropW(hwnd,Props[24]))!=uintptr_t(ready+1)||
            reinterpret_cast<uintptr_t>(GetPropW(hwnd,Props[32]))!=uintptr_t(active+1);
        Put(hwnd,24,ready);Put(hwnd,32,active);
        Put(hwnd,33,fits?(metrics.Visibility()==x::Visibility::Visible)+
            (media.Visibility()==x::Visibility::Visible)+(powerCard.Visibility()==x::Visibility::Visible)+
            (clock.Visibility()==x::Visibility::Visible):0);
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
    void OpenPower() noexcept {
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
        metrics.Visibility(page==Page::Metrics?x::Visibility::Visible:x::Visibility::Collapsed);
        media.Visibility(page==Page::Media?x::Visibility::Visible:x::Visibility::Collapsed);
        powerCard.Visibility(page==Page::Power?x::Visibility::Visible:x::Visibility::Collapsed);
        clock.Visibility(page==Page::Clock?x::Visibility::Visible:x::Visibility::Collapsed);
        x::Automation::AutomationProperties::SetName(card,page==Page::Metrics?L"CPU "+CpuText()+L"，内存 "+PercentText(memoryPercent):
            page==Page::Media?L"媒体 "+title.Text():page==Page::Power?L"电池 "+powerText.Text()+L"，"+powerStatus.Text():clockDate.Text()+L" "+clockTime.Text());
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
        if(stopped)return;stopped=true;++generation;
        CancelCommand();
        try{if(timer)timer.Stop();}catch(...){}
        try{if(request)request.Cancel();if(metadata)metadata.Cancel();}catch(...){}
        managerChanged.revoke();sessionsChanged.revoke();propertiesChanged.revoke();playbackChanged.revoke();unloaded.revoke();
        chargeChanged.revoke();batteryChanged.revoke();supplyChanged.revoke();
        try{if(wheelRoot&&wheelHandler)wheelRoot.RemoveHandler(x::UIElement::PointerWheelChangedEvent(),wheelHandler);}catch(...){}
        wheelHandler=nullptr;wheelRoot=nullptr;
        try{if(root){for(auto e:{card.try_as<x::UIElement>(),divider.try_as<x::UIElement>()}){uint32_t index=0;if(e&&root.Children().IndexOf(e,index))root.Children().RemoveAt(index);}
            if(oldPadding==x::DependencyProperty::UnsetValue())root.ClearValue(c::Grid::PaddingProperty());else root.SetValue(c::Grid::PaddingProperty(),oldPadding);
        }}catch(...){}
        try{if(background){if(oldMargin==x::DependencyProperty::UnsetValue())background.ClearValue(x::FrameworkElement::MarginProperty());else background.SetValue(x::FrameworkElement::MarginProperty(),oldMargin);}}catch(...){}
        for(auto key:Props)if(hwnd)RemovePropW(hwnd,key);
        if(hwnd)for(auto name:{L"LeanDock.Shell.LastRequest",L"LeanDock.Shell.PowerUser",L"LeanDock.Shell.QuickSettings"})RemovePropW(hwnd,name);
        session=nullptr;manager=nullptr;
    }
    bool Alive()const noexcept override{return !stopped;}
};
std::shared_ptr<Host> Create(c::Grid const& root){auto p=std::make_shared<Board>();p->Init(root);return p;}
}
