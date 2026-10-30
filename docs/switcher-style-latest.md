# Ctrl＋Alt＋Tab 布局与交互维护说明

版本：LeanDock 0.8.73；controller 0.3.29；backend 0.5.59（从 0.8.60 回退基线仅接回右下角两窗口确认；布局仍为 0.8.57，重复触发关闭保留）。部署与真实验收结果以 verification-latest.json 为准。

## 当前用户要求

切换器沿用 0.8.57 已验收规则；用户于 0.8.58 要求全部撤销应用启动自动最大化，已移除该独立功能；液态玻璃停在准备阶段。

窗口排布优先，外层画布由内容占用决定；先采用尽可能大且舒适的预览，数量增多后合理降档，一屏完整展示，无滚动。该要求已覆盖此前固定大画布要求。用户已确认 0.8.53 的分行、大小、背景收拢合适。0.8.54 曾隐藏选中圈并将画布透明，用户实际看到画布仍有类似玻璃的观感，随后更正为“画布还是不要消失的好，影响观感”。0.8.55 恢复原生画布，仅完全隐藏选中提示圈。用户随后指出画布外边距过大，0.8.56 缩小原生滚动容器的额外内边距。

## 布局边界

- 外层 Grid 中央区域最多占原生显示区 92% 宽、84% 高，预留 32 DIP 内部余量；它是可用上限，不固定撑大背景。
- 原生居中 Border、MinRow、FitToContent 保留。0.8.56 单窗口实测仍为 360 DIP 高，用户明确反馈过小。0.8.57 撤掉固定 360 DIP 上限，改为可用区域高度的 75%，向下取整到 24 DIP 档位；以此为起点逐档核对所有窗口的行宽与总高。当前显示区约 1706.67×1066.67 DIP，实际可用范围以原生尺寸和取整结果为准。本次单项真实读回 934×624 DIP，用户确认明显变大、大小合适；两项高 504 DIP，三项两行高 384 DIP，均完整容纳且无滚动或布局错误。宽长窗口还须通过宽度约束，窗口增加时自动降档；画布仍按实际内容收拢。窗口比例、列表次序、缩略图、点击及键盘导航仍由原生实现处理。
- 只通过已有 XAML 解析器构造正确类型的 MinRowHeight/MaxRowHeight 对象，Ratio=0；不写入、清除或探测私有 PanelLayoutMode 枚举，不调用硬编码 COM/vtable 偏移。
- 每个根分别保存两个行高的原本地值。元素替换、销毁及卸载恢复；只恢复仍由自己持有的值。原生宿主覆盖属性时让出控制。
- 沿用已有 Loaded/LayoutUpdated，按布局签名避免重复写入。真实 ScrollableHeight 溢出时最多降三档；仍失败则还原并记错，不隐藏滚动条或裁切项目。采样最多 128 项，超出保留原行为并明确边界。
- 本地尺寸策略是有限档位的可容纳选择，不声称取得数学上全局最优排布，也不声称复制 macOS 未公开算法。

## 画布与选中提示

0.8.55 撤除 AltTab 根内 BackgroundElement 的透明填充、透明描边及零边框覆盖，恢复 0.8.53 的原生底板材质。画布仍由实际窗口排布收拢，不固定撑满屏幕；没有调整父容器 Opacity、Visibility 或命中行为。

原生 SimpleVerticalScrollViewer 的 Padding=44 是额外外圈留白来源。0.8.56 用 AltTab 根、ModalRootGrid、BackgroundElement、SwitchItemListControl 及内部 ScrollViewer 的完整路径，仅将该容器 Padding 改为 8 DIP；保留每个卡片原生 Margin=12 和 24 DIP 项间距。无额外留白时，预计外侧为约 21 DIP（8+12+1），实际以测量为准。原生背景材质、行列算法与项目尺寸规则不变。

项目 UseSystemFocusVisuals=False，主次焦点线厚度均为 0，主次画刷透明。窗口预览和标题继续使用原生内容；保留原生悬停/按下与确认路径。直接键盘调用同一系统切换器也会呈现这套外观；Task View/Snap Assist 通过根与 HostControlName 限定排除。

