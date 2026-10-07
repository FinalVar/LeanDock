#pragma once
#include <windows.h>
inline constexpr wchar_t NativeControllerClass[] = L"NativeTaskbarDock.Controller.v1";
inline constexpr wchar_t NativeBackendClass[] = L"NativeTaskbarDock.Backend.v1";
inline constexpr UINT NativeQuery = WM_APP + 201;
inline constexpr UINT NativeStop = WM_APP + 202;
inline constexpr UINT NativeStart = WM_APP + 203;
inline constexpr UINT NativeOwnerExit = WM_APP + 204;
inline constexpr UINT NativeReconnect = WM_APP + 205;
enum NativeState : unsigned { NativeIdle=0, NativeStarting=1, NativeActive=2, NativeStopped=3, NativeFailed=4 };
inline constexpr unsigned NativeProtocolVersion = 1;
