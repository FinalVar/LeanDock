// Exercise the real detour with controlled native results and window identity.
// This verifies passthrough, source ownership and thread/exception boundaries;
// it cannot establish the pixels of a physical touchpad animation.
#include <windows.h>
#include <map>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <stdexcept>
#include "native-api-latest.h"
static std::map<std::wstring,ULONG_PTR> props;
static DWORD owner=77;
static std::wstring className=L"Shell_TrayWnd";
static DWORD Current(){return 77;}
static DWORD Owner(HWND,DWORD* output){*output=owner;return 1;}
static HANDLE Property(HWND,LPCWSTR name){return reinterpret_cast<HANDLE>(props[name]);}
static int Class(HWND,LPWSTR output,int cap){return lstrcpynW(output,className.c_str(),cap)?int(className.size()):0;}
namespace LeanDockBackdrop {void Put(HWND,const wchar_t* key,ULONG_PTR value){props[key]=value+1;}}
extern "C" void Native_ReportFailure(const wchar_t*,HRESULT){}
// The fixture does not install hooks. Registration is exercised by deployment.
extern "C" BOOL InternalWh_HookSymbols(void*,HMODULE,const WH_SYMBOL_HOOK*,size_t,const WH_HOOK_SYMBOLS_OPTIONS*){return FALSE;}
#define GetCurrentProcessId Current
#define GetWindowThreadProcessId Owner
#define GetPropW Property
#define GetClassNameW Class
#include "../src/gesture-thumbnail-latest.inc"
#undef GetCurrentProcessId
#undef GetWindowThreadProcessId
#undef GetPropW
#undef GetClassNameW
static std::vector<void*> calls,sourceReads;
static HWND source=reinterpret_cast<HWND>(1);
static HRESULT outcome=S_OK,initializationResult=S_OK;
static bool throwFreeze=false,throwInitialization=false;
static void* initializedSelf{};
static DWORD initializedOptions{};
static unsigned initializedDepth{};
static HRESULT WINAPI NativeFreeze(void* self){calls.push_back(self);if(throwFreeze)throw std::runtime_error("native failure");return outcome;}
static HWND WINAPI NativeSource(void* self){sourceReads.push_back(self);return source;}
static HRESULT WINAPI NativeInitialize(void* self,DWORD options){
 initializedSelf=self;initializedOptions=options;initializedDepth=LeanDockGesture::depth;
 if(throwInitialization)throw std::runtime_error("initialization failure");
 LeanDockGesture::FreezeHook(self);return initializationResult;
}
static unsigned checks;
void Check(bool v){++checks;if(!v)throw std::runtime_error("detour check failed");}
int main(){
 try{
  using namespace LeanDockGesture;
  auto self=reinterpret_cast<void*>(2);
  nativeInitialize=NativeInitialize;nativeFreeze=NativeFreeze;nativeSource=NativeSource;enabled=true;props[L"LeanDock.Material.Ready"]=2;
  auto run=[&]{calls.clear();sourceReads.clear();return FreezeHook(self);};
  Check(run()==S_OK&&calls.size()==1&&calls[0]==self&&sourceReads.empty()); // outside gesture
  {
   Scope gesture(depth);Check(depth==1);
   Check(run()==S_OK&&calls.empty()&&sourceReads.size()==1&&sourceReads[0]==self);
   Check(props[L"LeanDock.Gesture.FreezeSkipped"]==2);
   Check(run()==S_OK&&calls.empty()&&props[L"LeanDock.Gesture.FreezeSkipped"]==3);
   owner=99;outcome=E_ACCESSDENIED;Check(run()==E_ACCESSDENIED&&calls.size()==1&&calls[0]==self);owner=77;outcome=S_OK;
   className=L"ApplicationWindow";Check(run()==S_OK&&calls.size()==1);className=L"Shell_SecondaryTrayWnd";
   props[L"LeanDock.Material.Ready"]=1;Check(run()==S_OK&&calls.size()==1);props[L"LeanDock.Material.Ready"]=0;
   Check(run()==S_OK&&calls.size()==1);props[L"LeanDock.Material.Ready"]=2;
   Check(run()==S_OK&&calls.empty());
   source=nullptr;Check(run()==S_OK&&calls.size()==1);source=reinterpret_cast<HWND>(1);
   self=nullptr;Check(run()==S_OK&&calls.size()==1&&calls[0]==nullptr&&sourceReads.empty());self=reinterpret_cast<void*>(2);
   nativeSource=nullptr;Check(run()==S_OK&&calls.size()==1&&sourceReads.empty());nativeSource=NativeSource;
   Stop();Check(run()==S_OK&&calls.size()==1&&sourceReads.empty());enabled=true;
   std::thread other([&]{Check(depth==0);Check(run()==S_OK&&calls.size()==1&&sourceReads.empty());{Scope inner(depth);Check(depth==1);}Check(depth==0);});other.join();
   Check(depth==1);
   bool threw=false;
   try{Scope nested(depth);Check(depth==2);throwFreeze=true;className=L"ApplicationWindow";run();}catch(std::runtime_error const& e){threw=std::string(e.what())=="native failure";}
   Check(threw&&depth==1);throwFreeze=false;className=L"Shell_TrayWnd";
  }
  Check(depth==0);
  outcome=E_FAIL;Check(run()==E_FAIL&&calls.size()==1);outcome=S_OK;
  calls.clear();initializationResult=E_NOTIMPL;
  Check(InitializeHook(self,0x1234)==E_NOTIMPL&&depth==0&&calls.empty());
  Check(initializedSelf==self&&initializedOptions==0x1234&&initializedDepth==1);
  bool threw=false;throwInitialization=true;
  try{InitializeHook(self,2);}catch(std::runtime_error const& e){threw=std::string(e.what())=="initialization failure";}
  Check(threw&&depth==0);
  std::printf("PASS %u gesture adapter checks; visual acceptance remains separate\n",checks);
 }catch(std::exception const& e){std::printf("FAIL %s\n",e.what());return 1;}
}
