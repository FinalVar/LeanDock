// Optional external-query adaptation, owned by the existing hide lifecycle.
// Native Shell state, registration, work area and animations are untouched.
using AppBarMessage_t=LRESULT(WINAPI*)(void*,const COPYDATASTRUCT*);
AppBarMessage_t AppBarMessage_Original;
decltype(&SetPropW) AppBarSetProp_Original;
decltype(&SHAppBarMessage) AppBarPublic_Original;
bool g_appBarHookInstalled;
struct AppBarCacheEntry {HWND tray{};wchar_t key[24]{};HANDLE native{};};
std::array<AppBarCacheEntry,32> g_appBarCache;
std::mutex g_appBarCacheMutex;

bool OwnedAppBar(HWND window) {
    if(!window || !IsTaskbarWindow(window))return false;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    RECT dock{};
    return pid==GetCurrentProcessId() && LeanDockGeometry::Read(window,dock);
}
DWORD ReadyEdgeController() {
    HWND controller=FindWindowW(NativeControllerClass,nullptr);DWORD pid=0;
    GetWindowThreadProcessId(controller,&pid);
    return pid && GetPropW(controller,LeanDockAppBar::ControllerReady)==reinterpret_cast<HANDLE>(uintptr_t(pid))?pid:0;
}
bool AppBarReady(){return g_appBarHookInstalled && !g_eventStopping && g_wasAutoHideProcessed && ReadyEdgeController();}
bool InternalAppBarCaller(uint32_t wireWindow) {
    HWND window=reinterpret_cast<HWND>(static_cast<intptr_t>(static_cast<int32_t>(wireWindow)));
    wchar_t name[80]{};DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(!pid || !GetClassNameW(window,name,std::size(name)))return false;
    if(!wcscmp(name,NativeBackendClass))return pid==GetCurrentProcessId();
    return !wcscmp(name,NativeControllerClass) && pid==ReadyEdgeController();
}
void CountAppBar(HWND tray,const wchar_t* key) {
    const auto n=reinterpret_cast<uintptr_t>(GetPropW(tray,key));
    AppBarSetProp_Original(tray,key,reinterpret_cast<HANDLE>(n+1));
}
BOOL WINAPI AppBarSetProp_Hook(HWND tray,LPCWSTR key,HANDLE value) {
    if(!LeanDockAppBar::BottomCacheKey(key) || !IsTaskbarWindow(tray))
        return AppBarSetProp_Original(tray,key,value);
    DWORD pid=0;GetWindowThreadProcessId(tray,&pid);
    if(pid!=GetCurrentProcessId())return AppBarSetProp_Original(tray,key,value);
    std::lock_guard lock(g_appBarCacheMutex);
    const bool adjust=AppBarReady() && OwnedAppBar(reinterpret_cast<HWND>(value));
    AppBarCacheEntry* entry=nullptr;AppBarCacheEntry* empty=nullptr;
    for(auto& e:g_appBarCache){if(e.tray==tray && !wcscmp(e.key,key)){entry=&e;break;}if(!e.tray)empty=&e;}
    if(adjust && !entry)entry=empty;
    if(!adjust || !entry){
        const BOOL ok=AppBarSetProp_Original(tray,key,value);
        if(ok && entry)*entry={};return ok;
    }
    // shell32 treats value 1 as a known-empty edge and returns HWND nullptr.
    const BOOL ok=AppBarSetProp_Original(tray,key,reinterpret_cast<HANDLE>(uintptr_t(1)));
    if(ok){entry->tray=tray;wcscpy_s(entry->key,key);entry->native=value;CountAppBar(tray,LeanDockAppBar::CacheWrites);}
    return ok;
}
void RestoreAppBarCache() {
    if(!AppBarSetProp_Original)return;
    std::lock_guard lock(g_appBarCacheMutex);
    for(auto& entry:g_appBarCache){
        DWORD pid=0;GetWindowThreadProcessId(entry.tray,&pid);
        if(pid==GetCurrentProcessId() && GetPropW(entry.tray,entry.key)==reinterpret_cast<HANDLE>(uintptr_t(1)))
            AppBarSetProp_Original(entry.tray,entry.key,entry.native);
        entry={};
    }
}
int CALLBACK SyncAppBarProperty(HWND tray,LPWSTR key,HANDLE value,ULONG_PTR) {
    if(LeanDockAppBar::BottomCacheKey(key) && OwnedAppBar(reinterpret_cast<HWND>(value)))AppBarSetProp_Hook(tray,key,value);
    return TRUE;
}
void SyncAppBarCache() {
    if(!AppBarReady()){RestoreAppBarCache();return;}
    HWND tray=FindWindowW(L"Shell_TrayWnd",nullptr);
    if(OwnedAppBar(tray))EnumPropsExW(tray,SyncAppBarProperty,0);
}
LRESULT WINAPI AppBarMessage_Hook(void* self,const COPYDATASTRUCT* data) {
    LeanDockAppBar::WireRequest request{};
    const bool parsed=LeanDockAppBar::ReadRequest(data,request);
    const LRESULT result=AppBarMessage_Original(self,data);
    if(!parsed || !AppBarReady())return result;
    HWND primary=FindWindowW(L"Shell_TrayWnd",nullptr);
    const bool primaryOwned=request.message==ABM_GETSTATE && OwnedAppBar(primary);
    const bool returnedOwned=(request.message==ABM_GETAUTOHIDEBAR || request.message==ABM_GETAUTOHIDEBAREX) &&
        request.edge==ABE_BOTTOM && OwnedAppBar(reinterpret_cast<HWND>(result));
    const auto filtered=LeanDockAppBar::Filter(request,static_cast<ULONG_PTR>(result),true,
        InternalAppBarCaller(request.window),primaryOwned,returnedOwned);
    if(filtered!=static_cast<ULONG_PTR>(result) && primary)CountAppBar(primary,LeanDockAppBar::Queries);
    return static_cast<LRESULT>(filtered);
}
ULONG_PTR WINAPI AppBarPublic_Hook(DWORD query,PAPPBARDATA data) {
    // Explorer's internal reads bypass the external cache using its owned HWND.
    // The serialized PID is the shared-memory receiver, never a caller marker.
    if(AppBarReady() && data && data->cbSize==sizeof(*data) &&
       (query==ABM_GETSTATE || query==ABM_GETAUTOHIDEBAR || query==ABM_GETAUTOHIDEBAREX)) {
        HWND backend=nullptr;DWORD pid=0;
        while((backend=FindWindowExW(nullptr,backend,NativeBackendClass,nullptr))) {
            GetWindowThreadProcessId(backend,&pid);if(pid==GetCurrentProcessId())break;
        }
        ULONG_PTR result=0;
        if(backend && LeanDockAppBar::NativeWireQuery(FindWindowW(L"Shell_TrayWnd",nullptr),backend,query,data->uEdge,data->rc,result))return result;
    }
    return AppBarPublic_Original(query,data);
}
void PublishAppBarSupport(HWND window) {
    if(!g_eventStopping && g_appBarHookInstalled && IsTaskbarWindow(window)) {
        SetPropW(window,LeanDockAppBar::HookReady,reinterpret_cast<HANDLE>(uintptr_t(GetCurrentProcessId())));
        SyncAppBarCache();
    }
}
void InstallAppBarCompatibility() {
    const HMODULE module=GetModuleHandleW(nullptr),shell=GetModuleHandleW(L"shell32.dll");
    wchar_t path[32768]{},hash[65]{};
    const NativeModulePin pins[]={
        {L"explorer.exe",L"",L"EEE089FE6F927C7F0D51B0ABD9D70C094157E33673AD760713EC7B1CEA18EE11",3202867519u,3424256u},
        {L"shell32.dll",L"",L"C55A5F3B6EF059871FE2A0C8D82F086E0998D1154023E3ECF49698B97F9341F6",292758140u,7979008u}};
    const HMODULE modules[]={module,shell};
    for(unsigned i=0;i<2;++i)if(!GetModuleFileNameW(modules[i],path,std::size(path)) ||
        !NativeHashFile(path,hash) || _wcsicmp(hash,pins[i].sha256) || !NativeValidModule(modules[i],pins[i])) {
        Wh_Log(L"AppBar gap compatibility unavailable: module identity differs; native behavior retained");return;
    }
    // RVA 0x603f0, function extent [0x603f0,0x605b0), public symbol:
    // CTray::_OnAppBarMessage(tagCOPYDATASTRUCT*). Original is called first;
    // no borrowed request, shared memory or object field is ever modified.
    const bool cache=WindhawkUtils::SetFunctionHook(SetPropW,AppBarSetProp_Hook,&AppBarSetProp_Original);
    const bool internal=WindhawkUtils::SetFunctionHook(SHAppBarMessage,AppBarPublic_Hook,&AppBarPublic_Original);
    const bool messages=WindhawkUtils::SetFunctionHook(
        reinterpret_cast<AppBarMessage_t>(reinterpret_cast<BYTE*>(module)+0x603f0),
        AppBarMessage_Hook,&AppBarMessage_Original);
    g_appBarHookInstalled=cache && internal && messages;
}
