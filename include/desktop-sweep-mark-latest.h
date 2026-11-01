#pragma once
#include <windows.h>

// Shared between the hot-corner controller and the Explorer-side runtime.
// The desktop corner minimizes the windows Win+D only covered and tags each
// with this window property. The taskbar and Alt+Tab bring a minimized window
// back with SwitchToThisWindow, which posts WM_SYSCOMMAND SC_RESTORE; Windows
// Terminal leaves fullscreen on SC_RESTORE (IslandWindow.cpp) and stays
// minimized. For a tagged window the runtime in Explorer only activates it;
// the controller, which tagged it at its integrity level, then shows it with
// ShowWindow and no system command, so it returns unchanged.
namespace LeanDockDesktopSweepMark {
inline constexpr wchar_t Name[]=L"LeanDock.DesktopSweep.v1.Minimized";
inline const HANDLE Value=reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(1));
// Pure: only a still-minimized window carrying the tag is returned this way;
// everything else takes the system path unchanged.
inline bool RestoresHere(bool iconic,HANDLE tag) {return iconic && tag==Value;}
}
