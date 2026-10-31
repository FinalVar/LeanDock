# Sources and licensing

LeanDock (legacy NativeTaskbarDock). Distributed with corresponding source under GNU GPL version 3; see LICENSE.txt. This is a dedicated derivative, not an official Windhawk release. Current package and component versions are recorded in build/latest.json and docs/verification-latest.json.

Version 0.8.78 narrows only locally owned dashboard layout: a two-row battery
module and a shared media-control row. It reuses the original battery geometry,
native quick-settings mapping, media handlers and unload ownership. The retained
m417z Styler binding/value restoration and current LeanDock cleanup were reviewed;
no new third-party code, background task, input hook or external runtime is added.
The invisible XAML fixture loads the same battery markup and media layout helper
as production, and checks text, fill and distinct control rectangles.

The 0.8.77 dashboard keeps LeanDock's native XAML, media session, network,
power mapping and lifecycle implementations. The dashboard layout, independent
deadline scheduling and bounded app compression are local adaptation. The
existing m417z taskbar size implementation (upstream/size.cpp, version 1.3.10)
and LeanDock's binding-preserving tray property owner were reviewed; no second
launcher, replacement input stack or new third-party runtime was imported.
High-resolution icon sources and optical compensation are retained. Historical
carousel references below describe earlier design work, not the current runtime.

The 0.8.76 AppBar query/cache adapter is original LeanDock integration. It retains
m417z's native hide/reveal implementation and the existing event/MinHook
lifecycle; it does not copy Windows implementation code. Microsoft public
symbols, static analysis of the exact installed Explorer/shell32 and a hidden private
desktop test establish the version-bound call and wire contract. Tao 0.37.1
(Apache-2.0) and Chromium's Windows autohide queries (BSD) were inspected to
understand client behavior; neither runtime is vendored or patched. The Tao
geometry fixture is a local test of the documented observed rule, not a real
application acceptance result. The public NonRudeHWND contract remains the
only application-side state adaptation. See docs/appbar-compatibility-latest.md.

The 0.8.74 bottom-edge recovery keeps the existing public NonRudeHWND /
ITaskbarList2 adapter and reuses LeanDock's Start AttachPolicy (three bounded
readiness attempts, then event-driven renewal). The retained m417z auto-hide
1.2.6 native reveal/hide implementation was reviewed and is unchanged.
Generic resize-border admission and readiness reporting are local adaptation;
no new upstream implementation, polling engine or input hook was imported.
Microsoft contracts consulted:
https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow
https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist-hrinit
https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect

The launch-maximization feature used in 0.8.41-0.8.57 was removed in 0.8.58
at the user's request. It is no longer compiled or run. The corresponding
Microsoft PowerToys FancyZonesWindowProcessing source, MIT license and
revision 471db8bf9ceb644d6fc4149e549e795957b10d30 remain in
upstream/window-launch as historical attribution and recovery reference.
The removed feature also adapted GetWindowAppId from m417z's GPL-3.0
Taskbar auto-hide when maximized 1.2.6 (upstream/hide.cpp). That original
source remains required by the independent native taskbar hiding module.
Since 0.8.93 the adapted module stops its z-ordered occlusion scan at the
desktop window and re-runs the decision on desktop foreground transitions,
so windows left under "show desktop" no longer count as covering the Dock.
Center New Windows 1.0.1, crazyboyybs, MIT, revision
a8b6141258658b1fbae949d1902f4ae76e55c0fc, was design reference only;
its injection runtime was never imported. See docs/window-launch-latest.md
for removal scope and rollback evidence.

The 0.8.43 keyboard Win+X placement reuses the existing native menu entry,
XAML anchor and upstream Styler per-root lifecycle. The card target and
keyboard/pointer focus separation are local adaptations; no additional
upstream implementation, hotkey registration or input hook was imported.

