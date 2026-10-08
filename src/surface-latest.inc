// The material uses the existing backdrop composition brush. This hairline
// follows NextKDE DockDivider's centered, short-line proportions.
void UpdateLeanDockDivider(ClickThroughTaskbarState& state) try {
    if(state.cardsClosing)return;
    auto frame=state.taskbarFrame.get();
    if(!frame || !state.systemTrayFrame.get())return;
    if(Media::VisualTreeHelper::GetChildrenCount(frame)<1)return;
    auto root=Media::VisualTreeHelper::GetChild(frame,0).try_as<Controls::Grid>();
    if(!root || !root.IsLoaded())return;
    if(!state.cards || !state.cards->Alive())state.cards=LeanDockCards::Create(root);
    if(state.cards && state.islandHwnd)state.cards->Update(
        GetAncestor(state.islandHwnd,GA_ROOT),frame.XamlRoot().Size().Width,
        state.systemTrayFrame.get().ActualWidth());
    if(state.divider.get())return;
    Shapes::Rectangle line;
    line.Name(L"LeanDockDivider");line.Width(1);line.Height(40);
    line.RadiusX(0.5);line.RadiusY(0.5);
    line.HorizontalAlignment(HorizontalAlignment::Right);
    line.VerticalAlignment(VerticalAlignment::Center);
    line.Margin(Thickness{0,0,2,0});line.IsHitTestVisible(false);
    Media::TranslateTransform offset;offset.X(12);line.RenderTransform(offset);
    Media::LinearGradientBrush brush;
    brush.StartPoint({0.5f,0});brush.EndPoint({0.5f,1});
    for(auto [offset,alpha]:{std::pair{0.0,16},std::pair{0.18,70},std::pair{0.82,70},std::pair{1.0,16}}) {
        Media::GradientStop stop;stop.Offset(offset);
        stop.Color(winrt::Windows::UI::Color{static_cast<uint8_t>(alpha),100,110,125});
        brush.GradientStops().Append(stop);
    }
    line.Fill(brush);Controls::Canvas::SetZIndex(line,10);
    root.Children().Append(line);state.divider=winrt::make_weak(line);
    state.dividerParent=winrt::make_weak(root);
} catch(...) { Wh_Log(L"LeanDock divider waiting for a stable visual tree"); }
void ClearLeanDockDivider(ClickThroughTaskbarState& state) {
    LeanDockMenu::Cancel();
    state.cardsClosing=true;
    if(state.cards){state.cards->Stop();state.cards.reset();}
    auto parent=state.dividerParent.get();auto line=state.divider.get();
    if(parent && line){uint32_t index=0;if(parent.Children().IndexOf(line,index))parent.Children().RemoveAt(index);}
    state.divider={};state.dividerParent={};
}

// Intercept only the taskbar's background context menu. Application jump
// lists and notification icons retain their native, separate right-clicks.
bool HandleLeanDockContext(HWND window,LPARAM position) try {
    if(position==LPARAM(-1))return false; // leave keyboard focus handling native
    for(auto& state:g_clickThroughTaskbarState){
        if(state.cardsClosing||!state.islandHwnd||GetAncestor(state.islandHwnd,GA_ROOT)!=window)continue;
        auto frame=state.taskbarFrame.get();if(!frame)return false;
        POINT pt{static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};
        if(!ScreenToClient(state.islandHwnd,&pt))return false;
        const double scale=frame.XamlRoot().RasterizationScale();if(scale<=0)return false;
        auto content=frame.XamlRoot().Content();bool inDock=false;
        const auto hits=Media::VisualTreeHelper::FindElementsInHostCoordinates(
            winrt::Windows::Foundation::Point{float(pt.x/scale),float(pt.y/scale)},content,true);
        for(auto const& hit:hits){
            bool nativeControl=false;
            DependencyObject node=hit;
            for(unsigned depth=0;node&&depth<32;++depth,node=Media::VisualTreeHelper::GetParent(node)){
                auto fe=node.try_as<FrameworkElement>();if(!fe)continue;
                if(fe.Name()==L"LeanDockCard"||fe.Name()==L"LeanDockPowerCard"||
                   (fe.Name()==L"ControlCenterButton"&&GetPropW(window,L"LeanDock.Card.NativePowerHit")))
                    return LeanDockMenu::Request(window);
                const auto type=winrt::get_class_name(fe);
                if(fe.try_as<Controls::Primitives::ButtonBase>()||
                   type==L"SystemTray.NotifyIconView"||fe.Name()==L"NotifyItemIcon")nativeControl=true;
                if(fe==frame||fe==state.systemTrayFrame.get())inDock=true;
            }
            if(nativeControl)return false;
        }
        if(inDock)return LeanDockMenu::Request(window);
    }
    return false;
}catch(...){return false;}
