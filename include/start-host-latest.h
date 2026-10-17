#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <cwchar>
#include <iterator>
#include "verify-latest.h"
#include "start-protocol-latest.h"
#include "start-attach-policy-latest.h"
#include "quick-lifecycle-latest.h"

// Small Start-only companion, loaded by a thread-specific Windows hook.
// No additional process, service, symbol detours or periodic polling.
namespace LeanDockStart {
inline HMODULE module;
inline HWND target;
inline HANDLE targetProcess;
inline DWORD owner;
inline DWORD attemptedPid;
inline HOOKPROC procedure;
inline bool enabled,supported,blocked,attaching;
inline unsigned failures;
inline AttachPolicy attachPolicy;
inline UINT_PTR retryTimer;
inline HWND retryTarget;
inline ULONGLONG retryDue;
inline DWORD lastAttachError;
inline bool attachReceipt;
inline void CancelRetry(){if(retryTimer)KillTimer(nullptr,retryTimer);retryTimer=0;retryTarget=nullptr;retryDue=0;}
inline bool Attach(HWND w,bool nativeEvent=true);
inline void CALLBACK Retry(HWND,UINT,UINT_PTR timer,DWORD) {
 // KillTimer does not remove an already queued WM_TIMER. A reused ID must
 // not let an old callback cancel the new host's not-yet-due retry.
 if(timer!=retryTimer || GetTickCount64()<retryDue)return;
 const auto w=retryTarget;CancelRetry();Attach(w,false);
}
inline void QueueRetry(HWND w) {
 if(retryTimer || !attachPolicy.Retryable())return;
 retryTarget=w;retryDue=GetTickCount64()+AttachPolicy::DelayMs;
 retryTimer=SetTimer(nullptr,0,static_cast<UINT>(AttachPolicy::DelayMs),Retry);
 if(!retryTimer){lastAttachError=GetLastError();attachPolicy.Stop();retryTarget=nullptr;}
}
// Reuse the already bounded native Quick Settings host retirement policy.
// Normal shell retirement is distinct from a crash or unknown exit status.
inline LeanDockQuick::RestartPolicy restartPolicy;
inline DWORD lastExitCode;
inline bool lastExitKnown;
inline UINT Signal(){return LeanDockStartProtocol::Signal();}
inline bool Host(HWND w) {
 DWORD pid=0,session=0,ours=0;GetWindowThreadProcessId(w,&pid);
 if(!pid || !ProcessIdToSessionId(pid,&session) || !ProcessIdToSessionId(GetCurrentProcessId(),&ours) || session!=ours)return false;
 HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
 wchar_t path[32768],expected[32768];DWORD size=std::size(path);
 GetWindowsDirectoryW(expected,std::size(expected));wcscat_s(expected,L"\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\StartMenuExperienceHost.exe");
 bool valid=QueryFullProcessImageNameW(p,0,path,&size) && !_wcsicmp(path,expected);CloseHandle(p);return valid;
}
inline bool Verify() {
 const struct {const wchar_t* name;const wchar_t* sha;} pins[]={
  {L"StartMenuExperienceHost.exe",L"4D151C6B5662F5F94ED115CA7F7446356E9208549B0F06E446F579BC20F5E577"},
  {L"StartDocked.dll",L"8359E3BF5F3F58944F300B9A2D3E1B1FACD4CE8FC7813C799402DA7816874EB9"}
 };
 wchar_t base[32768];GetWindowsDirectoryW(base,std::size(base));
 for(auto const& pin:pins) {
  wchar_t path[32768],actual[65];_snwprintf_s(path,_TRUNCATE,L"%ls\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\%ls",base,pin.name);
  if(!NativeHashFile(path,actual) || _wcsicmp(actual,pin.sha))return false;
 }
 return true;
}
inline bool Query(HWND w) {
 // Status must not wait for a suspended Start UI thread and stall hot corners.
 return w && Host(w) && targetProcess && WaitForSingleObject(targetProcess,0)==WAIT_TIMEOUT &&
  reinterpret_cast<ULONG_PTR>(GetPropW(w,LeanDockStartProtocol::Owner))==owner &&
  GetPropW(w,LeanDockStartProtocol::Active);
}
inline bool Attach(HWND w,bool nativeEvent) {
 if(!supported || blocked || attaching || !procedure || !Host(w))return false;
 struct Guard{Guard(){attaching=true;}~Guard(){attaching=false;}} guard;
 // Read the original process handle before releasing it; a replacement PID
 // alone cannot establish whether the previous host exited cleanly.
 if(targetProcess && WaitForSingleObject(targetProcess,0)!=WAIT_TIMEOUT) {
  lastExitKnown=GetExitCodeProcess(targetProcess,&lastExitCode)!=FALSE;
  const bool recover=restartPolicy.Retired(lastExitKnown,lastExitCode,GetTickCount64());
  CloseHandle(targetProcess);targetProcess=nullptr;
  CancelRetry();enabled=false;target=nullptr;attemptedPid=0;attachPolicy={};lastAttachError=0;attachReceipt=false;
  if(!recover){blocked=true;++failures;return false;}
 }
 // Adopt a completed late initialization or an explicit recovery before any
 // attempt gate. Never double-initialize an already active native window.
 if(Query(w)){CancelRetry();target=w;enabled=true;attachPolicy.Success();return true;}
 DWORD pid=0;const auto tid=GetWindowThreadProcessId(w,&pid);
 // Do not reuse the tracked handle for another host that is still alive.
 if(targetProcess && GetProcessId(targetProcess)!=pid)return false;
 if(attemptedPid!=pid){CancelRetry();attachPolicy={};attemptedPid=pid;}
 if(!attachPolicy.Begin(GetTickCount64(),nativeEvent))return false;
 // Three bounded readiness probes; a suspended host is never forced awake.
 // Later native events can renew an exhausted budget after a minute.
 DWORD_PTR responsive=0;
 if(!SendMessageTimeoutW(w,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&responsive)){lastAttachError=GetLastError();QueueRetry(w);return false;}
 if(!targetProcess)targetProcess=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
 if(!targetProcess){lastAttachError=GetLastError();CancelRetry();attachPolicy.Stop();++failures;return false;}
 target=w;attachReceipt=false;
 RemovePropW(w,LeanDockStartProtocol::AttemptOwner);
 RemovePropW(w,LeanDockStartProtocol::InitResult);
 HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROC,procedure,module,tid);if(!hook){lastAttachError=GetLastError();CancelRetry();attachPolicy.Stop();++failures;return false;}
 DWORD_PTR result=0;
 bool sent=SendMessageTimeoutW(w,Signal(),1,owner,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result)!=0;
 const auto sendError=sent?0:GetLastError();
 UnhookWindowsHookEx(hook);
 enabled=sent && Query(w);
 attachReceipt=reinterpret_cast<ULONG_PTR>(GetPropW(w,LeanDockStartProtocol::AttemptOwner))==owner;
 lastAttachError=sent?static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(GetPropW(w,LeanDockStartProtocol::InitResult))):sendError;
 if(sent && !attachReceipt && !enabled)lastAttachError=ERROR_DLL_INIT_FAILED;
 if(enabled){CancelRetry();target=w;attemptedPid=0;attachPolicy.Success();lastAttachError=0;}
 else {
  ++failures;
  // Never retry an unacknowledged load, a timeout, or an ownership refusal.
  // Only the companion's completed rollback makes reinitialization safe.
  if(sent && attachReceipt && FAILED(static_cast<HRESULT>(lastAttachError)) && lastAttachError!=static_cast<DWORD>(E_ACCESSDENIED))QueueRetry(w);
  else {CancelRetry();attachPolicy.Stop();}
 }
 return enabled;
}
inline bool Start(const wchar_t* directory,DWORD ownerPid=GetCurrentProcessId()) {
 owner=ownerPid;supported=Verify();if(!supported)return false;
 wchar_t path[32768];_snwprintf_s(path,_TRUNCATE,L"%ls\\start\\latest.dll",directory);
 module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!module){supported=false;return false;}
 procedure=reinterpret_cast<HOOKPROC>(GetProcAddress(module,"LeanDockStartHook"));
 if(!procedure){FreeLibrary(module);module=nullptr;supported=false;return false;}
 HWND w=nullptr;while((w=FindWindowExW(nullptr,w,L"Windows.UI.Core.CoreWindow",nullptr)))if(Host(w)){Attach(w);break;}
 return true;
}
inline void Stop() {
 CancelRetry();
 if(target && Host(target)) {
  DWORD_PTR result=0;
  if(!SendMessageTimeoutW(target,Signal(),2,owner,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result) || result!=1)++failures;
 }
 enabled=false;target=nullptr;procedure=nullptr;supported=false;
 if(targetProcess){CloseHandle(targetProcess);targetProcess=nullptr;}
 attemptedPid=0;blocked=false;
 attachPolicy={};lastAttachError=0;attachReceipt=false;
 restartPolicy={};lastExitCode=0;lastExitKnown=false;
 if(module){FreeLibrary(module);module=nullptr;}
}
}
