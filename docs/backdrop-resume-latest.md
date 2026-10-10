# 桌面切换后的玻璃背景恢复

用户确认复现路径为左下触发角快速显示／恢复桌面，旧应用背景偶尔留在 Dock，任意点击或按键后恢复。0.7.5 的现场记录与 0.7.6 的点击修正没有证明该背景问题已经解决。0.8.8 前的只读采样中 DWMWA_FREEZE_REPRESENTATION=0、窗口未隐藏，但没有捕捉到残留帧，不能排除瞬态合成问题，也不能认定 Windows 冻结标志就是根因。

## 复用与改动

玻璃继续使用保留的 Windhawk Taskbar Styler XamlBlurBrush、原有高斯模糊、着色、主题代理与回退实现。没有新建特效图或替换渲染框架。源码适配由 build/generate-latest.py 生成；新增代码分别位于 include/backdrop-resume-latest.h、src/backdrop-brush-latest.inc 与 src/hide-state-latest.inc。

隐藏模块原本只有 foregroundWindowOnly 开启时才订阅前台事件。现在同一工作线程始终订阅这一个事件，仅桌面（Progman／WorkerW）前台状态改变时发送恢复消息；普通应用间切换不会因此触发背景恢复，也不改变原来的遮挡判断。隐藏策略改变并同步原生状态后，如果确认 Dock 已展开，也发送消息；原生 Unhide 可能先完成展开，因此不要求两次读取的值必须不同。触发角的原生 Win+D 输入、延迟与重新触发规则保持原样。

恢复在对应任务栏 XamlRoot 的 UI 线程执行。首轮延后 16 ms，末轮在首次执行后 160 ms；这是有限的恢复策略，不是假设 Windows 动画固定在此时间内结束。每轮只替换 CompositionEffectBrush 的 backdrop 输入，再调用 RequestCommitAsync。效果图、圆角、布局、命中、颜色和用户主题配置保留。没有模拟输入、透明度抖动、永久刷新或额外线程。

每次新请求会取消旧计时器并推进代次。迟到回调必须匹配当前代次、存活的弱引用、同一 XamlRoot、加载状态与有效任务栏；断开、销毁或停止时撤销订阅与计时器。两个回调结束后无后台刷新。过期超过 1 秒的窗口消息直接丢弃。

## 验证与限制

- tests/backdrop-latest.ps1 覆盖快速连续 64 次请求、旧回调迟到、重连、停止、UI 线程延迟和有限重试预算；tests/events-latest.ps1 核对原有工作线程停止流程。
- LeanDock.Backdrop.* 窗口属性记录请求次数、已提交的源更新次数、原因、时间、待处理状态和同步错误。属性采用数值加一编码，缺失表示尚未记录。RequestCommitAsync 返回不代表显示器已经呈现正确画面。
- 必须通过左下触发角反复切换的实际观察确认残留是否消失，同时复核 Dock 隐藏／展开、卡片切换与电池点击。后台就绪和这些计数不能代替视觉验收。
- 当前策略针对用户报告的背景残留；Windows 合成器内部为何停留旧帧尚未证明。若残留仍出现，应记录其发生时这些计数是否已增加，再决定是否需更深的合成生命周期修正，不能无限提高刷新频率。

## 一手参考

上游 [windhawk-mods issue 2869](https://github.com/ramensoftware/windhawk-mods/issues/2869) 记录过模糊主题从应用回到桌面时背景停留旧状态；作者报告优化后仍有未解决情形，不能把它当作现成完整修复。本地处理是对保留实现的有界生命周期适配。

[微软 RequestCommitAsync 文档](https://learn.microsoft.com/en-us/uwp/api/windows.ui.composition.compositor.requestcommitasync) 说明该调用尝试异步发起合成提交；它不是强制显示完成或成功修复残留的保证。

更新沿用 latest.ps1 的 Update／Rollback；部署前备份与哈希、配置保留、真实验收记录在 docs/verification-latest.json。

## 0.8.9：在原生展开前准备背景

0.8.8 用户实测反馈“会短暂残留，但能自行恢复”。当时控制程序记录 68 次左下显示桌面动作，背景请求 140 次、更新 104 次（包含先前 33 次组件探针请求和 4 次更新），没有同步合成错误。只证明恢复生效，不等于短暂残留已解决。

0.8.9 在保留的 TrayUI_Unhide_Original / CSecondaryTray__Unhide_Original 调用之前，通过同一任务栏 UI 线程同步交给样式模块更新背景源。此时允许宿主尚未显示，但要求元素仍加载、XamlRoot 匹配且已有有效模糊效果；断开或回退色状态不强行重建。更新仅替换原有效果的 backdrop 输入，随后交回原生展开动作。

准备动作首先取消前一轮未执行的更新。展开后仍使用 0.8.8 的两次有界恢复：每次实际准备额外提交一次背景源更新，没有提高空闲刷新频率、添加线程或修改窗口动画。同步入口仅接受任务栏所属线程，跨线程调用直接跳过，避免阻塞与死锁。

新增 LeanDock.Backdrop.Prepares / LastPrepareAt 记录真实准备次数与时间。tests/backdrop-latest.ps1 使用自有隐藏消息窗口检查准备在调用返回前完成，以及跨线程调用不会发送或阻塞；它仍不能代替真实 Dock 的首帧验收。需要继续观察快速左下触发角切换是否还有残留帧。

这个接入点位于 LeanDock 隐藏策略调用的原生 Unhide 之前；Windows 内部其他展开路径可能更早，不能仅凭准备入口执行就声称已覆盖全部首帧路径。若新版仍有残留，应先区分实际展开是否早于该调用，再决定后续接入位置，避免反复调整延时。
