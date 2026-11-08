# 资源管理器退出记录

适用版本：0.8.110 / 控制器 0.3.43。

2026-11-06 22:20 资源管理器意外退出过一次，控制器 6 秒后恢复了 Dock。这台机器的 Windows 错误报告服务被精简、策略也关了，不留转储也没有应用程序错误事件，所以分不清是崩溃还是被结束。用户 2026-11-08 同意加记录。

## 记什么

控制器本来就在等资源管理器进程结束，好重新接上 Dock。0.3.43 打开进程时多要一个查询权限（`PROCESS_QUERY_LIMITED_INFORMATION`），结束时在主线程读退出码和运行时长，写一行到安装目录的 `latest-explorer-exits.log`：

```
2026-11-08 19:30:05 explorer pid=23488 exit=0xC0000409 ran=5012s
```

- `exit`：退出码。`0xC` 开头是异常（`0xC0000005` 访问冲突、`0xC0000409` 快速失败或栈破坏、`0xC0000374` 堆损坏），说明是崩溃；`0x00000001` 这类小数字多半是任务管理器结束、`taskkill` 或正常退出。读不到时写 `unknown`。
- `ran`：这个资源管理器活了多少秒。
- 只记控制器在跑时的退出。LeanDock 更新会先停控制器再重启资源管理器，所以更新不会留下记录。
- 超过 64 KiB 从头再写。不记窗口标题、路径或其他内容。

`latest-status.json` 另有 `explorerExits`（本次控制器看到的次数）、`explorerLastExitKnown`、`explorerLastExitCode`。

## 构建

按 AGENTS.md 的控制器规则：以 `artifacts/controller-0.3.42-fullscreen/latest` 为底，先原样 `build/latest.ps1 -ControllerOnly` 重编，结果与正在用的 0.3.42（7580D4BB…）只差 PE 头 3 字节时间戳，生成的触发角源码相同；再只换入 `src/controller-latest.cpp` 重编，得到 4A3DB085…。构建输入存于 `artifacts/controller-0.3.43-exitlog/latest`；面板 `aggregate/theme-control-latest.h` 的控制器哈希已同步。

## 未验证

记录路径要等资源管理器真的退出才走得到。部署后没有为了验证去结束资源管理器，所以只验证了状态文件里的新字段（0 次）。
