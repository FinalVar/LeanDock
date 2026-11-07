// Taskbar.View 2608.26001.200.0 passes four nontrivial WinRT values by address
// and returns fire_and_forget in AL (verified from its matching public PDB and
// entry disassembly). The first FrameworkElement value is moved into the
// native coroutine. Replace that owned value using normal COM copy assignment;
// the original function still consumes/releases all four arguments.
using LeanDockNativeMenu=unsigned char(WINAPI*)(FrameworkElement*,void*,void*,void*);
LeanDockNativeMenu g_nativeDockMenu;
using LeanDockFlyoutShow=void(WINAPI*)(Controls::Primitives::FlyoutBase*,const DependencyObject*,const Controls::Primitives::FlyoutShowOptions*);
LeanDockFlyoutShow g_nativeFlyoutShow;
void (*g_nativeMenuResume)();
void (*g_nativeMenuShowLeft)();
void (*g_nativeMenuShowRight)();

// The ShowAt wrapper is shared by other flyouts. Match the native Win+X call
// sites from this pinned module's PDB as well as the one-use mouse gesture.
// Merely opening another MenuFlyout must never change its keyboard focus.
bool IsLeanDockMenuCaller(void* caller) {
    DWORD64 base=0;
    auto entry=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(caller),&base,nullptr);
    if(!entry)return false;
    const auto start=base+entry->BeginAddress;
    return (g_nativeMenuResume&&start==reinterpret_cast<DWORD64>(g_nativeMenuResume))||
        (g_nativeMenuShowLeft&&start==reinterpret_cast<DWORD64>(g_nativeMenuShowLeft))||
        (g_nativeMenuShowRight&&start==reinterpret_cast<DWORD64>(g_nativeMenuShowRight));
}
void ClearLeanDockMenuFocus(ClickThroughTaskbarState& state) noexcept {
    state.menuPointerAt=0;state.menuPlacementAt=0;
    try{if(auto flyout=state.menuFocusFlyout.get()){
        if(state.menuClosingToken.value)flyout.Closing(state.menuClosingToken);
        if(state.menuClosedToken.value)flyout.Closed(state.menuClosedToken);
    }}catch(...){}
    state.menuClosing.reset();state.menuClosingToken={};state.menuClosedToken={};
    try{if(auto flyout=state.menuFocusFlyout.get();flyout&&state.menuFocusOpened.value)flyout.Opened(state.menuFocusOpened);}catch(...){}
    try{if(auto item=state.menuFocusItem.get();item&&state.menuFocusGot.value)item.GotFocus(state.menuFocusGot);}catch(...){}
    state.menuFocusFlyout={};state.menuFocusOpened={};
    state.menuFocusItem={};state.menuFocusGot={};
}
void ClearLeanDockMenuReopen(ClickThroughTaskbarState& state) noexcept {
    try{if(auto flyout=state.menuReopenFlyout.get();flyout&&state.menuReopenClosed.value)flyout.Closed(state.menuReopenClosed);}catch(...){}
    state.menuReopenFlyout={};state.menuReopenClosed={};
    if(state.islandHwnd)for(auto key:{L"LeanDock.Menu.ReopenPending",L"LeanDock.Menu.ReopenAt"})
        RemovePropW(GetAncestor(state.islandHwnd,GA_ROOT),key);
}
LeanDockMenu::Reopen LeanDockMenu::ReopenVisible(const Gesture& next) {
    if(ReopenPending(next.taskbar,GetTickCount64()))return Reopen::Pending;
    for(auto& state:g_clickThroughTaskbarState){
        if(state.cardsClosing||!state.islandHwnd||GetAncestor(state.islandHwnd,GA_ROOT)!=next.taskbar)continue;
        try{
            auto menu=state.menuFocusFlyout.get();
            if(!menu||(!menu.IsOpen()&&(!state.menuClosing||!*state.menuClosing)))continue;
            auto frame=state.taskbarFrame.get();if(!frame||!frame.IsLoaded())return Reopen::Failed;
            ClearLeanDockMenuReopen(state);
            static std::atomic<ULONG_PTR> serial{0};
            const auto ticket=++serial;
            if(!SetPropW(next.taskbar,L"LeanDock.Menu.ReopenPending",reinterpret_cast<HANDLE>(ticket)))return Reopen::Failed;
            if(!SetPropW(next.taskbar,L"LeanDock.Menu.ReopenAt",reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(next.at)))){
                ClaimReopen(next.taskbar,ticket);return Reopen::Failed;
            }
            auto weakMenu=winrt::make_weak(menu);
            auto weakFrame=winrt::make_weak(frame);
            auto token=std::make_shared<winrt::event_token>();
            *token=menu.Closed([weakMenu,weakFrame,token,next,ticket](auto const&,auto const&) noexcept {
                try{
                    if(auto flyout=weakMenu.get())flyout.Closed(*token);
                    auto frame=weakFrame.get();
                    if(!frame||!frame.IsLoaded()||!Fresh(next,GetTickCount64())){ClaimReopen(next.taskbar,ticket);return;}
                    // Native Closed handlers finish first. This is one dispatcher
                    // turn, not a timer or a synthetic second click.
                    frame.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,[weakFrame,next,ticket]() noexcept {
                        if(!ClaimReopen(next.taskbar,ticket))return;
                        try{
                            auto frame=weakFrame.get();
                            if(!frame||!frame.IsLoaded()||!Fresh(next,GetTickCount64()))return;
                            // The consumed ticket deduplicates this real gesture;
                            // the previous opening's 200ms window must not drop it.
                            Put(next.taskbar,L"LeanDock.Menu.ReopenSubmitted",Submit(next,false));
                        }catch(...){Put(next.taskbar,L"LeanDock.Menu.ReopenError",static_cast<ULONG_PTR>(winrt::to_hresult()));}
                    });
                }catch(...){ClaimReopen(next.taskbar,ticket);Put(next.taskbar,L"LeanDock.Menu.ReopenError",static_cast<ULONG_PTR>(winrt::to_hresult()));}
            });
            state.menuReopenFlyout=weakMenu;state.menuReopenClosed=*token;
            Put(next.taskbar,L"LeanDock.Menu.ReopenCount",std::max<ULONG_PTR>(1,reinterpret_cast<ULONG_PTR>(GetPropW(next.taskbar,L"LeanDock.Menu.ReopenCount"))));
            if(menu.IsOpen())menu.Hide(); // a native light-dismiss may already be closing it
            return Reopen::Pending;
        }catch(...){
            ClearLeanDockMenuReopen(state);
            Put(next.taskbar,L"LeanDock.Menu.ReopenError",static_cast<ULONG_PTR>(winrt::to_hresult()));
            return Reopen::Failed;
        }
    }
    return Reopen::NotOpen;
}
bool SetLeanDockPointerFocus(Controls::Control const& item,HWND taskbar,ULONGLONG requested) noexcept {
    try{
        if(!item||!LeanDockMenu::Fresh({taskbar,{},requested},GetTickCount64())||
           !LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId())||item.FocusState()==FocusState::Unfocused)return false;
        LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusBefore",static_cast<ULONG_PTR>(item.FocusState()));
        if(item.FocusState()==FocusState::Keyboard||item.FocusState()==FocusState::Programmatic){
            const bool changed=item.Focus(FocusState::Pointer);
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.PointerFocus",changed&&item.FocusState()==FocusState::Pointer);
        }
        LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusAfter",static_cast<ULONG_PTR>(item.FocusState()));
        return item.FocusState()==FocusState::Pointer;
    }catch(...){LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusError",static_cast<ULONG_PTR>(winrt::to_hresult()));}
    return false;
}
void WINAPI ShowLeanDockFlyout(Controls::Primitives::FlyoutBase* value,const DependencyObject* target,const Controls::Primitives::FlyoutShowOptions* options) {
    if(value&&target&&IsLeanDockMenuCaller(__builtin_return_address(0)))try {
        auto menu=value->try_as<Controls::MenuFlyout>();
        auto element=target->try_as<FrameworkElement>();
        if(menu&&element)for(auto& state:g_clickThroughTaskbarState){
            auto frame=state.taskbarFrame.get();
            if(state.cardsClosing||!state.menuPlacementAt||!frame||!state.islandHwnd||
               frame.XamlRoot()!=element.XamlRoot())continue;
            const HWND taskbar=GetAncestor(state.islandHwnd,GA_ROOT);
            const auto requested=state.menuPlacementAt;
            const bool pointer=state.menuPointerAt!=0;
            ClearLeanDockMenuFocus(state); // consume before calling into XAML
            if(!LeanDockMenu::Fresh({taskbar,{},requested},GetTickCount64()))break;
            auto weak=winrt::make_weak(menu);
            // Keep the native flyout's lifecycle for both launch modes, so a
            // subsequent real card right-click can reopen a keyboard menu.
            // Keyboard calls retain native focus and accelerator hints.
            if(pointer){
                LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusItemCount",menu.Items().Size());
                for(auto const& item:menu.Items()){
                    auto control=item.try_as<Controls::Control>();
                    if(!control||!control.IsEnabled()||control.Visibility()!=Visibility::Visible)continue;
                    auto weakItem=winrt::make_weak(control);
                    auto focusToken=std::make_shared<winrt::event_token>();
                    *focusToken=control.GotFocus([weakItem,focusToken,taskbar,requested](auto const&,auto const&) noexcept {
                        try{if(auto item=weakItem.get()){
                            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusGot",reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.FocusGot")));
                            if(!LeanDockMenu::Fresh({taskbar,{},requested},GetTickCount64())||
                               SetLeanDockPointerFocus(item,taskbar,requested))item.GotFocus(*focusToken);
                        }}catch(...){}
                    });
                    state.menuFocusItem=weakItem;state.menuFocusGot=*focusToken;
                    break;
                }
                auto token=std::make_shared<winrt::event_token>();
                *token=menu.Opened([weak,token,taskbar,requested](auto const&,auto const&) noexcept {
                    try{
                        auto flyout=weak.get();if(!flyout)return;
                        LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusOpened",reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.FocusOpened")));
                        if(!LeanDockMenu::Fresh({taskbar,{},requested},GetTickCount64())){flyout.Opened(*token);return;}
                        // A windowed popup can have a distinct focus scope from
                        // the placement target. Inspect its own items directly.
                        for(auto const& item:flyout.Items())if(auto control=item.try_as<Controls::Control>();control&&control.FocusState()!=FocusState::Unfocused){
                            if(SetLeanDockPointerFocus(control,taskbar,requested))flyout.Opened(*token);
                            break;
                        }
                    }catch(...){LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusError",static_cast<ULONG_PTR>(winrt::to_hresult()));}
                });
                state.menuFocusOpened=*token;
                LeanDockMenu::Put(taskbar,L"LeanDock.Menu.FocusArmed",1);
            }
            state.menuFocusFlyout=weak;
            auto closing=std::make_shared<bool>(false);state.menuClosing=closing;
            state.menuClosingToken=menu.Closing([closing,taskbar](auto const&,auto const&) noexcept {
                *closing=true;LeanDockMenu::Put(taskbar,L"LeanDock.Menu.Closing",1);
            });
            state.menuClosedToken=menu.Closed([closing,taskbar](auto const&,auto const&) noexcept {
                *closing=false;LeanDockMenu::Put(taskbar,L"LeanDock.Menu.Closing",0);
            });
            break;
        }
    }catch(...){Wh_Log(L"LeanDock pointer menu focus unavailable: %08X",winrt::to_hresult());}
    g_nativeFlyoutShow(value,target,options);
}
unsigned char WINAPI ShowLeanDockNativeMenu(FrameworkElement* source,void* model,void* frameArg,void* coordinator) {
    for(auto& state:g_clickThroughTaskbarState){
        ClearLeanDockMenuReopen(state);ClearLeanDockMenuFocus(state);
        if(!state.cardsClosing&&state.islandHwnd){
            const HWND taskbar=GetAncestor(state.islandHwnd,GA_ROOT);
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.NativeCalls",std::max<ULONG_PTR>(1,reinterpret_cast<ULONG_PTR>(GetPropW(taskbar,L"LeanDock.Menu.NativeCalls"))));
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.NativeStage",0);
            for(auto key:{L"LeanDock.Menu.NativeAnchor",L"LeanDock.Menu.Invocation",L"LeanDock.Menu.Error",L"LeanDock.Menu.FocusArmed",L"LeanDock.Menu.PointerFocus",L"LeanDock.Menu.FocusBefore",L"LeanDock.Menu.FocusAfter",L"LeanDock.Menu.FocusError"})
                LeanDockMenu::Put(taskbar,key,0);
        }
    }
    LeanDockMenu::Gesture gesture;
    const bool pointer=LeanDockMenu::Take(gesture);
    HWND taskbar=nullptr;
    if(source&&*source)try {
        const auto sourceRoot=source->XamlRoot();
        if(pointer)LeanDockMenu::Put(gesture.taskbar,L"LeanDock.Menu.NativeStage",1);
        for(auto& state:g_clickThroughTaskbarState){
            if(state.cardsClosing||!state.islandHwnd)continue;
            const HWND candidate=GetAncestor(state.islandHwnd,GA_ROOT);
            if(pointer&&candidate!=gesture.taskbar)continue;
            auto frame=state.taskbarFrame.get();
            if(!frame||!frame.IsLoaded()||!sourceRoot||sourceRoot!=frame.XamlRoot())continue;
            taskbar=candidate;
            if(!LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))break;
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.Invocation",pointer?1:2);
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.NativeStage",3); // matching native XAML root
            if(Media::VisualTreeHelper::GetChildrenCount(frame)<1)break;
            auto root=Media::VisualTreeHelper::GetChild(frame,0).try_as<Controls::Grid>();if(!root)break;
            RECT dock{};MONITORINFO monitor{};
            if(!LeanDockGeometry::Read(taskbar,dock,&monitor))break;
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.NativeStage",4); // geometry ready
            const double scale=frame.XamlRoot().RasterizationScale();if(scale<=0)break;
            POINT point{};bool placed=false;
            // Over the key's center, wherever on the key the pointer was. The
            // shell selected the XAML root/monitor: resolve that root's current
            // card directly, never from old properties. Only a pointer gesture
            // without a usable card falls back to its own position.
            auto card=state.cards?state.cards->MenuTarget():FrameworkElement{nullptr};
            if(card&&card.XamlRoot()==sourceRoot){
                auto bounds=card.TransformToVisual(sourceRoot.Content()).TransformBounds(
                    {0,0,float(card.ActualWidth()),float(card.ActualHeight())});
                POINT origin{};
                if(std::isfinite(bounds.X)&&std::isfinite(bounds.Y)&&std::isfinite(bounds.Width)&&std::isfinite(bounds.Height)&&
                   ClientToScreen(state.islandHwnd,&origin)){
                    const RECT cardRect{origin.x+std::lround(bounds.X*scale),origin.y+std::lround(bounds.Y*scale),
                        origin.x+std::lround((bounds.X+bounds.Width)*scale),origin.y+std::lround((bounds.Y+bounds.Height)*scale)};
                    placed=LeanDockMenu::CardAnchor(cardRect,dock,GetDpiForWindow(taskbar),point);
                }
            }
            if(!placed){
                if(!pointer)break;
                point=LeanDockMenu::AnchorPoint(gesture.click,dock,GetDpiForWindow(taskbar));
            }
            const POINT screenPoint=point;
            if(!ScreenToClient(state.islandHwnd,&point))break;
            const winrt::Windows::Foundation::Point target{float(point.x/scale),float(point.y/scale)};
            auto anchor=state.menuAnchor.get();
            if(!anchor){
                anchor=Controls::Button();anchor.Name(L"LeanDockMenuAnchor");
                anchor.Width(1);anchor.Height(1);anchor.MinWidth(0);anchor.MinHeight(0);anchor.Padding({0,0,0,0});
                anchor.HorizontalAlignment(HorizontalAlignment::Left);anchor.VerticalAlignment(VerticalAlignment::Top);
                anchor.IsHitTestVisible(false);anchor.IsTabStop(false);anchor.Opacity(0);
                root.Children().Append(anchor);state.menuAnchor=winrt::make_weak(anchor);state.menuAnchorParent=winrt::make_weak(root);
            }
            // RootGrid's padding moves its children, so measure where the
            // anchor rests instead of assuming RootGrid's own corner.
            anchor.RenderTransform(nullptr);anchor.UpdateLayout();
            const auto rest=anchor.TransformToVisual(frame.XamlRoot().Content()).TransformPoint({0,0});
            const winrt::Windows::Foundation::Point local{target.X-rest.X,target.Y-rest.Y};
            Media::TranslateTransform offset;offset.X(local.X);offset.Y(local.Y);anchor.RenderTransform(offset);anchor.UpdateLayout();
            *source=anchor; // native coroutine now owns the correct XAML anchor
            state.menuPlacementAt=pointer?gesture.at:GetTickCount64();
            state.menuPointerAt=pointer?gesture.at:0;
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.NativeAnchor",1);
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.NativeStage",5); // anchor applied
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.AnchorX100",std::lround(local.X*100));
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.AnchorY100",std::lround(local.Y*100));
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.ScreenX",static_cast<ULONG_PTR>(static_cast<LONG_PTR>(screenPoint.x)));
            LeanDockMenu::Put(taskbar,L"LeanDock.Menu.ScreenY",static_cast<ULONG_PTR>(static_cast<LONG_PTR>(screenPoint.y)));
            break;
        }
    }catch(...){LeanDockMenu::Put(taskbar,L"LeanDock.Menu.Error",static_cast<ULONG_PTR>(winrt::to_hresult()));}
    return g_nativeDockMenu(source,model,frameArg,coordinator);
}
// 0 waiting for Taskbar.View.dll, 1 hooked, 2 not hooked. Reported in the backend
// status and required by the component health check.
std::atomic<unsigned> g_leanDockMenuHook{0};
// Explorer can load Taskbar.View.dll after the core started, so this runs again
// from the load hook and after init. Missed, the key's menu opens at the hidden
// native Start button. True when hooks were queued and need applying.
bool InstallLeanDockMenuHook() {
    static std::atomic<bool> claimed{false};
    if(claimed)return false;
    const HMODULE module=GetModuleHandleW(L"Taskbar.View.dll");
    if(!module||claimed.exchange(true))return false;
    const WindhawkUtils::SYMBOL_HOOK hooks[]={{
        {LR"(struct winrt::fire_and_forget __cdecl winrt::Taskbar::implementation::ContextMenus::ShowStartButtonContextMenuAsync(struct winrt::Windows::UI::Xaml::FrameworkElement,struct winrt::WindowsUdk::UI::Shell::TaskbarModel,struct winrt::Taskbar::TaskbarFrame,struct winrt::com_ptr<struct winrt::Taskbar::implementation::ViewCoordinator>))"},
        &g_nativeDockMenu,ShowLeanDockNativeMenu
    }};
    const bool placement=WindhawkUtils::HookSymbols(module,hooks,ARRAYSIZE(hooks));
    const WindhawkUtils::SYMBOL_HOOK focusHooks[]={
        {{LR"(winrt::Taskbar::implementation::ContextMenus::ShowStartButtonContextMenuAsync$_ResumeCoro$1)"},&g_nativeMenuResume,nullptr,true},
        {{LR"(`winrt::Taskbar::implementation::ContextMenus::ShowStartButtonContextMenuAsync'::`63'::<lambda_7>::operator()<winrt::Windows::Foundation::IInspectable const &,winrt::Windows::Foundation::IInspectable const &>)"},&g_nativeMenuShowLeft,nullptr,true},
        {{LR"(`winrt::Taskbar::implementation::ContextMenus::ShowStartButtonContextMenuAsync'::`68'::<lambda_8>::operator()<winrt::Windows::Foundation::IInspectable const &,winrt::Windows::Foundation::IInspectable const &>)"},&g_nativeMenuShowRight,nullptr,true},
        {{LR"(public: __cdecl winrt::impl::consume_Windows_UI_Xaml_Controls_Primitives_IFlyoutBase5<struct winrt::Windows::UI::Xaml::Controls::Primitives::FlyoutBase>::ShowAt(struct winrt::Windows::UI::Xaml::DependencyObject const &,struct winrt::Windows::UI::Xaml::Controls::Primitives::FlyoutShowOptions const &)const)"},&g_nativeFlyoutShow,ShowLeanDockFlyout,true}
    };
    const bool focus=WindhawkUtils::HookSymbols(module,focusHooks,ARRAYSIZE(focusHooks));
    g_leanDockMenuHook=placement&&focus?1:2;
    if(!placement||!focus)Wh_Log(L"LeanDock menu hook not installed: placement=%d focus=%d",placement,focus);
    return placement||focus;
}

