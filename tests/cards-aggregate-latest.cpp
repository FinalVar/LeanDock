// Owned hidden XAML island. No Explorer injection, visible window or input.
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <iostream>
#include "../include/cards-aggregate-geometry-latest.h"
#include "../src/cards-aggregate-xaml-latest.inc"
using namespace winrt::Windows::UI::Xaml;
namespace a=LeanDockAggregate;
int checks=0;
void Check(bool ok,char const* name){++checks;if(!ok)throw std::runtime_error(name);}
void Pump(){const auto until=GetTickCount64()+60;MSG m{};do{while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(5);}while(GetTickCount64()<until);}
int main(){HWND host{};
 try{
    const double perimeter=2*(a::Width-a::Height)+2*a::Pi*(a::Height/2-a::Inset);
    double previous=0;
    for(int percent=0;percent<=100;++percent){double length=0;
        auto segments=a::Ring(percent);
        for(auto const& s:segments){length+=s.length;Check(s.x>=a::Inset-.001&&s.x<=a::Width-a::Inset+.001&&s.y>=a::Inset-.001&&s.y<=a::Height-a::Inset+.001,"ring remains inside contour");}
        Check(std::abs(length-perimeter*percent/100)<.00001,"charge maps to physical perimeter");
        Check(length>=previous,"battery progress monotonic");previous=length;
        if(percent==50)Check(std::abs(segments.back().x-36)<.001&&std::abs(segments.back().y-44.4)<.001,"half charge reaches opposite midpoint");
        if(percent==100)Check(std::abs(segments.back().x-36)<.001&&std::abs(segments.back().y-1.6)<.001,"full charge meets start without seam");
    }
    Check(a::Ring(-1).empty()&&a::Ring(200).size()==5,"invalid charge clamp");
    Check(a::BatteryTone(-1,false)==0&&a::BatteryTone(0,false)==2,"unknown versus empty battery");
    Check(a::BatteryTone(9,false)==2&&a::BatteryTone(10,false)==1&&a::BatteryTone(19,false)==1&&a::BatteryTone(20,false)==0,"battery warning boundaries");
    for(int percent:{0,9,10,19,20,100})Check(a::BatteryTone(percent,true)==0,"plugging in removes discharge warning without changing percent");
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto manager=Hosting::WindowsXamlManager::InitializeForCurrentThread();
    host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"LeanDock aggregate fixture",WS_POPUP,-4096,-4096,400,150,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(host!=nullptr,"owned invisible host");
    Hosting::DesktopWindowXamlSource island;auto native=island.as<IDesktopWindowXamlSourceNative>();winrt::check_hresult(native->AttachToWindow(host));
    HWND child{};winrt::check_hresult(native->get_WindowHandle(&child));SetWindowPos(child,nullptr,0,0,400,150,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    Controls::Grid root;island.Content(root);
    auto card=Markup::XamlReader::Load(AggregateXaml).as<Controls::Border>();root.Children().Append(card);root.UpdateLayout();Pump();
    // Reproduce the rejected initial candidate, then assert distinct ownership.
    auto sameGeometry=a::Geometry(97);
    bool rejected=false;
    try{for(auto name:{L"RingNormal",L"RingLow",L"RingCritical"})card.FindName(name).as<Shapes::Path>().Data(sameGeometry);}
    catch(winrt::hresult_error const& e){rejected=e.code()==E_INVALIDARG;}
    Check(rejected,"native geometry cannot be reparented across states");
    for(auto name:{L"RingNormal",L"RingLow",L"RingCritical"})card.FindName(name).as<Shapes::Path>().Data(a::Geometry(97));
    auto key=card.FindName(L"TimeKey").as<Controls::Button>();key.ApplyTemplate();
    auto time=card.FindName(L"ClockTime").as<Controls::TextBlock>();auto date=card.FindName(L"ClockDate").as<Controls::TextBlock>();
    time.Text(L"23:59");date.Text(L"12/31");
    for(auto text:{time,date}){text.Measure({10000,10000});Check(text.DesiredSize().Width<=a::Width-2*a::Inset,"worst-case text fits inside ring");}
    for(auto theme:{ElementTheme::Light,ElementTheme::Dark,ElementTheme::Light}){
        root.RequestedTheme(theme);root.UpdateLayout();Pump();
        Check(card.ActualWidth()==72&&card.ActualHeight()==46&&key.ActualWidth()==72,"native key exact accepted geometry");
        auto color=time.Foreground().as<Media::SolidColorBrush>().Color();Check((unsigned(color.R)+color.G+color.B>384)==(theme==ElementTheme::Dark),"text follows real theme change");
        auto path=card.FindName(L"RingNormal").as<Shapes::Path>();
        for(int percent:{0,1,10,19,50,99,100}){path.Data(a::Geometry(percent));root.UpdateLayout();
            Check(path.Data().as<Media::PathGeometry>().Figures().GetAt(0).IsClosed()==(percent==100),"full ring closure only at full charge");}
    }
    Check(!IsWindowVisible(host),"fixture remained hidden");island.Content(nullptr);island.Close();manager.Close();DestroyWindow(host);host=nullptr;
    std::cout<<checks<<" checks passed; native aggregate geometry/theme/text; live input not tested\n";return 0;
 }catch(winrt::hresult_error const& e){std::cerr<<std::hex<<unsigned(e.code())<<" "<<winrt::to_string(e.message())<<'\n';}
 catch(std::exception const& e){std::cerr<<e.what()<<'\n';}
 if(host)DestroyWindow(host);return 1;
}
