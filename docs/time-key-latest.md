# 时间键：充电闪电、面板星期

适用版本：0.8.108 / 后端 0.5.93 / 面板 0.8.108；控制器不变（0.3.42）。

- 充电时（GetSystemPowerStatus 的 BatteryFlag 含 8）日期右边出一个小闪电，读屏名称加“正在充电”。插着电但已充满时不显示。
- 面板右上角的日期带星期（“11月7日 周六”）。

**实现**：XAML 里日期改成横排的 `DateLine`（日期 + Segoe Fluent Icons 的 E945，9 号字，主题色同电量环），`ReadPower` 按 BatteryFlag 设可见性。星期在 aggregate/latest.cpp，用 wDayOfWeek，0 为周日。

## 不再有滚动调节

0.8.100 曾让时间键滚轮调音量、Ctrl+滚轮调亮度、触摸板两指同样调节。这是我提议的，不是用户提的需求。2026-11-07 用户试过两指后说按键已经够方便，还怕误触，要求删掉；0.8.101 全部删除：

- 删除 include/key-levels-latest.h、键上的滚轮事件、触摸板路径和临时数值显示；后端不再链接 wbemuuid。
- include/cards-input-latest.h、cards-manipulation-latest.h、cards-pan-latest.h 及其测试还原到 0.8.99。

音量和亮度仍在悬停面板里调。不要再把滚动调节加回来，除非用户明确要求。
