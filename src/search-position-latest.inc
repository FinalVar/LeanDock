// Adapted from src/quick-position-latest.cpp (LeanDock, GPL-3.0).
// Public per-thread subclass/owner wait/rollback lifecycle; no XAML internals,
// theme tile, symbol detours, input synthesis, or persistent polling.
// Included in the Start companion; each host has its own process-local state.
#include "search-position-latest.h"
#include "protocol-latest.h"
namespace LeanDockSearchRuntime {
HWND window;
DWORD owner;
HANDLE ownerProcess;
PTP_WAIT ownerWait;
bool active,restoring,moved;
RECT original{},applied{};
ULONG_PTR changes;

bool IsHost(HWND w) {
    DWORD pid=0;
    return GetWindowThreadProcessId(w,&pid)==GetCurrentThreadId() &&
        pid==GetCurrentProcessId() && LeanDockSearch::Host(w);
}
HANDLE ValidOwner(HANDLE offer) {
    wchar_t dll[32768]{},expected[32768]{},actual[32768]{};
    if(!GetModuleFileNameW(module,dll,std::size(dll)))return nullptr;
    auto slash=wcsrchr(dll,L'\\');if(!slash)return nullptr;*slash=0;
    slash=wcsrchr(dll,L'\\');if(!slash)return nullptr;*slash=0;
    _snwprintf_s(expected,_TRUNCATE,L"%ls\\latest.exe",dll);
    // Clone the specifically offered read-only handle; the controller owns
    // the original offer and revokes it after this call returns.
    HANDLE p=nullptr;
    if(!offer||offer==INVALID_HANDLE_VALUE||!DuplicateHandle(GetCurrentProcess(),offer,GetCurrentProcess(),&p,0,FALSE,DUPLICATE_SAME_ACCESS))return nullptr;
    const DWORD pid=GetProcessId(p);
    DWORD n=std::size(actual);
    if(!QueryFullProcessImageNameW(p,0,actual,&n)){const DWORD error=GetLastError();CloseHandle(p);SetLastError(error);return nullptr;}
    // ProcessIdToSessionId opens the elevated owner again and is denied from
    // Search's AppContainer despite the explicitly offered read-only handle.
    // Instead match that live handle to the controller window visible on this
    // desktop; window enumeration is confined to our session/desktop. The
    // controller also verifies the Search session before offering the handle.
    DWORD windowPid=0;
    const HWND controller=FindWindowW(NativeControllerClass,nullptr);
    if(controller)GetWindowThreadProcessId(controller,&windowPid);
    if(!pid||_wcsicmp(actual,expected)||!controller||windowPid!=pid||
       WaitForSingleObject(p,0)!=WAIT_TIMEOUT){CloseHandle(p);SetLastError(ERROR_INVALID_OWNER);return nullptr;}
    return p;
}
LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
void Stop(bool restore=true) {
    active=false;
    if(ownerWait){SetThreadpoolWait(ownerWait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(ownerWait,TRUE);CloseThreadpoolWait(ownerWait);ownerWait=nullptr;}
    if(ownerProcess){CloseHandle(ownerProcess);ownerProcess=nullptr;}
    HWND w=window;window=nullptr;
    if(w){
        RemoveWindowSubclass(w,WindowProc,LeanDockSearch::SubclassId);
        for(auto key:{LeanDockSearch::Active,LeanDockSearch::Owner,LeanDockSearch::Adjustments,
                     LeanDockSearch::HookPid,LeanDockSearch::HookSignal,LeanDockSearch::HookMessage,
                     LeanDockSearch::InitStage,LeanDockSearch::InitError})RemovePropW(w,key);
        RECT current{};
        if(restore&&moved&&IsWindow(w)&&GetWindowRect(w,&current)&&EqualRect(&current,&applied)){
            restoring=true;
            SetWindowPos(w,nullptr,original.left,original.top,original.right-original.left,original.bottom-original.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
            restoring=false;
        }
    }
    moved=false;owner=0;
}
void CALLBACK OwnerExited(PTP_CALLBACK_INSTANCE,void* context,PTP_WAIT,TP_WAIT_RESULT) {
    PostMessageW(reinterpret_cast<HWND>(context),LeanDockSearch::Signal(),LeanDockSearch::OwnerGone,0);
}
void Position(WINDOWPOS& pos) {
    if(!active||restoring||pos.hwnd!=window)return;
    if(!ownerProcess||WaitForSingleObject(ownerProcess,0)!=WAIT_TIMEOUT){Stop(false);return;}
    RECT current{};if(!GetWindowRect(window,&current))return;
    RECT requested=current;
    if(!(pos.flags&SWP_NOMOVE)){OffsetRect(&requested,pos.x-current.left,pos.y-current.top);}
    if(!(pos.flags&SWP_NOSIZE)){requested.right=requested.left+pos.cx;requested.bottom=requested.top+pos.cy;}
    RECT dock{};MONITORINFO info{};
    if(!LeanDockPlacement::DockFor(requested,dock,info))return;
    RECT native{};
    if(!LeanDockSearch::Adjust(pos,current,dock,info.rcMonitor,GetDpiForWindow(window),&native))return;
    if(!moved||!EqualRect(&native,&applied))original=native;
    const LONG width=(pos.flags&SWP_NOSIZE)?current.right-current.left:pos.cx;
    const LONG height=(pos.flags&SWP_NOSIZE)?current.bottom-current.top:pos.cy;
    applied={pos.x,pos.y,pos.x+width,pos.y+height};moved=true;
    SetPropW(window,LeanDockSearch::Adjustments,reinterpret_cast<HANDLE>(++changes));
}
LRESULT CALLBACK WindowProc(HWND w,UINT message,WPARAM p,LPARAM l,UINT_PTR,DWORD_PTR) {
    if(message==LeanDockSearch::Signal()){
        if(p==LeanDockSearch::Query)return active&&ownerProcess&&WaitForSingleObject(ownerProcess,0)==WAIT_TIMEOUT;
        if(p==LeanDockSearch::Disable&&DWORD(l)==owner){Stop();return 1;}
        if(p==LeanDockSearch::OwnerGone&&ownerProcess&&WaitForSingleObject(ownerProcess,0)==WAIT_OBJECT_0){Stop();return 1;}
    }
    if(message==WM_NCDESTROY){Stop(false);return DefSubclassProc(w,message,p,l);}
    // Let the native procedure enforce size constraints first, then alter only
    // the pending rectangle. WM_WINDOWPOSCHANGED sees the committed position,
    // so the visual surface and native input both receive the same move.
    const auto result=DefSubclassProc(w,message,p,l);
    if(message==WM_WINDOWPOSCHANGING&&l)LeanDockSearchRuntime::Position(*reinterpret_cast<WINDOWPOS*>(l));
    return result;
}
bool Start(HWND w,HANDLE offer) {
    RemovePropW(w,LeanDockSearch::InitError);
    SetPropW(w,LeanDockSearch::InitStage,HANDLE(1));
    if(!IsHost(w)){SetPropW(w,LeanDockSearch::InitError,HANDLE(ERROR_INVALID_WINDOW_HANDLE));return false;}
    SetPropW(w,LeanDockSearch::InitStage,HANDLE(2));
    SetLastError(0);
    auto process=ValidOwner(offer);if(!process){SetPropW(w,LeanDockSearch::InitError,HANDLE(uintptr_t(GetLastError())));return false;}
    const DWORD pid=GetProcessId(process);
    if(active){
        if(owner==pid&&window==w){CloseHandle(process);return true;}
        if(ownerProcess&&WaitForSingleObject(ownerProcess,0)==WAIT_TIMEOUT){CloseHandle(process);return false;}
        Stop();
    }
    HMODULE pinned=nullptr;
    SetPropW(w,LeanDockSearch::InitStage,HANDLE(3));
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<PCWSTR>(WindowProc),&pinned)){CloseHandle(process);return false;}
    if(!SetWindowSubclass(w,WindowProc,LeanDockSearch::SubclassId,0)){CloseHandle(process);return false;}
    SetPropW(w,LeanDockSearch::InitStage,HANDLE(4));
    window=w;owner=pid;ownerProcess=process;changes=0;moved=false;active=true;
    ownerWait=CreateThreadpoolWait(OwnerExited,w,nullptr);
    if(!ownerWait){Stop(false);return false;}
    SetPropW(w,LeanDockSearch::InitStage,HANDLE(5));
    SetThreadpoolWait(ownerWait,ownerProcess,nullptr);
    SetPropW(w,LeanDockSearch::Owner,reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(pid)));
    SetPropW(w,LeanDockSearch::Active,reinterpret_cast<HANDLE>(1));
    // Prime only a hidden host. Later native moves and show requests are
    // intercepted synchronously; nothing is restored on hide/close.
    RECT r{};if(!LeanDockStartState::Shown(w)&&GetWindowRect(w,&r))SetWindowPos(w,nullptr,r.left,r.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    return true;
}
}
extern "C" __declspec(dllexport) LRESULT CALLBACK LeanDockSearchHook(int code,WPARAM p,LPARAM l) {
    if(code==HC_ACTION){auto call=reinterpret_cast<CWPSTRUCT*>(l);
        if(call->message>=0xC000&&call->wParam==LeanDockSearch::Enable){
            SetPropW(call->hwnd,LeanDockSearch::HookPid,HANDLE(uintptr_t(GetCurrentProcessId())));
            SetPropW(call->hwnd,LeanDockSearch::HookSignal,HANDLE(uintptr_t(LeanDockSearch::Signal())));
            SetPropW(call->hwnd,LeanDockSearch::HookMessage,HANDLE(uintptr_t(call->message)));
        }
        if(call->message==LeanDockSearch::Signal()&&call->wParam==LeanDockSearch::Enable)
            LeanDockSearchRuntime::Start(call->hwnd,reinterpret_cast<HANDLE>(call->lParam));
    }
    return CallNextHookEx(nullptr,code,p,l);
}
