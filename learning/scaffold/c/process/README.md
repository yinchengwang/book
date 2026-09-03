# process scaffold

POSIX 进程管理演示——fork/exec/waitpid + 僵尸进程 + 守护进程编写。

## 复现命令

```bash
cd learning/scaffold/process

# Linux/macOS/MSYS/Cygwin：完整 5 段演示
gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test

# Windows MinGW：仅 [pid] 段可用（fork/exec 不可用，代码自动降级）
```

> ⚠️ **平台限制**：Windows 原生 MSVC 无 `<unistd.h>`/`<sys/wait.h>`——本卡在 MinGW 下编译，但 MinGW 也无 `fork()`。完整 fork/exec 演示需在 Linux/macOS/Cygwin 运行。MinGW 下代码自动检测并降级为平台说明。

## 预期输出（POSIX 完整版，节选）

```
[main pid] PID=12345 PPID=12340 PGID=12345 SID=12345

[fork] === fork() 父子进程 ===
[parent pid] PID=12345 ...
  parent: child PID=12346, waiting...
[child pid] PID=12346 ...
  child: doing some work, then exit(42)
  parent: child exited with status=42

[exec] === exec* 三种变体 ===
  [execve] hello from execve
  [execlp] echo via PATH
  [execv] explicit path

[zombie] === 僵尸进程演示 ===
  PID=12347 STAT=Z    (zombie)

[daemon] === 守护进程编写 ===
  [process daemon] PID=12348 PGID=12348 SID=12348

=== PASS ===
```

## 预期输出（Windows MinGW）

```
[pid] === 进程标识符（Windows 模式） ===
[main pid] PID=53164 (MinGW/Windows: getppid 不可用)

[NOTE] 当前为 MinGW/Windows：fork/exec 不可用
       完整 fork/exec/zombie/daemon 演示请在 Linux/macOS 运行

=== PASS ===
```

## 关键点

- **`fork` 返回两次**：父进程返回子 PID（>0），子进程返回 0，失败返回 -1
- **`_exit` vs `exit`**：子进程用 `_exit()` 不走 `atexit` 处理器 + 关闭 stdio 缓冲区，避免父进程再次打印
- **僵尸进程**：子进程 exit 但父进程没 `wait` ——内核保留 PCB 直到父进程收尸
- **守护进程三件套**：`fork + setsid + fork` ——确保脱离控制终端、不是会话首进程
- **`exec` 三族**：l 后缀（参数列表 list）/ v 后缀（参数数组 vector）/ p 后缀（自动搜 PATH）

详见 NOTES.md 工程对照段。