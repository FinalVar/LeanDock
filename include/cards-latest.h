#pragma once
#include <memory>
#include <windows.h>
#undef GetCurrentTime
#include <winrt/Windows.UI.Xaml.Controls.h>
namespace LeanDockCards {
struct Host {
    virtual ~Host()=default;
    virtual void Update(HWND taskbar,double available,double tray)=0;
    virtual void TimeChanged() noexcept=0;
    virtual void Stop() noexcept=0;
    virtual bool Alive()const noexcept=0;
};
// Called only on the owning taskbar's XAML thread. The returned object owns
// its UI, subscriptions and reversible padding/background overrides.
std::shared_ptr<Host> Create(winrt::Windows::UI::Xaml::Controls::Grid const& root);
}