The 0.8.35 context-menu policy is a local adaptation of the retained XAML
event/lifecycle integration. It preserves native application/tray menus and
the existing Win+X anchor; only card right-clicks request Win+X, and blank
right-clicks are consumed. Native TaskListButton template names were checked
against the retained Taskbar Styler source. Battery handler order was checked
against the pinned SystemTray.dll and matching Microsoft public PDB; no native
implementation was copied. Static evidence and runtime limits are recorded in
docs/cards-latest.md.

The 0.8.31 card-input adapter adapts InputSite window discovery and function
hook placement from Taskbar Volume Control 1.3.1 by m417z (GPL-3.0), commit
131f90d6611b96811af101f4b8177dd24e40ed13, retrieved 2026-10-17:
https://github.com/ramensoftware/windhawk-mods/blob/131f90d6611b96811af101f4b8177dd24e40ed13/mods/taskbar-volume-control.wh.cpp
The complete reference source and GPL license are retained in
upstream/cards-input. Source SHA256:
8101289C27E08FC6748A44814762C12E4DDB1D01E4D771E3F13F5F22943619F7.
The mod explains why InputSite must not be subclassed. Local adaptation uses
the existing LeanDock shared hook runtime and validates the actual InputHost
module path/hash/PE identity. Card bounds, DM_POINTERHITTEST admission,
default touchpad recognition, horizontal wheel handling and Board lifetime
are local additions. No volume behavior, SendInput, focus changes, or shell
registration changes were imported. The ineffective 0.8.30 WH_GETMESSAGE
attempt has been removed. Contract and real-hardware limits are documented
in docs/cards-input-latest.md.
The 0.8.21 attention-only hide policy adapts the scoped `IsFlashing` query
and attention `UnhideRequest` filtering in Taskbar Auto-Hide Motion 1.0.8,
by jinSeong-P, GPL-3.0, commit 69eaa5c818311715ba1c433c6c3372f642c03022:
https://github.com/jinSeong-P/windhawk-floating-dock/blob/69eaa5c818311715ba1c433c6c3372f642c03022/mods/taskbar-autohide-motion.wh.cpp
The complete reference source and its license are retained under
upstream/attention. This is a small source adaptation, not an import of that
project's animation, edge polling, notification timers or runtime. LeanDock
omits attention reveals altogether, scopes the flashing override to the
native CTaskListWnd::PermitAutoHide query shared by primary/secondary lists,
uses a thread-local RAII scope for reentrancy, and passes through on Stop.
The button's flashing state is never cleared or overridden for drawing.
Cirn09's Better Taskbar Autohide 1.2 was also reviewed for PermitAutoHide;
its unconditional permission override and hard-coded registered message
number were not adopted.

The 0.8.22 attention-dot styling extends the existing Taskbar Styler 1.10
CommonStates / RunningIndicatorStates rules. The six attention-state names
and selectors were checked against the retained Plasma theme (Sand216) in
upstream/styler.cpp and its published styling guide. Local differences are
transparent attention backgrounds, the existing neutral hover/press feedback,
and a 4 DIP native running indicator. Native attention colors, timing and
clearing remain untouched; no notification monitor or drawing hook is added.

Preserved GPL-3.0 upstream sources by m417z and their credited contributors:

- Windows 11 Taskbar Styler 1.10: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp
- Taskbar height and icon size 1.3.10: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp
- Taskbar auto-hide when maximized 1.2.6: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-auto-hide-when-maximized.wh.cpp
- Taskbar notification icons show all 1.1: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-notification-icons-show-all.wh.cpp
- Taskbar notification icon spacing 1.4 plus the user's locally verified contiguous grid 1.4.2: https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-notification-icon-spacing.wh.cpp

The retained build inputs, including authorship, license statements and original metadata, are under upstream. The tray input already includes the locally maintained 1.4.2 grid; it is not an untouched upstream release. Hashes are recorded in build/sources-latest.json. Windhawk SDK interface headers are retained for reviewable adaptation, with a component-local identity and project-owned implementations. No Windhawk engine is linked or needed at runtime. Mature WinRT/XAML lifecycle and restoration code is intentionally retained.

