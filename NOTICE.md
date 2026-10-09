# Sources and licensing

LeanDock (legacy NativeTaskbarDock). Distributed with corresponding source under GNU GPL version 3; see LICENSE.txt. This is a dedicated derivative, not an official Windhawk release. Current package and component versions are recorded in build/latest.json and docs/verification-latest.json.

Preserved GPL-3.0 upstream sources by m417z and their credited contributors:

- Windows 11 Taskbar Styler 1.10: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp
- Taskbar height and icon size 1.3.10: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp
- Taskbar auto-hide when maximized 1.2.6: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-auto-hide-when-maximized.wh.cpp
- Taskbar notification icons show all 1.1: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-notification-icons-show-all.wh.cpp
- Taskbar notification icon spacing 1.4 plus the user's locally verified contiguous grid 1.4.2: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-notification-icon-spacing.wh.cpp

The retained build inputs, including authorship, license statements and original metadata, are under upstream. The tray input already includes the locally maintained 1.4.2 grid; it is not an untouched upstream release. Hashes are recorded in build/sources-latest.json. Windhawk SDK interface headers are retained for reviewable adaptation, with a component-local identity and project-owned implementations. No Windhawk engine is linked or needed at runtime. Mature WinRT/XAML lifecycle and restoration code is intentionally retained.

The local controller, pinned-symbol resolver, hook-chain adapter and maintenance scripts combine this functionality for one validated configuration. Theme catalog and statistics timer are removed; network content API is disabled. Styles and settings derive from the existing locally accepted TaskbarDock profile. NextKDE informed earlier spacing and layout choices; this Windows implementation does not run KDE code or components.

MinHook 1.3.4 by Tsuda Kageyu, commit c3fcafdc10146beb5919319d0683e44e3c30d537: https://github.com/TsudaKageyu/minhook . It and bundled HDE source retain all applicable notices in vendor/minhook/LICENSE.txt, AUTHORS.txt and source headers. The import inventory is in tests/imports-latest.json.

The separately installed build toolchain is Clang 20.1.3 with MinGW and WinRT
headers, retained from the Windhawk 1.7.3 compiler bundle. Its original
LICENSE.TXT and component notices remain with that local toolchain. It is not
distributed in this repository or the LeanDock deployment package. This build
provenance does not imply a dependency on the Windhawk application or engine.

The component uses internal Windows taskbar interfaces. Compatibility is deliberately limited to the verified system binaries. This does not establish compatibility with future Windows updates, third-party shell modifications or untested displays.

The 0.7.1 hook-operation shutdown gate follows the lifetime synchronization
pattern in Windhawk's LoadedMod::BeforeUninit, SetFunctionHook and
ApplyHookOperations, reviewed at commit aef1a9f1e77f30c8fd96c5dd29373c38181486bf:
https://github.com/ramensoftware/windhawk/blob/aef1a9f1e77f30c8fd96c5dd29373c38181486bf/src/windhawk/engine/mod.cpp
It is a small adaptation for this pinned, jointly managed backend, not a copy
of the Windhawk engine. The existing stock MinHook 1.3.4 and local chain adapter
remain; Windhawk's extended MinHook and MinHook-Detours were reviewed as possible
future replacements, not imported or represented as tested LeanDock dependencies.

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

The 0.4.0 information slot was designed with reference to NextKDE commit
5cfb6a3a18312c81844ee7b1ca3db85153753e75, inspected on 2026-10-08:
- https://github.com/SuceV587/NextKde/blob/5cfb6a3a18312c81844ee7b1ca3db85153753e75/shell/desktop/modules/dock/DockInfoCarousel.qml
- https://github.com/SuceV587/NextKde/blob/5cfb6a3a18312c81844ee7b1ca3db85153753e75/shell/desktop/modules/dock/DockContainer.qml

The fixed shared slot, manual switching and crowded-layout collapse informed
the design. The Windows implementation is new C++/WinRT code; it does not copy
or depend on QML, Qt, Quickshell, MPRIS or a separate hardware-monitor process.
Windows API references:
- https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes
- https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex
- https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager

