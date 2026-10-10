// A normal XAML pigment layer keeps contrast independent of backdrop sampling.
// WindhawkBlur still owns blur/saturation; NextKDE's LiquidGlassSurface is the
// design reference for separating the surface body from compositor effects.
// This Windows adapter owns one decorative sibling of BackgroundControl.
// Keeping it INSIDE TaskbarBackground still submits it through the native
// background subtree. Match geometry, not that subtree's composition lifetime.
#include "material-probe-latest.inc"

void ClearLeanDockMaterial(ClickThroughTaskbarState& state) noexcept {
    if(state.islandHwnd)ClearLeanDockMaterialProbe(GetAncestor(state.islandHwnd,GA_ROOT));
    try {
        auto host=state.materialHost.get();auto parent=state.materialParent.get();
        if(host&&parent){
            uint32_t index=0;
            if(parent.Children().IndexOf(host,index))parent.Children().RemoveAt(index);
        }
    }catch(...){}
    state.material={};state.materialHost={};state.materialParent={};state.materialSource={};state.materialFailed=false;
    if(state.islandHwnd)for(auto key:{L"LeanDock.Material.Ready",L"LeanDock.Material.Theme",
        L"LeanDock.Material.Color",L"LeanDock.Material.Error"})
        RemovePropW(GetAncestor(state.islandHwnd,GA_ROOT),key);
}