The 0.8.17 mixed-system-tray adaptation extends that existing grid and its
binding-aware property restoration to MainStack. Native icons, models and
event routes remain owned by Windows. No external permission monitor, new
privacy detection implementation, replacement tray host or status artwork was
imported. The system-specific adapter is local GPL-3.0 code.

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

The 0.7.4 native-content readiness guard was checked against this machine's
Microsoft public SystemTray.pdb, GUID/age and binary hash recorded in
docs/power-input-native-latest.json. OmniButton::HandleTapped forwards to an
existing descendant IconView before marking the gesture handled. No Microsoft
code, binary or PDB is redistributed, and no new native-function hook is added.
The existing reversible tray adapter and Invoke/shortcut fallback are retained.
The local host-lifetime policy now distinguishes an explicitly zero exit code
from abnormal/unknown exits and bounds clean recovery attempts; it adds no timer
or process watcher and uses the existing shell-window discovery events.

The 0.6.0 theme worker's IThemeManager ABI and appearance registry values were
adapted with reference to tinodin's Auto Theme Switcher 1.3.2 (GPL-3.0):
https://github.com/ramensoftware/windhawk-mods/blob/a5c9b87ba0824d826d6c1a95f6a497cb48d2dc66/mods/auto-theme-switcher.wh.cpp
The original reference is retained as upstream/theme-switch.cpp, SHA-256
421AC8B0EEF2C7DC6E65D374D4C5712113671D7E974E42B593DAEAA30785609B.
LeanDock adds a pinned system implementation, exact built-in theme allowlist,
isolated short-lived worker, same-session mutex, visible-window notification
budget, conditional appearance rollback, and complete-state verification.
It does not use that mod's scheduler, location lookup or Windhawk engine.

The 0.8.23 taskbar-only notification attempt subsequently recurred. In 0.8.24,
completion and conditional rollback restore the upstream HWND_BROADCAST /
ImmersiveColorSet contract, including hidden non-taskbar listeners. The local
adaptation adds SMTO_ERRORONEXIT, keeps the 100 ms per-window timeout, and orders
completion after the parallel fast pass. The visible-app 75 ms fast path,
early shell-owned taskbar notification and theme-manager hash pin remain.

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

0.7.5 诊断复用匹配系统 PDB 中已核验的 ViewCoordinator::IsExpanded 只读入口。继续使用组件哈希校验，在所属 UI 线程查询；不使用对象字段偏移，不新增原生挂钩。符号来源及 RVA 见同一输入诊断来源记录。

0.7.6 further inspects OmniButton::HandleLoaded and the native IconView input
path in the same pinned SystemTray module. The earlier ForwardClickToIconView
inspection alone did not establish that ControlCenterButton invokes it.
The local fix reuses DockNormalizeIcon and reversible dependency-property
storage to expand the actual visible IconView, preserving its native parent,
model and event handlers. No Windows code, binary or PDB is distributed.
Topmost hit-testing and handled-event observation follow the documented
VisualTreeHelper.FindElementsInHostCoordinates and UIElement.AddHandler APIs.
This is local adaptation, not newly copied upstream code.

The 0.8.0 ordered single-card preferences refer to the already retained NextKDE
commit 5cfb6a3a18312c81844ee7b1ca3db85153753e75, DockInfoCarousel.qml and
DockConfigService.qml (SuceV587/NextKde, GPL-3.0). Its enabled-page projection
and separation of runtime preferences informed this design. This is behavioral
reference, not direct copying of QML into the Windows implementation.
The local settings window reuses Windows common controls; network sampling uses
Microsoft IP Helper counters, with local filtering, lifetime and reset guards:
https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2
https://learn.microsoft.com/en-us/windows/win32/api/netioapi/ns-netioapi-mib_if_row2
No TrafficMonitor, network driver, polling service or third-party runtime is added.

