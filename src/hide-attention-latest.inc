// Selective attention policy adapted from Taskbar Auto-Hide Motion 1.0.8
// (jinSeong-P, GPL-3.0, pinned source and differences in NOTICE.md).
// Scope the IsFlashing override to native auto-hide permission queries;
// never clear a task item, intercept notification delivery, or alter drawing.
using CTaskListWnd_PermitAutoHide_t = BOOL(WINAPI*)(void*);
CTaskListWnd_PermitAutoHide_t CTaskListWnd_PermitAutoHide_Original;
using CTaskBtnGroup_IsFlashing_t = BOOL(WINAPI*)(void*);
CTaskBtnGroup_IsFlashing_t CTaskBtnGroup_IsFlashing_Original;
thread_local bool g_attentionPermissionQuery = false;
std::atomic<unsigned long long> g_attentionBlockedReveals{0};
std::atomic<unsigned long long> g_attentionIgnoredFlashing{0};
std::atomic<unsigned long long> g_attentionPassedReveals{0};

BOOL WINAPI CTaskListWnd_PermitAutoHide_Hook(void* self) {
    LeanDockAttention::PermissionScope scope(g_attentionPermissionQuery,
                                            !g_eventStopping.load());
    // Keep every native inhibitor (menus, pointer, drag, focus, etc.).
    return CTaskListWnd_PermitAutoHide_Original(self);
}

BOOL WINAPI CTaskBtnGroup_IsFlashing_Hook(void* self) {
    const BOOL flashing = CTaskBtnGroup_IsFlashing_Original(self);
    if (flashing && g_attentionPermissionQuery && !g_eventStopping.load()) {
        ++g_attentionIgnoredFlashing;
        return FALSE;
    }
    return flashing;
}

bool BlockAttentionReveal(int request) {
    if (!g_eventStopping.load() && request == LeanDockAttention::AttentionRequest) {
        ++g_attentionBlockedReveals;
        return true;
    }
    ++g_attentionPassedReveals;
    return false;
}

void WINAPI TrayUI_Unhide_Hook(void* self, int flags, int request) {
    if (!BlockAttentionReveal(request))
        TrayUI_Unhide_Original(self, flags, request);
}

void WINAPI CSecondaryTray__Unhide_Hook(void* self, int flags, int request) {
    if (!BlockAttentionReveal(request))
        CSecondaryTray__Unhide_Original(self, flags, request);
}
