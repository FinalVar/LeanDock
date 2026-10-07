# LeanDock

面向 Windows 11 的精简原生任务栏定制：悬浮 Dock、连续双排托盘、智能隐藏与触发角。

当前是 **0.2.3 私有源码基线**，从已部署的 NativeTaskbarDock 整理而来。它直接调整 Windows 原生任务栏，保留原生应用按钮、通知图标与快速设置入口。当前构建只支持已核验的一组 Windows 系统组件，尚不是面向所有 Windows 11 版本的通用安装包。

## 功能

- 居中、圆角、模糊半透明的悬浮 Dock。
- 应用托盘和电池按连续两排排列，最后一列只有一颗图标时上下居中；显示全部应用托盘图标。
- 隐藏开始、搜索、任务视图、小组件、日期时间、网络、音量、电量百分比及输入法标记。
- 窗口与任务栏区域相交时隐藏，桌面或没有重叠窗口时显示；保留底边唤出。
- 左下角显示／恢复桌面，右下角任务视图；零停留延迟，移出角落后才可再次触发。

## 运行结构

只有一个额外的隐藏控制进程，另有一个组件加载到 Explorer 中。通过当前用户交互式登录计划任务启动，没有自己的托盘图标、可见窗口或 Windows 服务。正常运行不需要 Windhawk 引擎、Python、Node 或 .NET；目前构建工具复用 Windhawk 随附的 Clang/MinGW/WinRT 开发环境。

控制进程使用事件处理 Explorer 生命周期和鼠标原始输入。触发角已接入同一消息循环。为了支持已有管理员权限的前台窗口，现有部署的任务使用 HighestAvailable；Explorer 本身不因此提权。

`NativeTaskbarDock` 任务名、窗口类、互斥锁等内部标识暂时保留，以兼容已有部署。LeanDock 是仓库和项目名称，首次入库没有重命名或重装当前运行的程序。

## 构建

### 要求

- Windows x64、PowerShell 7、Python 3。
- 带 Win32/WinRT 头文件的 Clang/MinGW 工具链。已核验的工具链是 Windhawk Compiler 的 Clang 20。
- 本机系统 DLL 必须匹配 `baseline/modules-latest.json` 的 SHA-256；生成器同时核对 PE 时间戳、映像大小和符号位置。

仓库保存构建所需的原始输入、生成源码、配置与许可证。没有上传编译器、Windows 系统 DLL、EXE/DLL 成品或本机运行数据。

### 命令

```powershell
.\build\latest.ps1 -CompilerRoot "$env:ProgramFiles\Windhawk\Compiler" -PythonPath python
```

也可通过 `LEANDOCK_COMPILER_ROOT`、`LEANDOCK_PYTHON` 指定工具位置。构建产生根目录 `latest.exe`、`latest.dll` 及被 Git 忽略的构建收据，不会安装、启动或注入任何进程。

`-ControllerOnly` 只构建控制程序；`-BackendOnly` 只构建 Explorer 组件，两者互斥。完整构建会先重跑生成器。维护任务栏适配时改 `build/generate-latest.py`，维护触发角接入时改 `build/adapt-hotcorners-latest.py`，不要只改生成文件。

## 配置与管理

`latest.ini` 仅提供运行日志、诊断开关，默认均关闭。外观参数是固定编译配置。`hotcorners/latest.ini` 提供触发角配置示例，采用当前部署的动作映射、3 像素触发区和 32 像素重新触发范围。

仓库管理入口保留以下动作：

```powershell
.\latest.ps1 -Action Status
.\latest.ps1 -Action Start
.\latest.ps1 -Action Stop
.\latest.ps1 -Action PauseCorners
.\latest.ps1 -Action ResumeCorners
.\latest.ps1 -Action ReloadCorners
.\latest.ps1 -Action Install
.\latest.ps1 -Action Uninstall
```

这些动作针对脚本所在的部署目录。若同名计划任务指向其他目录，管理器会拒绝修改；源码克隆目录不能直接接管现有安装。Install 需要管理员维护会话，只注册任务；Uninstall 停止进程并移除任务，保留文件。无效触发角配置会被拒绝，继续使用此前有效配置。

Stop 会恢复任务栏属性和钩子，但 DLL 保留在 Explorer 内直到其退出。更新 DLL 必须先备份、正常停止，再通过有回退保护的维护流程干净重启当前会话的精确 Explorer，清除旧映像后启动。不要按 `latest.exe` 文件名批量结束进程。

首次入库没有提供通用迁移／更新安装器。本机专用备份、自动恢复计划和一次性迁移脚本继续留在原部署中，未作为可移植功能发布。

## 验证

完整构建后，可运行不控制生产 Dock 的检查：

```powershell
.\tests\latest.ps1
.\tests\grid-latest.ps1
.\tests\hotcorners-latest.ps1
python .\tests\imports-latest.py
```

前三项分别检查运行层、布局不变量和触发角核心。测试脚本可传 `-CompilerRoot`，默认使用与构建相同的环境变量或开发工具目录。

触发角接入测试需显式加 `-IncludeHosted`，且生产触发角互斥锁必须空闲；默认不运行这一项。它创建测试窗口和原始输入接收端，但不提交按键动作。不要为源码检查擅自停止正在使用的 Dock。

历史验收及边界保存在 `docs/baseline-latest.json`，本次源码整理的复核见 `docs/verification-latest.json`。0.2.3 的原生托盘数量 5→6→7→6→5 变化已核对 29 个实际控件矩形；单独针对这次居中外观的用户反馈仍待确认。较早的 Dock 点击、智能隐藏、管理员窗口前的触发角以及输入法隐藏已有用户确认。

## 当前边界

- 依赖 Windows 内部任务栏接口，不能把“当前电脑可用”推广为所有 Windows 11 版本兼容。系统组件不匹配时拒绝旧 Dock 附加，不能跳过哈希检查继续注入。
- 智能隐藏及唤出目前仍使用原生全宽任务栏区域；实际 Dock 宽度范围的唤出属于后续工作。
- 当前是模糊半透明外观；尚未实现液态玻璃或菜单跟随 Dock 的位置调整。
- 曾在维护热切换中遇到一次 Explorer / Windows.UI.Xaml.dll 异常，确切原因未定位，后续使用受保护的干净加载。
- 多显示器、睡眠／锁屏恢复、注销登录、各类全屏游戏及长期运行尚未逐项验收。

## 来源与许可

使用 GPL-3.0，保留 Windhawk Mod 原作者及贡献者信息；MinHook 保留自身许可证。任务栏样式和行为来自保留的上游源码及本地适配，触发角复用本项目原有 Win32 实现。NextKDE 提供布局参考；当前没有运行或复制 KDE 的任务栏组件。详见 `NOTICE.md`、`LICENSE.txt` 及第三方目录中的声明。
