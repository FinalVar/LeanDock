<div align="center">

# LeanDock

**把 Windows 11 的任务栏，安静地变成一条 Dock。**

不换掉任务栏，只让它更好看、更顺手。<br>
应用按钮、托盘、右键菜单，全都还是 Windows 原生的。

<img src="docs/assets/badge-platform.svg" alt="platform: Windows 11">
<img src="docs/assets/badge-cpp.svg" alt="C++23">
<img src="docs/assets/badge-version.svg" alt="version 0.8.83">
<img src="docs/assets/badge-license.svg" alt="license: GPL-3.0">

<br><br>

<img src="docs/assets/screenshot.png" alt="LeanDock 实拍：底部是玻璃 Dock，应用和托盘之间是时间键，鼠标停在时间键上时弹出详情面板" width="640">

<sub>实机截图：鼠标停在时间键上，弹出详情面板</sub>

</div>

<br>

## ✨ 亮点

<table>
<tr>
<td width="50%" valign="top">

### 🪟 原生，不是仿制
Dock 就是 Windows 自己的任务栏换了身衣服。拖拽、跳转列表、托盘菜单、通知，一样不少，也一样稳。

</td>
<td width="50%" valign="top">

### 🕐 一颗键，装下整个状态栏
时间、日期、电量挤进一颗小小的腰圆键。悬停一下，CPU、显卡、网速、音量、亮度全都在。

</td>
</tr>
<tr>
<td width="50%" valign="top">

### 🖱️ 四个角都有用
左上锁屏，左下回桌面，右下切窗口。鼠标一甩就到，不用记快捷键。

</td>
<td width="50%" valign="top">

### 🪶 轻到感觉不到
只有一个隐藏的小进程。详情面板用的时候才启动，鼠标一走就退出，不在后台偷偷采样。

</td>
</tr>
</table>

## 🕐 聚合时间键

应用和托盘之间那颗腰圆键：大字是时间，小字是日期，外面一圈是电量。

| 操作 | 效果 |
|:--|:--|
| 单击 | 打开开始菜单 |
| 右键 | 打开 Win+X 菜单 |
| 悬停 0.4 秒 | 弹出详情面板 |

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/keys-dark.svg">
  <img src="docs/assets/keys-light.svg" alt="电量环的四种状态" width="100%">
</picture>

<sub>电量环从顶部顺时针走一圈。低于 20% 变黄，低于 10% 变红；插着电时始终是正常色。</sub>

**详情面板里有什么**

- **看**：带秒的时间 · 农历 · CPU 和最近 60 秒趋势 · 内存 · 每块显卡各自的 3D 占用 · 网速 · 磁盘剩余 · 电量和剩余时间 · 正在播放
- **调**：音量 · 亮度 · Wi-Fi · 蓝牙 · 深浅主题（连壁纸一起换）· 移动热点

鼠标离开 0.25 秒，面板就收起。

## 🪟 Dock 本身

- **毛玻璃**：居中、圆角、实时背景模糊，跟着系统深浅色走。
- **会让路**：只有窗口真正挡住 Dock 时才藏起来；鼠标碰一下底边，它就回来。
- **不打扰**：来消息不会把 Dock 弹出来，只在图标下面亮一个小红点。
- **图标清楚**：按两倍分辨率取图，自动对齐大小和居中；应用多到放不下时，按钮会自动缩小。
- **托盘还在**：通知托盘留在 Dock 里，排成两行。

## 🖱️ 触发角

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/corners-dark.svg">
  <img src="docs/assets/corners-light.svg" alt="四个触发角：左上锁屏，左下显示桌面，右下切换窗口，右上未使用" width="100%">
</picture>

右下角打开的是 Ctrl+Alt+Tab 切换器：已经开着的时候再碰一次就关掉。每个角的动作和延迟都能在 `hotcorners/latest.ini` 里改，详见 [hotcorners/README.md](hotcorners/README.md)。

## 🧩 顺手的小改进

