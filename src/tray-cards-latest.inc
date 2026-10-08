#include "shell-menu-latest.h"
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.h>

winrt::weak_ref<FrameworkElement> g_nativePowerButton;
winrt::weak_ref<Controls::Button> g_nativePowerCard;
UIElement::PointerEntered_revoker g_powerEntered;
UIElement::PointerExited_revoker g_powerExited;
UIElement::PointerPressed_revoker g_powerPressed;
UIElement::PointerReleased_revoker g_powerReleased;
UIElement::RightTapped_revoker g_powerContext;

void ClearNativePowerHit() {
    if(g_dockUnified&&g_dockUnified->threadId!=GetCurrentThreadId())return;
    g_powerEntered.revoke();g_powerExited.revoke();g_powerPressed.revoke();g_powerReleased.revoke();
    g_powerContext.revoke();
    if(auto card=g_nativePowerCard.get()){VisualStateManager::GoToState(card,L"Normal",false);card.IsHitTestVisible(true);card.IsTabStop(true);}
    g_nativePowerButton={};g_nativePowerCard={};
    for(auto name:{L"LeanDock.Card.NativePowerHit",L"LeanDock.Card.NativePowerWidth100",L"LeanDock.Card.NativePowerHeight100"})
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
bool MapNativePowerHit(FrameworkElement const& battery,HWND taskbar) {
    auto content=battery.XamlRoot().Content();
    auto card=g_nativePowerCard.get();
    if(!card||!card.IsLoaded()){
        auto own=DockFindNamed(content,L"LeanDockPowerCard");
        card=own?own.try_as<Controls::Button>():nullptr;
    }
    auto grid=FindChildByClassName(battery,L"Windows.UI.Xaml.Controls.Grid");
    if(!card||!grid||!card.IsLoaded()||card.Visibility()!=Visibility::Visible)return false;
    const double width=card.ActualWidth(),height=card.ActualHeight();
    if(width<24||width>160.5||height<24||height>80)return false;
    // Preserve the native parent, event handlers and accessibility. A zero
    // flow slot avoids restoring the old tray gap; the full native hit target
    // is translated onto the already-measured custom battery card.
    DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
    DockSet(battery,UIElement::OpacityProperty(),1.0);
    DockSet(grid,UIElement::OpacityProperty(),0.0);
    DockSet(battery,UIElement::IsHitTestVisibleProperty(),true);
    DockBox(battery,width,height,HorizontalAlignment::Left,false);
    DockSet(battery,FrameworkElement::MarginProperty(),Thickness{0,0,-width,0});
    DockBox(grid,width,height);
    if(!grid.as<Controls::Grid>().Background())
        DockSet(grid,Controls::Panel::BackgroundProperty(),Media::Brush{Media::SolidColorBrush{winrt::Windows::UI::Color{0,0,0,0}}});
    auto desired=card.TransformToVisual(content).TransformPoint({0,0});
    auto actual=battery.TransformToVisual(content).TransformPoint({0,0});
    if(g_nativePowerButton.get()==battery&&GetPropW(taskbar,L"LeanDock.Card.NativePowerHit")&&
       DockEqual(battery.ActualWidth(),width)&&DockEqual(battery.ActualHeight(),height)&&
       std::abs(actual.X-desired.X)<0.05&&std::abs(actual.Y-desired.Y)<0.05)return true;
    auto transform=battery.RenderTransform().try_as<Media::TranslateTransform>();
    const double x=(transform?transform.X():0)+desired.X-actual.X;
    const double y=(transform?transform.Y():0)+desired.Y-actual.Y;
    DockTranslate(battery,x,y);
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerWidth100",std::lround(battery.ActualWidth()*100));
    LeanDockMenu::Put(taskbar,L"LeanDock.Card.NativePowerHeight100",std::lround(battery.ActualHeight()*100));
    card.IsHitTestVisible(false);card.IsTabStop(false);
    bool found=false;
    const auto hits=Media::VisualTreeHelper::FindElementsInHostCoordinates(
        winrt::Windows::Foundation::Point{float(desired.X+width/2),float(desired.Y+height/2)},content,false);
    for(auto const& hit:hits){
        DependencyObject node=hit;
        for(unsigned depth=0;node&&depth<32;++depth,node=Media::VisualTreeHelper::GetParent(node))
            if(node==battery){found=true;break;}
        if(found)break;
    }
    if(!found){card.IsHitTestVisible(true);card.IsTabStop(true);RemovePropW(taskbar,L"LeanDock.Card.NativePowerHit");return false;}
    if(g_nativePowerButton.get()!=battery||g_nativePowerCard.get()!=card){
        ClearNativePowerHit();g_nativePowerButton=winrt::make_weak(battery);g_nativePowerCard=winrt::make_weak(card);
        card.IsHitTestVisible(false);card.IsTabStop(false);
        g_powerEntered=battery.PointerEntered(winrt::auto_revoke,[](auto&&,auto&&){NativePowerState(L"PointerOver");});
        g_powerExited=battery.PointerExited(winrt::auto_revoke,[](auto&&,auto&&){NativePowerState(L"Normal");});
        g_powerPressed=battery.PointerPressed(winrt::auto_revoke,[](auto&&,auto&&){NativePowerState(L"Pressed");});
        g_powerReleased=battery.PointerReleased(winrt::auto_revoke,[taskbar](auto&&,auto&&){
            NativePowerState(L"PointerOver");
            auto n=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Card.NativePowerClicks"));
            SetPropW(taskbar,L"LeanDock.Card.NativePowerClicks",reinterpret_cast<HANDLE>(n+1));
        });
        g_powerContext=battery.RightTapped(winrt::auto_revoke,[taskbar](auto&&,Input::RightTappedRoutedEventArgs const& e){if(LeanDockMenu::Request(taskbar))e.Handled(true);});
    }
    SetPropW(taskbar,L"LeanDock.Card.NativePowerHit",reinterpret_cast<HANDLE>(1));
    return true;
}

// The whole carousel owns the battery entry as soon as the slot is measured.
// Only the selected power page gets a native hit target. Other pages remove
// that target immediately, without bringing back an extra tray battery icon.
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
            DockSet(battery,UIElement::OpacityProperty(),1.0);
            if(auto grid=FindChildByClassName(battery,L"Windows.UI.Xaml.Controls.Grid"))DockSet(grid,UIElement::OpacityProperty(),1.0);
            DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
            DockSet(battery,UIElement::IsHitTestVisibleProperty(),true);
            DockSet(input,UIElement::VisibilityProperty(),Visibility::Visible);
            UpdateLegacyUnifiedDockGrid();return;
        }
        struct Guard{bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}}guard(g_dockUnified->updating);
        auto apps=DockCollect(root,true);if(!apps.container||!apps.panel)return;
        const bool powerActive=reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Card.PowerActive"))==2;
        // A failed map retains the working Invoke/keyboard fallback on the card.
        if(!powerActive){
            ClearNativePowerHit();
            DockSet(battery,UIElement::IsHitTestVisibleProperty(),false);
            DockSet(battery,UIElement::VisibilityProperty(),Visibility::Collapsed);
        }else if(!MapNativePowerHit(battery,taskbar)){
            ClearNativePowerHit();
            // A collapsed native button cannot acquire its new measured
            // bounds. Keep a transparent, non-interactive zero-flow slot
            // until the next layout can validate the actual hit target.
            DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
            DockSet(battery,UIElement::OpacityProperty(),0.0);
            DockSet(battery,UIElement::IsHitTestVisibleProperty(),false);
            DockBox(battery,160,44,HorizontalAlignment::Left,false);
            DockSet(battery,FrameworkElement::MarginProperty(),Thickness{0,0,-160,0});
        }
        DockSet(input,UIElement::VisibilityProperty(),Visibility::Collapsed);
        const double cell=g_settings.notificationIconWidth,pitch=cell+4,row=24;
        const int count=int(apps.icons.size());
        DockArrangeGroup(apps,0,1,cell,pitch,row);
        if(count%2){const int i=count-1;
            DockTranslate(apps.presenters[i],pitch*(i/2)-cell*i+(pitch-cell)/2,0);
        }
        std::wstring state=L"{\"active\":true,\"apps\":"+std::to_wstring(count)+
            L",\"battery\":0,\"input\":0,\"total\":"+std::to_wstring(count)+
            L",\"columns\":"+std::to_wstring(DockGrid::TotalColumns(count))+L",\"cells\":[";
        bool first=true;for(auto const& item:apps.icons){if(!first)state+=L",";first=false;state+=DockRect(item,root);}state+=L"]}";
        if(state!=g_dockUnified->lastState){Wh_SetStringValue(L"mergedGridState",state.c_str());g_dockUnified->lastState=std::move(state);
            auto& saved=g_dockUnified->saved;saved.erase(std::remove_if(saved.begin(),saved.end(),[](auto& x){return !x.element.get();}),saved.end());}
    }catch(...){Wh_Log(L"Card tray update error %08X",winrt::to_hresult());}
}
