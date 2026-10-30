# 自动隐藏留缝的统一兼容

0.8.76 / controller 0.3.32 / backend 0.5.61。构建、部署、实际接口检查和用户视觉验收分别记在 verification-latest.json。

## 问题与适配范围

栖页 0.1.11 普通最大化的 DWM 外框覆盖 2560×1600，但客户区与 WebView 只到 y=1599。工作区本身为完整屏幕。Tao 0.37.1 的 WM_NCCALCSIZE 在发现自动隐藏任务栏时按边扣除 1px；Chromium 也查询自动隐藏边，并有状态回退及客户端缓存。

本版在 Dock 宿主统一调整外部 AppBar 查询，不逐个修改或注入应用。适用于遵循这类查询的留缝；应用自己绘制的边框、硬编码间隙和未知客户端行为不保证消失。已有窗口可在正常还原后重新最大化时刷新几何，本版不主动调整用户窗口。

## 当前机制

1. 原生自动隐藏状态、注册、工作区、隐藏与展开动画继续由原运行层负责。保留事件恢复、主题、菜单及拖拽等责任。
2. CTray::_OnAppBarMessage 先调用原生实现，再只调整 ABM_GETSTATE 的 ABS_AUTOHIDE 位，以及 GETAUTOHIDEBAR/EX 返回的、确实由 LeanDock 管理的底边任务栏。其他边、外部 AppBar、所有写请求及未知线格式透传。
3. 重要纠正：序列化请求 +0x38 是共享内存的接收进程 PID，不是发送者。早期 0.8.75 的同进程隔离实验不足以识别这点；真实 API 复核未通过，因此该版没有完成验收。当前内部查询使用自有 HWND 标记，并验证窗口类和所属 PID，绝不依赖该 PID 字段识别调用者。这是内部路由，不是安全边界。
4. 本机 shell32 还会直接读 Shell_TrayWnd 的 WindowOnEdge:%08x:%1u 缓存。仅把当前 Dock 底边对应的缓存暂时设为系统已经使用的“空边”值 1；其他属性不改。初次同步与后续原生 SetPropW 写入均处理，不轮询缓存。最多记录32项，超出容量保留原生结果。
5. Explorer 自己的 SHAppBarMessage 三种只读调用、以及控制器查询，走原生消息入口并携带自有窗口标记，绕开对外缓存；原始 APPBARDATA 不修改。内部真实自动隐藏和注册仍然可读。
6. 只在控制器公开 Shell 适配就绪、隐藏组件活动、Dock 几何有效时启用。停止或控制器撤回 readiness 后恢复缓存；只恢复仍持有本组件值的属性，后续不属于 Dock 的原生写入不得被旧备份覆盖。停止门控与缓存写入共用互斥保护，晚到请求透传。现有更新器负责完整组件启停。
7. 普通最大化客户区精确铺满显示器时，沿用 NonRudeHWND 与 ITaskbarList2，避免被当作真正全屏压住 Dock；不调用应用最大化或尺寸调整。必须有普通可缩放标题样式，排除 popup/tool/topmost；Tao 真正全屏会撤销这些样式。此处不恢复用户已撤销的“启动默认最大化”功能。

## 版本与证据

- Explorer SHA256 EEE089FE6F927C7F0D51B0ABD9D70C094157E33673AD760713EC7B1CEA18EE11；PE 时间戳3202867519、映像3424256。匹配微软 PDB GUID 1D666F27BA2F85885888D3E2B94B0FD8、age1；CTray::_OnAppBarMessage RVA0x603F0，区间[0x603F0,0x605B0)。
- shell32 SHA256 C55A5F3B6EF059871FE2A0C8D82F086E0998D1154023E3ECF49698B97F9341F6；时间戳292758140、映像7979008。导出 SHAppBarMessage RVA0x2A770。静态指令确认 GetWindowThreadProcessId 接收者、缓存名、空值1和 WM_COPYDATA 回退；实际系统 API 的私有桌面实验独立确认这些行为。
- 任一模块身份不匹配时跳过该可选适配，保留原生行为。不得仅更新哈希继续使用旧地址或格式。
- Explorer REA 静态证据 ev_bc3c5b349ea785d28b66206c0890b01d7edc2aa6aca13fcbb6b0a525eecad086。shell32 的 REA 整体分析在330秒超时，已关闭并确认无残留；不称其成功。shell32 的依据是导出定位、PE/Capstone 指令和真实 API 对照实验。
- 操作记录、原始探针与静态输出在本任务 work/edge-unified-20261019；没有把微软二进制或 PDB 分发进安装包。

## 验证与边界

测试使用永不显示或切换的私有桌面、自己的隐藏 HWND。缓存集成测试直接编译生产兼容代码，调用本机真实 SHAppBarMessage；覆盖缓存绕过消息、内部查询、控制器就绪撤回、后续原生更新、他方值不覆盖、退出恢复和焦点不变。策略测试覆盖未知协议、其他边、写请求、最大化与真正全屏样式。

部署前真实外部 API 为 state=1、底边 HWND 非空，隐藏 Tao 几何夹具客户区2560×1599。0.8.76 部署后已核验外部 state=0、底边及扩展查询均为空、内部自动隐藏=1、工作区不变、夹具客户区2560×1600，前台焦点未变化。夹具结果与实际应用验收分别记录。

实际验收：2026-10-20 用户针对“栖页还原后重新最大化，再从底部正中唤出 Dock”的问题，明确反馈“白边消失，Dock 唤出正常”。同期只读窗口记录显示栖页主窗口普通最大化，客户区与 WebView 为完整2560×1600；底部正中命中原生任务栏。四组件后台 Ready，用户配置哈希保持不变。

GPT 等其他应用本版未逐一重新做视觉验收；真正全屏、多个显示器、混合 DPI、32位程序、睡眠恢复及全部应用框架也未完成本版实际验收。真正全屏样式排除已有策略测试，但不等于真实 F11／游戏场景通过。Ctrl＋Alt＋Tab 布局与点击代码本轮未修改。

## 来源与回退

- 保留 m417z Taskbar auto-hide when maximized 1.2.6 / GPL-3.0，upstream/hide.cpp。
- Tao 0.37.1： https://github.com/tauri-apps/tao/blob/tao-v0.37.1/src/platform_impl/windows/event_loop.rs
- Chromium autohide 查询： https://chromium.googlesource.com/chromium/src/+/refs/heads/main/chrome/browser/ui/views/chrome_views_delegate_win.cc ，审阅 blob a376c75811adb7c9c2f9f7dc663193b819b86f0f。
- 原生 Shell 合约： https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow

维护入口为 hide-appbar-latest.inc、appbar-query-policy-latest.h、client-edge-latest.h 和生成器。稳定回退候选为 before-0.8.75-20261020-001224201（内容是原0.8.74）；先核验完整回退链与当前文件哈希，再通过既有 Rollback 流程恢复完整四组件并保留用户配置，不得绕过事务冲突检查。0.8.76 首次部署的直接前一备份包含未完成验收的0.8.75，不能当作已知稳定基线。源码原备份位于本任务 work/edge-unified-20261019/before-0.8.74。本次没有执行回退。
