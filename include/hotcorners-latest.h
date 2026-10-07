#pragma once
#include <windows.h>
namespace NativeCorners {
inline constexpr UINT ControlMessage=WM_APP+221;
enum Command { Status=1, Pause, Resume, Reload };
bool Initialize(HWND owner,const wchar_t* root);
void Shutdown();
bool Dispatch(HWND owner,UINT message,WPARAM w,LPARAM l,LRESULT& result);
bool Snapshot();
bool Ready();
bool Validate(const wchar_t* root);
}