schema 6 样式诊断读取实际 focusReadbacks、canvasReadbacks、canvasAlpha、canvasBorder、行高及解析错误；canvasReadbacks 仅计观察次数，非实色原生材质的 canvasAlpha=-1 表示未知而非透明。schema 3 几何记录行数、项目尺寸、全部项目是否在画布内及 ScrollableHeight，增加实际 scrollPadding 和项目可见边界到画布的 outerInsets，顺序均为左、上、右、下。样式读回不是像素验收，样式未出现时保持未知。

## 重复触发关闭

0.8.59 将右下角实际动作分为打开和关闭：未显示时使用完整 Ctrl＋Alt＋Tab；原生切换器确在前台显示时使用一对 Esc，交由 Windows 执行原生取消。0.8.60 曾移除自动确认；0.8.66 按最新要求仅为右下角打开创建一次性两项确认请求。按键已有按下保护、部分注入清理、空间复位、锁屏与全屏保护保留。

标识只由已通过兼容门控的 WindowsInternal.ComposableShell.Experiences.Switcher.AltTab 根发布。沿用 Loaded/LayoutUpdated，并订阅 Unloaded、Visibility 与 XamlRoot.Changed；只有根已加载、根和宿主可见、前台为同线程同进程的 XamlExplorerHostIslandWindow 才标记。标识是 HWND 上的临时属性，所有者为单个根，旧根清理不能删除替换根的新标识；根移除、卸载与停止时清理。普通 Task View 和其他 XAML 岛不被该根类型识别。

控制器在实际热角触发时核对标识、可见性、原生类名与 Explorer PID，在提交按键前再次检查前台身份。不是全局真假开关，不依赖上次打开计数。兼容性或识别不足时继续使用原来的打开组合键，不猜测普通窗口并发送 Esc。SendInput 后焦点仍可能并发变化，和用户键盘输入一样没有跨进程原子焦点锁；本地防护缩短并检查发送前窗口，不能宣称消除所有系统级输入竞争。只把请求计数当作诊断，真实开关、动画和操作结果仍由用户验收。

没有额外轮询、线程或可见窗口。复用本地 NativeHotCorners 的原始输入、空间复位、保护及清理链，和 m417z Taskbar Styler 的 XAML 注册／卸载链；窗口标记和选择打开／取消的分支是本地新增适配，不宣称来自上游。

API 合同参考：Windows.UI.Xaml.XamlRoot 的 Changed 与 IsHostVisible，FrameworkElement.Unloaded；采用系统 Windows.UI.Xaml，不替换为 Windows App SDK Lifted Compositor。
- https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.xamlroot
- https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.frameworkelement.unloaded

## 右下角两窗口互换

2026-10-19 用户先要求移除两窗口确认并完成 0.8.60 验收，后来尝试第二页应用方案，再明确完整回退至提出第二页前的 0.8.60。随后最新指令为“只有两个窗口的时候互换，这个做好”。0.8.66 仅恢复该独立行为，不恢复第二页、滚轮分页、Win 键截获或自动最大化。

控制器只在右下角打开前向拥有任务栏的同一 Explorer 后端发送 NativeSwitcherPrepare（WM_APP+207），后端核验控制器 PID。请求寿命 1500 ms、唯一序号、单次消费；发送失败、切换器关闭、根卸载和后端停止时取消。输入前重新核对前台和显示状态；不复用已经退役的 WM_APP+206。键盘单独 Ctrl＋Alt＋Tab 不创建请求。跨进程焦点竞争无法原子消除，过期或不可用时保留原生列表。

在已有 Loaded/LayoutUpdated 及新增 GotFocus 事件中，将确认推迟到原生 UI Dispatcher 的 Normal 队列；每个根最多一个待执行回调。回调校验根生命周期、请求序号、前台原生宿主、列表数量、原生键盘焦点所属项目及两项实际布局就绪。恰好两项时，通过 ThumbnailHost 的 IInvokeProvider 调用原生焦点项目的缩略图；消费请求后才 Invoke，防止同步重入。鼠标按下时放弃自动确认，保留用户操作。不是 EnumWindows 猜窗口，也不发送 Enter。原生默认选中通常为另一个窗口；从桌面进入的默认项仍由 Windows 决定，真实往返必须验收。

