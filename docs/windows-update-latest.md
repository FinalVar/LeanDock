# Windows 更新兼容性记录

## `0.8.120`（`2026-11-18`｜修正 `26300.9974` 下 `ShowLeanDockFlyoutGeneral` 右键菜单崩溃与面板改进）

- **问题现象**：`0.8.119` 部署后，右键聚合时间/日期/电量键（触发原生 `Win+X` 菜单）立即导致 `explorer.exe` 以异常码 `0xC0000005`（`EXCEPTION_ACCESS_VIOLATION`）退出并重载（见 `latest-explorer-exits.log`）。
- **根因定位（逐字节反汇编核实）**：
  - `Taskbar.View.dll`（`2609.12001.100.0`，RVA `0x6342B4` / `6505140`）中 `winrt::impl::consume_general<IFlyoutBase5, FlyoutBase, int(abi::*)(void*,void*) noexcept, void*&, void*&>` 的第 3、4 个参数（`R8`、`R9`）类型为 `void*&`，即调用方栈上 `DependencyObject` / `FlyoutShowOptions` 句柄槽位的地址（单层指针 `const DependencyObject*`，内部保存裸 ABI 接口指针 `IDependencyObject*`）。
  - `0.8.119` 的 `ShowLeanDockFlyoutGeneral` 误将其声明为双层指针 `const DependencyObject* const*` 并向 `ArmLeanDockFlyout(value, *target)` 传入 `*target`，把 COM 对象首地址当作 `DependencyObject*` 解引用，使 `try_as<FrameworkElement>()` 将对象虚表地址误作接口指针读取，直接触发 `0xC0000005`。
  - 同时，`IFlyoutBase5` 仅有 `ShowAt` 一个方法，MSVC LTCG 在 `0x6342B4` 内直接内联调用固定桩 `0x2F6390` 并消除了全部 4 处调用点对 `RDX`（`method`）的赋值；因此钩子不得以 `method != nullptr` 作为前置条件，仅透传 `RDX` 即可。（对比核验：`SystemTray.dll` RVA `0x3A328` 与 `Taskbar.View.dll` RVA `0x6542D8` 的 `double&` 版本均显式传递并间接调用 `RDX`，且参数本就声明为单层 `double*`，无同类问题。）
- **修复与合并改动**：
  - `src/surface-latest.inc`：修正 `LeanDockFlyoutShowGeneral` 与 `ShowLeanDockFlyoutGeneral` 参数类型为 `(Controls::Primitives::FlyoutBase*, void*, const DependencyObject*, const Controls::Primitives::FlyoutShowOptions*)`，直接向 `ArmLeanDockFlyout(value, target)` 传递 `target` 并移除对未初始化 `RDX` 的非空判断；保持 `include/shell-actions-latest.h` 原子 4 事件 `SendInput` 原实现不变。
  - 合并本轮悬浮面板改进：`aggregate/metrics-latest.h` 首次媒体查询短等待预热、`aggregate/composition-latest.h` 底部锚定高度过渡动画、`aggregate/latest.cpp` + `include/aggregate-bridge-latest.h` + `src/cards-aggregate-latest.cpp` 4 秒静默保活复用，以及 `aggregate/control-panel-latest.h` `consteval Stack` 纵向布局计算与编译期校验。

## `0.8.119`（`2026-11-17`｜Windows 11 `26300.9711` → `26300.9974`）

