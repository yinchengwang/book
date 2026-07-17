# NOTES.md — net_diag 工程对照

## 网络诊断在数据库连接管理中的应用

### 背景

Linux `/proc/net/` 系列接口提供网络层的所有统计，包括 TCP 连接状态、SNMP 统计、接收/发送队列等。数据库作为网络服务，连接管理直接影响性能。

### 工程对照：数据库连接管理

在我们的 `engineering/src/db/core/db_server.c` 中：

```c
// db_server.c 中的连接统计（参考 /proc/net/snmp）
typedef struct {
    uint64_t active_connections;    // 当前活跃连接
    uint64_t total_connections;     // 历史总连接
    uint64_t rejected_connections;  // 被拒绝的连接
    uint64_t idle_connections;      // 空闲连接
    uint64_t connections_waiting;   // 等待中连接
} ConnectionStats;

/* 连接接受循环 */
void server_accept_loop(ServerContext *ctx) {
    while (ctx->running) {
        int client_fd = accept(ctx->listen_fd, NULL, NULL);
        if (client_fd < 0) continue;

        pthread_mutex_lock(&ctx->stats_lock);
        if (ctx->stats.active_connections >= ctx->max_connections) {
            // 连接队列满
            ctx->stats.rejected_connections++;
            pthread_mutex_unlock(&ctx->stats_lock);
            close(client_fd);
            continue;
        }

        ctx->stats.active_connections++;
        ctx->stats.total_connections++;
        pthread_mutex_unlock(&ctx->stats_lock);

        // 为每个客户端分配线程
        spawn_worker(ctx, client_fd);
    }
}
```

### SNMP 统计与数据库指标

| /proc/net/snmp 字段 | 数据库含义 | 监控动作 |
|---------------------|-----------|---------|
| tcp_curr_estab | 活跃连接数 | 超 max_connections 告警 |
| tcp_retrans_segs | 重传段数 | 网络故障警告 |
| tcp_in_errs | TCP 错误 | 连接质量下降 |
| udp_no_ports | 端口不可达 | 服务未监听 |
| ip_in_hdr_errors | IP 头错误 | 网络层故障 |

### 数据库服务器诊断

```bash
# 1. 检查 listen 端口
ss -tlnp | grep 5432

# 2. 统计 TIME_WAIT 数量（短连接）
ss -tan state time-wait | wc -l

# 3. 检查连接队列溢出
netstat -s | grep -i "listen queue"
# 输出: 123 times the listen queue of a socket overflowed

# 4. 检查重传率
netstat -s | grep retrans
# 高重传率 → 网络拥塞或 buffer 不足
```

### 连接池优化

```c
// conn_pool.c 中的连接复用（减少 TIME_WAIT）
typedef struct {
    Connection *conns[MAX_POOL_SIZE];
    int size;
    int active_count;
    pthread_mutex_t mutex;
} ConnectionPool;

/* 从池中获取连接 */
Connection* pool_acquire(ConnectionPool *pool) {
    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < pool->size; i++) {
        if (!pool->conns[i]->in_use) {
            pool->conns[i]->in_use = true;
            pool->active_count++;
            pthread_mutex_unlock(&pool->mutex);
            return pool->conns[i];
        }
    }
    // 池耗尽
    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}
```

### 监控最佳实践

1. **listen queue**：`net.core.somaxconn` 设置至少为 max_connections
2. **TIME_WAIT**：关注 netstat 统计，>1000 考虑连接复用
3. **重传率**：<0.1% 为正常，>1% 需要排查网络
4. **UDP 丢包**：`net.core.rmem_max/wmem_max` 增大缓冲区

### 性能影响

- 连接建立时 TCP 三次握手约 1-2 RTT
- TIME_WAIT 状态持续 2MSL（约 60s）
- 连接数过多 → 上下文切换开销增大

### 扩展思考

PostgreSQL 的连接配置：
- `max_connections`：硬限制，100-500 通常
- `superuser_reserved_connections`：为管理员保留
- PgBouncer：连接池中间件，复用后端连接
- 高并发场景建议使用连接池而非增加 max_connections
