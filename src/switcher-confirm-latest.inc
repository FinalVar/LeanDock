// Optional two-item confirmation for a real corner activation. Reads the
// native SwitchItemList, then invokes its selected native thumbnail button.
// Never enumerates arbitrary HWNDs, guesses a foreground window, or sends Enter.
#include "switcher-confirm-policy-latest.h"
#include "switcher-layout-policy-latest.h"
#include "switcher-clip-policy-latest.h"
#include "switcher-visibility-latest.h"
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <winrt/Windows.UI.Xaml.Input.h>

namespace LeanDockSwitcher {
FrameworkElement FindNamed(FrameworkElement const& root, PCWSTR name, unsigned& budget);
#include "switcher-layout-latest.inc"
#include "switcher-clip-latest.inc"

struct RootWatch {
    winrt::weak_ref<FrameworkElement> root;
    winrt::event_token loaded{}, layout{}, unloaded{}, hostChanged{},gotFocus{};
    bool loadedSet{}, layoutSet{}, unloadedSet{}, hostChangedSet{}, visibilitySet{},focusSet{};
    int64_t visibilityToken{};
    winrt::Windows::UI::Xaml::XamlRoot hostRoot{nullptr};
    LeanDockSwitcherVisibility::WindowMark windowMark;
    std::wstring lastGeometry;
    LayoutState sizing;
    ClipSyncState clipSync;
    bool attached{true},confirmQueued{};
    unsigned long long observedTicket{};
};
thread_local std::unordered_map<ElementId, std::shared_ptr<RootWatch>> g_roots;
static std::atomic<unsigned> g_confirmed{}, g_keptOpen{}, g_invokeErrors{},g_attachErrors{},g_lastCount{};
static std::atomic<int> g_lastSelected{-1};
static std::atomic<int> g_lastFocused{-1};

void ClearVisibility(std::shared_ptr<RootWatch> const& watch) noexcept {
    ReleasePublicClip(watch->clipSync);
    if(watch->observedTicket)Native_SwitcherConsume(watch->observedTicket);
    watch->observedTicket=0;
    watch->windowMark.Clear();
    try { if (watch->hostChangedSet && watch->hostRoot) watch->hostRoot.Changed(watch->hostChanged); } catch (...) {}
    watch->hostChangedSet=false;
    watch->hostRoot=nullptr;
}

void RefreshVisibility(std::shared_ptr<RootWatch> const& watch) noexcept {
    try {
        auto root=watch->root.get();
        if (!g_initializedForThread || !root || !root.IsLoaded()) { ClearVisibility(watch); return; }
        auto host=root.XamlRoot();
        if (host!=watch->hostRoot) {
            ClearVisibility(watch);
            watch->hostRoot=host;
            if (host) {
                std::weak_ptr<RootWatch> weak=watch;
                watch->hostChanged=host.Changed([weak](auto const&,auto const&){
                    if(auto value=weak.lock()) RefreshVisibility(value);
                });
                watch->hostChangedSet=true;
            }
        }
        if (!host || !host.IsHostVisible() || root.Visibility()!=Visibility::Visible) {
            ReleasePublicClip(watch->clipSync);
            if(watch->observedTicket)Native_SwitcherConsume(watch->observedTicket);
            watch->observedTicket=0;watch->windowMark.Clear(); return;
        }
        HWND foreground=GetForegroundWindow();
        DWORD owner=0;
        wchar_t name[64]{};
        if (GetWindowThreadProcessId(foreground,&owner)!=GetCurrentThreadId() ||
            owner!=GetCurrentProcessId() || !IsWindowVisible(foreground) ||
            !GetClassNameW(foreground,name,64) || wcscmp(name,L"XamlExplorerHostIslandWindow")) {
            if(watch->observedTicket)Native_SwitcherConsume(watch->observedTicket);
            watch->observedTicket=0;watch->windowMark.Clear(); return;
        }
        watch->windowMark.Set(foreground);
    } catch (...) { watch->windowMark.Clear(); }
}

FrameworkElement FindNamed(FrameworkElement const& root, PCWSTR name, unsigned& budget) {
    if (!root || !budget) return nullptr;
    --budget;
    if (root.Name() == name) return root;
    int count = Media::VisualTreeHelper::GetChildrenCount(root);
    for (int i=0; i<count && budget; ++i) {
        auto child = Media::VisualTreeHelper::GetChild(root, i).try_as<FrameworkElement>();
        if (auto found = FindNamed(child, name, budget)) return found;
    }
    return nullptr;
}

// Passive geometry evidence from the already installed native layout events.
// No input, window activation, extra timer or private layout-property parsing.
void RecordGeometry(std::shared_ptr<RootWatch> const& watch) noexcept {
    if (!g_initializedForThread) return;
    try {
        auto root=watch->root.get();
        if (!root || !root.IsLoaded() || root.Visibility()!=Visibility::Visible) return;
        auto xamlRoot=root.XamlRoot();
        if (!xamlRoot || !xamlRoot.IsHostVisible()) return;
        unsigned budget=96;
        auto canvas=FindNamed(root,L"BackgroundElement",budget);
        budget=96;
        auto listElement=FindNamed(root,L"SwitchItemListControl",budget);
        auto list=listElement ? listElement.try_as<Controls::ListView>() : nullptr;
        if (!canvas || !list || canvas.ActualWidth()<=0 || canvas.ActualHeight()<=0) return;
        auto bounds=[&](FrameworkElement const& element) {
            return element.TransformToVisual(root).TransformBounds(
                {0,0,static_cast<float>(element.ActualWidth()),static_cast<float>(element.ActualHeight())});
        };
        auto box=bounds(canvas);
        // Public XAML geometry only. Do not use GetElementVisual: obtaining a
        // hand-off Composition visual is not a passive size query.
        budget=64;
        auto presenter=FindNamed(listElement,L"ScrollContentPresenter",budget);
        winrt::Windows::Foundation::Rect clipBounds{-1,-1,-1,-1};
        bool clipKnown=false;
        if(presenter) {
            if(auto clip=presenter.Clip().try_as<Media::RectangleGeometry>()) {
                clipBounds=presenter.TransformToVisual(root).TransformBounds(clip.Rect());
                clipKnown=true;
            }
        }
        unsigned count=list.Items().Size(), realized=0, inside=0,insideClip=0;
        double minWidth=0,maxWidth=0,minHeight=0,maxHeight=0,scrollable=-1;
        double contentLeft=0,contentTop=0,contentRight=0,contentBottom=0;
        std::vector<float> rowTops;
        for(unsigned i=0;i<count && i<128;++i) {
            auto item=list.ContainerFromIndex(i).try_as<FrameworkElement>();
            if(!item || !item.IsLoaded() || item.Visibility()!=Visibility::Visible ||
                item.ActualWidth()<=0 || item.ActualHeight()<=0) continue;
            auto rect=bounds(item);
            if(!realized++) {
                minWidth=maxWidth=rect.Width;minHeight=maxHeight=rect.Height;
                contentLeft=rect.X;contentTop=rect.Y;
                contentRight=rect.X+rect.Width;contentBottom=rect.Y+rect.Height;
            }
            else {minWidth=std::min(minWidth,double(rect.Width));maxWidth=std::max(maxWidth,double(rect.Width));
                  minHeight=std::min(minHeight,double(rect.Height));maxHeight=std::max(maxHeight,double(rect.Height));
                  contentLeft=std::min(contentLeft,double(rect.X));contentTop=std::min(contentTop,double(rect.Y));
                  contentRight=std::max(contentRight,double(rect.X+rect.Width));contentBottom=std::max(contentBottom,double(rect.Y+rect.Height));}
            if(rect.X>=box.X-1 && rect.Y>=box.Y-1 &&
                rect.X+rect.Width<=box.X+box.Width+1 && rect.Y+rect.Height<=box.Y+box.Height+1) ++inside;
            if(clipKnown && rect.X>=clipBounds.X-1 && rect.Y>=clipBounds.Y-1 &&
                rect.X+rect.Width<=clipBounds.X+clipBounds.Width+1 &&
                rect.Y+rect.Height<=clipBounds.Y+clipBounds.Height+1) ++insideClip;
            if(std::none_of(rowTops.begin(),rowTops.end(),[&](float y){return std::abs(y-rect.Y)<2;})) rowTops.push_back(rect.Y);
        }
        budget=32;
        auto scrollElement=FindNamed(listElement,L"ScrollViewer",budget);
        Thickness scrollPadding{-1,-1,-1,-1};
        double viewportWidth=-1,viewportHeight=-1,extentHeight=-1;
        if(auto scroll=scrollElement ? scrollElement.try_as<Controls::ScrollViewer>() : nullptr) {
            scrollable=scroll.ScrollableHeight();
            scrollPadding=scroll.Padding();
            viewportWidth=scroll.ViewportWidth();viewportHeight=scroll.ViewportHeight();extentHeight=scroll.ExtentHeight();
        }
        wchar_t json[3072]{};
        _snwprintf_s(json,_TRUNCATE,
            L"{\"schema\":4,\"count\":%u,\"sampleLimited\":%ls,\"realized\":%u,\"insideCanvas\":%u,\"rows\":%u,"
            L"\"rootWidth\":%.2f,\"rootHeight\":%.2f,\"canvasX\":%.2f,\"canvasY\":%.2f,\"canvasWidth\":%.2f,\"canvasHeight\":%.2f,"
            L"\"minItemWidth\":%.2f,\"maxItemWidth\":%.2f,\"minItemHeight\":%.2f,\"maxItemHeight\":%.2f,\"scrollableHeight\":%.2f,"
            L"\"outerInsets\":[%.2f,%.2f,%.2f,%.2f],\"scrollPadding\":[%.2f,%.2f,%.2f,%.2f],"
            L"\"plannedRows\":%u,\"plannedCardHeight\":%.2f,\"layoutDisabled\":%ls,\"layoutError\":%ld,\"layoutRepairs\":%u,"
            L"\"publicClipKnown\":%ls,\"insidePublicClip\":%u,\"clipBoundsInRoot\":[%.2f,%.2f,%.2f,%.2f],"
            L"\"presenterSize\":[%.2f,%.2f],\"viewportSize\":[%.2f,%.2f],\"extentHeight\":%.2f,"
            L"\"clipRepairs\":%u,\"clipRepairErrors\":%u,\"clipRepairError\":%ld,\"clipRepairDisabled\":%ls,"
            L"\"clipHistoryKnown\":%ls,\"clipPeerRetained\":%ls}",
            count,count>128?L"true":L"false",realized,inside,static_cast<unsigned>(rowTops.size()),
            root.ActualWidth(),root.ActualHeight(),double(box.X),double(box.Y),double(box.Width),double(box.Height),
            minWidth,maxWidth,minHeight,maxHeight,scrollable,
            realized?contentLeft-box.X:-1,realized?contentTop-box.Y:-1,
            realized?box.X+box.Width-contentRight:-1,realized?box.Y+box.Height-contentBottom:-1,
            scrollPadding.Left,scrollPadding.Top,scrollPadding.Right,scrollPadding.Bottom,
            watch->sizing.plan.rows,watch->sizing.plan.cardHeight,
            watch->sizing.disabled?L"true":L"false",watch->sizing.error,watch->sizing.repairs,
            clipKnown?L"true":L"false",insideClip,double(clipBounds.X),double(clipBounds.Y),double(clipBounds.Width),double(clipBounds.Height),
            presenter?presenter.ActualWidth():-1,presenter?presenter.ActualHeight():-1,viewportWidth,viewportHeight,extentHeight,
            watch->clipSync.repairs,watch->clipSync.errors,watch->clipSync.error,watch->clipSync.disabled?L"true":L"false",
            watch->clipSync.sizes.known?L"true":L"false",watch->clipSync.geometry?L"true":L"false");
        if(watch->lastGeometry!=json) {watch->lastGeometry=json;Wh_SetStringValue(L"switcherGeometryState",json);}
    } catch (...) {
        // Explicitly replace old evidence when this read fails.
        Wh_SetStringValue(L"switcherGeometryState",L"{\"schema\":1,\"unavailable\":true}");
        watch->lastGeometry.clear();
    }
}

void PublishConfirm() noexcept {
    wchar_t json[512]{};
    _snwprintf_s(json,_TRUNCATE,
        L"{\"schema\":4,\"autoConfirm\":true,\"invokeAccepted\":%u,\"keptOpen\":%u,\"invokeErrors\":%u,\"attachErrors\":%u,\"lastCount\":%u,\"lastSelected\":%d,\"lastFocused\":%d,\"listSource\":\"native-SwitchItemList\",\"targetSource\":\"native-item-keyboard-focus\",\"scope\":\"corner-ticket-only\"}",
        g_confirmed.load(),g_keptOpen.load(),g_invokeErrors.load(),g_attachErrors.load(),g_lastCount.load(),g_lastSelected.load(),g_lastFocused.load());
    Wh_SetStringValue(L"switcherConfirmState",json);
}

int FocusedItemIndex(Controls::ListView const& list,
                         winrt::Windows::UI::Xaml::XamlRoot const& host) {
    if(!host||list.Items().Size()!=2)return -1;
    // This native template uses focus, not ListView selection (SelectedIndex
    // remains -1). Resolve the focused native item or any of its descendants.
    FrameworkElement containers[2]{nullptr,nullptr};
    for(int i=0;i<2;++i){
        containers[i]=list.ContainerFromIndex(i).try_as<FrameworkElement>();
    }
    auto focused=Input::FocusManager::GetFocusedElement(host).try_as<DependencyObject>();
    for(unsigned depth=0;focused&&depth<64;++depth){
        for(int i=0;i<2;++i)if(containers[i]&&focused==containers[i])return i;
        if(focused==list)break;
        focused=Media::VisualTreeHelper::GetParent(focused);
    }
    return -1;
}

void ConfirmReady(std::shared_ptr<RootWatch> const& watch,unsigned long long ticket) noexcept {
    if (!watch->attached || !ticket || Native_SwitcherPeek()!=ticket || !g_initializedForThread) return;
    try {
        auto root = watch->root.get();
        if (!root || !root.IsLoaded() || root.Visibility()!=Visibility::Visible) return;
        auto xamlRoot = root.XamlRoot();
        if (!xamlRoot || !xamlRoot.IsHostVisible()) return;
        HWND foreground = GetForegroundWindow();
        DWORD owner=0;
        if (GetWindowThreadProcessId(foreground, &owner)!=GetCurrentThreadId() || owner!=GetCurrentProcessId()) return;
        wchar_t cls[64]{};
        if (!GetClassNameW(foreground, cls, 64) || wcscmp(cls,L"XamlExplorerHostIslandWindow")) return;
        unsigned budget=64;
        auto listElement = FindNamed(root, L"SwitchItemListControl", budget);
        auto list = listElement ? listElement.try_as<Controls::ListView>() : nullptr;
        if (!list) return;
        auto count=list.Items().Size();
        int selected=FocusedItemIndex(list,xamlRoot);
        g_lastCount=count;g_lastSelected=list.SelectedIndex();g_lastFocused=selected;
        PublishConfirm();
        auto decision=NativeSwitcherPolicy::Decide(true,count,selected,false);
        if (decision==NativeSwitcherPolicy::Decision::KeepOpen) {
            Native_SwitcherReveal(ticket);
            if (Native_SwitcherConsume(ticket)) { ++g_keptOpen; PublishConfirm(); }
            return;
        }
        if (count!=2 || selected<0 || selected>=2) return;
        // Both entries must be realized before invoking; do not act on an
        // unfinished list or substitute a guessed window/Enter key.
        for(unsigned i=0;i<2;++i) {
            auto entry=list.ContainerFromIndex(i).try_as<FrameworkElement>();
            if(!entry||!entry.IsLoaded()||entry.Visibility()!=Visibility::Visible||entry.ActualWidth()<=0||entry.ActualHeight()<=0)return;
        }
        auto container=list.ContainerFromIndex(selected).try_as<FrameworkElement>();
        if (!container || container.ActualWidth()<=0 || container.ActualHeight()<=0) return;
        budget=96;
        auto thumbnail=FindNamed(container,L"ThumbnailHost",budget);
        if (!thumbnail) return;
        auto peer=Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(thumbnail);
        if (!peer) return;
        auto pattern=peer.GetPattern(Automation::Peers::PatternInterface::Invoke);
        auto invoke=pattern ? pattern.try_as<Automation::Provider::IInvokeProvider>() : nullptr;
        if (NativeSwitcherPolicy::Decide(true,count,selected,static_cast<bool>(invoke))!=NativeSwitcherPolicy::Decision::Confirm) return;
        // The dispatcher callback can outlive a close/reopen or a selection
        // change. Revalidate the actual marked host and current native item.
        if(LeanDockSwitcherVisibility::ActiveWindow()!=foreground||list.Items().Size()!=2||FocusedItemIndex(list,xamlRoot)!=selected)return;
        if(GetAsyncKeyState(VK_LBUTTON)&0x8000||GetAsyncKeyState(VK_RBUTTON)&0x8000) {
            Native_SwitcherReveal(ticket);
            if(Native_SwitcherConsume(ticket)){++g_keptOpen;PublishConfirm();}return;
        }
        // Consume before invoking: invocation can synchronously destroy/rebuild
        // the tree and re-enter Loaded/LayoutUpdated. It must never fire twice.
        if (!Native_SwitcherConsume(ticket)) return;
        try { invoke.Invoke(); ++g_confirmed; }
        catch (...) { Native_SwitcherReveal(ticket);++g_invokeErrors; }
        PublishConfirm();
    } catch (...) {
        // Missing templates/providers leave the ordinary native picker usable.
        Native_SwitcherReveal(ticket);
        if (Native_SwitcherConsume(ticket)) { ++g_invokeErrors; PublishConfirm(); }
    }
}

// Leave the native event stack before invoking, but do not wait for the input
// queue to become idle: a Low-priority callback can leave the picker visible.
// Normal is the documented UI priority; High is reserved for system events.
// One pending callback per root, no timer/poll or synchronous layout reentry.
void TryConfirm(std::shared_ptr<RootWatch> const& watch) noexcept {
    if(!watch->attached||watch->confirmQueued||!g_initializedForThread)return;
    auto ticket=Native_SwitcherPeek();if(!ticket)return;
    try {
        auto root=watch->root.get();
        if(!root||!root.IsLoaded()||root.Visibility()!=Visibility::Visible||!root.XamlRoot()||!root.XamlRoot().IsHostVisible()||!LeanDockSwitcherVisibility::ActiveWindow())return;
        watch->observedTicket=ticket;watch->confirmQueued=true;
        std::weak_ptr<RootWatch> weak=watch;
        root.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[weak,ticket]{
            if(auto value=weak.lock()){value->confirmQueued=false;ConfirmReady(value,ticket);}
        });
    }catch(...){watch->confirmQueued=false;Native_SwitcherReveal(ticket);if(Native_SwitcherConsume(ticket)){++g_invokeErrors;PublishConfirm();}}
}

