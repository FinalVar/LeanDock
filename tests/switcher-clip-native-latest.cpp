// Native XAML COM-peer lifetime regression. Creates no window or visual host.
// Exercises the production retain/release helpers, not live switcher gestures.
#include <windows.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <cstdio>
#include <stdexcept>
#include "../include/switcher-clip-policy-latest.h"
using namespace winrt::Windows::UI::Xaml;
// Admission is intentionally disabled in this no-host fixture. The actual
// production state/identity/release helpers are compiled below unchanged.
bool g_initializedForThread=false;
struct LayoutState {bool disabled{};struct {bool valid{};} plan;winrt::weak_ref<FrameworkElement> panel;};
FrameworkElement FindNamed(FrameworkElement const&,PCWSTR,unsigned&){return nullptr;}
#include "../src/switcher-clip-latest.inc"
static unsigned checks;
static void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);try{
 winrt::init_apartment(winrt::apartment_type::single_threaded);
 auto manager=Hosting::WindowsXamlManager::InitializeForCurrentThread();
 {
  Controls::ScrollContentPresenter presenter;
  Media::RectangleGeometry original;original.Rect({0,0,1081.33f,816});presenter.Clip(original);
  check(!original.Transform(),"native geometry default has no transform");
  auto oldWeak=winrt::make_weak(original);original=nullptr;
  check(!oldWeak.get(),"reproduced weak peer expiry despite native clip ownership");
  ClipSyncState s;
  auto native=presenter.Clip().as<Media::RectangleGeometry>();
  RememberPublicClip(s,presenter,native);
  const NativeSwitcherClip::Size before{1081.33,816},after{1532,528};
  auto r=native.Rect();
  check(s.sizes.Observe({r.X,r.Y,r.Width,r.Height},before,before)==NativeSwitcherClip::Action::Synchronized,"prior viewport seeded");
  auto held=winrt::make_weak(native);native=nullptr;
  check(bool(held.get()),"production holder retains native peer");
  native=presenter.Clip().as<Media::RectangleGeometry>();
  check(native==s.geometry,"native reads preserve retained identity");
  RememberPublicClip(s,presenter,native);
  check(s.sizes.known,"repeated read does not erase viewport history");
  r=native.Rect();
  check(s.sizes.Observe({r.X,r.Y,r.Width,r.Height},after,after)==NativeSwitcherClip::Action::RepairPreviousSize,"real native stale rectangle now admitted");
  native.Rect({0,0,1532,528});r=native.Rect();
  check(s.sizes.Observe({r.X,r.Y,r.Width,r.Height},after,after)==NativeSwitcherClip::Action::Synchronized,"native rectangle write and readback");
  check(presenter.Clip()==native,"native property object never replaced");
  Media::RectangleGeometry replacement;replacement.Rect({0,0,300,200});presenter.Clip(replacement);
  RememberPublicClip(s,presenter,replacement);
  check(!s.sizes.known,"native replacement loses old ownership evidence");
  r=replacement.Rect();
  check(s.sizes.Observe({r.X,r.Y,r.Width,r.Height},after,after)==NativeSwitcherClip::Action::Ignore,"unknown replacement not expanded");
  auto retainedPresenter=winrt::make_weak(presenter);
  auto retainedGeometry=winrt::make_weak(replacement);
  native=nullptr;replacement=nullptr;presenter=nullptr;
  check(bool(retainedPresenter.get())&&bool(retainedGeometry.get()),"visible-state references retained");
  ReleasePublicClip(s);
  check(!s.presenter&&!s.geometry&&!s.sizes.known,"hide/unload clears retained state");
  check(!retainedPresenter.get()&&!retainedGeometry.get(),"native peers released, no retained host");
  ReleasePublicClip(s);check(!s.presenter&&!s.geometry,"repeated cleanup safe");
 }
 std::printf("PASS: %u native XAML clip lifetime checks; no desktop window/input/focus.\n",checks);
 manager.Close();return 0;
}catch(winrt::hresult_error const& e){std::printf("HRESULT %08x %s\n",unsigned(e.code()),winrt::to_string(e.message()).c_str());return 1;}
 catch(std::exception const& e){std::printf("FAIL %s\n",e.what());return 1;}}
