// Exercise the actual material adapter in a test-owned hidden native XAML
// island. This is component/layout validation, not desktop gesture acceptance.
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Composition.h>
#include <vector>
#include <cmath>
#include <iostream>
#include "../include/backdrop-resume-latest.h"
using namespace winrt::Windows::UI::Xaml;
struct ClickThroughTaskbarState {
    HWND islandHwnd{};bool cardsClosing=false,materialFailed=false;
    winrt::weak_ref<Controls::Grid> material,materialParent;
    winrt::weak_ref<Controls::Canvas> materialHost;
    winrt::weak_ref<Shapes::Rectangle> materialSource;
};
std::vector<ClickThroughTaskbarState> g_clickThroughTaskbarState;
#include "../src/material-latest.inc"
static unsigned checks;
void Check(bool value,const char* what){++checks;if(!value)throw std::runtime_error(what);}
void Pump(){MSG m{};for(unsigned i=0;i<5;++i){while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(10);}}
int main(){
    HWND window{};
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto manager=Hosting::WindowsXamlManager::InitializeForCurrentThread();
        window=CreateWindowExW(0,L"STATIC",L"LeanDock material fixture",WS_OVERLAPPEDWINDOW,0,0,900,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window!=nullptr,"host window");
        Hosting::DesktopWindowXamlSource island;
        auto native=island.as<IDesktopWindowXamlSourceNative>();
        winrt::check_hresult(native->AttachToWindow(window));HWND child{};
        winrt::check_hresult(native->get_WindowHandle(&child));
        SetWindowPos(child,nullptr,0,0,900,100,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        Controls::Grid root;root.HorizontalAlignment(HorizontalAlignment::Left);root.VerticalAlignment(VerticalAlignment::Top);
        Controls::Grid background;background.Name(L"BackgroundControl");background.Height(60);background.VerticalAlignment(VerticalAlignment::Top);
        Controls::Grid sourceParent;Shapes::Rectangle source;source.Name(L"BackgroundFill");source.RadiusX(23);source.RadiusY(30);
        sourceParent.Children().Append(source);background.Children().Append(sourceParent);
        Media::ScaleTransform scale;scale.ScaleX(1.4);background.RenderTransform(scale);background.RenderTransformOrigin({0,0});
        Controls::Border apps;apps.Width(300);apps.Height(60);
        root.Children().Append(background);root.Children().Append(apps);island.Content(root);root.UpdateLayout();Pump();
        ClickThroughTaskbarState state;state.islandHwnd=child;
        UpdateLeanDockMaterial(state,root);root.UpdateLayout();Pump();
        auto layer=state.material.get();auto host=state.materialHost.get();
        Check(layer&&host&&!state.materialFailed,"adapter created");
        Check(Media::VisualTreeHelper::GetParent(layer)==host&&Media::VisualTreeHelper::GetParent(host)==root,"sibling ownership");
        Check(!host.IsHitTestVisible()&&!layer.IsHitTestVisible(),"decoration is input transparent");
        Check(root.Children().Size()==3&&root.Children().GetAt(1)==host,"behind foreground items");
        auto pigment=layer.Children().GetAt(0).as<Shapes::Rectangle>();
        for(auto width:{300.,210.,410.,180.}){
            apps.Width(width);root.UpdateLayout();Pump();UpdateLeanDockMaterial(state,root);root.UpdateLayout();Pump();
            Check(std::abs(root.ActualWidth()-width)<0.1,"root can grow and shrink");
            Check(std::abs(layer.ActualWidth()-background.ActualWidth())<0.1,"width tracks native surface");
            const auto a=background.TransformToVisual(root).TransformBounds({0,0,float(background.ActualWidth()),float(background.ActualHeight())});
            const auto b=layer.TransformToVisual(root).TransformBounds({0,0,float(layer.ActualWidth()),float(layer.ActualHeight())});
            Check(std::abs(a.X-b.X)<0.1&&std::abs(a.Y-b.Y)<0.1&&std::abs(a.Width-b.Width)<0.1&&std::abs(a.Height-b.Height)<0.1,"transformed surface matches");
            Check(root.Children().Size()==3,"no duplicate layers");
        }
        background.Opacity(0.15);
        Hosting::ElementCompositionPreview::GetElementVisual(background).Opacity(0.2f);
        root.UpdateLayout();Pump();
        Check(layer.Opacity()==1&&Hosting::ElementCompositionPreview::GetElementVisual(layer).Opacity()==1,"native background opacity not shared");
        for(auto node=Media::VisualTreeHelper::GetParent(layer);node;node=Media::VisualTreeHelper::GetParent(node))
            Check(node!=background&&node!=sourceParent,"no native background ancestor");
        source.RadiusX(19);Pump();Check(pigment.RadiusX()==19,"live radius binding");
        for(auto theme:{ElementTheme::Light,ElementTheme::Dark}){
            root.RequestedTheme(theme);root.UpdateLayout();Pump();
            auto c=pigment.Fill().as<Media::SolidColorBrush>().Color();
            unsigned argb=(unsigned(c.A)<<24)|(unsigned(c.R)<<16)|(unsigned(c.G)<<8)|c.B;
            Check(argb==(theme==ElementTheme::Light?0x9EC8D1DEu:0xB3252C38u),"theme resources preserved");
        }
        ClearLeanDockMaterial(state);Check(root.Children().Size()==2&&!state.material.get(),"owned layer cleanup");
        UpdateLeanDockMaterial(state,root);root.UpdateLayout();Pump();Check(root.Children().Size()==3&&!state.materialFailed,"reload");
        ClearLeanDockMaterial(state);island.Content(nullptr);island.Close();manager.Close();DestroyWindow(window);
        std::cout<<checks<<" checks passed; native layout and lifetime only, real gesture unverified\n";
        return 0;
    }catch(winrt::hresult_error const& e){std::cerr<<std::hex<<unsigned(e.code())<<" "<<winrt::to_string(e.message())<<'\n';}
    catch(std::exception const& e){std::cerr<<e.what()<<'\n';}
    if(window)DestroyWindow(window);return 1;
}
