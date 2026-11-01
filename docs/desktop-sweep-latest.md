# 左下角回桌面：窗口不再一起冒出来

适用版本：0.8.99 / 控制器 0.3.42 / 后端 0.5.84。

代码分两处：
- 控制器：include/desktop-sweep-latest.h，接入在 build/adapt-hotcorners-latest.py 末尾一段，生成 src/hotcorners-latest.cpp。
- 资源管理器侧：src/desktop-restore-latest.inc，挂在 src/runtime-latest.cpp 的核心挂钩里。

两边共用的标记在 include/desktop-sweep-mark-latest.h。

## 问题

用户反馈：左下角回到桌面后，点 Dock 上某个应用、或双击桌面上的文件夹，希望只出来这一个，结果终端也跟着冒出来。全屏的终端会这样，不全屏但无标题栏、铺满屏的终端也会这样。

## 原因

触发角发的是 Win+D。Win+D 只最小化带最小化按钮的窗口。没有最小化按钮的窗口只是被抬到前面的桌面（Progman）盖住，例如：
- F11 全屏的 Windows Terminal，样式 `0x94000000`（WS_POPUP）；
- 无标题栏、铺满屏的 Windows Terminal，样式 `0x14840000`。

任何窗口一被激活，桌面就降回最底层，这些窗口重新露出来。

## 走过的路

- **0.8.94：最小化被盖住的窗口。** 用户反馈“点回终端变成非全屏”。原因在 Windows Terminal 源码（microsoft/terminal，src/cascadia/WindowsTerminal/IslandWindow.cpp）：全屏时收到 `WM_SYSCOMMAND SC_RESTORE`，它会退出全屏并返回，不恢复窗口。任务栏和 Alt+Tab 用 `SwitchToThisWindow` 恢复最小化窗口，而这个函数发的正是 SC_RESTORE。
  - 实测：`SwitchToThisWindow` 之后，样式由 `0xB4000000` 变成 `0x34840000`，窗口仍是最小化，和点 Dock 的表现一样。
  - 实测：`ShowWindow(SW_RESTORE)` 恢复，仍是 `0x94000000`、0,0,2560,1600。
- **0.8.95 / 0.8.96：不最小化，改为用了别的窗口后压到桌面下面。** 这条路不成立。桌面窗口始终是最底层的窗口：屏幕外测试窗口用 `HWND_BOTTOM`，或者插到 Progman 之后，结果都停在 Progman 上面一层。桌面被抬起时，窗口可以暂时在它下面；资源管理器一把桌面放回最底层，窗口就又露出来。实测打开开始菜单后，终端停在第 143 层，桌面在第 144 层。
- **试过但没采用：挪到屏幕外，点它时再挪回来。** 点 Dock 回来时全屏不变。但关掉刚打开的窗口时，Windows 会把前台交给这个看不见的终端，打字会进到看不见的窗口里。
- **0.8.97 / 0.8.98：真正最小化并打标记，资源管理器里直接恢复。** 实机上点 Dock，终端到了前台却仍是最小化。0.8.98 的日志查清原因：用户的 Windows Terminal 以管理员身份运行（高完整性级别），资源管理器是中完整性级别。资源管理器里 `ShowWindowAsync` 和 `RemovePropW` 都被 UIPI 拒绝（错误 5），只有 `SetForegroundWindow` 和 `SwitchToThisWindow` 的系统内部路径能用。0.8.98 拒绝后退回系统路径，于是又退出了全屏。控制器以高完整性级别运行（标记就是它打的），所以恢复要交给它。

## 0.8.99 的做法

**控制器**
- Win+D 后，等桌面在前台稳定 120 ms，把 z 序在桌面之下的窗口最小化，最多 16 个。用的是 `ShowWindowAsync(SW_SHOWMINNOACTIVE)`，不发系统命令。
- 选哪些窗口：可见，未最小化，不是 owned、工具、置顶、外壳或游戏窗口，并且在某个显示器上。不看尺寸，也不看有没有最小化按钮。
- 每个被最小化的窗口打上窗口属性 `LeanDock.DesktopSweep.v1.Minimized`，值为 1。
- 前台变化的钩子（WinEvent，EVENT_SYSTEM_FOREGROUND，进程外回调）在回桌面期间挂着，之后只要还有带标记的窗口就继续挂着，标记全部清掉后才卸下。只靠事件，不轮询。
- 带标记的窗口在仍是最小化时成为前台：去掉标记，用 `ShowWindowAsync(SW_RESTORE)` 恢复，不发 SC_RESTORE。实测 0–32 ms 内完成。
- 标记什么时候去掉：
  - 上面这种恢复时；
  - 这个窗口以别的方式回到前台、已经不是最小化时；
  - 下次碰这个角时，已经不是最小化的窗口；
  - 在桌面上再碰一次角恢复时；
  - 控制器停止时全部去掉。
