// NativeTaskbarDock dedicated runtime. GPL-3.0; see NOTICE.md.
#define ND_COMPONENT 0
#include "native-api-latest.h"
#include "protocol-latest.h"
#include "verify-latest.h"
#include "settings-generated.h"
#include "config-latest.h"
#include "../vendor/minhook/include/MinHook.h"
#include "../vendor/minhook/src/hde/hde64.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <cwchar>
#include <cstring>

static HMODULE g_module;
static wchar_t g_directory[32768],g_config[32768];
static HWND g_window;
static std::atomic<unsigned> g_state{NativeIdle};
static std::atomic<bool> g_startIssued{false},g_fault{false};
static bool g_logging=false,g_diagnostics=false,g_everInitialized=false,g_hasApplied=false;
static bool g_stoppingCore=false;
static HANDLE g_owner;
static PTP_WAIT g_ownerWait;
static DWORD g_ownerPid;
static std::recursive_mutex g_hookMutex;
static std::mutex g_storageMutex;
static std::map<std::wstring,std::wstring> g_storage;
static const wchar_t* g_sections[]={L"runtime",L"styler",L"tray",L"size",L"hide",L"show"};

static std::wstring FilePath(const wchar_t* name) {
    return std::wstring(g_directory)+L"\\"+name;
}
static void Log(const wchar_t* message) {
    wchar_t line[2048];
    SYSTEMTIME now;GetLocalTime(&now);
    _snwprintf_s(line,_TRUNCATE,L"%04u-%02u-%02u %02u:%02u:%02u pid=%lu %s\r\n",
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
        GetCurrentProcessId(),message);
    char utf8[8192];int bytes=WideCharToMultiByte(CP_UTF8,0,line,-1,utf8,sizeof(utf8),nullptr,nullptr);
    HANDLE file=CreateFileW(FilePath(L"latest.log").c_str(),FILE_APPEND_DATA,
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE) {
        DWORD written;if(bytes>1)WriteFile(file,utf8,bytes-1,&written,nullptr);
        CloseHandle(file);
    }
}
extern "C" void Native_ReportFailure(const wchar_t* stage,HRESULT error) {
    g_fault=true;wchar_t line[1024];
    _snwprintf_s(line,_TRUNCATE,L"%s failed: 0x%08X",stage,static_cast<unsigned>(error));
    Log(line);
}
extern "C" BOOL InternalWh_IsLogEnabled(void*) {return g_logging;}
extern "C" void InternalWh_Log(void*,PCWSTR format,va_list args) {
    wchar_t line[1800];_vsnwprintf_s(line,_TRUNCATE,format,args);Log(line);
}
static std::wstring Setting(void* component,PCWSTR format,va_list args) {
    const unsigned id=static_cast<unsigned>(reinterpret_cast<uintptr_t>(component));
    if(id>=std::size(g_sections))return {};
    wchar_t name[256];_vsnwprintf_s(name,_TRUNCATE,format,args);
    const wchar_t* fallback=L"";
    for(auto const& item:NativeDefaults)
        if(item.component==static_cast<int>(id) && !wcscmp(item.key,name)){fallback=item.value;break;}
    if(id==2 && !wcscmp(name,L"debugLayout"))return g_diagnostics?L"1":L"0";
    return fallback;
}
extern "C" int InternalWh_GetIntSetting(void* m,PCWSTR f,va_list args) {
    return static_cast<int>(wcstol(Setting(m,f,args).c_str(),nullptr,10));
}
extern "C" PCWSTR InternalWh_GetStringSetting(void* m,PCWSTR f,va_list args) {
    auto value=Setting(m,f,args);
    auto copy=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,(value.size()+1)*sizeof(wchar_t)));
    if(!copy)throw std::bad_alloc();
    memcpy(copy,value.c_str(),(value.size()+1)*sizeof(wchar_t));return copy;
}
extern "C" void InternalWh_FreeStringSetting(void*,PCWSTR value) {
    if(value)HeapFree(GetProcessHeap(),0,const_cast<wchar_t*>(value));
}
extern "C" int InternalWh_GetIntValue(void*,PCWSTR,int fallback){return fallback;}
extern "C" BOOL InternalWh_SetIntValue(void*,PCWSTR,int){return FALSE;}
extern "C" size_t InternalWh_GetStringValue(void*,PCWSTR name,PWSTR result,size_t capacity) {
    std::lock_guard guard(g_storageMutex);auto it=g_storage.find(name);
    if(it==g_storage.end() || !capacity || it->second.size()>=capacity){if(capacity)result[0]=0;return 0;}
    wcscpy_s(result,capacity,it->second.c_str());return it->second.size();
}
extern "C" BOOL InternalWh_SetStringValue(void*,PCWSTR name,PCWSTR value) {
    std::lock_guard guard(g_storageMutex);g_storage[name]=value?value:L"";return TRUE;
}
extern "C" size_t InternalWh_GetBinaryValue(void*,PCWSTR,void*,size_t){return 0;}
extern "C" BOOL InternalWh_SetBinaryValue(void*,PCWSTR,const void*,size_t){return FALSE;}
extern "C" BOOL InternalWh_DeleteValue(void*,PCWSTR){return FALSE;}
extern "C" size_t InternalWh_GetModStoragePath(void*,PWSTR result,size_t capacity) {
    const size_t size=wcslen(g_directory);if(size>=capacity)return 0;
    wcscpy_s(result,capacity,g_directory);return size;
}
// Remote images and telemetry are intentionally absent from the dedicated profile.
extern "C" const WH_URL_CONTENT* InternalWh_GetUrlContent(void*,PCWSTR,const WH_GET_URL_CONTENT_OPTIONS*){return nullptr;}
extern "C" void InternalWh_FreeUrlContent(void*,const WH_URL_CONTENT*){}

