# NOTES.md — procfs 工程对照

## /proc 文件系统在系统诊断中的工程应用

### 背景

Linux 的 `/proc` 是一个虚拟文件系统，内核将系统运行时信息以文件形式暴露给用户空间。理解和善用 `/proc` 是 Linux 系统诊断的基础。

### 工程对照：数据库 Buffer Pool 诊断

在我们的 `engineering/src/db/storage/bufmgr.c` Buffer Pool 实现中，可以借鉴 procfs 的思路来设计诊断接口：

```c
// bufmgr.c 中的诊断接口设计（参考 procfs 模式）
typedef struct {
    uint64_t total_pages;     // 总页数，对应 MemTotal
    uint64_t free_pages;      // 空闲页数，对应 MemFree
    uint64_t pinned_pages;    // 固定页数（正在使用）
    uint64_t dirty_pages;     // 脏页数（待刷盘）
    uint64_t hit_count;       // 缓存命中次数
    uint64_t miss_count;      // 缓存未命中次数
} BufferPoolStats;

// 诊断函数，类似读取 /proc/meminfo
void buffer_pool_get_stats(BufferPoolStats *stats) {
    pthread_mutex_lock(&pool->mutex);
    stats->total_pages = pool->size;
    stats->free_pages = pool->free_list.size;
    stats->pinned_pages = pool->size - pool->free_list.size;
    stats->dirty_pages = count_dirty_pages(pool);
    stats->hit_count = pool->stats.hit;
    stats->miss_count = pool->stats.miss;
    pthread_mutex_unlock(&pool->mutex);
}
```

### 关键指标映射

| /proc/meminfo | Buffer Pool 诊断 |
|---------------|------------------|
| MemTotal      | pool->size (总页数) |
| MemFree       | free_list.size (空闲页) |
| MemAvailable  | 可用页 = total - pinned |
| Cached        | 缓存的磁盘页数 |
| Buffers       | 元数据/索引缓存 |

### 诊断最佳实践

1. **原子性读取**：使用锁或 RCU 确保统计数据的 Consistency
2. **零拷贝设计**：避免在诊断路径中复制大量数据
3. **格式化输出**：提供易读的分层输出（如 procfs 的 kB 单位）
4. **关键指标精简**：不必暴露所有内部状态，聚焦核心指标

### 性能影响

- 诊断接口应该是**只读**操作，不应触发任何副作用
- 高频诊断（如监控）应考虑采样降频
- 避免在诊断路径中持有长锁

### 扩展思考

生产环境的数据库（如 PostgreSQL、MySQL）都提供类似 `/proc` 的诊断接口：
- PostgreSQL: `pg_stat_bgwriter`、`pg_stat_database`
- MySQL: `SHOW ENGINE INNODB STATUS`

这些接口的设计哲学与 Linux `/proc` 一脉相承：提供统一的、文本化的、只读的系统状态窗口。