没有新增轮询、线程或窗口，没有改写原生缩略图点击或过渡动画。schema 4 switcherConfirmState 的 invokeAccepted 仅代表原生 Invoke 返回，不能证明前台已经切至目标；keptOpen、invokeErrors、attachErrors、lastCount、lastSelected、lastFocused 分开记录。迟到回调使用 weak_ref 和 attached 标志，关闭后消费旧请求，旧序号不能确认新一轮。

0.8.66 用户验收失败，记录确认右下角打开 12 次、两项预览均完整，但 SelectedIndex=-1、invokeAccepted=0。不能把旧策略测试通过当作原生选择模型正确。0.8.67 改用 FocusManager.GetFocusedElement(XamlRoot)，从实际焦点元素沿父链匹配当前两项容器，再调用该项 ThumbnailHost；不把 -1 猜成索引 1。根 GotFocus 仅重试已有一次性请求，卸载时撤销。调用前重新匹配同一焦点项；非两项立即消费请求并保留列表。公开接口：https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.input.focusmanager.getfocusedelement 。实际互换仍须重新验收。

0.8.67 用户已确认“能正常来回互换”，后台同时读回 invokeAccepted=19、lastFocused=1、lastCount=2、调用与挂接错误均为 0；这些计数不代表逐次前台切换已自动核验。用户仍看到切换器短暂闪现。0.8.68 仅将 RunAsync 优先级由 Low 改为 Normal，离开当前事件栈后按正常顺序执行，不再额外等候输入队列空闲；不使用系统保留的 High。请求和原生 Invoke 路径未改，不提前假定零闪帧，也不增加遮罩、移窗或第二套窗口列表。公开契约：https://learn.microsoft.com/en-us/uwp/api/windows.ui.core.coredispatcher.runasync 。

## 精确系统边界与故障历史

| 组件 | 版本/范围 | SHA256 |
| --- | --- | --- |
| Switcher.dll | 10.0.26100.9549 | A9A33AE9735F191895B39A2C62E2510993C3CA6E66D6723A5E16C770C70265F8 |
| ShellComponents.Switcher.pri | 本机 23 项 XBF | 928753406E1636E897847D07AC5203ACF12C12C6BBC1B17E9BB56BBC037E2141 |

不匹配或启用高对比度时跳过可选样式，原 Ctrl+Alt+Tab 动作仍可用。Windows 更新后重新审查模板，不删除哈希限制。

0.8.49 清空 MinRow 曾出现滚动。0.8.51 重复写入私有布局属性及增加解析诊断后，Explorer 三次 XAML stowed exception（0xc000027b，WER 0x80004002）；无对应转储，不能认定具体抛错语句。已事务回退，0.8.52 撤除这两类操作。用户确认 0.8.52 固定画布、一屏、Dock 正常，但拒绝六项单行排布。

0.8.53 真实读回 5 项全部生成、全部在画布内，2 行，卡片高 312 DIP、宽 435.33 DIP，背景 1468×762.67 DIP，ScrollableHeight=0；一次降档纠正，layoutError=0。用户确认分行、大小、背景合适。82 秒观察 Explorer 23712 不变、四组件 Ready、无新增崩溃。这些是当次快照，不替代后续版本验收。

## 复用与证据

运行层继续直接复用已保存的 m417z Windows 11 Taskbar Styler 1.10，commit 162bca0d6b4fe4da2c63c21f17921a165d5870fa，GPL-3.0。属性解析、样式所有权、主题重新应用及卸载恢复保持原实现；本地新增尺寸策略、类型正确的行高适配、兼容门控；一次性确认请求曾于 0.8.60 移除，0.8.66 按最新要求恢复并补充关闭和迟到回调保护。没有另造整个切换器。

离线 XBF 工具：chausner/PriTools 765d4544fd48aaaebf848b5b1b24ade2cf1770a0、XbfTools dbeadcd75f30fb8dea3109039e0082854cb9a89d，均 Apache-2.0，保留在独立调查输出，不是生产依赖。

