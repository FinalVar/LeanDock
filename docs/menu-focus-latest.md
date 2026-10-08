# 鼠标打开 Win+X 菜单的初始焦点（0.5.7）

用户在 0.5.6 的开始菜单、快速设置重复开关和智能隐藏验收通过后，补充反馈：右键系统工具菜单的第一项看起来像默认选中。这里的鼠标右键仍借用固定 Win+X 动作进入原生菜单，Windows 可能沿用键盘焦点外观；菜单项并未因此自动执行。

## 修改范围

保留 0.5.4 的原生 XAML 参照控件定位，以及 0.5.6 的面板定位、隐藏和媒体生命周期修补。只针对鼠标手势对应的原生 Win+X 菜单，在首次 Opened 或首项首次 GotFocus 时，把其已聚焦的顶层菜单项从 Keyboard / Programmatic 转为 Pointer 焦点。菜单的点击、悬停、方向键和关闭继续由 Windows 处理；没有永久禁用焦点指示或可访问性样式。

两项临时订阅在原生 ShowAt 之前建立，确认实际菜单项已取得 Pointer 焦点或手势过期后撤销。收到事件但菜单项仍为 Unfocused 时，不把修正视为已完成；后续继续由这份原生菜单的事件驱动。回调直接处理，不排入延迟计时器，不再移动窗口。未触发的订阅在下一次菜单调用及停止 Dock 时撤销；只保留弱引用，避免持有已关闭菜单。手动 Win+X 没有鼠标票据，因此不应用这项修正。

第一候选仅从 MenuFlyout.XamlRoot 查询当前焦点，用户反馈仍像选中第一项。该候选不算通过。后续直接读取这份菜单 Items 的 FocusState，并观察首项的首次 GotFocus，避免假定弹出层与放置目标共享同一焦点范围。

第二候选在首次事件回调就撤销处理，用户再次反馈未改善。诊断显示 Opened 和 GotFocus 已触发，但被检查项的状态仍是 Unfocused。最终修正以实际取得焦点为完成条件，不再把首个事件当作可见菜单焦点已就绪的证据。

## 范围检查

Taskbar.View 的 ShowAt 包装函数为其他菜单共用，因此不能只按 MenuFlyout 类型匹配。修正同时要求：

- 返回地址属于当前固定版本的原生 Win+X 协程或其两处显示回调；函数边界取自 PE 展开元数据，与匹配 PDB 的符号地址比较。
- 有尚未消费、未超过 1500ms 的真实 Dock 右键手势，且目标 XamlRoot 与该 Dock 一致。
- Opened 时仍为本进程的任务栏，且聚焦对象确实属于这个 MenuFlyout 的顶层 Items。

进入原生 ShowAt 前先消费票据，防止重入；后续键盘调用清除旧票据。符号缺失、手势过期、没有焦点或其他条件不满足时保留原生行为。没有新增输入注入种类、后台进程或轮询。

## 依据和验证边界

本机 Taskbar.View.dll 的 SHA-256 仍由运行时固定，新增四条 PDB 记录：三条只读函数地址和一处 ShowAt 包装函数钩子。构建前检查了三处调用的实际 call 目标与 PE 展开函数边界。该检查证明调用筛选与当前二进制匹配，不证明可见外观。

Microsoft WinUI 源码可用于解释焦点顺序，但不是这台系统 Windows.UI.Xaml.dll 的逐字源码。其 FlyoutBase 在加载过程中先设置焦点，再发出 Opened；MenuFlyoutItem 区分 PointerFocused 与 Focused，方向键则切换到 Keyboard。参考：

- https://github.com/microsoft/microsoft-ui-xaml/blob/main/dxaml/xcp/dxaml/lib/FlyoutBase_partial.cpp
- https://github.com/microsoft/microsoft-ui-xaml/blob/main/dxaml/xcp/dxaml/lib/MenuFlyoutItem_Partial.cpp
- https://github.com/microsoft/microsoft-ui-xaml/blob/main/dxaml/xcp/dxaml/lib/MenuFlyoutPresenter_Partial.cpp

LeanDock.Menu.FocusArmed、FocusBefore、FocusAfter、PointerFocus、FocusError 为只读数值诊断，不能代替真实菜单点击与视觉验收。最新结果见 verification-latest.json，0.5.6 的原有时序验收保留在 verification-v0.5.6.json。

## 维护与回滚

权威实现位于 surface-latest.inc；生成器保留新增状态与 WinRT 头文件，生成源码须可重现。只替换 Explorer 后端；控制器、触发角配置和 Start / Quick Settings 组件二进制保持原样。更新前保存部署文件哈希和可独立执行的恢复入口，按完整进程路径与会话核对后重载任务栏 Explorer。

2026-10-08，用户回复“没有了”，确认第三候选的第一项默认描边已消失；同次诊断记录 FocusBefore=2、FocusAfter=1、PointerFocus=1、FocusError=0。该回复仅作为描边消失的验收，不扩大为第三候选的点击、方向键或逐帧定位验收。多显示器、辅助技术、系统更新后的不同组件版本不从单机验收推导。
