# daemon 学习笔记

## 概念地图

Linux/UNIX daemon 的核心约定：

- **生命周期独立于登录会话**——不持有 tty
- **单实例**——pidfile 防重
- **可被 init (PID 1) 收养**——两次 fork 让进程脱离父进程组
- **可重读配置**——`SIGHUP` 触发 reload（与传统 inetd 守护呼应）
- **优雅停机**——`SIGTERM` 触发清理

经典 daemonize 五步：
```
fork        // 父进程退出，子进程被 init 收养
setsid      // 子进程成 session leader，脱离 tty
fork        // 二次 fork，确保不会重新获取 tty
umask(0)    // 完整权限控制
chdir(/)    // 不阻塞 umount
close+dup2  // 释放 std fd
open(/dev/null)
```

现代替代：systemd unit；容器环境；nohup + setsid 命令。但本卡聚焦"自己写 daemon"的代码路径——minivecdb initdb/pg_ctl 的风格需要这条能力。

## 踩坑记录

1. **MinGW-w64 缺 `fork()` 完整实现**：本机直接 `#error`。Windows 服务用 `CreateProcess` + `RegisterServiceCtrlHandler`，完全不同的 API 哲学。
2. **不二次 fork 的隐患**：第二次 fork 是 Stevens APUE 推荐的"防止 daemon 重新获取 tty"防御层。本 demo 严格按 APUE 模型走两次 fork。
3. **`dev/null` 重定向 std fd 后丢错误**：本 demo 把 STDIN/STDOUT/STDERR 都 dup2 到 /dev/null；调试时需要 `--background` 不传，从 stderr 还能看到东西。本 README 说明前台跑法。
4. **pidfile 路径硬编码**：用 `/tmp/daemon-scaffold.pid`。生产：`/var/run/<name>.pid`，需 root 或 `$HOME/.local/run/` 替代。
5. **unlink 的并发**：daemon 正常停止会 unlink pidfile；但被 SIGKILL（kill -9）的进程不会 unlink，留 stale pidfile。下次启动需检测 stale pid 并覆盖或 error 退出。本 demo **简化**：总是写覆盖。
6. **chdir 到根**带来的问题：未来 NFS 挂载 umount 会失败。本 demo 沿用 APUE 老教程，但实际应保留 cwd。

## 工程对照（≥100 字硬约束）

daemon 模型在工程侧有以下直接迁移价值：

1. **minivecdb initdb/pg_ctl 风格控制面**：未来 minivecdb 启动要把 `initdb`（创建 data dir）、`postgres`（数据库服务）、`pg_ctl`（start/stop）分进程。这些进程都是 daemon。本卡刷过后能写出 initdb 的 main 循环。
2. **rag server 独立进程**：当前 OPSX `rag-remote-index-backend` 的 RAG Server 是单进程多 client；未来要支持"standalone daemon + sd_notify 与 systemd 协作"。本卡提供 daemonize 骨架，把 `rag_server.c` 的 `main` 改为"先 daemonize 再 enter event loop"。
3. **`engineering/src/db/core/db_server.c` 的服务端运行模式**：当前硬编码 `port=5432`，未来交付 systemd unit、reload 配置、健康检查端点，都依赖本卡的内容。
4. **`engineering/src/db/consensus/raft.c` 中 follower / leader 的进程模型**：raft 节点在多副本集群中是 daemon 进程，pidfile + SIGHUP reload config 是落地 PG/Pglogical 风格的标配。
5. **pg_dump 风格备份工具**：`engineering/src/algo-prod/backup/agent.c`（假设未来）需要 24/7 在线 daemon，本卡提供 daemon+signal 一体骨架。

**与 §6 signal 互引**：

- signal 提供 handler 注册与 flag 模式
- 本卡提供 fork 后**必须重置 handler** 的方法：父进程的 SIGTERM handler 在子进程继承后仍指向旧 handler——若函数指针指向 loaded module 的地址，daemon 化后可能导致 segfault。生产代码在 `daemonize()` 后 `signal(SIGTERM, SIG_DFL)` 重置，handler 由 `install_handler()` 重新注册。本 demo 顺序正确。
- 完整的"daemon + signal + pidfile + reload"四件套是 PG `postmaster.c` 的核心模型；本卡刷过后读者能读懂那段五千行代码的骨架。

## 与 §9 makefile 的关系

daemon 化进程要装成 systemd unit 时需要 `[Service] ExecStart=...`；本 daemon_demo 的 Makefile 没有 `install` 目标——读者可加 `install: all`、`install -m 0755 daemon_demo /usr/local/bin/`、`install -m 0644 daemon.service /etc/systemd/system/`，这是 §9 的伏笔。
