// Kept separate from preserved upstream. Battery moves only after a visible
// replacement card is measured. A crowded or failed card restores the native
// entry, including its original compact two-row placement.
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
            DockSet(battery,UIElement::VisibilityProperty(),Visibility::Visible);
            DockSet(battery,UIElement::IsHitTestVisibleProperty(),true);
            DockSet(input,UIElement::VisibilityProperty(),Visibility::Visible);
            UpdateLegacyUnifiedDockGrid();return;
        }
        struct Guard{bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}}guard(g_dockUnified->updating);
        auto apps=DockCollect(root,true);if(!apps.container||!apps.panel)return;
        // Collapsed removes the native layout slot; opacity alone leaves a gap.
        DockSet(battery,UIElement::VisibilityProperty(),Visibility::Collapsed);
        DockSet(battery,UIElement::IsHitTestVisibleProperty(),false);
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
