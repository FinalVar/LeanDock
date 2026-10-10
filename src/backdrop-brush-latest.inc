// Keep the upstream effect graph and XAML brush. Only reconnect its backdrop
// source around a desktop/unhide transition. All XAML work stays on its UI
// thread; HWND painting is not used as a substitute for backdrop invalidation.
thread_local std::vector<winrt::weak_ref<XamlBlurBrush>> g_leanDockBlurBrushes;

template<typename... Args>
winrt::Windows::Foundation::IInspectable MakeLeanDockBlurBrush(Args&&... args) {
    auto brush=winrt::make_self<XamlBlurBrush>(std::forward<Args>(args)...);
    std::erase_if(g_leanDockBlurBrushes,[](auto const& weak){return !weak.get();});
    g_leanDockBlurBrushes.push_back(brush->get_weak());
    return brush.template as<winrt::Windows::Foundation::IInspectable>();
}
void XamlBlurBrush::StopBackdropResume() noexcept {
    m_backdropPlan.Cancel();
    if(m_backdropTimer)try{
        m_backdropTimer.Stop();
        if(m_backdropTick.value)m_backdropTimer.Tick(m_backdropTick);
    }catch(...){}
    m_backdropTimer=nullptr;m_backdropTick={};
    if(LeanDockShell::IsTaskbar(m_backdropTaskbar,GetCurrentProcessId()))
        LeanDockBackdrop::Put(m_backdropTaskbar,L"LeanDock.Backdrop.Pending",0);
    m_backdropTaskbar=nullptr;
}
bool XamlBlurBrush::RefreshBackdropSource(XamlRoot const& root,HWND taskbar,bool requireVisible) {
    auto owner=m_backdropOwner.get();
    if(!root||!owner||owner.XamlRoot()!=root||!owner.IsLoaded()||
       (requireVisible&&!root.IsHostVisible())||
       !LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))return false;
    auto effect=CompositionBrush().try_as<wuc::CompositionEffectBrush>();
    if(!effect)return false; // disconnected or native fallback stays native
    effect.SetSourceParameter(L"backdrop",m_compositor.CreateBackdropBrush());
    // Submission is not proof that the display has presented the scene.
    m_compositor.RequestCommitAsync();
    LeanDockBackdrop::Count(taskbar,L"LeanDock.Backdrop.Refreshes");
    LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.LastRefreshAt",GetTickCount64());
    return true;
}
void XamlBlurBrush::PrepareBackdrop(XamlRoot const& root,HWND taskbar) noexcept {
    try {
        auto owner=m_backdropOwner.get();if(!owner||owner.XamlRoot()!=root)return;
        StopBackdropResume(); // an old visible scene must not run after Unhide
        if(RefreshBackdropSource(root,taskbar,false)){
            LeanDockBackdrop::Count(taskbar,L"LeanDock.Backdrop.Prepares");
            LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.LastPrepareAt",GetTickCount64());
        }
    }catch(...){LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Error",static_cast<unsigned>(winrt::to_hresult()));}
}
void XamlBlurBrush::ScheduleBackdropResume(XamlRoot const& root,HWND taskbar,unsigned reason) noexcept {
    try {
        auto owner=m_backdropOwner.get();
        if(!owner||owner.XamlRoot()!=root||!CompositionBrush().try_as<wuc::CompositionEffectBrush>())return;
        StopBackdropResume();
        auto dispatcher=winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if(!dispatcher)return;
        m_backdropTaskbar=taskbar;
        auto timer=dispatcher.CreateTimer();timer.IsRepeating(false);
        const auto ticket=m_backdropPlan.Begin(GetTickCount64());
        auto weak=get_weak();auto weakRoot=winrt::make_weak(root);
        m_backdropTick=timer.Tick([weak,weakRoot,taskbar,ticket](auto const& sender,auto const&) noexcept {
            auto self=weak.get();if(!self||!self->m_backdropPlan.Current(ticket))return;
            try {
                auto root=weakRoot.get();auto owner=self->m_backdropOwner.get();
                if(!root||!owner||owner.XamlRoot()!=root||!owner.IsLoaded()||!root.IsHostVisible()||
                   !LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId())){
                    self->StopBackdropResume();LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Pending",0);return;
                }
                const auto now=GetTickCount64();
                if(!self->m_backdropPlan.Ready(ticket,now)){
                    sender.Interval(std::chrono::milliseconds(self->m_backdropPlan.due-now));sender.Start();return;
                }
                if(!self->RefreshBackdropSource(root,taskbar,true)){
                    self->StopBackdropResume();return;
                }
                if(self->m_backdropPlan.Advance(ticket,now)){
                    sender.Interval(std::chrono::milliseconds(LeanDockBackdrop::Plan::SettleDelay));sender.Start();
                }else{
                    self->StopBackdropResume();LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Pending",0);
                }
            }catch(...){
                LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Error",static_cast<unsigned>(winrt::to_hresult()));
                self->StopBackdropResume();LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Pending",0);
            }
        });
        m_backdropTimer=timer;
        LeanDockBackdrop::Count(taskbar,L"LeanDock.Backdrop.Requests");
        LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.LastRequestAt",GetTickCount64());
        LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Reason",reason);
        LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Pending",1);
        timer.Interval(std::chrono::milliseconds(LeanDockBackdrop::Plan::FirstDelay));timer.Start();
    }catch(...){
        LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Error",static_cast<unsigned>(winrt::to_hresult()));
        StopBackdropResume();LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Pending",0);
    }
}
void ResumeLeanDockBackdrop(HWND taskbar,WPARAM reason,LPARAM at) noexcept {
    if(!LeanDockBackdrop::Fresh(static_cast<ULONGLONG>(at),GetTickCount64())||
       (reason!=static_cast<WPARAM>(LeanDockBackdrop::Reason::Desktop)&&
        reason!=static_cast<WPARAM>(LeanDockBackdrop::Reason::Expansion)&&
        reason!=static_cast<WPARAM>(LeanDockBackdrop::Reason::BeforeShow)))return;
    try {
        std::erase_if(g_leanDockBlurBrushes,[](auto const& weak){return !weak.get();});
        for(auto& state:g_clickThroughTaskbarState){
            if(state.cardsClosing||!state.islandHwnd||GetAncestor(state.islandHwnd,GA_ROOT)!=taskbar)continue;
            auto root=state.xamlRoot.get();if(!root)continue;
            // Copy weak entries before calling XAML, which may reenter styling.
            const auto brushes=g_leanDockBlurBrushes;
            for(auto const& weak:brushes)if(auto brush=weak.get()){
                if(reason==static_cast<WPARAM>(LeanDockBackdrop::Reason::BeforeShow))brush->PrepareBackdrop(root,taskbar);
                else brush->ScheduleBackdropResume(root,taskbar,static_cast<unsigned>(reason));
            }
        }
    }catch(...){LeanDockBackdrop::Put(taskbar,L"LeanDock.Backdrop.Error",static_cast<unsigned>(winrt::to_hresult()));}
}
void StopLeanDockBackdrops() noexcept try {
    const auto brushes=std::move(g_leanDockBlurBrushes);g_leanDockBlurBrushes.clear();
    for(auto const& weak:brushes)if(auto brush=weak.get())brush->StopBackdropResume();
    for(auto const& state:g_clickThroughTaskbarState)if(state.islandHwnd){
        const HWND taskbar=GetAncestor(state.islandHwnd,GA_ROOT);
        for(auto key:{L"LeanDock.Backdrop.Requests",L"LeanDock.Backdrop.Refreshes",L"LeanDock.Backdrop.LastRequestAt",
                      L"LeanDock.Backdrop.LastRefreshAt",L"LeanDock.Backdrop.Reason",L"LeanDock.Backdrop.Pending",L"LeanDock.Backdrop.Error",
                      L"LeanDock.Backdrop.Prepares",L"LeanDock.Backdrop.LastPrepareAt"})
            RemovePropW(taskbar,key);
    }
}catch(...){Wh_Log(L"Backdrop cleanup: %08X",winrt::to_hresult());}
