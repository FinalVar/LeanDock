#include "shell-menu-latest.h"
#include "layout-cost-latest.h"
#include "cards-dashboard-latest.h"
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.h>

// Before the aggregate time key (0.8.78), the native power entry was mapped onto
// the dashboard's power card. The key owns power now, so the entry only stays
// measured and dormant; the mapping code was removed in 0.8.110 (it is in git
// history before that, with src/cards-latest.cpp). This removes the properties
// the dormant entry and the old mapping published.
void ClearNativePowerHit() {
    if(g_dockUnified&&g_dockUnified->threadId!=GetCurrentThreadId())return;
    const HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);
    for(auto name:{L"LeanDock.Card.NativePowerHit",L"LeanDock.Card.NativePowerPrepared",L"LeanDock.Card.NativePowerWidth100",L"LeanDock.Card.NativePowerHeight100",L"LeanDock.Card.NativePowerIcons",L"LeanDock.Card.NativePowerLoadedIcons",L"LeanDock.Card.NativePowerTargetWidth100",L"LeanDock.Card.NativePowerTargetHeight100",L"LeanDock.Card.NativePowerCoverage",L"LeanDock.Card.NativePowerTipOwners",L"LeanDock.Card.NativePowerTipsRemaining",L"LeanDock.Card.NativePowerTipScanComplete",L"LeanDock.Card.NativePowerTipError"})
        RemovePropW(taskbar,name);
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
    // The negative margin contributes zero flow width; this adds neither a
    // tray icon nor an empty column.
    DockSet(battery,UIElement::IsHitTestVisibleProperty(),false);
    DockSet(battery,UIElement::OpacityProperty(),0.0);
    if(auto control=battery.try_as<Controls::Control>())
        DockSet(control,Controls::Control::IsTabStopProperty(),false);
    DockSet(battery,Automation::AutomationProperties::AccessibilityViewProperty(),Automation::Peers::AccessibilityView::Raw);
    DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
    DockBox(battery,LeanDockCards::Dashboard::PowerWidth,LeanDockCards::CardHeight,HorizontalAlignment::Left,false);
    DockSet(battery,FrameworkElement::MarginProperty(),Thickness{0,0,-LeanDockCards::Dashboard::PowerWidth,0});
    if(auto grid=FindChildByClassName(battery,L"Windows.UI.Xaml.Controls.Grid"))
        DockBox(grid,LeanDockCards::Dashboard::PowerWidth,LeanDockCards::CardHeight);
    const bool measured=DockEqual(battery.ActualWidth(),LeanDockCards::Dashboard::PowerWidth)&&
                        DockEqual(battery.ActualHeight(),LeanDockCards::CardHeight);
    const HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);const HANDLE prepared=reinterpret_cast<HANDLE>(measured?2:1);
    if(GetPropW(taskbar,L"LeanDock.Card.NativePowerPrepared")!=prepared)SetPropW(taskbar,L"LeanDock.Card.NativePowerPrepared",prepared);
}
// The time key owns power as soon as its slot is measured: the native entry
// keeps a measured, noninteractive zero-flow slot without an extra tray icon.
// Until then the original compact native entry stays.
void UpdateUnifiedDockGrid() {
    LeanDockLayoutCost::Scope cost(LeanDockLayoutCost::Tray);
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
        PrepareDormantNativePower(battery);
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
