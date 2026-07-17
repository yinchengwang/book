# SO_REUSEPORT 工程对照笔记

## 多核扩展场景

SO_REUSEPORT 解决了传统单 socket 监听的多核扩展瓶颈。在传统模式下，所有进入的连接都由单个 accept() 串行处理，即使服务器有多个 CPU 核心，也无法充分利用。

使用 SO_REUSEPORT 后：
- 每个工作进程/线程创建独立的监听 socket
- 内核在各 socket 间自动负载均衡
- 各进程可绑定到不同 CPU 核心（通过 `sched_setaffinity`）
- 避免用户态的锁竞争，减少 accept() 惊群问题

## 典型应用：Nginx 多进程监听

Nginx 在多进程模式下使用 SO_REUSEPORT 实现高性能：

```nginx
# worker_processes auto;  # 多个 worker 进程
# 每个 worker 独立监听相同端口
```

配置方式：
```bash
# 内核自动在多个 worker 间分配连接
# 配合 CPU affinity 效果更佳
worker_cpu_affinity auto;
```

Nginx 的负载分发特点：
- 同一 source IP 的连接倾向于同一 worker（会话保持）
- 避免 accept() 惊群（通过锁或 SO_REUSEPORT）
- 配合 worker_connections 实现高并发

## 典型应用：Redis 多进程

Redis Cluster 或 Redis 6.0+ 多线程模式中：
- 主进程监听端口，创建多个 socket
- 子进程继承 socket，利用 SO_REUSEPORT 特性
- 每个子进程处理不同连接，减少锁竞争

## 数据库服务器的多核扩展策略

数据库服务器（如 PostgreSQL、MySQL）通常采用以下多核扩展方案：

### 方案一：进程池 + SO_REUSEPORT
```
[Listener 1] → [Worker 进程 1] → CPU Core 0
[Listener 2] → [Worker 进程 2] → CPU Core 1
[Listener 3] → [Worker 进程 3] → CPU Core 2
...
```

### 方案二：连接池 + 异步 I/O
- 单进程多线程模型
- epoll/kqueue/IOCP 处理所有连接
- 适合 I/O 密集型工作负载

### 方案三：PostgreSQL 架构
```
PostgreSQL 采用进程模型：
├── postgres (主进程) - 监听端口，分发连接
│   ├── postgres (worker 1) - 处理查询
│   ├── postgres (worker 2) - 处理查询
│   └── ...
```

PostgreSQL 的多核扩展通过：
- 连接池管理器分配连接
- 每个后端进程独立处理查询
- 共享内存用于缓冲区管理
- 进程间通过信号和共享内存通信

### 方案四：MySQL 架构
```
MySQL 采用线程模型：
├── mysqld (主线程) - 接受连接
├── thread_per_connection - 每连接一线程
└── thread pool (企业版) - 线程池复用
```

### 关键设计考量

1. **连接分配策略**：随机、轮询、哈希（SO_REUSEPORT 使用源 IP 哈希）
2. **CPU 亲和性**：将特定 socket 绑定到特定核心
3. **NUMA 感知**：多路服务器上考虑本地内存访问
4. **会话状态**：无状态服务适合水平扩展，有状态服务需要会话亲和性

## 参考资料

- Linux kernel: `net/core/sock.c` 中 SO_REUSEPORT 实现
- Nginx upstream 模块的负载均衡策略
- Redis 源码 `server.c` 中的端口监听逻辑
