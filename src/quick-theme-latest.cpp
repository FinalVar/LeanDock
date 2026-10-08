// LeanDock Quick Settings theme tile, GPL-3.0. XAML diagnostics discovery
// follows the IObjectWithSite pattern documented in NOTICE.md. Discovery is
// one-shot: unadvise and release diagnostic handles, then use local UI events.
#include <windows.h>
#include <ocidl.h>
#include <xamlom.h>
#include <cstdio>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include "quick-theme-latest.h"
#include "quick-position-latest.h"
#include "theme-latest.h"
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
namespace x=winrt::Windows::UI::Xaml;
namespace c=x::Controls;
namespace f=winrt::Windows::Foundation;
namespace {
constexpr CLSID TapId={0x6153fb20,0x4106,0x4958,{0x88,0x09,0x3b,0xcb,0xdf,0x20,0xe8,0xad}};
constexpr IID TestHooksId={0x735941a2,0x3ee3,0x495a,{0x8d,0xa9,0x97,0x26,0x27,0x00,0x30,0x75}};
struct TestHooks: IUnknown {virtual HRESULT STDMETHODCALLTYPE UnregisterInstance(InstanceHandle)=0;virtual HRESULT STDMETHODCALLTYPE TryGetDispatcherQueueForObject(InstanceHandle,void**)=0;};
UINT Signal(){static UINT message=RegisterWindowMessageW(L"LeanDock.QuickTheme.v1");return message;}
enum Command:WPARAM {Reconcile=1,DiscoveryDone=2,WorkerDone=3};
HMODULE module;
std::atomic<unsigned> epoch{};
std::atomic<HWND> hostWindow{};
std::atomic<DWORD> hostThread{};
std::atomic<bool> discovering{};
std::atomic<HRESULT> discoveryResult{};
struct Tile {
 winrt::weak_ref<c::ContentControl> host;
 f::IInspectable local{nullptr};x::Data::BindingBase binding{nullptr};
 x::DataTemplate replacement{nullptr};
 winrt::weak_ref<c::Primitives::ToggleButton> button;
 winrt::event_token click{};
};
struct State {
 HWND host{};unsigned generation{},discoveries{},failures{},switches{};bool active{},queued{},applying{},busy{};
 winrt::weak_ref<x::FrameworkElement> root;winrt::event_token layout{};
 std::unique_ptr<Tile> tile;
 HANDLE worker{};PTP_WAIT wait{};DWORD workerPid{},lastExit{};
 wchar_t controller[32768]{};
};
State* state; // Created and accessed only by the ControlCenterWindow UI thread.
void Publish() noexcept {
 try{if(!state)return;wchar_t path[32768]{};GetModuleFileNameW(module,path,std::size(path));auto slash=wcsrchr(path,L'\\');if(!slash)return;
  wcscpy_s(slash+1,std::size(path)-(slash+1-path),L"latest-status.json");bool dark=false;const bool known=LeanDockTheme::ReadDark(dark);
  bool buttonReady=state->tile&&state->tile->button.get();
  char text[768];int n=snprintf(text,sizeof(text),"{\"version\":\"0.2.0\",\"pid\":%lu,\"ownerPid\":%llu,\"active\":%s,\"rootReady\":%s,\"tileInstalled\":%s,\"buttonReady\":%s,\"busy\":%s,\"workerPid\":%lu,\"dark\":%s,\"stateKnown\":%s,\"discoveries\":%u,\"switches\":%u,\"failures\":%u,\"lastExit\":%lu,\"discoveryResult\":%ld,\"watchMode\":\"events\"}\n",
   GetCurrentProcessId(),static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(GetPropW(state->host,LeanDockQuick::Owner))),state->active?"true":"false",state->root.get()?"true":"false",state->tile?"true":"false",buttonReady?"true":"false",state->busy?"true":"false",state->workerPid,dark?"true":"false",known?"true":"false",state->discoveries,state->switches,state->failures,state->lastExit,long(discoveryResult.load()));
  HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(file,text,n,&written,nullptr);CloseHandle(file);}
 }catch(...){}
}
void Queue() noexcept {
 if(state&&state->active&&!state->queued&&!discovering.load()){state->queued=true;if(!PostMessageW(state->host,Signal(),Reconcile,state->generation))state->queued=false;}
}
void CloseWorker() noexcept {
 if(state->wait){SetThreadpoolWait(state->wait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(state->wait,TRUE);CloseThreadpoolWait(state->wait);state->wait=nullptr;}
 if(state->worker){CloseHandle(state->worker);state->worker=nullptr;}state->workerPid=0;state->busy=false;
}
void RefreshButton() {
 if(!state||!state->tile)return;
 if(auto button=state->tile->button.get()){
  bool dark=false;const bool known=LeanDockTheme::ReadDark(dark);
  button.IsChecked(known&&dark);button.IsEnabled(known&&!state->busy);
  x::Automation::AutomationProperties::SetName(button,state->busy?L"正在切换主题":L"深色主题");
 }
}
void CALLBACK Completed(PTP_CALLBACK_INSTANCE,void* context,PTP_WAIT,TP_WAIT_RESULT) {
 const unsigned generation=static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(context));
 if(epoch.load()==generation)if(auto host=hostWindow.load())PostMessageW(host,Signal(),WorkerDone,generation);
}
void SwitchTheme() noexcept {
 try{
  if(!state||!state->active||state->busy)return;bool dark=false;if(!LeanDockTheme::ReadDark(dark))return;
  wchar_t command[32768]{};if(_snwprintf_s(command,_TRUNCATE,L"\"%ls\" --theme %ls",state->controller,dark?L"light":L"dark")<0)return;
  STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
  state->wait=CreateThreadpoolWait(Completed,reinterpret_cast<void*>(static_cast<ULONG_PTR>(state->generation)),nullptr);
  if(!state->wait){++state->failures;RefreshButton();Publish();return;}
  if(!CreateProcessW(state->controller,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)){
   state->lastExit=GetLastError();++state->failures;CloseWorker();RefreshButton();Publish();return;
  }
  CloseHandle(process.hThread);state->worker=process.hProcess;state->workerPid=process.dwProcessId;state->busy=true;++state->switches;
  SetThreadpoolWait(state->wait,state->worker,nullptr);RefreshButton();Publish();
 }catch(...){if(state){++state->failures;Publish();}}
}
template<class Predicate> x::FrameworkElement Find(x::DependencyObject const& root,Predicate predicate){
 if(!root)return nullptr;std::vector<x::DependencyObject> pending{root};unsigned count=0;
 while(!pending.empty()&&++count<2500){auto obj=pending.back();pending.pop_back();
  if(auto fe=obj.try_as<x::FrameworkElement>();fe&&predicate(fe))return fe;
  int children=x::Media::VisualTreeHelper::GetChildrenCount(obj);for(int i=children-1;i>=0;--i)pending.push_back(x::Media::VisualTreeHelper::GetChild(obj,i));
 }return nullptr;
}
void RestoreTile() noexcept {
 if(!state||!state->tile)return;auto& tile=*state->tile;
 try{if(auto button=tile.button.get())button.Click(tile.click);}catch(...){}
 try{if(auto host=tile.host.get();host&&host.ContentTemplate()==tile.replacement){
  if(tile.binding)host.SetBinding(c::ContentControl::ContentTemplateProperty(),tile.binding);
  else if(tile.local==x::DependencyProperty::UnsetValue())host.ClearValue(c::ContentControl::ContentTemplateProperty());
  else host.SetValue(c::ContentControl::ContentTemplateProperty(),tile.local);
 }}catch(...){++state->failures;}
 state->tile.reset();
}
void Apply(){
 if(!state||!state->active||state->applying)return;
 state->applying=true;struct Guard{~Guard(){state->applying=false;}} guard;
 auto root=state->root.get();if(!root)return;
 if(state->tile){auto host=state->tile->host.get();if(!host||host.ContentTemplate()!=state->tile->replacement)RestoreTile();}
 if(!state->tile){
  auto original=Find(root,[](auto const& fe){return x::Automation::AutomationProperties::GetAutomationId(fe)==L"Microsoft.QuickAction.BlueLightReduction";});
  if(!original)return;
  // Replace only the tile's presentation template. The original model and
  // template/binding are retained for stop/reload; no native Night Light event
  // is reused or invoked. Do not match translated text or positional indexes.
  c::ContentControl host{nullptr};auto parent=x::Media::VisualTreeHelper::GetParent(original);
  for(unsigned i=0;parent&&i<12;++i){if(winrt::get_class_name(parent)==L"Windows.UI.Xaml.Controls.ContentControl"){host=parent.as<c::ContentControl>();break;}parent=x::Media::VisualTreeHelper::GetParent(parent);}
  if(!host)return;
  auto tile=std::make_unique<Tile>();tile->host=winrt::make_weak(host);tile->local=host.ReadLocalValue(c::ContentControl::ContentTemplateProperty());
  if(auto expression=host.GetBindingExpression(c::ContentControl::ContentTemplateProperty()))tile->binding=expression.ParentBinding();
  tile->replacement=x::Markup::XamlReader::Load(LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><Grid Height="88"><Grid.RowDefinitions><RowDefinition Height="48"/><RowDefinition Height="8"/><RowDefinition Height="24"/><RowDefinition Height="8"/></Grid.RowDefinitions><ToggleButton x:Name="LeanDockThemeToggle" AutomationProperties.AutomationId="LeanDock.ThemeToggle" AutomationProperties.Name="深色主题" HorizontalAlignment="Stretch" VerticalAlignment="Stretch" Padding="0" CornerRadius="4" IsThreeState="False"><FontIcon FontFamily="Segoe Fluent Icons" FontSize="16" Glyph="&#xE708;"/></ToggleButton><TextBlock Grid.Row="2" Text="深色主题" FontSize="12" HorizontalAlignment="Center" VerticalAlignment="Center"/></Grid></DataTemplate>)").as<x::DataTemplate>();
  host.ContentTemplate(tile->replacement);state->tile=std::move(tile);Publish();
 }
 auto& tile=*state->tile;
 if(!tile.button.get())if(auto host=tile.host.get()){
  auto found=Find(host,[](auto const& fe){return fe.Name()==L"LeanDockThemeToggle";});
  if(found){auto button=found.as<c::Primitives::ToggleButton>();tile.button=winrt::make_weak(button);
   tile.click=button.Click([](auto const&,x::RoutedEventArgs const&){SwitchTheme();});Publish();
  }
 }
 RefreshButton();
}
void Discovered(x::FrameworkElement const& root,unsigned generation){
 if(!state||!state->active||state->generation!=generation||GetCurrentThreadId()!=hostThread.load())return;
 if(auto old=state->root.get()){if(old==root)return;old.LayoutUpdated(state->layout);RestoreTile();}
 state->root=winrt::make_weak(root);
 state->layout=root.LayoutUpdated([](auto const&,auto const&){
  if(!state||!state->active||state->applying)return;
  // A stable installed tile makes layout events constant-time. Only a native
  // template recreation requires another bounded visual-tree search.
  if(state->tile){auto host=state->tile->host.get();auto button=state->tile->button.get();if(host&&host.ContentTemplate()==state->tile->replacement&&button&&button.IsLoaded())return;}
  Queue();
 });Queue();
}
struct Watcher:winrt::implements<Watcher,IVisualTreeServiceCallback2>{
 winrt::com_ptr<IXamlDiagnostics> diagnostics;unsigned generation;std::mutex mutex;std::vector<InstanceHandle> handles;bool found{};
 Watcher(IUnknown* site,unsigned g):generation(g){winrt::check_hresult(site->QueryInterface(__uuidof(IXamlDiagnostics),diagnostics.put_void()));}
 HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation,VisualElement element,VisualMutationType mutation) noexcept override {
  try{if(mutation==Add){std::lock_guard lock(mutex);handles.push_back(element.Handle);
   if(!found&&generation==epoch.load()&&GetCurrentThreadId()==hostThread.load()){
    f::IInspectable object{nullptr};if(SUCCEEDED(diagnostics->GetIInspectableFromHandle(element.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(object)))))
     if(auto fe=object.try_as<x::FrameworkElement>();fe&&fe.Name()==L"ControlCenterRegion"){found=true;Discovered(fe,generation);}
   }
  }}catch(...){}return S_OK;
 }
 HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR) noexcept override{return S_OK;}
};
void DiscoveryFinished(unsigned generation,HRESULT result){
 discoveryResult.store(result);discovering.store(false);
 if(generation==epoch.load())if(auto host=hostWindow.load())PostMessageW(host,Signal(),DiscoveryDone,generation);
}
struct Tap:winrt::implements<Tap,IObjectWithSite>{
 winrt::com_ptr<IUnknown> site;
 HRESULT STDMETHODCALLTYPE SetSite(IUnknown* value) noexcept override{try{
  site.copy_from(value);if(!value)return S_OK;
  auto watcher=winrt::make_self<Watcher>(value,epoch.load());auto raw=watcher.detach();
  HANDLE thread=CreateThread(nullptr,0,[](void* data)->DWORD{
   winrt::com_ptr<Watcher> watcher;watcher.attach(static_cast<Watcher*>(data));HRESULT result=E_FAIL;
   try{auto service=watcher->diagnostics.as<IVisualTreeService3>();
    // Require the release interface before advising, so unsupported runtimes
    // cannot silently retain every reported XAML object.
    winrt::com_ptr<TestHooks> cleanup;winrt::check_hresult(watcher->diagnostics->QueryInterface(TestHooksId,cleanup.put_void()));
    result=service->AdviseVisualTreeChange(watcher.get());HRESULT stopped=service->UnadviseVisualTreeChange(watcher.get());
    std::lock_guard lock(watcher->mutex);std::sort(watcher->handles.begin(),watcher->handles.end());
    watcher->handles.erase(std::unique(watcher->handles.begin(),watcher->handles.end()),watcher->handles.end());
    for(auto handle:watcher->handles)cleanup->UnregisterInstance(handle);
    if(SUCCEEDED(result)&&FAILED(stopped))result=stopped;
   }catch(...){result=winrt::to_hresult();}
   DiscoveryFinished(watcher->generation,result);return 0;
  },raw,0,nullptr);
  if(thread)CloseHandle(thread);else{raw->Release();DiscoveryFinished(epoch.load(),HRESULT_FROM_WIN32(GetLastError()));}
  return S_OK;
 }catch(...){auto result=winrt::to_hresult();DiscoveryFinished(epoch.load(),result);return result;}}
 HRESULT STDMETHODCALLTYPE GetSite(REFIID iid,void** result) noexcept override{return site?site->QueryInterface(iid,result):E_FAIL;}
};
struct Factory:winrt::implements<Factory,IClassFactory>{
 HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** result) noexcept override{if(outer)return CLASS_E_NOAGGREGATION;try{return winrt::make<Tap>().as(iid,result);}catch(...){return winrt::to_hresult();}}
 HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override{return S_OK;}
};
void Discover(){
 if(!state||!state->active||state->root.get()||discovering.exchange(true))return;
 ++state->discoveries;const unsigned generation=state->generation;
 HANDLE thread=CreateThread(nullptr,0,[](void* context)->DWORD{
  auto generation=static_cast<unsigned>(reinterpret_cast<ULONG_PTR>(context));HRESULT result=E_FAIL;
  HMODULE xaml=LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  if(xaml){auto init=reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(xaml,"InitializeXamlDiagnosticsEx"));
   wchar_t path[32768]{};GetModuleFileNameW(module,path,std::size(path));
   if(init)for(int i=1;i<=32&&epoch.load()==generation;++i){wchar_t name[64];swprintf_s(name,L"VisualDiagConnection%d",i);
    result=init(name,GetCurrentProcessId(),L"",path,TapId,nullptr);if(result!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;
   }FreeLibrary(xaml);
  }
  if(FAILED(result))DiscoveryFinished(generation,result);return 0;
 },reinterpret_cast<void*>(static_cast<ULONG_PTR>(generation)),0,nullptr);
 if(thread)CloseHandle(thread);else DiscoveryFinished(generation,HRESULT_FROM_WIN32(GetLastError()));
 Publish();
}
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdll-attribute-on-redeclaration"
extern "C" __declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** result){
 if(!result)return E_POINTER;*result=nullptr;if(clsid!=TapId)return CLASS_E_CLASSNOTAVAILABLE;
 try{return winrt::make<Factory>().as(iid,result);}catch(...){return winrt::to_hresult();}
}
#pragma clang diagnostic pop
namespace LeanDockQuickTheme {
void Stop() noexcept {
 if(!state)return;state->active=false;++epoch;hostWindow.store(nullptr);hostThread.store(0);
 try{if(auto root=state->root.get())root.LayoutUpdated(state->layout);}catch(...){}
 state->root=nullptr;RestoreTile();CloseWorker();state->queued=false;Publish();
}
void Start(HWND host,HMODULE image) noexcept {try{
 Stop();module=image;if(!state)state=new State();state->host=host;state->generation=++epoch;state->active=true;state->queued=false;
 hostWindow.store(host);hostThread.store(GetCurrentThreadId());
 GetModuleFileNameW(module,state->controller,std::size(state->controller));auto slash=wcsrchr(state->controller,L'\\');if(!slash){Stop();return;}*slash=0;
 slash=wcsrchr(state->controller,L'\\');if(!slash){Stop();return;}wcscpy_s(slash+1,std::size(state->controller)-(slash+1-state->controller),L"latest.exe");
 Discover();
 }catch(...){if(state){++state->failures;Publish();}}}
void WindowMessage(UINT message,WPARAM p,LPARAM l) noexcept {try{
 if(!state||!state->active)return;
 if(message==Signal()&&unsigned(l)==state->generation){
  if(p==Reconcile){state->queued=false;Apply();}
  else if(p==DiscoveryDone){if(FAILED(discoveryResult.load()))++state->failures;Queue();Publish();}
  else if(p==WorkerDone&&state->worker&&WaitForSingleObject(state->worker,0)==WAIT_OBJECT_0){
   GetExitCodeProcess(state->worker,&state->lastExit);if(state->lastExit)++state->failures;CloseWorker();RefreshButton();Publish();
  }
 }else if((message==WM_SHOWWINDOW&&p)||message==WM_WINDOWPOSCHANGED||
           (message==WM_ACTIVATE&&LOWORD(p)!=WA_INACTIVE)){Discover();Queue();}
 else if(message==WM_SETTINGCHANGE||message==WM_THEMECHANGED){Queue();}
 }catch(...){if(state){++state->failures;Publish();}}}
}
