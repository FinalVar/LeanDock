#pragma once
#include <windows.h>
#include <cwchar>

namespace LeanDockTheme {
// Completion must reach hidden top-level listeners too. A taskbar-only send
// can update its current tree while leaving other shell theme caches stale.
// Match Auto Theme Switcher's native broadcast contract; no visibility or
// traversal budget applies here. The timeout is per receiver, not per pass.
inline bool BroadcastAppearance() {
    return SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,
        reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
        SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,100,nullptr)!=0;
}
struct TaskbarNotifyResult { unsigned matched=0,delivered=0,failed=0; };

inline TaskbarNotifyResult NotifyTaskbarsForProcess(DWORD shellPid) {
    struct Context { DWORD pid;TaskbarNotifyResult result; } context{shellPid,{}};
    if(!shellPid)return {};
    // Separate from the 75 ms visible-app fast path: auto-hidden/cloaked
    // taskbars still own live XAML resources and must receive the new mode.
    EnumWindows([](HWND window,LPARAM data)->BOOL {
        auto& context=*reinterpret_cast<Context*>(data);
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(pid!=context.pid)return TRUE;
        wchar_t name[64]{};
        if(!GetClassNameW(window,name,64)||
           (wcscmp(name,L"Shell_TrayWnd")&&wcscmp(name,L"Shell_SecondaryTrayWnd")))return TRUE;
        ++context.result.matched;
        if(SendMessageTimeoutW(window,WM_SETTINGCHANGE,0,
            reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
            SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,100,nullptr))++context.result.delivered;
        else ++context.result.failed;
        return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    return context.result;
}

inline TaskbarNotifyResult NotifyTaskbars() {
    DWORD shellPid=0;
    if(auto shell=GetShellWindow())GetWindowThreadProcessId(shell,&shellPid);
    return NotifyTaskbarsForProcess(shellPid);
}
}
