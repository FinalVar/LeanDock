#pragma once

namespace LeanDockAttention {
// TrayCommon::UnhideRequest for app attention, as used by the referenced
// Taskbar Auto-Hide Motion mod. Only the hash-pinned taskbar is supported.
inline constexpr int AttentionRequest = 17;

// Nested native permission checks must restore the previous scope. The scope
// belongs to a thread, so drawing on another thread retains the real state.
class PermissionScope {
    bool& current;
    const bool previous;
public:
    explicit PermissionScope(bool& state, bool enable)
        : current(state), previous(state) { current = state || enable; }
    ~PermissionScope() { current = previous; }
    PermissionScope(const PermissionScope&) = delete;
    PermissionScope& operator=(const PermissionScope&) = delete;
};
}
