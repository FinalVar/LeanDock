# LeanDock 0.7.1 来源与重复实现复核

复核日期：2026-10-09。范围：加载与退出、菜单定位、开始菜单适配。

结论：任务栏五个基础模块确实使用保留的 Mod 源码生成；这不等于
LeanDock 复用了 Windhawk 引擎，也不等于自写接入层已经具有其全部边界保护。
本次补上已复现的停止时序缺口，消除一份重复定位函数；不改变已验收的
外观、菜单锚点或开始菜单布局。不能按生成文件行数给出可信的复用百分比。

## 来源和保留决策

| 模块 | 现有实现和来源 | 本次决策 |
| --- | --- | --- |
| 任务栏样式 | Taskbar Styler 1.10 源码，保留 XAML 生命周期与还原逻辑 | 继续复用，不另写样式引擎 |
| 图标尺寸、智能隐藏、全托盘、托盘间距 | 四个对应 Mod 的保留源码经生成器适配；托盘输入已含本地连续双排改动 | 继续复用，不能称为未经修改的官方版本 |
| 挂钩运行层 | 库为 stock MinHook 1.3.4；共享目标链、接入和停止管理为本地代码 | 参照 Windhawk 的同锁退出门控修复，保留当前库与链 |
| 开始菜单与快速设置位置 | 在所属 UI 线程、原生位置提交前调整；参照 Shell Flyout Positions 1.3 的处理阶段 | 保留受限窗口子类方式；两者共用 Dock 查找与位置计算 |
| Win+X 菜单 | 固定版本原生 XAML 入口的锚点替换 | 保留；不能用事后搬窗口替代原生布局与命中 |
| 开始菜单网格 | 参考 Compact Start Menu 1.0.0 的分组平铺、恢复和语义缩放处理；本地事件驱动、原生模型、内容尺寸适配 | 保留现行实现，不为复用数量换回整份通用 Mod |
| 卡片、触发角、管理器 | 本地实现；NextKDE 是布局/行为参考，NativeHotCorners 是用户已有自写代码 | 如实标注，不记作第三方直接复用 |

五个保留输入的哈希与 build/sources-latest.json 一致。完整作者与许可证记录
在 NOTICE.md，保留的源码在 upstream；生成文件包含适配，不能用于估算未改动代码占比。

## 本次修正

### 停止后的迟到挂钩操作

此前 Stop 在禁用挂钩时没有关闭注册和提交入口。一个在停止期间等待操作锁的
LoadLibrary 回调仍可能随后增加新目标或提交待启用挂钩，违反“停止后保持原生”
的约定。这是独立进程测试证实的代码缺口；没有证据认定它就是此前某次桌面故障的原因。

参照 Windhawk LoadedMod 的 BeforeUninit、SetFunctionHook、ApplyHookOperations：
在同一把锁下关闭入口并禁用挂钩，拒绝迟到注册和提交；明确恢复成功才重新开放。
前置还原回调与后置 UI 清理不持有该操作锁，避免把跨线程 UI 等待放进锁中。
状态读取的挂钩数量也通过同一锁取快照，避免与动态注册并发读取容器。
排队启用失败现在会如实上报。DLL 与跳板仍保持映射至 Explorer 退出。

来源固定至 Windhawk 提交
`aef1a9f1e77f30c8fd96c5dd29373c38181486bf`：
https://github.com/ramensoftware/windhawk/blob/aef1a9f1e77f30c8fd96c5dd29373c38181486bf/src/windhawk/engine/mod.cpp

### 重复的 Dock 查找

快速设置之前有一份与 LeanDockPlacement::DockFor 相同的实现。现在直接调用
开始菜单已使用的辅助函数。按原有显示器选择和几何读取工作，不改变定位策略。

## 没有整体替换的理由与后续边界

- Windhawk 仓库中的扩展 MinHook 提供多挂钩标识和链管理，确有替代本地链的价值。
  但它与当前 stock MinHook 的跳板、线程暂停、链移除机制不同；仓库另有
  MinHook-Detours/SlimDetours 路径。此次只读比较，未导入、构建或声称已验收。
  将来若替换，应先在隔离宿主中验证共享目标、重复注册、停用中回调、恢复、
  跳板寿命，再评估是否减少维护成本。
- Shell Flyout Positions 1.3 的通用坐标钩子并不直接替代当前 Win+X XAML 锚点。
  现行开始菜单与快速设置已在原生显示提交前定位；整份引入会扩大影响范围，
  也不能据此证明避免首次显示闪跳。
- 开始菜单仍依赖固定系统模板。首次遇到不支持的数据模型会保留原生行为；
  已接管的同一控件后来换为不支持的模型时，Refresh 失败和已锁定视图控件之间
  仍缺少专门的回退回归。此次没有在真实宿主上复现该场景，也未修改该状态机。
  不能把“当前模板正常”扩大为“任意模板重建和未来 Windows 更新兼容”。
- Start 的窗口重建、安装/卸载应用期间的数据变动、不同 DPI 和多个真实显示器
  仍须分别验证。系统组件哈希不匹配时继续拒绝接入，不放宽校验。

对照来源：

- Windhawk 扩展 MinHook：
  https://github.com/ramensoftware/windhawk/tree/aef1a9f1e77f30c8fd96c5dd29373c38181486bf/src/windhawk/engine/libraries/MinHook
- Shell Flyout Positions：
  https://github.com/ramensoftware/windhawk-mods/blob/main/mods/shell-flyout-positions.wh.cpp
- Compact Start Menu 1.0.0：
  https://github.com/ramensoftware/windhawk-mods/blob/5b32bb24425ef9af3e7769665f667b8144352a20/mods/compact-start-menu.wh.cpp

## 验证

- 运行层 91 项通过，含真实 MinHook 目标、共享链、停止期间迟到注册/提交、
  未提交目标和 20 轮并发启停。
  恢复检查针对已经注册的函数链，不代表完整 Explorer 热重新接入已通过；
  正式运行组件更新继续使用现有的干净宿主重启流程。
- 去掉两处退出门控的隔离反例出现 6 项预期失败，包含停止后挂钩重新启用；
  证明新增回归能够识别所修复的问题。反例不加载到 Explorer。
- 定位 72 项、系统动作 67 项通过；动作测试未发送实际按键。
- 四个运行产物只导入系统库，没有新增常驻程序或运行依赖。
- 控制程序和开始菜单 DLL 的哈希保持不变；正式配置通过部署事务保留。

这些证据不代替用户对真实菜单点击、动画和外观的验收。此次未增加多显示器、
睡眠、锁屏恢复或重新登录的验收结论；开始菜单剩余状态机边界明确保留为未完成验证。
