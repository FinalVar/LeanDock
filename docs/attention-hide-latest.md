# 通知与智能隐藏

## 行为边界

用户要求：应用收到消息时保留图标的红色提醒，但不强行显示 Dock，也不阻止
智能隐藏收起 Dock。鼠标在 Dock／底部触发区、键盘操作、菜单、拖动以及桌面
本来应该显示 Dock 的情况，继续交由原有逻辑决定。没有关闭 Windows 通知、
任务栏闪烁设置、应用通知权限或修改应用自身的未读状态。

## 实现与复用

适配 Taskbar Auto-Hide Motion 1.0.8 的两处实现：筛选应用关注的 Unhide 请求，
并仅在自动隐藏查询栈中将 IsFlashing 报告为 false。完整上游、提交与许可证
见 NOTICE.md 和 upstream/attention。

本项目不采用“先显示数秒再隐藏”、通知计时器、动画改写或屏幕边缘轮询。
请求 17 在运行时被直接过滤，其他请求及其 flags、this 参数原样传入。主任务栏
和副任务栏各自的 Unhide 入口均覆盖。

自动隐藏判断仍调用完整的 CTaskListWnd::PermitAutoHide，只在该调用的当前线程
和栈作用域内忽略 IsFlashing。菜单、拖放、焦点、指针等原生条件并未强制返回
允许隐藏。CTaskListWndMulti 在本机二进制中继续委托这一函数。
作用域使用 RAII，嵌套调用保留外层状态，返回或异常展开时恢复；停止阶段与
非隐藏查询中的 IsFlashing 继续返回原值。

兼容边界仍是 baseline 中四个已固定哈希的系统文件。新增两个地址取自本机
taskbar.dll 匹配的微软公开 PDB，未猜测偏移或删除哈希校验：

| 函数 | RVA |
| --- | --- |
| CTaskListWnd::PermitAutoHide | 399616 |
| CTaskBtnGroup::IsFlashing | 375296 |

状态中 attentionPolicy 提供 active、blockedReveals、ignoredFlashingChecks、
passedReveals 四个无应用名计数项。计数仅诊断，不驱动显示、隐藏或定时重试。

## 验证与交付

attention-latest 回归直接调用实际挂钩函数，检查视觉状态、菜单／鼠标／拖动
阻止项、主副任务栏所有测试请求的参数透传、嵌套与跨线程隔离、异常退出与
停止阶段恢复。它不能替代真实 Windows 任务栏的消息提醒验证。

最新部署、实际通知样本及未验证边界记录在 docs/verification-latest.json。
用户本次已自行关闭智能应用控制；更新通过既有备份、Update／Rollback 流程，
不修改安全策略。重新开启后的准入与实际行为需单独验证，不能沿用旧构建结论。
