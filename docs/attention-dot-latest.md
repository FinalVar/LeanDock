# 原生运行状态点与提醒

## 0.8.26 圆点位置

用户在 2026-10-12 要求图标下方的小圆点再往下移一点，增加图标与圆点的距离。
0.8.25 先下移 2 DIP，用户看过后回复“还可以再往下一点”。0.8.26 再增加 1 DIP：
现有 RunningIndicator 规则使用 `RenderTransform:=<TranslateTransform Y="3" />`，
使所有运行状态的圆点相对原始位置统一下移 3 DIP；不改变图标布局、圆点大小、原生颜色和提醒状态。
使用原有 Styler 的 XAML 对象样式解析与卸载恢复机制，没有增加行为挂钩。
此处仅调整 Rectangle#RunningIndicator，不影响原生应用进度条。

实际生成规则、构建、部署和视觉验收边界记录在 verification-latest.json；
不以编译成功或组件就绪代替主观间距确认。

## 0.8.22 红点提醒

用户在 2026-10-12 明确要求取消整块红色提醒底框，只保留红点。

0.8.22 / backend 0.5.21 仅扩展 baseline/styles-latest.json 中现有的
Taskbar Styler 规则，经原有生成器写入后端；没有增加挂钩、计时器或通知监听。

- CommonStates 的 RequestingAttention 和 RequestingAttentionMulti 背景透明。
- 对应 PointerOver、Pressed 状态使用与普通未激活按钮相同的淡色背景，避免鼠标经过时重新出现红框。
- RunningIndicatorStates 的 RequestingAttentionRunningIndicator 宽度固定 4 DIP，沿用现有 4 DIP 高度和 2 DIP 圆角。
- 不覆盖原生 Fill、Opacity、Visibility 或应用提醒标记。红点的出现和消除由 Windows 与应用控制，不能等同于独立的微信未读计数。
- 0.8.21 的 attention-only hide 代码不变，通知仍不强制唤出或钉住 Dock。

状态名称与选择器复用已保存的 Windows 11 Taskbar Styler 1.10；逐项对照
upstream/styler.cpp 的 Plasma 主题（Sand216）和
https://github.com/ramensoftware/windows-11-taskbar-styling-guide/blob/main/Themes/Plasma/README.md 。
原作者、许可证和输入哈希沿用 NOTICE.md 与 build/sources-latest.json。

更新使用现有 Package / Update / Rollback 维护流程，保留用户配置和更新前的运行组件。
只重编译后端；控制器、开始菜单和快速设置 DLL 的内容哈希应与更新前一致。
部署就绪不等于红点视觉验收；当前构建结果、部署回读和待确认范围见 verification-latest.json。