- **问题原因**：系统更新并重启（约 `14:09`）后，Windows 构建升至 `26H2 / 26300.9974`。计划任务 `NativeTaskbarDock` 与控制器 `0.3.43` 正常启动、触发角正常就绪，但 `NativeVerifySystem` 拦截到 `taskbar.dll` 等系统二进制指纹变更（`latest-status.json` 记录 `state: 4`、`error: "taskbar.dll"`、`dockSupported: false`），主动中止注入。
- **全量核验与适配**：
  - 按 `AGENTS.md` 与 Microsoft 公开符号服务器下载 `26300.9974` 对应 PDB（`Taskbar.pdb`、`Taskbar.View.pdb`、`SystemTray.pdb`、`explorer.pdb`、`twinui.pcshell.pdb`、`ThemeUI.pdb`）并核验全量 15 个已变更系统二进制（`ShellComponents.Switcher.pri` 未变更）的 SHA-256、`TimeDateStamp`、`SizeOfImage` 与微软 Authenticode 签名（全部 `Valid`）。
  - **`baseline/icon-resolution-latest.json`**：`taskbar.dll` 中 `TaskbarHost::IconSizeInViewPixels` 移至 RVA `482476`、`ShellIconLoaderV2::s_ForceImagePresent` 移至 RVA `486396`；全部 6 个 `IconSizeInViewPixels` 调用点（5 个图标加载来源 + `CTaskListWnd::GetIconSizeInViewPixels`）与 2 个 `ForceImage` 调用点均为唯一直接 `E8` 调用，`build/generate-latest.py` 逐字节校验通过。
  - **C++/WinRT `consume_general` 模板并合适配**：`Taskbar.View.dll`（`2609.12001.100.0`）与 `SystemTray.dll`（`2609.11000.100.0`）中以下 3 处可选钩子在新版编译器优化下通过 `winrt::impl::consume_general`（传递 ABI 虚表成员桩指针 + 引用参数）调用：
    1. `SystemTrayFrame::Height(double)` → `consume_general<IFrameworkElement, SystemTrayFrame, ...>`（RVA `238376`），在 `build/generate-latest.py` 中新增 `SystemTrayFrame_Height_General_Hook` 校验 `IsAbiVtableCallOffset(method, 144)`（`put_Height`）、非 `NaN` 与 `IsRuntimeClass(pThis, L"SystemTray.SystemTrayFrame")` 后覆写 `g_taskbarHeight`。
    2. `ProgressBar::Width(double)` → `consume_general<IFrameworkElement, ProgressBar, ...>`（RVA `6636248`），新增 `ProgressBar_Width_General_Hook` 校验 `IsAbiVtableCallOffset(method, 128)`（`put_Width`）与 `ProgressBar` 运行时类型。
    3. `IFlyoutBase5::ShowAt(...)` → `consume_general<IFlyoutBase5, FlyoutBase, ...>`（RVA `6505140`），在 `src/surface-latest.inc` 提取共享 `ArmLeanDockFlyout` 与 `ShowLeanDockFlyoutGeneral` 并保持 `IsLeanDockMenuCaller` 调用点校验，保留指针右键去焦点框与连续右键重开跟踪。
    - 另 3 个内联旧版回退符号（`taskbar.dll` 内 `ITaskbarSettings::Size`，`Taskbar.View.dll` 内旧版 `TaskbarFrame::Height(double)`/`MaxHeight(double)`）在现代 26H2 已有 `TaskbarConfiguration::GetFrameSize` 与 `TaskbarController::UpdateFrameHeight` 直接覆盖，保留可选空置。
  - **`explorer.exe` / `twinui.pcshell.dll` / `themeui.dll` / 宿主进程**：确认 `CTray::_OnAppBarMessage`（`0x72a8`，范围 `[0x72a8, 0x7464)`）与 `XamlAltTabViewHost::DisplayAltTab`（`1118596`）入口与语义一致；同步 `shell32.dll`、`InputHost.dll`、`themeui.dll`、`ShellHost.exe`、`ControlCenter.dll`、`StartMenuExperienceHost.exe`、`StartDocked.dll`、`SearchHost.exe`、`WindowsInternal.ComposableShell.Experiences.Switcher.dll` 的严格指纹引脚。
  - **控制器与组件构建策略**：按 `AGENTS.md` 从 Clean 基准 `artifacts/controller-0.3.43-exitlog/latest` 仅同步更新后的 system-pin 头文件编译控制器 `0.3.44`，将其 SHA-256 回填至 `aggregate/theme-control-latest.h` 的 `ControllerHash` 后重编 `aggregate/latest.exe` 与 `latest.dll`，并继续沿用 `0.8.78` 的 `start/latest.dll` 与 `panels/latest.dll`。
- **详细证据**：见 [windows-update-evidence-latest.json](windows-update-evidence-latest.json) 与 [verification-latest.json](verification-latest.json)。

## `0.8.29`（`2026-10-14`）

- 当日 Windows 更新后任务栏与 StartDocked 二进制变更，导致 LeanDock 停留在 `NativeVerifySystem` 安全校验分支并记录报错 `"taskbar.dll"`（Triggers & Hot corners 未受影响）。
- 已通过官方公开 PDB 核验全部 `taskbar.dll` 符号与 `IconSizeInViewPixels` 调用点位置、复核 `Windows.UI.Xaml.dll` 导出和 `StartDocked.dll` 运行契约、补充系统组件签名校验，并在更新基线后重编控制器与后端模块，恢复通过全部 91 项运行时回归与布局/热区单测。
- 详细分析与符号比对表留存于本地 [windows-update-evidence-latest.json](windows-update-evidence-latest.json)（已随 26300.9974 轮次刷新）与 [verification-v0.8.29.json](verification-v0.8.29.json)。
