# 底边唤出适配 0.8.74

## 当前事实与修正边界

2026-10-19 重启后的实际加载版本为 0.8.73、控制器 0.3.29、后台 0.5.59；四个运行文件与构建收据哈希一致。用户报告 GPT 和其他某些最大化应用的底边出现缩放光标，无法唤出 Dock。未取得其他应用的具体名称，不把 GPT 实测扩展为其他应用已通过。

只读几何与命中检查确认：

- 屏幕和工作区均为 (0,0)-(2560,1600)。隐藏任务栏位于 y=1598..1694，裁剪范围 x=894..1666，底边两像素仍在。
- GPT 保持原生最大化，窗口矩形到 y=1600，可见 DWM 边框到 y=1598。底部 y=1599 的 WM_NCHITTEST 返回 HTBOTTOM=15。
- 隐藏任务栏没有 WS_EX_TOPMOST；Dock 正下方的 WindowFromPoint 返回 GPT。故障是可点击层级被遮挡，不能由此认定需要新增任务栏工作区留白。
- 控制器报告 clientEdgeReady=false、failures=1。控制器启动于 21:51:12.127，Explorer 启动于 21:51:12.325。旧实现启动时初始化一次，失败即完整退出，该功能没有恢复路径。
- 当前独立只读探针的 COM 初始化、TaskbarList 创建、HrInit 和五组事件注册均成功。早期任务栏未就绪是与证据一致的解释；旧版没有记录具体 HRESULT，不能声称已追溯到那次失败的确切 API。

## 实现

只更新控制器与维护状态，不重建后台、开始菜单、快速设置 DLL。继续使用公开的 NonRudeHWND 与 ITaskbarList2::MarkFullscreenWindow(FALSE)，恢复属于 LeanDock 的原生任务栏层级，不调整应用尺寸、工作区或任务栏裁剪。

初始化移到原生后台 NativeActive 之后，使用已有的 Start AttachPolicy：失败完成资源清理后间隔 750 ms 最多三次。成功、预算耗尽或停止即撤销定时器；之后只能由符合预算条件的原生重新接入事件重试，不常驻轮询。拒绝访问或永久 COM 注册失败不自动重试。Explorer 生命周期变化清理旧连接及自有标记，然后等待新后台就绪。COM 初始化使用局部资源，发布前核对生命周期，迟到成功不能复活已经停止的组件。

GPT 包身份规则仍保留，供既有触发角规则使用。其他应用仅在以下证据全部成立时允许底边兼容修正：原生最大化、可调整大小、非置顶/工具窗、覆盖对应屏幕；同屏底部自动隐藏任务栏确实属于 LeanDock；底边裁剪区域仍存在；窗口实际占住 Dock 中心底边点；该点返回底部缩放命中；DWM 可见边框有有限内缩。普通窗口、其他位置的缩放命中、真正全屏可见边界或未知数据均不处理。泛用修正后若失去这些基本几何条件，撤销自有标记。已有外部 NonRudeHWND 值不覆盖、不清除。

事件回调仅安排合并检查，不截获鼠标。单次跨进程只读命中查询上限 50 ms；不存在后台鼠标轮询。任务栏本身的原生位置/层级事件也会触发重新核对。

状态增加初始化阶段、错误码、尝试/恢复次数、有限重试状态和泛用修复计数。维护健康检查单独展示 ClientEdge，后续更新须恢复原本就绪的底边组件，不能只看整体 Live。

## 复用与归属

审阅并保留 m417z Taskbar auto-hide when maximized 1.2.6 的原生隐藏/唤出路径，未替换其运行层。复用 LeanDock 已有 Start AttachPolicy 和控制器生命周期。新增通用边框识别为本地适配，不称为外部成熟实现直接移植。

微软依据：
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow
- https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist-hrinit
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect

## 验证与回退

72 项底边规则/原生属性/生命周期检查通过，覆盖有限重试、预算耗尽、终止拒绝、迟到回调、初始化期间停止及当前系统真实 COM/事件注册与释放。测试只创建隐藏的自有窗口，不操作桌面输入、不发布实际应用的兼容标记。51 项维护检查、138 项触发角规则与 33 项配置检查通过。编译和实际唤出分别验收。

构建、部署、用户验收实时结果见 verification-latest.json。原 0.8.73 验收保留为 verification-v0.8.73-before-edge-recovery.json；旧底边文档保留为 client-edge-v0.8.73.md。

独立现场材料位于主任务 work/edge-regression-20261019，修改前源码和控制器文件已备份；正式回退使用既有 Update 事务的备份，不直接覆盖部署文件。测试不等于下一次真实开机、睡眠恢复、多屏、全部应用或真正全屏的交互验收。