- **开始菜单**：“全部应用”不再按字母分组，排成一张紧凑的网格，高度跟着内容走。
- **弹出位置**：开始菜单和快速设置都对准 Dock 中间弹出。
- **切换器**：Ctrl+Alt+Tab 窗口少的时候，预览图更大。
- **最大化也能唤出**：最大化的窗口（比如 ChatGPT 桌面版）盖住屏幕底边时，照样能把 Dock 叫出来。
- **不留缝**：任务栏设为自动隐藏时，有些应用会在底边留 1 像素空隙（比如栖页、Chromium 系应用）。LeanDock 让它们正常铺满屏幕，不修改应用本身。

## 🚧 还没做好

- [ ] 四指滑动切换虚拟桌面时，Dock 会短暂变透明（[排查记录](docs/gesture-investigation-latest.md)）
- [ ] 显示温度：还没找到可靠的数据来源
- [ ] 面板里的上一首／播放／下一首按钮
- [ ] 快速设置里的“深色主题”磁贴目前没有挂上，暂时请用详情面板切换
- [ ] Liquid Glass 那样的折射效果
- [ ] 移动热点的真实联网、多显示器、睡眠唤醒和全屏游戏，还没有逐项验证

## ⚠️ 用之前要知道

> [!WARNING]
> LeanDock 会修改 Windows 内部的任务栏组件，目前只在作者自己的一台电脑上验证过。

- **对不上就不碰**：每个被改动的系统文件都要和记录的哈希一致，对不上时 LeanDock 不加载，任务栏保持原样。好处是安全，代价是 Windows 更新后往往要重新适配。
- **更新会重启资源管理器**：每次更新运行组件，都会干净地重启一次承载任务栏的 Explorer。
- **安全软件可能拦截**：智能应用控制和 Defender 都拦过本项目的组件。项目不添加排除项，也不建议关闭防护（[处理记录](docs/defender-block-latest.md)）。

## 🚀 构建与安装

目前没有现成的安装包，需要自己构建。你需要 Windows x64、PowerShell 7、Python 3，以及一套独立的 Clang/MinGW 工具链（[怎么准备](docs/toolchain-latest.md)）。

```powershell
# 0. 告诉构建脚本工具链和 Python 在哪
$env:LEANDOCK_COMPILER_ROOT = 'D:\Toolchains\LeanDock'
$env:LEANDOCK_PYTHON = 'C:\Python313\python.exe'

# 1. 构建：先构建悬停面板，再构建其余组件，然后打包
.\aggregate\build-latest.ps1
.\build\latest.ps1
.\build\package-latest.ps1 -OutputDirectory D:\LeanDock\package

# 2. 安装到一个新的空目录（管理员 PowerShell）
.\latest.ps1 -Action Install -PackagePath D:\LeanDock\package -Destination D:\LeanDock\app

# 3. 以后更新：先只检查，确认无误再正式更新
D:\LeanDock\app\latest.ps1 -Action Update -PackagePath <新的包> -ValidateOnly
D:\LeanDock\app\latest.ps1 -Action Update -PackagePath <新的包>
```

<details>
<summary><b>常用管理命令</b></summary>

<br>

在安装目录里运行 `.\latest.ps1 -Action <动作>`：

| 动作 | 作用 |
|:--|:--|
| `Status` | 查看运行状态和各组件是否就绪 |
| `Start` ／ `Stop` | 启动或停止，Stop 会把任务栏恢复原样 |
| `Update` ／ `Rollback` | 带备份的更新（只留最近 5 份备份），以及回到上一次更新前 |
| `Install` ／ `Uninstall` | 注册或移除开机启动的计划任务（需要管理员）；Uninstall 不删文件 |
| `PauseCorners` ／ `ResumeCorners` ／ `ReloadCorners` | 暂停、恢复触发角，或改完配置后重新读取 |
| `Resources` | 测量资源占用 |

配置文件：

- `hotcorners/latest.ini`：触发角的动作和延迟。写错了会整个被拒绝，继续用上一份有效配置。
- `latest.ini`：日志和诊断开关，默认都关。
- `cards/latest.ini` 和 `Cards` 动作属于旧版小组件，当前不起作用，只为能回退而保留。

> ⛔ **不要按 `latest.exe` 这个文件名批量结束进程。** 要停止请用 `Stop`。

</details>

<details>
<summary><b>它是怎么工作的</b></summary>

<br>

