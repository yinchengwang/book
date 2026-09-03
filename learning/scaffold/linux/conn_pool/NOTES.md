# Linux 连接池学习笔记

本文档记录连接池在项目工程代码中的实际应用对照。

## 工程对照

### 1. 数据库连接池

工程代码中实现数据库连接池：

```c
// engineering/src/db/pool/ 中可能有连接池实现
typedef struct db_conn {
    int fd;                     /* Socket 描述符 */
    int in_use;                 /* 是否被占用 */
    time_t last_used;           /* 最后使用时间 */
    struct db_conn *next;
} db_conn_t;

typedef struct db_pool {
    db_conn_t *connections;
    int max_size;
    int current_size;
    pthread_mutex_t lock;
    /* 连接参数 */
    char host[256];
    int port;
    char database[64];
} db_pool_t;
```

**连接池关键设计：**

| 设计点 | 选项 | 说明 |
|--------|------|------|
| 预创建 | 启动时创建 | 减少首次请求延迟 |
| 按需创建 | 首次使用时创建 | 节省资源 |
| 混合策略 | 预创建 + 按需 | 平衡延迟和资源 |

### 2. 连接获取与归还

工程代码中的连接获取模式：

```c
// 获取连接（带超时）
db_conn_t *db_pool_get(db_pool_t *pool, int timeout_ms) {
    pthread_mutex_lock(&pool->lock);

    /* 查找空闲连接 */
    db_conn_t *conn = pool->connections;
    while (conn) {
        if (!conn->in_use) {
            conn->in_use = 1;
            conn->last_used = time(NULL);
            pthread_mutex_unlock(&pool->lock);
            return conn;
        }
        conn = conn->next;
    }

    /* 池未满，按需创建 */
    if (pool->current_size < pool->max_size) {
        conn = db_connect(pool->host, pool->port, pool->database);
        if (conn) {
            conn->in_use = 1;
            conn->next = pool->connections;
            pool->connections = conn;
            pool->current_size++;
        }
    }

    pthread_mutex_unlock(&pool->lock);
    return conn;
}

// 归还连接
void db_pool_put(db_pool_t *pool, db_conn_t *conn) {
    pthread_mutex_lock(&pool->lock);
    conn->in_use = 0;
    conn->last_used = time(NULL);
    pthread_mutex_unlock(&pool->lock);
}
```

### 3. 空闲连接回收

工程代码实现空闲回收：

```c
// 后台线程定期回收
void *pool_recycler(void *arg) {
    db_pool_t *pool = (db_pool_t *)arg;

    while (1) {
        sleep(60);  /* 每分钟检查一次 */

        pthread_mutex_lock(&pool->lock);

        db_conn_t **prev = &pool->connections;
        db_conn_t *conn = pool->connections;

        while (conn) {
            if (!conn->in_use) {
                time_t idle = time(NULL) - conn->last_used;
                if (idle > MAX_IDLE_TIME) {
                    /* 回收空闲连接 */
                    *prev = conn->next;
                    db_disconnect(conn);
                    pool->current_size--;
                    printf("[pool] 回收空闲连接\n");
                    continue;
                }
            }
            prev = &conn->next;
            conn = conn->next;
        }

        pthread_mutex_unlock(&pool->lock);
    }
}
```

### 4. 连接池参数调优

| 参数 | 默认值 | 调优建议 |
|------|--------|----------|
| min_connections | 0 | 根据峰值负载设置 |
| max_connections | 100 | 根据 max_connections 设置 |
| max_idle_time | 600s | 设为连接超时的 2-3 倍 |
| connection_timeout | 30s | 设为查询超时的 1.5 倍 |

### 5. 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 连接泄漏 | 未归还连接 | 使用 RAII 或 finally |
| 连接超时 | 网络问题 | 增加超时、启用重连 |
| 连接耗尽 | 高并发 + 小池 | 增加池大小 |
| 空闲连接断开 | 服务端超时 | 启用心跳检测 |

## 参考源码

- `engineering/src/db/pool/` — 连接池实现
- `engineering/src/db/network/` — 网络层
- `engineering/src/db/server/` — 数据库服务器