The 0.5.0 battery card and scoped context actions are original C++/WinRT code.
Battery readings use GetSystemPowerStatus, with PowerManager change events:
- https://learn.microsoft.com/en-us/uwp/api/windows.system.power.powermanager
The package adds no third-party power monitor, elevated message bridge or
background process. Existing native notification objects and menus remain.

The 0.5.1 pointer-target mapping is original code. Its post-show HWND placement,
and the later 0.5.2 HWND position constraint, were withdrawn after user reports
of flashing and nonfunctional item clicks. A private-COM activation experiment
also failed desktop acceptance and is not included here.

The 0.5.4 implementation substitutes a XAML anchor at the actual native menu
entry, preserving native rendering, input, command handling and submenus. Its
entry and x64 calling convention were checked against the matching Microsoft
public symbols for the already-pinned Taskbar.View.dll. No Microsoft binary or
PDB is redistributed; menu-native-symbol-latest.json records the verification
metadata. The original Quick Settings object and working Win+X invocation
remain. No menu-position polling or additional runtime process was added.

The 0.5.5 Quick Settings companion is original Win32 code. The distinction
between pre-commit positioning and asynchronous post-show moves was checked
against m417z's Shell Flyout Positions 1.3 (GPL-3.0), inspected on 2026-10-08:
https://github.com/ramensoftware/windhawk-mods/blob/main/mods/shell-flyout-positions.wh.cpp
LeanDock uses a scoped window subclass rather than that mod's SetWindowPos
detour. It adds no Windhawk runtime and copies no Windows binary or symbol file.
Documented APIs used for thread ownership and the placement transaction:
- https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging
- https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-setwindowsubclass
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw

The 0.5.6 timing changes are original adaptations: shared native pending placement, versioned scalar geometry, media command generations and bounded event-thread shutdown. No additional upstream code or runtime dependency was introduced.

The 0.7.2 battery readiness change reuses the existing native ControlCenterButton
and the retained tray adapter's reversible dependency-property mechanism.
The local zero-flow fallback is also retained while inactive, with hit testing,
Tab navigation and the accessibility control view disabled until activation.
The shared card slot supplies already-measured geometry. This is a local timing
adaptation, not newly copied upstream implementation. The panel uses the same
pre-commit placement helper as before, with its Dock anchor changed to center.
There is no new polling, process, input simulation or post-show repositioning.

The 0.6.0 theme worker's IThemeManager ABI and appearance registry values were
adapted with reference to tinodin's Auto Theme Switcher 1.3.2 (GPL-3.0):
https://github.com/ramensoftware/windhawk-mods/blob/a5c9b87ba0824d826d6c1a95f6a497cb48d2dc66/mods/auto-theme-switcher.wh.cpp
The original reference is retained as upstream/theme-switch.cpp, SHA-256
421AC8B0EEF2C7DC6E65D374D4C5712113671D7E974E42B593DAEAA30785609B.
LeanDock adds a pinned system implementation, exact built-in theme allowlist,
isolated short-lived worker, same-session mutex, visible-window notification
budget, conditional appearance rollback, and complete-state verification.
It does not use that mod's scheduler, location lookup or Windhawk engine.

The quick-theme companion uses the XAML diagnostics IObjectWithSite pattern
already preserved in upstream/styler.cpp, originally derived from TranslucentTB
ExplorerTAP and adapted by m417z. The minimal pattern was also checked against:
https://gist.github.com/m417z/8741e52d8eaad67b47ee365a20070bf8
Its discovery subscription is temporary. Diagnostic object handles are released
before normal operation continues with the target UI thread's local events.
Only the BlueLightReduction tile presentation is replaced; the native data
model and original template/binding remain available for restoration.
Windows theme file format:
https://learn.microsoft.com/en-us/windows/win32/controls/themesfileformat-overview
