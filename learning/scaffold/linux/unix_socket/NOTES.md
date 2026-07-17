# Unix Domain Socket 工程对照笔记

## 本地进程通信场景

Unix Domain Socket 是高性能本地 IPC 的首选方案。当进程间通信发生在同一台机器上时，使用 Unix 域 socket 相比 TCP 环回（127.0.0.1）有显著优势：

- **零拷贝优化**：数据在内核空间直接传递，无需经过协议层处理
- **更少上下文切换**：避免了网络协议栈的复杂处理
- **文件系统权限天然隔离**：socket 文件的权限位直接控制访问权限

典型应用场景包括：
- 守护进程与控制面板通信（如 Docker daemon）
- 桌面环境组件间通信（如 X11、Wayland）
- 应用服务器与 FastCGI 进程通信
- 数据库客户端与本地服务通信

## 工程中的应用

在本项目工程代码中，Redis 源码（`reference/open-source/redis/`）大量使用 Unix Domain Socket 实现客户端-服务器通信。Redis 配置文件中通过 `unixsocket` 和 `unixsocketperm` 参数启用：

```bash
unixsocket /var/run/redis.sock
unixsocketperm 700
```

这种方式比 TCP 连接更安全（不受网络攻击）、性能更高（无需经过 TCP/IP 协议栈），适合服务器监听本地连接的场景。

进程间通信（IPC）在数据库存储引擎中也是核心需求：Buffer Pool 与 WAL 模块、Catalog 与事务管理器之间都存在高频数据交换，虽然当前实现使用共享内存+信号量，但 Unix Domain Socket 提供了另一种可选的进程隔离通信方案。

## TCP 环回 vs Unix 域 socket

| 维度 | TCP 环回 (127.0.0.1:port) | Unix Domain Socket |
|------|--------------------------|-------------------|
| 协议栈开销 | 完整的 TCP/IP 处理 | 直接内核传递 |
| 端口占用 | 需要分配端口号 | 使用文件系统路径 |
| 权限控制 | 依赖防火墙/iptables | 使用 chmod 直接控制 |
| 跨容器通信 | 支持（通过容器网络） | 同一主机内 |
| 调试便利性 | 可用 telnet/nc 测试 | 需要专用客户端 |

对于数据库存储引擎这类需要极致性能的场景，Unix Domain Socket 是本地 IPC 的首选方案，既保证了进程隔离的安全性，又获得了接近零开销的通信效率。
