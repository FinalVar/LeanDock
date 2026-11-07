#include "../src/cards-aggregate-latest.cpp"
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <iostream>
extern "C" void Native_tray_refresh_cards(){}
extern "C" size_t InternalWh_GetModStoragePath(void*,PWSTR,size_t){return 0;}
namespace LeanDockMenu{Reopen ReopenVisible(const Gesture&){return Reopen::NotOpen;}}
using namespace winrt::Windows::UI::Xaml;
int main(){HWND host{};
 try{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto manager=Hosting::WindowsXamlManager::InitializeForCurrentThread();
    host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"aggregate lifetime fixture",WS_POPUP,-4096,-4096,900,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Hosting::DesktopWindowXamlSource island;auto native=island.as<IDesktopWindowXamlSourceNative>();winrt::check_hresult(native->AttachToWindow(host));
    HWND child{};winrt::check_hresult(native->get_WindowHandle(&child));SetWindowPos(child,nullptr,0,0,900,96,SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW);
    Controls::Grid root;island.Content(root);root.Padding({3,2,4,1});
    Controls::Border background;background.Name(L"BackgroundControl");background.Margin({1,2,3,4});
    background.Height(60);background.VerticalAlignment(VerticalAlignment::Top);root.Children().Append(background);
    Controls::Grid apps;apps.Name(L"TaskbarFrameRepeater");apps.Width(350);root.Children().Append(apps);
    root.UpdateLayout();
    unsigned updates=0;
    std::cout<<"before production Board::Init"<<std::endl;
    auto board=LeanDockCards::Create(root);
    std::cout<<"after production Board::Init"<<std::endl;
    auto token=root.LayoutUpdated([&](auto&&,auto&&){if(++updates>1000){std::cerr<<"layout never settled\n";ExitProcess(91);}board->Update(host,1500,100);});
    board->Update(host,1500,100);
    const auto until=GetTickCount64()+3000;MSG m{};
    while(GetTickCount64()<until){while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(5);}
    if(IsWindowVisible(host)||reinterpret_cast<intptr_t>(GetPropW(host,L"LeanDock.Card.Errors"))>1)throw std::runtime_error("hidden native host failed");
    auto card=board->MenuTarget();if(!card||card.ActualWidth()!=72||card.ActualHeight()!=46)throw std::runtime_error("native card missing");
    const auto expected=background.TransformToVisual(root).TransformPoint({0,float(background.ActualHeight()/2)});
    const auto actual=card.TransformToVisual(root).TransformPoint({0,float(card.ActualHeight()/2)});
    if(std::abs(actual.Y-expected.Y)>.01)throw std::runtime_error("key not centered on visible surface");
    if(root.Padding().Right!=78)throw std::runtime_error("legacy dashboard gutter remains");
    std::cout<<"surfaceCenter="<<expected.Y<<", keyCenter="<<actual.Y<<", reservedWidth="<<root.Padding().Right<<'\n';
    std::cout<<"layoutUpdates="<<updates<<", nativeCard="<<card.ActualWidth()<<"x"<<card.ActualHeight()<<'\n';
    root.LayoutUpdated(token);board->Stop();board.reset();
    if(root.Padding().Left!=3||background.Margin().Right!=3||GetPropW(host,L"LeanDock.Card.Ready"))throw std::runtime_error("original properties not restored");
    island.Content(nullptr);island.Close();manager.Close();DestroyWindow(host);host=nullptr;
    std::cout<<"production native host init/layout/power subscriptions/stop restored; hidden fixture\n";return 0;
 }catch(winrt::hresult_error const& e){std::cerr<<std::hex<<unsigned(e.code())<<" "<<winrt::to_string(e.message())<<'\n';}
 catch(std::exception const& e){std::cerr<<e.what()<<'\n';}
 if(host)DestroyWindow(host);return 1;
}
