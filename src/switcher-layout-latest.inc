// Adapt only typed RowHeight objects. Never write a boxed private layout enum.

struct LayoutState {
    winrt::weak_ref<FrameworkElement> panel;
    DependencyProperty minProperty{nullptr},maxProperty{nullptr};
    winrt::Windows::Foundation::IInspectable oldMin{nullptr},oldMax{nullptr},minValue{nullptr},maxValue{nullptr};
    std::wstring signature;
    NativeSwitcherLayout::Plan plan;
    bool writing{},disabled{};
    unsigned repairs{};
    long error{};
};

void RestoreLayout(LayoutState& s) noexcept {
    try {if(auto panel=s.panel.get()) {
        s.writing=true;
        auto restore=[&](auto const& property,auto const& applied,auto const& original) {
            if(property && applied && panel.GetValue(property)==applied) {
                if(original==DependencyProperty::UnsetValue()) panel.ClearValue(property);
                else panel.SetValue(property,original);
            }
        };
        restore(s.minProperty,s.minValue,s.oldMin);
        restore(s.maxProperty,s.maxValue,s.oldMax);
    }} catch(...) {}
    s.writing=false;s.panel={};s.minProperty=nullptr;s.maxProperty=nullptr;
    s.oldMin=nullptr;s.oldMax=nullptr;s.minValue=nullptr;s.maxValue=nullptr;
}

FrameworkElement FindFlowPanel(FrameworkElement element) {
    for(unsigned depth=0;element && depth<16;++depth) {
        if(winrt::get_class_name(element)==L"WindowsInternal.ComposableShell.Experiences.Switcher.DynamicFlowPanel") return element;
        element=Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
    }
    return nullptr;
}

