# daemon scaffold

经典 daemonize 五步：`fork → setsid → fork → umask/chdir → close+redirect` + 注册 SIGHUP/SIGTERM handler + 写 pidfile。

## 复现命令

仅 POSIX（WSL/Linux/macOS）：

```bash
cd learning/scaffold/daemon
gcc -Wall -Wextra -O2 -std=c11 -o daemon_demo main.c

# 前台跑（不真后台）
./daemon_demo

# 真后台模式（fork 出父进程）
./daemon_demo --background
cat /tmp/daemon-scaffold.pid
kill -TERM $(cat /tmp/daemon-scaffold.pid)
```

## 预期输出（前台模式）

```
[daemon] started pid=<pid> pidfile=/tmp/daemon-scaffold.pid
[daemon] daemon-demo: tick=5
[daemon] daemon-demo: tick=10
...
[daemon] graceful stop
```

## 在本机（MinGW-w64）的现状

- `#error` 已显式触发（缺 `<unistd.h>` 完整 + `<sys/types.h>`）
- Windows 上 daemon 用 `CreateProcess + SERVICE_*` API，不在 POSIX 范畴
- POSIX 平台直接跑

## 关键点

- **为何两次 fork**：第一次让父进程退出 → 进程脱离 shell 进程组；第二次让未来无法获取 tty（即使 reopen 也是非 tty）
- **`chdir("/")` 不是必需**：早期教程说要；但现代可省（保留 cwd 也常见）；本 demo 走老规范
- **`umask(0)` 是给 daemon 完全文件权限控制权**：避免 spawn 子进程受 umask 钳制
- **pidfile 路径**：本 demo 用 `/tmp/`；生产用 `/var/run/<name>.pid`；非 root 用户用 `$HOME/.local/run/` 兜底
- **`SA_RESTART` 重启慢 syscall**：与 signal 互引；优雅停机靠 `SIGTERM/SIGINT → flag → 清理 → exit`

详见 NOTES.md 工程对照段（与 §6 signal 互引）。
