# NOTES.md — io_uring_storage 工程对照

## io_uring 在数据库高速存储中的应用

### 背景

io_uring 是 Linux 5.1+ 的高性能异步 I/O 接口，通过共享内存的环形队列减少系统调用。对于 NVMe SSD 等高速存储，io_uring 可以充分发挥硬件性能，实现百万级 IOPS。

### 工程对照：io_uring WAL 和 Buffer Pool

```c
// io_uring_wal.c — 基于 io_uring 的高性能 WAL
#include <liburing.h>

#define BATCH_SIZE 64

typedef struct {
    struct io_uring ring;
    int wal_fd;
    struct iovec iov[BATCH_SIZE];
    char buffers[BATCH_SIZE][WAL_PAGE_SIZE];
    int pending;
} IoUringWAL;

/* 初始化 io_uring WAL */
int iouring_wal_init(IoUringWAL *wal, const char *path) {
    wal->wal_fd = open(path, O_WRONLY | O_DSYNC);
    if (wal->wal_fd < 0) return -1;

    // 初始化 io_uring
    struct io_uring_params params = {
        .flags = IORING_SETUP_SQPOLL,  // SQ 轮询模式
        .sq_thread_idle = 1000,        // 空闲超时 1s
    };

    if (io_uring_queue_init_params(256, &wal->ring, &params) < 0)
        return -1;

    wal->pending = 0;
    return 0;
}

/* 批量追加 WAL 记录 */
int iouring_wal_append_batch(IoUringWAL *wal,
                               const void *records,
                               int count, size_t record_size) {
    for (int i = 0; i < count; i++) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&wal->ring);
        if (!sqe) break;  // 队列满

        const char *src = (const char *)records + i * record_size;
        size_t offset = wal->pending * WAL_PAGE_SIZE;

        // 准备 I/O
        io_uring_prep_write(sqe, wal->wal_fd, src, record_size, offset);

        wal->pending++;
    }

    // 批量提交
    return io_uring_submit(&wal->ring);
}

/* 等待完成 */
int iouring_wal_wait_batch(IoUringWAL *wal, int min_complete) {
    struct io_uring_cqe *cqe;
    int completed = 0;

    while (completed < min_complete) {
        int ret = io_uring_wait_cqe(&wal->ring, &cqe);
        if (ret < 0) return ret;
        if (cqe->res < 0) return cqe->res;
        io_uring_cqe_seen(&wal->ring, cqe);
        completed++;
    }

    return completed;
}
```

### io_uring 性能优化策略

| 策略 | 实现 | 性能提升 |
|------|------|---------|
| 批量提交 | 合并多个 SQE 一次 submit | 2-5x 吞吐 |
| 固定缓冲区 | 预注册 iovec/buffers | 减少 30% 开销 |
| SQ 轮询 | IORING_SETUP_SQPOLL | 延迟 <5us |
| 链接操作 | IORING_OP_LINK | 原子性保证 |
| 多文件 | 分配多个 ring | 利用多核 |

### 数据库存储 I/O 模式

```c
// 1. WAL 批量刷盘（最高优先级）
void wal_flush_batch(IoUringWAL *wal, LogSegment *seg) {
    // 批量追加
    for (int i = 0; i < seg->dirty_count; i++) {
        PageID page_id = seg->dirty_pages[i];
        Page *page = get_page(page_id);

        io_uring_prep_write(wal->sqe, wal->fd,
                           page, PAGE_SIZE,
                           page_id * PAGE_SIZE);
    }

    // 添加 fsync 确保持久性
    io_uring_prep_fsync(wal->sqe, wal->fd, 0);

    io_uring_submit(&wal->ring);
}

// 2. Buffer Pool 页面预读
void buffer_prefetch(IoUringWAL *ring, BufferPool *pool,
                     PageID start, int count) {
    for (int i = 0; i < count; i++) {
        if (!page_cached(pool, start + i)) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
            void *buf = alloc_buffer_slot(pool);

            io_uring_prep_read(sqe, pool->fd, buf, PAGE_SIZE,
                              (start + i) * PAGE_SIZE);
        }
    }

    io_uring_submit(ring);
}
```

### io_uring 配置建议

| 参数 | 推荐值 | 说明 |
|------|--------|------|
| 队列深度 | 128-512 | 根据并发度调整 |
| SQ 轮询 | 启用 | NVMe 场景延迟降低 10x |
| 缓冲区注册 | 启用 | 减少系统调用 |
| 无锁模式 | 6.0+ | 多进程共享 |

### 性能影响

- 批量提交 64 个请求：比逐个提交快 10-50x
- SQ 轮询模式：延迟从 ~50us 降至 ~5us
- 固定缓冲区：减少 ~30% 的内存分配开销
- 总吞吐：NVMe SSD 可达 1M+ IOPS

### 扩展思考

- Linux 6.1+ 的 io_uring `multishot` 模式
- SPDK + io_uring 的用户态 NVMe 驱动
- io_uring 替代 libaio 成为 Linux AIO 标准
- 分布式存储（MinIO、Ceph）io_uring 集成
