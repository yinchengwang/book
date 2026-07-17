# Linux 块设备层学习笔记

本文档记录块设备抽象层机制在项目工程代码中的实际应用对照。

## 工程对照：块设备抽象 → 数据库存储引擎 IO 调度对照

### 1. 两级 IO 调度架构

Linux 内核和数据库存储引擎都实现了 IO 调度层，形成两级调度架构：

```
应用层:   数据库查询引擎 (SQL Executor)
              |
调度层1:  数据库 Buffer Pool (Clock-Sweep)
              |  脏页回写、预读、页面置换
              |
内核层:   VFS / 页缓存
              |
调度层2:  块设备层 (bio → request → IO 调度器)
              |  合并、排序、公平调度
              |
硬件层:   磁盘驱动 (NVMe/SCSI)
```

### 2. Buffer Pool 的 IO 调度

工程代码中 Buffer Pool 实现了类似块设备层的调度策略：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// Clock-Sweep 置换算法 —— 类似 IO 调度器的请求选择

typedef struct buffer_pool_t {
    page_t      *pages;           // 缓冲池页面数组
    int          clock_hand;      // Clock 指针（类似调度器的当前请求指针）
    int          num_pages;       // 总页面数
    hash_table_t *page_table;     // 页面哈希表（快速查找，类似 bio 合并查找）
} buffer_pool_t;

// Clock-Sweep: 选择淘汰页面（类似 IO 调度器选择下一个请求）
static int clock_sweep(buffer_pool_t *pool) {
    while (1) {
        page_t *page = &pool->pages[pool->clock_hand];
        if (page->ref_count == 0 && !page->is_dirty) {
            return pool->clock_hand;  // 找到可淘汰页面
        }
        page->ref_count = 0;  // 给第二次机会
        pool->clock_hand = (pool->clock_hand + 1) % pool->num_pages;
    }
}
```

### 3. 请求合并对应：顺序扫描优化

工程代码检测顺序访问并触发预读，类似块设备层的后向合并：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// 顺序扫描检测 → 预读（对应块设备层的后向合并 + 预读）

static void check_sequential_scan(buffer_pool_t *pool, page_id_t page_id) {
    if (page_id == pool->last_read_page + 1) {
        pool->seq_scan_count++;
        if (pool->seq_scan_count > 3) {
            /* 连续访问检测：触发预读（类似块设备的合并+预读） */
            for (int i = 1; i <= READAHEAD_PAGES; i++) {
                buffer_prefetch(pool, page_id + i);
            }
        }
    } else {
        pool->seq_scan_count = 0;
    }
    pool->last_read_page = page_id;
}
```

### 4. WAL 日志写入调度

WAL 日志写入对应块设备层的写入调度策略：

```c
// engineering/src/db/storage/wal/wal.c
// WAL 日志追加写 —— 天然顺序写，类似 noop 调度器最优场景

int wal_flush(wal_t *wal) {
    // 批量写入 WAL 缓冲区（合并多次小写为一次大写入）
    if (write(wal->fd, wal->buf, wal->offset) < 0) {
        return -1;
    }
    if (fsync(wal->fd) < 0) {
        return -1;
    }
    wal->offset = 0;
    return 0;
}
```

### 5. 调度策略对比

| 概念 | Linux 块设备层 | 数据库存储引擎 |
|------|---------------|---------------|
| 请求单元 | bio (4KB~1MB) | 数据库页面 (8KB) |
| 合并策略 | 前向/后向扇区合并 | 顺序扫描检测 + 预读 |
| 调度算法 | noop/deadline/bfq/kyber | Clock-Sweep + LRU |
| 队列模型 | blk-mq (Per-CPU 队列) | Buffer Pool 哈希表 + 自由链表 |
| 写入优化 | 写合并 + 写屏障 | WAL 顺序写 + 组提交 |
| 公平性 | BFQ 按进程预算 | 连接池 + 查询超时控制 |

### 6. 数据库 IO 调度建议

| 存储介质 | 推荐调度器 | 数据库参数建议 |
|----------|-----------|---------------|
| NVMe SSD | none (kyber) | effective_io_concurrency=200+ |
| SATA SSD | mq-deadline | random_page_cost=1.1 |
| HDD | mq-deadline/bfq | random_page_cost=4.0 |

## 参考源码

- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 实现（Clock-Sweep 调度）
- `engineering/src/db/storage/wal/wal.c` — WAL 日志（顺序写优化）
- `engineering/src/db/storage/page/disk.c` — 页面 I/O（磁盘读写封装）
