#pragma once
#include <windows.h>
#include "../core/latest.h"
namespace nhc {
inline constexpr unsigned ActionInputCapacity = 6;
// Windows-logo keys are extended keys (E0-prefixed hardware keys). Include
// the marker on BOTH transitions; a successful SendInput count alone does
// not prove the shell recognized an unmarked logo key.
inline unsigned BuildActionInput(Action action, INPUT (&out)[ActionInputCapacity], bool dismissSwitcher = false) {
    for (auto& input : out) { input = {}; input.type = INPUT_KEYBOARD; }
    if (action == AppSwitcher) {
        if (dismissSwitcher) {
            out[0].ki.wVk = VK_ESCAPE; out[0].ki.wScan = 0x01;
            out[1] = out[0]; out[1].ki.dwFlags = KEYEVENTF_KEYUP;
            return 2;
        }
        // Windows owns the persistent switcher. All three keys are released
        // in the same submission; no modifier is held across user interaction.
        out[0].ki.wVk = VK_LCONTROL; out[0].ki.wScan = 0x1d;
        out[1].ki.wVk = VK_LMENU; out[1].ki.wScan = 0x38;
        out[2].ki.wVk = VK_TAB; out[2].ki.wScan = 0x0f;
        for (unsigned i = 0; i < 3; ++i) {
            out[3+i] = out[2-i]; out[3+i].ki.dwFlags |= KEYEVENTF_KEYUP;
        }
        return 6;
    }
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

// Check the non-modifier key too: an already-held Tab/D must not be released
// by our chord. The caller separately retains mouse/modifier suppression.
template<class IsDown>
inline bool AnyActionKeyHeld(const INPUT (&keys)[ActionInputCapacity], unsigned count, IsDown isDown) {
    if (count > ActionInputCapacity) return true;
    for (unsigned i = 0; i < count; ++i)
        if (!(keys[i].ki.dwFlags & KEYEVENTF_KEYUP) && isDown(keys[i].ki.wVk)) return true;
    return false;
}

// SendInput may insert only a prefix. Release only our accepted key-downs
// that lack an accepted key-up, in reverse order. Never release other keys.
inline unsigned BuildReleaseInput(const INPUT (&keys)[ActionInputCapacity], unsigned count,
                                  unsigned sent, INPUT (&out)[ActionInputCapacity]) {
    for (auto& input : out) input = {};
    if (count > ActionInputCapacity || sent >= count) return 0;
    unsigned n = 0;
    for (unsigned end = sent; end > 0; --end) {
        const INPUT& down = keys[end-1];
        if (down.type != INPUT_KEYBOARD || (down.ki.dwFlags & KEYEVENTF_KEYUP)) continue;
        bool released = false;
        for (unsigned j = end; j < sent; ++j) {
            const INPUT& up = keys[j];
            if (up.type == INPUT_KEYBOARD && (up.ki.dwFlags & KEYEVENTF_KEYUP) &&
                up.ki.wVk == down.ki.wVk && up.ki.wScan == down.ki.wScan &&
                ((up.ki.dwFlags ^ down.ki.dwFlags) & KEYEVENTF_EXTENDEDKEY) == 0) {
                released = true; break;
            }
        }
        if (!released) { out[n] = down; out[n++].ki.dwFlags |= KEYEVENTF_KEYUP; }
    }
    return n;
}
}
