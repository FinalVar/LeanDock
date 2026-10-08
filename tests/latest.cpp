#define NATIVE_RUNTIME_TEST
#include "../src/runtime-latest.cpp"
#include <cstdlib>
#include <thread>

static unsigned passed=0,failed=0;
static void Check(bool condition,const char* name) {
    if(condition)++passed;
    else {++failed;printf("FAIL: %s\n",name);}
}
static int IntSetting(unsigned component,const wchar_t* name,...) {
    va_list args;va_start(args,name);
    const int value=InternalWh_GetIntSetting(reinterpret_cast<void*>(component),name,args);
    va_end(args);return value;
}
static std::wstring StringSetting(unsigned component,const wchar_t* name,...) {
    va_list args;va_start(args,name);
    auto value=InternalWh_GetStringSetting(reinterpret_cast<void*>(component),name,args);
    va_end(args);std::wstring result=value;
    InternalWh_FreeStringSetting(nullptr,value);return result;
}
using Function=int(*)(int);
static Function firstOriginal,secondOriginal,pendingOriginal;
__declspec(noinline) static int Target(int value) { volatile int copy=value;return copy*3+1; }
__declspec(noinline) static int FirstHook(int value) {return firstOriginal(value)+10;}
__declspec(noinline) static int SecondHook(int value) {return secondOriginal(value)+100;}
__declspec(noinline) static int PendingTarget(int value) {volatile int copy=value;return copy*5+2;}
__declspec(noinline) static int PendingHook(int value) {return pendingOriginal(value)+1000;}
__declspec(noinline) static int LateTarget(int value) {volatile int copy=value;return copy*7+3;}
__declspec(noinline) static int LateHook(int value) {return value+5000;}

