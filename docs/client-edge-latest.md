# 底边唤出适配 0.8.74

## 0.8.93：回桌面露出 Dock；交还全屏只在前台时持有（控制器 0.3.37）

回滚到 0.8.91 后用户反馈回桌面 Dock 仍收着，说明不只是 0.8.92 的 TRUE。原因在自动隐藏模块：m417z 的 `ShouldKeepTaskbarShown` 枚举所有可见、未最小化、未隐身的窗口判断“能否隐藏”，不看 z 序；“显示桌面”无法最小化没有最小化按钮的全屏窗口（WS_POPUP 的全屏终端、播放器），只把 Progman 抬到它们上面（现场：Progman z12，终端 z16，可见、未最小化）。0.8.90 之前全屏时触发角暂停，无法从全屏应用用触发角回桌面，所以以前碰不到。另外该模块只在窗口创建/显示/位置/隐身事件时重算，前台换到桌面不触发重算。

- 自动隐藏模块（生成器适配 upstream/hide.cpp）：枚举按 z 序自上而下，遇到桌面（外壳窗口，或承载 SHELLDLL_DefView 的 WorkerW）即停止——其下窗口都被桌面盖住。平常桌面就在最底层，结果不变。已有的桌面前台观察（背景恢复用）在桌面进出时返回 true，让前台事件同样排一次 200 ms 合并的重算；普通应用间切换仍不重算。
- 控制器：恢复 0.8.92 的交还（`Hold`：撤 NonRudeHWND，写 Held 属性，`MarkFullscreenWindow(TRUE)`），但只在该窗口仍是前台时持有：`Kept` 对 held 条目要求前台，离开前台（回桌面、切到别的应用、开始菜单）即撤属性并 `FALSE`，由 Windows 在下次激活时自行判定；Dock 收回时若窗口已不在前台则直接释放不交还。异常结束遗留的 Held 属性仍由下一个控制器在该窗口处于前台时接手或清理。
- 测试：client-edge 108 项、hotcorners 138 项、配置 37 项。

## 0.8.92 试验：Dock 收回后交还全屏（控制器 0.3.36，已回滚）

部署后用户反馈：用触发角回桌面时 Dock 也收着，尽管没有遮挡。回滚前录到的现场：前台为 Progman，全屏的终端（无最小化按钮，显示桌面不能最小化它）仍可见、未最小化、位于 Progman 之下，带着 held 标记（`MarkFullscreenWindow(TRUE)`），QUNS 为 2。即持有的 TRUE 让 Windows 在桌面前台时仍认为有全屏应用。0.8.91 不持有 TRUE，用户确认表现正常。用户不再配合实测，按其意见用 Update/Rollback 事务回到 0.8.91（backups/before-0.8.92-20261101-001954159），源码同步退回。0.8.93 采用“只在前台时持有”。以下为试验时的记录。

0.8.91 部署后用只读采样（每 200 ms 记录 `SHQueryUserNotificationState`、前台窗口、NonRudeHWND、任务栏位置与 TOPMOST）陪用户实测 PotPlayer 和 Windows 终端全屏：

- 进入全屏、未碰底边：QUNS 2（忙碌）、任务栏被压到后面，即 Windows 的全屏处理完整保留（0.8.90 做不到）。
- 碰底边：标记、Dock 弹出；收回后撤标记——但 QUNS 停在 5、任务栏仍 TOPMOST，直到窗口重新激活（切到桌面再切回）才回到 2。撤掉 NonRudeHWND 后的 `MarkFullscreenWindow(FALSE)` 只是“交回自动判断”，而 Windows 只在激活时自动判断。
- 在卡住状态的真实终端窗口上实测公开接口：`MarkFullscreenWindow(TRUE)` 立即 QUNS 2、任务栏压后；随后 `FALSE` 又回到 5。所以必须保持 TRUE。

试验实现：Dock 不再需要时（`DockWanted` 为假且窗口仍是全屏应用）`Hold`：撤 NonRudeHWND/Owner，写 `LeanDock.ClientEdge.v1.Fullscreen`（值为控制器令牌），调用 `MarkFullscreenWindow(TRUE)`，条目记为 held。再次需要 Dock 时在同一槽位去掉该属性、重新取得 NonRudeHWND，原有的 `MarkFullscreenWindow(FALSE)` 同时撤掉 TRUE。held 条目在窗口不再是全屏应用（退出全屏、最小化、变成游戏/远程）、自动隐藏关闭或控制器停止时撤属性并 `FALSE`；窗口销毁时属性随 HWND 消失，按 PID 防句柄复用。控制器异常结束留下的属性（令牌不是自己的）在该窗口下次处于前台判断时处理：仍全屏则接手为 held，否则撤属性并 `FALSE`。状态新增 `clientEdgeFullscreenHolds`。测试：client-edge 108 项（新增交还、释放、接手、清理、句柄复用 8 项）。

## 0.8.91：按需放行与远程控制（控制器 0.3.35）

用户实测 0.8.90 在各类全屏应用里都能叫出 Dock，要求再检查有没有更好的做法；之后说明很少放映 PPT、远程控制时希望边缘透传给远程电脑、不玩游戏。

