// Native MainStack joins the existing app tray. Keep native models, parents,
// visibility decisions, tooltips and click/context handlers. No status monitor
// or synthesized privacy indicator is introduced.
#include <winrt/Windows.UI.Xaml.Automation.h>

bool DockExcludedInput(FrameworkElement const& element,unsigned depth=0) {
    if(!element||depth>14)return false;
    const auto type=winrt::get_class_name(element);
    if(type==L"SystemTray.InkWorkspaceButton"||type==L"SystemTray.InputIndicatorButton"||
       type==L"SystemTray.IMEButton"||type==L"SystemTray.LanguageTextIconContent"||
       type==L"SystemTray.LanguageImageIconContent")return true;
    // These model class names are present in the pinned SystemTray.dll. Unlike
    // localized accessible names, they don't depend on Windows display language.
    if(auto model=element.DataContext()){
        const auto modelType=winrt::get_class_name(model);
        if(modelType==L"SystemTray.TouchKeyboardSystemTrayIconDataModel"||
           modelType==L"SystemTray.InkWorkspaceSystemTrayIconDataModel"||
           modelType==L"SystemTray.VirtualTouchpadSystemTrayIconDataModel"||
           modelType==L"SystemTray.EmojiAndMoreSystemTrayIconDataModel"||
           modelType==L"SystemTray.LanguageSystemTrayIconDataModel"||
           modelType==L"SystemTray.ImeSystemTrayIconDataModel")return true;
    }
    bool excluded=false;
    EnumChildElements(element,[&](FrameworkElement child){
        excluded=DockExcludedInput(child,depth+1);return excluded;
    });
    return excluded;
}

void DockRestoreProperty(FrameworkElement const& element,DependencyProperty const& property) {
    auto& saved=g_dockUnified->saved;
    const auto it=std::find_if(saved.begin(),saved.end(),[&](auto const& entry){
        return entry.element.get()==element&&entry.property==property;
    });
    if(it==saved.end())return;
    if(it->binding)Data::BindingOperations::SetBinding(element,property,it->binding);
    else if(it->value==DependencyProperty::UnsetValue())element.ClearValue(property);
    else element.SetValue(property,it->value);
    saved.erase(it);
}

DockGroup DockCollectSystem(FrameworkElement const& root) {
    DockGroup result;
    result.container=FindChildByName(root,L"MainStack");
    if(!result.container)return result;
    auto content=FindChildByName(result.container,L"Content");
    auto items=content?FindChildByName(content,L"IconStack"):nullptr;
    result.panel=DockItemsPanel(items);
    if(!result.panel)return result;
    EnumChildElements(result.panel,[&](FrameworkElement child){
        auto icon=FindChildByName(child,L"SystemTrayIcon");
        if(!icon)return false;
        if(DockExcludedInput(icon)){
            DockSet(child,UIElement::VisibilityProperty(),Visibility::Collapsed);
            return false;
        }
        // A native presenter can be reused with a different model. Release only
        // our exclusion before respecting Windows' current visibility binding.
        DockRestoreProperty(child,UIElement::VisibilityProperty());
        if(result.container.Visibility()==Visibility::Visible&&
           child.Visibility()==Visibility::Visible&&icon.Visibility()==Visibility::Visible){
            result.presenters.push_back(child);result.icons.push_back(icon);
        }
        return false;
    });
    return result;
}

void DockCenterFinal(DockGroup& group,int before,int total,double cell,double pitch,double row){
    if(!(total%2)||group.icons.empty()||before+int(group.icons.size())!=total)return;
    const int i=int(group.icons.size())-1;
    DockTranslate(group.presenters[i],pitch*DockGrid::Column(before,i)-cell*i+(pitch-cell)/2,
                  row*DockGrid::RowOffset(before,i,total));
}

std::wstring DockSystemTypes(FrameworkElement const& element,unsigned depth=0){
    if(!element||depth>14)return L"";
    std::wstring text=std::wstring(winrt::get_class_name(element));
    if(auto model=element.DataContext())text+=L"("+std::wstring(winrt::get_class_name(model))+L")";
    // Class names only: don't persist app names, permission users or tooltips.
    EnumChildElements(element,[&](FrameworkElement child){
        text+=L"/"+DockSystemTypes(child,depth+1);return false;
    });
    return text;
}
