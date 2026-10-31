# 左下角回桌面：窗口不再一起冒出来

适用版本：0.8.95 / 控制器 0.3.39。代码：include/desktop-sweep-latest.h，接入在 build/adapt-hotcorners-latest.py 末尾一段（生成 src/hotcorners-latest.cpp）。

## 问题

用户反馈：左下角回到桌面后，点 Dock 上某个应用、或双击桌面上的文件夹，希望只出来这一个窗口，结果全屏的终端也跟着冒出来；终端不全屏（无标题栏、铺满屏）时也一样。

## 原因

触发角发的是 Win+D。Win+D 只最小化带最小化按钮的窗口；没有最小化按钮的窗口（F11 全屏的 Windows Terminal 为 `0x94000000`，即 WS_POPUP；退出全屏但仍无标题栏铺满屏时为 `0x14840000`）不被最小化，只是被抬到前面的桌面（Progman）盖住。任何窗口一被激活，桌面就降回最底层，这些窗口重新露出来。实测：回桌面后 Progman 在 z 序第 26 位、终端在其下且 `IsIconic=false`。

## 0.8.94 的做法和被否的原因

0.8.94 在 Win+D 之后把桌面下面被盖住的应用窗口用 `ShowWindowAsync(SW_SHOWMINNOACTIVE)` 最小化。用户反馈“左下角后重新点回终端，它变成非全屏了”。复现：F11 全屏的终端（`0x94000000`）被最小化后为 `0x34840000`，从 Dock 点回为 `0x14840000`、窗口矩形 0,0,2560,1600、DWM 边界 9,0,2551,1591，即 Windows Terminal 处理 SC_RESTORE 时退出了全屏。改用 `PostMessage(SC_MINIMIZE)` 结果相同。不能靠事后给应用发 F11 补救（那是向应用注入按键）。

## 0.8.95 的做法

- Win+D 后桌面在前台稳定 120 ms，记录 z 序在桌面之下、可见、非 owned/工具/置顶/外壳/游戏、且（有最小化按钮或盖满显示器）的窗口，最多 16 个。
- **有最小化按钮的**照旧最小化（Win+D 本来就该最小化它们，这里只是补上被漏掉的情况）。
- **没有最小化按钮的不最小化**。等用户用了别的窗口（前台变为桌面和任务栏以外的窗口），立刻把它们 `SetWindowPos(HWND_BOTTOM, SWP_NOACTIVATE…)` 压到桌面下面（“压下”）。之后每次前台变化，以及变化后 600 ms 内每 50 ms，检查压下的窗口是否又被 Windows 降下来的桌面露出，是则再压下。
- 压下的窗口被激活（点 Dock、Alt+Tab）或关闭后不再管；激活后保持原样，全屏仍是全屏。
- 在桌面上再碰左下角：Win+D 收回桌面，被盖住的窗口随桌面回到原位；最小化的窗口按原 z 序放回，原前台窗口恢复并激活。
- 只有回桌面会话或有压下的窗口时才挂一个 `EVENT_SYSTEM_FOREGROUND` 进程外钩子，没有轮询、线程或常驻进程；600 ms 的复查定时器只在前台变化后短暂存在。
- 控制器停止时清空记录，已压下的窗口留在原处，下次激活时照常回来。

状态文件 `hotcorners/latest-status.json` 的 `desktopSweep` 记录 sweeps、minimized、restored、ended、burials、buried、open；日志 `hotcorners/latest.log` 记录 desktop-sweep、desktop-sweep-ended（压下数量）、desktop-sweep-restored。

## 范围与边界

- 只接管左下角发出的 Win+D；键盘 Win+D 仍是 Windows 原来的行为。
- 游戏（与全屏策略同一判定）、置顶、工具、owned 窗口不碰；远程控制客户端照常处理。
- 压下的窗口仍在 Alt+Tab 和任务栏里，没有被隐藏或改变状态。
- 压下与激活之间有一个前台事件的延迟，新窗口出现的瞬间被压窗口可能闪一帧。

## 验证

- tests/desktop-sweep-latest.cpp：48 项，覆盖筛选规则、只最小化带最小化按钮的窗口、激活/关闭后释放、隐藏窗口不移动、计时器只处理自己的 id 并有上限、停止时释放钩子和记录。
- 控制器构建测试：hotcorners 138、配置 37、client-edge 108 项通过；生成的 hotcorners 与 0.8.94 只差状态行两个字段。
- 实机（用户同意自测）：F11 全屏终端 → 左下角 → 点 Dock 上的 WorkBuddy → 终端留在桌面下 → 点 Dock 上的终端 → 回到前台且仍为 `0x94000000`、0,0,2560,1600。部署后结果见 verification-latest.json。

参考的公开接口：ShowWindowAsync、SetWindowPos（HWND_BOTTOM）、SetWinEventHook（EVENT_SYSTEM_FOREGROUND）、GetShellWindow。本地新写，不复用第三方代码。
