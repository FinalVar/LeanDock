#pragma once
#include <cwchar>
#include <iterator>
#include "quick-position-latest.h"
#include "quick-lifecycle-latest.h"
#include "verify-latest.h"

namespace LeanDockQuickHost {
inline HMODULE module;
inline HOOKPROC procedure;
inline HWND target;
inline HANDLE targetProcess;
inline DWORD owner,attemptedPid;
inline bool supported,blocked,attaching;
inline unsigned failures;
inline LeanDockQuick::RestartPolicy restartPolicy;
inline DWORD lastExitCode;
inline bool lastExitKnown;
inline bool Host(HWND w) {
    DWORD pid=0,session=0,ours=0;wchar_t cls[64]{};
    if(!w||GetAncestor(w,GA_ROOT)!=w||!GetClassNameW(w,cls,std::size(cls))||wcscmp(cls,L"ControlCenterWindow"))return false;
    GetWindowThreadProcessId(w,&pid);
    if(!pid||!ProcessIdToSessionId(pid,&session)||!ProcessIdToSessionId(GetCurrentProcessId(),&ours)||session!=ours)return false;
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
    wchar_t path[32768]{},expected[32768]{};DWORD n=std::size(path);
    GetWindowsDirectoryW(expected,std::size(expected));wcscat_s(expected,L"\\System32\\ShellHost.exe");
    const bool valid=QueryFullProcessImageNameW(p,0,path,&n)&&!_wcsicmp(path,expected);CloseHandle(p);return valid;
}
inline bool Query() {
    return target&&Host(target)&&targetProcess&&WaitForSingleObject(targetProcess,0)==WAIT_TIMEOUT&&
        reinterpret_cast<ULONG_PTR>(GetPropW(target,LeanDockQuick::Owner))==owner&&GetPropW(target,LeanDockQuick::Active);
}
inline bool Verify() {
    const struct{const wchar_t* name;const wchar_t* sha;} pins[]={
        {L"ShellHost.exe",L"371163BA43B42D1F1C13AB7EA200F15EF5E60EA8D3514D00D3FFC572E53A436D"},
        {L"ControlCenter.dll",L"0A9A84B0B7ED6110A410BD76150CE0DA04DC29B901838155AB7CE2D8BF2FFB51"}
    };
    wchar_t base[32768]{};GetWindowsDirectoryW(base,std::size(base));
    for(auto const& pin:pins){wchar_t path[32768]{},actual[65]{};
        _snwprintf_s(path,_TRUNCATE,L"%ls\\System32\\%ls",base,pin.name);
        if(!NativeHashFile(path,actual)||_wcsicmp(actual,pin.sha))return false;
    }
    return true;
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
    // Unlike Start, this host can be primed while closed. Check responsiveness
    // before spending the single attempt, so a suspended host can retry on SHOW.
    DWORD_PTR result=0;
    if(!SendMessageTimeoutW(w,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,300,&result))return false;
    attemptedPid=pid;
    if(!targetProcess)targetProcess=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!targetProcess){++failures;return false;}
    HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROC,procedure,module,tid);
    if(!hook){++failures;return false;}
    target=w;
    const bool sent=SendMessageTimeoutW(w,LeanDockQuick::Signal(),LeanDockQuick::Enable,owner,
        SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result)!=0;
    UnhookWindowsHookEx(hook);
    // A successful host may later recreate its window in the same process.
    // Permit that new lifecycle; only a failed attempt is latched for this PID.
    if(sent&&Query()){attemptedPid=0;return true;}
    ++failures;return false;
}
inline bool Start(const wchar_t* directory) {
    owner=GetCurrentProcessId();supported=Verify();if(!supported)return false;
    wchar_t path[32768]{};_snwprintf_s(path,_TRUNCATE,L"%ls\\panels\\latest.dll",directory);
    module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module){supported=false;return false;}
    procedure=reinterpret_cast<HOOKPROC>(GetProcAddress(module,"LeanDockQuickHook"));
    if(!procedure){FreeLibrary(module);module=nullptr;supported=false;return false;}
    HWND w=nullptr;while((w=FindWindowExW(nullptr,w,L"ControlCenterWindow",nullptr)))if(Host(w)){Attach(w);break;}
    return true;
}
inline void Stop() {
    if(target&&Host(target)){
        DWORD_PTR result=0;
        if(!SendMessageTimeoutW(target,LeanDockQuick::Signal(),LeanDockQuick::Disable,owner,
            SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result)||result!=1)++failures;
    }
    if(targetProcess){CloseHandle(targetProcess);targetProcess=nullptr;}
    target=nullptr;procedure=nullptr;owner=attemptedPid=0;supported=blocked=attaching=false;
    restartPolicy={};lastExitCode=0;lastExitKnown=false;
    if(module){FreeLibrary(module);module=nullptr;}
}
}
