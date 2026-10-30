#pragma once
#include <windows.h>
#include <cwchar>
#include <iterator>

// The capsule's tray relay deliberately also uses Shell_TrayWnd. Shell
// companions must resolve Explorer's real window, never the first class match.
inline HWND LeanDockNativeShell() {
    wchar_t expected[MAX_PATH]{};
    if(!GetWindowsDirectoryW(expected,MAX_PATH))return nullptr;
    wcscat_s(expected,L"\\explorer.exe");
    HWND w=nullptr;
    while((w=FindWindowExW(nullptr,w,L"Shell_TrayWnd",nullptr))){
        DWORD pid=0;GetWindowThreadProcessId(w,&pid);
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!p)continue;
        wchar_t path[32768]{};DWORD n=std::size(path),session=0,ours=0;
        bool ok=QueryFullProcessImageNameW(p,0,path,&n)&&!_wcsicmp(path,expected)&&
            ProcessIdToSessionId(pid,&session)&&ProcessIdToSessionId(GetCurrentProcessId(),&ours)&&session==ours;
        CloseHandle(p);if(ok)return w;
    }
    return nullptr;
}
