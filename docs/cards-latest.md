# 中间卡片维护说明

## 功能与布局

0.5.0 使用“应用｜CPU／媒体卡片＋电池卡片｜应用托盘”。CPU／媒体共享 160×44 DIP，滚轮切换；标题和 72×24 DIP 的三按钮行各自居中。缺失的歌手信息不占布局列，有歌手时放入提示。

电池卡片为 52×44 DIP，距主卡片 8 DIP，整个中间槽宽 244 DIP。电池独立可见，左键优先调用原生 ControlCenterButton 的 Invoke 接口，失败或缺失时使用 Win+A；右键任一卡片打开 Win+X。Dock 空白的 WM_CONTEXTMENU 只在 XAML 命中确认无原生应用／托盘按钮时改为 Win+X。应用跳转列表和通知图标的菜单不改。

快捷键在真实 Dock 手势处理过程中，由 Explorer 激活自身窗口后发送。只有 Win+A／Win+X 两项固定动作；没有新增控制器消息接口、通用按键接口或提权操作。发送不完整时释放已提交的按键。命令提交成功不等于面板实际显示，必须保留真实点击验收。

## 布局所有权与退路

cards-latest.cpp 持有 RootGrid.Padding 和 BackgroundControl.Margin 的原始值，停止时恢复；这些属性不再由样式规则重复控制。覆盖布局为卡片分配零额外内容测量宽度，通过右内边距保留真实空间。共享 Dock 边界、玻璃和菜单定位使用扩展后的 TaskbarFrame。

空间不足先收起主卡片，保留宽 76 DIP 的电池槽；更窄时也取消电池卡片占位。PowerReady 只在卡片已完成布局且能够容纳时发布。托盘读取该状态，才折叠原电池及已隐藏的输入法容器；未就绪时使用原先的紧凑原生电池布局。四 DIP 滞回防止分数缩放边界抖动。

应用托盘保留原生父对象与点击对象，连续两排排布；奇数个应用时最后一颗整体上下居中，包括悬停和命中区域。托盘适配在 tray-cards-latest.inc 中，生成器将其插入受版本固定的上游适配。停止时按反序恢复依赖属性与绑定。多显示器与 DPI 切换还须实测，不能把单屏结果扩大。

## 数据与生命周期

CPU 使用 GetSystemTimes 两次差值，kernel 已含 idle。首个区间显示“—”，超过 64 逻辑处理器时不把部分组负载当整机。内存使用 GlobalMemoryStatusEx。两秒 DispatcherTimer 仅在主卡片可见且处于状态页时运行；隐藏或切换媒体后停止，重现时重建 CPU 基线。

媒体使用 SMTC 会话事件。回调排入 UI Dispatcher，弱引用和代次判断阻止过时结果覆盖新会话。按钮按会话能力启用，提交期间禁用。歌名只在内存，不写日志、不下载封面。

电池使用 GetSystemPowerStatus，并监听 PowerManager.RemainingChargePercentChanged、BatteryStatusChanged、PowerSupplyStatusChanged。回调合并后在 UI 线程刷新，Dock 从隐藏恢复时也读取一次。未知电量不显示 0%，未知充电标志不当成正在充电。没有新增定时器或常驻进程。

停止时撤销所有媒体／电源订阅，取消挂起读取，停止计时器，移除新增 XAML、窗口属性并恢复布局。关闭中的状态禁止由 LayoutUpdated 重新建立卡片。

## 构建、诊断与回滚

build/latest.ps1 重现生成源码。cards、surface 和 tray-cards 属于 Explorer DLL；本次正式部署沿用已验证的控制程序和 Start companion，不改变触发角。维护生成适配时同时修改生成器或其引用的片段，不能只修改生成输出。

LeanDock.Card.* 提供只读数值：采样与错误计数、视图、媒体存在、电池值和卡片／控件几何。LeanDock.Shell.* 仅在真实动作后提供计数；两者均不是外部命令通道。真实布局还通过 latest-layout.json 记录应用数量、格子尺寸和坐标。

回归包含 cards-latest.ps1 的负载、电源未知值及空间边界，shell-actions-latest.ps1 的固定按键序列（不发送输入），geometry-latest.ps1 的共享坐标。源码构建和系统 DLL 依赖检查不能替代桌面点击验收。

生产更新须备份，停止控制程序，干净重载经完整路径和会话核对的任务栏 Explorer；DLL 被映射时不覆盖。回滚按哈希恢复既有代码、配置和二进制。绝不能按 latest.exe 文件名停止其他工具。

一次候选控制程序在 2026-10-08 被 Defender 行为监控拦截，相关任务也被移除，已用修改前的已验证程序及任务定义恢复。候选新增的管理员动作消息接口已撤回；正式实现不依赖它。没有关闭防护、添加排除或允许被隔离文件。完整运行和用户验收状态见 verification-latest.json。
