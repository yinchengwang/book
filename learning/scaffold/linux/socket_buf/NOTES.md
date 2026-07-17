# Linux Socket 缓冲区学习笔记

本文档记录 Socket 缓冲区在项目工程代码中的实际应用对照。

## 工程对照

### 1. 网络缓冲区在数据库中的应用

工程代码中数据库服务器的网络层使用 Socket 缓冲区：

```c
// 工程网络模块可能类似这样设置缓冲区
// engineering/src/db/network/ 中的实现
int server_socket_init(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    /* 设置地址复用 */
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /* 根据服务器规模调整缓冲区 */
    int sndbuf = 256 * 1024;  /* 256 KB */
    int rcvbuf = 256 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    return sock;
}
```

### 2. 缓冲区与性能调优

**缓冲区大小对性能的影响：**

| 场景 | 推荐缓冲区 | 原因 |
|------|------------|------|
| 高吞吐量传输 | 大缓冲区 (256KB+) | 减少系统调用次数 |
| 低延迟交互 | 小缓冲区 + TCP_NODELAY | 减少数据等待时间 |
| 高并发连接 | 小缓冲区 | 节省内存，支持更多连接 |

### 3. TCP_NODELAY 与数据库

数据库交互通常需要禁用 Nagle 算法：

```c
// 数据库客户端启用 TCP_NODELAY
// 减少查询响应延迟
int enable = 1;
setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

// 发送查询
send(sock, query, query_len, 0);
// 立即收到响应（不等待 ACK 合并）
```

**Nagle 算法 vs TCP_NODELAY：**

| 模式 | 行为 | 适用场景 |
|------|------|----------|
| Nagle 启用 | 等待 ACK 后发送小包 | 减少网络拥塞 |
| Nagle 禁用 | 立即发送小包 | 低延迟交互 |

### 4. 连接数与内存

每个 Socket 消耗的内存：

```
每个连接内存 = SO_RCVBUF + SO_SNDBUF + 连接结构体
            ≈ 256KB + 256KB + 2KB
            ≈ 514KB

1000 连接 ≈ 500 MB
```

**高并发优化策略：**

1. 减小单个连接缓冲区
2. 使用 epoll 减少线程数
3. 限制最大连接数

### 5. 工程中的网络层封装

工程代码可能封装网络操作为：

```c
// 工程中的网络缓冲区配置接口
typedef struct {
    size_t sndbuf_size;
    size_t rcvbuf_size;
    int tcp_nodelay;
    int tcp_quickack;
} net_buffer_config_t;

// 默认配置
static const net_buffer_config_t default_config = {
    .sndbuf_size = 256 * 1024,
    .rcvbuf_size = 256 * 1024,
    .tcp_nodelay = 1,
    .tcp_quickack = 0,
};
```

## 参考源码

- `engineering/src/db/network/` — 网络层实现
- `engineering/src/db/server/` — 数据库服务器
- `/proc/sys/net/core/` — 系统网络配置
