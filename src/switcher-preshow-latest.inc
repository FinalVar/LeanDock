// Pre-show scope adapted from L3r0y's Alt+Tab window delayer 1.2.0 (MIT).
// See NOTICE.md and upstream/switcher-delayer. We preserve ShowWindow/focus
// and briefly cloak only the corner-request host instead of withholding Show.
#include <dwmapi.h>

namespace NativeSwitcherPreshow {
constexpr NativeModulePin kPin{L"twinui.pcshell.dll", L"C:\\Windows\\System32\\twinui.pcshell.dll",
    L"8BA440EDED430A2417692A98B34695A12AD0EE000333DFED07E0C2723E17111C",3162946381u,10113024u};
// Microsoft public PDB 9507F3DA-D47F-5F02-6569-FC50273C40D7, DBI age 1.
constexpr unsigned kDisplayRva=104492;
constexpr PCWSTR kProperty=L"LeanDock.SwitcherPreshow.0.8.70";
constexpr ULONGLONG kMaximumDelay=200;
static std::mutex mutex;
static bool enabled{}, supported{};
static HWND window{};
static unsigned long long ticket{};
static ULONGLONG deadline{};
static PTP_TIMER watchdog{};
static unsigned held{},revealed{},timedOut{},errors{},uncloakDeferred{};
static unsigned fromShow{},fromUncloak{},nativeClosed{};
static thread_local unsigned displayDepth{};
using DisplayFn=void(WINAPI*)(void*);
static DisplayFn displayOriginal;
static decltype(&ShowWindow) showOriginal;
static decltype(&DwmSetWindowAttribute) dwmOriginal=DwmSetWindowAttribute;

void PublishLocked() noexcept {
    wchar_t value[768]{};
    _snwprintf_s(value,_TRUNCATE,
        L"{\"schema\":2,\"supported\":%ls,\"enabled\":%ls,\"pending\":%ls,\"held\":%u,\"released\":%u,\"timeouts\":%u,\"errors\":%u,\"uncloakDeferred\":%u,\"fromShow\":%u,\"fromUncloak\":%u,\"nativeClosed\":%u,\"maximumDelayMs\":200,\"method\":\"pre-display-app-cloak\"}",
        supported?L"true":L"false",enabled?L"true":L"false",window?L"true":L"false",held,revealed,timedOut,errors,uncloakDeferred,fromShow,fromUncloak,nativeClosed);
    InternalWh_SetStringValue(nullptr,L"switcherPreshowState",value);
}
void ScheduleLocked(ULONGLONG milliseconds) noexcept {
    LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(milliseconds*10000);
    FILETIME time{due.LowPart,static_cast<DWORD>(due.HighPart)};
    SetThreadpoolTimer(watchdog,&time,0,0);
}
enum class Release { Reveal, NativeAlreadyCloaked };
void ReleaseLocked(Release mode=Release::Reveal) noexcept {
    if(!window)return;
    // The property prevents a late callback from touching a recycled HWND.
    if(GetPropW(window,kProperty)==reinterpret_cast<HANDLE>(&window)){
        if(mode==Release::Reveal){
            BOOL cloak=FALSE;
            if(FAILED(dwmOriginal(window,DWMWA_CLOAK,&cloak,sizeof(cloak))))++errors;
        }
        RemovePropW(window,kProperty);
    }
    window=nullptr;ticket=0;deadline=0;++revealed;
    if(watchdog)SetThreadpoolTimer(watchdog,nullptr,0,0);
    PublishLocked();
}
void CALLBACK Timeout(PTP_CALLBACK_INSTANCE,void*,PTP_TIMER) noexcept {
    std::lock_guard guard(mutex);
    if(!window)return;
    auto now=GetTickCount64();
    // A callback already queued for an older request must not reveal a newer
    // one early. This is a rearmed one-shot, never an idle polling timer.
    if(now<deadline){ScheduleLocked(deadline-now);return;}
    Native_SwitcherConsume(ticket);++timedOut;ReleaseLocked();
}
void Reveal(unsigned long long request) noexcept {
    std::lock_guard guard(mutex);
    if(window&&ticket==request)ReleaseLocked();
}
enum class Entry { Show, NativeUncloak };
void Begin(HWND hwnd,Entry entry=Entry::Show) noexcept {
    const auto request=Native_SwitcherPeek();
    // Reused hosts can keep WS_VISIBLE while native DWM cloak hides them.
    // Only a native uncloak within DisplayAltTab authorizes taking over that
    // existing cloak; an arbitrary ShowWindow must not claim it.
    const bool nativeUncloak=entry==Entry::NativeUncloak;
    if(!request||(!nativeUncloak&&IsWindowVisible(hwnd)))return;
    DWORD pid=0;wchar_t name[80]{};
    if(GetWindowThreadProcessId(hwnd,&pid)!=GetCurrentThreadId()||pid!=GetCurrentProcessId()||
       !GetClassNameW(hwnd,name,80)||wcscmp(name,L"XamlExplorerHostIslandWindow"))return;
    std::lock_guard guard(mutex);
    if(!enabled||!watchdog||Native_SwitcherPeek()!=request)return;
    if(window==hwnd&&ticket==request)return;
    if(window==hwnd&&GetPropW(hwnd,kProperty)==reinterpret_cast<HANDLE>(&window)){
        // Superseding a ticket on the same host must not create an uncloak /
        // recloak gap. The old timer checks the new deadline under this lock.
        ticket=request;deadline=GetTickCount64()+kMaximumDelay;
        ScheduleLocked(kMaximumDelay);++held;
        if(nativeUncloak)++fromUncloak;else ++fromShow;
        PublishLocked();return;
    }
    ReleaseLocked();
    DWORD cloaked=0;
    if(FAILED(DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))||
       (!nativeUncloak&&(cloaked&DWM_CLOAKED_APP))||GetPropW(hwnd,kProperty))return;
    if(!SetPropW(hwnd,kProperty,reinterpret_cast<HANDLE>(&window))){++errors;PublishLocked();return;}
    BOOL cloak=TRUE;
    if(!(cloaked&DWM_CLOAKED_APP)&&FAILED(dwmOriginal(hwnd,DWMWA_CLOAK,&cloak,sizeof(cloak)))){
        RemovePropW(hwnd,kProperty);++errors;PublishLocked();return;
    }
    window=hwnd;ticket=request;deadline=GetTickCount64()+kMaximumDelay;
    ScheduleLocked(kMaximumDelay);++held;
    if(nativeUncloak)++fromUncloak;else ++fromShow;
    PublishLocked();
}
BOOL WINAPI ShowHook(HWND hwnd,int command) {
    if(displayDepth && command!=SW_HIDE)Begin(hwnd);
    const BOOL result=showOriginal(hwnd,command);
    if(command==SW_HIDE){
        // Restore only after the native host has hidden, never before its last
        // frame. No private activation or synthetic Enter is introduced.
        std::lock_guard guard(mutex);
        if(window==hwnd&&!IsWindowVisible(hwnd)){
            Native_SwitcherConsume(ticket);ReleaseLocked();
        }
    }
    return result;
}
void WINAPI DisplayHook(void* self) {
    struct Scope{Scope(){++displayDepth;}~Scope(){--displayDepth;}} scope;
    displayOriginal(self);
}
HRESULT WINAPI DwmHook(HWND hwnd,DWORD attribute,LPCVOID value,DWORD size) {
    if(attribute==DWMWA_CLOAK&&value&&size==sizeof(BOOL)){
        const bool cloak=*static_cast<const BOOL*>(value)!=FALSE;
        if(!cloak&&displayDepth)Begin(hwnd,Entry::NativeUncloak);
        std::lock_guard guard(mutex);
        // DisplayAltTab makes the native host visible and then uncloaks it.
        // Preserve our owned gate across that native call; all other windows
        // and all other DWM attributes pass through without modification.
        if(enabled&&window==hwnd&&GetPropW(hwnd,kProperty)==reinterpret_cast<HANDLE>(&window)){
            if(cloak){
                // Native dismissal already wants this window cloaked. Apply
                // that request and relinquish ownership WITHOUT uncloaking;
                // neither a late callback nor Stop may expose it again.
                const HRESULT result=dwmOriginal(hwnd,attribute,value,size);
                if(SUCCEEDED(result)){
                    Native_SwitcherConsume(ticket);++nativeClosed;
                    ReleaseLocked(Release::NativeAlreadyCloaked);
                }else{++errors;PublishLocked();}
                return result;
            }
            ++uncloakDeferred;PublishLocked();return S_OK;
        }
    }
    return dwmOriginal(hwnd,attribute,value,size);
}
bool Initialize() noexcept {
    auto module=GetModuleHandleW(kPin.name);wchar_t path[MAX_PATH]{},hash[65]{};
    // Optional adaptation: an unknown OS build keeps the accepted native
    // switch/confirmation path. It must not disable the entire Dock.
    if(!module||!GetModuleFileNameW(module,path,MAX_PATH)||!NativeValidModule(module,kPin)||
       !NativeHashFile(path,hash)||_wcsicmp(hash,kPin.sha256)){PublishLocked();return true;}
    watchdog=CreateThreadpoolTimer(Timeout,nullptr,nullptr);
    if(!watchdog){++errors;PublishLocked();return true;}
    // Shared runtime hook chains own detours/trampolines and pin the image on
    // shutdown. Hook registration still participates in the core transaction.
    if(!InternalWh_SetFunctionHook(nullptr,reinterpret_cast<BYTE*>(module)+kDisplayRva,
        reinterpret_cast<void*>(DisplayHook),reinterpret_cast<void**>(&displayOriginal))||
       !InternalWh_SetFunctionHook(nullptr,reinterpret_cast<void*>(ShowWindow),
        reinterpret_cast<void*>(ShowHook),reinterpret_cast<void**>(&showOriginal))||
       !InternalWh_SetFunctionHook(nullptr,reinterpret_cast<void*>(DwmSetWindowAttribute),
        reinterpret_cast<void*>(DwmHook),reinterpret_cast<void**>(&dwmOriginal)))return false;
    supported=true;PublishLocked();return true;
}
void Start() noexcept {
    std::lock_guard guard(mutex);
    if(supported&&!watchdog)watchdog=CreateThreadpoolTimer(Timeout,nullptr,nullptr);
    enabled=supported&&watchdog;PublishLocked();
}
void Stop() noexcept {
    PTP_TIMER timer{};
    {std::lock_guard guard(mutex);enabled=false;ReleaseLocked();timer=watchdog;watchdog=nullptr;
        if(timer)SetThreadpoolTimer(timer,nullptr,0,0);PublishLocked();}
    if(timer){WaitForThreadpoolTimerCallbacks(timer,TRUE);CloseThreadpoolTimer(timer);}
}
}
extern "C" void Native_SwitcherReveal(unsigned long long ticket){NativeSwitcherPreshow::Reveal(ticket);}
