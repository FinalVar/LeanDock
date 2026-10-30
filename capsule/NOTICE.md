# 来源与边界

本目录是 LeanDock 的胶囊重构工作，遵循仓库 GPL-3.0。

- 0.9.6 控制面板沿用本项目 Popup、Metrics、Composition、Direct2D/DirectWrite
  运行层。controls-latest.h / control-panel-latest.h 是公开系统 API 的本地适配，
  非外部控制中心源码的直接移植。参考并审阅了微软 MIT 示例的真实设备调用：
  Windows-classic-samples 提交 434f6002bdf9cf9829406c3ff2b33387982d6168 的
  Samples/Win7Samples/multimedia/audio/EndpointVolume/endpointvolumechanger.cpp；
  Windows-universal-samples 提交 4eb2fcb499c5bc549e918920cfd2b64396a650d9 的
  Samples/RadioManager/cs/RadioModel.cs。示例未作为运行依赖，没有复制其完整程序。
  原文及 MIT 许可快照保存在 outputs/dock-control-center-20261022/references。
  本地差异是普通用户 Win32 接入、同一采样线程内合并命令、异步超时/取消、
  设备状态读回、滑条捕获及卸载清理。未修改系统 DWM、无线服务或驱动配置。
  亮度使用公开 WmiMonitorBrightness / WmiMonitorBrightnessMethods，按活动屏幕
  InstanceName 配对；不可用时禁用滑条，不模拟亮度遮罩。

  https://github.com/microsoft/Windows-classic-samples/tree/434f6002bdf9cf9829406c3ff2b33387982d6168
  https://github.com/microsoft/Windows-universal-samples/tree/4eb2fcb499c5bc549e918920cfd2b64396a650d9
  https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmisetbrightness-method-in-class-wmimonitorbrightnessmethods
  https://learn.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.getradiosasync
  https://learn.microsoft.com/en-us/uwp/api/windows.devices.radios.radio.requestaccessasync

- 0.9.5 的 time-key-latest.h 是用户选定 HTML A 版的本地材质/布局适配：
  用现有 Direct2D/DirectWrite 运行层绘制短胶囊电量环、时间与日期。
  没有引入新第三方运行层；原 Composition 表面、DPI、主题、隐藏、
  通知注销及卸载责任继续复用。time-key-tests-latest.h 仅在显式测试参数下
  对私有纹理进行离屏读回，不捕获用户桌面。

- effects-latest.h 直接提取本仓库已保存的 m417z Windows 11 Taskbar Styler
  1.10 中 IGraphicsEffectD2D1Interop、GaussianBlurEffect、ColorMatrixEffect；
  原始源码及许可继续保存在 upstream/styler.cpp 与 LICENSE.txt。效果的
  属性、COM 所有权和错误返回保持原实现；宿主改用公开 HostBackdropBrush。
- Win32 DesktopWindowTarget、DispatcherQueue 和宿主背景来自 Windows SDK
  公开接口；没有借用私有偏移、挂钩 DWM 或捕获桌面截图。
- tray-latest.h 是 cairoshell/ManagedShell
  855222035e927910ca53fd3215caef056337a048 的 TrayService.cs、NotifyIcon.cs
  与相关互操作结构的 C++ 协议/生命周期移植。作者 Cairo Desktop / ManagedShell
  contributors，Apache-2.0，完整许可证保存在 licenses/ManagedShell.txt。
  保留 32 位 wire handle、956 字节结构、NIM_ADD/MODIFY/DELETE/SETVERSION、
  GUID 标识、版本 3/4 回调、CopyIcon 所有权、Explorer 转发与重建广播。
  本地差异：普通用户 Win32 窗口、现有 20 ms 循环维护接收窗口顺序、
  200 ms 有界转发、源程序菜单存活判定与数据冻结边界。
  不复用上游 WPF 显示层，不声称 C# 运行层原样复用。
- apps-latest.h 的 IPinnedList3 ABI 对照 Chromium chrome/installer/util/
  taskbar_util.cc（下载快照保存在任务 work 目录）和本机 shell32.dll
  10.0.26100.9549 实测。此接口没有公开兼容承诺；只开放枚举，
  写入测试不通过，未使用私有注册表二进制重写、进程身份伪装或注入固定列表。