void AdaptLayout(FrameworkElement const& root,LayoutState& s) noexcept {
    if(s.writing || s.disabled || !g_initializedForThread) return;
    try {
        if(!root || !root.IsLoaded() || root.Visibility()!=Visibility::Visible ||
           !root.XamlRoot() || !root.XamlRoot().IsHostVisible()) return;
        HIGHCONTRASTW hc{sizeof(hc)};
        if(!SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0) ||
           (hc.dwFlags&HCF_HIGHCONTRASTON)) {RestoreLayout(s);return;}
        unsigned budget=96;
        auto listElement=FindNamed(root,L"SwitchItemListControl",budget);
        auto list=listElement?listElement.try_as<Controls::ListView>():nullptr;
        if(!list || root.ActualWidth()<=0 || root.ActualHeight()<=0) return;
        const auto count=list.Items().Size();
        if(!count || count>128) {RestoreLayout(s);s.signature.clear();s.plan={};return;}
        auto first=list.ContainerFromIndex(0).try_as<FrameworkElement>();
        auto panel=first?FindFlowPanel(first):nullptr;
        if(!panel) return;
        // Outer grid is only an available-area ceiling. Native FitToContent
        // sizes the actual canvas around its arranged content.
        const double availableWidth=std::floor(root.ActualWidth()*0.92-32);
        const double availableHeight=std::floor(root.ActualHeight()*0.84-32);
        if(availableWidth<120 || availableHeight<100) return;
        std::vector<NativeSwitcherLayout::Item> items;
        items.reserve(count);
        std::wstring key=std::to_wstring(count)+L":"+std::to_wstring(int(availableWidth))+L":"+std::to_wstring(int(availableHeight));
        for(unsigned i=0;i<count;++i) {
            auto item=list.ContainerFromIndex(i).try_as<FrameworkElement>();
            if(!item || !item.IsLoaded()) return;
            budget=96;
            auto thumbnail=FindNamed(item,L"ThumbnailHost",budget);
            if(!thumbnail || thumbnail.ActualWidth()<=0 || thumbnail.ActualHeight()<=0) return;
            const double aspect=thumbnail.ActualWidth()/thumbnail.ActualHeight();
            const double chromeHeight=std::max(0.0,item.ActualHeight()-thumbnail.ActualHeight());
            const double chromeWidth=std::max(0.0,item.ActualWidth()-thumbnail.ActualWidth());
            if(!std::isfinite(aspect) || aspect<0.05 || aspect>20 || chromeHeight>100 || chromeWidth>100) return;
            // Round measurement noise, not the content: native thumbnails
            // continue using the original window dimensions and proportions.
            const auto a=std::lround(aspect*50),h=std::lround(chromeHeight),w=std::lround(chromeWidth);
            items.push_back({a/50.0,double(h),double(w)});
            key+=L":"+std::to_wstring(a)+L","+std::to_wstring(h)+L","+std::to_wstring(w);
        }
        if(auto old=s.panel.get();old && old!=panel) {RestoreLayout(s);s.signature.clear();}
        double ceiling=NativeSwitcherLayout::ComfortHeight(availableHeight);
        if(key==s.signature) {
            budget=32;
            auto element=FindNamed(listElement,L"ScrollViewer",budget);
            auto scroll=element?element.try_as<Controls::ScrollViewer>():nullptr;
            // A finite repair budget prevents a layout event feedback loop.
            // Do not hide scrollbars to turn overflow into apparent success.
            if(!scroll || scroll.ScrollableHeight()<=1 || !s.plan.valid) return;
            if(s.repairs>=3) {RestoreLayout(s);s.disabled=true;s.error=HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);return;}
            ceiling=s.plan.cardHeight-NativeSwitcherLayout::kHeightStep;++s.repairs;
        } else {s.repairs=0;}
        auto plan=NativeSwitcherLayout::Choose(availableWidth,availableHeight,items,ceiling);
        if(!plan.valid) {RestoreLayout(s);s.signature=key;s.plan={};return;}
        const int minHeight=int(plan.cardHeight),maxHeight=minHeight+int(NativeSwitcherLayout::kGap);
        auto name=winrt::get_class_name(panel);
        std::wstring setters=L"<Setter Property=\"MinRowHeight\"><Setter.Value><s:RowHeight xmlns:s=\"using:WindowsInternal.ComposableShell.Experiences.Switcher\" Ratio=\"0\" Pixels=\""+
            std::to_wstring(minHeight)+L"\" /></Setter.Value></Setter>"+
            L"<Setter Property=\"MaxRowHeight\"><Setter.Value><s:RowHeight xmlns:s=\"using:WindowsInternal.ComposableShell.Experiences.Switcher\" Ratio=\"0\" Pixels=\""+
            std::to_wstring(maxHeight)+L"\" /></Setter.Value></Setter>";
        auto style=GetStyleFromXamlSetters(name,setters);
        auto minSetter=style.Setters().GetAt(0).as<Setter>(),maxSetter=style.Setters().GetAt(1).as<Setter>();
        if(!s.panel.get()) {
            s.panel=panel;s.minProperty=minSetter.Property();s.maxProperty=maxSetter.Property();
            s.oldMin=panel.ReadLocalValue(s.minProperty);s.oldMax=panel.ReadLocalValue(s.maxProperty);
        } else if((s.minValue && panel.GetValue(s.minProperty)!=s.minValue) ||
                  (s.maxValue && panel.GetValue(s.maxProperty)!=s.maxValue)) {
            // A native owner has replaced a property. Restore only what is
            // still ours, then relinquish this panel for the rest of its life.
            RestoreLayout(s);s.disabled=true;return;
        }
        s.signature=key;s.plan=plan;s.writing=true;
        s.maxValue=maxSetter.Value();panel.SetValue(s.maxProperty,s.maxValue);
        s.minValue=minSetter.Value();panel.SetValue(s.minProperty,s.minValue);
        s.writing=false;
        Record(panel);
    } catch(winrt::hresult_error const& e) {s.error=e.code();RestoreLayout(s);s.disabled=true;}
      catch(...) {s.error=E_FAIL;RestoreLayout(s);s.disabled=true;}
}
