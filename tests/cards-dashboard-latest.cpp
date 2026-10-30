// Test-owned hidden XAML island. Never shows a desktop window or sends input.
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <iostream>
#include "../include/cards-dashboard-latest.h"
#include "../src/cards-dashboard-xaml-latest.inc"
using namespace winrt::Windows::UI::Xaml;
using namespace LeanDockCards;
int checks{};
void Check(bool ok,const char* name){++checks;if(!ok)throw std::runtime_error(name);}
void Pump(unsigned ms=40){auto until=GetTickCount64()+ms;MSG msg{};do{
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(5);
}while(GetTickCount64()<until);}
int main(){HWND host{};
 try{
    PageOrder order;
    Check(Dashboard::Width(order,false,false)==358,"baseline includes four useful modules without media gap");
    Check(Dashboard::Width(order,true,false)==388,"available media adds only a compact entry");
    Check(Dashboard::Width(order,true,true)==492,"expanded media shares its title and control width");
    auto custom=*ParseOrder(L"power,clock");
    Check(Dashboard::Width(custom,false,false)==154,"explicit existing subset has no disabled gaps");
    custom=*ParseOrder(L"media");
    Check(Dashboard::Width(custom,false,false)==24,"Start survives when every content module is absent");
    Check(Dashboard::Width(custom,true,false)==54,"disabled clock does not remove Start or duplicate its gap");
    Check(Dashboard::MemoryText(16ull<<30,6ull<<30)==L"10.0 / 16.0 GB","physical memory unit conversion");
    Check(Dashboard::MemoryText(0,0)==L"— / — GB"&&Dashboard::MemoryText(1,2)==L"— / — GB","unknown and inconsistent memory are not displayed as real zero");
    for(unsigned n=1;n<=32;++n){double last=40;
        for(double budget=40;budget<=1800;budget+=3){const double size=Dashboard::AppWidth(budget,n*52,n);
            Check(size>=40&&size<=52&&size>=last,"app size grows monotonically with available width");last=size;}}
    Check(Dashboard::AppWidth(520,520,10)==52,"normal app size retained when content fits");
    Check(Dashboard::AppWidth(519,520,10)==48,"crowding takes a modest first step");
    Check(Dashboard::AppWidth(520,520,10,48)==48&&Dashboard::AppWidth(528,520,10,48)==52,"hysteresis prevents alternating sizes at the boundary");
    Check(Dashboard::Delay(1000,true,1200,true,60000,true,false,2000)==200,"CPU warmup is not postponed by minute clock");
    Check(Dashboard::Delay(1200,true,3200,true,60000,true,false,2000)==800,"network remains at its own deadline");
    Check(Dashboard::Delay(2000,true,3200,true,60000,true,true,2000)==1200,"pending network read is not polled");
    Check(Dashboard::Delay(2000,false,0,false,0,true,true,0)==0,"only pending async work needs no timer");
    Check(Dashboard::Delay(2000,false,0,false,0,false,false,0)==0,"hidden dashboard has no sampling timer");
    Check(Dashboard::Delay(2100,true,2000,false,0,false,false,0)==50,"overdue work has a bounded wakeup");

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto manager=Hosting::WindowsXamlManager::InitializeForCurrentThread();
    host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"LeanDock dashboard fixture",WS_POPUP,-4096,-4096,1400,150,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(host!=nullptr,"hidden host created");
    Hosting::DesktopWindowXamlSource island;auto native=island.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(native->AttachToWindow(host));HWND child{};winrt::check_hresult(native->get_WindowHandle(&child));
    SetWindowPos(child,nullptr,0,0,1400,150,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    Controls::Grid root;island.Content(root);
    auto board=Markup::XamlReader::Load(DashboardXaml).as<Controls::Border>();
    board.HorizontalAlignment(HorizontalAlignment::Left);board.Width(Dashboard::Width(order,false,false));root.Children().Append(board);root.UpdateLayout();Pump();
    Check(!IsWindowVisible(host),"test remains invisible");
    auto modules=board.FindName(L"DashboardModules").as<Controls::StackPanel>();
    auto power=Markup::XamlReader::Load(DashboardPowerXaml).as<Controls::Button>();power.Width(Dashboard::PowerWidth);
    uint32_t powerIndex=0;Check(modules.Children().IndexOf(board.FindName(L"PowerPlaceholder").as<UIElement>(),powerIndex),"battery placeholder found");
    modules.Children().SetAt(powerIndex,power);
    auto media=board.FindName(L"Media").as<Controls::Grid>();auto group=board.FindName(L"MediaGroup").as<Controls::Grid>();
    auto toggle=board.FindName(L"MediaToggle").as<Controls::Button>();
    auto summary=board.FindName(L"MetricsSummary").as<Controls::TextBlock>();
    summary.Text(L"CPU 100%  内存 100%");
    auto memory=board.FindName(L"MemoryUsage").as<Controls::TextBlock>();memory.Text(L"15.4 / 15.4 GB");
    auto time=board.FindName(L"ClockTime").as<Controls::TextBlock>();time.Text(L"23:59");
    auto date=board.FindName(L"ClockDate").as<Controls::TextBlock>();date.Text(L"12月31日");
    for(auto pair:{std::pair{summary,Dashboard::MetricsWidth},std::pair{memory,Dashboard::MetricsWidth},std::pair{time,60.0},std::pair{date,60.0}}){
        pair.first.Measure({10000,10000});std::cout<<winrt::to_string(pair.first.Name())<<" required="<<pair.first.DesiredSize().Width<<" available="<<pair.second<<'\n';
        Check(pair.first.DesiredSize().Width<=pair.second,"worst-case numeric text fits without clipping");}
    auto percent=power.FindName(L"PowerText").as<Controls::TextBlock>();percent.Text(L"100%");
    percent.Measure({10000,10000});
    std::cout<<"PowerText required="<<percent.DesiredSize().Width<<" available="<<Dashboard::PowerWidth-28<<'\n';
    Check(percent.FontSize()==14&&percent.DesiredSize().Width<=Dashboard::PowerWidth-28,"full battery percentage fits at original font size");
    auto status=power.FindName(L"PowerStatus").as<Controls::TextBlock>();
    for(auto caption:{L"正在充电",L"已充满",L"插电未充电",L"未检测到电池",L"电源状态未知",L"已插电 · 未知",L"使用电池"}){
        status.Text(caption);status.Measure({10000,10000});
        Check(status.DesiredSize().Width<=Dashboard::PowerWidth,"battery status never clips or trims");}
    root.UpdateLayout();Pump();
    auto powerSummary=power.FindName(L"PowerSummary").as<Controls::StackPanel>();
    auto summaryAt=powerSummary.TransformToVisual(power).TransformPoint({0,0});
    auto statusAt=status.TransformToVisual(power).TransformPoint({0,0});
    Check(summaryAt.X>=0&&summaryAt.X+powerSummary.ActualWidth()<=Dashboard::PowerWidth+0.1,"battery icon and percentage stay inside actual native hit rectangle");
    Check(statusAt.Y>=summaryAt.Y+powerSummary.ActualHeight()-0.1&&statusAt.Y+status.ActualHeight()<=44.1,"battery rows never overlap");
    auto level=power.FindName(L"PowerLevelHost").as<Controls::Grid>();
    auto fill=power.FindName(L"PowerFill").as<Shapes::Rectangle>();
    for(int charge:{0,1,50,99,100}){
        level.ColumnDefinitions().GetAt(0).Width({double(charge),GridUnitType::Star});
        level.ColumnDefinitions().GetAt(1).Width({100.0-charge,GridUnitType::Star});root.UpdateLayout();Pump();
        Check(fill.ActualWidth()>=0&&fill.ActualWidth()<=level.ActualWidth()+0.1,"battery fill stays inside existing border");
        if(charge==0)Check(fill.ActualWidth()==0,"empty battery does not appear charged");
        if(charge==100)Check(std::abs(fill.ActualWidth()-level.ActualWidth())<0.1,"full battery still fills the whole interior");
    }
    board.FindName(L"Title").as<Controls::TextBlock>().Text(L"很长的媒体标题用于确认展开后不会挤占其他信息或播放按钮");
    for(auto mode:{0,1,2,0}){
        group.Visibility(mode?Visibility::Visible:Visibility::Collapsed);
        ArrangeDashboardMedia(group,media,toggle,mode!=0,mode==2);
        board.Width(Dashboard::Width(order,mode!=0,mode==2));root.InvalidateMeasure();root.UpdateLayout();Pump();
        double end=0;unsigned visible=0;
        for(auto item:modules.Children()){auto fe=item.as<FrameworkElement>();if(fe.Visibility()!=Visibility::Visible)continue;
            const auto pos=fe.TransformToVisual(board).TransformPoint({0,0});
            Check(pos.X>=end-0.1,"modules never overlap");end=pos.X+fe.ActualWidth();++visible;
            Check(pos.Y>=-0.1&&pos.Y+fe.ActualHeight()<=44.1,"module height remains inside dashboard");}
        std::cout<<"mode="<<mode<<" measured="<<end<<" reserved="<<board.ActualWidth()<<'\n';
        Check(std::abs(end-board.ActualWidth())<0.1,"no trailing placeholder or unreserved media width");
        Check(visible==(mode?5u:4u),"empty media is genuinely collapsed");
        if(mode==2){
            auto mediaTitle=board.FindName(L"Title").as<Controls::TextBlock>();
            auto titleAt=mediaTitle.TransformToVisual(group).TransformPoint({0,0});
            double right=0;
            for(auto button:{toggle,board.FindName(L"Previous").as<Controls::Button>(),board.FindName(L"Play").as<Controls::Button>(),board.FindName(L"Next").as<Controls::Button>()}){
                const auto at=button.TransformToVisual(group).TransformPoint({0,0});
                Check(at.X>=right-0.1&&at.X+button.ActualWidth()<=group.ActualWidth()+0.1,"all four media hit rectangles are separate and inside the module");
                Check(button.ActualWidth()>=24&&button.ActualHeight()>=24,"media buttons keep original usable hit size");
                Check(at.Y>=titleAt.Y+mediaTitle.ActualHeight()-0.1&&at.Y+button.ActualHeight()<=44.1,"media title does not cover playback controls");
                right=at.X+button.ActualWidth();
            }
        }else if(mode==1){
            const auto at=toggle.TransformToVisual(group).TransformPoint({0,0});
            Check(at.X==0&&at.Y==0&&toggle.ActualHeight()==44,"collapsed media entry regains its full-height hit target");
        }
    }
    for(auto theme:{ElementTheme::Light,ElementTheme::Dark,ElementTheme::Light}){
        root.RequestedTheme(theme);root.UpdateLayout();Pump();
        auto color=summary.Foreground().as<Media::SolidColorBrush>().Color();
        Check((unsigned(color.R)+color.G+color.B>384)==(theme==ElementTheme::Dark),"text follows actual light-dark-light changes");}
    Check(!IsWindowVisible(host),"fixture never became visible during layout tests");
    island.Content(nullptr);island.Close();manager.Close();DestroyWindow(host);host=nullptr;
    std::cout<<checks<<" checks passed; hidden native layout only, real Dock clicks pending\n";return 0;
 }catch(winrt::hresult_error const& e){std::cerr<<std::hex<<unsigned(e.code())<<" "<<winrt::to_string(e.message())<<'\n';}
 catch(std::exception const& e){std::cerr<<e.what()<<'\n';}
 if(host)DestroyWindow(host);return 1;
}
