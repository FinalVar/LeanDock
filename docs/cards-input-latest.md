# 卡片触控板输入（0.8.34 / backend 0.5.29）

0.8.32 于 2026-10-17 07:34 完成拦截后的恢复和部署，实时保护重新开启。用户确认“双指两个方向都能切换，滚轮和电池正常”，随后指出一次双指滑动容易切过两三页。0.8.33 加入单次接触锁定后，用户确认持续滑动只切一次，但松手额外切一次；后台也记录 9 次接触、18 次翻页。0.8.34 修正手势复位坐标与结束阶段的接触检查，用户随后确认“每次一页，松手不多切，下一次正常”。安全恢复经过见 defender-block-latest.md。

## 已确认的路径与失败

- 0.8.30 的 WH_GETMESSAGE 监听无效。01:39–01:41 的真实对照捕获 3 次卡片内 DM_POINTERHITTEST，直接发送给 InputSite WndProc，类型均为 PT_TOUCHPAD；队列没有这些消息。鼠标滚轮的 XAML 计数与切页计数同时从 5 增至 8。
- 0.8.31 已接到上述入口，但用户确认仍不切页。01:57:54–01:57:59 记录到 13 次已认领的命中，后续指针流、默认滚轮转换、XAML 滚轮和换页均为 0。因此“交给 DefWindowProc 就会转成滚轮”的假设在本机场景已被否定。
- 两次临时只读观察均结束、文件留存；没有使用全局输入钩子、模拟输入或占用鼠标。窗口过程身份查询确认实际组件为 System32\InputHost.dll，版本 10.0.26100.9711。独立 hash/PE pin 的源是 baseline/cards-input-latest.json，禁止直接编辑生成头文件。

## 当前接入

保留 Taskbar Volume Control 的 InputSite 查找与函数挂钩适配：核对所属线程、任务栏→DesktopWindowContentBridge→InputSite 层级、真实过程所属模块、路径、SHA256 和 PE 身份。使用既有 MinHook 链与退出门闩，不修改 GWLP_WNDPROC，不普通子类化，不硬编码函数地址。未知组件放弃新接入，原生行为与原 XAML 滚轮保留。

新的手势识别采用 Chromium 的 DirectManipulation 初始化、SetContact、内容变换回调及退出流程。每个卡片有自己的无视觉内容视口和 manager，矩形限定为卡片当前的物理坐标。只有当前可见卡片中的 PT_TOUCHPAD 命中才调用 SetContact；失败时调用原窗口过程，不吞掉输入。失败的 DefWindowProc 流转换已移除，不叠加保留。

系统提供的位移在扣除 DPI 后换算为页切换：主要方向锁定，累计到 48 DIP 触发一页，并锁定本次接触序列；继续滑动、增加距离或反向均不触发第二次，直到手势结束、抬手后再开始新手势。0.8.32 曾按每 48 DIP 连续切页，已按用户反馈改掉。缩放分类锁定后不翻页，视口不启用平移惯性。自然滚动方向由系统产生的内容位移决定。原生鼠标和指针滚轮仍走原换页函数，点击、右键和 Ctrl＋滚轮保持原有路径。

只在手势活动期间订阅 XAML CompositionTarget.Rendering，复用真实帧事件更新 DirectManipulation。手势结束、隐藏、停止或 30 秒无内容更新时取消订阅；无独立定时器或后台常驻采样。Stop 先移除消息路由、释放触点、停止视口、移除事件、Abandon/Deactivate，再撤销帧订阅。COM 回调通过弱状态持有；卡片回调通过弱 Board 引用。

