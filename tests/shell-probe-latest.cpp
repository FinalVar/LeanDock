#include <windows.h>
#include <dwmapi.h>
#include <cstdio>
#include <cwchar>
#include "../include/dock-geometry-latest.h"
static BOOL CALLBACK Inspect(HWND w, LPARAM) {
    DWORD pid=0; GetWindowThreadProcessId(w,&pid);
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!p)return TRUE;
    wchar_t path[32768]{}; DWORD size=32768;
    bool ok=QueryFullProcessImageNameW(p,0,path,&size); CloseHandle(p);
    if(!ok)return TRUE;
    const auto name=wcsrchr(path,L'\\');if(!name)return TRUE;
    if(_wcsicmp(name+1,L"StartMenuExperienceHost.exe") &&
       _wcsicmp(name+1,L"ShellHost.exe") && _wcsicmp(name+1,L"ShellExperienceHost.exe"))return TRUE;
    wchar_t cls[128]{},title[128]{}; GetClassNameW(w,cls,128);GetWindowTextW(w,title,128);
    RECT r{}; GetWindowRect(w,&r); DWORD cloaked=0;DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));
    wprintf(L"pid=%lu hwnd=%p visible=%d cloak=%lu rect=%ld,%ld,%ld,%ld class=%ls title=%ls exe=%ls\n",pid,w,IsWindowVisible(w),cloaked,r.left,r.top,r.right,r.bottom,cls,title,name+1);
    return TRUE;
}
int main(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HWND w=nullptr;while((w=FindWindowExW(nullptr,w,nullptr,nullptr)))Inspect(w,0);
    w=FindWindowW(L"Shell_TrayWnd",nullptr);RECT r{},clip{},dock{};
    GetWindowRect(w,&r);int region=GetWindowRgnBox(w,&clip);bool valid=LeanDockGeometry::Read(w,dock);
    printf("taskbar rect=%ld,%ld,%ld,%ld region=%d clip=%ld,%ld,%ld,%ld geometry=%d dock=%ld,%ld,%ld,%ld\n",r.left,r.top,r.right,r.bottom,region,clip.left,clip.top,clip.right,clip.bottom,valid,dock.left,dock.top,dock.right,dock.bottom);
}
