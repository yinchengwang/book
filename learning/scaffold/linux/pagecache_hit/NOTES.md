# NOTES.md — pagecache_hit 工程对照

## 页缓存命中率在数据库 Buffer Pool 中的应用

### 背景

Linux 页缓存是操作系统层面的磁盘缓存，而数据库 Buffer Pool 是数据库进程私有的缓存。两者都需要关注命中率这一核心指标。

### 工程对照：Buffer Pool 命中率

在我们的 `engineering/src/db/storage/bufmgr.c` 中，Buffer Pool 的命中率分析与页缓存类似：

```c
// bufmgr.c 中的命中率统计
typedef struct {
    uint64_t hit_count;      // 缓存命中次数
    uint64_t miss_count;     // 缓存未命中次数
    uint64_t read_count;     // 页面读取次数
    uint64_t write_count;    // 页面写入次数
    uint64_t evict_count;    // 页面淘汰次数
} BufferPoolStats;

/* 获取页面（可能有缓存未命中）*/
Page* buffer_get_page(BufferPool *pool, PageID page_id) {
    // 1. 先查 Hash 表
    BufferDesc *desc = hash_lookup(pool->hash_table, page_id);
    if (desc) {
        // 命中！
        pool->stats.hit_count++;
        pin_page(desc);
        return &desc->page;
    }

    // 2. 未命中，需要从磁盘加载
    pool->stats.miss_count++;
    desc = alloc_buffer_slot(pool);
    if (!desc) {
        // 需要淘汰页面
        desc = select_victim(pool);
        evict_page(desc);
        pool->stats.evict_count++;
    }

    disk_read(pool->fd, page_id, &desc->page);
    pool->stats.read_count++;
    hash_insert(pool->hash_table, page_id, desc);
    pin_page(desc);

    return &desc->page;
}

/* 计算命中率 */
double buffer_get_hit_ratio(const BufferPool *pool) {
    uint64_t total = pool->stats.hit_count + pool->stats.miss_count;
    if (total == 0) return 0.0;
    return 100.0 * pool->stats.hit_count / total;
}
```

### 命中率指标对比

| 指标 | Linux 页缓存 | 数据库 Buffer Pool |
|------|--------------|-------------------|
| 命中 | pgmajfault=0 | hit_count++ |
| 未命中 | pgmajfault>0 | miss_count++ |
| 淘汰 | pgsteal | evict_count |
| 写回 | pgpgout | write_count |

### 数据库缓存优化策略

```c
// 1. 热点页面保持常驻（避免被淘汰）
void pin_critical_pages(BufferPool *pool) {
    // 系统表页面永不淘汰
    for (int i = 0; i < catalog_page_count; i++) {
        Page *p = buffer_get_page(pool, CATALOG_PAGE_BASE + i);
        // 标记为不可淘汰
        p->flags |= PAGE_PINNED;
    }
}

// 2. 批量预读（利用顺序访问局部性）
void prefetch_pages(BufferPool *pool, PageID start, int count) {
    for (int i = 0; i < count; i++) {
        PageID page_id = start + i;
        if (!hash_lookup(pool->hash_table, page_id)) {
            // 异步预读，不阻塞
            async_read(pool->fd, page_id, &pool->prefetch_buf[i]);
        }
    }
}

// 3. 脏页批量刷盘（减少 I/O 次数）
void flush_dirty_pages_batch(BufferPool *pool, int batch_size) {
    BufferDesc *descs[BATCH_SIZE];
    int n = select_dirty_pages(pool, descs, batch_size);

    // 合并写请求
    io_batch_submit(pool->io_ctx, descs, n);
    io_batch_wait(pool->io_ctx);
}
```

### 监控指标

PostgreSQL 的缓存监控：
```sql
-- 缓冲区缓存命中率
SELECT
    blk_read_time / 1000.0 as "磁盘读秒",
    blk_write_time / 1000.0 as "磁盘写秒",
    blks_hit,
    blks_read,
    100.0 * blks_hit / (blks_hit + blks_read) as "命中率%"
FROM pg_stat_database
WHERE datname = 'mydb';
```

### 最佳实践

1. **shared_buffers 合理设置**：通常为机器内存的 25%
2. **预读策略**：顺序扫描时启用，预读未来可能访问的页
3. **脏页刷盘节奏**：平衡内存压力和 I/O 开销
4. **表/索引对齐**：按页大小对齐减少碎片

### 性能影响

- 命中率高（>95%）→ 查询延迟低、吞吐高
- 命中率低 → 大量磁盘 I/O，查询变慢
- 脏页过多 → 内存压力、丢失风险增加

### 扩展思考

现代数据库的 adaptive buffer pool：
- 根据访问模式动态调整缓存策略
- 机器学习预测热点页面
- 分层缓存（DRAM + NVMe SSD）
