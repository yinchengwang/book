# NOTES.md — iouring 工程对照

## io_uring 在数据库存储引擎中的应用

### 背景

io_uring 是 Linux 5.1+ 引入的高性能异步 I/O 接口，通过共享内存的环形队列（SQ/CQ）减少系统调用。对于数据库这类 I/O 密集应用，io_uring 可带来 20-50% 的吞吐提升。

### 工程对照：WAL 和 Buffer Pool 的 io_uring

在我们的 `engineering/src/db/storage/` 模块中：

```c
// wal_io_uring.c — 基于 io_uring 的 WAL 写入
#include <liburing.h>

typedef struct {
    struct io_uring ring;
    int wal_fd;
    void *pending_writes[64];
    int pending_count;
} IoUringWal;

/* 初始化 io_uring WAL */
int iouring_wal_init(IoUringWal *wal, const char *path) {
    wal->wal_fd = open(path, O_WRONLY | O_DSYNC);
    if (wal->wal_fd < 0) return -1;

    // 初始化 io_uring，队列深度 64
    if (io_uring_queue_init(64, &wal->ring, 0) < 0)
        return -1;

    wal->pending_count = 0;
    return 0;
}

/* 批量提交 WAL 记录 */
int iouring_wal_append_batch(IoUringWal *wal,
                               const void *records,
                               int num_records, size_t record_size) {
    for (int i = 0; i < num_records; i++) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&wal->ring);
        if (!sqe) break;

        const char *buf = (const char *)records + i * record_size;
        io_uring_prep_write(sqe, wal->wal_fd,
                           buf, record_size,
                           i * record_size);

        wal->pending_count++;
    }

    // 一次性提交
    int submitted = io_uring_submit(&wal->ring);
    if (submitted < 0) return -1;

    // 等待所有完成
    struct io_uring_cqe *cqe;
    for (int i = 0; i < submitted; i++) {
        int ret = io_uring_wait_cqe(&wal->ring, &cqe);
        if (ret < 0) return ret;
        if (cqe->res < 0) return cqe->res;
        io_uring_cqe_seen(&wal->ring, cqe);
    }

    wal->pending_count = 0;
    return submitted;
}

/* 异步预读页面（用于 Buffer Pool）*/
void iouring_async_readahead(struct io_uring *ring,
                               int fd, PageID *pages, int count,
                               void *buf) {
    for (int i = 0; i < count; i++) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe) break;

        off_t offset = pages[i] * PAGE_SIZE;
        void *dest = (char *)buf + i * PAGE_SIZE;

        io_uring_prep_read(sqe, fd, dest, PAGE_SIZE, offset);
    }

    // 异步提交，不阻塞
    io_uring_submit(ring);
}
```

### io_uring vs 传统 I/O

| 方案 | 系统调用 | 拷贝 | 延迟 | 吞吐 |
|------|---------|------|------|------|
| read/write | 每次 1 次 | 1 次 | 基准 | 基准 |
| libaio | 每次 2 次 | 1 次 | 稍好 | 1.5x |
| mmap | 按需 | 0 次 | 首次较高 | 2-5x |
| io_uring | 批量提交 | 0 次(寄存器) | 最低 | 5-10x |

### 数据库 WAL 写入性能对比

```c
// 性能对比测试框架
typedef struct {
    double latency_us;
    double throughput_mbps;
    double cpu_usage_pct;
} IoBenchResult;

void bench_wal_write(const char *path) {
    IoBenchResult sync, iouring;

    // 同步写
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < N_WRITES; i++) {
        write(wal_fd, buf, 512);
        fsync(wal_fd);  // 每次刷盘
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    sync.latency_us = (end - start) / N_WRITES;

    // io_uring 批量写
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < N_WRITES; i += BATCH_SIZE) {
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_writev(sqe, wal_fd, iovecs, BATCH_SIZE, offset);
        io_uring_submit(&ring);
        io_uring_wait_cqe(&ring, &cqe);  // 批量等待
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    iouring.latency_us = (end - start) / N_WRITES;

    printf("同步: %.0f us/write,  io_uring: %.0f us/write (%.1fx 提升)\n",
           sync.latency_us, iouring.latency_us,
           sync.latency_us / iouring.latency_us);
}
```

### io_uring 特性与数据库场景

| 特性 | 数据库使用场景 |
|------|--------------|
| Fixed Files/Buffers | Buffer Pool 预注册页面缓冲区 |
| Polled Mode | 低延迟 WAL 写入（绕过调度器）|
| Linked SQE | 原子写入（数据 + fsync）|
| Multi-Shot Accept | 高并发连接接受 |
| Ring Mapped Buffer | 零拷贝网络 I/O |

### 最佳实践

1. **WAL 批量提交**：每批 16-64 个写入，一次 submit
2. **Buffer Pool 预读**：异步提交未来页读取请求
3. **固定缓冲区**：预注册避免每次注册/注销开销
4. **轮询模式**：对于 SSD/NVMe，使用 IORING_SETUP_IOPOLL 可达更低延迟
5. **内核版本**：Linux 5.15+ 推荐，稳定性和性能最佳

### 性能影响

- 批量提交减少 50-80% 的系统调用
- 共享内存队列消除数据拷贝
- 轮询模式（IOPOLL）延迟低至 ~10us
- 内存占用：每 SQE ~64 字节，每 CQE ~16 字节

### 扩展思考

- PostgreSQL 15+ 实验性 io_uring 支持（`enable_io_uring`）
- RocksDB 使用 io_uring 加速 Compaction
- SPDK/DPDK 绕过内核，直接控制 NVMe
- 未来：io_uring 完全替代 libaio/AIO
