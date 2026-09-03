# signal scaffold

自触发信号 + 优雅停机演示。注册 SIGINT（Ctrl-C）触发优雅退出；SIGUSR1 触发 metrics dump。

## 复现命令

仅 POSIX（WSL/Linux/macOS）：

```bash
cd learning/scaffold/signal
gcc -Wall -Wextra -O2 -std=c11 -o signal_demo main.c
./signal_demo              # 跑直到 Ctrl-C

# 另开一窗口发 SIGUSR1 触发 dump：
kill -USR1 <pid>
```

## 预期输出

```
[signal] running pid=<pid> (Ctrl-C to stop, kill -USR1 <pid> to dump)
[metrics] processed=5 errors=0 rss_kb=<n>
[metrics] processed=10 errors=0 rss_kb=<n>
...
^C
[signal] graceful stop (processed=N, errors=0, dumps=M)
```

每 5 个工单自触发 SIGUSR1 dump 一次；Ctrl-C 后打印统计退出。

## 在本机（MinGW-w64）的现状

- `#error` 已显式触发
- Windows 上 SIGINT 是 `SetConsoleCtrlHandler`，API 完全不同
- POSIX 平台直接跑

## 关键点

- **`volatile sig_atomic_t`**：handler 触达的唯一安全变量类型，避免编译器优化掉
- **`SA_RESTART`**：让被信号打断的 syscall 自动重启；不设会让 read/accept 返回 EINTR
- **`kill(getpid(), SIGUSR1)`**：自触发标准做法；signal 仍要"先注册 handler 再发"
- **优雅停机模式**：handler 只设 flag，主循环在安全点检查 flag；绝不直接 `exit(0)` 在 handler 里（会跳过 RAII 类清理）

详见 NOTES.md 工程对照段（与 §7 daemon 互引）。