The 0.8.18 network aggregation change was checked against zhongyang219's
TrafficMonitor at commit 930f17533d6098989ebad62f210aa97d75ef174b (Anti 996
License v1.0 Draft): AdapterCommon.cpp and TrafficMonitorDlg.cpp, especially
the all-connections sum, elapsed-time conversion and connection-change reset.
https://github.com/zhongyang219/TrafficMonitor/tree/930f17533d6098989ebad62f210aa97d75ef174b
This is a behavioral/source review, not copied implementation code. LeanDock
retains its existing 64-bit IP Helper sampler, per-LUID baseline and background
lifetime guards. The local adaptation includes active virtual wireless ports
with NDIS wireless physical-medium metadata, while excluding Ethernet-shaped
virtual adapters, tunnels, endpoints and filters. RX/TX link sums intentionally
include both sides of hotspot forwarding; no packet deduplication is claimed.

### 0.8.7 material and theme palette

The glass contrast adjustment reuses the existing upstream WindhawkBlur composition effect and theme-color resolution without replacing their implementation. Light/Dark/HighContrast card resources and gradient dividers use Windows XAML theme resources. The palette is a local design adjustment, not copied NextKDE rendering code.

### 0.8.8 backdrop resume adaptation

The retained upstream XamlBlurBrush remains the effect implementation. The local generator adds weak brush tracking, cancellable UI-thread timers, and a bounded reconnect of the existing effect's backdrop source after observed desktop/expansion transitions. The hide worker's existing WinEvent lifetime is reused; no new worker or permanent refresh timer is introduced. Discussion in https://github.com/ramensoftware/windhawk-mods/issues/2869 informed the investigation but is not a copied complete fix. See docs/backdrop-resume-latest.md for unresolved compositor-cause and visual-acceptance boundaries.

0.8.9 additionally prepares that same backdrop source immediately before the retained native Unhide calls, using a same-thread-only synchronous message. This is a local lifecycle adaptation; it neither replaces native window animation nor adds new private Windows interfaces. The upstream implementation and license attribution above remain applicable.

### 0.8.10 independent material pigment

The retained WindhawkBlur still performs blur and saturation. Its tint and
luminosity parameters are now zero; a normal XAML Rectangle supplies the themed
pigment separately. This layer and its weak ownership/template guards are local
Windows adaptation. NextKDE's already retained LiquidGlassSurface.qml and
DockThemeService.qml at f9154f4c3603204614925e54d329b39de5987374 (SuceV587,
GPL-3.0) were reviewed for separating compositor blur, stable material body and
foreground contrast. No QML code or KWin renderer was copied or added as a
runtime dependency. The existing source attribution above remains applicable.

### 四指任务栏缩略图适配（0.8.14）

参考 ADeltaX 的 DWM Thumbnail/VirtualDesktop 示例的私有 API/标志定义，以及 ALTaleX531/flip3d_comp（commit 6109667ab3cf88c4354b71985660432fcc9c6ff2）的 ENABLE3D + DISABLEFORCECVI 组合。仅核对接口与行为，不复制项目源文件；适配器独立编写。函数签名、标志指令和模块 SHA256 另外由本机微软公开符号交叉核验。参考链接：https://gist.github.com/ADeltaX/aea6aac248604d0cb7d423a61b06e247 ，https://github.com/ALTaleX531/flip3d_comp/tree/6109667ab3cf88c4354b71985660432fcc9c6ff2 。这两份参考均不构成本问题已经解决的证据。

### 手势任务栏实时副本适配（0.8.15）

