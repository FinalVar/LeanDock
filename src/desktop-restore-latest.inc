// The desktop hot corner minimizes the windows Win+D only covered and tags
// them (include/desktop-sweep-mark-latest.h). The taskbar and Alt+Tab, both
// in Explorer, bring a minimized window back with SwitchToThisWindow, which
// posts WM_SYSCOMMAND SC_RESTORE; Windows Terminal leaves fullscreen on it and
// stays minimized. Explorer runs at medium integrity and may not show an
// elevated window (ShowWindowAsync fails with ERROR_ACCESS_DENIED there), but
// it may activate it. For a tagged window still minimized, Explorer therefore
// only activates it; the controller, which tagged the window at its own
// integrity level, shows it with ShowWindow on that foreground change. If the
// window is still minimized after a bounded wait (controller stopped or busy),
// the system call runs unchanged.
#include "desktop-sweep-mark-latest.h"

namespace NativeDesktopRestore {
static decltype(&SwitchToThisWindow) original;
static std::atomic<unsigned long long> calls{},restored{},fallbacks{};
// Bounded wait on the caller's thread; the controller normally answers in a
// few milliseconds.
constexpr ULONGLONG kWaitMs=250;
static void WINAPI Hook(HWND window,BOOL altTab) {
    calls.fetch_add(1,std::memory_order_relaxed);
    if(window && LeanDockDesktopSweepMark::RestoresHere(IsIconic(window)!=FALSE,GetPropW(window,LeanDockDesktopSweepMark::Name)) &&
       !IsHungAppWindow(window)) {
        const bool activated=SetForegroundWindow(window)!=FALSE;
        const ULONGLONG start=GetTickCount64();
        while(activated && IsIconic(window) && GetTickCount64()-start<kWaitMs)Sleep(5);
        const auto waited=static_cast<unsigned long long>(GetTickCount64()-start);
        const bool back=!IsIconic(window);
        (back?restored:fallbacks).fetch_add(1,std::memory_order_relaxed);
        wchar_t line[160];
        _snwprintf_s(line,_TRUNCATE,L"desktop-restore %ls hwnd=%p activated=%d waited=%llums",
            back?L"shown":L"fallback",window,activated,waited);
        Log(line);
        if(back)return;
    }
    original(window,altTab);
}
// Part of the core hook transaction; the shared chains disable it with the
// other hooks when the Dock stops.
bool Initialize() noexcept {
    return InternalWh_SetFunctionHook(nullptr,reinterpret_cast<void*>(SwitchToThisWindow),
        reinterpret_cast<void*>(Hook),reinterpret_cast<void**>(&original));
}
}