using DockContextTarget=LeanDockContext::Target;
DockContextTarget ClassifyDockContext(DependencyObject node,FrameworkElement const& frame,
                                     FrameworkElement const& tray,HWND taskbar) {
    LeanDockContext::Classifier classifier;
    const bool nativePowerMapped=GetPropW(taskbar,L"LeanDock.Card.NativePowerHit")!=nullptr;
    for(unsigned depth=0;node&&depth<32;++depth,node=Media::VisualTreeHelper::GetParent(node)){
        auto fe=node.try_as<FrameworkElement>();if(!fe)continue;
        const auto name=fe.Name();
        const auto type=winrt::get_class_name(fe);
        const auto result=classifier.Visit(name,type,bool(fe.try_as<Controls::Control>()),
                                          fe==frame||fe==tray,nativePowerMapped);
        if(result!=DockContextTarget::Unknown)return result;
    }
    return DockContextTarget::Unknown;
}
void BindLeanDockContext(ClickThroughTaskbarState& state,FrameworkElement const& owner,
                         winrt::weak_ref<Controls::Grid>& saved,UIElement::ContextRequested_revoker& subscription) {
    if(!owner||Media::VisualTreeHelper::GetChildrenCount(owner)<1)return;
    auto root=Media::VisualTreeHelper::GetChild(owner,0).try_as<Controls::Grid>();
    if(!root||!root.IsLoaded()||saved.get()==root)return;
    subscription.revoke();saved={};
    const HWND taskbar=GetAncestor(state.islandHwnd,GA_ROOT);
    auto frame=state.taskbarFrame,tray=state.systemTrayFrame;
    // Child RootGrid runs before the native ContextRequested handler on the
    // frame. A late handler on the shared XamlRoot could run after it opens.
    subscription=root.ContextRequested(winrt::auto_revoke,[frame,tray,taskbar](UIElement const& sender,Input::ContextRequestedEventArgs const& e) noexcept {
        try{
            winrt::Windows::Foundation::Point point{};
            if(e.Handled()||!e.TryGetPosition(sender,point)||
               !LeanDockShell::IsTaskbar(taskbar,GetCurrentProcessId()))return;
            const auto target=ClassifyDockContext(e.OriginalSource().try_as<DependencyObject>(),frame.get(),tray.get(),taskbar);
            if(LeanDockContext::Route(target)==LeanDockContext::Action::Pass)return;
            e.Handled(true); // ownership does not depend on SendInput succeeding
            LeanDockMenu::HandleContext(taskbar,target,target==DockContextTarget::Card?
                LeanDockMenu::ContextSource::XamlCard:LeanDockMenu::ContextSource::XamlBackground);
        }catch(...){LeanDockMenu::Put(taskbar,L"LeanDock.Menu.ContextError",static_cast<ULONG_PTR>(winrt::to_hresult()));}
    });
    saved=winrt::make_weak(root);
}