void UpdateLeanDockMaterial(ClickThroughTaskbarState& state,Controls::Grid const& root) noexcept {
    const HWND taskbar=state.islandHwnd?GetAncestor(state.islandHwnd,GA_ROOT):nullptr;
    try {
        if(state.cardsClosing||state.materialFailed||!root||!root.IsLoaded())return;
        auto layer=state.material.get();auto parent=state.materialParent.get();
        auto host=state.materialHost.get();
        auto source=state.materialSource.get();
        auto sourceParent=source?Media::VisualTreeHelper::GetParent(source).try_as<Controls::Grid>():nullptr;
        auto background=sourceParent?Media::VisualTreeHelper::GetParent(sourceParent).try_as<FrameworkElement>():nullptr;
        if(!layer||!host||!source||!parent||parent!=root||!source.IsLoaded()||source.XamlRoot()!=root.XamlRoot()||
           !background||Media::VisualTreeHelper::GetParent(background)!=root||
           Media::VisualTreeHelper::GetParent(layer)!=host||Media::VisualTreeHelper::GetParent(host)!=parent){
            ClearLeanDockMaterial(state);
            // Resolve only the known template, never scan application visuals.
            background=nullptr;
            for(auto const& child:root.Children())if(auto fe=child.try_as<FrameworkElement>();
                fe&&fe.Name()==L"BackgroundControl"){background=fe;break;}
            if(!background||Media::VisualTreeHelper::GetChildrenCount(background)!=1)return;
            sourceParent=Media::VisualTreeHelper::GetChild(background,0).try_as<Controls::Grid>();
            if(!sourceParent)return;
            source=nullptr;uint32_t insertAt=0;
            for(auto const& child:sourceParent.Children()){
                auto rectangle=child.try_as<Shapes::Rectangle>();
                if(rectangle&&rectangle.Name()==L"BackgroundFill"){source=rectangle;break;}
            }
            if(!source||!root.Children().IndexOf(background,insertAt))return;
            parent=root;++insertAt;
            layer=Markup::XamlReader::Load(LR"xaml(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
 x:Name="LeanDockMaterial" IsHitTestVisible="False">
 <Grid.Resources>
  <ResourceDictionary><ResourceDictionary.ThemeDictionaries>
   <ResourceDictionary x:Key="Default"><SolidColorBrush x:Key="LeanDockMaterialBrush" Color="#B3252C38"/></ResourceDictionary>
   <ResourceDictionary x:Key="Light"><SolidColorBrush x:Key="LeanDockMaterialBrush" Color="#9EC8D1DE"/></ResourceDictionary>
   <ResourceDictionary x:Key="HighContrast"><SolidColorBrush x:Key="LeanDockMaterialBrush" Color="{ThemeResource SystemColorWindowColor}"/></ResourceDictionary>
  </ResourceDictionary.ThemeDictionaries></ResourceDictionary>
 </Grid.Resources>
 <Rectangle Fill="{ThemeResource LeanDockMaterialBrush}"/>
</Grid>)xaml").as<Controls::Grid>();
            auto pigment=layer.Children().GetAt(0).as<Shapes::Rectangle>();
            // Copy the native control's measured geometry and our Dock scale,
            // but do not inherit its opacity, visibility or composition visual.
            // A Canvas contributes no desired size. Otherwise an ActualWidth
            // binding on a direct Grid child would hold the previous width and
            // prevent the Dock from shrinking when applications close.
            host=Controls::Canvas();host.Name(L"LeanDockMaterialHost");host.IsHitTestVisible(false);
            auto bind=[&](FrameworkElement const& target,DependencyProperty const& property,
                          FrameworkElement const& from,const wchar_t* path){
                Data::Binding binding;binding.Source(from);binding.Path(PropertyPath(path));
                binding.Mode(Data::BindingMode::OneWay);target.SetBinding(property,binding);
            };
            bind(layer,FrameworkElement::MarginProperty(),background,L"Margin");
            bind(layer,FrameworkElement::HorizontalAlignmentProperty(),background,L"HorizontalAlignment");
            bind(layer,FrameworkElement::VerticalAlignmentProperty(),background,L"VerticalAlignment");
            bind(layer,UIElement::RenderTransformProperty(),background,L"RenderTransform");
            bind(layer,UIElement::RenderTransformOriginProperty(),background,L"RenderTransformOrigin");
            bind(pigment,Shapes::Rectangle::RadiusXProperty(),source,L"RadiusX");
            bind(pigment,Shapes::Rectangle::RadiusYProperty(),source,L"RadiusY");
            Controls::Grid::SetRow(host,Controls::Grid::GetRow(background));
            Controls::Grid::SetColumn(host,Controls::Grid::GetColumn(background));
            Controls::Grid::SetRowSpan(host,Controls::Grid::GetRowSpan(background));
            Controls::Grid::SetColumnSpan(host,Controls::Grid::GetColumnSpan(background));
            Controls::Canvas::SetZIndex(host,Controls::Canvas::GetZIndex(background));
            // Save weak ownership first, since inserting can reenter layout.
            state.material=winrt::make_weak(layer);state.materialParent=winrt::make_weak(parent);
            state.materialHost=winrt::make_weak(host);state.materialSource=winrt::make_weak(source);
            host.Children().Append(layer);parent.Children().InsertAt(insertAt,host);
        }
        // ActualWidth/Height don't provide ordinary Binding updates reliably
        // in this XAML island. Reuse the existing layout callback, changing
        // only our element and only when the measured native size changes.
        if(layer.Width()!=background.ActualWidth())layer.Width(background.ActualWidth());
        if(layer.Height()!=background.ActualHeight())layer.Height(background.ActualHeight());
        auto putChanged=[&](const wchar_t* key,ULONG_PTR value){
            if(reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,key))!=value+1)
                LeanDockBackdrop::Put(taskbar,key,value);
        };
        putChanged(L"LeanDock.Material.Ready",layer.IsLoaded()&&layer.ActualWidth()>0&&layer.ActualHeight()>0);
        putChanged(L"LeanDock.Material.Theme",static_cast<ULONG_PTR>(layer.ActualTheme()));
        if(auto brush=layer.Children().GetAt(0).as<Shapes::Rectangle>().Fill().try_as<Media::SolidColorBrush>()){
            const auto color=brush.Color();
            putChanged(L"LeanDock.Material.Color",(ULONG_PTR(color.A)<<24)|(ULONG_PTR(color.R)<<16)|(ULONG_PTR(color.G)<<8)|color.B);
        }
    }catch(...){
        const auto error=winrt::to_hresult();
        ClearLeanDockMaterial(state);state.materialFailed=true;
        LeanDockBackdrop::Put(taskbar,L"LeanDock.Material.Error",static_cast<unsigned>(error));
    }
}
