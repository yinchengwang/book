# signal 学习笔记

## 概念地图

POSIX 信号是内核向进程异步通知事件的机制：

- **`SIGINT`**（Ctrl-C）：交互中断，请求优雅停机
- **`SIGTERM`**（kill 默认）：请求优雅停机（与 SIGINT 等价语义）
- **`SIGKILL`**（kill -9）：强制终止，不可捕获
- **`SIGHUP`**：终端断开；daemon 化后常用于"重读配置"
- **`SIGUSR1/SIGUSR2`**：用户自定义信号，工程上常作"心跳触发 dump"
- **`SIGCHLD`**：子进程退出或停止

注册方式历史：老 `signal()` 在不同实现下行为不一致；现代统一用 `sigaction(2)`：
```c
struct sigaction sa = {0};
sa.sa_handler = handler;
sigemptyset(&sa.sa_mask);  // 不要 SIGTERM 阻断 SIGUSR1
sa.sa_flags = SA_RESTART;  // 自动重启慢 syscall
sigaction(SIGINT, &sa, NULL);
```

handler 必须**只设置 atomic flag**——复杂逻辑在主循环里。POSIX 列出 handler 安全的少数 API：`write`、`_exit`、`signal`（自己注册自己）、`sigaction` 系列、`errno` 改动。

## 踩坑记录

1. **MinGW-w64 缺 POSIX signal 完整实现**：本机直接 `#error`。Windows 上的 `SetConsoleCtrlHandler` 与 `GenerateConsoleCtrlEvent` 提供类似能力，但 API 哲学不同。
2. **handler 不可调用 malloc/printf**：printf 不是 asynsignal-safe；本 demo 把 metrics dump 留在主循环里（`if (g_dump) ...`）而非 handler 内调用，是**正确做法**。
3. **`volatile sig_atomic_t` 的边界**：SIG_ATOMIC_MAX 不保证 int 宽度——但可见性保证是正确的。若需要传更多状态，用 `signalfd(2)` / `eventfd` 或 sigqueue 带 value（real-time signal）。
4. **`nanosleep` vs `sleep`**：nanosleep 不被 SA_RESTART 自动重启；被 SIGUSR1 打断会返回 -1 + EINTR，但 sleep 被 SA_RESTART 重启。
5. **自触发 SIGUSR1 不是无限的**：递归进入 handler 会消耗栈——`kill(getpid(), SIGUSR1)` 当 handler 已注册时只会再次跑相同 handler，**不会死循环**，但栈用过多是隐患。
6. **优雅停机的"忙等"问题**：本 demo 用 `while(!g_stop)` 简单模型；真实 daemon 在 `while(!g_stop) epoll_wait(...)` 里等待事件并定期检查 flag。

## 工程对照（≥100 字硬约束）

signal 在 rag 服务化与 minivecdb 控制面有以下直接迁移价值：

1. **rag server 优雅停机**：未来 rag server 在收到 `SIGTERM` 时（systemd stop / docker stop），必须能完成正在处理的 LLM 请求、关闭长连接、刷出 WAL 未 commit 条目；本卡就是该模式的标准实现。本 README/README.md 与 §7 daemon 互引。
2. **SIGUSR1 触发 metrics dump**：minivecdb 与 engineering/src/db/core/db_server.c 都需要"运行时不停服、不重启"地把 metrics 输出到 stdout 或 metrics endpoint。本卡刷过后读者能识别 PG 风格 `pg_ctl reload` 与 SIGUSR1 dump 的等价语义。
3. **raft election timeout**：`engineering/src/db/consensus/raft.h` 中 election timer 是 pthread_cond_timedwait + SIGALRM 的混合实现；遇到 SIGALRM 触发定时器重置。本卡读懂 `sigaction(SIGALRM, ...)` 段。
4. **engineering/src/algo-prod/dist_txn.c 两阶段提交**：协调者向参与者发信号要求 prepare；参与者通过 `sigwait` 同步等待协调者信号。本卡提供这套语义的入门。

**与 §7 daemon 互引**：daemon 化进程必须正确处理 SIGHUP 重读配置、SIGTERM 优雅退出、SIGCHLD 子进程收割；本卡提供 handler 注册与 flag 模式，daemon 提供 fork 后 handler 重置的方法。两者必须配套使用。
