# Quick Settings theme switch

## 0.8.24 completion broadcast

The 0.8.23 taskbar-only repair below FAILED sustained user acceptance. The
user first reported recovery, then immediately corrected it: the Dock turned
dark again. A fresh native read at 15:51 on 2026-10-12 confirmed light app and
system registry values with a visible Dark XAML layer and B3252C38 material.
The quick-theme companion reported only the two requested switches and no
active worker. This was not another requested switch to Dark.

A complete WM_SETTINGCHANGE / ImmersiveColorSet broadcast, without changing
settings or restarting anything, restored Light / 9EC8D1DE. All 155 passive
samples over the next 40 seconds remained correct while the Dock was visible;
the broadcast took 147 ms and did not change the foreground. Explorer has
hidden non-taskbar listeners omitted by both the fast pass and 0.8.23's scoped
notification. The exact internal listener/cache that reverted the XAML theme
was not traced, so do not present that inferred mechanism as proven.

The correction restores Auto Theme Switcher's full native broadcast contract
AFTER full theme verification and after waiting for the fast notification thread
(the existing join is bounded to 1000 ms).
The fast pass still provides early feedback. The final HWND_BROADCAST has no
visibility filter or traversal budget. SMTO_ABORTIFHUNG / SMTO_ERRORONEXIT and
100 ms apply per receiver; a slow/hung receiver does not deliberately exclude
later windows. Conditional failure rollback also broadcasts the restored mode.
No timer, resident monitor, forced RequestedTheme or palette override was added.
Worker success verifies settings; it does not certify every receiver or repaint.

Validation: 22 native notification checks run on a private, never displayed
test desktop, including real broadcast delivery to a hidden non-taskbar
listener, repeats and destruction. The fixture never switches the input
desktop or notifies user apps. Quick lifecycle regression passed 22 checks.
Three real dark/light round trips produced 456 native observations, with zero
post-completion mismatches; the final light state was observed for 20 seconds.
All those round-trip samples were of a hidden host; the independent 40-second
broadcast recovery observation covered the visible host. Native first matching
state was sampled at 123-215 ms; worker completion at 777-1112 ms. These are
sampled state timings, not pixel/frame timings or guaranteed performance.
After deployment, the user exercised the real Quick Settings tile (12 recorded
requests, no worker failures), changed windows and revealed/hid the Dock, then
confirmed "没有再变黑". A 90-second read-only observation captured 335 samples
covering both host visibility states with no mismatch after the system mode had
been stable for one second. Short mismatches during the actual transition were
recorded and are not counted as permanent success/failure evidence. A final
read confirmed system/app Light, XAML Light and material 9EC8D1DE, without
injecting a repair notification. This is current-session acceptance, not a
claim of all-session or all-machine stability. Physical secondary displays,
lock/sleep and a hung shell remain untested. See verification-latest.json.

## 0.8.23 hidden taskbar refresh (superseded after recurrence)

On 2026-10-12, after the user's dark-to-light switch, both appearance values
were 1 and CurrentTheme was aero.theme, while the live Dock XAML ActualTheme
was Dark and its material remained B3252C38. Sending one scoped native
WM_SETTINGCHANGE / ImmersiveColorSet notification changed the same live layer
to Light / 9EC8D1DE, with the foreground unchanged and without reloading it.

The former responsiveness optimization notified only visible non-cloaked
windows within 75 ms. It did not guarantee delivery to taskbars. The native
theme manager's exact reason for omitting a refresh after pre-applied registry
values was not independently traced; the observed failure is stale shell theme
state, not a wrong material resource value.

The short-lived worker now notifies shell-owned Shell_TrayWnd and
Shell_SecondaryTrayWnd windows regardless of visibility, separately from the
visible-app budget. It repeats this native notification after full theme,
appearance and wallpaper verification. Each send has a 100 ms timeout and
aborts for a hung/destroyed receiver; there is no permanent polling, extra
resident thread, backdrop repaint loop, private API or hard-coded theme on
Dock elements. Original rollback and early visible-app updates remain.

This extends the existing Auto Theme Switcher-derived registry/notification
path; see NOTICE.md. The notification is a scoped retry of the native message,
not a replacement for the full theme manager or a system-wide repaint.

