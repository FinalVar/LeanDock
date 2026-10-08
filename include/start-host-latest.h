#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <cwchar>
#include <iterator>
#include "verify-latest.h"

// Small Start-only companion, loaded by a thread-specific Windows hook.
// No additional process, service, symbol detours or periodic polling.
namespace LeanDockStart {
inline HMODULE module;
inline HWND target;
inline HANDLE targetProcess;
inline DWORD owner;
inline DWORD attemptedPid;
inline HOOKPROC procedure;
inline bool enabled,supported,blocked;
inline unsigned failures;
inline UINT Signal(){static UINT id=RegisterWindowMessageW(L"LeanDock.StartLayout.v1");return id;}
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
  {L"StartDocked.dll",L"1EEF242E7E0AABC8ED4F2F3A3365CB7B6E13E58636BBF67D9841CBD852178A6F"}
 };
 wchar_t base[32768];GetWindowsDirectoryW(base,std::size(base));
 for(auto const& pin:pins) {
  wchar_t path[32768],actual[65];_snwprintf_s(path,_TRUNCATE,L"%ls\\SystemApps\\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\\%ls",base,pin.name);
  if(!NativeHashFile(path,actual) || _wcsicmp(actual,pin.sha))return false;
 }
 return true;
}
inline bool Query(HWND w) {
 DWORD_PTR result=0;
 return Host(w) && SendMessageTimeoutW(w,Signal(),3,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result) && result==1;
}
inline bool Attach(HWND w) {
 if(!supported || blocked || !procedure || !Host(w))return false;
 // Never create a crash/restart loop. Once a modified Start host exits,
 // leave its replacement native until the controller is explicitly restarted.
 if(targetProcess && WaitForSingleObject(targetProcess,0)!=WAIT_TIMEOUT) {
  blocked=true;enabled=false;target=nullptr;++failures;return false;
 }
 if(w==target && Query(w)){enabled=true;return true;}
 // Start may be suspended while closed; wait for the existing SHOW/UNCLOAKED
 // event instead of spending the one allowed attempt on a sleeping UI thread.
 DWORD cloaked=0;
 if(!IsWindowVisible(w) || (SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked))) && cloaked))return false;
 DWORD pid=0;const auto tid=GetWindowThreadProcessId(w,&pid);
 if(attemptedPid==pid)return false;
 attemptedPid=pid;
 if(!targetProcess)targetProcess=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
 if(!targetProcess){++failures;return false;}
 HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROC,procedure,module,tid);if(!hook){++failures;return false;}
 DWORD_PTR result=0;
 bool sent=SendMessageTimeoutW(w,Signal(),1,owner,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result)!=0;
 UnhookWindowsHookEx(hook);
 enabled=sent && Query(w);
 if(enabled)target=w;else ++failures;
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
 if(target && Host(target)) {
  DWORD_PTR result=0;
  if(!SendMessageTimeoutW(target,Signal(),2,owner,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result) || result!=1)++failures;
 }
 enabled=false;target=nullptr;procedure=nullptr;supported=false;
 if(targetProcess){CloseHandle(targetProcess);targetProcess=nullptr;}
 attemptedPid=0;blocked=false;
 if(module){FreeLibrary(module);module=nullptr;}
}
}