| 组件 | 文件 | 在哪里运行 |
|:--|:--|:--|
| 控制进程 | `latest.exe` | 隐藏的独立进程，登录时由计划任务启动，负责触发角和各组件的生命周期 |
| Dock 和时间键 | `latest.dll` | 加载进 Explorer |
| 开始菜单布局 | `start/latest.dll` | 加载进开始菜单的宿主进程 |
| 快速设置定位 | `panels/latest.dll` | 加载进快速设置的宿主进程 |
| 详情面板 | `aggregate/latest.exe` | 悬停时由 Explorer 以普通用户权限启动，收起即退出 |

- 没有托盘图标、可见窗口或 Windows 服务；运行时不需要 Windhawk、Python、Node 或 .NET。
- 计划任务以“最高可用权限”运行，好让触发角也能作用于管理员窗口；Explorer 和详情面板不会因此提权。
- 计划任务、窗口类等内部名字仍叫 `NativeTaskbarDock`，为了兼容已有安装没有改名。

</details>

<details>
<summary><b>给维护者</b></summary>

<br>

- 改代码前先读 [AGENTS.md](AGENTS.md)，重要决定记在 [DECISIONS.md](DECISIONS.md)。
- **当前维护线只重建 Explorer 组件**：`.\build\latest.ps1 -BackendOnly`。控制进程、开始菜单和快速设置组件沿用 0.8.78 验收过的二进制，因为工作区里 hotcorners 的源码还有未审查的改动。
- 任务栏适配代码由 `build/generate-latest.py` 生成，触发角接入由 `build/adapt-hotcorners-latest.py` 生成。改生成器，不要只改生成出来的文件。
- `tests/` 下每个 `*-latest.ps1` 检查一个模块，例如 `tests\latest.ps1`（运行层）、`tests\aggregate-autotest-latest.ps1`（时间键和面板）。它们不接管正在用的 Dock，也不发送真实按键；测试通过不等于实际用起来没问题。
- `latest-backend.json` 里的 `layoutCost` 记录 Dock 每次重新排版时 LeanDock 回调的次数和累计耗时（微秒），用来判断这条路径值不值得优化。运行 `Status` 会刷新它。
- 每个版本的检查和验收记录在 [docs/verification-latest.json](docs/verification-latest.json)，历史版本在 `docs/verification-v*.json`。
- 和早期设计稿的逐项对照：[docs/aggregate-comparison-latest.md](docs/aggregate-comparison-latest.md)。
- 改写前的旧 README（逐版本的改动说明）：[docs/readme-v0.8.82.md](docs/readme-v0.8.82.md)。
- 首屏的 `docs/assets/screenshot.png` 是实机截图；时间键和触发角两张插图由 `docs/assets/draw-latest.py` 生成，是示意图。界面变了就重新截图，或改脚本重新生成。

</details>

## 🗺️ 一路走来

| 时间 | 版本 | 发生了什么 |
|:--|:--|:--|
| 10 月 7 日 | 0.2 – 0.3 | 第一版玻璃 Dock、紧凑开始菜单、触发角 |
| 10 月 8 日起 | 0.5 – 0.8.34 | Dock 上长出一排小组件卡片：时间、系统状态、电量、网速、媒体 |
| 10 月 17 日起 | 0.8.35 – 0.8.77 | 打磨图标清晰度、右键菜单、窗口切换器，以及与最大化窗口的相处 |
| 10 月 20 日 | 胶囊 0.9.x | 试过用一条独立的胶囊 Dock 取代任务栏，最后决定放弃。代码留在 `capsule/` 作为历史 |
| 10 月 23 日 | 0.8.78 – 0.8.82 | 回到原生任务栏，小组件区换成一颗聚合时间键 |
| 10 月 31 日 | 0.8.83 | 详情面板打开后更快出数；更新只保留最近 5 份备份 |

## 🙏 致谢

任务栏部分改编自 [m417z](https://github.com/m417z) 等作者的 [Windhawk](https://windhawk.net) Mod（Taskbar Styler、任务栏图标大小、自动隐藏等），钩子使用 [MinHook](https://github.com/TsudaKageyu/minhook)，玻璃质感参考了 NextKDE 的设计。完整来源和本地改动见 [NOTICE.md](NOTICE.md)。

本项目不是 Windhawk 的官方发行版。以 [GPL-3.0](LICENSE.txt) 发布。
