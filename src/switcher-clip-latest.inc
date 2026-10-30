// Reconcile only a proven stale native ScrollContentPresenter rectangle.
// Public XAML only; no hand-off visual, layout reentry, async callback or focus.
struct ClipSyncState {
    // Native XAML can retain the core geometry while releasing its COM peer.
    // A weak peer reference then expires and loses the previous-size evidence.
    // Retain both peers only for the visible root; release on hide/unload.
    FrameworkElement presenter{nullptr};
    Media::RectangleGeometry geometry{nullptr};
    NativeSwitcherClip::State sizes;
    bool writing{},disabled{};
    unsigned repairs{},errors{};
    long error{};
};

void ReleasePublicClip(ClipSyncState& s) noexcept {
    s.geometry=nullptr;s.presenter=nullptr;s.sizes={};
}

void RememberPublicClip(ClipSyncState& s,FrameworkElement const& presenter,Media::RectangleGeometry const& geometry) {
    if(s.presenter!=presenter||s.geometry!=geometry) {
        s.sizes={};s.presenter=presenter;s.geometry=geometry;
    }
}

void SynchronizePublicClip(FrameworkElement const& root,LayoutState const& layout,ClipSyncState& s) noexcept {
    if(s.writing||s.disabled||!g_initializedForThread||layout.disabled||!layout.plan.valid||!layout.panel.get())return;
    try {
        if(!root||!root.IsLoaded()||root.Visibility()!=Visibility::Visible||
           !root.XamlRoot()||!root.XamlRoot().IsHostVisible())return;
        unsigned budget=96;
        auto list=FindNamed(root,L"SwitchItemListControl",budget);
        budget=64;auto presenter=FindNamed(list,L"ScrollContentPresenter",budget);
        budget=32;auto scrollElement=FindNamed(list,L"ScrollViewer",budget);
        auto scroll=scrollElement?scrollElement.try_as<Controls::ScrollViewer>():nullptr;
        if(!presenter||!presenter.try_as<Controls::ScrollContentPresenter>()||!scroll||!presenter.IsLoaded())return;
        auto geometry=presenter.Clip().try_as<Media::RectangleGeometry>();
        if(!geometry||geometry.Transform()) {ReleasePublicClip(s);return;}
        RememberPublicClip(s,presenter,geometry);
        const NativeSwitcherClip::Size actual{presenter.ActualWidth(),presenter.ActualHeight()};
        const NativeSwitcherClip::Size viewport{scroll.ViewportWidth(),scroll.ViewportHeight()};
        auto r=geometry.Rect();
        if(s.sizes.Observe({r.X,r.Y,r.Width,r.Height},actual,viewport)!=NativeSwitcherClip::Action::RepairPreviousSize)return;
        // Keep the native geometry object and its clipping semantics. Its
        // arrange contract is Rect(0,0,viewport.Width,viewport.Height). Updating
        // this render rectangle does not resize/rearrange or activate a window.
        s.writing=true;
        geometry.Rect({0,0,static_cast<float>(actual.width),static_cast<float>(actual.height)});
        r=geometry.Rect();
        if(s.sizes.Observe({r.X,r.Y,r.Width,r.Height},actual,viewport)!=NativeSwitcherClip::Action::Synchronized) {
            s.disabled=true;s.error=E_FAIL;++s.errors;
        }else ++s.repairs;
        s.writing=false;
    }catch(winrt::hresult_error const& e){s.writing=false;s.disabled=true;s.error=e.code();++s.errors;}
     catch(...){s.writing=false;s.disabled=true;s.error=E_FAIL;++s.errors;}
}
