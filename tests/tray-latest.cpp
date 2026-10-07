// Disposable native notification-icon fixture. It never sends mouse/keyboard
// input or reads tooltip text. All fixture icons are removed before exit.
#include <windows.h>
#include <shellapi.h>
#include <cwchar>
#include <iterator>
#include "protocol-latest.h"
#include "dock-geometry-latest.h"
#include <cstdio>
static NOTIFYICONDATAW icons[2]{};
static unsigned step=0;
static const int counts[]={0,1,2,1,0};
static bool visible[2]{};
static wchar_t root[32768],out[32768];
static void SetCount(int count) {
    for(int i=0;i<2;++i)if(visible[i]!=(i<count)){
        if(Shell_NotifyIconW(i<count?NIM_ADD:NIM_DELETE,&icons[i]))visible[i]=i<count;
    }
}
static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM p,LPARAM l) {
    if(m==WM_TIMER){
        HWND backend=FindWindowW(NativeBackendClass,nullptr);
        DWORD_PTR state=0;
        bool ok=backend && SendMessageTimeoutW(backend,NativeQuery,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&state) && state==NativeActive;
        wchar_t source[32768],target[32768];
        _snwprintf_s(source,_TRUNCATE,L"%ls\\latest-layout.json",root);
        _snwprintf_s(target,_TRUNCATE,L"%ls\\tray-stage-%u.json",out,step);
        if(!ok || !CopyFileW(source,target,FALSE)){SetCount(0);PostQuitMessage(2);return 0;}
        HWND bar=FindWindowW(L"Shell_TrayWnd",nullptr);RECT dock{},clip{};
        bool geometry=LeanDockGeometry::Read(bar,dock);int region=GetWindowRgnBox(bar,&clip);
        _snwprintf_s(target,_TRUNCATE,L"%ls\\geometry-stage-%u.json",out,step);
        if(FILE* file=_wfopen(target,L"wb")) {
            fprintf(file,"{\"valid\":%s,\"region\":%d,\"left\":%ld,\"right\":%ld,\"clipLeft\":%ld,\"clipRight\":%ld}\n",geometry?"true":"false",region,dock.left,dock.right,clip.left,clip.right);fclose(file);
        }
        if(++step==std::size(counts)){SetCount(0);KillTimer(w,1);PostQuitMessage(0);return 0;}
        SetCount(counts[step]);return 0;
    }
    return DefWindowProcW(w,m,p,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int n=0;auto args=CommandLineToArgvW(GetCommandLineW(),&n);
    if(n!=3)return 1;wcscpy_s(root,args[1]);wcscpy_s(out,args[2]);LocalFree(args);
    WNDCLASSW c{};c.hInstance=instance;c.lpfnWndProc=Proc;c.lpszClassName=L"NativeDock.TrayFixture";RegisterClassW(&c);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,c.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
    if(!window)return 1;
    for(int i=0;i<2;++i){
        icons[i].cbSize=sizeof(icons[i]);icons[i].hWnd=window;icons[i].uID=i+1;
        icons[i].uFlags=NIF_ICON|NIF_TIP;icons[i].hIcon=LoadIconW(nullptr,i?IDI_INFORMATION:IDI_WARNING);
        _snwprintf_s(icons[i].szTip,_TRUNCATE,L"NativeTaskbarDock temporary test %d",i+1);
    }
    SetTimer(window,1,2200,nullptr);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    SetCount(0);DestroyWindow(window);return static_cast<int>(msg.wParam);
}