0.8.34 的松手修正：Chromium 已说明正常一轮手势可能有两次 READY，第二次由 ZoomToRect 复位产生。旧适配用视口的窗口坐标作为内容坐标；真实隐藏窗口对照显示，视口位于 (600,100) 时，这会产生 (-600,-100) 的内容位移，而 (0,0,width,height) 恢复到单位变换。旧 READY 又清空单页锁定，后续 RUNNING/content 因为未检查活动接触而把复位识别成另一页。现在 READY 在 COM 复位前关闭接触门闩并清空累积，复位使用内容原点；内容回调必须同时满足 RUNNING 和接触有效，新 SetContact 成功后才重新启用。END 与 Cancel 同样先关闭门闩；没有活动接触的 BEGIN 不开启帧订阅。

## 检查和验收边界

- cards-input-latest.ps1：36 项通过，覆盖同步命中、SetContact 失败放行、边界、隐藏、Ctrl、不同输入类型、点击不截获、线程隔离与回调内停止。
- cards-manipulation-latest.ps1：47 项通过。生产 COM 回调在旧版复现松手多翻，修正后覆盖迟到 RUNNING/content/READY、先 END 后 READY、短手势、Cancel、下一次接触与复位不重启帧订阅。真实 Windows COM 的单位变换对照和原有轴锁定、方向、48 DIP 阈值、缩放、非法变换检查保留，Init HRESULT=0。
- 原卡片 115 项与配置/网络 492 项在本轮前一构建已通过，其算法在 0.8.32–0.8.34 未再修改。
- 隐藏窗口检查没有伪造硬件指针，回调序列是隔离回归。0.8.32 的双向切换、电池点击与滚轮已获用户确认；0.8.33 的松手行为被否定。0.8.34 的松手和重新开始已获用户确认，记录 13 次接触、12 次翻页、DMError=0，松手后 DMActive=0 且帧数保持 326，临时观察进程已退出。本次记录捕获纵向；横向算法回归通过，但未单独补充新版实物记录。多屏、睡眠、分数 DPI 保留未覆盖边界。

诊断分开记录：Wheel.Ready 是挂钩绑定；DMReady 是真实识别视口初始化；TouchpadStarts 是命中次数；DMContacts 是 SetContact 成功数；DMUpdates 是真实内容更新次数；DMSwitches 是由位移触发的实际换页次数；DMActive 是帧订阅状态；DMFrames 是帧更新次数；DMState/DMError 是系统状态/HRESULT。Native/Xaml 仍只计各自滚轮。旧 Fallbacks 字段保留为零，不再使用默认转换。停止时清除属性。

## 来源和版本

- Taskbar Volume Control 1.3.1，m417z，GPL-3.0，commit 131f90d6611b96811af101f4b8177dd24e40ed13。
- Chromium DirectManipulationHelper / DirectManipulationEventHandler，The Chromium Authors，BSD-3-Clause，commit abc2ec4376609fa5df49daee6d57edaa10bea83f。
- 原文件及许可证保留在 upstream/cards-input，NOTICE.md 记录适配边界。Chromium 的 compositor 观察改接现有 XAML 帧事件；卡片范围、翻页阈值、无惯性和失败诊断属于本地策略。未复制浏览器事件注入、任务栏音量或 SendInput。

公开契约：
https://learn.microsoft.com/en-us/windows/win32/inputmsg/dm-pointerhittest
https://learn.microsoft.com/en-us/windows/win32/api/directmanipulation/nf-directmanipulation-idirectmanipulationmanager-createviewport
https://chromium.googlesource.com/chromium/src/+/abc2ec4376609fa5df49daee6d57edaa10bea83f/content/browser/renderer_host/direct_manipulation_helper_win.cc

当前系统上 DM 命中的 wParam 可解析为触控板指针 ID，这与 Chromium 使用方式相同，但微软该消息页仍将参数记为未使用；因此保留已验证 InputHost 版本限制，不宣传为任意 Windows 构建的保证。

更新和回退沿用现有 Update/Rollback。Controller、Start 与 Quick DLL 字节保持不变；更新仍会恢复全部共享组件，不能用后端就绪代替开始菜单、快速设置或实际硬件验收。