0.8.14 的 ENABLE3D/DISABLEFORCECVI 组合实测无效，现已移除；上节作为历史来源记录保留。0.8.15 依据本机已固定哈希的 twinui.pcshell.dll、twinui.dll 及匹配微软公开 PDB，核对 VirtualDesktopGestureWindow::Initialize、CDCompWindowThumbnail::Freeze、GetSourceWindow 的实际调用和接口一致性。精确符号及 PDB 来源见 docs/gesture-thumbnail-symbols-latest.json。候选适配独立编写，只跳过真实手势初始化内已识别 Dock 副本的早期冻结；没有复制微软实现，也没有宣称其他成熟项目已解决本故障。独立对照工具的私有 DWM 标志定义参考上一节 ADeltaX 资料，未作为常驻运行依赖。

### 无效手势实验撤回（0.8.16）

0.8.15 的 Freeze 跳过已被实际四指手势否定。0.8.16 不再编入或安装该适配器；上面的来源和实验源码仅作历史复核材料，不能当作可用修复或当前运行依赖。底层成熟 Mod 的作者、许可和已验收恢复逻辑保持不变。

### Maximized client edge compatibility (0.8.19 source, not yet deployed)

The NonRudeHWND / ITaskbarList2::MarkFullscreenWindow contract comes from
Microsoft's public API documentation:
https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow
The same application symptom and per-window workaround were reviewed in
https://github.com/openai/codex/issues/45369 . The local event/ownership adapter
is independently written; no OpenAI or Microsoft implementation was copied.
Taskbar Z-Order Override by meteoni, v0.8.1, was reviewed at
https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-z-order-override.wh.cpp
for taskbar lifecycle and z-order behavior. Its global always-on-top policy,
SetWindowPos/SetWindowBand hooks and implementation are not copied or enabled.
LeanDock instead limits the documented shell correction to the verified
OpenAI.Codex package's resizable maximized ChatGPT window. The WinEvent lifetime,
property ownership guards and fixtures are local adaptation; runtime acceptance
of this new controller is pending application-control admission.

### Shifted Dock clip ownership (0.8.19 source, not yet deployed)

The implementation continues to reuse the retained Windows 11 Taskbar Styler
region calculation, layout callbacks, window subclass and cleanup. The local
generator adds a narrow ownership predicate for an observed upward translation
of the last full-height simple rectangle, preserving the upstream native fold
and foreign-region protections. This predicate and its compile-time fixtures
are local adaptation, not an upstream fix. The retained upstream source hash is
88d2d43768a2e8d92522f9c1615bcd2b6c209dd32e1610426738e4c5af40d592.
The original copyright, license and version attribution above remains valid.
Current-window recovery and pending source deployment are documented separately
in docs/clip-recovery-latest.md.

### Start host retirement recovery (0.8.20 source, not yet deployed)

Start now reuses LeanDock's existing Quick Settings RestartPolicy verbatim:
known zero-code retirement may recover within its existing bounded budget;
unknown or nonzero exits still stop recovery. Only constexpr qualification was
added to that shared method for compile-time fixtures; its runtime algorithm
is unchanged. Host discovery, pinned identities, thread-specific hook and
the existing Start DLL remain in use. This is local lifecycle adaptation, not
a copied third-party restart engine or a Windows security-policy modification.
The existing upstream Compact Start Menu attribution continues to apply to
the unchanged layout component. See docs/start-recovery-latest.md.

## GPT maximized hot-corner compatibility (2026-10-13)
NativeHotCorners remains the retained implementation. The integration now shares the existing client-edge package identity and maximization policy; it does not add another runtime or copy additional third-party code. The generator adds a narrowly scoped fullscreen exception and reason-level diagnostics. Public contracts: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iszoomed and https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles .

Version 0.8.48 adds the native persistent app switcher (Ctrl+Alt+Tab) to the
same retained NativeHotCorners runtime. Chord construction, held-key checks,
and partial-submission cleanup are local adapters; no additional third-party
implementation or runtime is incorporated. Original Win+Tab, event delivery,
spatial rearming, fullscreen suppression and session lifecycle are retained.
Public contracts: https://support.microsoft.com/en-gb/accessibility/windows/keyboard-shortcuts-in-windows
and https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput .

