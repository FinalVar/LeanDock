#include "shell-menu-latest.h"
#include "cards-core-latest.h"
#include "power-target-latest.h"
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.h>

winrt::weak_ref<FrameworkElement> g_nativePowerButton;
winrt::weak_ref<FrameworkElement> g_nativePowerTarget;
winrt::weak_ref<Controls::Grid> g_nativePowerGrid;
winrt::weak_ref<Controls::Button> g_nativePowerCard;
UIElement::PointerEntered_revoker g_powerEntered;
UIElement::PointerExited_revoker g_powerExited;
winrt::Windows::Foundation::IInspectable g_powerPressed{nullptr},g_powerReleased{nullptr};
UIElement::ContextRequested_revoker g_powerContext;

void ClearNativePowerHit() {
    if(g_dockUnified&&g_dockUnified->threadId!=GetCurrentThreadId())return;
    g_powerEntered.revoke();g_powerExited.revoke();
    if(auto target=g_nativePowerTarget.get()){
        if(g_powerPressed)target.RemoveHandler(UIElement::PointerPressedEvent(),g_powerPressed);
        if(g_powerReleased)target.RemoveHandler(UIElement::PointerReleasedEvent(),g_powerReleased);
    }
    g_powerPressed=nullptr;g_powerReleased=nullptr;
    g_powerContext.revoke();
    if(auto card=g_nativePowerCard.get()){VisualStateManager::GoToState(card,L"Normal",false);card.IsHitTestVisible(true);card.IsTabStop(true);}
    g_nativePowerButton={};g_nativePowerTarget={};g_nativePowerGrid={};g_nativePowerCard={};
    for(auto name:{L"LeanDock.Card.NativePowerHit",L"LeanDock.Card.NativePowerPrepared",L"LeanDock.Card.NativePowerWidth100",L"LeanDock.Card.NativePowerHeight100",L"LeanDock.Card.NativePowerIcons",L"LeanDock.Card.NativePowerLoadedIcons",L"LeanDock.Card.NativePowerTargetWidth100",L"LeanDock.Card.NativePowerTargetHeight100",L"LeanDock.Card.NativePowerCoverage",L"LeanDock.Card.NativePowerTipOwners",L"LeanDock.Card.NativePowerTipsRemaining",L"LeanDock.Card.NativePowerTipScanComplete",L"LeanDock.Card.NativePowerTipError"})
        RemovePropW(FindWindowW(L"Shell_TrayWnd",nullptr),name);
}
FrameworkElement DockFindNamed(DependencyObject const& node,const wchar_t* name,unsigned depth=0) {
    if(!node||depth>24)return nullptr;
    if(auto fe=node.try_as<FrameworkElement>();fe&&fe.Name()==name)return fe;
    const auto count=Media::VisualTreeHelper::GetChildrenCount(node);
    for(int i=0;i<count;++i)if(auto found=DockFindNamed(Media::VisualTreeHelper::GetChild(node,i),name,depth+1))return found;
    return nullptr;
}
void NativePowerState(const wchar_t* state) {
    if(auto card=g_nativePowerCard.get())VisualStateManager::GoToState(card,state,false);
}
void SuppressNativePowerToolTips(FrameworkElement const& battery,HWND taskbar) noexcept {
    if(!g_dockUnified||g_unloading||g_dockUnified->threadId!=GetCurrentThreadId())return;
    try{
        // Only the native quick-settings entry that overlays our power card.
        // Keep the existing binding-aware restore owner; never alter global tips.
        const auto property=Controls::ToolTipService::ToolTipProperty();
        std::vector<std::pair<DependencyObject,unsigned>> pending{{battery,0}};
        unsigned visited=0,remaining=0;bool complete=true;
        while(!pending.empty()&&visited<128){
            auto [node,depth]=pending.back();pending.pop_back();++visited;
            if(auto element=node.try_as<FrameworkElement>()){
                if(auto value=Controls::ToolTipService::GetToolTip(element)){
                    if(auto tip=value.try_as<Controls::ToolTip>())tip.IsOpen(false);
                    DockRemember(element,property);element.SetValue(property,nullptr);
                }
                if(Controls::ToolTipService::GetToolTip(element))++remaining;
            }
            const int count=Media::VisualTreeHelper::GetChildrenCount(node);
            if(depth>=24){if(count)complete=false;continue;}
            for(int i=0;i<count;++i){
                if(visited+pending.size()>=128){complete=false;break;}
                pending.emplace_back(Media::VisualTreeHelper::GetChild(node,i),depth+1);
            }
        }
        unsigned owners=0;
        for(auto const& saved:g_dockUnified->saved)if(saved.property==property)
            if(auto element=saved.element.get();element&&LeanDockPower::DescendsFrom(element,battery))++owners;
        LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTipOwners",owners);
        LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTipsRemaining",remaining);
        LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTipScanComplete",complete&&pending.empty());
        LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTipError",0);
    }catch(...){
        const auto error=winrt::to_hresult();
        LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTipError",static_cast<uint32_t>(error));
        Wh_Log(L"Native power tooltip suppression error %08X",error);
    }
}
void RestoreNativePowerToolTips(FrameworkElement const& battery) {
    const auto property=Controls::ToolTipService::ToolTipProperty();
    for(auto const& saved:g_dockUnified->saved)if(saved.property==property){
        auto element=saved.element.get();
        if(!element||!LeanDockPower::DescendsFrom(element,battery)||element.GetValue(property))continue;
        if(saved.binding)Data::BindingOperations::SetBinding(element,property,saved.binding);
        else if(saved.value==DependencyProperty::UnsetValue())element.ClearValue(property);
        else element.SetValue(property,saved.value);
    }
}
void RestoreNativePowerNavigation(FrameworkElement const& battery) {
    // The dormant slot is excluded from keyboard/accessibility navigation.
    // Restore the actual original values/bindings when it becomes interactive.
    for(auto property:{Controls::Control::IsTabStopProperty(),Automation::AutomationProperties::AccessibilityViewProperty()})
        for(auto const& saved:g_dockUnified->saved)if(saved.element.get()==battery&&saved.property==property){
            if(saved.binding)Data::BindingOperations::SetBinding(battery,property,saved.binding);
            else if(saved.value==DependencyProperty::UnsetValue())battery.ClearValue(property);
            else battery.SetValue(property,saved.value);
            break;
        }
}
void PrepareDormantNativePower(FrameworkElement const& battery) {
    // Keep the native input object measured across carousel changes. Collapsed
    // discarded its dimensions, leaving the first click racing the next layout.
    // The negative margin contributes zero flow width, just like the existing
    // map-failure fallback; this adds neither a tray icon nor an empty column.
    DockSet(battery,UIElement::IsHitTestVisibleProperty(),false);
    DockSet(battery,UIElement::OpacityProperty(),0.0);
    if(auto control=battery.try_as<Controls::Control>())
        DockSet(control,Controls::Control::IsTabStopProperty(),false);
    DockSet(battery,Automation::AutomationProperties::AccessibilityViewProperty(),Automation::Peers::AccessibilityView::Raw);
    DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
    DockBox(battery,LeanDockCards::CardWidth,LeanDockCards::CardHeight,HorizontalAlignment::Left,false);
    DockSet(battery,FrameworkElement::MarginProperty(),Thickness{0,0,-LeanDockCards::CardWidth,0});
    if(auto grid=FindChildByClassName(battery,L"Windows.UI.Xaml.Controls.Grid"))
        DockBox(grid,LeanDockCards::CardWidth,LeanDockCards::CardHeight);
    const bool measured=DockEqual(battery.ActualWidth(),LeanDockCards::CardWidth)&&
                        DockEqual(battery.ActualHeight(),LeanDockCards::CardHeight);
    SetPropW(FindWindowW(L"Shell_TrayWnd",nullptr),L"LeanDock.Card.NativePowerPrepared",reinterpret_cast<HANDLE>(measured?2:1));
}
bool MapNativePowerHit(FrameworkElement const& battery,HWND taskbar) {
    // In this pinned SystemTray build, ControlCenterButton receives its click
    // through IconView. OmniButton::HandleLoaded registers its own Tapped
    // forwarding only on the NotificationCenterButton branch. Stretching the
    // outer Grid therefore created dead areas around the small native icon.
    // Use the one loaded, visible IconView and retain its native event route.
    const auto nativeContent=LeanDockPower::Inspect(battery);
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerIcons",nativeContent.icons);
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerLoadedIcons",nativeContent.loaded);
    if(nativeContent.visible!=1)return false;
    auto target=nativeContent.target;
    auto content=battery.XamlRoot().Content();
    auto card=g_nativePowerCard.get();
    if(!card||!card.IsLoaded()){
        auto own=DockFindNamed(content,L"LeanDockPowerCard");
        card=own?own.try_as<Controls::Button>():nullptr;
    }
    auto grid=FindChildByClassName(battery,L"Windows.UI.Xaml.Controls.Grid");
    if(!card||!grid||!card.IsLoaded()||card.Visibility()!=Visibility::Visible)return false;
    // The shared slot is already laid out on every page. The power page itself
    // may just have left Collapsed and still report zero/stale ActualWidth.
    auto slot=FindParentByName(card,L"LeanDockCard");
    if(!slot||!slot.IsLoaded()||slot.Visibility()!=Visibility::Visible)return false;
    const double width=slot.ActualWidth(),height=slot.ActualHeight();
    if(width<24||width>LeanDockCards::CardWidth+0.5||height<24||height>80)return false;
    // Preserve the native parent, event handlers and accessibility. A zero
    // flow slot avoids restoring the old tray gap; the full native hit target
    // is translated onto the already-measured custom battery card.
    DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
    DockSet(battery,UIElement::OpacityProperty(),1.0);
    DockSet(grid,UIElement::OpacityProperty(),0.0);
    DockSet(battery,UIElement::IsHitTestVisibleProperty(),true);
    if(Automation::AutomationProperties::GetAccessibilityView(battery)==Automation::Peers::AccessibilityView::Raw)
        RestoreNativePowerNavigation(battery);
    DockBox(battery,width,height,HorizontalAlignment::Left,false);
    DockSet(battery,FrameworkElement::MarginProperty(),Thickness{0,0,-width,0});
    DockBox(grid,width,height);
    DockNormalizeIcon(target,width,height);
    // Keep every original parent. The original horizontal items panel has
    // only one visible entry; its presenters must cover the same rectangle.
    for(DependencyObject node=target;node&&node!=grid;node=Media::VisualTreeHelper::GetParent(node)){
        if(auto fe=node.try_as<FrameworkElement>()){
            DockBox(fe,width,height);
            DockSet(fe,FrameworkElement::MaxWidthProperty(),width);
            DockSet(fe,FrameworkElement::MaxHeightProperty(),height);
        }
    }
    auto targetRoot=FindChildByName(target,L"ContainerGrid");
    auto targetGrid=targetRoot?targetRoot.try_as<Controls::Grid>():nullptr;
    if(!targetGrid)return false;
    if(!targetGrid.Background())
        DockSet(targetGrid,Controls::Panel::BackgroundProperty(),Media::Brush{Media::SolidColorBrush{winrt::Windows::UI::Color{0,0,0,0}}});
    if(!DockEqual(battery.ActualWidth(),width)||!DockEqual(battery.ActualHeight(),height)||
       !DockEqual(grid.ActualWidth(),width)||!DockEqual(grid.ActualHeight(),height)||
       !DockEqual(target.ActualWidth(),width)||!DockEqual(target.ActualHeight(),height)||
       !DockEqual(targetGrid.ActualWidth(),width)||!DockEqual(targetGrid.ActualHeight(),height))return false;
    if(!grid.as<Controls::Grid>().Background())
        DockSet(grid,Controls::Panel::BackgroundProperty(),Media::Brush{Media::SolidColorBrush{winrt::Windows::UI::Color{0,0,0,0}}});
    auto desired=slot.TransformToVisual(content).TransformPoint({0,0});
    auto actual=battery.TransformToVisual(content).TransformPoint({0,0});
    auto targetPoint=target.TransformToVisual(content).TransformPoint({0,0});
    auto gridPoint=targetGrid.TransformToVisual(content).TransformPoint({0,0});
    if(g_nativePowerButton.get()==battery&&g_nativePowerTarget.get()==target&&
       g_nativePowerGrid.get()==targetGrid&&g_nativePowerCard.get()==card&&
       GetPropW(taskbar,L"LeanDock.Card.NativePowerHit")&&
       std::abs(actual.X-desired.X)<0.05&&std::abs(actual.Y-desired.Y)<0.05&&
       std::abs(targetPoint.X-desired.X)<0.05&&std::abs(targetPoint.Y-desired.Y)<0.05&&
       std::abs(gridPoint.X-desired.X)<0.05&&std::abs(gridPoint.Y-desired.Y)<0.05){
        SuppressNativePowerToolTips(battery,taskbar);return true;
    }
    auto transform=battery.RenderTransform().try_as<Media::TranslateTransform>();
    const double x=(transform?transform.X():0)+desired.X-actual.X;
    const double y=(transform?transform.Y():0)+desired.Y-actual.Y;
    DockTranslate(battery,x,y);
    card.IsHitTestVisible(false);card.IsTabStop(false);
    // Sample the interior edge, corners and center. Only the first/topmost
    // result may prove coverage; the old any-descendant center check could
    // accept an inert outer Grid or a native target covered by another view.
    unsigned coverage=0;
    for(double dy:{2.0,height/2,height-2.0})for(double dx:{2.0,width/2,width-2.0})
        if(LeanDockPower::TopHit(content,target,{float(desired.X+dx),float(desired.Y+dy)}))++coverage;
    if(coverage!=9){card.IsHitTestVisible(true);card.IsTabStop(true);RemovePropW(taskbar,L"LeanDock.Card.NativePowerHit");return false;}
    if(g_nativePowerButton.get()!=battery||g_nativePowerTarget.get()!=target||g_nativePowerGrid.get()!=targetGrid||g_nativePowerCard.get()!=card){
        ClearNativePowerHit();g_nativePowerButton=winrt::make_weak(battery);g_nativePowerTarget=winrt::make_weak(target);g_nativePowerGrid=winrt::make_weak(targetGrid);g_nativePowerCard=winrt::make_weak(card);
        card.IsHitTestVisible(false);card.IsTabStop(false);
        g_powerEntered=battery.PointerEntered(winrt::auto_revoke,[taskbar](auto&&,auto&&){
            NativePowerState(L"PointerOver");
            if(auto entry=g_nativePowerButton.get())SuppressNativePowerToolTips(entry,taskbar);
        });
        g_powerExited=battery.PointerExited(winrt::auto_revoke,[](auto&&,auto&&){NativePowerState(L"Normal");});
        g_powerPressed=winrt::box_value(Input::PointerEventHandler([](auto&&,auto&&){NativePowerState(L"Pressed");}));
        g_powerReleased=winrt::box_value(Input::PointerEventHandler([taskbar](auto&&,auto&&){
            NativePowerState(L"PointerOver");
            auto n=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Card.NativePowerClicks"));
            SetPropW(taskbar,L"LeanDock.Card.NativePowerClicks",reinterpret_cast<HANDLE>(n+1));
        }));
        target.AddHandler(UIElement::PointerPressedEvent(),g_powerPressed,true);
        target.AddHandler(UIElement::PointerReleasedEvent(),g_powerReleased,true);
        g_powerContext=target.ContextRequested(winrt::auto_revoke,[taskbar](UIElement const& sender,Input::ContextRequestedEventArgs const& e){
            winrt::Windows::Foundation::Point point{};
            if(e.Handled()||!e.TryGetPosition(sender,point))return; // keyboard stays native
            e.Handled(true);
            LeanDockMenu::Consume(taskbar,LeanDockMenu::ContextSource::NativePower);
        });
    }
    SuppressNativePowerToolTips(battery,taskbar);
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerWidth100",std::lround(battery.ActualWidth()*100));
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerHeight100",std::lround(battery.ActualHeight()*100));
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTargetWidth100",std::lround(target.ActualWidth()*100));
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerTargetHeight100",std::lround(target.ActualHeight()*100));
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerCoverage",coverage);
    SetPropW(taskbar,L"LeanDock.Card.NativePowerHit",reinterpret_cast<HANDLE>(1));
    SetPropW(taskbar,L"LeanDock.Card.NativePowerPrepared",reinterpret_cast<HANDLE>(2));
    return true;
}

