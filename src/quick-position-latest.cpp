#include <windows.h>
#include <commctrl.h>
#include <cwchar>
#include <iterator>
#include "quick-position-latest.h"

// One window, on its owning thread. No detours, private interfaces, keyboard
// synthesis, polling or new process. The controller loads this companion once.
namespace {
HMODULE module;
HWND window;
DWORD owner;
HANDLE ownerProcess;
PTP_WAIT ownerWait;
bool active,restoring,moved;
RECT original{},applied{};
ULONG_PTR changes;

bool IsHost(HWND w) {
    DWORD pid=0;wchar_t cls[64]{},path[32768]{},expected[32768]{};
    if(GetWindowThreadProcessId(w,&pid)!=GetCurrentThreadId()||pid!=GetCurrentProcessId()||
       GetAncestor(w,GA_ROOT)!=w||!GetClassNameW(w,cls,std::size(cls))||wcscmp(cls,L"ControlCenterWindow"))return false;
    GetWindowsDirectoryW(expected,std::size(expected));wcscat_s(expected,L"\\System32\\ShellHost.exe");
    return GetModuleFileNameW(nullptr,path,std::size(path))&&!_wcsicmp(path,expected);
}
HANDLE ValidOwner(DWORD pid) {
    wchar_t dll[32768]{},expected[32768]{},actual[32768]{};
    if(!GetModuleFileNameW(module,dll,std::size(dll)))return nullptr;
    auto slash=wcsrchr(dll,L'\\');if(!slash)return nullptr;*slash=0;
    slash=wcsrchr(dll,L'\\');if(!slash)return nullptr;*slash=0;
    _snwprintf_s(expected,_TRUNCATE,L"%ls\\latest.exe",dll);
    HANDLE p=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return nullptr;
    DWORD n=std::size(actual),session=0,ours=0;
    if(!QueryFullProcessImageNameW(p,0,actual,&n)||_wcsicmp(actual,expected)||
       !ProcessIdToSessionId(pid,&session)||!ProcessIdToSessionId(GetCurrentProcessId(),&ours)||session!=ours||
       WaitForSingleObject(p,0)!=WAIT_TIMEOUT){CloseHandle(p);return nullptr;}
    return p;
}
bool DockFor(const RECT& proposed,RECT& dock,MONITORINFO& info) {
    const auto monitor=MonitorFromRect(&proposed,MONITOR_DEFAULTTONEAREST);
    for(auto cls:{L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"}){
        HWND bar=nullptr;
        while((bar=FindWindowExW(nullptr,bar,cls,nullptr)))
            if(MonitorFromWindow(bar,MONITOR_DEFAULTTONEAREST)==monitor && LeanDockGeometry::Read(bar,dock,&info))return true;
    }
    return false;
}
LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
void Stop(bool restore=true) {
    active=false;
    if(ownerWait){SetThreadpoolWait(ownerWait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(ownerWait,TRUE);CloseThreadpoolWait(ownerWait);ownerWait=nullptr;}
    if(ownerProcess){CloseHandle(ownerProcess);ownerProcess=nullptr;}
    HWND w=window;window=nullptr;
    if(w){
        RemoveWindowSubclass(w,WindowProc,LeanDockQuick::SubclassId);
        for(auto key:{LeanDockQuick::Active,LeanDockQuick::Owner,LeanDockQuick::Adjustments})RemovePropW(w,key);
        RECT current{};
        if(restore&&moved&&IsWindow(w)&&GetWindowRect(w,&current)&&EqualRect(&current,&applied)){
            restoring=true;
            SetWindowPos(w,nullptr,original.left,original.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            restoring=false;
        }
    }
    moved=false;owner=0;
}
void CALLBACK OwnerExited(PTP_CALLBACK_INSTANCE,void* context,PTP_WAIT,TP_WAIT_RESULT) {
    PostMessageW(reinterpret_cast<HWND>(context),LeanDockQuick::Signal(),LeanDockQuick::OwnerGone,0);
}
void Position(WINDOWPOS& pos) {
    if(!active||restoring||pos.hwnd!=window)return;
    if(!ownerProcess||WaitForSingleObject(ownerProcess,0)!=WAIT_TIMEOUT){Stop(false);return;}
    RECT current{};if(!GetWindowRect(window,&current))return;
    RECT requested=current;
    if(!(pos.flags&SWP_NOMOVE)){OffsetRect(&requested,pos.x-current.left,pos.y-current.top);}
    if(!(pos.flags&SWP_NOSIZE)){requested.right=requested.left+pos.cx;requested.bottom=requested.top+pos.cy;}
    RECT dock{};MONITORINFO info{};
    if(!DockFor(requested,dock,info))return;
    RECT native{};
    if(!LeanDockQuick::Adjust(pos,current,dock,info.rcMonitor,GetDpiForWindow(window),&native))return;
    if(!moved||(!(pos.flags&SWP_NOSIZE)&&!EqualRect(&native,&applied))||
       (native.left!=applied.left||native.top!=applied.top))original=native;
    applied={pos.x,pos.y,pos.x+(native.right-native.left),pos.y+(native.bottom-native.top)};moved=true;
    SetPropW(window,LeanDockQuick::Adjustments,reinterpret_cast<HANDLE>(++changes));
}
LRESULT CALLBACK WindowProc(HWND w,UINT message,WPARAM p,LPARAM l,UINT_PTR,DWORD_PTR) {
    if(message==LeanDockQuick::Signal()){
        if(p==LeanDockQuick::Query)return active&&ownerProcess&&WaitForSingleObject(ownerProcess,0)==WAIT_TIMEOUT;
        if(p==LeanDockQuick::Disable&&DWORD(l)==owner){Stop();return 1;}
        if(p==LeanDockQuick::OwnerGone&&ownerProcess&&WaitForSingleObject(ownerProcess,0)==WAIT_OBJECT_0){Stop();return 1;}
    }
    if(message==WM_NCDESTROY){Stop(false);return DefSubclassProc(w,message,p,l);}
    // Let the native procedure enforce size constraints first, then alter only
    // the pending coordinates. WM_WINDOWPOSCHANGED sees the committed position,
    // so the visual surface and native input both receive the same move.
    const auto result=DefSubclassProc(w,message,p,l);
    if(message==WM_WINDOWPOSCHANGING&&l)Position(*reinterpret_cast<WINDOWPOS*>(l));
    return result;
}
bool Start(HWND w,DWORD pid) {
    if(!IsHost(w))return false;
    if(active){
        if(owner==pid&&window==w)return true;
        if(ownerProcess&&WaitForSingleObject(ownerProcess,0)==WAIT_TIMEOUT)return false;
        Stop();
    }
    auto process=ValidOwner(pid);if(!process)return false;
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<PCWSTR>(WindowProc),&pinned)){CloseHandle(process);return false;}
    if(!SetWindowSubclass(w,WindowProc,LeanDockQuick::SubclassId,0)){CloseHandle(process);return false;}
    window=w;owner=pid;ownerProcess=process;changes=0;moved=false;active=true;
    ownerWait=CreateThreadpoolWait(OwnerExited,w,nullptr);
    if(!ownerWait){Stop(false);return false;}
    SetThreadpoolWait(ownerWait,ownerProcess,nullptr);
    SetPropW(w,LeanDockQuick::Owner,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid)));
    SetPropW(w,LeanDockQuick::Active,reinterpret_cast<HANDLE>(1));
    // Prime even a hidden host. Later native moves and show requests are
    // intercepted synchronously; nothing is restored on hide/close.
    RECT r{};if(GetWindowRect(w,&r))SetWindowPos(w,nullptr,r.left,r.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    return true;
}
}
extern "C" __declspec(dllexport) LRESULT CALLBACK LeanDockQuickHook(int code,WPARAM p,LPARAM l) {
    if(code==HC_ACTION){auto call=reinterpret_cast<CWPSTRUCT*>(l);
        if(call->message==LeanDockQuick::Signal()&&call->wParam==LeanDockQuick::Enable)
            Start(call->hwnd,DWORD(call->lParam));
    }
    return CallNextHookEx(nullptr,code,p,l);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);}return TRUE;
}
