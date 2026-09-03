# ipc scaffold

POSIX 三件 IPC：pipe / FIFO / signal。覆盖 `fork + pipe`、`mkfifo + open`、父子进程 `kill + sigaction` 全链路。

## 复现命令

需要 POSIX 兼容的 C 编译器与 Linux/macOS/WSL。**MinGW-w64 不支持**（缺 `<sys/wait.h>`）。

```bash
# Linux / WSL / macOS
cd learning/scaffold/ipc
gcc -Wall -Wextra -O2 -std=c11 -o ipc_demo main.c
./ipc_demo              # 全部三件
./ipc_demo pipe         # 仅 pipe
./ipc_demo fifo /tmp/x  # 仅 FIFO
./ipc_demo signal       # 仅 signal
```

## 预期输出（Linux 下）

```
[pipe] parent read: 'hello-pipe' (n=12)
[fifo] read: 'via-fifo' (n=9)
[signal] parent received SIGUSR1 from child (pid=<n>)
OK
```

## 在本机（MinGW-w64）的现状

- 当前 Windows + mingw 14.2 环境无法编译（缺 `<sys/wait.h>` 等 POSIX 头）
- 已在源码中加 `#warning` 提示
- 设计意图是 POSIX 学习——本 scaffold 仅在 POSIX 平台有效

## 关键点

- `pipe(2)` 创建匿名管道，配合 `fork` 实现父子间单向字节流
- `mkfifo(3)` 在文件系统中创建设备文件，**任意两个进程可通过路径通信**
- `sigaction(2)` 是 POSIX 推荐接口，替代老旧 `signal()`，支持 SA_RESTART 与精确信号屏蔽

详见 NOTES.md 工程对照段。