### Card gesture recognition (0.8.32)
The 0.8.31 DefWindowProc conversion was tested and rejected: 13 admitted
DM_POINTERHITTEST messages produced no pointer/wheel stream. That path is
removed. The adapter now reuses the DirectManipulation manager/viewport
setup, SetContact, transform callbacks, READY reset and interaction lifetime
from Chromium's DirectManipulationHelper and DirectManipulationEventHandler:
Copyright 2015, 2019 The Chromium Authors, BSD-3-Clause, commit
abc2ec4376609fa5df49daee6d57edaa10bea83f. Preserved source and license are in
upstream/cards-input. The helper .cc SHA256 is
7B90A56C4C269EEF64406FB8272E7D4A594EF00BCC46615E0BB254B26265A5D6;
event handler .cc SHA256 is
D06EDD4AF908AD260A9073547339AFC596C1D2647BAD9FA759F40F908986E64F.
Local adaptation replaces Chromium compositor observation with the existing
XAML Rendering event, uses weak Board/state ownership, limits contact to the
measured card, disables translation inertia and maps 48 DIP dominant-axis
motion to paging, with one page per contact sequence since 0.8.33. Version 0.8.34
corrects the content-space reset origin and rejects asynchronous reset callbacks
outside an admitted contact; the original Chromium reset lifecycle remains the
reference and both callback and real Windows COM regressions are retained.
Pinch rejection, per-gesture paging, lifecycle bounds and diagnostics are
local integration. No Chromium browser event injection or taskbar-volume
SendInput/focus behavior is included. Real hardware acceptance is separate.

Version 0.8.36 icon optical sizing is local code using the public Windows
WriteableBitmap/RenderTargetBitmap APIs and the preserved styler's element lifetime. It measures
alpha bounds rather than app names, preserves native icon source/layout, and
adds a bounded aspect-preserving drawing transform. This is not upstream icon
normalization code or an image asset replacement. API reference and acceptance
limits are in docs/icon-optics-latest.md.

Version 0.8.44 adds a local source-resolution adapter to the retained
taskbar-icon-size lifecycle. Microsoft matching symbols and decoded call sites
identify five resource/cache callers of TaskbarHost::IconSizeInViewPixels; the
layout caller is excluded. Only source requests receive extra pixel budget.
The Windows loaders, image-list cache, resource identity, DPI and change
notifications remain native. No Microsoft implementation source is copied.
Pinned call-site provenance and real-pixel verification limits are documented
in docs/icon-optics-latest.md.

Version 0.8.49 reuses the retained Windows 11 Taskbar Styler rule parser,
XAML diagnostics attachment, UI-thread ownership and property restoration.
Reference: m417z, ramensoftware/windhawk-mods, Windows 11 Taskbar Styler 1.10,
commit 162bca0d6b4fe4da2c63c21f17921a165d5870fa (GPL-3.0), already retained
as the upstream implementation. The additional switcher selectors, hash gate,
diagnostics, single-use corner request and native thumbnail Invoke adaptation
are local code. No alternative switcher implementation is imported.

The installed Microsoft Switcher PRI/XBF was inspected offline with chausner
PriTools (765d4544fd48aaaebf848b5b1b24ade2cf1770a0) and XbfTools
(dbeadcd75f30fb8dea3109039e0082854cb9a89d), both Apache-2.0. These are analysis
tools, not packaged/runtime dependencies. Their source and licenses remain in
the task's isolated research output. Selectors and layout values were checked
against decoded resources; no Microsoft implementation source is copied.
Exact system hashes, public API references, limitations and acceptance gates
are recorded in docs/switcher-style-latest.md.

Version 0.8.53 adds a local, independently written preview-size policy and
per-root typed RowHeight ownership. Native DynamicFlowPanel remains responsible
for layout, thumbnail rendering and interaction. Offline Ghidra analysis of the
exact installed Microsoft binary established the minimum-row selection and
24-DIP row spacing; it is evidence, not redistributed Microsoft source or a
new runtime dependency. No private vtable offsets or layout-enum writes are
added. The retained upstream property parser and lifecycle remain in use.

