#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <initializer_list>
#include "quick-lifecycle-latest.h"
static int checks,failures;
static void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
int wmain(int argc,wchar_t** argv){
    if(argc==2)return !wcscmp(argv[1],L"--child-success")?0:17;
    using LeanDockQuick::RestartPolicy;
    RestartPolicy p;
    Check(p.Retired(true,0,1000),"first clean retirement recovers");
    Check(p.Retired(true,0,3000),"second clean retirement recovers");
    Check(!p.Retired(true,0,5000)&&p.blocked,"third rapid retirement stops loop");
    Check(!p.Retired(true,0,100000)&&p.recoveries==2,"blocked policy stays closed");
    p={};Check(p.Retired(true,0,1000),"fresh lifetime starts clean");
    Check(p.Retired(true,0,61000)&&p.recent==1,"budget resets after full window");
    Check(p.Retired(true,0,61001)&&p.recent==2,"new window second attempt allowed");
    Check(!p.Retired(true,0,61002),"new window also bounded");
    for(auto code:{1u,17u,259u,0xc0000005u,0xc0000409u}){
        p={};Check(!p.Retired(true,code,1000)&&p.blocked,"nonzero exit remains native");
    }
    p={};Check(!p.Retired(false,0,1000),"unknown exit remains native");
    // Real process handles and exit codes, without touching any desktop host.
    wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);
    for(auto arg:{L"--child-success",L"--child-failure"}){
        wchar_t command[32768];swprintf_s(command,L"\"%ls\" %ls",exe,arg);
        STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
        bool created=CreateProcessW(exe,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=FALSE;
        Check(created,"test-owned hidden child starts");if(!created)continue;
        CloseHandle(pi.hThread);auto wait=WaitForSingleObject(pi.hProcess,5000);
        Check(wait==WAIT_OBJECT_0,"child terminates within bound");
        DWORD code=259;bool known=GetExitCodeProcess(pi.hProcess,&code)!=FALSE;
        Check(known,"exit code read from original handle");
        p={};Check(p.Retired(known,code,1000)==!wcscmp(arg,L"--child-success"),"native exit drives recovery decision");
        CloseHandle(pi.hProcess);
    }
    std::printf("Quick lifecycle: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
