// Foreground events use the existing hide worker, including when occlusion
// considers all windows. Win+D from the corner and the physical keyboard then
// follow the same observed desktop transition, not a guessed input completion.
// Returns true on a desktop transition, which also re-runs the keep-shown
// decision: "show desktop" may only raise the desktop over windows that
// cannot be minimized, with no window event the decision otherwise follows.
thread_local bool g_backdropWasDesktop;
bool ObserveBackdropForeground(HWND window) {
    if(window!=GetForegroundWindow())return false; // skip superseded queued events
    const bool desktop=LeanDockBackdrop::IsDesktop(window);
    const bool changed=desktop!=g_backdropWasDesktop;
    g_backdropWasDesktop=desktop;
    if(!changed)return false;
    std::unordered_set<HWND> secondary;
    if(auto primary=FindTaskbarWindows(&secondary))LeanDockBackdrop::Notify(primary,LeanDockBackdrop::Reason::Desktop);
    for(auto taskbar:secondary)LeanDockBackdrop::Notify(taskbar,LeanDockBackdrop::Reason::Desktop);
    return true;
}

// Read-only diagnostics. The getter is resolved by exact symbol and the
// existing module hash pin; no object fields or memory offsets are assumed.
using ViewCoordinator_IsExpanded_t = bool(WINAPI*)(void*,HWND);
ViewCoordinator_IsExpanded_t ViewCoordinator_IsExpanded_Original;
void ResumeValue(HWND window,const wchar_t* key,intptr_t value) {
    std::wstring name=L"LeanDock.Resume.";name+=key;
    SetPropW(window,name.c_str(),reinterpret_cast<HANDLE>(value+1));
}
int ReadExpandedState(HWND window,const wchar_t* key) {
    if(g_eventStopping || !ViewCoordinator_IsExpanded_Original ||
       GetWindowThreadProcessId(window,nullptr)!=GetCurrentThreadId())return -1;
    const auto it=g_taskbarToViewCoordinator.find(window);
    if(it==g_taskbarToViewCoordinator.end())return -1;
    const bool result=ViewCoordinator_IsExpanded_Original(it->second,window);
    ResumeValue(window,key,result);return result;
}
void ClearResumeState(HWND window) {
    for(auto key:{L"ExpandedAtQuery",L"QueryAt",L"Decision",L"BeforeSync",L"AfterSync",L"SyncAt",L"AtInput",
                  L"HostVisible",L"RootHit",L"InputClass",L"FrozenAtInput",L"InputIcon"}) {
        std::wstring name=L"LeanDock.Resume.";name+=key;RemovePropW(window,name.c_str());
    }
}