Version 0.8.37 renders a neutral same-source sample to avoid native visual
offsets and clipping in the measured bounds, then applies the correction
before the preserved native transform. Sampling uses an offscreen zero-size
container on the existing tree, bounded frame retries, and explicit cleanup.

Version 0.8.59 adds locally written native-switcher visibility ownership and a corner open/cancel decision. It retains the NativeHotCorners input/latch protections and the existing m417z Taskbar Styler XAML lifecycle. No new third-party implementation, global polling, UI thread or replacement switcher is introduced. The prior two-item confirmation and layout algorithm remain in use.

Version 0.8.60 removes the locally written two-item automatic confirmation request and thumbnail Invoke path. NativeHotCorners input/latch protections and the existing switcher visibility, layout and m417z Taskbar Styler lifecycle remain. No new third-party code is introduced.

Version 0.8.66, after the full rollback of the second-page experiment to 0.8.60, restores only the local corner-scoped two-item confirmation policy at the user's new request. It reuses the retained pre-0.8.60 single-use request and native thumbnail Invoke path, with deferred dispatcher execution, root lifetime/close cancellation, realized-item checks and foreground revalidation. NativeHotCorners input/latch protection and m417z Taskbar Styler loading, template compatibility and unload responsibility are unchanged. The new guards are local adaptations, not attributed to upstream. No new third-party dependency, application page, Win-key interceptor, replacement switcher or polling loop is added.

Version 0.8.67 corrects the failed 0.8.66 assumption that native ListView.SelectedIndex identifies the switch target. It reads the current XAML island's keyboard focus through public FocusManager.GetFocusedElement(XamlRoot), maps the focused descendant to the existing native item container, and observes/removes GotFocus alongside root lifecycle events. No native selection or private offsets are modified.

Version 0.8.68 changes only the corner confirmation dispatcher priority from Low to Normal to avoid waiting for an idle input queue. The user accepted 0.8.67 two-window round trips but reported picker flash. Native invocation, weak callbacks, request identity and lifetime guards are unchanged; no new third-party code or visual suppression is introduced. Normal follows the public CoreDispatcher guidance; High is deliberately not used.

Version 0.8.69 adapts the pre-display hook boundary from L3r0y, Alt+Tab window delayer 1.2.0, ramensoftware/windhawk-mods commit b07c7e0d65abbbf7d10547608004595ce07189e0 (MIT under the repository license policy). Unmodified upstream source, policy, attribution and MIT terms are retained in upstream/switcher-delayer. Unlike upstream's withheld ShowWindow, the local adapter preserves native ShowWindow/focus and temporarily owns only the requested host's DWM cloak, with exact system hash/RVA gating, a bounded one-shot watchdog, scoped uncloak interception and explicit Stop/resume restoration. Microsoft symbols and local disassembly are compatibility evidence, not copied implementation source.


Version 0.8.70 extends the same local adapter to native uncloak calls inside the verified DisplayAltTab scope for reused hosts. Native close cloak is retained when ownership is released; no additional upstream algorithm, private offset or hook is introduced. These lifecycle fixes and hidden-window regression tests are local adaptations.


Version 0.8.72 adds a locally written, public-XAML stale clip reconciliation policy. Microsoft microsoft-ui-xaml commit 1af995b98a02d86b1951c4babe35caa269a6d569 (MIT), ScrollContentPresenter::ArrangeOverride/UpdateClip, was reviewed as a contract reference only; no implementation code or new runtime dependency is imported. The reference source and license remain in the isolated investigation directory. The running Windows.UI.Xaml binary is not claimed to equal this WinUI source revision. Existing m417z diagnostics attachment and root lifetime are retained; the native clip object, native focus and input paths remain intact.


