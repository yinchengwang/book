# NOTES.md — blktrace 工程对照

## 块设备 I/O 追踪在数据库存储中的应用

### 背景

`blktrace` 是 Linux 块设备层的追踪工具，能够捕获每个 I/O 请求的完整生命周期。对于数据库存储引擎，理解 I/O 路径是优化性能的关键。

### 工程对照：数据库 Buffer Pool 与磁盘交互

在我们的 `engineering/src/db/storage/bufmgr.c` 中，Buffer Pool 与磁盘的交互正是 blktrace 的典型分析目标：

```c
// bufmgr.c 中的 I/O 操作（blktrace 分析对象）
typedef struct {
    PageID page_id;           // 页面号
    uint32_t flags;           // 读/写/同步标志
    struct timespec submit_time;  // 提交时间
    size_t size;              // I/O 大小
} IoRequest;

/* 页面刷盘（触发写 I/O） */
int buffer_flush_page(BufferPool *pool, PageID page_id) {
    Page *page = find_page(pool, page_id);
    IoRequest req = {
        .page_id = page_id,
        .flags = WRITE | SYNC,
        .size = pool->page_size
    };

    // 记录 I/O 开始时间
    clock_gettime(CLOCK_MONOTONIC, &req.submit_time);

    // 提交到通用块层（blktrace 追踪点在这里）
    int ret = submit_io(&req);
    if (ret < 0) {
        return ERROR;
    }

    // 等待完成
    wait_for_io(&req);

    return SUCCESS;
}
```

### I/O 延迟与数据库性能

| blktrace 事件 | 含义 | 数据库影响 |
|---------------|------|------------|
| Q (Queue) | I/O 请求入队 | 调度效率 |
| G (Get) | 获取块设备 | 路径长度 |
| I (Insert) | 插入调度队列 | 调度算法选择 |
| D (Dispatch) | 发送到设备 | 磁盘利用率 |
| C (Complete) | I/O 完成 | 响应延迟 |

### 数据库 I/O 模式分析

```bash
# 1. 分析读 I/O 延迟分布
sudo blktrace -d /dev/sda -o /tmp/read_trace &
./db_driver "SELECT * FROM big_table LIMIT 10000"
blkparse -i /tmp/read_trace -a completion | awk '{print $11}' | sort -n

# 2. 分析写 I/O 延迟（检查点期间）
sudo blktrace -d /dev/sda -o /tmp/checkpoint &
# 触发检查点
blkparse -i /tmp/checkpoint -a completion | grep 'W ' | awk '{print $11}' | histogram

# 3. 分析同步写（fsync 调用）
blktrace -d /dev/sda -a sync | blkparse -i - | grep 'fsync'
```

### WAL 写入优化

数据库的 Write-Ahead Log (WAL) 是典型的顺序写场景：

```c
// wal.c 中的批量写入优化（减少 I/O 次数）
typedef struct {
    IoRequest requests[BATCH_SIZE];  // 批量请求
    int count;                        // 当前批量大小
    size_t total_bytes;
} WriteBatch;

/* 批量刷盘（合并多个小写入）*/
int wal_flush_batch(WriteBatch *batch) {
    for (int i = 0; i < batch->count; i++) {
        submit_io(&batch->requests[i]);
    }
    // 一次等待所有完成
    wait_for_all(batch);
}
```

### 最佳实践

1. **批量提交**：合并多个小 I/O 为大 I/O
2. **预写日志分离**：WAL 使用独立磁盘/SSD
3. **检查点优化**：分批刷脏页，避免 I/O 突刺
4. **异步 I/O**：使用 io_uring 减少等待

### 性能影响

- blktrace 本身开销 <2%
- 高采样率可能影响 I/O 延迟测量
- 长期追踪产生大量数据

### 扩展思考

PostgreSQL 的 `pg_stat_bgwriter` 与 blktrace 互补：
- `pg_stat_bgwriter`：数据库层面的 I/O 统计
- `blktrace`：操作系统层面的 I/O 追踪
- 两者结合才能完整分析 I/O 瓶颈
