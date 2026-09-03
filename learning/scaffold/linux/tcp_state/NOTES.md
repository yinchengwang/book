# Linux TCP 状态学习笔记

本文档记录 TCP 连接诊断在项目工程代码中的实际应用对照。

## 工程对照

### 1. TCP 连接与数据库连接

工程中数据库服务器监听 TCP 端口：

```c
// 工程代码中数据库监听连接
// 类似 demo 中读取 LISTEN 状态
typedef struct {
    char local_ip[INET_ADDRSTRLEN];
    int local_port;
    char remote_ip[INET_ADDRSTRLEN];
    int remote_port;
    tcp_state_t state;
} connection_info_t;
```

**典型数据库端口：**

| 服务 | 端口 | 状态 |
|------|------|------|
| PostgreSQL | 5432 | LISTEN |
| MySQL | 3306 | LISTEN |
| Redis | 6379 | LISTEN |

### 2. TCP 连接状态监控

工程代码实现连接状态监控：

```c
// 类似 demo 中的统计逻辑
typedef struct {
    int established;
    int time_wait;
    int close_wait;
    int listen;
    int other;
} tcp_stats_t;

int get_tcp_stats(const char *proto, tcp_stats_t *stats) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/net/%s", proto);

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[1024];
    fgets(line, sizeof(line), fp);  // 跳过标题

    while (fgets(line, sizeof(line), fp)) {
        unsigned int state_hex;
        sscanf(line, "%*d: %*X:%*X %*X:%*X %X", &state_hex);
        // 统计各状态数量
    }

    fclose(fp);
    return 0;
}
```

### 3. 连接泄漏检测

工程代码检测连接泄漏：

```c
// 定时检查 ESTABLISHED 连接数
void check_connection_leak(void) {
    tcp_stats_t stats = {0};
    get_tcp_stats("tcp", &stats);

    if (stats.established > MAX_ALLOWED_CONNECTIONS) {
        log_error("连接数过多: %d > %d", stats.established, MAX_ALLOWED_CONNECTIONS);
        // 触发告警或拒绝新连接
    }

    // 检测 TIME_WAIT 积累
    if (stats.time_wait > 10000) {
        log_warn("TIME_WAIT 连接过多: %d", stats.time_wait);
    }
}
```

### 4. 网络缓冲区调优

工程代码根据连接状态调整缓冲区：

```c
// 根据连接数动态调整
if (stats.established > 1000) {
    // 高并发时减小缓冲区以支持更多连接
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &small_buf, sizeof(small_buf));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &small_buf, sizeof(small_buf));
} else {
    // 正常时使用大缓冲区提升吞吐
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &large_buf, sizeof(large_buf));
}
```

### 5. TCP 状态与故障诊断

| 状态异常 | 可能原因 | 排查方法 |
|----------|----------|----------|
| TIME_WAIT > 10000 | 短连接频繁 | 启用 tcp_tw_reuse |
| CLOSE_WAIT > 100 | 代码未 close() | 检查连接释放逻辑 |
| SYN_RECV > 1000 | SYN 攻击 | 启用 syncookies |
| ESTABLISHED 接近上限 | 连接泄漏 | 检查连接池 |

## 工程网络模块

工程代码可能位于：
- `engineering/src/db/network/` — 网络层实现
- `engineering/src/db/server/` — 数据库服务器

## 参考资料

- `man tcp` — TCP 协议说明
- `/proc/net/tcp` — 内核文档
- `ss` 命令源码 — 更好的连接统计工具