Version 0.8.73 corrects local native-XAML COM-peer retention. Existing public geometry, native focus/input and layout policy remain. The no-window native regression compiles production retain/release helpers and demonstrates weak-peer expiry and explicit release. No new upstream source or dependency is added.

### 2026-10-23 native aggregate adaptation

The native upstream modules and their existing notices remain unchanged. `aggregate/` adapts selected locally maintained LeanDock capsule 0.9.8 material/popup/control/metrics code; `origins-latest.json` records the exact imported source hashes. This is source reuse of those components, not reuse of capsule's independent taskbar receiver, app list, pinning, guard or taskbar hiding. The native XAML stadium ring ports the existing arc-length mapping into PathGeometry; the native host owns layout/subscriptions, and the ordinary-user popup owns graphics/device workers and their cancellation. No new third-party runtime or license has been introduced.

Version 0.8.81 adds locally written visible-only CPU trend/clock and metric throttling. The GPU 3D grouping uses PDH adapter LUID/physical-engine identities matched to DXGI; Microsoft's DirectX article "GPUs in the task manager" is a behavior reference, not imported source. This view explicitly reports 3D, not total GPU load. Existing input, control workers, native tray/pins, component ownership and rollback remain retained.

Version 0.8.83 reorders the locally written hover-panel sampling (inexpensive values first, deferred PDH/WMI opens, a 300 ms first rate interval), reuses the default audio endpoint while it is unchanged, prunes finished update backups beyond the newest five, and adds always-on call/time counters around LeanDock's tray and surface layout handlers. No third-party source is added.

Version 0.8.84 replaces the hover panel's PDH GPU query with the public D3DKMT statistics thunks exported by gdi32; only the fields read are declared locally from the documented Windows SDK 10.0.26100 d3dkmthk.h layout and asserted at compile time. The liquid-glass composition layers, UISettings accent colors, staged reveal/leave animations and two-stage hover launch are locally written. No third-party source is added.

Version 0.8.90 (controller 0.3.34) lets the auto-hide Dock and the hot corners work over fullscreen applications other than games. It extends LeanDock's existing use of Microsoft's documented NonRudeHWND window property and ITaskbarList2::MarkFullscreenWindow(FALSE) (https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow) from maximized windows to true fullscreen ones, and narrows the retained NativeHotCorners PauseOnFullscreen rule to games, detected with SHQueryUserNotificationState (QUNS_RUNNING_D3D_FULL_SCREEN, https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shqueryusernotificationstate), game-store library folders and a user list. The policy is locally written; no third-party source is added.

Version 0.8.89 adds the panel's Bluetooth, hotspot and sound pages, all locally written against public Windows APIs (Windows.Devices.Enumeration association endpoints, NetworkOperatorTetheringManager, Core Audio session and endpoint interfaces) with two exceptions taken as reference only, no source copied: connecting and disconnecting a Bluetooth audio device sends the driver's one-shot KSPROPSETID_BtAudio property through IKsControl as ToothTray does (m2jean/ToothTray, BSD-2-Clause), and choosing the default output uses the undocumented PolicyConfig client whose interface layout is declared as in EarTrumpet (File-New-Project/EarTrumpet, MIT) and AudioDeviceCmdlets (frgnca/AudioDeviceCmdlets, MIT). No third-party runtime is added.

Version 0.8.85 adds `aggregate/glass-latest.h`, a locally written D2D gamma-transfer effect wrapper following the same public IGraphicsEffectD2D1Interop pattern as the extracted `effects-latest.h`, which stays unchanged; it reworks the locally written panel material and fixes the void WmiSetBrightness result check. No third-party source is added.

Version 0.8.82 adds STARTF_FORCEOFFFEEDBACK only to the native hover-panel launch. Microsoft's public STARTUPINFOW contract documents default GUI startup feedback and this opt-out: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-startupinfow . This is an API contract reference, not copied source or a new dependency. Suspended creation, owner publication, child lifetime and all accepted panel binaries remain unchanged.
