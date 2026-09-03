# Linux 磁盘 I/O 诊断学习笔记

本文档记录磁盘 I/O 在项目工程代码中的实际应用对照。

## 工程对照

### 1. 页面读写与磁盘 I/O

在项目 `engineering/src/db/storage/page/disk.c` 中，页面读写直接触发磁盘 I/O：

```c
// disk.c 第 89-101 行
ssize_t file_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    ssize_t written = pwrite(fd, buf, count, offset);
    if (written < 0) {
        return -1;
    }
    return written;
}
```

**I/O 层次分析：**

| 层级 | 操作 | 触发磁盘 |
|------|------|----------|
| Buffer Pool | buffer_get_page() | 否（命中缓存） |
| Buffer Pool | buffer_flush_page() | 是（脏页刷盘） |
| WAL | wal_write() | 是（写日志） |
| Page File | file_pwrite() | 是（直接写） |

### 2. Buffer Pool 与 I/O 合并

工程代码通过 Buffer Pool 减少直接磁盘 I/O：

```c
// bufmgr.c 第 201-215 行
page_t *buffer_get_page(buffer_pool_t *pool, page_id_t page_id) {
    /* 1. 先查哈希表 */
    buf_block_t *block = buf_hash_lookup(&pool->hash, page_id);
    if (block) {
        block->ref_count++;  /* 增加引用 */
        return block->data;
    }

    /* 2. 缓存未命中，从磁盘读取 */
    block = buf_select_victim(pool);  /* 选择淘汰页 */
    if (block->is_dirty) {
        disk_write_page(pool->disk, block->page_id, block->data);  /* 刷盘 */
    }

    disk_read_page(pool->disk, page_id, block->data);  /* 读入新页 */
    buf_hash_insert(&pool->hash, page_id, block);
    return block->data;
}
```

**I/O 合并策略：**

1. **脏页批量刷盘**：减少随机写
2. **预读（Read-Ahead）**：顺序扫描时预取多页
3. **延迟写（Write-Behind）**：合并多次小写

### 3. WAL 与磁盘同步

WAL 模块控制 I/O 同步策略：

```c
// wal.c 第 78-95 行
int wal_write(wal_t *wal, const void *data, size_t len) {
    /* 1. 写入 WAL 缓冲区 */
    memcpy(wal->buf + wal->offset, data, len);
    wal->offset += len;

    /* 2. 缓冲区满时刷盘 */
    if (wal->offset >= WAL_BUFFER_SIZE) {
        wal_flush(wal);  /* fsync 确保持久化 */
    }
    return 0;
}
```

**fsync 的作用：**

- 确保数据写入磁盘（不只是在页缓存）
- 每次事务提交时调用
- 代价较高，是数据库性能瓶颈之一

### 4. I/O 性能调优

工程代码支持多种 I/O 模式：

```c
// disk.c 第 45-52 行
typedef enum {
    IO_SYNC,      /* 同步 I/O，每次 write 后 fsync */
    IO_ASYNC,     /* 异步 I/O，使用 O_DIRECT 绕过缓存 */
    IO_BATCHED,   /* 批量 I/O，定期同步 */
} io_mode_t;
```

**各模式对比：**

| 模式 | 性能 | 可靠性 | 适用场景 |
|------|------|--------|----------|
| IO_SYNC | 低 | 高 | WAL 日志 |
| IO_ASYNC | 高 | 低 | 临时数据 |
| IO_BATCHED | 中 | 中 | 用户数据 |

## 参考源码

- `engineering/src/db/storage/page/disk.c` — 页面文件读写
- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 管理
- `engineering/src/db/storage/wal/wal.c` — WAL 日志写入
