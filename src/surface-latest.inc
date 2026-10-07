// The material uses the existing backdrop composition brush. This hairline
// follows NextKDE DockDivider's centered, short-line proportions.
void UpdateLeanDockDivider(ClickThroughTaskbarState& state) try {
    auto frame=state.taskbarFrame.get();
    if(!frame || !state.systemTrayFrame.get())return;
    if(Media::VisualTreeHelper::GetChildrenCount(frame)<1)return;
    auto root=Media::VisualTreeHelper::GetChild(frame,0).try_as<Controls::Grid>();
    if(!root)return;
    if(state.divider.get())return;
    Shapes::Rectangle line;
    line.Name(L"LeanDockDivider");line.Width(1);line.Height(28);
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
    auto parent=state.dividerParent.get();auto line=state.divider.get();
    if(parent && line){uint32_t index=0;if(parent.Children().IndexOf(line,index))parent.Children().RemoveAt(index);}
    state.divider={};state.dividerParent={};
}
