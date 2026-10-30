#pragma once
#include <windows.h>
#include <cwchar>

namespace LeanDockSwitcherVisibility {
inline constexpr wchar_t WindowProperty[] = L"LeanDock.CtrlAltTab.Visible.v1";

// A live XAML root owns this mark, not the last corner activation. Removing an
// old root must never clear the mark installed by its replacement.
class WindowMark {
    HWND window{};
    HANDLE Owner() const { return reinterpret_cast<HANDLE>(const_cast<WindowMark*>(this)); }
public:
    WindowMark() = default;
    WindowMark(WindowMark const&) = delete;
    WindowMark& operator=(WindowMark const&) = delete;
    ~WindowMark() { Clear(); }
    void Clear() noexcept {
        if (window && GetPropW(window, WindowProperty) == Owner())
            RemovePropW(window, WindowProperty);
        window = nullptr;
    }
    bool Set(HWND next) noexcept {
        if (next == window && next && GetPropW(next, WindowProperty) == Owner()) return true;
        Clear();
        if (!next || !SetPropW(next, WindowProperty, Owner())) return false;
        window = next;
        return true;
    }
};

// The XAML callback checks the exact Switcher.AltTab root. The controller also
// checks the current foreground, shell owner, visibility and native host class.
// Task View and other XAML islands do not acquire a mark from this component.
inline HWND ActiveWindow() noexcept {
    HWND foreground = GetForegroundWindow();
    if (!foreground || !IsWindowVisible(foreground) || IsIconic(foreground) ||
        !GetPropW(foreground, WindowProperty)) return nullptr;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD foregroundPid=0, shellPid=0;
    if (!taskbar || !GetWindowThreadProcessId(foreground, &foregroundPid) ||
        !GetWindowThreadProcessId(taskbar, &shellPid) || foregroundPid!=shellPid) return nullptr;
    wchar_t name[64]{};
    if (!GetClassNameW(foreground, name, 64) || wcscmp(name, L"XamlExplorerHostIslandWindow")) return nullptr;
    return foreground;
}
}
