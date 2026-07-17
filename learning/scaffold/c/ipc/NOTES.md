# ipc 学习笔记

## 概念地图

POSIX IPC 三大件：

- **pipe（匿名管道）**：内核缓冲区的两个 fd，父子/兄弟进程间单向字节流；半双工
- **FIFO（命名管道）**：`mkfifo(3)` 创建的文件系统实体，路径作为名字；任何两个进程可连接
- **signal（信号）**：`sigaction(2)` 注册处理器；进程间最轻量的"通知"

更现代的 IPC：Unix domain socket (`socketpair` / `bind(unix:/path)`)、POSIX message queue、共享内存 `mmap + shm_open`。但在 R5 学习闭环里 pipe + FIFO + signal 已覆盖典型场景。

跨进程设计原则：父进程持有资源、fork 后子进程继承；两端**必须各自关闭不需要的 fd**（pipe 写端读完要 close）。

## 踩坑记录

1. **MinGW-w64 缺 `<sys/wait.h>`**：本会话在 Windows + mingw 14.2 下尝试 `gcc main.c` 直接 `fatal error: sys/wait.h: No such file or directory`。这是 MinGW 故意剥离的部分 POSIX 头，因为 Windows 的 WaitForSingleObject 模型不同。
2. **本机无法跑通的诚实处理**：spec 要求 Step 2 跑通验证，本会话在 MinGW 上做不到。把 README 显式标注"需 WSL 或 Linux/macOS"，在 NOTES 留下事实，避免后人误把这段 scaffold 当"已验证"。
3. **pipe 是半双工**：本 demo 走"父读 + 子写"单向。若要双向通信需要 2 个 pipe 或用 socketpair(2)。
4. **FIFO 的 open 阻塞**：open(O_RDONLY) 在 FIFO 上等写端，反之亦然；如果只 open 一端可能 hang。父进程 fork 后子进程先 open 写端、退出，父进程 open 读端拿数据，不会死锁。
5. **sigaction 比 signal() 可靠**：老 `signal()` 在不同实现下重启行为不一致；`SA_RESTART` 让慢系统调用自动重启；`SA_RESETHAND` 等同老 BSD 行为。
6. **`pause()` 在收到信号后才返回**：本 demo 用 `while (!g_signal_received) pause()` 等子进程的 SIGUSR1；可以改成 `sigwaitinfo` 或 `signalfd`，但都不是 R5 范畴。

## 工程对照（≥100 字硬约束）

IPC 在工程侧 rag 服务化与 minivecdb 控制面有以下直接迁移价值：

1. **rag server ↔ engineering rest server 的 IPC fallback**：当前 OPSX `rag-remote-index-backend` 用 HTTP 通信；本地/低延迟场景下可考虑 Unix domain socket（POSIX IPC 衍生），本卡刷过后即可读懂 socketpair 与 Unix-domain-bind 路径。
2. **engineering/src/db/core/initdb.c 与 pg_ctl.c**：未来的 daemo 风格控制面（minivecdb initdb）要 spawn 多个子进程（bufmgr、walwriter、autovacuum 等），父子通信用 pipe 与 signal 是 PG 经典手法；本卡刷过后读 PG 源码不再卡 IPC。
3. **engineering/include/db/consensus/raft.h**：raft election 超时由 heartbeat pipe 唤醒；AppendEntries 走 TCP，但 snapshot 触发用 `SIGUSR1` 给 worker 进程；本卡让读者识别 sigaction 在 raft 实现的实际应用。
4. **engineering/src/algo-prod/dist_txn.c 两阶段提交**：参与者向协调者汇报 prepare/decision 用 RPC，但协调者 fork worker 处理并发决策时用 pipe 把决策结果回传主流程，与本卡 demo_pipe 模型同构。

更高阶 IPC（Unix domain socket、`splice`、`vmsplice`、`tee`、eventfd）属 R6+ 学习闭环。
