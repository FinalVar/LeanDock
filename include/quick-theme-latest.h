#pragma once
#include <windows.h>
namespace LeanDockQuickTheme {
void Start(HWND host,HMODULE module) noexcept;
void Stop() noexcept;
void WindowMessage(UINT message,WPARAM p,LPARAM l) noexcept;
}
