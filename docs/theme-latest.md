# Quick Settings theme switch

LeanDock 0.6.0 uses controller 0.3.4 and panel companion 0.2.0. The Start binary is unchanged from 0.5.12. Explorer 0.4.12 separately adds the clock card and removes the gripper layout gap.

## Behavior

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

## Lifecycle and boundaries

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
in parallel with full application. Background windows receive Windows' normal
theme notifications. This prioritizes first response; it does not claim that
wallpaper decoding or every app's repaint completes in one frame.

If theme application fails before another theme replaces the original, the
worker conditionally restores its own pre-applied appearance values. It never
overwrites a theme selected elsewhere in the meantime. Unsupported or unknown
states are errors, not a reported successful switch.

## Maintenance

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
