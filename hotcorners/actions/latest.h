#pragma once
#include <windows.h>
#include "../core/latest.h"
namespace nhc {
// Windows-logo keys are extended keys (E0-prefixed hardware keys). Include
// the marker on BOTH transitions; a successful SendInput count alone does
// not prove the shell recognized an unmarked logo key.
inline unsigned BuildActionInput(Action action, INPUT (&out)[4]) {
    for (auto& input : out) { input = {}; input.type = INPUT_KEYBOARD; }
    // Locking uses LockWorkStation, never a simulated Win+L sequence.
    if (action != StartMenu && action != TaskView && action != ShowDesktop) return 0;
    out[0].ki.wVk = VK_LWIN;
    out[0].ki.wScan = 0x5b;
    out[0].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    if (action == StartMenu) {
        out[1] = out[0]; out[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        return 2;
    }
    out[1].ki.wVk = action == TaskView ? VK_TAB : 'D';
    out[1].ki.wScan = action == TaskView ? 0x0f : 0x20;
    out[2] = out[1]; out[2].ki.dwFlags = KEYEVENTF_KEYUP;
    out[3] = out[0]; out[3].ki.dwFlags |= KEYEVENTF_KEYUP;
    return 4;
}
}
