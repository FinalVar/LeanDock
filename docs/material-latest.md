# 玻璃底色与四指手势

## 当前验收状态

2026-10-10 用户反馈 0.8.10「一样实时变透」，四指手势修复未通过。0.8.11 的 180 秒、2787 个有效快照覆盖了用户确认的真实故障：底色 ARGB 9EC8D1DE、主题及 9 级祖先的 XAML/Composition Opacity getter 不变，HostVisible 随宿主隐藏/显示改变。用户再次明确「当然测试了。就是变透，现在也是」，因此不能把该采样记为没有复现。

0.8.12 修正底色层级：原来的普通 XAML 层仍在 TaskbarBackground 的模板内，并未独立于原生背景子树。新层与 BackgroundControl 同级。这个结构问题已由源码和组件测试定位；它是否正是四指过程中变透的原因，用户已反馈「一样」，该版没有解决问题，不能再把层级调整记为根因。白色背景层次的视觉验收仍未确认。

src/material-probe-latest.inc 通过有时效限制、无跨进程指针的注册消息采集底色与最多 16 级祖先的 XAML/Composition Opacity、尺寸、可见性、加载状态，以及底色实际主题和画刷颜色。没有计时器、事件订阅或默认采样，外部读取脚本最多运行 180 秒。合成器属性 getter 可能返回动画基值，因此数值不变不能排除系统内部动画或替代画面。正常卸载清除采样属性。

本次核对了匹配本机 Taskbar.View 的公开 PDB、CreateBackgroundVisual/InitializeExpandAnimation 路径，以及 m417z Taskbar Background Helper 1.1（a20a1f7643ccfc30af34f684c2b78047b5f5eb91）。后者处理 Peek/MultitaskingView 下窗口背景策略，不是可直接移植的四指手势解决方案；没有引入其全窗口背景设置或额外事件线程。

继续核对了匹配本机 twinui.pcshell 的 VirtualDesktopSwitchAnimator、MultitaskingViewGestureHandler 和 CAppThumbnailWindow 代码；系统存在独立动画/缩略图路径，但尚未证明本例使用哪份任务栏背景。没有据此添加私有动画钩子、猜测内存偏移或更改系统手势设置。

0.8.10 将模糊和负责可读性的底色分层。当前反馈有两个现象：白色应用上 Dock 与背景融在一起；四指切换虚拟桌面的动画期间过透，用户明确说明松手立即恢复。这与 0.8.9 已通过的左下触发角旧帧残留分开记录。

## 实现与来源

- 保留上游 WindhawkBlur 的高斯模糊和饱和度效果，BlurAmount=12、TintSaturation=0.85；TintOpacity 和 TintLuminosityOpacity 均为 0，避免颜色保护与 backdrop 取样共用一个效果输出。
- src/material-latest.inc 在 TaskbarFrame.RootGrid 中将 Canvas#LeanDockMaterialHost 插在 BackgroundControl 之后、前景按钮之前，其中 Grid/Rectangle 提供主题底色。浅色 #9EC8D1DE、深色 #B3252C38；高对比度取系统窗口色。
- 装饰层复用现有布局通知同步 BackgroundControl 的实际尺寸，并绑定其边距和 Dock 缩放，以及 BackgroundFill 的圆角。没有原生背景控件作为祖先，不继承它的透明度/可见性/合成动画。共同的 TaskbarFrame 仍控制整个 Dock 的显隐。
- Canvas 在测量阶段不贡献期望宽度，避免把背景的 ActualWidth 直接绑定到 RootGrid 的普通子元素后锁住旧宽度，导致关闭应用时 Dock 无法收窄。没有额外窗口或逐帧重绘。
- 中间卡片的浅色底板 #A3FFFFFF、深色 #9E202732；所有页面共用，悬停与切页不改变底色。图标、字体、命中与点击入口未改。
- 借鉴保留的 NextKDE LiquidGlassSurface / DockThemeService 将合成器模糊、稳定底色与前景区分的设计；这是 Windows XAML 适配，没有移植 QML 或替换上游模糊算法。来源版本见 NOTICE.md。

## 生命周期与验证

装饰层、Canvas 宿主、源矩形和父 Grid 用弱引用保存，复用现有布局通知。在原生模板替换或元素失效时移除自己创建的宿主，再解析已知模板；不遍历其他应用的视觉树。发生异常时移除未完成的层，记录错误并停止在同一个宿主内重试，避免布局回调反复抛错。卸载时只移除自己的元素，绑定随元素销毁；不修改原生背景对象的生命周期。没有手势钩子、计时器、永久轮询、额外线程或强制重绘。

tests/material-latest.ps1 在测试自己创建的隐藏 XAML island 中调用生产 material-latest.inc，检查主题、原生背景父子关系隔离、绘制顺序、不拦截输入、缩放后边界、应用宽度收缩/增长、圆角、重复布局、清理与重建。这不等于测试 Windows 的真实四指手势。

LeanDock.Material.Ready/Theme/Color/Error 提供只读运行诊断，数值加一编码。Ready 要求装饰层已加载且有非零尺寸；它不能证明屏幕上的混合结果或真实触控板动画正确。代码路径、XAML 构造、浅深主题、实际手势必须分别描述证据。

