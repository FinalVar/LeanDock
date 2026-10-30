#pragma once
#include <windows.h>
#include <userenv.h>
#include <string>
#include <vector>
#include <cwchar>
#include <cstdio>

// Fixed child path and bounded scalar protocol. This is not an elevation
// broker: no arbitrary executable, command, input or caller-provided path.
namespace LeanDockCompanion {
inline HANDLE child{};
inline PTP_WAIT childWait{};
inline DWORD childPid{},lastError{};
inline bool attempted{};
inline constexpr UINT_PTR Timer=0xCA91;
inline constexpr UINT Exited=WM_APP+496;
inline void CALLBACK ChildExited(PTP_CALLBACK_INSTANCE,void* context,PTP_WAIT,TP_WAIT_RESULT){
    PostMessageW(static_cast<HWND>(context),Exited,childPid,0);
}
inline bool Watch(HWND host){
    childWait=CreateThreadpoolWait(ChildExited,host,nullptr);
    if(!childWait)return false;
    SetThreadpoolWait(childWait,child,nullptr);return true;
}
inline bool NormalToken(HANDLE token) {
    TOKEN_ELEVATION e{};DWORD n=0,session=0,ours=0;
    if(!GetTokenInformation(token,TokenElevation,&e,sizeof(e),&n)||e.TokenIsElevated||
       !GetTokenInformation(token,TokenSessionId,&session,sizeof(session),&n)||
       !ProcessIdToSessionId(GetCurrentProcessId(),&ours)||session!=ours)return false;
    GetTokenInformation(token,TokenIntegrityLevel,nullptr,0,&n);
    if(!n||n>65536)return false;
    std::vector<BYTE> b(n);
    if(!GetTokenInformation(token,TokenIntegrityLevel,b.data(),n,&n))return false;
    PSID sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(b.data())->Label.Sid;
    const DWORD level=*GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1);
    return level>=SECURITY_MANDATORY_MEDIUM_RID&&level<SECURITY_MANDATORY_HIGH_RID;
}
inline bool SameUser(HANDLE a,HANDLE b) {
    DWORD na=0,nb=0;GetTokenInformation(a,TokenUser,nullptr,0,&na);GetTokenInformation(b,TokenUser,nullptr,0,&nb);
    if(!na||!nb||na>65536||nb>65536)return false;
    std::vector<BYTE> va(na),vb(nb);
    return GetTokenInformation(a,TokenUser,va.data(),na,&na)&&GetTokenInformation(b,TokenUser,vb.data(),nb,&nb)&&
        EqualSid(reinterpret_cast<TOKEN_USER*>(va.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(vb.data())->User.Sid);
}
inline bool Launch(const wchar_t* root,bool fixture=false) {
    if(attempted)return child!=nullptr;
    attempted=true;
    HANDLE current=nullptr,limited=nullptr,shell=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&current)){lastError=GetLastError();return false;}
    TOKEN_ELEVATION elevation{};DWORD n=0;
    if(!GetTokenInformation(current,TokenElevation,&elevation,sizeof(elevation),&n)){lastError=GetLastError();CloseHandle(current);return false;}
    if(elevation.TokenIsElevated){
        // TokenLinkedToken may only be SecurityIdentification, so it cannot
        // launch a process. Use the documented Explorer parent attribute.
        // Hold the exact verified process handle throughout creation; do not
        // borrow an arbitrary process token or enable additional privileges.
        DWORD pid=0;GetWindowThreadProcessId(GetShellWindow(),&pid);
        shell=pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_CREATE_PROCESS,FALSE,pid):nullptr;
        wchar_t path[32768]{},expected[32768]{};DWORD length=32768;
        GetWindowsDirectoryW(expected,32768);wcscat_s(expected,L"\\explorer.exe");
        if(!shell||!QueryFullProcessImageNameW(shell,0,path,&length)||_wcsicmp(path,expected)||
           !OpenProcessToken(shell,TOKEN_QUERY,&limited)){
            lastError=ERROR_ACCESS_DENIED;
            if(shell)CloseHandle(shell);CloseHandle(current);return false;
        }
    }else limited=current;
    bool ok=NormalToken(limited)&&SameUser(current,limited);
    if(!ok)lastError=ERROR_BAD_TOKEN_TYPE;
    std::wstring exe=std::wstring(root)+L"\\capsule\\latest.exe";
    std::wstring command=L"\""+exe+L"\" "+(fixture?L"--token-probe":L"--companion-guard "+std::to_wstring(GetCurrentProcessId()));
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(STARTUPINFOW);
    startup.StartupInfo.dwFlags=STARTF_USESHOWWINDOW;startup.StartupInfo.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION pi{};void* environment=nullptr;
    std::vector<BYTE> attributes;
    bool attributesInitialized=false;
    if(ok&&elevation.TokenIsElevated){
        ok=CreateEnvironmentBlock(&environment,limited,FALSE)!=FALSE;
        SIZE_T bytes=0;
        if(ok){
            InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
            if(!bytes||bytes>65536){ok=false;SetLastError(ERROR_INVALID_DATA);}
            else{
                attributes.resize(bytes);
                startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
                attributesInitialized=InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes)!=FALSE;
                ok=attributesInitialized;
            }
        }
        if(ok)ok=UpdateProcThreadAttribute(startup.lpAttributeList,0,
            PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,&shell,sizeof(shell),nullptr,nullptr)!=FALSE;
        if(ok){
            startup.StartupInfo.cb=sizeof(startup);
            ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,
                CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,
                environment,root,&startup.StartupInfo,&pi)!=FALSE;
        }
        if(!ok)lastError=GetLastError();
    }else if(ok){
        ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,
            CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,root,&startup.StartupInfo,&pi)!=FALSE;
        if(!ok)lastError=GetLastError();
    }
    if(attributesInitialized)DeleteProcThreadAttributeList(startup.lpAttributeList);
    if(environment)DestroyEnvironmentBlock(environment);
    if(ok){
        HANDLE token=nullptr;
        ok=OpenProcessToken(pi.hProcess,TOKEN_QUERY,&token)&&NormalToken(token)&&SameUser(current,token);
        if(token)CloseHandle(token);
        if(!ok){lastError=ERROR_BAD_TOKEN_TYPE;TerminateProcess(pi.hProcess,lastError);WaitForSingleObject(pi.hProcess,2000);}
        else if(ResumeThread(pi.hThread)==DWORD(-1)){
            lastError=GetLastError();ok=false;TerminateProcess(pi.hProcess,lastError);WaitForSingleObject(pi.hProcess,2000);
        }
        CloseHandle(pi.hThread);
        if(ok){child=pi.hProcess;childPid=pi.dwProcessId;}else CloseHandle(pi.hProcess);
    }
    if(limited!=current)CloseHandle(limited);
    if(shell)CloseHandle(shell);CloseHandle(current);
    return ok;
}
inline void Stop(HWND host) {
    if(host)KillTimer(host,Timer);
    if(childWait){
        SetThreadpoolWait(childWait,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(childWait,TRUE);
        CloseThreadpoolWait(childWait);childWait=nullptr;
    }
    if(child){
        HWND w=FindWindowW(L"NativeTaskbarDock.Capsule.Guard.v1",nullptr);DWORD pid=0;
        GetWindowThreadProcessId(w,&pid);
        if(pid==childPid)PostMessageW(w,WM_CLOSE,0,0);
        // The guard closes its UI and restores the AppBar before exiting.
        // Do not terminate it: its independent owner-death branch is the fallback.
        WaitForSingleObject(child,7000);CloseHandle(child);child=nullptr;
    }
    if(host)RemovePropW(host,L"LeanDock.Companion.v1");
}
inline int Probe(const wchar_t* root) {
    if(!Launch(root,true))return static_cast<int>(lastError?lastError:1);
    DWORD code=1;
    if(WaitForSingleObject(child,10000)==WAIT_OBJECT_0)GetExitCodeProcess(child,&code);
    else {TerminateProcess(child,ERROR_TIMEOUT);WaitForSingleObject(child,2000);code=ERROR_TIMEOUT;}
    CloseHandle(child);child=nullptr;return static_cast<int>(code);
}
}
