// LeanDock native Start layout, GPL-3.0.
// Group flattening reference: Asteski's Compact Start Menu 1.0.0,
// windhawk-mods commit 5b32bb24425ef9af3e7769665f667b8144352a20.
// This implementation uses collection/window events rather than a polling timer.
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <memory>
#include <vector>
#include <algorithm>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Media.h>
namespace f=winrt::Windows::Foundation;
namespace fc=f::Collections;
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace d=x::Data;
namespace core=winrt::Windows::UI::Core;

static HMODULE module;
static UINT Signal(){static UINT id=RegisterWindowMessageW(L"LeanDock.StartLayout.v1");return id;}
static constexpr WPARAM Enable=1,Disable=2,Query=3,OwnerGone=4;
struct Subscription {fc::IObservableVector<f::IInspectable> source{nullptr};winrt::event_token token{};};
struct Flat {
 winrt::weak_ref<c::ItemsControl> target;
 f::IInspectable original{nullptr};
 d::BindingBase binding{nullptr};
 fc::IObservableVector<f::IInspectable> flat{nullptr};
 d::CollectionViewSource viewSource{nullptr};
 d::ICollectionView view{nullptr};
 std::vector<c::GroupStyle> styles;
 std::vector<Subscription> subscriptions;
 int64_t sourceToken{};
 unsigned count{},groups{};
};
struct Wrap {
 winrt::weak_ref<c::ItemsWrapGrid> target;
 x::Thickness padding;
 bool sticky;
};
struct Selector {
 winrt::weak_ref<c::Button> target;
 x::Visibility visibility;
 bool enabled,tabStop;
 int64_t visibilityToken{},enabledToken{};
};
struct Zoom {
 winrt::weak_ref<c::SemanticZoom> target;
 bool canChange,button,zoomedIn;
};
struct State {
 HWND host{};DWORD owner{};HANDLE ownerProcess{};PTP_WAIT ownerWait{};
 x::Window window{nullptr};winrt::event_token activated{},visible{};
 bool active{},mutating{},queued{};unsigned generation{},passes{},failures{};
 std::vector<std::unique_ptr<Flat>> lists;
 std::vector<Wrap> wraps;
 std::vector<Selector> selectors;
 std::vector<Zoom> zooms;
};
// Allocated only inside the target UI thread; pinned DLL owns event callbacks.
static State* state;
static void Schedule();
static void Restore();
static void WriteStatus() noexcept {
 try {
  wchar_t path[32768];GetModuleFileNameW(module,path,std::size(path));
  auto slash=wcsrchr(path,L'\\');if(!slash)return;
  wcscpy_s(slash+1,std::size(path)-(slash+1-path),L"latest-status.json");
  unsigned items=0,groups=0;size_t applied=0;
  for(auto const& entry:state->lists)if(auto target=entry->target.get())if(target.ItemsSource()==entry->view){++applied;items+=entry->count;groups+=entry->groups;}
  char text[700];auto bytes=snprintf(text,sizeof(text),
    "{\"version\":\"0.1.1\",\"pid\":%lu,\"ownerPid\":%lu,\"active\":%s,\"lists\":%zu,\"items\":%u,\"sourceGroups\":%u,\"lockedSelectors\":%zu,\"passes\":%u,\"failures\":%u,\"watchMode\":\"events\"}\n",
    GetCurrentProcessId(),state->owner,state->active?"true":"false",applied,items,groups,state->selectors.size(),state->passes,state->failures);
  HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(file,text,bytes,&written,nullptr);CloseHandle(file);}
 }catch(...){}
}
static void Unsubscribe(Flat& entry) noexcept {
 for(auto& sub:entry.subscriptions)try{sub.source.VectorChanged(sub.token);}catch(...){}
 entry.subscriptions.clear();
}
static void Subscribe(Flat& entry,fc::IObservableVector<f::IInspectable> const& source) {
 if(!source)return;
 for(auto const& sub:entry.subscriptions)if(sub.source==source)return;
 auto token=source.VectorChanged([](auto const&,auto const&){if(state && !state->mutating)Schedule();});
 entry.subscriptions.push_back({source,token});
}
static bool Collect(f::IInspectable const& source,std::vector<f::IInspectable>& items,
                    unsigned& count,std::vector<fc::IObservableVector<f::IInspectable>>& observe) {
 auto view=source.try_as<d::ICollectionView>();if(!view)return false;
 auto groups=view.CollectionGroups();if(!groups)return false;
 count=groups.Size();observe.push_back(groups);observe.push_back(view);
 for(auto groupObject:groups) {
  auto group=groupObject.try_as<d::ICollectionViewGroup>();if(!group)return false;
  auto members=group.GroupItems();if(!members)return false;
  observe.push_back(members);
  for(auto member:members) {
   auto name=winrt::get_class_name(member);
   // Preserve native folder/suite models. Never flatten category cards or a
   // different future model merely because it implements ICollectionView.
   if(name!=L"StartMenu.AllAppsTileViewModel" && name!=L"StartMenu.AllAppsSuiteViewModel")return false;
   if(items.size()>=10000)return false;
   items.push_back(member);
  }
 }
 return true;
}
static bool Refresh(Flat& entry) {
 auto target=entry.target.get();if(!target)return false;
 auto current=target.ItemsSource();
 if(current!=entry.view) {
  entry.original=current;
  if(auto expr=target.GetBindingExpression(c::ItemsControl::ItemsSourceProperty()))entry.binding=expr.ParentBinding();
  else entry.binding=nullptr;
  entry.styles.clear();for(auto style:target.GroupStyle())entry.styles.push_back(style);
 }
 std::vector<f::IInspectable> items;
 std::vector<fc::IObservableVector<f::IInspectable>> observe;
 unsigned groups=0;
 if(!Collect(entry.original,items,groups,observe))return false;
 bool changed=!entry.flat || entry.flat.Size()!=items.size();
 if(!changed)for(unsigned i=0;i<items.size();i++)if(entry.flat.GetAt(i)!=items[i]){changed=true;break;}
 Unsubscribe(entry);for(auto const& vector:observe)Subscribe(entry,vector);
 if(!entry.flat) {
  entry.flat=winrt::single_threaded_observable_vector<f::IInspectable>();
  // Keep the native ICollectionView contract: Start can query it while
  // changing focus/view. A bare observable vector does not implement it.
  entry.viewSource=d::CollectionViewSource();
  entry.viewSource.IsSourceGrouped(false);
  entry.viewSource.Source(entry.flat);
  entry.view=entry.viewSource.View();
  if(!entry.view)winrt::throw_hresult(E_NOINTERFACE);
 }
 // ReplaceAll is a single Reset notification and correctly handles zero apps.
 if(changed)entry.flat.ReplaceAll(items);
 if(target.ItemsSource()!=entry.view)target.ItemsSource(entry.view);
 if(target.GroupStyle().Size())target.GroupStyle().Clear();
 entry.count=static_cast<unsigned>(items.size());entry.groups=groups;
 return true;
}
static void LockSelector(c::Button const& target) {
 bool found=false;for(auto const& entry:state->selectors)if(entry.target.get()==target){found=true;break;}
 if(!found) {
  Selector entry{winrt::make_weak(target),target.Visibility(),target.IsEnabled(),target.IsTabStop()};
  auto changed=[](x::DependencyObject const& sender,auto const&) {
   if(!state || !state->active || state->mutating)return;
   try{if(auto b=sender.try_as<c::Button>()){b.Visibility(x::Visibility::Collapsed);b.IsEnabled(false);b.IsTabStop(false);}}catch(...){++state->failures;}
  };
  entry.visibilityToken=target.RegisterPropertyChangedCallback(x::UIElement::VisibilityProperty(),changed);
  entry.enabledToken=target.RegisterPropertyChangedCallback(c::Control::IsEnabledProperty(),changed);
  state->selectors.push_back(entry);
 }
 if(auto flyout=target.Flyout())flyout.Hide();
 target.Visibility(x::Visibility::Collapsed);target.IsEnabled(false);target.IsTabStop(false);
}
static void LockZoom(c::SemanticZoom const& target) {
 bool found=false;for(auto const& entry:state->zooms)if(entry.target.get()==target){found=true;break;}
 if(!found)state->zooms.push_back({winrt::make_weak(target),target.CanChangeViews(),target.IsZoomOutButtonEnabled(),target.IsZoomedInViewActive()});
 target.CanChangeViews(false);target.IsZoomOutButtonEnabled(false);
 if(!target.IsZoomedInViewActive())target.IsZoomedInViewActive(true);
}
static bool AllAppsAncestor(x::DependencyObject obj) {
 for(unsigned depth=0;obj && depth<64;depth++) {
  if(auto fe=obj.try_as<x::FrameworkElement>())if(fe.Name()==L"AllAppsGrid")return true;
  obj=x::Media::VisualTreeHelper::GetParent(obj);
 }
 return false;
}
static void Apply() noexcept {
 if(!state || !state->active || state->mutating)return;
 state->queued=false;state->mutating=true;
 try {
  auto root=state->window.Content();if(!root){state->mutating=false;return;}
  std::erase_if(state->lists,[](auto& entry){if(entry->target.get())return false;Unsubscribe(*entry);return true;});
  std::erase_if(state->wraps,[](auto const& entry){return !entry.target.get();});
  std::erase_if(state->selectors,[](auto const& entry){return !entry.target.get();});
  std::erase_if(state->zooms,[](auto const& entry){return !entry.target.get();});
  // Gather controls before changing their source; no traversal over a tree
  // concurrently invalidated by replacing a virtualized ItemsSource.
  std::vector<x::DependencyObject> todo{root};
  std::vector<c::ItemsControl> lists;
  std::vector<c::ItemsWrapGrid> wraps;
  std::vector<c::Button> selectors;
  std::vector<c::SemanticZoom> zooms;
  unsigned visited=0;
  while(!todo.empty() && ++visited<20000) {
   auto obj=todo.back();todo.pop_back();
   if(auto fe=obj.try_as<x::FrameworkElement>()) {
    if(fe.Name()==L"AllAppsGrid")if(auto items=fe.try_as<c::ItemsControl>())lists.push_back(items);
    if(auto wrap=fe.try_as<c::ItemsWrapGrid>())if(AllAppsAncestor(wrap))wraps.push_back(wrap);
    if(fe.Name()==L"ViewSelectionButton")if(auto b=fe.try_as<c::Button>())selectors.push_back(b);
    if(auto z=fe.try_as<c::SemanticZoom>())zooms.push_back(z);
   }
   int children=x::Media::VisualTreeHelper::GetChildrenCount(obj);
   for(int i=0;i<children;i++)todo.push_back(x::Media::VisualTreeHelper::GetChild(obj,i));
  }
  for(auto target:lists) {
   Flat* entry=nullptr;
   for(auto& candidate:state->lists)if(candidate->target.get()==target){entry=candidate.get();break;}
   if(!entry) {
    auto fresh=std::make_unique<Flat>();fresh->target=winrt::make_weak(target);
    // A future/unsupported source is left untouched.
    if(!Refresh(*fresh))continue;
    fresh->sourceToken=target.RegisterPropertyChangedCallback(c::ItemsControl::ItemsSourceProperty(),
      [](auto const&,auto const&){if(state && !state->mutating)Schedule();});
    state->lists.push_back(std::move(fresh));
   }else Refresh(*entry);
  }
  // Unknown/new Start data models leave their view controls untouched.
  if(!state->lists.empty()) {
   for(auto z:zooms)LockZoom(z);
   for(auto b:selectors)LockSelector(b);
  }
  if(!state->lists.empty())for(auto target:wraps) {
   bool tracked=false;for(auto const& entry:state->wraps)if(entry.target.get()==target){tracked=true;break;}
   if(!tracked)state->wraps.push_back({winrt::make_weak(target),target.GroupPadding(),target.AreStickyGroupHeadersEnabled()});
   if(target.GroupPadding()!=x::Thickness{})target.GroupPadding({});
   if(target.AreStickyGroupHeadersEnabled())target.AreStickyGroupHeadersEnabled(false);
  }
  ++state->passes;
 }catch(...){++state->failures;}
 state->mutating=false;WriteStatus();
}
static void Schedule() {
 if(!state || !state->active || state->queued || state->mutating)return;
 state->queued=true;const auto generation=state->generation;
 try {state->window.Dispatcher().RunAsync(core::CoreDispatcherPriority::Low,[generation](){
  if(state && state->active && state->generation==generation)Apply();
 });}catch(...){state->queued=false;++state->failures;}
}
static void CALLBACK OwnerExited(PTP_CALLBACK_INSTANCE,void* context,PTP_WAIT,TP_WAIT_RESULT) {
 PostMessageW(reinterpret_cast<HWND>(context),Signal(),OwnerGone,0);
}
static void Restore() {
 if(!state || !state->active)return;
 state->active=false;++state->generation;state->queued=false;state->mutating=true;
 if(state->ownerWait){SetThreadpoolWait(state->ownerWait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(state->ownerWait,TRUE);CloseThreadpoolWait(state->ownerWait);state->ownerWait=nullptr;}
 if(state->ownerProcess){CloseHandle(state->ownerProcess);state->ownerProcess=nullptr;}
 try{state->window.Activated(state->activated);state->window.VisibilityChanged(state->visible);}catch(...){}
 for(auto& entry:state->lists) {
  Unsubscribe(*entry);
  try {if(auto target=entry->target.get()) {
   target.UnregisterPropertyChangedCallback(c::ItemsControl::ItemsSourceProperty(),entry->sourceToken);
   if(target.ItemsSource()==entry->view) {
    if(entry->binding)target.SetBinding(c::ItemsControl::ItemsSourceProperty(),entry->binding);
    else target.ItemsSource(entry->original);
    auto styles=target.GroupStyle();styles.Clear();for(auto style:entry->styles)styles.Append(style);
   }
  }}catch(...){++state->failures;}
 }
 for(auto& entry:state->wraps)try{if(auto target=entry.target.get()){target.GroupPadding(entry.padding);target.AreStickyGroupHeadersEnabled(entry.sticky);}}catch(...){}
 for(auto& entry:state->selectors)try{if(auto target=entry.target.get()) {
  target.UnregisterPropertyChangedCallback(x::UIElement::VisibilityProperty(),entry.visibilityToken);
  target.UnregisterPropertyChangedCallback(c::Control::IsEnabledProperty(),entry.enabledToken);
  target.Visibility(entry.visibility);target.IsEnabled(entry.enabled);target.IsTabStop(entry.tabStop);
 }}catch(...){++state->failures;}
 for(auto& entry:state->zooms)try{if(auto target=entry.target.get()){target.CanChangeViews(entry.canChange);target.IsZoomOutButtonEnabled(entry.button);target.IsZoomedInViewActive(entry.zoomedIn);}}catch(...){++state->failures;}
 state->lists.clear();state->wraps.clear();state->selectors.clear();state->zooms.clear();state->window=nullptr;state->mutating=false;WriteStatus();
}
static bool ValidOwner(DWORD pid) {
 wchar_t dll[32768],expected[32768],actual[32768];GetModuleFileNameW(module,dll,std::size(dll));
 auto slash=wcsrchr(dll,L'\\');if(!slash)return false;*slash=0;
 slash=wcsrchr(dll,L'\\');if(!slash)return false;*slash=0;
 _snwprintf_s(expected,_TRUNCATE,L"%ls\\latest.exe",dll);
 HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
 DWORD length=std::size(actual),ownSession=0,session=0;
 bool ok=QueryFullProcessImageNameW(p,0,actual,&length) && !_wcsicmp(actual,expected) &&
  ProcessIdToSessionId(GetCurrentProcessId(),&ownSession) && ProcessIdToSessionId(pid,&session) && ownSession==session;
 CloseHandle(p);return ok;
}
static LRESULT CALLBACK WindowProc(HWND w,UINT message,WPARAM p,LPARAM l,UINT_PTR,DWORD_PTR) {
 if(message==Signal()) {
  if(p==Query){WriteStatus();return state && state->active?1:0;}
  if(p==Disable && state && DWORD(l)==state->owner){Restore();return 1;}
  if(p==OwnerGone && state && state->ownerProcess && WaitForSingleObject(state->ownerProcess,0)==WAIT_OBJECT_0){Restore();return 1;}
 }
 if(message==WM_NCDESTROY){Restore();RemoveWindowSubclass(w,WindowProc,1);}
 auto result=DefSubclassProc(w,message,p,l);
 if(state && state->active && (message==WM_SHOWWINDOW || message==WM_SIZE || message==WM_WINDOWPOSCHANGED))Schedule();
 return result;
}
static bool Start(HWND host,DWORD owner) {
 if(!ValidOwner(owner))return false;
 if(state && state->active) {
  if(state->owner==owner){Schedule();return true;}
  // The old controller may exit while Start is suspended. Restore its state
  // here before accepting a new owner, even if OwnerGone is still queued.
  if(!state->ownerProcess || WaitForSingleObject(state->ownerProcess,0)!=WAIT_OBJECT_0)return false;
  Restore();
 }
 auto window=x::Window::Current();if(!window)return false;
 if(!state)state=new State();
 if(!SetWindowSubclass(host,WindowProc,1,0))return false;
 // Keep executable callbacks mapped until the system Start process exits.
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<PCWSTR>(WindowProc),&pinned))return false;
 state->host=host;state->owner=owner;state->window=window;state->active=true;++state->generation;
 state->ownerProcess=OpenProcess(SYNCHRONIZE,FALSE,owner);
 state->ownerWait=CreateThreadpoolWait(OwnerExited,host,nullptr);
 if(!state->ownerProcess || !state->ownerWait){Restore();return false;}
 SetThreadpoolWait(state->ownerWait,state->ownerProcess,nullptr);
 state->activated=window.Activated([](auto const&,auto const& args){if(args.WindowActivationState()!=core::CoreWindowActivationState::Deactivated)Schedule();});
 state->visible=window.VisibilityChanged([](auto const&,auto const& args){if(args.Visible())Schedule();});
 Apply();return true;
}
extern "C" __declspec(dllexport) LRESULT CALLBACK LeanDockStartHook(int code,WPARAM p,LPARAM l) {
 if(code==HC_ACTION) {
  auto call=reinterpret_cast<CWPSTRUCT*>(l);
  if(call->message==Signal() && call->wParam==Enable) {
   try {
    wchar_t own[32768],expected[32768];GetModuleFileNameW(nullptr,own,std::size(own));GetWindowsDirectoryW(expected,std::size(expected));
    wcscat_s(expected,L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\StartMenuExperienceHost.exe");
    if(!_wcsicmp(own,expected))Start(call->hwnd,DWORD(call->lParam));
   }catch(...){if(state){++state->failures;Restore();}}
  }
 }
 return CallNextHookEx(nullptr,code,p,l);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
 if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;
}