0.8.9 的展开前准备与有限后续恢复保持原样，不能把它的触发角验收当作 0.8.10 的四指验收。Windows 在手势动画期间是否停用 backdrop 效果或调整了其他父级合成状态尚未证明；普通底色将可读性与该效果分开，但也不能绕过系统对整个任务栏的动画。若仍变淡，先读取层与父级状态，再决定后续处理，不提高背景刷新频率。

验证需要覆盖白色窗口、蓝色桌面以及四指持续滑动中途；松手后恢复只证明退出状态正常。保持 Update/Rollback 精确文件校验和配置保留，最新证据见 docs/verification-latest.json。

## 0.8.13 临时诊断

在既有显式采样消息续租的一秒窗口内，记录 Taskbar.View 的 CreateBackgroundVisual，以及 twinui.pcshell 的 CAppThumbnailWindow 背景／应用缩略图创建次数和时间。未采样时只经过原生函数，没有记录、线程或轮询。三个入口均以本机公开 PDB、PE 和 SHA256 固定；所有参数、原生返回值与异常原样传递。背景方法的 this/隐藏返回指针位置已对照两条 COM ABI 包装器与函数反汇编。停止时关闭采样租约，钩子由既有运行层停用；下一版确定路径后应移除不需要的临时诊断。该版没有修复手势渲染，不是新视觉验收请求。

## 0.8.14 手势缩略图适配候选

微软本机匹配 PDB 确认四指入口是 VirtualDesktopGestureWindow::Initialize，缩略图工厂为 twinui.dll 的 CSwitchThumbnailFactory::CreateThumbnail。0.8.13 的三个未命中临时钩子已移除。两处新入口均继续使用 SHA256 固定与既有钩子退出保护。

适配通过线程局部作用域限定到手势初始化，并检查源窗口类名、本进程所有权及 LeanDock 材质就绪标记。仅这份任务栏副本启用 3D、取消 ForceCvi；其他应用、任务视图和普通缩略图直接透传。原来的全部位置、尺寸、父对象、IID 与所有权保持不变。新调用失败且没有返回对象时重试原始参数；有返回对象时不擅自释放或替换。无截图缓存、无永久计时器、无输入模拟。

标志值已与本机 CDCompWindowThumbnail::SetEnable3D/SetForceCvi 的指令核对；组合参考 ALTaleX531/flip3d_comp（6109667ab3cf88c4354b71985660432fcc9c6ff2）。这是有边界的合成方式实验，还不能据此断言 CVI 是故障根因。原生返回成功和入口计数只证明代码执行，最终对比仍待手势像素观察。0.8.9 热角恢复逻辑未改。

## 0.8.15 手势任务栏副本保持实时（已证实无效）

0.8.14 已实测失败：真实手势命中 3D/CVI 适配，返回 S_OK、无回退，用户仍反馈“一样”。该标志修改已完整撤回，原生 CreateThumbnail 不再被挂钩。不能把该实验解释为已经找到 CVI 根因。

随后修正独立对照工具的矩形参数、合成表面绘制范围和层叠顺序。工具在真实 VirtualDesktopGestureSwitcher 显示约 500 毫秒后自动冻结两份实时任务栏副本，另外两份提前冻结；四组图像均保留了 Dock 底色与白色卡片。时间页的 15:48 与 15:50 区别证明前后图像并非同一帧。这里证明的是普通实时源和独立冻结副本没有丢失底色，不能推导所有 Freeze 都会丢失底色，也不能将独立工具当作原生动画的视觉验收。工具已退出，无新增常驻记录。

本机匹配 PDB 及原生调用序列确认：VirtualDesktopGestureWindow::Initialize 创建任务栏缩略图，设置矩形后，通过 IDCompThumbnail 的 Freeze 冻结；该函数在此构建下仅将 0x100000 提交给 DwmUpdateThumbnailProperties。GetSourceWindow 与 Freeze 采用相同接口指针。新适配仅在 Initialize 的线程局部作用域内、源 HWND 属于当前 Explorer、类名为主／副任务栏且 LeanDock 材质标记就绪时跳过这次 Freeze。实际源码调用已解析的 GetSourceWindow，不使用对象字段偏移。

这是一项针对原生动画早期冻结时序的候选改动，根因尚未最终证实。其他应用、任务视图、普通缩略图及停止后的所有调用均透传；位置、变换、父对象和生命周期完全由 Shell 管理。无截屏贴图、假动画、额外进程、持久对象或新计时器。实际物理四指手势仍须验证，手势结束前用户不需要操作其他窗口或回复消息。窗口属性只保存累计命中计数和时间，以便松手后读取。

适配器 26 项检查通过，覆盖目标识别、主／副任务栏、外部进程、未就绪标记、空参数、原生失败值、停止、跨线程隔离与异常展开。该检查不证明动画像素正确。0.8.9 已验收的触发角背景恢复代码和底色数值保持不变。

## 0.8.16 撤回无效手势实验

用户明确反馈 0.8.15 “仍然变透”，留存记录 FreezeSkipped=1，证明候选改动实际命中但无效。因此从生成器移除 Install/Stop，从材质诊断移除适配器 include，删除仅为手势实验增加的三个符号和两项模块依赖。系统原生缩略图创建、冻结和生命周期完整恢复；历史适配器与测试保留为未编入的失败实验材料。本版不声称修复四指动画。0.8.9 触发角恢复、材质数值和卡片行为保持原样。
