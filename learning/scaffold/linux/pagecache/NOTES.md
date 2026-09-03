# Linux 页缓存学习笔记

本文档记录页缓存机制在项目工程代码中的实际应用对照。

## 工程对照

### 1. Buffer Pool 与页缓存

工程代码中 Buffer Pool 与页缓存的关系：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// Buffer Pool 实际上是应用层的缓存
// 与内核页缓存形成两级缓存架构
```

**缓存层次：**

| 层级 | 缓存内容 | 管理方 |
|------|----------|--------|
| Buffer Pool | 数据库页面 | 数据库进程 |
| 页缓存 | 文件内容 | 内核 |

### 2. 直接 I/O 在数据库中的应用

数据库使用 O_DIRECT 绕过页缓存：

```c
// engineering/src/db/storage/page/disk.c
// 使用直接 I/O 避免双缓存

#ifdef USE_DIRECT_IO
    int flags = O_RDWR | O_DIRECT;
#else
    int flags = O_RDWR;
#endif

int fd = open(path, flags, 0644);
```

**直接 I/O 的优势：**

1. 避免数据在用户态和内核态之间复制
2. 精确控制缓存内容
3. 数据库可以使用自己的缓存策略

### 3. 预读策略

工程代码实现顺序扫描预读：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// 检测顺序扫描模式，触发预读

static void check_sequential_scan(buffer_pool_t *pool, page_id_t page_id) {
    if (page_id == pool->last_read_page + 1) {
        pool->seq_scan_count++;
        if (pool->seq_scan_count > 3) {
            /* 触发预读 */
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

### 4. 脏页回写策略

```c
// engineering/src/db/storage/buffer/bufmgr.c
// 脏页回写策略

typedef enum {
    WRITE_BACK,    /* 延迟写，性能高，可能丢数据 */
    WRITE_THROUGH, /* 同步写，性能低，数据安全 */
} write_policy_t;

// Clock-Sweep 驱逐时检查脏页
if (block->is_dirty) {
    disk_write_page(pool->disk, block->page_id, block->data);
    block->is_dirty = 0;
}
```

### 5. fsync 与数据持久化

```c
// 工程中使用 fsync 确保数据落盘
// WAL 模块确保事务持久性

// wal.c
int wal_flush(wal_t *wal) {
    if (write(wal->fd, wal->buf, wal->offset) < 0) {
        return -1;
    }
    if (fsync(wal->fd) < 0) {  // 确保持久化
        return -1;
    }
    wal->offset = 0;
    return 0;
}
```

## 页缓存与数据库性能

### 缓存命中场景

```
读取已缓存页面:
  时间: ~0.1ms (内存访问)

读取未缓存页面:
  时间: ~5-10ms (SSD) / ~50ms (HDD)
```

### 性能调优建议

| 参数 | 说明 | 建议 |
|------|------|------|
| shared_buffers | 数据库缓存大小 | 25% 物理内存 |
| effective_cache_size | 预估内核缓存 | 75% 物理内存 |
| random_page_cost | 随机读成本 | SSD: 1.1, HDD: 4.0 |

## 参考源码

- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 实现
- `engineering/src/db/storage/page/disk.c` — 页面 I/O
- `engineering/src/db/storage/wal/wal.c` — WAL 日志