void Detach(std::shared_ptr<RootWatch> const& watch) noexcept {
    watch->attached=false;
    ClearVisibility(watch);
    try { if (auto root=watch->root.get()) {
        if (watch->loadedSet) root.Loaded(watch->loaded);
        if (watch->layoutSet) root.LayoutUpdated(watch->layout);
        if (watch->unloadedSet) root.Unloaded(watch->unloaded);
        if (watch->focusSet) root.GotFocus(watch->gotFocus);
        if (watch->visibilitySet) root.UnregisterPropertyChangedCallback(UIElement::VisibilityProperty(),watch->visibilityToken);
    }} catch (...) {}
    RestoreLayout(watch->sizing);
}
void Remove(ElementId id) noexcept {
    auto it=g_roots.find(id);
    if (it!=g_roots.end()) { Detach(it->second); g_roots.erase(it); }
}
void Clear() noexcept {
    for (auto const& entry:g_roots) Detach(entry.second);
    g_roots.clear();
}
void Attach(ElementId id, FrameworkElement const& element) noexcept {
    try {
        if (winrt::get_class_name(element)!=L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab" || !Supported()) return;
        for (auto it=g_roots.begin();it!=g_roots.end();) {
            if (!it->second->root.get()) { Detach(it->second); it=g_roots.erase(it); } else ++it;
        }
        if (g_roots.contains(id)) return;
        auto watch=std::make_shared<RootWatch>();
        watch->root=element;
        g_roots.emplace(id,watch);
        std::weak_ptr<RootWatch> weak=watch;
        try {
            watch->loaded=element.Loaded([weak](auto const&,auto const&){if(auto value=weak.lock()){RefreshVisibility(value);AdaptLayout(value->root.get(),value->sizing);SynchronizePublicClip(value->root.get(),value->sizing,value->clipSync);RecordGeometry(value);TryConfirm(value);}});
            watch->loadedSet=true;
            watch->layout=element.LayoutUpdated([weak](auto const&,auto const&){if(auto value=weak.lock()){RefreshVisibility(value);AdaptLayout(value->root.get(),value->sizing);SynchronizePublicClip(value->root.get(),value->sizing,value->clipSync);RecordGeometry(value);TryConfirm(value);}});
            watch->layoutSet=true;
            watch->gotFocus=element.GotFocus([weak](auto const&,auto const&){if(auto value=weak.lock()){RefreshVisibility(value);TryConfirm(value);}});
            watch->focusSet=true;
            watch->unloaded=element.Unloaded([weak](auto const&,auto const&){if(auto value=weak.lock()) ClearVisibility(value);});
            watch->unloadedSet=true;
            watch->visibilityToken=element.RegisterPropertyChangedCallback(UIElement::VisibilityProperty(),[weak](auto const&,auto const&){if(auto value=weak.lock()) RefreshVisibility(value);});
            watch->visibilitySet=true;
        } catch (...) { Remove(id); throw; }
        // Initial replay may see an already loaded native island.
        RefreshVisibility(watch);
        AdaptLayout(element,watch->sizing);
        SynchronizePublicClip(element,watch->sizing,watch->clipSync);
        RecordGeometry(watch);
        TryConfirm(watch);PublishConfirm();
    } catch (...) { ++g_attachErrors; PublishConfirm(); }
}
} // namespace LeanDockSwitcher