// The whole carousel owns the battery entry as soon as the slot is measured.
// Only the selected power page gets a native hit target. Other pages retain a
// measured but noninteractive zero-flow slot, without an extra tray icon.
// A crowded/failed carousel restores the original compact native entry.
void UpdateUnifiedDockGrid() {
    if(!g_dockUnified||g_dockUnified->updating||g_unloading||
       g_dockUnified->threadId!=GetCurrentThreadId())return;
    auto root=g_dockUnified->root.get();
    if(!root||!root.IsLoaded()||IsVerticalSystemTray(root)||root.ActualHeight()<48||root.ActualHeight()>120)return;
    try{
        auto battery=FindChildByName(root,L"ControlCenterButton");
        auto input=FindChildByName(root,L"NonActivatableStack");
        if(!battery||!input)return;
        HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);
        const bool moved=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Card.PowerReady"))==2;
        if(!moved){
            ClearNativePowerHit();
            RestoreNativePowerNavigation(battery);
            RestoreNativePowerToolTips(battery);
            DockSet(battery,UIElement::OpacityProperty(),1.0);
            if(auto grid=FindChildByClassName(battery,L"Windows.UI.Xaml.Controls.Grid"))DockSet(grid,UIElement::OpacityProperty(),1.0);
            DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
            DockSet(battery,UIElement::IsHitTestVisibleProperty(),true);
            DockSet(input,UIElement::VisibilityProperty(),Visibility::Visible);
            UpdateLegacyUnifiedDockGrid();return;
        }
        struct Guard{bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}}guard(g_dockUnified->updating);
        auto apps=DockCollect(root,true);if(!apps.container||!apps.panel)return;
        auto system=DockCollectSystem(root);
        const bool powerActive=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Card.PowerActive"))==2;
        // A failed map retains the working Invoke/keyboard fallback on the card.
        if(!powerActive){
            ClearNativePowerHit();
            PrepareDormantNativePower(battery);
        }else if(!MapNativePowerHit(battery,taskbar)){
            ClearNativePowerHit();
            PrepareDormantNativePower(battery);
        }
        DockSet(input,UIElement::VisibilityProperty(),Visibility::Collapsed);
        const double cell=g_settings.notificationIconWidth,pitch=cell+4,row=24;
        const int appCount=int(apps.icons.size()),systemCount=int(system.icons.size());
        const int count=appCount+systemCount;
        DockArrangeGroup(apps,0,1,cell,pitch,row);
        DockArrangeGroup(system,appCount,2,cell,pitch,row);
        DockCenterFinal(apps,0,count,cell,pitch,row);
        DockCenterFinal(system,appCount,count,cell,pitch,row);
        std::wstring state=L"{\"active\":true,\"apps\":"+std::to_wstring(appCount)+
            L",\"system\":"+std::to_wstring(systemCount)+
            L",\"battery\":0,\"input\":0,\"total\":"+std::to_wstring(count)+
            L",\"columns\":"+std::to_wstring(DockGrid::TotalColumns(count))+L",\"cells\":[";
        bool first=true;for(auto const& item:apps.icons){if(!first)state+=L",";first=false;state+=DockRect(item,root);}
        for(auto const& item:system.icons){if(!first)state+=L",";first=false;state+=DockRect(item,root);}
        state+=L"],\"systemTypes\":[";first=true;
        for(auto const& item:system.icons){if(!first)state+=L",";first=false;state+=L"\""+DockSystemTypes(item)+L"\"";}
        state+=L"]}";
        if(state!=g_dockUnified->lastState){Wh_SetStringValue(L"mergedGridState",state.c_str());g_dockUnified->lastState=std::move(state);
            auto& saved=g_dockUnified->saved;saved.erase(std::remove_if(saved.begin(),saved.end(),[](auto& x){return !x.element.get();}),saved.end());}
    }catch(...){Wh_Log(L"Card tray update error %08X",winrt::to_hresult());}
}