本次 REA/Ghidra 12.1.4 静态分析核对最少行分支与 24 DIP 行间空间；未执行目标 DLL，没有将反编译地址用于运行挂钩或复制 Microsoft 源码。完整 Evidence 及限制在任务 outputs/switcher-layout-20261018/native-layout-evidence.json。当前精确 PDB 请求返回 404，使用的是 trace 字符串交叉引用与反编译证据，不是调试符号。

Apple Mission Control 单层窗口总览仅作设计参考，公开说明不提供具体排列算法：https://support.apple.com/en-ie/guide/mac-help/mh35798/27/mac/27 。

## 验证与回退

0.8.53–0.8.56 的旧尺寸策略曾通过 1041 次断言，0.8.57 在此基础上改为屏幕比例上限，并增加单窗口放大、两/四窗口、竖长/超宽窗口、四种可用区域下 1–64 个混合比例窗口的边界、增项不放大与最大可用档位检查。实际检查数量及结果见 verification-latest.json。0.8.66 恢复两窗口确认策略及 21 项测试；同时执行热角输入、配置、可见性所有权及布局回归。编译和尺寸模型检查不等于真实原生布局及视觉验收。

实际验收分别检查：

1. 当前窗口数量下行列合理、所有项目完整可见、没有滚动；增减窗口后预览合理缩放。
2. 画布保持原生底板并随内容收拢，四边多余留白收紧，选中提示圈不可见，窗口及标题正常可见。
3. 仅两个窗口时从 A 触发右下角切至 B，移开后再触发切回 A；三个及以上窗口仍保持列表并可再次触发关闭。键盘直接 Ctrl＋Alt＋Tab 仍可手动选择和 Esc 取消。
4. Explorer 与四组件恢复稳定，配置保留。多屏、极端数量、受保护窗口和不同 DPI 不因一次单屏验收而宣称通过。

更新沿用既有冻结打包、ValidateOnly、Update/Rollback；用户已授权通过检查后直接加载。准确部署版本、备份与验收边界分别记录，不把编译、加载、后台 Ready 或旧版用户反馈互相替代。


## 0.8.69 首次显示门控

0.8.68 用户反馈互换正常但仍闪现，不能把 Normal 优先级视作呈现屏障。0.8.69 适配 L3r0y 的 Alt+Tab window delayer 1.2.0 的 DisplayAltTab 线程范围与 ShowWindow 接入方式；不直接延迟 ShowWindow，而使用公开 DWMWA_CLOAK 暂缓呈现，以继续原生激活和 XAML 焦点处理。具体能否避免可见帧必须实测。

仅匹配：本进程本线程 XamlExplorerHostIslandWindow、首次显示、尚有效的右下角一次性请求，且原窗口无 APP 遮蔽。原生 DisplayAltTab 随后有 IsCloaked(false) 调用，因此仅在已接管的同一 HWND 上暂缓 DWMWA_CLOAK=false；其他窗口和属性直接放行。不枚举桌面 HWND，不替换原生窗口列表。

多项/一项、鼠标保护或 Invoke 异常立即恢复；原生 SW_HIDE 后恢复，200 ms 单次线程池计时作为失败兜底，同时取消迟到自动确认。计时器仅在处理请求时武装，无空闲轮询；停止先禁用、恢复属性并撤销计时，再排空回调和关闭句柄。宿主属性标记防止 HWND 重用误清理；原有 APP 遮蔽不接管。DLL/跳板寿命仍交给既有运行层。

内部入口只认 twinui.pcshell.dll 10.0.26100.9278，SHA256 8BA440EDED430A2417692A98B34695A12AD0EE000333DFED07E0C2723E17111C，DisplayAltTab RVA 104492。来源为微软匹配 PDB，GUID 9507F3DA-D47F-5F02-6569-FC50273C40D7、DBI age 1（public PDB 信息流 age 2）；已核对 PE 节映射及本机反汇编。未匹配时只禁用去闪门控，不阻止 Dock。独立调查中 REA 分析超时并已关闭会话；符号和反汇编由本地只读 PDB/Capstone 工具获得，不能写成 REA 成功证据。