int main() {
    GetModuleFileNameW(nullptr,g_directory,std::size(g_directory));
    auto slash=wcsrchr(g_directory,L'\\');if(slash)*slash=0;
    _snwprintf_s(g_config,_TRUNCATE,L"%s\\test-config.ini",g_directory);
    DeleteFileW(g_config); // Exact test fixture, never the deployed configuration.
    Check(IntSetting(3,L"IconSize")==40,"native icon size");
    Check(IntSetting(3,L"TaskbarHeight")==64,"native taskbar height");
    Check(IntSetting(2,L"notificationIconRows")==2,"two tray rows");
    Check(StringSetting(4,L"mode")==L"intersected","intelligent hide profile");
    Check(StringSetting(5,L"mode")==L"showAll","all tray icons profile");
    Check(StringSetting(4,L"excludedPrograms[%d]",0).empty(),"empty exclusions terminate");
    Check(StringSetting(1,L"theme").empty(),"no theme catalog");
    Check(InternalWh_GetUrlContent(nullptr,L"https://invalid.test",nullptr)==nullptr,"offline runtime");
    bool logging=false,diagnostics=false;
    Check(!NativeReadConfiguration(g_config,logging,diagnostics),"reject missing configuration");
    WritePrivateProfileStringW(L"runtime",L"Logging",L"0",g_config);
    WritePrivateProfileStringW(L"runtime",L"Diagnostics",L"1",g_config);
    Check(NativeReadConfiguration(g_config,logging,diagnostics) && !logging && diagnostics,"read bounded diagnostic switches");
    WritePrivateProfileStringW(L"runtime",L"Diagnostics",L"bad",g_config);
    Check(!NativeReadConfiguration(g_config,logging,diagnostics),"reject malformed switch");
    WritePrivateProfileStringW(L"runtime",L"Diagnostics",L"0",g_config);
    WritePrivateProfileStringW(L"size",L"IconSize",L"99",g_config);
    Check(!NativeReadConfiguration(g_config,logging,diagnostics),"reject unvalidated layout override");
    Check(IntSetting(3,L"IconSize")==40,"configuration cannot change fixed native icon size");
    DeleteFileW(g_config);

    const unsigned char fieldLoad[]={0xF2,0x0F,0x10,0x81,0x78,0x03,0,0,0xC3,0,0,0,0,0,0,0};
    WH_DISASM_RESULT instruction{};
    Check(InternalWh_Disasm(nullptr,(void*)fieldLoad,&instruction),"decode field load");
    Check(instruction.length==8,"field-load instruction length");
    Check(!strcmp(instruction.text,"movsd xmm0, qword ptr [rcx+0x378]"),"exact field-load offset");
    Check(InternalWh_Disasm(nullptr,(void*)(fieldLoad+8),&instruction) && !strcmp(instruction.text,"ret"),"decode return boundary");
    const unsigned char negative[]={0xF2,0x0F,0x10,0x41,0xF8,0xC3,0,0,0,0,0,0,0,0,0,0};
    Check(InternalWh_Disasm(nullptr,(void*)negative,&instruction) && !strcmp(instruction.text,"other"),"reject negative member offset");

    BYTE* image=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+128);
    pe->Signature=IMAGE_NT_SIGNATURE;pe->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
    pe->FileHeader.TimeDateStamp=NativeModules[0].timestamp;pe->OptionalHeader.SizeOfImage=NativeModules[0].imageSize;
    Check(NativeValidModule(reinterpret_cast<HMODULE>(image),NativeModules[0]),"valid module pin");
    ++pe->FileHeader.TimeDateStamp;
    Check(!NativeValidModule(reinterpret_cast<HMODULE>(image),NativeModules[0]),"reject different system build");
    --pe->FileHeader.TimeDateStamp;++pe->OptionalHeader.SizeOfImage;
    Check(!NativeValidModule(reinterpret_cast<HMODULE>(image),NativeModules[0]),"reject changed image size");
    VirtualFree(image,0,MEM_RELEASE);

    wchar_t input[32768];_snwprintf_s(input,_TRUNCATE,L"%s\\hash-fixture.bin",g_directory);
    HANDLE file=CreateFileW(input,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    DWORD written=0;WriteFile(file,"abc",3,&written,nullptr);CloseHandle(file);
    wchar_t hash[65];Check(NativeHashFile(input,hash),"SHA256 file read");
    Check(!wcscmp(hash,L"BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"),"SHA256 known vector");
    DeleteFileW(input);
    wchar_t mismatch[256];Check(NativeVerifySystem(mismatch,std::size(mismatch)),"current system hashes verified");

    Check(!InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&FirstHook,(void**)&firstOriginal),"reject registration before initialization");
    Check(!InternalWh_ApplyHookOperations(nullptr),"reject application before initialization");
    Check(InitializeHooks()==MH_OK,"hook manager initialize");
    Function volatile invoke=&Target;
    Check(invoke(5)==16,"original behavior");
    Check(InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&FirstHook,(void**)&firstOriginal),"first hook prepared");
    Check(invoke(5)==16,"prepare leaves original behavior");
    Check(InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&SecondHook,(void**)&secondOriginal),"second hook shares target");
    Check(g_hooks.size()==1,"one physical hook for shared target");
    Check(InternalWh_ApplyHookOperations(nullptr),"hook transaction applied");
    Check(invoke(5)==126,"both hook layers execute");
    Check(firstOriginal(5)==16,"bottom original trampoline");
    Check(secondOriginal(5)==26,"top original links previous layer");
    Check(InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&SecondHook,(void**)&secondOriginal),"duplicate registration is idempotent");
    Check(g_hooks[0].entries.size()==2 && invoke(5)==126,"duplicate does not add another layer");
    Function volatile pending=&PendingTarget;
    Check(InternalWh_SetFunctionHook(nullptr,(void*)&PendingTarget,(void*)&PendingHook,(void**)&pendingOriginal),"prepare unapplied hook before stop");
    Check(pending(5)==27,"queued hook remains disabled");
    // Deterministic contention: a callback arrives before Stop, but cannot
    // acquire the operation lock until after Stop has disabled the hooks.
    // It must not extend a live chain, add a target, or apply a queued hook.
    HANDLE arrived=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    Check(arrived!=nullptr,"late callback synchronization event");
    std::unique_lock hold(g_hookMutex);
    bool extended=false,created=false,duplicate=false,applied=false;
    Function lateOriginal=nullptr,duplicateOriginal=nullptr;
    std::thread callback([&]{
        SetEvent(arrived);
        extended=InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&LateHook,(void**)&lateOriginal);
        created=InternalWh_SetFunctionHook(nullptr,(void*)&LateTarget,(void*)&LateHook,(void**)&lateOriginal);
        duplicate=InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&SecondHook,(void**)&duplicateOriginal);
        applied=InternalWh_ApplyHookOperations(nullptr);
    });
    Check(WaitForSingleObject(arrived,5000)==WAIT_OBJECT_0,"late callback ready before stop");
    Check(SuspendHooks()==MH_OK && invoke(5)==16,"disable restores native function");
    hold.unlock();callback.join();CloseHandle(arrived);
    Check(!extended&&!created&&!duplicate&&!applied,"late callback cannot modify or enable stopped hooks");
    Check(!lateOriginal&&!duplicateOriginal,"rejected callback leaves original pointers untouched");
    Check(g_hooks.size()==2&&g_hooks[0].entries.size()==2,"stop rejects both new targets and new chain layers");
    Check(invoke(5)==16&&pending(5)==27,"late apply leaves both native functions unchanged");
    Check(!g_fault,"expected shutdown rejections do not fault runtime");
    Check(ResumeHooks()==MH_OK && invoke(5)==126 && pending(5)==1027,"explicit resume preserves chain and queued target");
    Check(SuspendHooks()==MH_OK && invoke(5)==16 && pending(5)==27,"second stop restores native functions");
    Check(!InternalWh_ApplyHookOperations(nullptr),"repeated late application remains rejected");
    // Exercise the same real MinHook transaction under concurrent callbacks,
    // using existing registrations so this stress run never grows the chain.
    for(unsigned cycle=0;cycle<20;++cycle) {
        Check(ResumeHooks()==MH_OK,"concurrent cycle resume");
        std::atomic<bool> ready=false,finish=false;
        std::thread racing([&]{
            ready=true;
            while(!finish.load()) {
                InternalWh_SetFunctionHook(nullptr,(void*)&Target,(void*)&SecondHook,nullptr);
                InternalWh_ApplyHookOperations(nullptr);
                SwitchToThread();
            }
        });
        while(!ready.load())SwitchToThread();
        const auto stopped=SuspendHooks();
        finish=true;racing.join();
        Check(stopped==MH_OK && invoke(5)==16 && pending(5)==27 &&
              !InternalWh_ApplyHookOperations(nullptr),"concurrent stop stays native");
    }
    MH_Uninitialize();
    for(auto& hook:g_hooks)VirtualFree(hook.stub,0,MEM_RELEASE);
    printf("%u checks passed, %u failed\n",passed,failed);
    return failed?1:0;
}
