// NativeTaskbarDock: ordinary-user event-driven controller, no visible windows.
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include "protocol-latest.h"
#include "verify-latest.h"
#include "config-latest.h"
#include "hotcorners-latest.h"
#include "shell-panels-latest.h"

static HINSTANCE g_instance;
static HWND g_window;
static UINT g_taskbarCreated;
static DWORD g_explorerPid;
static HANDLE g_explorer,g_helper;
static PTP_WAIT g_explorerWait;
static wchar_t g_executable[32768],g_directory[32768],g_error[512];
static unsigned g_state=NativeIdle,g_attempts,g_rapidAttempts;
static ULONGLONG g_lastAttach;
static int g_exitCode;
static bool g_stopping,g_dockSupported=true;
static bool g_elevated;
static bool g_panelsReady;

static void Path(wchar_t* output,size_t capacity,const wchar_t* name) {
    _snwprintf_s(output,capacity,_TRUNCATE,L"%s\\%s",g_directory,name);
}
static bool OwnProcessPath(DWORD pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return false;
    wchar_t path[32768];DWORD length=std::size(path);
    const bool match=QueryFullProcessImageNameW(process,0,path,&length) && !_wcsicmp(path,g_executable);
    CloseHandle(process);return match;
}
static void SaveStatus() {
    NativeCorners::Snapshot();
    wchar_t path[32768];Path(path,std::size(path),L"latest-status.json");
    char json[1600],error[1200];WideCharToMultiByte(CP_UTF8,0,g_error,-1,error,sizeof(error),nullptr,nullptr);
    // Error messages are fixed internal labels, never window titles or application text.
    int size=snprintf(json,sizeof(json),
        "{\"version\":\"0.3.1\",\"controllerPid\":%lu,\"explorerPid\":%lu,\"state\":%u,\"attempts\":%u,\"watchMode\":\"events\",\"hotCornersReady\":%s,\"dockSupported\":%s,\"elevated\":%s,\"panelAnchoring\":%s,\"panelMoves\":[%u,%u],\"panelFailures\":%u,\"startLayoutSupported\":%s,\"startLayoutActive\":%s,\"startLayoutBlocked\":%s,\"startLayoutFailures\":%u,\"error\":\"%s\"}\n",
        GetCurrentProcessId(),g_explorerPid,g_state,g_attempts,NativeCorners::Ready()?"true":"false",g_dockSupported?"true":"false",g_elevated?"true":"false",g_panelsReady?"true":"false",LeanDockPanels::moves[0],LeanDockPanels::moves[1],LeanDockPanels::failures,LeanDockStart::supported?"true":"false",(LeanDockStart::target && LeanDockStart::Query(LeanDockStart::target))?"true":"false",LeanDockStart::blocked?"true":"false",LeanDockStart::failures,error);
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(file,json,size,&written,nullptr);CloseHandle(file);}
}
static bool GetShell(DWORD* pid) {
    HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);if(!taskbar)return false;
    GetWindowThreadProcessId(taskbar,pid);
    DWORD session=0,ownSession=0;
    if(!ProcessIdToSessionId(*pid,&session) || !ProcessIdToSessionId(GetCurrentProcessId(),&ownSession) || session!=ownSession)return false;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,*pid);if(!process)return false;
    wchar_t path[32768],expected[32768];DWORD length=std::size(path);
    GetWindowsDirectoryW(expected,std::size(expected));wcscat_s(expected,L"\\explorer.exe");
    const bool ok=QueryFullProcessImageNameW(process,0,path,&length) && !_wcsicmp(path,expected);
    CloseHandle(process);return ok;
}
static HWND Backend(DWORD pid) {
    HWND current=nullptr;
    while((current=FindWindowExW(nullptr,current,NativeBackendClass,nullptr))) {
        DWORD owner=0;GetWindowThreadProcessId(current,&owner);if(owner==pid)return current;
    }
    return nullptr;
}
static bool QueryBackend(HWND window,unsigned* state) {
    DWORD_PTR result=0;
    if(!SendMessageTimeoutW(window,NativeQuery,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result))return false;
    if(result>NativeFailed)return false;
    *state=static_cast<unsigned>(result);return true;
}
static void ClearExplorer() {
    if(g_explorerWait) {
        SetThreadpoolWait(g_explorerWait,nullptr,nullptr);
        WaitForThreadpoolWaitCallbacks(g_explorerWait,TRUE);
        CloseThreadpoolWait(g_explorerWait);g_explorerWait=nullptr;
    }
    if(g_explorer){CloseHandle(g_explorer);g_explorer=nullptr;}
}
static void CALLBACK ExplorerExited(PTP_CALLBACK_INSTANCE,void*,PTP_WAIT,TP_WAIT_RESULT) {
    PostMessageW(g_window,NativeReconnect,0,0);
}
static bool TrackExplorer(DWORD pid) {
    if(g_explorerPid==pid && g_explorer && WaitForSingleObject(g_explorer,0)==WAIT_TIMEOUT)return true;
    ClearExplorer();g_explorerPid=pid;
    g_explorer=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!g_explorer)return false;
    g_explorerWait=CreateThreadpoolWait(ExplorerExited,nullptr,nullptr);if(!g_explorerWait)return false;
    SetThreadpoolWait(g_explorerWait,g_explorer,nullptr);return true;
}
static uintptr_t RemoteModule(DWORD pid,const wchar_t* name,bool rejectWindhawk=false) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(snapshot==INVALID_HANDLE_VALUE)return 0;
    MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);uintptr_t result=0;
    if(Module32FirstW(snapshot,&entry))do {
        if(rejectWindhawk && !_wcsicmp(entry.szModule,L"windhawk.dll")){result=1;break;}
        if(!rejectWindhawk && !_wcsicmp(wcschr(name,L'\\')?entry.szExePath:entry.szModule,name)){result=reinterpret_cast<uintptr_t>(entry.modBaseAddr);break;}
    }while(Module32NextW(snapshot,&entry));
    CloseHandle(snapshot);return result;
}
static int Attach(DWORD pid,DWORD owner) {
    DWORD actual=0;if(!GetShell(&actual) || actual!=pid || !OwnProcessPath(owner))return 10;
    if(RemoteModule(pid,L"",true))return 11;
    wchar_t mismatch[256];if(!NativeVerifySystem(mismatch,std::size(mismatch)))return 12;
    wchar_t dll[32768];Path(dll,std::size(dll),L"latest.dll");
    HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|
        PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!process)return 13;
    const SIZE_T bytes=(wcslen(dll)+1)*sizeof(wchar_t);
    void* argument=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!argument){CloseHandle(process);return 14;}
    SIZE_T written=0;
    if(!WriteProcessMemory(process,argument,dll,bytes,&written) || written!=bytes) {
        VirtualFreeEx(process,argument,0,MEM_RELEASE);CloseHandle(process);return 15;
    }
    const auto localLoad=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
    HMODULE host=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(localLoad),&host)) {
        VirtualFreeEx(process,argument,0,MEM_RELEASE);CloseHandle(process);return 16;
    }
    wchar_t hostPath[32768];GetModuleFileNameW(host,hostPath,std::size(hostPath));
    const auto hostName=wcsrchr(hostPath,L'\\');const uintptr_t remoteHost=RemoteModule(pid,hostName?hostName+1:hostPath);
    if(!remoteHost){VirtualFreeEx(process,argument,0,MEM_RELEASE);CloseHandle(process);return 17;}
    const auto loader=reinterpret_cast<LPTHREAD_START_ROUTINE>(
        remoteHost+(reinterpret_cast<uintptr_t>(localLoad)-reinterpret_cast<uintptr_t>(host)));
    HANDLE thread=CreateRemoteThread(process,nullptr,0,loader,argument,0,nullptr);
    if(!thread){VirtualFreeEx(process,argument,0,MEM_RELEASE);CloseHandle(process);return 18;}
    const DWORD wait=WaitForSingleObject(thread,20000);CloseHandle(thread);
    if(wait!=WAIT_OBJECT_0){CloseHandle(process);return 19;} // Keep argument valid for a late loader.
    VirtualFreeEx(process,argument,0,MEM_RELEASE);
    const auto remoteDll=RemoteModule(pid,dll);
    if(!remoteDll){CloseHandle(process);return 20;}
    // Inspect the export without executing the local DLL or its static initializers.
    HMODULE local=LoadLibraryExW(dll,nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!local){CloseHandle(process);return 21;}
    const auto start=GetProcAddress(local,"NativeDock_Start");
    if(!start){FreeLibrary(local);CloseHandle(process);return 22;}
    const uintptr_t rva=reinterpret_cast<uintptr_t>(start)-reinterpret_cast<uintptr_t>(local);
    FreeLibrary(local);
    thread=CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteDll+rva),
                               reinterpret_cast<void*>(static_cast<uintptr_t>(owner)),0,nullptr);
    if(!thread){CloseHandle(process);return 23;}
    DWORD result=24;
    if(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0)GetExitCodeThread(thread,&result);
    CloseHandle(thread);CloseHandle(process);return static_cast<int>(result);
}
static void CancelHelper() {
    if(!g_helper)return;
    if(WaitForSingleObject(g_helper,0)==WAIT_TIMEOUT)TerminateProcess(g_helper,30);
    CloseHandle(g_helper);g_helper=nullptr;
}
static void StopDock() {
    LeanDockStart::Stop();
    LeanDockPanels::Stop();
    g_stopping=true;KillTimer(g_window,1);CancelHelper();
    if(HWND backend=Backend(g_explorerPid)) {
        DWORD_PTR result=0;
        if(!SendMessageTimeoutW(backend,NativeStop,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,10000,&result) || result!=NativeStopped) {
            wcscpy_s(g_error,L"Dock restoration did not confirm");g_exitCode=5;
        }
    }
    g_state=NativeStopped;SaveStatus();ClearExplorer();
}
static bool TryConnect() {
    DWORD pid=0;if(!GetShell(&pid))return false;
    if(!TrackExplorer(pid))return false;
    if(HWND backend=Backend(pid)) {
        unsigned state=NativeIdle;
        if(QueryBackend(backend,&state)) {
            if(state==NativeActive){
                DWORD_PTR acquired=NativeFailed;
                if(!SendMessageTimeoutW(backend,NativeStart,GetCurrentProcessId(),0,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&acquired) || acquired!=NativeActive)return false;
                g_state=state;g_error[0]=0;SaveStatus();CancelHelper();return true;
            }
            if(state==NativeFailed){wcscpy_s(g_error,L"Native backend failed; automatic retry stopped");g_exitCode=4;PostMessageW(g_window,WM_CLOSE,0,0);return false;}
            if(state==NativeStopped)PostMessageW(backend,NativeStart,GetCurrentProcessId(),0);
            return false;
        }
    }
    if(g_helper) {
        if(WaitForSingleObject(g_helper,0)==WAIT_TIMEOUT)return false;
        DWORD result=1;GetExitCodeProcess(g_helper,&result);CloseHandle(g_helper);g_helper=nullptr;
        if(result) {
            _snwprintf_s(g_error,_TRUNCATE,L"Attachment failed (%lu)",result);
            if(result==11 || result==12){g_exitCode=result;PostMessageW(g_window,WM_CLOSE,0,0);}
        }
    }
    if(g_lastAttach && GetTickCount64()-g_lastAttach<5000)return false;
    if(GetTickCount64()-g_lastAttach>60000)g_rapidAttempts=0;
    if(++g_rapidAttempts>3){wcscpy_s(g_error,L"Repeated attachment stopped");g_exitCode=4;PostMessageW(g_window,WM_CLOSE,0,0);return false;}
    g_lastAttach=GetTickCount64();
    wchar_t command[32768];_snwprintf_s(command,_TRUNCATE,L"\"%s\" --attach %lu %lu",g_executable,pid,GetCurrentProcessId());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(g_executable,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,g_directory,&startup,&child))return false;
    g_helper=child.hProcess;CloseHandle(child.hThread);return false;
}
static LRESULT CALLBACK ControllerProc(HWND w,UINT message,WPARAM p,LPARAM l) {
    if(LeanDockPanels::Dispatch(message,p))return 0;
    LRESULT cornerResult=0;
    if(NativeCorners::Dispatch(w,message,p,l,cornerResult))return cornerResult;
    if(message==WM_CREATE){g_window=w;PostMessageW(w,NativeReconnect,0,0);return 0;}
    if(message==WM_CLOSE || (message==WM_ENDSESSION && p)){NativeCorners::Shutdown();StopDock();DestroyWindow(w);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(g_exitCode);return 0;}
    if(message==NativeStop){PostMessageW(w,WM_CLOSE,0,0);return 1;}
    if(message==NativeQuery){SaveStatus();return g_state;}
    if(message==NativeReconnect || message==g_taskbarCreated ||
       message==WM_DISPLAYCHANGE || (message==WM_POWERBROADCAST && p==PBT_APMRESUMEAUTOMATIC)) {
        if(!g_stopping && g_dockSupported){g_attempts=0;g_state=NativeStarting;SetTimer(w,1,750,nullptr);}
        return TRUE;
    }
    if(message==WM_TIMER && p==1) {
        if(g_stopping)return 0;
        if(TryConnect()){KillTimer(w,1);return 0;}
        if(++g_attempts>=60){wcscpy_s(g_error,L"Taskbar was not ready within startup limit");g_exitCode=3;PostMessageW(w,WM_CLOSE,0,0);}
        return 0;
    }
    return DefWindowProcW(w,message,p,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    g_instance=instance;GetModuleFileNameW(nullptr,g_executable,std::size(g_executable));
    HANDLE token=nullptr;TOKEN_ELEVATION elevation{};DWORD needed=0;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){
        if(GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&needed))g_elevated=elevation.TokenIsElevated!=0;
        CloseHandle(token);
    }
    wcscpy_s(g_directory,g_executable);auto slash=wcsrchr(g_directory,L'\\');if(!slash)return 1;*slash=0;
    int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!args)return 1;
    if(count==4 && !wcscmp(args[1],L"--attach")) {
        DWORD pid=wcstoul(args[2],nullptr,10),owner=wcstoul(args[3],nullptr,10);
        LocalFree(args);return Attach(pid,owner);
    }
    if(count==2 && !wcscmp(args[1],L"--verify-corners")){
        LocalFree(args);return NativeCorners::Validate(g_directory)?0:25;
    }
    // Read-only validation of a staged build must work while the production
    // controller remains active. Mutation commands retain the exact-path gate.
    if(count==2 && !wcscmp(args[1],L"--verify")) {
        bool logging,diagnostics;wchar_t config[32768];Path(config,std::size(config),L"latest.ini");
        const bool valid=NativeVerifySystem(g_error,std::size(g_error)) && NativeReadConfiguration(config,logging,diagnostics);g_state=valid?NativeIdle:NativeFailed;
        LocalFree(args);return valid?0:12;
    }
    HWND previous=FindWindowW(NativeControllerClass,nullptr);
    DWORD previousPid=0;if(previous)GetWindowThreadProcessId(previous,&previousPid);
    if(previous && !OwnProcessPath(previousPid)){LocalFree(args);return 2;}
    if(count==2 && !wcsncmp(args[1],L"--corners-",10)){
        UINT command=!wcscmp(args[1],L"--corners-status")?NativeCorners::Status:
            !wcscmp(args[1],L"--corners-pause")?NativeCorners::Pause:
            !wcscmp(args[1],L"--corners-resume")?NativeCorners::Resume:
            !wcscmp(args[1],L"--corners-reload")?NativeCorners::Reload:0;
        LocalFree(args);if(!command || !previous)return 3;
        DWORD_PTR result=0;
        if(!SendMessageTimeoutW(previous,NativeCorners::ControlMessage,command,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,3000,&result))return 4;
        return result==1?0:result==2?25:5;
    }
    if(count==2 && !wcscmp(args[1],L"--status")) {
        GetShell(&g_explorerPid);unsigned state=NativeIdle;
        if(HWND backend=Backend(g_explorerPid))QueryBackend(backend,&state);
        DWORD_PTR result;
        if(previous){if(!SendMessageTimeoutW(previous,NativeQuery,0,0,SMTO_ABORTIFHUNG,3000,&result)){LocalFree(args);return 4;}}
        else {g_state=NativeStopped;SaveStatus();}
        LocalFree(args);return previous && state==NativeActive?0:1;
    }
    if(count==2 && !wcscmp(args[1],L"--stop")) {
        LocalFree(args);if(!previous)return 0;
        HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,previousPid);
        if(!process)return 2;
        if(!PostMessageW(previous,NativeStop,0,0)){CloseHandle(process);return 4;}
        DWORD result=2;
        if(WaitForSingleObject(process,15000)==WAIT_OBJECT_0)GetExitCodeProcess(process,&result);
        CloseHandle(process);return static_cast<int>(result);
    }
    if(count!=1){LocalFree(args);return 2;}LocalFree(args);
    if(previous)return 0;
    bool logging,diagnostics;wchar_t config[32768];Path(config,std::size(config),L"latest.ini");
    if(!NativeReadConfiguration(config,logging,diagnostics)) {
        wcscpy_s(g_error,L"Invalid configuration");g_state=NativeFailed;SaveStatus();return 25;
    }
    HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\NativeTaskbarDock.Controller.v1");
    if(!mutex || GetLastError()==ERROR_ALREADY_EXISTS){if(mutex)CloseHandle(mutex);return 0;}
    g_dockSupported=NativeVerifySystem(g_error,std::size(g_error));
    if(!g_dockSupported)g_state=NativeFailed;
    // Physical corner geometry must use the same per-monitor physical pixels
    // as the existing NativeHotCorners implementation.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW type{};type.hInstance=instance;type.lpszClassName=NativeControllerClass;type.lpfnWndProc=ControllerProc;
    RegisterClassW(&type);
    if(!CreateWindowExW(WS_EX_TOOLWINDOW,NativeControllerClass,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr)) {
        ReleaseMutex(mutex);CloseHandle(mutex);return 1;
    }
    // Keep normal-user maintenance usable when the task runs elevated. These
    // four messages accept no pointers, paths, arbitrary code or action input:
    // query, stop, bounded corner controls, and the shell-recreated signal.
    // No global UIPI filter changes, WM_COPYDATA, or generic input whitelist.
    for(UINT message:{NativeQuery,NativeStop,NativeCorners::ControlMessage,g_taskbarCreated}){
        if(!ChangeWindowMessageFilterEx(g_window,message,MSGFLT_ALLOW,nullptr)){
            wcscpy_s(g_error,L"Cannot configure maintenance message access");SaveStatus();
            DestroyWindow(g_window);ReleaseMutex(mutex);CloseHandle(mutex);return 26;
        }
    }
    if(!NativeCorners::Initialize(g_window,g_directory)){
        if(!g_dockSupported){SaveStatus();DestroyWindow(g_window);ReleaseMutex(mutex);CloseHandle(mutex);return 25;}
    }
    // Start layout support is independently pinned and never blocks Dock or
    // corners when a Windows update changes the Start components.
    LeanDockStart::Start(g_directory);
    g_panelsReady=g_dockSupported && LeanDockPanels::Start(g_window);
    if(!g_panelsReady)LeanDockPanels::Stop();
    SaveStatus();
    MSG message;
    while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    NativeCorners::Shutdown();CancelHelper();ClearExplorer();ReleaseMutex(mutex);CloseHandle(mutex);return g_exitCode;
}