已知边界：定时器受线程调度影响，200 ms 是预定恢复期限，不是实时系统保证。后台计数和 hidden-window DWM 测试不证明真实切换零闪帧；必须核对两个窗口双向切换、键盘手动列表、三个及以上窗口点击和再次触发关闭。


## 0.8.70 复用宿主与原生关闭

0.8.69 用户仔细观察仍见闪现，只是减轻；只读记录为 36 次 native Invoke、仅 1 次显示门控。此前 Begin 拒绝 WS_VISIBLE 和已有 APP cloak，漏掉 Windows 反复复用同一宿主的路径。显示函数内先 IsVisible(true)，后 IsCloaked(false)，因此新版在后一个原生调用到达时也获取门控：仅在已核验 DisplayAltTab 线程范围、有效右下角请求、同进程同线程精确宿主类内生效。这一明确的原生解除意图允许接管已有 APP cloak；普通 ShowWindow 仍不能接管任意已有 cloak。首次显示路径和三个 hooks 保留，未扩大私有偏移适配。

Windows 关闭时请求 CLOAK=true，新版先执行原生请求，成功后撤销单次 watchdog、消费尚在的请求并移除属性，保留原生 cloak；清理不再额外写 false。原生请求失败时仍保留门控和 watchdog，按原生 HRESULT 返回错误。同一宿主被新请求替代时直接转移 ticket，避免先解除再遮蔽形成缝隙；旧回调核对新期限，不提前释放。停止时仅解除仍属本组件的暂缓显示；已经归还给 Windows 的关闭状态不再触碰。

59 项真实 DWM 隔离检查通过，包括 8 次同一个 WS_VISIBLE 且被 DWM 遮蔽的窗口重复打开/关闭、迟到回调、重复 ticket、原生关闭错误、超时回退和 Stop/resume。测试窗口未呈现、不激活、不发送输入；这些测试不替代真实切换器视觉和交互验收。计数新增 fromShow、fromUncloak、nativeClosed，供加载后区分两条入口。


## 0.8.70 实际验收（2026-10-19）

用户明确确认“没有闪现，互换正常”。同次后台记录 16 次 Invoke、16 次门控并全部释放；fromShow=1、fromUncloak=15、timeouts/errors=0。首次与复用入口均进入，和用户视觉/双向切换结果对应。nativeClosed=0，因此本次实际经过的关闭路径不能用于声称原生 cloak=true 分支已现场验收，该分支只有隔离回归证据。键盘手动 Ctrl＋Alt＋Tab、三项及以上、多屏和睡眠恢复本轮没有新增实际验收。


## 数量变化后裁剪调查（0.8.71）

用户在 0.8.70 复现：原生列表从 7 项减少到 6 项，卡片高 312→360 DIP，底板高 690.67→786.67 DIP，外层 insideCanvas 全部通过且 ScrollableHeight=0，但用户仍看到下部截断。原有外层尺寸不能证明内部裁剪正确。

0.8.71 仅在现有 RecordGeometry 内查一次命名 ScrollContentPresenter，读取公开 UIElement.Clip 的 RectangleGeometry.Rect、ActualSize，以及已有 ScrollViewer 的 ViewportSize/ExtentHeight，按 root 坐标统计 insidePublicClip。没有获取 Composition hand-off visual、没有 UI Automation 或逐层 16 级视觉遍历、没有新增事件或改写裁剪/布局/焦点。新 schema=4 只覆盖公开 XAML 裁剪，不代表全部原生 DWM 缩略图合成链。

0.8.60-diagnostic 曾出现点击回归并回退；其中 GetElementVisual 的因果关系未证明，仍明确不恢复该实现。本版仍需验证重排后读数及实际单击切换。0.8.70 两窗口无闪现已获用户确认，但不能扩大为本版新诊断无回归。


## 数量变化后的原生裁剪同步（0.8.72）

0.8.71 用户复现并确认单击切换正常。20:24:53 至 20:24:56 同一原生切换器从 3 项变 2 项：ScrollContentPresenter 和 Viewport 由 1081.33×816 变为 1532×528 DIP，公开 Clip.Rect 仍为 1081.33×816。insidePublicClip 从 3/3 变为 1/2，用户截图右侧 Edge 标题栏和预览一同截断，与旧裁剪宽度吻合。已证明该次是公开裁剪矩形未跟随视口，不代表所有历史截断均单独取证。