- 0.9.1 直接复用本仓库 include/icon-optics-latest.h 已验收的 alpha 边界算法，
  2 倍源像素预算沿用 include/icon-resolution-latest.h 的策略；Shell 图像工厂、
  WIC 转换与无裁边 Composition surface 是新宿主适配。间距降档移植自
  include/cards-dashboard-latest.h 的 AppWidth；底板、图标和圆角值来自
  baseline/settings-latest.json、baseline/styles-latest.json，覆盖 handoff 初始尺寸。
- 其余原有 LeanDock 图标读取、系统数据和取消机制作为设计参考；新 HWND、
  合成树、显示策略、看门狗、采样线程及维护事务属于本地适配。
- 构建与测试文件不是性能/交互通过的声明；实际证据单独保存在
  verification-latest.json，不能扩大旧版的人工验收。
- 0.9.2 继续保留上述光学补偿与 Shell 图标身份；公开 WIC Fant 缩图和物理像素
  对齐属于新宿主适配。256 像素是请求档，窗口属性和离屏检查另外记录返回尺寸。
  开始菜单身份检查提取自本仓库 shell-panels-latest.h 的既有受信路径校验，
  增加 DWM cloak 检查。ManagedShell 同一快照的 AppVisibilityHelper.cs 已审阅，
  本次采用现有宿主身份/窗口状态，不直接移植其 COM 事件运行层。
  触发角的短时关闭等待与超时取消是本地新增，源生成器保留全部旧输入保护。
- 原生配套版本继续使用原 LeanDock 中的 Windhawk Taskbar Styler 适配、
  NativeHotCorners、MinHook、开始菜单及快速设置组件。具体原作者、原
  提交、各许可证和旧本地差异沿用仓库原 NOTICE.md。companion-adapt
  的新增差异是启动范围收窄、真实 Explorer 查找、普通令牌子进程交接和
  生命周期通知；并未把成熟运行层重新实现一遍。

- 0.9.3 的 Start/Search 统一显示识别复用上述身份检查；Search 定位直接适配
  本仓库 quick-position-latest.cpp 与 quick-host-latest.h（GPL-3.0）的线程钩子、
  子类、进程所有权、退出等待、窗口销毁和原值恢复代码。移除主题/XAML 操作，
  换为经本机 SHA-256 核验的 SearchHost 身份，位置策略与 Start 共用。
  不复刻系统搜索 UI，不增加常驻进程或轮询，不更改 Ctrl＋Alt＋Tab 的布局与点击。
- 同版补回原维护入口的精确 pinned-host 释放责任，适用于 Start DLL 的更新和回退，
  保留已有复制哈希、配置保护、普通用户子进程与组件恢复门控。
- WIC 修正来自本机资源实证和微软 alpha 合同；旧版将直通值错误声明为预乘，
  本版改为直通导入、最终输出一次预乘，不改写应用图标或添加锐化。

## 0.9.8 旧版主题功能与移动热点

- 用户于 2026-10-22 明确纠正：“夜间模式”指旧 Dock 的深色主题。当前新面板
  直接复用正式原生控制器的 --theme dark/light 工作进程，未改其实现或二进制。
  路径/哈希校验、隐藏启动、异步完成、只读模式状态、等待生命周期是新适配。
  旧机制包含 Windows 内置完整主题和对应壁纸，不能缩减成仅设置两个配色值。
  原逻辑与完整授权见根 NOTICE.md 的 Auto Theme Switcher 1.3.2 段落：
  tinodin / ramensoftware，GPL-3.0，提交 a5c9b87ba0824d826d6c1a95f6a497cb48d2dc66。
  本地 upstream/theme-switch.cpp 是保留源码；本次审阅 include/theme-latest.h、
  theme-notify-latest.h、src/quick-theme-latest.cpp 与控制器 --theme 分派。
  没有新增 Windhawk 依赖，也未复制旧 XAML 替换卡片的注入运行层。

### 已退役的 0.9.7 暖色护眼适配

下面保留历史来源归属。nightlight-latest.h、样本与 MIT 许可已归档到本次
outputs/dock-theme-correction-20261022/retired-v0.9.7，0.9.8 不编译或打包此功能。