#include "material-latest.inc"

// Own the material, native context routes and aggregate key. The compact key
// replaces the old dashboard; no decorative separator remains beside it.
void UpdateLeanDockSurface(ClickThroughTaskbarState& state) try {
    if(state.cardsClosing)return;
    auto frame=state.taskbarFrame.get();
    if(!frame || !state.systemTrayFrame.get())return;
    if(Media::VisualTreeHelper::GetChildrenCount(frame)<1)return;
    auto root=Media::VisualTreeHelper::GetChild(frame,0).try_as<Controls::Grid>();
    if(!root || !root.IsLoaded())return;
    UpdateLeanDockMaterial(state,root);
    BindLeanDockContext(state,frame,state.contextRoot,state.contextRequested);
    BindLeanDockContext(state,state.systemTrayFrame.get(),state.contextTrayRoot,state.contextTrayRequested);
    if(!state.cards || !state.cards->Alive())state.cards=LeanDockCards::Create(root);
    if(state.cards && state.islandHwnd)state.cards->Update(
        GetAncestor(state.islandHwnd,GA_ROOT),frame.XamlRoot().Size().Width,
        state.systemTrayFrame.get().ActualWidth());
} catch(...) { Wh_Log(L"LeanDock surface waiting for a stable visual tree"); }
void ClearLeanDockSurface(ClickThroughTaskbarState& state) {
    ClearLeanDockMaterial(state);
    state.contextRequested.revoke();state.contextTrayRequested.revoke();
    state.contextRoot={};state.contextTrayRoot={};
    ClearLeanDockMenuReopen(state);
    if(state.islandHwnd)for(auto key:{L"LeanDock.Menu.ContextSource",L"LeanDock.Menu.ContextCount",L"LeanDock.Menu.ContextSubmitted",L"LeanDock.Menu.ContextError",L"LeanDock.Menu.ContextTarget",L"LeanDock.Menu.BlankSuppressed",L"LeanDock.Menu.ReopenCount",L"LeanDock.Menu.ReopenSubmitted",L"LeanDock.Menu.ReopenError",L"LeanDock.Menu.NativeCalls",L"LeanDock.Menu.NativeStage",L"LeanDock.Menu.Invocation",L"LeanDock.Menu.ScreenX",L"LeanDock.Menu.ScreenY"})
        RemovePropW(GetAncestor(state.islandHwnd,GA_ROOT),key);
    LeanDockMenu::Cancel();
    ClearLeanDockMenuFocus(state);
    auto anchor=state.menuAnchor.get();auto anchorParent=state.menuAnchorParent.get();
    if(anchor&&anchorParent){uint32_t index=0;if(anchorParent.Children().IndexOf(anchor,index))anchorParent.Children().RemoveAt(index);}
    state.menuAnchor={};state.menuAnchorParent={};
    state.cardsClosing=true;
    if(state.cards){state.cards->Stop();state.cards.reset();}
}
void LeanDockTimeChanged(HWND taskbar) {
    for(auto& state:g_clickThroughTaskbarState)if(!state.cardsClosing&&state.cards&&state.islandHwnd&&
        GetAncestor(state.islandHwnd,GA_ROOT)==taskbar)state.cards->TimeChanged();
}
void LeanDockPreferencesChanged(HWND taskbar) {
    for(auto& state:g_clickThroughTaskbarState)if(!state.cardsClosing&&state.cards&&state.islandHwnd&&
        GetAncestor(state.islandHwnd,GA_ROOT)==taskbar)state.cards->ReloadPreferences();
}

// Cards alone open Win+X. Empty background is consumed without a menu.
// Application jump lists and notification icons keep their native routes.
bool HandleLeanDockContext(HWND window,LPARAM position) try {
    if(position==LPARAM(-1))return false; // leave keyboard focus handling native
    for(auto& state:g_clickThroughTaskbarState){
        if(state.cardsClosing||!state.islandHwnd||GetAncestor(state.islandHwnd,GA_ROOT)!=window)continue;
        auto frame=state.taskbarFrame.get();if(!frame)return false;
        POINT pt{static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};
        if(!ScreenToClient(state.islandHwnd,&pt))return false;
        const double scale=frame.XamlRoot().RasterizationScale();if(scale<=0)return false;
        auto content=frame.XamlRoot().Content();
        const auto hits=Media::VisualTreeHelper::FindElementsInHostCoordinates(
            winrt::Windows::Foundation::Point{float(pt.x/scale),float(pt.y/scale)},content,false).First();
        if(!hits.HasCurrent())return false;
        const auto target=ClassifyDockContext(hits.Current(),frame,state.systemTrayFrame.get(),window);
        return LeanDockMenu::HandleContext(window,target,LeanDockMenu::ContextSource::WindowMessage);
    }
    return false;
}catch(...){return false;}