修复独立于尺寸选择和确认逻辑：复用原有 LayoutUpdated/Loaded 生命周期，确认同一 presenter 与 RectangleGeometry 对象，记录曾与视口一致的矩形。只有当前 ActualSize 与 ViewportSize 一致、旧矩形精确对应上次已确认尺寸、零原点、无 Transform 且本地尺寸适配仍有效时，写入该原生矩形的 Rect(0,0,currentWidth,currentHeight)。未知裁剪、对象替换、布局中间态、卸载/隐藏、高对比度退出和不支持的宿主不进行扩展。写入后读回，失败停止该 root 的修正并记录错误；不增加计时器、异步回调、UpdateLayout、焦点调用或 Composition hand-off visual。

原生 Clip 属性和 RectangleGeometry 对象始终保留，系统后续 Arrange 可继续更新该对象。本修正没有需卸载恢复的样式/绑定/替换对象；不得在卸载时写回已经过时的旧矩形。关闭时沿用原订阅撤销与弱引用生命周期。

实现参考 Microsoft microsoft-ui-xaml 1af995b98a02d86b1951c4babe35caa269a6d569（MIT）的 ScrollContentPresenter::ArrangeOverride/UpdateClip：原生边界矩形对应排列尺寸。它仅用于验证处理契约，不是当前 Windows.UI.Xaml 的逐版本实现证明；本地新增的旧尺寸识别、对象/视口约束与诊断属于适配代码。未复制该项目运行源码。

验收必须同时检查：持续打开的列表 3→2、7→6 等增大/换行，以及重新打开；预览完整、单击选择仍成功、两窗口右下角互换无闪现。后台 clipRepairs 和 insidePublicClip 只提供尺寸证据，不能替代用户视觉/交互确认。


## 裁剪对象生命周期修正（0.8.73）

0.8.72 实测裁剪仍在、单击与互换正常，clipRepairs=0：实际 1498×576，裁剪仍为上次 1470×408，insidePublicClip=0/2。不能把数学策略测试通过等同于原生对象识别通过。

无窗口的本机原生 XAML 测试证明：把 RectangleGeometry 交给 ScrollContentPresenter.Clip 后，释放局部 COM 引用，弱引用失效；再读 Clip 会获得新的 peer，虽然底层原生裁剪数据仍在。0.8.72 每次因此重置尺寸历史，无法入场修正。

0.8.73 对显示中的同一 presenter/geometry 保留强引用，使用原生产 RememberPublicClip/ReleasePublicClip 辅助函数；隐藏、根卸载/移除、宿主替换与停止时释放。对象真的被原生替换仍清空旧证据，未知裁剪仍不扩展。没有延迟回调，引用不跨隐藏保留。15 项原生 XAML 回归直接编译生产状态和辅助函数：复现旧弱引用失败，验证新引用稳定、旧尺寸被识别、原生矩形读写、系统替换不误改、隐藏释放和重复清理。测试不显示任何窗口或控制桌面，仍不替代真实切换器验收。


## 0.8.73 实际验收（2026-10-19）

用户确认“预览完整，单击和互换正常”。只读记录覆盖 7→6→5→4→3 与 4→3→2，两段裁剪修正计数分别达到 4、2；所有本版有效记录 insidePublicClip=count，clipRepairErrors=0。通过真实重排与切换，不以编译或后台状态替代。新版后台 0.5.59、组件 Ready；临时观察 PID 27540 已在 20:52:17 退出，640 次采样、21 次变更、读取失败 0。保留预检、更新回执、before-0.8.73-20261019-204612389 回退集和旧失败记录，冻结包未改写。

维护审查：新增状态在显示期间保留原生 peer，隐藏/卸载/宿主变化/停止释放；对象替换、未知裁剪与中间布局不修正；失败不无限重试。布局、输入/焦点、两窗口遮蔽实现未改，相关实际点击与互换已确认。验收不扩大为任意系统版本/显示器/DPI 均已测试。