- 问题：0.8.90 只要非游戏窗口全屏在前台，就一直挂着 NonRudeHWND。Windows 对“全屏”的判断还决定全屏时的自动免打扰和通知压制（`SHQueryUserNotificationState` 的 QUNS_BUSY 等），一直挂着就等于全屏期间关掉了这些；Dock 隐藏时任务栏也保持 TOPMOST，隐藏时留在屏幕上的那几像素压在全屏画面之上。
- 按需：全屏应用只在 `DockWanted` 成立时标记，即鼠标落在同屏任务栏（LeanDock 的或原生副屏的，只读位置）隐藏时露出的那条边（任务栏窗口矩形与窗口区域的交集，没有区域时取整个窗口），或任务栏露出超过 8 像素（展开或滑动中）。Dock 收回、鼠标离开那条边后按原有 `Kept` 规则撤销标记并 `MarkFullscreenWindow(FALSE)`，原以为 Windows 会随即重新按全屏处理，实测要等窗口重新激活或重新进入全屏（见 0.8.92 试验）；这是 0.8.91 的已知限制。几何判断是纯函数 `LeanDockFullscreen::Wanted`，有单元测试。
- 鼠标来源：不另挂钩子或计时器。触发角已注册的原始输入（`RIDEV_INPUTSINK`，合并成一条 kEvaluate）在处理时先调用 `NativeCorners::pointerObserver`，控制器把它接到 `LeanDockClientEdge::Pointer`。`Pointer` 只在前台是全屏应用（`candidate`）且“是否需要 Dock”与“是否已标记”不一致时排一次判断，否则只读一次光标和任务栏位置。四角暂停时原始输入照常注册，照转；锁屏时不转。触发角没有初始化成功时 `pointerFeed` 保持 false，`DockWanted` 恒为真，退回 0.8.90 的常驻标记。
- 远程控制：`Game` 更名 `Quiet`，在游戏之外加入固定的远程控制客户端文件名（远程桌面 mstsc/msrdc、UU 远程 GameViewer.exe、ToDesk、向日葵、AnyDesk、TeamViewer、RustDesk、Parsec、Moonlight、Hyper-V vmconnect、VMware、VirtualBox）。这些全屏时 Dock 不放行、触发角暂停，边缘交给远程电脑。UU 远程的文件名取自用户机安装目录。
- 未做：PPT 放映不单独处理（用户很少用）；Game Bar 的“记住这是游戏”不读取（用户不玩游戏）。
- 构建：沿用 0.8.90 的方式，以 ctl-0.8.90 的构建输入为底只换入本轮文件；生成的触发角源码与 0.8.90 相比只多 `Quiet`、`pointerObserver` 三处。控制器 D03157F5…，主题开关哈希同步。构建输入存于 artifacts/controller-0.3.35-fullscreen/latest。0.8.93 起由 0.3.37 取代。0.8.92 起由 0.3.36 取代。
- 测试：client-edge 100 项（新增远程控制识别与 `Wanted` 几何 7 项）、hotcorners 核心 138 项、配置 37 项。

## 0.8.90：全屏应用（控制器 0.3.34）

用户报告视频播放器、游戏、ChatGPT、Windows 终端等全屏后叫不出 Dock，四个触发角也不响应。原因是两处有意的保护：Windows 把无标题栏、盖满屏幕的前台窗口当作全屏窗口，降低自动隐藏任务栏的层级；触发角沿用上游 NativeHotCorners 的 PauseOnFullscreen，对同样的窗口暂停四角。用户选择只在游戏里保持不触发。

- 新增 `include/fullscreen-app-policy-latest.h`：`Covers`（无完整标题栏、DWM 边框覆盖整块显示器）与 `Game`（`SHQueryUserNotificationState` 为独占 Direct3D 全屏；或可执行文件位于游戏平台库目录；或文件名列在 `hotcorners/latest.ini` 的 `Games=`；无法读取进程路径也算游戏）。
- Dock：`Evaluate` 在原有最大化分支之外，对前台的 `FullscreenApp` 走同一套 NonRudeHWND 取得、`MarkFullscreenWindow(FALSE)` 与任务栏 TOPMOST 修复。排除 Explorer 自己的窗口（桌面、任务栏、任务视图）、`Windows.UI.Core.CoreWindow`（开始、搜索等外壳界面）和工具窗口；置顶的全屏窗口不排除。窗口退出全屏、变成游戏或不再覆盖屏幕时，按原有规则撤销自己的标记。F11 这类不换前台的切换由已挂的位置变化事件触发重新判断。状态新增 `clientEdgeFullscreenRepairs`。
- 触发角：生成器末尾一段把 `Fullscreen()` 的结果再与 `Game()` 相与，`PauseOnFullscreen=1` 的含义变为“全屏游戏时暂停”。状态新增 `fullscreenAppEntries`。
- 构建：控制器只能以已验收源码为底。2026-10-31 先把 artifacts/widgets-density-0.8.78-accepted/latest 原样复制后 `build/latest.ps1 -ControllerOnly` 重编，生成的触发角源码哈希相同，`latest.exe` 与 FCA312C8 只差 PE 头的 3 字节链接时间戳；再在该副本上只换入上述文件重编，得到 CD380B5E…（构建输入存于 artifacts/controller-0.3.34-fullscreen/latest）。工作树里另有的开始菜单关闭改动没有带入。面板主题开关按控制器哈希核对，`aggregate/theme-control-latest.h` 同步为新哈希。
- 测试：client-edge 89 项（新增全屏与游戏判断）、hotcorners 核心 138 项、配置 37 项（新增 Games= 读取、拒绝时保留、状态字段）。
- 未验证：真实全屏下碰底边能否叫出 Dock、Dock 是否显示在置顶全屏窗口之上、隐藏时是否在全屏视频底部留下可见细线，都要用户实机确认；用户电脑上没有游戏，游戏判断只有规则测试。

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
