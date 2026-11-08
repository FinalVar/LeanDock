// Local adaptation of the upstream styler's visual-tree lifetime. Native
// Image content remains native: only its drawing transform is compensated.
namespace LeanDockIconView {
using winrt::Windows::UI::Xaml::Media::Imaging::RenderTargetBitmap;
struct State {
    winrt::weak_ref<Controls::Image> image;
    winrt::event_token loaded{}, opened{}, sized{}, retryFrame{};
    int64_t sourceToken{}, transformToken{};
    bool sourceRegistered{}, transformRegistered{}, active{true}, writing{}, queued{}, busy{};
    uint64_t revision{}, slot{};
    unsigned retries{};
    HWND taskbar{};
    Media::TransformGroup applied{nullptr};
    Media::CompositeTransform correction{nullptr};
    LeanDockIcons::Fit current{};
    bool tray{};
    uint64_t measuredAt{};
    DispatcherTimer later{nullptr};
    Media::ImageSource original{nullptr},filtered{nullptr};
    DispatcherTimer watch{nullptr};uint64_t shownFrom{};
    Controls::Canvas sampleHost{nullptr};
    winrt::weak_ref<Controls::Panel> sampleParent;
    winrt::Windows::Foundation::IInspectable originalLocal{nullptr};
    winrt::Windows::Foundation::IAsyncAction render{nullptr};
};
thread_local std::unordered_map<ElementId,std::shared_ptr<State>> states;
thread_local uint64_t nextSlot{};
constexpr const wchar_t* properties[]={L"Status",L"Scale",L"Left",L"Top",L"Right",L"Bottom",L"Source",L"Error",L"PixelsWidth",L"PixelsHeight",L"OffsetX",L"OffsetY",L"SourceWidth",L"SourceHeight",L"DecodeWidth",L"DecodeHeight",L"DisplayWidth100",L"DisplayHeight100",L"Tray",L"CopyWidth"};
// Tray icons can animate. Two measures a second are enough for a steady size.
constexpr uint64_t TrayPace=500;
std::wstring Property(State const& s,const wchar_t* name){
    return L"LeanDock.Icons."+std::to_wstring(s.slot)+L"."+name;
}
void Put(State const& s,const wchar_t* key,ULONG_PTR value) noexcept {
    try{if(s.taskbar)SetPropW(s.taskbar,Property(s,key).c_str(),reinterpret_cast<HANDLE>(value+1));}catch(...){}
}
bool IsAppIcon(Controls::Image const& image) {
    if(image.Name()!=L"Icon")return false;
    DependencyObject parent=image;
    for(int i=0;i<16&&parent;++i){
        const auto type=winrt::get_class_name(parent);
        if(type==L"Taskbar.TaskListButton"||type==L"Taskbar.TaskListButtonPanel"||type==L"Taskbar.TaskListLabeledButtonPanel")return true;
        if(std::wstring_view(type).starts_with(L"SystemTray."))return false;
        parent=Media::VisualTreeHelper::GetParent(parent);
    }
    return false;
}
// An application's notification icon. Text and language content have no artwork
// to measure, so those stay native.
bool IsTrayIcon(Controls::Image const& image) {
    DependencyObject parent=Media::VisualTreeHelper::GetParent(image);
    for(int i=0;i<8&&parent;++i){
        const auto type=winrt::get_class_name(parent);
        if(type==L"SystemTray.ImageIconContent")return true;
        if(type==L"SystemTray.NotifyIconView"||std::wstring_view(type).starts_with(L"Taskbar."))return false;
        parent=Media::VisualTreeHelper::GetParent(parent);
    }
    return false;
}
void Aim(Media::CompositeTransform const& correction,LeanDockIcons::Fit const& fit,double width,double height,winrt::Windows::Foundation::Point origin) {
    correction.CenterX((0.5-origin.X)*width);correction.CenterY((0.5-origin.Y)*height);
    correction.ScaleX(fit.scale);correction.ScaleY(fit.scale);
    correction.TranslateX(fit.x*width);correction.TranslateY(fit.y*height);
}
void Restore(State& s) noexcept {
    try{if(auto image=s.image.get();image&&s.applied&&image.RenderTransform()==s.applied){
        s.writing=true;
        if(s.originalLocal==DependencyProperty::UnsetValue())image.ClearValue(UIElement::RenderTransformProperty());
        else image.SetValue(UIElement::RenderTransformProperty(),s.originalLocal);
    }}catch(...){}
    s.writing=false;s.applied=nullptr;s.correction=nullptr;s.originalLocal=nullptr;
}
void RestoreSource(State& s) noexcept {
    try{if(auto image=s.image.get();image&&s.filtered&&image.Source()==s.filtered){
        s.writing=true;image.Source(s.original);
    }}catch(...){}
    s.writing=false;s.filtered=nullptr;s.original=nullptr;
}
// A source far larger than the display turns rounded corners into stairs, so show
// an averaged copy. Only with a one-way compiled binding: its next value simply
// replaces the copy, while a local value would end a {Binding} for good.
void ShowCopy(State& s,Controls::Image const& image,Media::Imaging::WriteableBitmap const& bitmap,double display) {
    if(image.GetBindingExpression(Controls::Image::SourceProperty()))return;
    const int width=bitmap.PixelWidth(),height=bitmap.PixelHeight(),longest=std::max(width,height);
    const int size=LeanDockIcons::OversizedCopy(longest,display);
    if(!size)return;
    const int toWidth=std::max(1,int(std::lround(double(width)*size/longest)));
    const int toHeight=std::max(1,int(std::lround(double(height)*size/longest)));
    auto buffer=bitmap.PixelBuffer();
    const auto pixels=LeanDockIcons::Shrink({buffer.data(),buffer.Length()},width,height,toWidth,toHeight);
    if(pixels.empty())return;
    Media::Imaging::WriteableBitmap copy(toWidth,toHeight);
    std::copy(pixels.begin(),pixels.end(),copy.PixelBuffer().data());
    copy.Invalidate();
    s.shownFrom=LeanDockIcons::Fingerprint({buffer.data(),buffer.Length()});
    s.original=bitmap;s.filtered=copy;s.writing=true;
    image.Source(copy);
    s.writing=false;Put(s,L"CopyWidth",toWidth);
}
void Queue(std::shared_ptr<State> const& s) noexcept;
void Later(std::shared_ptr<State> const& s,uint64_t delay) {
    if(!s->later){
        std::weak_ptr<State> weak=s;
        s->later=DispatcherTimer();
        s->later.Tick([weak](auto const&,auto const&) noexcept {
            if(auto s=weak.lock()){
                try{s->later.Stop();}catch(...){}
                Queue(s);
            }
        });
    }
    s->later.Interval(std::chrono::milliseconds(delay));s->later.Start();
}
// Explorer could draw a new frame into the same bitmap without setting Source
// again, and the copy would go stale. While a copy is shown, look at the
// original now and then and start over when its pixels changed.
constexpr int64_t CopyCheck=2000;
void Refresh(std::shared_ptr<State> const& s) noexcept;
void Watch(std::shared_ptr<State> const& s) {
    if(!s->watch){
        std::weak_ptr<State> weak=s;
        s->watch=DispatcherTimer();s->watch.Interval(std::chrono::milliseconds(CopyCheck));
        s->watch.Tick([weak](auto const&,auto const&) noexcept {
            auto s=weak.lock();if(!s)return;
            try{
                auto image=s->image.get();
                auto original=s->original.try_as<Media::Imaging::WriteableBitmap>();
                if(!s->active||!image||!original||!s->filtered||image.Source()!=s->filtered){s->watch.Stop();return;}
                auto buffer=original.PixelBuffer();
                if(LeanDockIcons::Fingerprint({buffer.data(),buffer.Length()})==s->shownFrom)return;
                s->watch.Stop();RestoreSource(*s);Refresh(s);
            }catch(...){try{s->watch.Stop();}catch(...){}}
        });
    }
    s->watch.Start();
}
void ClearSample(State& s) noexcept {
    try{if(s.sampleHost){
        if(auto parent=s.sampleParent.get()){
            uint32_t index{};
            if(parent.Children().IndexOf(s.sampleHost,index))parent.Children().RemoveAt(index);
        }
        s.sampleHost.Children().Clear();
    }}catch(...){}
    s.sampleHost=nullptr;s.sampleParent={};
}
Controls::Image SampleSource(State& s,Controls::Image const& image) {
    // RenderTargetBitmap of the live taskbar image can contain native visual
    // offsets/cropping. Render its same source in a private neutral Image.
    // A zero-size offscreen parent keeps this connected to the existing
    // XAML tree without painting, taking input or affecting taskbar layout.
    if(s.sampleHost&&s.sampleHost.Children().Size()==1){
        if(auto sample=s.sampleHost.Children().GetAt(0).try_as<Controls::Image>();sample&&sample.Source()==image.Source())return sample;
    }
    ClearSample(s);
    Controls::Panel parent{nullptr};
    for(DependencyObject node=Media::VisualTreeHelper::GetParent(image);node;
        node=Media::VisualTreeHelper::GetParent(node))
        if(auto panel=node.try_as<Controls::Panel>())parent=panel;
    if(!parent)winrt::throw_hresult(E_NOINTERFACE);
    Controls::Image sample;sample.Width(128);sample.Height(128);
    sample.Stretch(Media::Stretch::Uniform);sample.Source(image.Source());
    Controls::Canvas host;host.Width(0);host.Height(0);host.IsHitTestVisible(false);
    Media::TranslateTransform offscreen;offscreen.X(-image.XamlRoot().Size().Width-1024);host.RenderTransform(offscreen);
    host.HorizontalAlignment(HorizontalAlignment::Left);host.VerticalAlignment(VerticalAlignment::Top);
    host.Children().Append(sample);s.sampleHost=host;s.sampleParent=winrt::make_weak(parent);
    parent.Children().Append(host);host.UpdateLayout();
    sample.Measure({128,128});sample.Arrange({0,0,128,128});
    return sample;
}
void CancelRetry(State& s) noexcept {
    try{if(s.retryFrame.value)Media::CompositionTarget::Rendering(s.retryFrame);}catch(...){}
    s.retryFrame={};
}
void RetryAfterFrame(std::shared_ptr<State> const& s,uint64_t revision) noexcept {
    if(!s->active||s->revision!=revision||s->retryFrame.value||s->retries>=2)return;
    try{
        ++s->retries;std::weak_ptr<State> weak=s;
        s->retryFrame=Media::CompositionTarget::Rendering([weak,revision](auto const&,auto const&) noexcept {
            if(auto s=weak.lock()){
                CancelRetry(*s);
                if(s->active&&s->revision==revision)Queue(s);
            }
        });
    }catch(...){}
}
winrt::fire_and_forget Measure(std::shared_ptr<State> s) {
    const auto revision=s->revision;
    bool retry=false;
    s->busy=true;
    try{
        auto image=s->image.get();
        if(s->active&&image&&image.IsLoaded()&&image.Source()&&
           image.ActualWidth()>1&&image.ActualHeight()>1&&image.Visibility()==Visibility::Visible&&
           std::abs(image.ActualWidth()-image.ActualHeight())<1&&image.Stretch()==Media::Stretch::Uniform){
            for(auto& host:g_clickThroughTaskbarState){
                auto frame=host.taskbarFrame.get();
                if(frame&&frame.XamlRoot()==image.XamlRoot()&&host.islandHwnd){
                    s->taskbar=GetAncestor(host.islandHwnd,GA_ROOT);break;
                }
            }
            // Do not replace a caller's bound transform. Independent native
            // transform objects are retained in the group below.
            Put(*s,L"Status",4);
            if(!image.GetBindingExpression(UIElement::RenderTransformProperty())){
                const auto source=image.Source();
                const double width=image.ActualWidth(),height=image.ActualHeight();
                // Our own copy is already measured: its original's numbers stay.
                const bool ownCopy=s->filtered&&source==s->filtered;
                if(auto bitmap=source.try_as<Media::Imaging::BitmapSource>();bitmap&&!ownCopy){
                    Put(*s,L"SourceWidth",bitmap.PixelWidth());Put(*s,L"SourceHeight",bitmap.PixelHeight());
                }
                if(auto bitmap=source.try_as<Media::Imaging::BitmapImage>()){
                    Put(*s,L"DecodeWidth",bitmap.DecodePixelWidth());Put(*s,L"DecodeHeight",bitmap.DecodePixelHeight());
                }
                LeanDockIcons::Fit fit;
                if(ownCopy)fit=s->current;
                else if(auto bitmap=source.try_as<Media::Imaging::WriteableBitmap>()){
                    Put(*s,L"Source",1);
                    auto buffer=bitmap.PixelBuffer();
                    Put(*s,L"PixelsWidth",bitmap.PixelWidth());Put(*s,L"PixelsHeight",bitmap.PixelHeight());
                    fit=LeanDockIcons::Measure({buffer.data(),buffer.Length()},bitmap.PixelWidth(),bitmap.PixelHeight());
                }else{
                    Put(*s,L"Source",source.try_as<Media::Imaging::BitmapImage>()?2:3);
                    RenderTargetBitmap rendered;
                    const int pixelsW=256,pixelsH=256;
                    auto sample=SampleSource(*s,image);
                    if(s->active&&s->revision==revision&&image.IsLoaded()&&image.Source()==source){
                        s->render=rendered.RenderAsync(sample,pixelsW,pixelsH);
                        co_await s->render;
                        auto buffer=co_await rendered.GetPixelsAsync();
                        if(s->active&&s->revision==revision&&image.IsLoaded()&&image.Source()==source){
                            Put(*s,L"PixelsWidth",rendered.PixelWidth());Put(*s,L"PixelsHeight",rendered.PixelHeight());
                            fit=LeanDockIcons::Measure({buffer.data(),buffer.Length()},rendered.PixelWidth(),rendered.PixelHeight());
                        }
                    }
                    retry=!fit.valid;
                }
                if(s->active&&s->revision==revision&&image.IsLoaded()&&image.Source()==source){
                        Put(*s,L"Status",fit.valid?1:3);
                        Put(*s,L"Tray",s->tray);
                        // A tray icon holds its scale on a blank frame (flashing)
                        // and through small differences between frames.
                        const bool owned=s->applied&&s->correction&&image.RenderTransform()==s->applied;
                        if(fit.valid&&owned&&LeanDockIcons::SameFit(s->current,fit))fit=s->current;
                        if(fit.valid){
                            const double raster=image.XamlRoot().RasterizationScale();
                            Put(*s,L"DisplayWidth100",std::lround(width*fit.scale*raster*100));
                            Put(*s,L"DisplayHeight100",std::lround(height*fit.scale*raster*100));
                            Put(*s,L"Scale",std::lround(fit.scale*10000));
                            Put(*s,L"OffsetX",static_cast<ULONG_PTR>(static_cast<LONG_PTR>(std::lround(fit.x*width*10000))));
                            Put(*s,L"OffsetY",static_cast<ULONG_PTR>(static_cast<LONG_PTR>(std::lround(fit.y*height*10000))));
                            Put(*s,L"Left",fit.left);Put(*s,L"Top",fit.top);
                            Put(*s,L"Right",fit.right);Put(*s,L"Bottom",fit.bottom);
                            const auto origin=image.RenderTransformOrigin();
                            if(owned){
                                // New numbers on the same object: no property change, no flicker.
                                Aim(s->correction,fit,width,height,origin);
                                s->current=fit;Put(*s,L"Status",2);
                            }else if(fit.scale!=1||fit.x!=0||fit.y!=0){
                                auto previous=image.RenderTransform();
                                s->originalLocal=image.ReadLocalValue(UIElement::RenderTransformProperty());
                                Media::CompositeTransform correction;
                                Aim(correction,fit,width,height,origin);
                                Media::TransformGroup group;
                                // Normalize artwork first, then preserve native animation/translation.
                                group.Children().Append(correction);
                                if(previous)group.Children().Append(previous);
                                s->applied=group;s->correction=correction;s->current=fit;s->writing=true;
                                image.RenderTransform(group);
                                s->writing=false;Put(*s,L"Status",2);
                            }
                            s->current=fit;
                            if(s->tray&&!ownCopy)if(auto bitmap=source.try_as<Media::Imaging::WriteableBitmap>()){
                                ShowCopy(*s,image,bitmap,width*raster*fit.scale);
                                if(s->filtered&&image.Source()==s->filtered)Watch(s);
                            }
                        }
                }
            }
        }
    }catch(...){s->writing=false;retry=true;if(s->active&&s->revision==revision){Put(*s,L"Status",3);Put(*s,L"Error",static_cast<ULONG_PTR>(winrt::to_hresult()));}}
    // Keep a newly attached sample through the bounded next-frame retry; its
    // BitmapImage surface may not exist yet on the first RenderAsync call.
    if(!retry||!s->active||s->revision!=revision||s->retries>=2)ClearSample(*s);
    s->render=nullptr;s->busy=false;
    if(s->active&&s->revision!=revision)Queue(s);
    else if(retry)RetryAfterFrame(s,revision);
}
void Queue(std::shared_ptr<State> const& s) noexcept {
    if(!s->active||s->queued||s->busy)return;
    try{
        auto image=s->image.get();if(!image)return;
        s->queued=true;
        std::weak_ptr<State> weak=s;
        image.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low,[weak]() noexcept {
            if(auto s=weak.lock()){
                s->queued=false;
                try{if(s->active&&!s->busy)if(auto image=s->image.get();image&&(s->tray||IsAppIcon(image))){
                    if(s->tray){
                        const auto now=GetTickCount64();
                        if(now-s->measuredAt<TrayPace){Later(s,TrayPace-(now-s->measuredAt));return;}
                        s->measuredAt=now;
                    }
                    Measure(s);
                }}catch(...){Put(*s,L"Status",3);}
            }
        });
    }catch(...){s->queued=false;Put(*s,L"Status",3);}
}
void Refresh(std::shared_ptr<State> const& s) noexcept {
    if(!s->active||s->writing)return;
    ++s->revision;CancelRetry(*s);s->retries=0;ClearSample(*s);
    // A tray icon keeps its scale until the new frame is measured; going back to
    // native size in between would flicker on every flash.
    if(!s->tray)Restore(*s);
    Queue(s);
}
void Remove(ElementId id) noexcept {
    auto it=states.find(id);if(it==states.end())return;
    auto s=it->second;states.erase(it);s->active=false;++s->revision;
    CancelRetry(*s);
    try{if(s->later)s->later.Stop();}catch(...){}
    s->later=nullptr;
    try{if(s->watch)s->watch.Stop();}catch(...){}
    s->watch=nullptr;
    // The backend is process-pinned; late async completion is inert. Cancel
    // first, detach callbacks, then restore only a transform still owned by us.
    try{if(s->render)s->render.Cancel();}catch(...){}
    ClearSample(*s);
    if(auto image=s->image.get()){
        try{if(s->loaded.value)image.Loaded(s->loaded);}catch(...){}
        try{if(s->opened.value)image.ImageOpened(s->opened);}catch(...){}
        try{if(s->sized.value)image.SizeChanged(s->sized);}catch(...){}
        try{if(s->sourceRegistered)image.UnregisterPropertyChangedCallback(Controls::Image::SourceProperty(),s->sourceToken);}catch(...){}
        try{if(s->transformRegistered)image.UnregisterPropertyChangedCallback(UIElement::RenderTransformProperty(),s->transformToken);}catch(...){}
    }
    Restore(*s);RestoreSource(*s);
    try{for(auto key:properties)if(s->taskbar)RemovePropW(s->taskbar,Property(*s,key).c_str());}catch(...){}
}
void Attach(ElementId id,FrameworkElement const& element) noexcept {
    try{
        auto image=element.try_as<Controls::Image>();
        if(!image||states.contains(id))return;
        const bool tray=IsTrayIcon(image);
        if(!tray&&image.Name()!=L"Icon")return;
        auto s=std::make_shared<State>();s->image=winrt::make_weak(image);s->slot=++nextSlot;s->tray=tray;
        states.emplace(id,s);std::weak_ptr<State> weak=s;
        auto refresh=[weak](auto const&,auto const&) noexcept {if(auto s=weak.lock())Refresh(s);};
        s->loaded=image.Loaded(refresh);s->opened=image.ImageOpened(refresh);s->sized=image.SizeChanged(refresh);
        s->sourceToken=image.RegisterPropertyChangedCallback(Controls::Image::SourceProperty(),refresh);s->sourceRegistered=true;
        s->transformToken=image.RegisterPropertyChangedCallback(UIElement::RenderTransformProperty(),refresh);s->transformRegistered=true;
        Queue(s);
    }catch(...){Remove(id);}
}
void Clear() noexcept {while(!states.empty())Remove(states.begin()->first);}
}