- Kevin Xiao，win-nightlight-cli / win-nightlight-lib，MIT，固定提交
  0461529455f3594b9b9d6cc3d6c2e7a9736de9b5。
  https://github.com/kvnxiao/win-nightlight-cli/tree/0461529455f3594b9b9d6cc3d6c2e7a9736de9b5
  实际审阅 state.rs、cloudstore.rs、nightlight.rs、store.rs、bond/reader.rs、
  types.rs 和 fixtures.rs；nightlight-latest.h 是其状态/封装协议与边界检查的
  C++ 移植，nightlight-fixtures-latest.h 保留上游状态样本，完整 MIT 许可在
  0.9.7 包及本次退役归档中保留。没有引入 Rust、CLI 或持续监视进程。
  本地差异：只实现手动开关、限制已核对基础版本 26300、拒绝重复/乱序字段和
  非无损表示、4KiB/16层上限、写前比较与首次备份；不用温度/定时/动画写入。
  这是内部数据格式适配，不声称是微软支持的状态写入 API。
- Microsoft Settings Reference 对 Nightlight 的 state/source/timestampUTC/
  isSupported 语义给出说明；其 Cloud Data Store Reader 是读取工具，不能据此
  宣称公开支持直接写入 CloudStore。
  https://learn.microsoft.com/en-us/windows/apps/develop/settings/settings-common#nightlight
- 移动热点使用公开 Windows.Networking.NetworkOperators 接口合同，本地自行
  适配现有 Metrics 线程的异步等待、状态读回、超时和释放；不是照搬其他 Dock。
  普通 Win32 进程已在目标机只读验证 capability=Enabled、operational=Off；
  未为测试开启热点，能力与状态读取不等于 Start/Stop 或共享联网已通过。
  https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager.createfromconnectionprofile
  https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager.starttetheringasync
  https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager.stoptetheringasync

公开接口资料：

- https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle
  0.9.4 对 Search 的适配只交付查询与等待退出句柄，复用已有所有权验证与等待清理。
  已用真实 Search 的受限令牌在自有文件上检验只读授权前后及句柄查询，不将此
  隔离权限检查当作实际菜单视觉或点击验收。
- https://learn.microsoft.com/en-us/windows/win32/api/userenv/nf-userenv-deriveappcontainersidfromappcontainername
  从固定 CBS 包身份派生 SID，已与实际 SearchHost 令牌一致性对照；仅授权一个组件文件。
- https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-processidtosessionid
  该 API 的访问要求不能由预先交付的有限查询句柄满足。真实 Search 令牌复测得到
  错误 5；改为查询句柄对应的精确路径、存活状态和当前桌面控制器窗口的所属 PID，
  交付端仍验证同会话，不扩展控制器进程 DACL。

- https://learn.microsoft.com/en-us/windows/win32/api/wincodec/ne-wincodec-wicbitmapalphachanneloption
- https://learn.microsoft.com/en-us/windows/win32/api/wincodec/nf-wincodec-iwicimagingfactory-createbitmapfromhbitmap

- https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
- https://learn.microsoft.com/en-us/windows/win32/api/windows.ui.composition.interop/nn-windows-ui-composition-interop-icompositordesktopinterop
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook
- https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand
  SC_TASKLIST：公开开始菜单系统命令；本地适配只在真实时钟点击时投递到
  路径核验的 Explorer 主任务栏。ManagedShell 同版 ShellHelper.ShowStartMenu
  仍用模拟按键，不能照搬解决普通权限 UI 对高权限前台的输入限制。
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput
  UIPI 限制与当前问题相符，但未经此次复现轨迹确认，不把它记作唯一根因。
- https://github.com/cairoshell/ManagedShell/tree/855222035e927910ca53fd3215caef056337a048
- https://devblogs.microsoft.com/oldnewthing/20190425-00/?p=102443
  普通权限创建流程按该公开方案适配；加入固定启动路径、真实 Explorer 路径、
  同用户/会话/Medium 检查、挂起子进程核验及完整句柄清理。不使用关联令牌
  的 Identification token 创建进程，也不启用额外系统权限。
