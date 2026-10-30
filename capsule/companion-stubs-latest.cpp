#define ND_COMPONENT 4
#include "native-api-latest.h"
#include "protocol-latest.h"
#include "verify-latest.h"
#include "dock-geometry-latest.h"
#include "appbar-query-policy-latest.h"
#include "companion-shell-latest.h"
#include <array>
#include <mutex>
#include <atomic>
#include <iterator>
#include <shellapi.h>
namespace CompanionAppBar {
bool g_eventStopping=true,g_wasAutoHideProcessed=true;
bool IsTaskbarWindow(HWND w) {
    return w&&w==LeanDockNativeShell();
}
// This adapted include retains the exact module hashes, native-first query,
// cache ownership and restoration. No hide/reveal or pointer hooks are loaded.
#include "companion-appbar-generated.inc"
}
extern "C" BOOL Native_hide_init(){CompanionAppBar::InstallAppBarCompatibility();return TRUE;}
extern "C" void Native_hide_after(){
    CompanionAppBar::g_eventStopping=false;
    CompanionAppBar::PublishAppBarSupport(LeanDockNativeShell());
}
extern "C" void Native_hide_stop(){
    CompanionAppBar::g_eventStopping=true;CompanionAppBar::RestoreAppBarCache();
    HWND w=LeanDockNativeShell();
    for(auto key:{LeanDockAppBar::HookReady,LeanDockAppBar::Queries,LeanDockAppBar::CacheWrites})RemovePropW(w,key);
}
extern "C" unsigned long long Native_hide_attention(unsigned){return 0;}
#define EMPTY_COMPONENT(x) extern "C" BOOL Native_##x##_init(){return TRUE;} \
    extern "C" void Native_##x##_after(){} extern "C" void Native_##x##_stop(){}
EMPTY_COMPONENT(size)
EMPTY_COMPONENT(tray)
EMPTY_COMPONENT(show)
extern "C" void Native_tray_before(){}
extern "C" void Native_size_before(){}
extern "C" void Native_tray_resume(){}
extern "C" void Native_size_resume(){}
extern "C" void Native_tray_refresh_cards(){}
extern "C" int Native_hide_expanded(HWND){return 0;}