// Stable writable pointer + RX branch stub allow several core units to share
// one physical detour. No callback points into memory that is freed on Stop.
struct HookChain {
    void* target;
    void* trampoline;
    BYTE* stub;
    void** destination;
    std::vector<std::pair<void*,void**>> entries;
};
static std::vector<HookChain> g_hooks;
extern "C" BOOL InternalWh_SetFunctionHook(void*,void* target,void* hook,void** original) {
    if(!target || !hook)return FALSE;
    std::lock_guard guard(g_hookMutex);
    for(auto& chain:g_hooks) {
        if(chain.target!=target)continue;
        for(auto const& entry:chain.entries)if(entry.first==hook) {
            if(original && entry.second)*original=*entry.second;
            return TRUE;
        }
        if(original)*original=*chain.destination;
        chain.entries.emplace_back(hook,original);
        InterlockedExchangePointer(chain.destination,hook);
        return TRUE;
    }
    // Separate pages ensure the function pointer remains RW while code is RX.
    auto memory=static_cast<BYTE*>(VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!memory){Native_ReportFailure(L"hook allocation",HRESULT_FROM_WIN32(GetLastError()));return FALSE;}
    auto destination=reinterpret_cast<void**>(memory+4096);
    memory[0]=0xFF;memory[1]=0x25;
    const int32_t displacement=4096-6;memcpy(memory+2,&displacement,sizeof(displacement));
    *destination=hook;DWORD oldProtect;
    if(!VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&oldProtect)) {
        VirtualFree(memory,0,MEM_RELEASE);return FALSE;
    }
    FlushInstructionCache(GetCurrentProcess(),memory,6);
    void* trampoline=nullptr;
    const MH_STATUS result=MH_CreateHook(target,memory,&trampoline);
    if(result!=MH_OK) {
        wchar_t line[160];_snwprintf_s(line,_TRUNCATE,L"MH_CreateHook failed (%d)",result);Log(line);
        g_fault=true;VirtualFree(memory,0,MEM_RELEASE);return FALSE;
    }
    if(original)*original=trampoline;
    g_hooks.push_back({target,trampoline,memory,destination,{{hook,original}}});
    MH_QueueEnableHook(target);
    return TRUE;
}
extern "C" BOOL InternalWh_RemoveFunctionHook(void*,void*){return FALSE;}
extern "C" BOOL InternalWh_ApplyHookOperations(void*) {
    std::lock_guard guard(g_hookMutex);
    const auto status=MH_ApplyQueued();
    if(status!=MH_OK)Native_ReportFailure(L"apply hooks",E_FAIL);
    return status==MH_OK;
}
extern "C" BOOL InternalWh_HookSymbols(void* component,HMODULE module,
    const WH_SYMBOL_HOOK* requested,size_t count,const WH_HOOK_SYMBOLS_OPTIONS*) {
    wchar_t path[32768];if(!GetModuleFileNameW(module,path,std::size(path)))return FALSE;
    const wchar_t* name=wcsrchr(path,L'\\');name=name?name+1:path;
    const NativeModulePin* pin=nullptr;
    for(auto const& candidate:NativeModules)if(!_wcsicmp(name,candidate.name)){pin=&candidate;break;}
    if(!pin || !NativeValidModule(module,*pin)) {
        Native_ReportFailure(L"unrecognized taskbar module",HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
        return FALSE;
    }
    // Resolve the complete batch before registering any detour.
    std::vector<void*> addresses(count,nullptr);
    for(size_t i=0;i<count;i++) {
        for(size_t j=0;j<requested[i].symbolsCount && !addresses[i];j++) {
            std::wstring_view wanted(requested[i].symbols[j].string,requested[i].symbols[j].length);
            for(auto const& symbol:NativeSymbols)
                if(!_wcsicmp(symbol.module,name) && wanted==symbol.name) {
                    addresses[i]=reinterpret_cast<BYTE*>(module)+symbol.rva;break;
                }
        }
        if(!addresses[i] && !requested[i].optional) {
            std::wstring message=L"Missing pinned symbol: ";
            if(requested[i].symbolsCount)message.append(requested[i].symbols[0].string,requested[i].symbols[0].length);
            Log(message.c_str());g_fault=true;return FALSE;
        }
    }
    for(size_t i=0;i<count;i++)if(addresses[i]) {
        if(requested[i].hookFunction) {
            if(!InternalWh_SetFunctionHook(component,addresses[i],requested[i].hookFunction,
                                          requested[i].pOriginalFunction))return FALSE;
        } else if(requested[i].pOriginalFunction)*requested[i].pOriginalFunction=addresses[i];
    }
    return TRUE;
}
extern "C" HANDLE InternalWh_FindFirstSymbol4(void*,HMODULE,const WH_FIND_SYMBOL_OPTIONS*,WH_FIND_SYMBOL*){return nullptr;}
extern "C" BOOL InternalWh_FindNextSymbol2(void*,HANDLE,WH_FIND_SYMBOL*){return FALSE;}
extern "C" void InternalWh_FindCloseSymbol(void*,HANDLE){}
extern "C" BOOL InternalWh_Disasm(void*,void* address,WH_DISASM_RESULT* result) {
    if(!address || !result)return FALSE;
    hde64s decoded{};const auto length=hde64_disasm(address,&decoded);
    if(!length || decoded.flags&F_ERROR)return FALSE;
    result->length=length;strcpy_s(result->text,"other");
    if(decoded.opcode==0xC2 || decoded.opcode==0xC3)strcpy_s(result->text,"ret");
    // The pinned x64 size core only asks for this specific field load.
    if(decoded.p_rep==0xF2 && decoded.opcode==0x0F && decoded.opcode2==0x10 &&
       !decoded.rex_b && decoded.modrm_rm==1 &&
       (decoded.modrm_mod==1 || decoded.modrm_mod==2)) {
        int32_t offset=decoded.modrm_mod==1?static_cast<int8_t>(decoded.disp.disp8):static_cast<int32_t>(decoded.disp.disp32);
        if(offset>0 && offset<=0xFFFF)
            sprintf_s(result->text,"movsd xmm%u, qword ptr [rcx+0x%x]",
                      decoded.modrm_reg+decoded.rex_r*8,static_cast<unsigned>(offset));
    }
    return TRUE;
}

#ifndef NATIVE_RUNTIME_TEST
#define DECLARE_CORE(name) extern "C" BOOL Native_##name##_init(); \
    extern "C" void Native_##name##_after(); extern "C" void Native_##name##_stop();
DECLARE_CORE(styler) DECLARE_CORE(tray) DECLARE_CORE(size) DECLARE_CORE(hide) DECLARE_CORE(show)
extern "C" void Native_tray_before();
extern "C" void Native_size_before();
extern "C" void Native_tray_resume();
extern "C" void Native_size_resume();
extern "C" unsigned Native_styler_count();
static bool g_initializedCore[5]{};

static void SaveState() {
    unsigned elements=0;
    if(g_state==NativeActive)elements=Native_styler_count();
    char json[1024];
    const int size=snprintf(json,sizeof(json),
        "{\"protocol\":1,\"version\":\"0.4.11\",\"explorerPid\":%lu,\"ownerPid\":%lu,\"state\":%u,\"hooks\":%zu,\"styledElements\":%u,\"failed\":%s}\n",
        GetCurrentProcessId(),g_ownerPid,g_state.load(),g_hooks.size(),elements,g_fault?"true":"false");
    HANDLE file=CreateFileW(FilePath(L"latest-backend.json").c_str(),GENERIC_WRITE,
        FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(file,json,size,&written,nullptr);CloseHandle(file);}
    std::lock_guard guard(g_storageMutex);
    for(auto const& [key,value]:g_storage) {
        if(key!=L"mergedGridState")continue;
        const auto path=FilePath(L"latest-layout.json");
        HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f!=INVALID_HANDLE_VALUE) {
            const int length=WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,nullptr,0,nullptr,nullptr);
            std::vector<char> data(length);
            WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,data.data(),length,nullptr,nullptr);
            DWORD written;if(length>1)WriteFile(f,data.data(),length-1,&written,nullptr);CloseHandle(f);
        }
    }
}
static void ClearOwner() {
    if(g_ownerWait) {
        SetThreadpoolWait(g_ownerWait,nullptr,nullptr);
        WaitForThreadpoolWaitCallbacks(g_ownerWait,TRUE);
        CloseThreadpoolWait(g_ownerWait);g_ownerWait=nullptr;
    }
    if(g_owner){CloseHandle(g_owner);g_owner=nullptr;}
    g_ownerPid=0;
}
static void CALLBACK OwnerExited(PTP_CALLBACK_INSTANCE,void* owner,PTP_WAIT,TP_WAIT_RESULT) {
    PostMessageW(g_window,NativeOwnerExit,reinterpret_cast<WPARAM>(owner),0);
}
static bool SetOwner(DWORD pid) {
    DWORD session=0,ownSession=0;
    if(!pid || !ProcessIdToSessionId(pid,&session) ||
       !ProcessIdToSessionId(GetCurrentProcessId(),&ownSession) || session!=ownSession)return false;
    HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return false;
    wchar_t path[32768];DWORD length=std::size(path);
    const auto expected=FilePath(L"latest.exe");
    if(!QueryFullProcessImageNameW(process,0,path,&length) || _wcsicmp(path,expected.c_str()) ||
       WaitForSingleObject(process,0)!=WAIT_TIMEOUT){CloseHandle(process);return false;}
    if(g_ownerPid==pid && g_owner && WaitForSingleObject(g_owner,0)==WAIT_TIMEOUT){CloseHandle(process);return true;}
    auto wait=CreateThreadpoolWait(OwnerExited,reinterpret_cast<void*>(static_cast<uintptr_t>(pid)),nullptr);
    if(!wait){CloseHandle(process);return false;}
    ClearOwner();g_owner=process;g_ownerPid=pid;g_ownerWait=wait;
    SetThreadpoolWait(wait,process,nullptr);return true;
}
static void StopCore() {
    if(g_stoppingCore)return;
    if(g_state!=NativeActive && g_state!=NativeStarting)return;
    struct StopGuard{StopGuard(){g_stoppingCore=true;}~StopGuard(){g_stoppingCore=false;}} stopGuard;
    Log(L"Stopping native Dock and restoring original properties");
    bool restored=true;
    auto restore=[&](unsigned index,void (*function)()) {
        if(!g_hasApplied || !g_initializedCore[index])return;
        try{function();}catch(...){restored=false;Native_ReportFailure(L"property restoration",E_FAIL);}
    };
    restore(3,Native_hide_stop);
    restore(1,Native_tray_before);
    restore(2,Native_size_before);
    const auto disabled=MH_DisableHook(MH_ALL_HOOKS);
    if(disabled!=MH_OK && disabled!=MH_ERROR_NOT_INITIALIZED){restored=false;Native_ReportFailure(L"hook restoration",E_FAIL);}
    restore(0,Native_styler_stop);
    restore(4,Native_show_stop);
    restore(1,Native_tray_stop);
    restore(2,Native_size_stop);
    g_hasApplied=false;
    ClearOwner();g_state=restored?NativeStopped:NativeFailed;SaveState();
    // Keep the image and trampolines pinned until Explorer exits; callbacks may
    // still be unwinding. They consume no polling or controller process.
}
static bool StartCore(DWORD owner) {
    if(g_state==NativeActive)return SetOwner(owner);
    if(g_state==NativeFailed)return false;
    if(!SetOwner(owner))return false;
    g_state=NativeStarting;g_fault=false;
    if(!g_everInitialized) {
        if(MH_Initialize()!=MH_OK){Native_ReportFailure(L"hook initialization",E_FAIL);StopCore();g_state=NativeFailed;SaveState();return false;}
        BOOL (*init[])(void)={Native_styler_init,Native_tray_init,Native_size_init,Native_hide_init,Native_show_init};
        // One transaction: resolve all core symbols and only then enable hooks.
        for(unsigned i: {2u,1u,3u,4u,0u}) {
            if(!init[i]() || g_fault) {
                Native_ReportFailure(L"core initialization",E_FAIL);StopCore();g_state=NativeFailed;SaveState();return false;
            }
            g_initializedCore[i]=true;
        }
        if(!InternalWh_ApplyHookOperations(nullptr)){StopCore();g_state=NativeFailed;return false;}
        g_everInitialized=true;
        g_hasApplied=true;
        Native_size_after();Native_tray_after();
    } else {
        if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){Native_ReportFailure(L"hook resume",E_FAIL);StopCore();g_state=NativeFailed;SaveState();return false;}
        g_hasApplied=true;
        Native_size_resume();Native_tray_resume();
    }
    Native_show_after();Native_hide_after();Native_styler_after();
    if(g_fault){StopCore();g_state=NativeFailed;SaveState();return false;}
    g_state=NativeActive;SaveState();Log(L"Native Dock active");return true;
}
static LRESULT CALLBACK BackendProc(HWND w,UINT message,WPARAM p,LPARAM l) {
    try {
    if(g_stoppingCore && (message==NativeQuery||message==NativeStart||message==NativeStop||message==NativeOwnerExit))return NativeStarting;
    if(message==NativeQuery){SaveState();return g_state.load();}
    if(message==NativeStop || (message==NativeOwnerExit && static_cast<DWORD>(p)==g_ownerPid)){StopCore();return g_state.load();}
    if(message==NativeStart){return StartCore(static_cast<DWORD>(p))?g_state.load():NativeFailed;}
    if(message==WM_ENDSESSION && p){StopCore();return 0;}
    return DefWindowProcW(w,message,p,l);
    }catch(...){Native_ReportFailure(L"backend message",E_FAIL);StopCore();g_state=NativeFailed;SaveState();return NativeFailed;}
}
static DWORD WINAPI BackendThread(void* owner) {
    if(!NativeReadConfiguration(g_config,g_logging,g_diagnostics)){
        Native_ReportFailure(L"invalid configuration",E_INVALIDARG);g_state=NativeFailed;SaveState();return 1;
    }
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    WNDCLASSW type{};type.hInstance=g_module;type.lpfnWndProc=BackendProc;type.lpszClassName=NativeBackendClass;
    RegisterClassW(&type);
    g_window=CreateWindowExW(WS_EX_TOOLWINDOW,NativeBackendClass,L"",WS_POPUP,
                            0,0,0,0,nullptr,nullptr,g_module,nullptr);
    if(!g_window){g_state=NativeFailed;SaveState();return 1;}
    try{StartCore(static_cast<DWORD>(reinterpret_cast<uintptr_t>(owner)));}
    catch(...){Native_ReportFailure(L"unhandled initialization exception",E_FAIL);StopCore();g_state=NativeFailed;SaveState();}
    MSG message;
    while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    CoUninitialize();return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI NativeDock_Start(void* owner) {
    if(g_startIssued.exchange(true)) {
        if(g_window)PostMessageW(g_window,NativeStart,reinterpret_cast<WPARAM>(owner),0);
        return 0;
    }
    HMODULE pinned;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                      reinterpret_cast<LPCWSTR>(&NativeDock_Start),&pinned);
    HANDLE thread=CreateThread(nullptr,0,BackendThread,owner,0,nullptr);
    if(!thread){g_startIssued=false;return GetLastError();}
    CloseHandle(thread);return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {
        g_module=instance;GetModuleFileNameW(instance,g_directory,std::size(g_directory));
        auto slash=wcsrchr(g_directory,L'\\');if(slash)*slash=0;
        _snwprintf_s(g_config,_TRUNCATE,L"%s\\latest.ini",g_directory);
        // No allocation-heavy initialization, waits or hooks under loader lock.
    }
    return TRUE;
}
#endif
