// A hidden, test-owned XAML island. No taskbar injection, screen capture,
// mouse/keyboard input or foreground-window changes.
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string_view>
#include <iostream>
#include "../include/icon-optics-latest.h"
using namespace winrt::Windows::UI::Xaml;
enum class ElementId:uint64_t {None=0,Icon=1};
struct ClickThroughTaskbarState {HWND islandHwnd{};winrt::weak_ref<FrameworkElement> taskbarFrame;};
std::vector<ClickThroughTaskbarState> g_clickThroughTaskbarState;
#include "../src/icon-optics-latest.inc"
int checks{};
void Check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
void Pump(unsigned ms=100){auto until=GetTickCount64()+ms;MSG msg{};do{
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(5);
}while(GetTickCount64()<until);}
void Wait(std::shared_ptr<LeanDockIconView::State> const& state){
    const auto until=GetTickCount64()+5000;while(state->busy&&GetTickCount64()<until)Pump(10);
    Check(!state->busy,"render completed within five seconds");
}
winrt::fire_and_forget Encode(Media::Imaging::WriteableBitmap source,
                             Media::Imaging::BitmapImage target,bool& done,bool& ok){
    try{
        using namespace winrt::Windows::Graphics::Imaging;
        winrt::Windows::Storage::Streams::InMemoryRandomAccessStream stream;
        auto encoder=co_await BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(),stream);
        auto b=source.PixelBuffer();
        encoder.SetPixelData(BitmapPixelFormat::Bgra8,BitmapAlphaMode::Premultiplied,
            source.PixelWidth(),source.PixelHeight(),96,96,{b.data(),b.data()+b.Length()});
        co_await encoder.FlushAsync();stream.Seek(0);co_await target.SetSourceAsync(stream);ok=true;
    }catch(...){}done=true;
}
int main(){
    HWND window{};
    try{
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto manager=Hosting::WindowsXamlManager::InitializeForCurrentThread();
        window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"LeanDock icon fixture",WS_POPUP,
            GetSystemMetrics(SM_XVIRTUALSCREEN)-4096,GetSystemMetrics(SM_YVIRTUALSCREEN)-4096,200,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window!=nullptr,"hidden test host created");
        Hosting::DesktopWindowXamlSource island;auto native=island.as<IDesktopWindowXamlSourceNative>();
        winrt::check_hresult(native->AttachToWindow(window));HWND child{};winrt::check_hresult(native->get_WindowHandle(&child));
        SetWindowPos(child,nullptr,0,0,200,200,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        Controls::Grid root;Controls::Image image;image.Name(L"Icon");image.Width(40);image.Height(40);
        Media::Imaging::WriteableBitmap bitmap(64,64);
        auto bytes=bitmap.PixelBuffer().data();std::fill(bytes,bytes+64*64*4,0);
        for(int y=4;y<58;++y)for(int x=5;x<59;++x){auto p=bytes+(y*64+x)*4;p[0]=128;p[3]=255;}
        bitmap.Invalidate();image.Source(bitmap);root.Children().Append(image);island.Content(root);root.UpdateLayout();Pump();
        for(int i=0;i<50&&(!image.IsLoaded()||image.ActualWidth()<1);++i){root.UpdateLayout();Pump(20);}
        Check(image.IsLoaded(),"native image loaded");
        std::cout<<"image size="<<image.ActualWidth()<<','<<image.ActualHeight()<<" source="<<bool(image.Source())
                 <<" bound="<<bool(image.GetBindingExpression(UIElement::RenderTransformProperty()))<<" visibility="<<int(image.Visibility())<<'\n';
        Check(!IsWindowVisible(window),"no desktop window shown");
        Check(!LeanDockIconView::IsAppIcon(image),"non-taskbar image excluded by admission");
        auto s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=1;
        const auto originalLocal=image.ReadLocalValue(UIElement::RenderTransformProperty());
        LeanDockIconView::states.emplace(ElementId::Icon,s);
        // Call the production measurement after testing the admission boundary.
        LeanDockIconView::Measure(s);Wait(s);
        const auto status=reinterpret_cast<ULONG_PTR>(GetPropW(window,L"LeanDock.Icons.1.Status"));
        std::cout<<"hidden render status="<<(status?status-1:99)<<'\n';
        Check(status==3,"native image pixels measured and correction applied");
        Check(s->applied&&image.RenderTransform()==s->applied,"owned transform installed");
        auto correction=s->applied.Children().GetAt(0).as<Media::CompositeTransform>();
        Check(correction.ScaleX()>1.15&&correction.ScaleX()<1.23,"padded artwork automatically enlarged");
        Check(correction.ScaleX()==correction.ScaleY(),"uniform scale");
        Check(correction.TranslateY()>0,"asymmetric padding centered");
        Check(image.Width()==40&&image.Height()==40&&image.Source()==bitmap,"source and layout unchanged");
        LeanDockIconView::Remove(ElementId::Icon);
        Check(!s->active&&!s->applied&&LeanDockIconView::states.empty(),"lifetime removed");
        Check(image.ReadLocalValue(UIElement::RenderTransformProperty())==originalLocal,"original local transform restored");
        Check(!GetPropW(window,L"LeanDock.Icons.1.Status"),"diagnostic cleanup");
        // A native owner replacing our transform must not be rolled back.
        s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=2;
        LeanDockIconView::states.emplace(ElementId::Icon,s);LeanDockIconView::Measure(s);Wait(s);
        Media::TranslateTransform nativeTransform;nativeTransform.X(2);image.RenderTransform(nativeTransform);
        LeanDockIconView::Remove(ElementId::Icon);
        Check(image.RenderTransform()==nativeTransform,"native replacement retains ownership");
        image.ClearValue(UIElement::RenderTransformProperty());
        // Actual BitmapImage has no readable pixel buffer. Its private neutral
        // sample must ignore live clip/offset while retaining the same source.
        Media::Imaging::BitmapImage encoded;bool encodedDone=false,encodedOk=false;
        Encode(bitmap,encoded,encodedDone,encodedOk);
        for(int i=0;i<500&&!encodedDone;++i)Pump(10);
        Check(encodedDone&&encodedOk,"real in-memory BitmapImage decoded");
        ShowWindow(window,SW_SHOWNOACTIVATE);Pump(100);
        Check(GetForegroundWindow()!=window&&GetForegroundWindow()!=child,"offscreen rendering does not activate test window");
        RECT testRect{};GetWindowRect(window,&testRect);
        Check(testRect.right<GetSystemMetrics(SM_XVIRTUALSCREEN),"render fixture outside every desktop monitor");
        image.Source(encoded);Media::TranslateTransform original;original.X(3);original.Y(5);
        image.RenderTransform(original);image.RenderTransformOrigin({0.5f,0.5f});
        Media::RectangleGeometry clip;clip.Rect({0,5,40,30});image.Clip(clip);root.UpdateLayout();Pump();
        std::cout<<"decoded source="<<encoded.PixelWidth()<<','<<encoded.PixelHeight()<<'\n';
        auto childrenBefore=root.Children().Size();
        s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=4;
        LeanDockIconView::states.emplace(ElementId::Icon,s);LeanDockIconView::Measure(s);Wait(s);
        for(int attempt=0;attempt<2&&!s->applied;++attempt){Pump(50);LeanDockIconView::CancelRetry(*s);LeanDockIconView::Measure(s);Wait(s);}
        for(auto key:LeanDockIconView::properties){auto value=reinterpret_cast<LONG_PTR>(GetPropW(window,LeanDockIconView::Property(*s,key).c_str()));if(value)std::cout<<winrt::to_string(key)<<'='<<value-1<<' ';}std::cout<<'\n';
        Check(bool(s->applied),"BitmapImage uses neutral source rendering");
        correction=s->applied.Children().GetAt(0).as<Media::CompositeTransform>();
        Check(std::abs(correction.ScaleX()-64.0/54)<0.015,"rendered size excludes native clipping");
        auto center=s->applied.TransformPoint({0,-0.625f});
        Check(std::abs(center.X-3)<0.2&&std::abs(center.Y-5)<0.2,"visible center preserves native translation without magnifying it");
        Check(s->applied.Children().GetAt(1)==original,"original transform object retained after optical correction");
        Check(image.Source()==encoded&&image.Clip()==clip&&image.RenderTransformOrigin().Y==0.5f,"live image source clip and origin untouched");
        Check(!s->sampleHost&&root.Children().Size()==childrenBefore,"neutral sample removed after render");
        LeanDockIconView::Remove(ElementId::Icon);
        Check(image.RenderTransform()==original,"original transform restored after rendered measurement");
        image.Clip(nullptr);image.ClearValue(UIElement::RenderTransformProperty());
        // A real larger PNG follows the same native BitmapImage path. It must
        // retain optical sizing while being downsampled into the original
        // button, with no source replacement or extra layout space.
        Media::Imaging::WriteableBitmap detailed(256,256);
        auto fine=detailed.PixelBuffer().data();std::fill(fine,fine+256*256*4,0);
        for(int y=20;y<236;++y)for(int x=20;x<236;++x){
            auto p=fine+(y*256+x)*4;p[0]=(x%8<4)?240:20;p[1]=80;p[2]=120;p[3]=255;
        }
        detailed.Invalidate();Media::Imaging::BitmapImage high;
        bool highDone=false,highOk=false;Encode(detailed,high,highDone,highOk);
        for(int i=0;i<500&&!highDone;++i)Pump(10);
        Check(highDone&&highOk&&high.PixelWidth()==256,"high-resolution PNG retained by native decoder");
        image.Source(high);root.UpdateLayout();Pump();
        const auto highOriginalTransform=image.ReadLocalValue(UIElement::RenderTransformProperty());
        s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=5;
        LeanDockIconView::states.emplace(ElementId::Icon,s);LeanDockIconView::Measure(s);Wait(s);
        for(int attempt=0;attempt<2&&!s->applied;++attempt){Pump(50);LeanDockIconView::CancelRetry(*s);LeanDockIconView::Measure(s);Wait(s);}
        Check(bool(s->applied),"high-resolution artwork normalized");
        correction=s->applied.Children().GetAt(0).as<Media::CompositeTransform>();
        Check(std::abs(correction.ScaleX()-256.0/216)<0.015,"high-resolution optical extent remains correct");
        Check(high.PixelWidth()>image.ActualWidth()*correction.ScaleX()*image.XamlRoot().RasterizationScale(),"final transform downsamples instead of enlarging source pixels");
        Check(image.Width()==40&&image.ActualWidth()==40&&image.Source()==high,"larger source does not enlarge native layout");
        Check(!s->sampleHost&&root.Children().Size()==childrenBefore,"high-resolution sample cleaned up");
        LeanDockIconView::Remove(ElementId::Icon);
        Check(image.Source()==high&&image.ReadLocalValue(UIElement::RenderTransformProperty())==highOriginalTransform,"stop preserves native high-resolution source and original transform property");
        // A tray icon keeps one transform while frames change: no flicker at native size.
        Check(!LeanDockIconView::IsTrayIcon(image),"an ordinary image is not a tray icon");
        auto trayFrame=[&](int low,int high){
            Media::Imaging::WriteableBitmap frame(64,64);
            auto data=frame.PixelBuffer().data();std::fill(data,data+64*64*4,0);
            for(int y=low;y<high;++y)for(int x=low;x<high;++x){auto p=data+(y*64+x)*4;p[0]=128;p[3]=255;}
            frame.Invalidate();return frame;
        };
        image.Source(trayFrame(5,59));root.UpdateLayout();Pump();
        s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=6;s->tray=true;
        LeanDockIconView::states.emplace(ElementId::Icon,s);LeanDockIconView::Measure(s);Wait(s);
        Check(bool(s->applied),"tray icon normalized");
        const auto trayGroup=s->applied;const auto trayFit=s->correction;
        Check(trayFit&&std::abs(trayFit.ScaleX()-64.0/54)<0.015,"tray icon uses the Dock's scale");
        image.Source(trayFrame(8,56));LeanDockIconView::Refresh(s);
        Check(image.RenderTransform()==trayGroup,"a new frame keeps the current transform until measured");
        for(int i=0;i<40&&std::abs(trayFit.ScaleX()-64.0/48)>=0.015;++i)Pump(20);
        Check(s->applied==trayGroup&&std::abs(trayFit.ScaleX()-64.0/48)<0.015,"a different frame updates the same object");
        const double trayScale=trayFit.ScaleX();
        image.Source(trayFrame(0,0));LeanDockIconView::Refresh(s);
        for(int i=0;i<60;++i)Pump(20); // past the pace and the bounded retries
        Check(s->applied==trayGroup&&image.RenderTransform()==trayGroup&&trayFit.ScaleX()==trayScale,"a blank flash frame holds the scale");
        image.Source(trayFrame(8,57));LeanDockIconView::Refresh(s);
        for(int i=0;i<60;++i)Pump(20);
        Check(trayFit.ScaleX()==trayScale,"a pixel of noise between frames holds the scale");
        auto trayLate=s->later;
        LeanDockIconView::Remove(ElementId::Icon);
        Check(!s->applied&&!s->correction&&!s->later&&(!trayLate||!trayLate.IsEnabled()),"tray icon lifetime removed");
        image.ClearValue(UIElement::RenderTransformProperty());
        // An oversized tray source is shown through an averaged copy, then handed back.
        auto oversized=[&](int low,int high){
            Media::Imaging::WriteableBitmap frame(256,256);
            auto data=frame.PixelBuffer().data();std::fill(data,data+256*256*4,0);
            for(int y=low;y<high;++y)for(int x=low;x<high;++x){auto p=data+(y*256+x)*4;p[0]=p[3]=255;}
            frame.Invalidate();return frame;
        };
        auto big=oversized(23,233);image.Source(big);root.UpdateLayout();Pump();
        s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=7;s->tray=true;
        LeanDockIconView::states.emplace(ElementId::Icon,s);LeanDockIconView::Measure(s);Wait(s);
        const auto shown=image.Source().try_as<Media::Imaging::WriteableBitmap>();
        Check(shown&&shown!=big&&s->filtered==shown&&s->original==big,"oversized source shown through a copy");
        Check(shown&&shown.PixelWidth()<big.PixelWidth()&&shown.PixelWidth()==shown.PixelHeight(),"the copy is smaller and square");
        Check(big.PixelWidth()==256&&image.Width()==40,"the original bitmap and layout are unchanged");
        const auto bigScale=s->correction?s->correction.ScaleX():0;
        Check(std::abs(bigScale-256.0/210)<0.015,"scale comes from the original's artwork");
        LeanDockIconView::Refresh(s);for(int i=0;i<40;++i)Pump(20);
        Check(image.Source()==shown&&s->correction.ScaleX()==bigScale,"a layout refresh keeps the copy and its scale");
        auto next=oversized(40,216);image.Source(next);LeanDockIconView::Refresh(s);
        for(int i=0;i<60&&s->original!=next;++i)Pump(20);
        Check(s->original==next&&image.Source()==s->filtered&&image.Source()!=shown,"a new source replaces the copy");
        Check(std::abs(s->correction.ScaleX()-1.35)<0.001,"a new frame is measured from its own artwork, up to the cap");
        LeanDockIconView::Remove(ElementId::Icon);
        Check(image.Source()==next&&!s->filtered&&!s->original,"stop hands the native source back");
        image.ClearValue(UIElement::RenderTransformProperty());
        // Force the asynchronous, non-readable source route for cancellation.
        image.Source(Media::Imaging::BitmapImage{});
        s=std::make_shared<LeanDockIconView::State>();s->image=winrt::make_weak(image);s->taskbar=window;s->slot=3;
        LeanDockIconView::states.emplace(ElementId::Icon,s);LeanDockIconView::Measure(s);LeanDockIconView::Remove(ElementId::Icon);Wait(s);
        Check(!s->applied&&!GetPropW(window,L"LeanDock.Icons.3.Status"),"late render cannot reapply after stop");
        Check(!s->sampleHost&&root.Children().Size()==childrenBefore,"cancel removes neutral sample");
        island.Content(nullptr);island.Close();manager.Close();DestroyWindow(window);
        std::cout<<checks<<" checks passed; hidden native XAML rendering and lifetime\n";return 0;
    }catch(winrt::hresult_error const& e){std::cerr<<std::hex<<unsigned(e.code())<<" "<<winrt::to_string(e.message())<<'\n';}
    catch(std::exception const& e){std::cerr<<e.what()<<'\n';}
    LeanDockIconView::Clear();if(window)DestroyWindow(window);return 1;
}
