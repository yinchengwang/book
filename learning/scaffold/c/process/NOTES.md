# process 学习笔记

## 概念地图

进程（Process）是操作系统资源分配的基本单位——每个进程有独立的 PID/PPID/PGID/SID、独立的虚拟地址空间、独立的文件描述符表：

- **PID 三件套**：
  - `getpid()` —— 当前进程
  - `getppid()` —— 父进程（PPID）
  - `getpgrp()` —— 进程组 ID（PGID）
  - `getsid(0)` —— 会话 ID（SID）
- **进程生命周期模型**：
  - **创建**：`fork()` 复制父进程（Copy-on-Write，写时复制）
  - **执行新程序**：`exec*()` 族替换当前进程的代码段、数据段
  - **终止**：`exit()` / `_exit()` —— `_exit` 不走 stdio 缓冲，`exit` 走
  - **回收**：`wait()` / `waitpid()` —— 父进程收尸，读取子进程退出状态
- **孤儿进程**：父进程先于子进程退出，子进程被 init (PID=1) 收养
- **僵尸进程**：子进程 exit 后父进程未 wait ——内核保留 PCB 直到父进程收尸。**大量僵尸 = fd 泄漏**
- **守护进程（daemon）**：脱离控制终端 + 后台运行的进程。**三步走**：`fork + setsid + fork`
- **exec 族 6 变体**：

| 变体 | 路径 | 参数 | 环境 |
|------|------|------|------|
| `execl` | 显式 | list | 当前 |
| `execlp` | PATH 搜 | list | 当前 |
| `execle` | 显式 | list | 自定义 |
| `execv` | 显式 | vector | 当前 |
| `execvp` | PATH 搜 | vector | 当前 |
| `execve` | 显式 | vector | 自定义 |

## 踩坑记录

1. **`fork` 后 stdio 缓冲区被复制**：父子都 `printf` 会输出**两次**——fork 前必须 `fflush(stdout)` 或子进程用 `_exit`
2. **僵尸进程未回收**：父进程忘了 `wait` ——内核保留 PCB，pid 数泄漏。**长期运行的进程（服务器）必须 `signal(SIGCHLD, SIG_IGN)` 或显式 wait**
3. **守护进程的 chdir("/")**：不切换到根目录可能阻塞 umount（设备 busy）
4. **`setsid` 后无法获取控制终端**：这是优点（脱离 tty），但调试时**日志必须显式重定向到 syslog 或文件**
5. **`fork` 失败是 OS 资源问题**：内存不足、达到 pid 上限、超过进程数 ulimit。**必须检查 `pid < 0`**
6. **Windows 兼容性**：`fork` 在 Windows 上不存在（POSIX 仅 Linux/macOS/MinGW/MSYS）——Windows 用 `CreateProcess` + `WaitForSingleObject`

## 工程对照（≥100 字硬约束）

进程管理在本仓库 `engineering/` 中体现于数据库服务与分布式组件：

1. **`engineering/src/db/core/pg_ctl.c` 的 postgres 启停模型**：是守护进程的教科书实现——`fork + setsid + 重定向 stdin/stdout/stderr` + 写 PID 文件 + signal 处理。数据库服务器必须在后台运行、不被 Ctrl+C 杀死
2. **`engineering/src/db/consensus/raft.c` 的子进程选举**：Raft 选举时**不会真 fork**——而是用线程/协程模拟。但其状态机设计借鉴了进程生命周期：candidate → leader → follower 的转换类似 process state machine
3. **`engineering/src/db/core/initdb.c` 的初始化流程**：`initdb -D /path` 创建数据目录是单进程串行操作——所有 PG 风格的工具都遵循"父进程 fork → 子进程执行 → 父进程 wait" 模式
4. **`engineering/src/db/storage/vector/vector_engine.c` 的 background worker**：某些定期 compaction 任务用 detached 线程（pthread_detach）实现——这等价于"线程版守护进程"，但更轻量
5. **`engineering/apps/web/knowledge_hub/server.js`**：Node.js HTTP 服务器用 child_process 模块 spawn 工作进程处理耗时任务——这是高级语言对 fork 的封装
6. **`<sys/wait.h>` 的 W* 宏**：`WIFEXITED(status)` 判正常退出，`WEXITSTATUS(status)` 取退出码，`WIFSIGNALED(status)` 判信号杀死——`engineering/src/db/core/pg_ctl.c` 用全套 W* 宏处理子进程退出

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，凡是用 `system("...")` 调用外部命令的，改为 fork+exec 并显式 waitpid——这能避免 SIGCHLD 泄漏和僵尸进程。