Validation: tests/theme-notify-latest.ps1 passed 16 actual native-message
checks against test-owned invisible windows, including both shell classes,
wrong/unknown ownership, repeat sends, destroyed windows and untouched focus.
Quick lifecycle regression passed 22 checks. The new worker completed two
real dark/light round trips while the live Dock host was hidden: all four
observations matched native theme and material color, with successful worker
exits and 629–699 ms completion times. The original light mode was restored.
These are native-state checks; they do not establish pixel-level acceptance
or test physical secondary displays, lock/sleep or a busy/hung Explorer.
Deployment and user feedback remain separately recorded in verification-latest.json.

## Original implementation

LeanDock 0.6.0 uses controller 0.3.4 and panel companion 0.2.0. The Start binary is unchanged from 0.5.12. Explorer 0.4.12 separately adds the clock card and removes the gripper layout gap.

### Behavior

The `Microsoft.QuickAction.BlueLightReduction` tile becomes **深色主题**.
Checked means that both Windows and apps use dark mode. A click requests the
opposite built-in full theme:

- Dark: Windows Resources/Themes/dark.theme, Windows wallpaper img19.jpg.
- Light: Windows Resources/Themes/aero.theme, Windows wallpaper img0.jpg.

The user explicitly selected full themes, including wallpaper. This therefore
also applies the other settings contained in those two system theme files.
It does not turn Night Light on or off. The Night Light page in Windows Settings
remains available, and any schedule or color temperature remains system-owned.

The replacement is a new ToggleButton and label in the existing tile slot.
It does not reuse or invoke the original Night Light button's events. The
model is untouched; stop restores the original ContentTemplate or its binding,
only if the companion still owns the current template value.

### Lifecycle and boundaries

1. The existing controller checks ShellHost/ControlCenter and XAML identities.
2. A one-shot diagnostics subscription discovers ControlCenterRegion on its
   owning UI thread, then unsubscribes and releases all collected handles.
3. Activation and position events retry discovery when the cold host has not
   constructed its XAML island yet. There is no polling timer.
4. Local layout events only traverse when the tile is missing or recreated.
   Installed templates take a constant-time fast path.
5. A click starts the same executable with exactly `--theme dark` or
   `--theme light`, using ShellHost's token. This does not elevate ShellHost or
   add a command message to the elevated controller.
6. The theme worker validates the system theme library, takes a session mutex,
   and calls the native theme manager. It exits after verification. Repeated
   clicks cannot queue conflicting theme applications.

Changing a Windows component invalidates the existing compatibility pins.
The theme manager interface is private and independently pinned to themeui.dll
10.0.26100.8875, SHA-256
2AA99876194E8EE59D806A92DD740348279B6F1B704B4AE5A4E6BDB89552CD51.
Do not remove a pin merely to make an updated Windows installation run.

The first accepted implementation switched correctly, but the user reported
a pause before Windows and apps began changing color. Local timing placed the
main delay inside ApplyTheme (roughly 0.5–0.7 seconds), rather than COM setup
(16–31 ms in those samples). The revised worker writes the actual appearance
settings before full application. A temporary thread notifies visible,
non-cloaked windows with a 20 ms per-window timeout and a 75 ms traversal budget,
in parallel with full application. Since 0.8.24, an explicit full broadcast
after verified completion includes background listeners; relying on the theme
manager's notifications plus a taskbar-only refresh was insufficient. This
prioritizes first response; it does not claim that
wallpaper decoding or every app's repaint completes in one frame.

If theme application fails before another theme replaces the original, the
worker conditionally restores its own pre-applied appearance values. It never
overwrites a theme selected elsewhere in the meantime. Unsupported or unknown
states are errors, not a reported successful switch.

### Maintenance

`latest.ps1 -Action Status` includes QuickTheme only when the current controller,
exact ShellHost path, loaded companion and owner PID agree. A stale status file
alone is not liveness or visual acceptance. `tileInstalled` and `buttonReady`
describe attachment; `switches`, `lastExit` and `failures` describe requests.
They do not replace the user's visible theme/wallpaper confirmation.

Build with `build/latest.ps1 -ControllerOnly -SkipGenerate` when unrelated
generated hot-corner work must remain untouched. The panel DLL is pinned until
ShellHost exits; replace it only after stopping the controller and checking the
exact ShellHost path/session. Use a backed-up deployment with restoration of
the original files. Explorer's diagnostics may fail to reconnect in place;
reload only the verified taskbar-owning Explorer process when needed. Never
stop every explorer.exe or latest.exe by filename.

Current machine checks and user acceptance are recorded in
verification-latest.json; the predecessor is verification-v0.5.12.json.
Real login, multi-monitor, theme transitions during lock/sleep, and long-term
operation have not been newly exercised.
