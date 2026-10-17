# Defender 拦截与部署中断

## 拦截现场

2026-10-17 07:18，用户完成 UAC 确认后，现有 Update 事务开始部署 0.8.32。07:19:07，Defender 实时保护拦截部署目录中的 latest.exe；07:19:36，事件 1116/1117 记录 `Trojan:Win32/Bearfoos.B!ml`、ThreatID 2147731849，隔离成功。处置对象包含控制器文件、PID 24068 和 NativeTaskbarDock 计划任务。07:21 的只读核对确认控制器文件与任务均不存在，卡片诊断属性已清除。

智能应用控制仍为 Off，Defender 实时保护为 On。本次阻断是 Defender 查杀，不是 UAC 尚未批准或此前的智能应用控制问题。安全智能版本为 1.461.53.0，日志没有给出具体命中特征，不能仅根据检测名称判定误报。

## 文件与事务边界

0.8.31 的备份构建清单、0.8.32 构建清单和更新前检查均记录控制器版本 0.3.13、292352 字节、SHA256 `3EBAE5829C94E93730051BAE310D5645A7DA9617FFF6EF6E0EE6A14464D7DADA`。本次变更清单没有 latest.exe，唯一改变的运行文件是 latest.dll。隔离后再次读取包中控制器也被系统拒绝，没有换路径或重建文件规避。

这些证据支持“本次未修改控制器”，不等于完成了恶意软件鉴定。源代码中的跨进程加载用于已核验的原生宿主定制；是否因此触发本次检测尚未被证明。

部署目录当前 latest.dll 为 0.5.27，SHA256 `CC01F4D6FCFF1FD8DE085E1E77D34798CD7EEB232BCEEAA2190F6D6F605E9C85`。它曾短暂接入 Explorer，控制器被隔离后按退出流程还原界面。更新未提交，自动回退也因控制器受阻而停止，因此不能称为成功升级或已回退。

用户配置与更新前保护清单一致：latest.ini 为 `5D3060BEAC27A3D4BDD975FFB1C079E5D31269D63EF3C72260EF0C51346AC375`，hotcorners/latest.ini 为 `59E3EC5C7AEBF63816D904256908A33EB552A30A7B22F176750741198FD5446E`。

事务与原文件保留在部署目录 backups/before-0.8.32-20261017-071843985；manual-recovery-required 标记保留。现场日志保存在本次工作目录 work/card-touchpad-20261017/v0.8.32-final/ 中的 apply-result-latest.json、defender-events-latest.json、blocked-status-latest.json。此前限时观察器已于 03:21 自动退出，更新助手亦已退出。

## 恢复决定与执行

用户随后选择“我会核对并允许、恢复 Dock”，并明确告知已关闭实时保护。07:30 只读配置观察到该 ThreatID 的处置覆盖值为 6；控制器文件当时仍缺失。07:31 从已可读取的本地包向原路径恢复同哈希控制器，没有更改文件名、重编译规避检测或添加 Defender 排除项。是否为误报仍未完成独立鉴定。

恢复脚本先核对 279 个包文件、原事务与保护配置，然后重新开启 Defender 实时保护并确认开启。其后使用原有 Install 入口恢复相同自启任务，使用原 Rollback 完成 07:18 事务的回退，再通过正常 Update 部署 0.8.32。07:34:53 收据为 Succeeded，更新收据为 Applied，ComponentHealth=Ready。新回退点为 backups/before-0.8.32-20261017-073436405；原手工恢复标记和失败证据保留，不抹除历史失败。

07:35:53 再核对实时保护为 On、智能应用控制仍为 Off，四个运行文件与构建哈希一致。控制器 PID 8268、Explorer PID 21708、后端 0.5.27，均为当次快照而非固定标识。输入已记录 29 次成功 SetContact、1263 次内容更新、53 次真实位移换页，DMError=0。这证明新路径实际工作，不代替用户对方向、手感、鼠标滚轮与电池单击的确认。

恢复脚本与回执位于工作目录 v0.8.32-final/recover-latest.ps1、recovery-result-latest.json、recovery-rollback-latest.json、recovery-update-latest.json、post-recovery-latest.json。新只读观察器有一小时上限并使用同目录 observe.stop 停止；不发送输入或改变焦点。不能重放已完成的恢复事务。

微软的保护历史说明：隔离文件与“允许在设备上”是不同处置，仅恢复可能再次触发检测。官方依据：https://support.microsoft.com/en-us/windows/security/windows-security/protection-history-in-the-windows-security-app