- 在桌面上再碰一次角（中间没用别的窗口）：Win+D 收回桌面，被最小化的窗口用 `ShowWindowAsync(SW_SHOWNOACTIVATE)` 按原 z 序放回，原来的前台窗口恢复并激活。

**资源管理器侧（后端）**
- 挂接 user32 的 `SwitchToThisWindow`（任务栏 Taskbar.dll、资源管理器本体、Alt+Tab 所在的 twinui.pcshell.dll 都导入它）。
- 目标窗口仍是最小化、并带有值为 1 的标记时：只调用 `SetForegroundWindow`，由控制器在前台变化时恢复。最多等 250 ms，看窗口是否已经不是最小化。
  - 已恢复：返回，不再调用系统函数。
  - 未恢复（控制器停了或卡住）：调用原来的 `SwitchToThisWindow`，窗口照常出来，只是 Windows Terminal 会退出全屏。
- 窗口卡死（IsHungAppWindow）时不等，直接交给系统。
- 其他调用原样交给系统。
- 这个挂钩属于核心挂钩事务，随 Dock 一起停用和恢复。

**与“压到桌面下”相比**
- 窗口是真正最小化的：关掉别的窗口时，前台不会落到它身上；任务栏、Alt+Tab、任务视图看到的都是最小化状态。

**状态与日志**
- 控制器状态 `hotcorners/latest-status.json` 的 `desktopSweep`：sweeps、minimized、restored（再碰一次角放回的）、returned（成为前台时恢复的）、ended、marked、open。
- 后端状态 `latest-backend.json` 的 `desktopRestore`：calls（全部 SwitchToThisWindow 调用次数）、restored（只激活、由控制器恢复的次数）、fallbacks（等不到、交回系统的次数）。
- 热角日志 `hotcorners/latest.log`：desktop-sweep、desktop-sweep-ended、desktop-sweep-returned、desktop-sweep-restored。
- 后端日志 `latest.log`：每次接管一行，`desktop-restore shown|fallback hwnd=… activated=… waited=…ms`。

## 范围与边界

- 只接管左下角发出的 Win+D；键盘 Win+D 仍是 Windows 原来的行为。
- 游戏（与全屏策略同一判定）、置顶、工具和 owned 窗口不碰；远程控制客户端照常处理。
- 只有经过 `SwitchToThisWindow` 的恢复路径能保持全屏。实测 Dock 和 Alt+Tab 走这条路；任务视图未验证。Windows Terminal 自己的全局唤出（quake 或 summon）在它进程内部发 SC_RESTORE，仍会退出全屏。
- 控制器停止时标记被去掉。之后再从 Dock 恢复，就和普通最小化一样，Windows Terminal 会退出全屏。
- 控制器的 UIPI 白名单没有增加消息。资源管理器和控制器之间只通过窗口属性（读）和前台变化来配合。

## 验证

- tests/desktop-sweep-latest.cpp：58 项。覆盖：
  - 筛选规则；
  - 资源管理器侧只接管“最小化且带我们的值”的窗口；
  - 成为前台时：列表里、最小化、带标记的恢复；已回来的或值不是我们的移出列表；不在列表里的不管；
  - 钩子在没有回桌面会话、但还有最小化的带标记窗口时保留，标记清空后卸下；
  - 哪些前台会结束回桌面状态；
  - 按原 z 序放回；
  - 标记的设置、接管、去除，别人的值不动，关掉的窗口被丢弃，停止时全部去除；
  - 计时器只处理自己的 id，且有上限。
- 控制器构建测试：hotcorners 138 项、配置 37 项、client-edge 108 项，全部通过。
- 部署后的实机结果见 verification-latest.json。

参考的公开接口：ShowWindowAsync、SetForegroundWindow、IsHungAppWindow、SetPropW / GetPropW / RemovePropW、SwitchToThisWindow、SetWinEventHook（EVENT_SYSTEM_FOREGROUND）。Windows Terminal 的 SC_RESTORE 处理只作为行为参考（MIT）。两处代码都是本地新写，沿用后端已有的 MinHook 挂钩链，没有复制第三方代码。
