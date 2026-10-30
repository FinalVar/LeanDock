> 历史方案，2026-10-23 已退出运行。用户已选择恢复原生 Dock，托盘、应用操作与触发角恢复通过。当前维护线为根目录原生0.8.79；这里只保留0.9.8源码、验收和可回退材料。时间键和悬停控制已按局部边界适配到 aggregate，不再运行独立 Dock/guard。

# LeanDock 0.9.8 主题入口纠正交接

## 当前结果

2026-10-22 13:43:40，通过既有 Update 事务部署 0.9.8。13:43:58 的独立只读核验确认 60 个新包文件一致，普通用户 UI（当时 PID 4388）就绪、ControlPanelVersion=3、主胶囊仍为 72×46 DIP。配置、计划任务 XML、四个原生配套二进制未变。当前状态为 DeployedThemeAcceptedHotspotPending。用户已确认“深浅切换和 Dock 恢复都正常”；本项主题往返、对应壁纸与 Dock 配色恢复验收通过。

UI SHA-256：D8EE9896D140FB9791B14D5B08D202F7EB7FC2595DC780C79EEC8B409167A6A6。

用户明确纠正 0.9.7：“我要之前那个dock的夜间模式，你这是默认的夜阑模式。”此前把需求做成 Windows 暖色护眼属于助手理解错误。源码与 eksight 历史用户反馈共同确认：旧 Dock 的功能是深色/浅色完整主题切换，旧按钮名称为“深色主题”。本轮没有继续扩展暖色护眼功能。

## 当前行为与复用边界

- 悬停面板原第二行左侧改为“深色主题”。主体切换深浅，右箭头打开 Windows 主题设置；移动热点位置与行为保留。
- 直接隐藏启动正式目录原有 latest.exe --theme dark/light，继承普通用户权限，不发起 UAC，不启动第二个常驻控制器。
- 完整复用旧程序的内置 dark/aero 主题、对应 img19/img0 壁纸、应用及系统配色、主题通知、互斥、失败条件回退和完成验证。这不是仅写两个配色值的新实现。
- 原始主题程序的私有 IThemeManager 调用留在短时进程中，未引入新 UI；themeui.dll SHA-256 继续为 2AA99876194E8EE59D806A92DD740348279B6F1B704B4AE5A4E6BDB89552CD51。
- 新适配器另核验原控制器 SHA-256：6162DF641B87F3904EFAD17C99DDAD06CB68296898C1070B958056A64859044C。以后重建控制器必须核对适配器中的 pin，不能直接删掉校验。
- 复用来源：本仓库 include/theme-latest.h、theme-notify-latest.h、src/quick-theme-latest.cpp 及控制器分派；上游参考 tinodin 的 Auto Theme Switcher 1.3.2（GPL-3.0），Windhawk mods 提交 a5c9b87ba0824d826d6c1a95f6a497cb48d2dc66，保留源码 upstream/theme-switch.cpp。原作者与差异见两级 NOTICE。
- 原生 QuickTheme 的 rootReady/tileInstalled/buttonReady=false 是既有局限，仍记录。新面板直接调用同一个主题工作程序，不依赖旧原生卡片挂接；不能因此宣称旧卡片本身已经恢复。

## 生命周期与失败行为

新面板沿用 Metrics 工作线程；打开时检查可用性，显示期间读取实际配色。混合应用/系统配色明确显示为混合；状态未知或依赖不匹配时不允许切换。真实点击才创建工作进程。

待执行时防重复，50ms 非阻塞检查进程完成；退出码为 0 且实际配色等于目标，才记成功。面板隐藏后允许已经发出的主题应用完成；没有新常驻线程。卸载或 30 秒等待超时仅关闭等待句柄，不强杀正在应用系统主题的进程，原有命名互斥继续挡住并发请求。系统调用异常卡住时不能承诺工作进程必定按时退出；超时显示失败，不伪报成功。

0.9.7 的 nightlight-latest.h、nightlight-fixtures-latest.h 和 MIT 许可证已移入本任务 retired-v0.9.7，最新源码不编译、最新包不分发这条暖色护眼实现。此前正式安装留下的无引用源码/许可证与回退集可能仍保留，不作为当前运行依赖。没有删除恢复材料，也没有自动还原用户选过的护眼状态。

本次工作前后，Windows 当前主题、壁纸、两项配色值以及 Night Light state/settings 哈希一致。新代码没有写 CloudStore；没有以验证为由改变主题或开关热点。

## 验证证据

- 构建通过；115 项现有策略、123 项面板/控制模型、23 项主题状态/完成判定/禁用条件检查通过。
- 普通用户只读探针：读到当前浅色、热点关闭且可用、音量 16、亮度 100、Wi-Fi 开启、蓝牙关闭。这些只是当时快照。
- 探针写入数 0；隐藏后详情/设备读取计数不增长，重复 Stop 正常，退出等待 stopMs=0。
- 同一生产绘制函数生成 100/125/150/200% 离屏图；本轮查看 100% 和 150% 图，文字、按钮和原滑条无重叠。离屏图片不是桌面点击或主题实际效果的验收。
- 原有图标、胶囊电量环、协议、数据、Composition、看门狗、托盘检查通过。热点后端代码块与 0.9.7 逐字一致。
- 首次托盘检查仅 foregroundPreserved=false，其他托盘添加、删除、原接收器恢复均成功。该检查只比较前后前台窗口，不能区分自然用户切换与其他来源；没有足够证据给它确定归因。原始失败证据保留，复核全套通过，第二次 foregroundPreserved=true。未修改测试门槛。
- Update -ValidateOnly 通过，正式 60 文件匹配；Dock/托盘、Start/Search、触发角、快速设置定位、底边兼容后台就绪。UI 普通用户，原控制器仍按原任务权限运行。
- 前一版 0.9.6 音量、亮度、静音/恢复、拖动保持已由用户确认；不把旧验收扩大成新版主题和热点已通过。
- 未使用 Computer Use、输入模拟、主动切换焦点、屏幕抓取或系统防护改动。

用户已在 0.9.8 确认“深浅切换和 Dock 恢复都正常”（call_b173d88903e74c879da431a5f009608a）；对应问题同时核对了旧版配色与壁纸切换。主题往返与 Dock 恢复通过，不扩大为每个第三方应用的主题兼容保证。热点真实启停、设备接入和共享联网仍未测试。未测试其他 Windows 组件版本或系统异常下的全部生命周期。

## 维护与回退

源码：LeanDock/capsule。正式部署：C:\Users\x\Documents\Codex\Tools\NativeTaskbarDock。

源码修改前备份：本目录 before-v0.9.8-20261022-133919。用户既有未提交/未跟踪文件保留，未 Git 提交或推送。

正式回退集：C:\Users\x\Documents\Codex\Tools\NativeTaskbarDock\backups\capsule-20261022-134319（更新前 0.9.7）。仍使用 maintenance/capsule-latest.ps1 的 Rollback，不手动替换宿主或按 latest.exe 名批量结束进程。

入口：capsule/build-latest.ps1 → test-user-latest.ps1 -DeferVisualAcceptance → package-latest.ps1 → 包内 Update -ValidateOnly（明确 Destination）→ install-latest.ps1。本任务 verify-latest.ps1 可重新做部署只读核验。

同目录 verification-latest.json、technical-validation-latest.json、appearance-preserved-latest.json、theme-worker-compatibility-latest.json、native-tray-first-focus-changed.json、native-tray-latest.json 和测试日志保存本轮边界与证据。进程号、状态与哈希均应在后续更新前重新核验。
