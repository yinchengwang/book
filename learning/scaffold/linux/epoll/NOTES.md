# NOTES - 工程对照

## 工程源码对照

`engineering/src/db/api/` 目录下的代码目前使用 `pthread` 单线程 accept 模式，而非 epoll。

### 当前工程实现（rest_api.c）

```c
// rest_api.c:710
pthread_create(&server->accept_thread, NULL, request_handler_thread, server);
```

当前实现是：主线程 accept → 单线程处理请求。这是简化实现，生产环境应使用 epoll。

### 事件驱动架构演进方向

```c
// 理想的生产级架构（未来可演进）
struct connection_t {
    int fd;
    buffer_t recv_buf;
    buffer_t send_buf;
    enum { STATE_READING, STATE_WRITING, STATE_CLOSING } state;
};

// 事件循环
while (g_running) {
    int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
    for (int i = 0; i < n; i++) {
        if (events[i].data.fd == listen_fd) {
            // accept 新连接
            int cfd = accept(listen_fd, ...);
            add_to_epoll(epfd, cfd, EPOLLIN);
        } else if (events[i].events & EPOLLIN) {
            // 读取请求
            read_request(events[i].data.fd);
        } else if (events[i].events & EPOLLOUT) {
            // 发送响应
            write_response(events[i].data.fd);
        }
    }
}
```

### 与 Redis/Nginx 的对比

| 项目 | 事件驱动方案 | 特点 |
|------|-------------|------|
| 我们的 DB | pthread 单线程 | 简化，易理解 |
| Redis | ae_evict (select/poll/epoll/kqueue) | 跨平台自适应 |
| Nginx | epoll (LT+ET) | 高性能 HTTP 服务器 |

## 工程实践要点

| 模式 | 说明 |
|------|------|
| 非阻塞 I/O | epoll 必须配合 non-blocking socket |
| 边沿触发 | ET 模式需循环读取直到 EAGAIN |
| 连接管理 | 使用结构体存储连接状态，而非仅依赖 fd |
| 心跳检测 | 定期检测空闲连接，关闭超时连接 |

## 为什么 DB 服务器用 pthread 而非 epoll？

1. **简化实现**：单线程模型逻辑简单
2. **同步 API**：存储引擎内部是同步 API，无需复杂的事件分发
3. **pthread_join**：等待线程结束比 epoll 事件循环更直接

**未来演进方向**：如果要支持高并发，可以引入 epoll：
- 连接管理：connection pool
- 请求分发：round-robin 或 work-stealing
- 超时检测：定时器（timerfd 或单独的 timer 线程）

## 学习路径

1. **socket** (本系列第一张卡) → 理解基础 I/O
2. **epoll** (本卡) → 理解高性能事件驱动
3. **后续** → 学习连接池、超时管理、TLS 等高级主题
