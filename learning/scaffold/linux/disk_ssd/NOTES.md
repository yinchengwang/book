# Linux SSD 优化学习笔记

本文档记录 SSD 优化策略在项目工程代码中的实际应用对照。

## 工程对照：SSD 数据库优化 → 数据库针对 SSD 的 IO 策略调整

### 1. SSD 与 HDD 的成本模型差异

数据库查询优化器在选择执行计划时，依赖成本参数估算 I/O 开销。SSD 和 HDD 的物理特性差异导致成本模型完全不同：

```
                  HDD               SSD (NVMe)
顺序读延迟:      ~2-5ms            ~0.05-0.1ms
随机读延迟:      ~5-10ms (寻道)    ~0.05-0.1ms (无寻道)
随机/顺序比:     2-5x              1-1.2x (几乎无差异)
IOPS:            100-200            500K-1M
```

### 2. 数据库 IO 核心参数调整

工程代码中需要根据 SSD/HDD 调整关键参数：

```c
// engineering/include/db/guc.h
// GUC 配置参数 — 数据库 IO 行为控制

// SSD 推荐配置
typedef struct guc_io_params_t {
    double random_page_cost;          // SSD: 1.1,  HDD: 4.0
    double seq_page_cost;             // 通常保持 1.0
    int    effective_io_concurrency;  // SSD: 200,  HDD: 2
    int    max_worker_processes;      // 并行工作进程数
    int    max_parallel_workers;      // 并行查询工作线程
    int    max_parallel_workers_per_gather;  // 每次 Gather 的工作线程
} guc_io_params_t;

// random_page_cost = 1.1 的含义:
//   SSD 上随机读成本几乎等于顺序读成本
//   优化器会更倾向选择索引扫描（而非全表扫描）
//   因为索引扫描的随机 I/O 在 SSD 上代价很低
```

### 3. direct I/O 与 SSD

工程代码中使用 O_DIRECT 绕过页缓存，在 SSD 上尤为重要：

```c
// engineering/src/db/storage/page/disk.c
// 直接 I/O 配置 —— SSD 上效果更好

#ifdef USE_DIRECT_IO
    // SSD 上使用 O_DIRECT:
    //   1. 避免双缓存（内核页缓存 + Buffer Pool）
    //   2. SSD 随机读很快，不需要内核预读
    //   3. 减少内存占用
    int flags = O_RDWR | O_DIRECT;
#else
    int flags = O_RDWR;
#endif

int fd = open(path, flags, 0644);

// SSD 上的直接 I/O 对齐要求:
//   - 缓冲区必须 512 字节对齐（最好 4KB 对齐到 NAND 页大小）
//   - 读写长度必须扇区对齐
//   - 文件偏移必须扇区对齐
```

### 4. 预读策略适配

SSD 的预读策略与 HDD 截然不同：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// 预读策略 —— 根据存储介质调整

// HDD: 大量预读（利用顺序带宽，掩盖寻道延迟）
#define HDD_READAHEAD_PAGES 64

// SSD: 少量或不预读（随机访问成本低，预读浪费带宽）
#define SSD_READAHEAD_PAGES 4

static int get_readahead_pages(buffer_pool_t *pool) {
    // 根据存储类型动态调整预读页数
    return pool->is_ssd ? SSD_READAHEAD_PAGES : HDD_READAHEAD_PAGES;
}
```

### 5. TRIM / DISCARD 在数据库中的应用

数据库可以主动释放不再使用的磁盘空间：

```c
// engineering/src/db/storage/page/disk.c
// 空间回收 —— 对应 SSD TRIM

int disk_punch_hole(disk_t *disk, uint32_t page_id) {
    off_t offset = (off_t)page_id * PAGE_SIZE;
    
    // fallocate PUNCH_HOLE: 通知文件系统和 SSD 释放空间
    // 对应 Linux 的 FITRIM ioctl
    if (fallocate(disk->fd, 
                  FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                  offset, PAGE_SIZE) < 0) {
        return -1;
    }
    
    // 用途:
    //   - VACUUM 后释放死元组占用的页面
    //   - DROP TABLE 后释放表空间
    //   - 索引重建后释放旧索引页面
    return 0;
}
```

### 6. 写入放大与 WAL 设计

SSD 的写入放大问题影响 WAL 日志设计：

```c
// engineering/src/db/storage/wal/wal.c
// WAL 顺序写 —— 对 SSD 友好

// SSD 友好设计原则:
//   1. 顺序写优于随机写（减少 GC 压力）
//   2. 大块写优于小块写（对齐 NAND 页大小）
//   3. 批量提交优于逐条提交（减少写放大）
//   4. 避免原地更新（避免读-改-写循环）

int wal_flush(wal_t *wal) {
    // 批量写入: 减少小块写 → 降低 WAF
    ssize_t written = write(wal->fd, wal->buf, wal->offset);
    if (written < 0) return -1;
    if (fsync(wal->fd) < 0) return -1;
    wal->offset = 0;
    return 0;
}
```

### 7. 数据库 SSD 优化清单

| 参数 | HDD 推荐值 | SSD 推荐值 | 原因 |
|------|-----------|-----------|------|
| random_page_cost | 4.0 | 1.1 | SSD 随机读几乎等于顺序读 |
| effective_io_concurrency | 2 | 200 | SSD 支持高并发 IO |
| seq_page_cost | 1.0 | 1.0 | 顺序读成本基准不变 |
| shared_buffers | 25% 内存 | 25% 内存 | Buffer Pool 大有利于缓存命中 |
| wal_sync_method | fdatasync | fdatasync | SSD 上 fsync 开销更低 |
| checkpoint_timeout | 5min | 10-15min | SSD 上检查点 I/O 影响更小 |
| max_wal_size | 1GB | 4-8GB | SSD 有更大写入带宽 |
| O_DIRECT | 可选 | 推荐 | 避免双缓存 |
| IO 调度器 | mq-deadline | none/kyber | SSD 无需 LBA 排序 |

## 参考源码

- `engineering/include/db/guc.h` — GUC 配置参数定义
- `engineering/src/db/storage/page/disk.c` — 页面 I/O 与空间回收
- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 与预读策略
- `engineering/src/db/storage/wal/wal.c` — WAL 顺序写设计
