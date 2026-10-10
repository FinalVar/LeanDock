// Explicit, read-only snapshots for diagnosing a real desktop gesture. No
// timer/subscription, no visual writes, no caller-supplied pointers. Animated
// compositor getters may return base values: these are not pixel evidence.
#include <cwchar>
UINT LeanDockMaterialProbeMessage() {
    static const UINT message=RegisterWindowMessageW(L"LeanDock.Material.Probe.1");
    return message;
}
void ClearLeanDockMaterialProbe(HWND taskbar) noexcept {
    if(!taskbar||!GetPropW(taskbar,L"LeanDock.Probe.At"))return;
    for(auto key:{L"At",L"Ready",L"Error",L"Count",L"HostVisible",L"Theme",L"Argb",L"BrushOpacity",
        L"BackgroundCopies",L"BackgroundCopyAt",L"BackgroundCopyResult",L"AppThumbnails",L"AppThumbnailAt",L"AppThumbnailResult",
        L"BackgroundThumbnails",L"BackgroundThumbnailAt",L"BackgroundThumbnailResult"}){
        wchar_t name[128];std::swprintf(name,128,L"LeanDock.Probe.%ls",key);RemovePropW(taskbar,name);
    }
    for(unsigned i=0;i<16;++i)for(auto key:{L"Opacity",L"VisualOpacity",L"Visible",L"Loaded",L"Width",L"Height",L"Type",L"Name"}){
        wchar_t name[128];std::swprintf(name,128,L"LeanDock.Probe.Node%u.%ls",i,key);RemovePropW(taskbar,name);
    }
}
LRESULT SampleLeanDockMaterial(HWND taskbar,WPARAM protocol,LPARAM requested) noexcept {
    if(protocol!=1||!LeanDockBackdrop::Fresh(static_cast<ULONGLONG>(requested),GetTickCount64()))return 0;
    auto put=[&](const wchar_t* key,ULONG_PTR value){
        wchar_t name[128];std::swprintf(name,128,L"LeanDock.Probe.%ls",key);LeanDockBackdrop::Put(taskbar,name,value);
    };
    put(L"Ready",0);put(L"Error",0);put(L"Count",0);
    try {
        for(auto const& state:g_clickThroughTaskbarState){
            if(state.cardsClosing||!state.islandHwnd||GetAncestor(state.islandHwnd,GA_ROOT)!=taskbar)continue;
            auto layer=state.material.get();if(!layer)continue;
            put(L"HostVisible",layer.XamlRoot().IsHostVisible());
            put(L"Theme",static_cast<ULONG_PTR>(layer.ActualTheme()));
            auto brush=layer.Children().GetAt(0).as<Shapes::Rectangle>().Fill().as<Media::SolidColorBrush>();
            const auto color=brush.Color();
            put(L"Argb",(ULONG_PTR(color.A)<<24)|(ULONG_PTR(color.R)<<16)|(ULONG_PTR(color.G)<<8)|color.B);
            put(L"BrushOpacity",std::lround(brush.Opacity()*1000));
            auto hash=[](winrt::hstring const& s){uint32_t h=2166136261u;for(wchar_t c:s){h^=c;h*=16777619u;}return h;};
            DependencyObject node=layer;unsigned count=0;
            for(;node&&count<16;++count,node=Media::VisualTreeHelper::GetParent(node)){
                auto fe=node.try_as<FrameworkElement>();if(!fe)break;
                auto visual=Hosting::ElementCompositionPreview::GetElementVisual(fe);
                auto key=[&](const wchar_t* field,ULONG_PTR value){
                    const auto name=L"Node"+std::to_wstring(count)+L"."+field;put(name.c_str(),value);
                };
                key(L"Opacity",std::lround(fe.Opacity()*1000));
                key(L"VisualOpacity",std::lround(visual.Opacity()*1000));
                key(L"Visible",fe.Visibility()==Visibility::Visible&&visual.IsVisible());
                key(L"Loaded",fe.IsLoaded());
                key(L"Width",std::lround(fe.ActualWidth()*100));
                key(L"Height",std::lround(fe.ActualHeight()*100));
                key(L"Type",hash(winrt::get_class_name(fe)));key(L"Name",hash(fe.Name()));
            }
            put(L"Count",count);put(L"Ready",1);break;
        }
    }catch(...){put(L"Error",static_cast<unsigned>(winrt::to_hresult()));}
    put(L"At",static_cast<ULONG_PTR>(requested));return 1;
}
