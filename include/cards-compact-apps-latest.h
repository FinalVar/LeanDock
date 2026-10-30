#pragma once
#include "cards-dashboard-latest.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Media.h>

namespace LeanDockCards {
namespace dx=winrt::Windows::UI::Xaml;
// Uses the same binding-preserving property ownership as the native tray
// adapter. Never reparents task buttons or replaces their input/bitmap source.
class CompactApps {
    struct Saved {
        winrt::weak_ref<dx::FrameworkElement> element;
        dx::DependencyProperty property{nullptr};
        winrt::Windows::Foundation::IInspectable value{nullptr};
        dx::Data::Binding binding{nullptr};
    };
    std::vector<Saved> saved;
    double width=Dashboard::NormalAppWidth;
    void Set(dx::FrameworkElement const& element,dx::DependencyProperty const& property,double value) {
        auto existing=winrt::unbox_value<double>(element.GetValue(property));
        if(existing==value||(std::isnan(existing)&&std::isnan(value)))return;
        auto found=std::find_if(saved.begin(),saved.end(),[&](auto const& s){return s.element.get()==element&&s.property==property;});
        if(found==saved.end()){
            Saved s{winrt::make_weak(element),property,element.ReadLocalValue(property),nullptr};
            if(auto expression=element.GetBindingExpression(property))s.binding=expression.ParentBinding();
            saved.push_back(std::move(s));
        }
        element.SetValue(property,winrt::box_value(value));
    }
    static void Buttons(dx::DependencyObject const& node,std::vector<dx::FrameworkElement>& out,unsigned& budget,unsigned depth=0) {
        if(!node||depth>16||!budget)return;--budget;
        if(auto fe=node.try_as<dx::FrameworkElement>()){
            if(fe.Visibility()!=dx::Visibility::Visible)return;
            const auto type=winrt::get_class_name(fe);
            if(type==L"Taskbar.TaskListButton"){out.push_back(fe);return;}
        }
        const int n=dx::Media::VisualTreeHelper::GetChildrenCount(node);
        for(int i=0;i<n&&budget;++i)Buttons(dx::Media::VisualTreeHelper::GetChild(node,i),out,budget,depth+1);
    }
    void Parts(dx::DependencyObject const& node,double extent,unsigned& budget,unsigned depth=0) {
        if(!node||depth>16||!budget)return;--budget;
        if(auto fe=node.try_as<dx::FrameworkElement>()){
            const auto name=fe.Name();const auto type=winrt::get_class_name(fe);
            if(name==L"IconPanel"||type==L"Taskbar.TaskListLabeledButtonPanel"||type==L"Taskbar.TaskListButtonPanel"){
                Set(fe,dx::FrameworkElement::MinWidthProperty(),0);Set(fe,dx::FrameworkElement::WidthProperty(),extent);
            }else if(name==L"Icon"&&type==L"Windows.UI.Xaml.Controls.Image"){
                const auto size=40.0*extent/Dashboard::NormalAppWidth;
                Set(fe,dx::FrameworkElement::WidthProperty(),size);Set(fe,dx::FrameworkElement::HeightProperty(),size);
            }else if(name==L"BackgroundElement"){
                const auto size=48.0*extent/Dashboard::NormalAppWidth;
                Set(fe,dx::FrameworkElement::WidthProperty(),size);Set(fe,dx::FrameworkElement::HeightProperty(),size);
            }
        }
        const int n=dx::Media::VisualTreeHelper::GetChildrenCount(node);
        for(int i=0;i<n&&budget;++i)Parts(dx::Media::VisualTreeHelper::GetChild(node,i),extent,budget,depth+1);
    }
public:
    double Width()const{return width;}
    void Restore() noexcept {
        for(auto it=saved.rbegin();it!=saved.rend();++it)try{if(auto e=it->element.get()){
            if(it->binding)dx::Data::BindingOperations::SetBinding(e,it->property,it->binding);
            else if(it->value==dx::DependencyProperty::UnsetValue())e.ClearValue(it->property);
            else e.SetValue(it->property,it->value);
        }}catch(...){}
        saved.clear();width=Dashboard::NormalAppWidth;
    }
    void Apply(dx::FrameworkElement const& repeater,double budget) {
        if(!std::isfinite(budget)||budget<Dashboard::MinimumAppWidth){Restore();return;}
        const double measured=std::max(double(repeater.ActualWidth()),double(repeater.DesiredSize().Width));
        // Most updates are just text samples. Do not walk task buttons when
        // their unchanged normal-size layout already fits the available slot.
        if(width==Dashboard::NormalAppWidth&&measured<=budget){Restore();return;}
        unsigned limit=1024;std::vector<dx::FrameworkElement> buttons;Buttons(repeater,buttons,limit);
        if(!limit||buttons.empty()){Restore();return;}
        const double natural=std::max(measured+buttons.size()*(Dashboard::NormalAppWidth-width),buttons.size()*Dashboard::NormalAppWidth);
        const double next=Dashboard::AppWidth(budget,natural,unsigned(buttons.size()),width);
        if(next==Dashboard::NormalAppWidth){Restore();return;}
        // Existing native overflow handles counts beyond the minimum size.
        Set(repeater,dx::FrameworkElement::MaxWidthProperty(),budget);
        for(auto const& button:buttons){
            Set(button,dx::FrameworkElement::MinWidthProperty(),0);
            Set(button,dx::FrameworkElement::WidthProperty(),next);
            Set(button,dx::FrameworkElement::MaxWidthProperty(),next);
            unsigned partBudget=128;Parts(button,next,partBudget);
        }
        width=next;
        std::erase_if(saved,[](auto const& item){return !item.element.get();});
    }
};
}
