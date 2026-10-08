# Sources and licensing

LeanDock 0.3.3 (legacy NativeTaskbarDock controller 0.3.1, backend 0.2.2 and Start layout 0.1.1). Distributed with corresponding source under GNU GPL version 3; see LICENSE.txt. This is a dedicated derivative, not an official Windhawk release.

Preserved GPL-3.0 upstream sources by m417z and their credited contributors:

- Windows 11 Taskbar Styler 1.10: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp
- Taskbar height and icon size 1.3.10: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp
- Taskbar auto-hide when maximized 1.2.6: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-auto-hide-when-maximized.wh.cpp
- Taskbar notification icons show all 1.1: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-notification-icons-show-all.wh.cpp
- Taskbar notification icon spacing 1.4 plus the user's locally verified contiguous grid 1.4.2: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-notification-icon-spacing.wh.cpp

The retained build inputs, including authorship, license statements and original metadata, are under upstream. The tray input already includes the locally maintained 1.4.2 grid; it is not an untouched upstream release. Hashes are recorded in build/sources-latest.json. Windhawk SDK interface headers are retained for reviewable adaptation, with a component-local identity and project-owned implementations. No Windhawk engine is linked or needed at runtime. Mature WinRT/XAML lifecycle and restoration code is intentionally retained.

The local controller, pinned-symbol resolver, hook-chain adapter and maintenance scripts combine this functionality for one validated configuration. Theme catalog and statistics timer are removed; network content API is disabled. Styles and settings derive from the existing locally accepted TaskbarDock profile. NextKDE informed earlier spacing and layout choices; this Windows implementation does not run KDE code or components.

MinHook 1.3.4 by Tsuda Kageyu, commit c3fcafdc10146beb5919319d0683e44e3c30d537: https://github.com/TsudaKageyu/minhook . It and bundled HDE source retain all applicable notices in vendor/minhook/LICENSE.txt, AUTHORS.txt and source headers. The import inventory is in tests/imports-latest.json.

The component uses internal Windows taskbar interfaces. Compatibility is deliberately limited to the verified system binaries. This does not establish compatibility with future Windows updates, third-party shell modifications or untested displays.

NativeHotCorners 1.0.2 is the user's existing locally written C++/Win32 implementation. Its original source, tests and notices are retained under upstream/NativeHotCorners. This integration reuses the existing core and SendInput actions; build/adapt-hotcorners-latest.py adapts the window entry point to the controller's message loop and fixes portable wide-string formatting. No additional third-party hot-corner runtime or copied KDE/GNOME source was added.

Behavioral references inspected on 2026-10-07:
- KDE KWin screen-edge state, movement reset, and fullscreen blocking: https://github.com/KDE/kwin/blob/master/src/screenedge.cpp
- GNOME Shell HotCorner / PressureBarrier behavior: https://github.com/GNOME/gnome-shell/blob/main/js/ui/layout.js
- HotCornersWin feature and polling comparison: https://github.com/flexits/HotCornersWin

The user's explicit zero dwell/cooldown and 32-pixel rearm preferences override those projects' different default timing or pressure policies. References were used for comparison, not to claim feature parity or shared upstream authorship.

Windows input and integrity reference: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput . The deployed elevated controller was required by the user's existing elevated foreground applications. Maintenance uses only four narrowly defined window messages across integrity levels; no global UIPI changes or arbitrary input/command endpoint is added.

Repository-specific notes:
- New repository-owned source and adaptations are distributed under GPL-3.0; upstream notices remain in force.
- The preserved NativeHotCorners notice describes its earlier standalone distribution. This combined repository selects GPL-3.0 for its owner-written contributions; no third-party hot-corner source was copied.
- SDK interface headers come from Windhawk; their GPL-3.0 license is retained in vendor/windhawk-sdk/LICENSE.txt.
- Personal paths, deployment receipts, backups, binary payloads and original one-machine migration scripts are excluded. Source behavior is retained; the repository manager exposes only routine controls and refuses another deployment's task path.

NextKDE design references reviewed at commit f9154f4c3603204614925e54d329b39de5987374:
- https://github.com/SuceV587/NextKde/blob/f9154f4c3603204614925e54d329b39de5987374/shell/desktop/modules/common/LiquidGlassSurface.qml
- https://github.com/SuceV587/NextKde/blob/f9154f4c3603204614925e54d329b39de5987374/shell/desktop/modules/dock/DockDivider.qml
- https://github.com/SuceV587/NextKde/blob/f9154f4c3603204614925e54d329b39de5987374/shell/desktop/modules/dock/DockRevealGeometry.mjs
These guided reflection, separator proportions and projected edge geometry. The
KWin Glass/Snell shader was inspected but not copied or linked; Windows rendering
retains the existing backdrop brush and XAML composition. This is a glass finish,
not a claim of physically refractive rendering or full macOS feature parity.

The original NativeHotCorners input is retained unchanged. The current local
core adds LockScreen and routes it to the Windows LockWorkStation API through
the generator; it deliberately generates no synthetic Win+L keystrokes.

The native Start grid flattening, restoration and semantic-zoom handling were
adapted with reference to Asteski's Compact Start Menu 1.0.0 at windhawk-mods
commit 5b32bb24425ef9af3e7769665f667b8144352a20:
https://github.com/ramensoftware/windhawk-mods/blob/5b32bb24425ef9af3e7769665f667b8144352a20/mods/compact-start-menu.wh.cpp
The full reference is retained as upstream/start-layout.cpp. LeanDock's small
companion retains a native CollectionViewSource.View, preserves native models,
uses collection/window events instead of the upstream traversal timer, and
limits changes to the grid and its view selector. No Windhawk runtime is used.
