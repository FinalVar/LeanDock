#include "cards-latest.h"
#include "cards-core-latest.h"
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>

namespace LeanDockCards {
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace m=winrt::Windows::Media::Control;
using Manager=m::GlobalSystemMediaTransportControlsSessionManager;
using Session=m::GlobalSystemMediaTransportControlsSession;
using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::UI::Core::CoreDispatcherPriority;

static constexpr const wchar_t* Props[]={L"LeanDock.Card.Ready",L"LeanDock.Card.Visible",
    L"LeanDock.Card.Sampling",L"LeanDock.Card.Samples",L"LeanDock.Card.Cpu",L"LeanDock.Card.Memory",
    L"LeanDock.Card.Media",L"LeanDock.Card.View",L"LeanDock.Card.Errors",L"LeanDock.Card.Commands",
    L"LeanDock.Card.Left100",L"LeanDock.Card.Top100",L"LeanDock.Card.Right100",L"LeanDock.Card.Bottom100",
    L"LeanDock.Card.AppRight100",L"LeanDock.Card.RootRight100",L"LeanDock.Card.Fits"};
static uint64_t Bits(FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
static std::optional<CpuTimes> Times(){
    // This API is group-local on machines with more than 64 processors. Do
    // not label that partial value as whole-machine CPU utilization.
    if(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)>64)return {};
    FILETIME i{},k{},u{};if(!GetSystemTimes(&i,&k,&u))return {};return CpuTimes{Bits(i),Bits(k),Bits(u)};
}
static void Put(HWND w,unsigned i,uintptr_t value){if(w){auto v=reinterpret_cast<HANDLE>(value+1);if(GetPropW(w,Props[i])!=v)SetPropW(w,Props[i],v);}}

struct Board final:Host,std::enable_shared_from_this<Board> {
    c::Grid root{nullptr}; x::FrameworkElement background{nullptr}, apps{nullptr};
    IInspectable oldPadding{nullptr},oldMargin{nullptr};
    c::Border card{nullptr}; x::Shapes::Rectangle divider{nullptr};
    c::Grid metrics{nullptr},media{nullptr}; c::TextBlock cpu{nullptr},memory{nullptr},title{nullptr},artist{nullptr};
    x::Shapes::Rectangle cpuBar{nullptr},memoryBar{nullptr};
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
    HWND hwnd=nullptr; bool stopped=false,shown=false,fits=false,mediaView=false,busy=false,sampling=false,commandBusy=false;
    unsigned generation=0,samples=0,errors=0,commands=0;
    int wheelRemainder=0;
    std::optional<CpuTimes> baseline;
    std::atomic<bool> refreshQueued=false;
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
 x:Name="LeanDockCard" Width="160" Height="44" CornerRadius="10" Background="#0CFFFFFF"
 HorizontalAlignment="Right" VerticalAlignment="Center" Margin="0,0,-160,0">
 <Border.RenderTransform><TranslateTransform X="21"/></Border.RenderTransform>
 <Grid Margin="9,3">
  <Grid.Resources>
   <Style TargetType="TextBlock"><Setter Property="Foreground" Value="{ThemeResource SystemControlForegroundBaseHighBrush}"/><Setter Property="FontFamily" Value="Segoe UI Variable"/></Style>
   <Style TargetType="Button">
    <Setter Property="Width" Value="24"/><Setter Property="Height" Value="24"/><Setter Property="Padding" Value="0"/>
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
  <Grid x:Name="Metrics"><Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="12"/><ColumnDefinition Width="*"/></Grid.ColumnDefinitions>
   <StackPanel><TextBlock Text="CPU" FontSize="10" Opacity="0.70"/><TextBlock x:Name="Cpu" Text="—" FontSize="16" FontWeight="SemiBold" Margin="0,-2,0,0"/>
    <Grid Height="2" Margin="0,1,0,0" Background="#18808080"><Rectangle x:Name="CpuBar" Width="0" Fill="#886C9AD8" HorizontalAlignment="Left"/></Grid></StackPanel>
   <StackPanel Grid.Column="2"><TextBlock Text="内存" FontSize="10" Opacity="0.70"/><TextBlock x:Name="Memory" Text="—" FontSize="16" FontWeight="SemiBold" Margin="0,-2,0,0"/>
    <Grid Height="2" Margin="0,1,0,0" Background="#18808080"><Rectangle x:Name="MemoryBar" Width="0" Fill="#8885ADA6" HorizontalAlignment="Left"/></Grid></StackPanel>
  </Grid>
  <Grid x:Name="Media" Visibility="Collapsed"><Grid.RowDefinitions><RowDefinition Height="15"/><RowDefinition Height="23"/></Grid.RowDefinitions>
   <TextBlock x:Name="Title" FontSize="11" TextTrimming="CharacterEllipsis"/>
   <Grid Grid.Row="1"><Grid.ColumnDefinitions><ColumnDefinition Width="*"/><ColumnDefinition Width="72"/></Grid.ColumnDefinitions>
    <TextBlock x:Name="Artist" FontSize="10" Opacity="0.7" TextTrimming="CharacterEllipsis" VerticalAlignment="Center"/>
    <StackPanel Grid.Column="1" Orientation="Horizontal">
     <Button x:Name="Previous" AutomationProperties.Name="上一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE892;"/></Button>
     <Button x:Name="Play" AutomationProperties.Name="播放或暂停"><TextBlock x:Name="PlayGlyph" FontFamily="Segoe Fluent Icons" FontSize="14" Text="&#xE768;"/></Button>
     <Button x:Name="Next" AutomationProperties.Name="下一首"><TextBlock FontFamily="Segoe Fluent Icons" FontSize="12" Text="&#xE893;"/></Button>
    </StackPanel>
   </Grid>
  </Grid>
 </Grid>
</Border>)xaml").as<c::Border>();
        metrics=card.FindName(L"Metrics").as<c::Grid>();media=card.FindName(L"Media").as<c::Grid>();
        cpu=card.FindName(L"Cpu").as<c::TextBlock>();memory=card.FindName(L"Memory").as<c::TextBlock>();
        title=card.FindName(L"Title").as<c::TextBlock>();artist=card.FindName(L"Artist").as<c::TextBlock>();
        cpuBar=card.FindName(L"CpuBar").as<x::Shapes::Rectangle>();memoryBar=card.FindName(L"MemoryBar").as<x::Shapes::Rectangle>();
        previous=card.FindName(L"Previous").as<c::Button>();play=card.FindName(L"Play").as<c::Button>();next=card.FindName(L"Next").as<c::Button>();
        playGlyph=card.FindName(L"PlayGlyph").as<c::TextBlock>();
        divider=x::Shapes::Rectangle();divider.Name(L"LeanDockCardDivider");divider.Width(1);divider.Height(40);
        divider.HorizontalAlignment(x::HorizontalAlignment::Right);divider.VerticalAlignment(x::VerticalAlignment::Center);
        divider.Margin({0,0,2,0});divider.IsHitTestVisible(false);
        x::Media::TranslateTransform shift;shift.X(12+SlotWidth);divider.RenderTransform(shift);
        x::Media::LinearGradientBrush brush;brush.StartPoint({.5f,0});brush.EndPoint({.5f,1});
        for(auto [offset,alpha]:{std::pair{0.0,16},std::pair{0.18,70},std::pair{0.82,70},std::pair{1.0,16}}){
            x::Media::GradientStop s;s.Offset(offset);s.Color({uint8_t(alpha),100,110,125});brush.GradientStops().Append(s);
        }
        divider.Fill(brush);c::Canvas::SetZIndex(card,20);c::Canvas::SetZIndex(divider,10);
        card.Visibility(x::Visibility::Collapsed);divider.Visibility(x::Visibility::Collapsed);
        root.Children().Append(card);root.Children().Append(divider);
        auto weak=weak_from_this();
        card.PointerWheelChanged([weak](IInspectable const& sender,x::Input::PointerRoutedEventArgs const& e){
            if(auto p=weak.lock();p && !p->stopped && p->session)try{
                if(AdvanceWheel(e.GetCurrentPoint(sender.as<x::UIElement>()).Properties().MouseWheelDelta(),p->wheelRemainder)){p->mediaView=!p->mediaView;p->Render();}
                e.Handled(true);
            }catch(...){p->Fail();}
        });
        previous.Click([weak](auto&&,auto&&){Command(weak,0);});
        play.Click([weak](auto&&,auto&&){Command(weak,1);});
        next.Click([weak](auto&&,auto&&){Command(weak,2);});
        unloaded=root.Unloaded(winrt::auto_revoke,[weak](auto&&,auto&&){if(auto p=weak.lock())p->Stop();});
        timer=x::DispatcherTimer();timer.Interval(std::chrono::seconds(2));
        timer.Tick([weak](auto&&,auto&&){if(auto p=weak.lock())try{p->Tick();}catch(...){p->Fail();}});
        c::ToolTipService::SetToolTip(card,winrt::box_value(L"CPU／内存；播放媒体后，可用滚轮切换"));
        Request(weak);
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
            bool nextFits=Fits(available,appWidth,tray,fits);
            const double slot=nextFits?SlotWidth:0;
            auto padding=root.Padding();if(padding.Left!=12||padding.Right!=12+slot||padding.Top||padding.Bottom)root.Padding({12,0,12+slot,0});
            auto margin=background.Margin();if(margin.Right!=-12-slot||margin.Left!=-13||margin.Top!=-1||margin.Bottom!=-1)background.Margin({-13,-1,-12-slot,-1});
            if(fits!=nextFits){fits=nextFits;card.Visibility(fits?x::Visibility::Visible:x::Visibility::Collapsed);divider.Visibility(card.Visibility());}
            shown=Visible();Sampling();Put(hwnd,0,1);Put(hwnd,1,shown);Publish();
            if(fits){
                auto content=root.XamlRoot().Content();
                auto pos=card.TransformToVisual(content).TransformPoint({0,0});
                auto appPos=apps.TransformToVisual(content).TransformPoint({0,0});
                auto rootPos=root.TransformToVisual(content).TransformPoint({0,0});
                // Read-only geometry evidence, centi-DIP relative to the island.
                Put(hwnd,10,std::lround(pos.X*100));Put(hwnd,11,std::lround(pos.Y*100));
                Put(hwnd,12,std::lround((pos.X+card.ActualWidth())*100));Put(hwnd,13,std::lround((pos.Y+card.ActualHeight())*100));
                Put(hwnd,14,std::lround((appPos.X+apps.ActualWidth())*100));Put(hwnd,15,std::lround((rootPos.X+root.ActualWidth())*100));
            }
            Put(hwnd,16,fits);
        }catch(...){Fail();}
        busy=false;
    }
    void Sampling() {
        const bool want=ShouldSample(shown,!mediaView,!stopped);
        if(want==sampling)return;
        sampling=want;Put(hwnd,2,sampling);
        if(want){baseline=Times();cpu.Text(L"—");cpuBar.Width(0);RemovePropW(hwnd,Props[4]);ReadMemory();timer.Start();}
        else{timer.Stop();baseline.reset();}
    }
    void ReadMemory(){MEMORYSTATUSEX s{sizeof(s)};if(GlobalMemoryStatusEx(&s)){
        memory.Text(winrt::to_hstring(uint32_t(s.dwMemoryLoad))+L"%");memoryBar.Width(65.0*s.dwMemoryLoad/100);
        Put(hwnd,5,s.dwMemoryLoad);
    }else{memory.Text(L"—");memoryBar.Width(0);RemovePropW(hwnd,Props[5]);}}
    void Tick(){
        if(stopped)return;shown=Visible();Put(hwnd,1,shown);Sampling();if(!sampling)return;
        auto current=Times();auto percent=(current&&baseline)?CpuPercent(*baseline,*current):std::nullopt;
        baseline=current;
        if(percent){cpu.Text(winrt::to_hstring(*percent)+L"%");cpuBar.Width(65.0**percent/100);Put(hwnd,4,*percent);}
        else{cpu.Text(L"—");cpuBar.Width(0);RemovePropW(hwnd,Props[4]);}
        ReadMemory();++samples;Put(hwnd,3,samples);
        x::Automation::AutomationProperties::SetName(card,L"CPU "+cpu.Text()+L"，内存 "+memory.Text());
    }
    void Publish(){Put(hwnd,3,samples);Put(hwnd,6,bool(session));Put(hwnd,7,mediaView);Put(hwnd,8,errors);Put(hwnd,9,commands);}
    void Render(){
        if(stopped)return;if(!session)mediaView=false;
        metrics.Visibility(mediaView?x::Visibility::Collapsed:x::Visibility::Visible);
        media.Visibility(mediaView?x::Visibility::Visible:x::Visibility::Collapsed);
        c::ToolTipService::SetToolTip(card,winrt::box_value(session?L"滚轮切换系统状态／媒体控制":L"CPU／内存；播放媒体后，可用滚轮切换"));
        Sampling();Publish();
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
            propertiesChanged.revoke();playbackChanged.revoke();session=current;
            title.Text(L"媒体");artist.Text(L"");
            if(session){auto weak=weak_from_this();
                propertiesChanged=session.MediaPropertiesChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
                playbackChanged=session.PlaybackInfoChanged(winrt::auto_revoke,[weak](auto&&,auto&&){Queue(weak);});
            }
        }
        ++generation;if(metadata){metadata.Cancel();metadata=nullptr;}
        if(session){
            auto info=session.GetPlaybackInfo();auto controls=info.Controls();
            previous.IsEnabled(controls.IsPreviousEnabled()&&!commandBusy);next.IsEnabled(controls.IsNextEnabled()&&!commandBusy);
            play.IsEnabled(controls.IsPlayPauseToggleEnabled()&&!commandBusy);
            const bool playing=info.PlaybackStatus()==m::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            playGlyph.Text(playing?L"\xE769":L"\xE768");
            x::Automation::AutomationProperties::SetName(play,playing?L"暂停":L"播放");
            Metadata(weak_from_this(),session,generation);
        }
        Render();
    }catch(...){
        Fail();++generation;
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
            p->title.Text(data.Title().empty()?L"媒体":data.Title());p->artist.Text(data.Artist());
            x::Automation::AutomationProperties::SetName(p->media,p->title.Text()+L" "+p->artist.Text());
        }catch(winrt::hresult_canceled const&){}catch(...){if(auto p=weak.lock();p&&!p->stopped&&p->generation==ticket)p->Fail();}
    }
    static winrt::fire_and_forget Command(std::weak_ptr<Board> weak,int action){
        Session source{nullptr};
        try{
            auto p=weak.lock();if(!p||p->stopped||p->commandBusy||!p->session)co_return;
            source=p->session;p->commandBusy=true;p->previous.IsEnabled(false);p->play.IsEnabled(false);p->next.IsEnabled(false);p.reset();
            bool success=false;
            if(action==0)success=co_await source.TrySkipPreviousAsync();
            else if(action==1)success=co_await source.TryTogglePlayPauseAsync();
            else success=co_await source.TrySkipNextAsync();
            if(auto q=weak.lock();q&&!q->stopped){q->commandBusy=false;if(success)++q->commands;else q->Fail();q->Refresh();}
        }catch(...){if(auto p=weak.lock();p&&!p->stopped){p->commandBusy=false;p->Fail();try{p->Refresh();}catch(...){}}}
    }
    void Stop() noexcept override {
        if(stopped)return;stopped=true;++generation;
        try{if(timer)timer.Stop();}catch(...){}
        try{if(request)request.Cancel();if(metadata)metadata.Cancel();}catch(...){}
        managerChanged.revoke();sessionsChanged.revoke();propertiesChanged.revoke();playbackChanged.revoke();unloaded.revoke();
        try{if(root){for(auto e:{card.try_as<x::UIElement>(),divider.try_as<x::UIElement>()}){uint32_t index=0;if(e&&root.Children().IndexOf(e,index))root.Children().RemoveAt(index);}
            if(oldPadding==x::DependencyProperty::UnsetValue())root.ClearValue(c::Grid::PaddingProperty());else root.SetValue(c::Grid::PaddingProperty(),oldPadding);
        }}catch(...){}
        try{if(background){if(oldMargin==x::DependencyProperty::UnsetValue())background.ClearValue(x::FrameworkElement::MarginProperty());else background.SetValue(x::FrameworkElement::MarginProperty(),oldMargin);}}catch(...){}
        for(auto key:Props)if(hwnd)RemovePropW(hwnd,key);
        session=nullptr;manager=nullptr;
    }
    bool Alive()const noexcept override{return !stopped;}
};
std::shared_ptr<Host> Create(c::Grid const& root){auto p=std::make_shared<Board>();p->Init(root);return p;}
}
