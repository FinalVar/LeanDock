# 时间键：滚动调音量和亮度、充电闪电、面板星期

适用版本：0.8.100 / 后端 0.5.85 / 面板 0.8.100；控制器不变（0.3.42）。

## 用户要的

- 在时间键上滚动调音量，按住 Ctrl 滚动调亮度，不用打开面板。
- 充电时在日期旁边能看出来。
- 面板的日期带上星期几。

## 怎么用

| 操作 | 效果 |
|:--|:--|
| 鼠标滚轮 | 音量，每格 2（和 Windows 音量键一样） |
| Ctrl + 滚轮 | 亮度，每格 5 |
| 触摸板两指上下滑 | 同滚轮；手指放下时按着 Ctrl 就是亮度，整次滑动不变 |

- 调的时候，时间键的两行临时换成数值和名称（“22 / 音量”、“0 / 静音”、“60 / 亮度”）。最后一次变化后 1.2 秒换回时间和日期。
- 数值还没回来时先显示“…”；读写失败或没有可调亮度的屏幕（外接显示器）显示“—”。
- 静音时往上滚会取消静音。
- 滚动不会弹出详情面板：面板还没出来时，滚一下就取消这次悬停。面板已经开着时照常调，面板不收起。
- 横向滚轮和横向两指滑动不处理。
- 充电时（GetSystemPowerStatus 的 BatteryFlag 含 8）日期右边出一个小闪电，读屏名称加“正在充电”。插着电但已充满时不显示。临时显示数值时闪电隐藏。
- 面板右上角的日期变成“11月1日 周日”。

## 实现

**按键侧（后端，src/cards-aggregate-latest.cpp）**
- 鼠标滚轮：在键上用 XAML 的 PointerWheelChanged（handledEventsToo），Ctrl 来自事件的 KeyModifiers。不挂钩子。
- 触摸板：两指滑动在 XAML 里收不到滚轮事件，所以重新接上旧卡片已验收的输入路径：
  - include/cards-input-latest.h：InputSite 窗口过程挂接，只认核验过的 InputHost.dll（哈希 5C0CBA72…）。新增 `CardInputOptions`：时间键用 `claimWheel=false`（滚轮留给 XAML）、`passControl=false`（按住 Ctrl 的两指也接）。默认值与旧卡片一致。
  - include/cards-manipulation-latest.h、cards-pan-latest.h：DirectManipulation 视口。新增 repeat 模式：每滑 12 DIP 一格，余量带到下一帧，一帧滑得快会出多格；锁定主轴、捏合不算。默认（48 DIP、一次接触一格）不变。
  - 只在键的范围内接管；触摸屏、键外、Dock 隐藏时都交回系统。
- 滚轮格数：include/key-levels-latest.h 的 `Notches`，一个 WHEEL_DELTA 一格，高精度滚轮累积，停顿超过 0.5 秒丢掉余量。
- 显示：`Display` 换两行文字和读屏名称，计时器 1.2 秒后 `EndReadout` 恢复。

**读写音量和亮度（include/key-levels-latest.h）**
- 用的是面板已有的调用（aggregate/controls-latest.h）：
  - 音量：IAudioEndpointVolume（默认输出设备）；
  - 亮度：WMI `ROOT\WMI` 的 WmiMonitorBrightness / WmiMonitorBrightnessMethods.WmiSetBrightness。
- WMI 可能很慢，所以放在单独线程，不在任务栏线程上跑：
  - 有请求时才创建线程（MTA），连续的请求合并成一次；
  - 空闲 4 秒退出，用 FreeLibraryAndExitThread 释放对模块的引用，Dock 卸载时不会留线程；
  - 亮度每次线程启动时读一次，之后按设置的值累计。
- 结果用调度器回到 UI 线程显示。

**充电闪电**：XAML 里日期改成横排的 `DateLine`（日期 + Segoe Fluent Icons 的 E945，9 号字，主题色同电量环）。`ReadPower` 按 BatteryFlag 设可见性。

**面板星期**：aggregate/latest.cpp 在日期后加“ 周X”（wDayOfWeek，0 为周日）。

## 状态

任务栏窗口上的属性（数值都 +1 存放，0 表示未设置）：
- `LeanDock.Card.Levels.Wheel/Steps/Results/Failures/Volume/Brightness`
- `LeanDock.Card.Input.Ready/Prepared/TouchpadStarts/Contacts/Error`
- `LeanDock.Card.ChargeBolt`

## 验证

- 构建前测试：
  - cards-input 44 项：时间键路线不接滚轮（含 Ctrl），接两指（含 Ctrl），触摸屏和键外不接；
  - cards-manipulation 56 项：repeat 的多格、余量、反向、横向、捏合，默认不变；
  - cards-aggregate 562 项：格数、范围、四种显示；最宽日期加闪电 40 DIP，在 46 DIP 的弧内；“100 / 静音”放得下。
- 实机（用户同意后测，测完恢复）：
  - 滚轮上两格：音量 18→22，键显示“22 / 音量”；下两格回 18；
  - Ctrl+滚轮：亮度 100（上限不变）→95，键显示“100 / 亮度”；
  - 1.2 秒后换回时间；未弹出面板；失败 0；
  - 面板日期“11月1日 周日”。
- 未在实机上验证：
  - 触摸板两指（远程无法模拟，等用户试）；
  - 闪电：测试时电池 100% 已充满，不在充电。
