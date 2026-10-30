#pragma once
#include <cwchar>
#include <iterator>
#include <cstdio>
#include "search-position-latest.h"
#include "quick-lifecycle-latest.h"
#include "verify-latest.h"

namespace LeanDockSearchHost {
inline HMODULE module;
inline HOOKPROC procedure;
inline HWND target;
inline HANDLE targetProcess;
inline DWORD owner,attemptedPid;
inline bool supported,blocked,attaching;
inline unsigned failures;
inline LeanDockQuick::RestartPolicy restartPolicy;
inline DWORD lastExitCode;
inline DWORD lastError;
inline bool lastExitKnown;
// Controller-side adapter of the existing Quick host lifecycle.
inline bool Host(HWND w) {
    DWORD pid=0,session=0,ours=0;GetWindowThreadProcessId(w,&pid);
    return pid&&ProcessIdToSessionId(pid,&session)&&
        ProcessIdToSessionId(GetCurrentProcessId(),&ours)&&session==ours&&LeanDockSearch::Host(w);
}
inline bool Query() {
    return target&&Host(target)&&targetProcess&&WaitForSingleObject(targetProcess,0)==WAIT_TIMEOUT&&
        reinterpret_cast<ULONG_PTR>(GetPropW(target,LeanDockSearch::Owner))==owner&&GetPropW(target,LeanDockSearch::Active);
}
inline int Status(char* output,size_t capacity) {
    int n=snprintf(output,capacity,"\"searchHookPid\":%llu,\"searchHookSignal\":%llu,\"searchHookMessage\":%llu,\"searchInitStage\":%llu,\"searchInitError\":%llu,",
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(GetPropW(target,LeanDockSearch::HookPid))),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(GetPropW(target,LeanDockSearch::HookSignal))),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(GetPropW(target,LeanDockSearch::HookMessage))),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(GetPropW(target,LeanDockSearch::InitStage))),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(GetPropW(target,LeanDockSearch::InitError))));
    return n+snprintf(output+n,capacity-n,"\"searchPositionSupported\":%s,\"searchPositionActive\":%s,\"searchPositionBlocked\":%s,\"searchPositionFailures\":%u,\"searchPositionError\":%lu,\"searchPositionAdjustments\":%llu,\"searchPositionWindow\":%llu,",
        supported?"true":"false",Query()?"true":"false",blocked?"true":"false",failures,lastError,
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(GetPropW(target,LeanDockSearch::Adjustments))),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(target)));
}
inline bool Verify() {
    // Deliberately pin the actual Windows build even though positioning uses
    // only public APIs. A changed shell leaves its native placement intact.
    wchar_t path[32768]{},actual[65]{};GetWindowsDirectoryW(path,std::size(path));
    wcscat_s(path,L"\\SystemApps\\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\\SearchHost.exe");
    return NativeHashFile(path,actual)&&!_wcsicmp(actual,
        L"E5846EC8EA9494941AF388C83E3675580E8100D26D099F273AC3D549FCCCD0DF");
}
inline bool Attach(HWND w) {
    if(!supported||blocked||attaching||!procedure||!Host(w))return false;
    if(targetProcess&&WaitForSingleObject(targetProcess,0)!=WAIT_TIMEOUT){
        lastExitKnown=GetExitCodeProcess(targetProcess,&lastExitCode)!=FALSE;
        const bool recover=restartPolicy.Retired(lastExitKnown,lastExitCode,GetTickCount64());
        CloseHandle(targetProcess);targetProcess=nullptr;target=nullptr;attemptedPid=0;
        if(!recover){blocked=true;++failures;return false;}
    }
    if(target==w&&Query())return true;
    DWORD pid=0;const DWORD tid=GetWindowThreadProcessId(w,&pid);
    // Never reuse the tracked handle for another still-live host.
    if(targetProcess&&GetProcessId(targetProcess)!=pid)return false;
    if(attemptedPid==pid)return false;
    attaching=true;
    struct Guard{~Guard(){attaching=false;}} guard;
    // Search positioning does not initialize a XAML template and can attach while closed. Check responsiveness
    // before spending the single attempt, so a suspended host can retry on SHOW.
    DWORD_PTR result=0;
    if(!SendMessageTimeoutW(w,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,300,&result))return false;
    attemptedPid=pid;
    if(!targetProcess)targetProcess=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_DUP_HANDLE,FALSE,pid);
    if(!targetProcess){lastError=GetLastError();++failures;return false;}
    HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROC,procedure,module,tid);
    if(!hook){lastError=GetLastError();++failures;return false;}
    target=w;
    // An AppContainer cannot OpenProcess the controller. Explicitly offer only
    // QUERY_LIMITED_INFORMATION | SYNCHRONIZE to this verified, pinned host.
    // No VM access, termination, write, or privilege manipulation is granted.
    HANDLE offer=nullptr;
    if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),targetProcess,&offer,
        SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,0)){
        lastError=GetLastError();UnhookWindowsHookEx(hook);++failures;return false;
    }
    const bool sent=SendMessageTimeoutW(w,LeanDockSearch::Signal(),LeanDockSearch::Enable,reinterpret_cast<LPARAM>(offer),
        SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result)!=0;
    lastError=sent?0:GetLastError();
    UnhookWindowsHookEx(hook);
    // The runtime clones the offer for its own wait. Only revoke the offer
    // after synchronous completion. A timeout can still have a late callback;
    // leave at most this one read-only handle until host retirement, no retry.
    if(sent){HANDLE returned=nullptr;
        DuplicateHandle(targetProcess,offer,GetCurrentProcess(),&returned,0,FALSE,DUPLICATE_CLOSE_SOURCE);
        if(returned)CloseHandle(returned);
    }
    // A successful host may later recreate its window in the same process.
    // Permit that new lifecycle; only a failed attempt is latched for this PID.
    if(sent&&Query()){attemptedPid=0;lastError=0;return true;}
    if(!lastError)lastError=ERROR_DLL_INIT_FAILED;
    ++failures;return false;
}
inline bool Start(const wchar_t* directory) {
    owner=GetCurrentProcessId();supported=Verify();if(!supported)return false;
    wchar_t path[32768]{};_snwprintf_s(path,_TRUNCATE,L"%ls\\start\\latest.dll",directory);
    module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module){supported=false;return false;}
    procedure=reinterpret_cast<HOOKPROC>(GetProcAddress(module,"LeanDockSearchHook"));
    if(!procedure){FreeLibrary(module);module=nullptr;supported=false;return false;}
    HWND w=nullptr;while((w=FindWindowExW(nullptr,w,L"Windows.UI.Core.CoreWindow",nullptr)))if(Host(w)){Attach(w);break;}
    return true;
}
inline void Stop() {
    if(target&&Host(target)){
        DWORD_PTR result=0;
        if(!SendMessageTimeoutW(target,LeanDockSearch::Signal(),LeanDockSearch::Disable,owner,
            SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result)||result!=1)++failures;
        for(auto key:{LeanDockSearch::HookPid,LeanDockSearch::HookSignal,LeanDockSearch::HookMessage,
                     LeanDockSearch::InitStage,LeanDockSearch::InitError})RemovePropW(target,key);
    }
    if(targetProcess){CloseHandle(targetProcess);targetProcess=nullptr;}
    target=nullptr;procedure=nullptr;owner=attemptedPid=0;supported=blocked=attaching=false;
    restartPolicy={};lastExitCode=0;lastExitKnown=false;
    if(module){FreeLibrary(module);module=nullptr;}
}
}
