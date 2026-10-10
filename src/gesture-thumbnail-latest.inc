// Keep only LeanDock's four-finger taskbar thumbnail live during the gesture.
// Shell still owns its creation, parent, transforms, animation and destruction.
// Initialize calls Freeze after creating/positioning the taskbar thumbnail.
// Freeze and GetSourceWindow use the same IDCompThumbnail interface, verified
// from this build's pinned vtable. No object offsets are used in this adapter.
#include "gesture-thumbnail-latest.h"
#include <atomic>
namespace LeanDockGesture {
thread_local unsigned depth=0;
std::atomic<bool> enabled{false};
using Initialize=HRESULT(WINAPI*)(void*,DWORD);
using Freeze=HRESULT(WINAPI*)(void*);
using SourceWindow=HWND(WINAPI*)(void*);
Initialize nativeInitialize{};
Freeze nativeFreeze{};
SourceWindow nativeSource{};
bool OwnTaskbar(HWND window) noexcept {
    DWORD process{};
    if(!window||!GetWindowThreadProcessId(window,&process)||process!=GetCurrentProcessId()||
       GetPropW(window,L"LeanDock.Material.Ready")!=reinterpret_cast<HANDLE>(2))return false;
    wchar_t name[64]{};
    return GetClassNameW(window,name,64)&&
        (!wcscmp(name,L"Shell_TrayWnd")||!wcscmp(name,L"Shell_SecondaryTrayWnd"));
}
HRESULT WINAPI InitializeHook(void* self,DWORD options){
    Scope gesture(depth);
    return nativeInitialize(self,options);
}
HRESULT WINAPI FreezeHook(void* self){
    // Resolve the source through the original class method, only for calls
    // already inside the exact gesture initializer. No raw pointer is retained.
    if(!enabled.load(std::memory_order_acquire)||!depth||!self||!nativeSource)
        return nativeFreeze(self);
    const HWND source=nativeSource(self);
    if(!OwnTaskbar(source))return nativeFreeze(self);
    const auto calls=reinterpret_cast<ULONG_PTR>(GetPropW(source,L"LeanDock.Gesture.FreezeSkipped"));
    LeanDockBackdrop::Put(source,L"LeanDock.Gesture.FreezeSkipped",calls?calls:1);
    LeanDockBackdrop::Put(source,L"LeanDock.Gesture.FreezeAt",GetTickCount64());
    // The ordinary live DWM thumbnail and all its flags are left intact;
    // release stays with Shell. Other app previews keep native Freeze behavior.
    return S_OK;
}
void Stop() noexcept {enabled.store(false,std::memory_order_release);}
void Install(){
    const WindhawkUtils::SYMBOL_HOOK initialization[]={
        {{LR"(public: long __cdecl VirtualDesktopGestureWindow::Initialize(unsigned long) __ptr64)"},&nativeInitialize,InitializeHook}
    };
    const WindhawkUtils::SYMBOL_HOOK thumbnail[]={
        {{LR"(public: virtual long __cdecl CDCompWindowThumbnail::Freeze(void) __ptr64)"},&nativeFreeze,FreezeHook},
        {{LR"(public: virtual struct HWND__ * __ptr64 __cdecl CDCompWindowThumbnail::GetSourceWindow(void) __ptr64)"},&nativeSource,nullptr}
    };
    const auto shell=GetModuleHandleW(L"twinui.pcshell.dll");
    const auto factory=GetModuleHandleW(L"twinui.dll");
    if(!shell||!factory){Native_ReportFailure(L"gesture thumbnail module",HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));return;}
    if(WindhawkUtils::HookSymbols(shell,initialization,ARRAYSIZE(initialization))&&
       WindhawkUtils::HookSymbols(factory,thumbnail,ARRAYSIZE(thumbnail)))
        enabled.store(true,std::memory_order_release);
}
}
