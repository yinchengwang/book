# NOTES.md — mmap_db 工程对照

## MMAP 存储引擎设计

### 背景

mmap 将文件直接映射到进程虚拟地址空间，读写映射区域就像操作普通内存。SQLite、RocksDB 等数据库使用 mmap 优化存储性能。

### 工程对照：MMAP 存储引擎

在我们的 `engineering/src/db/storage/` 模块中，可以设计一个简化版 mmap 存储引擎：

```c
// mmap_storage.c — 基于 mmap 的存储引擎
#include <sys/mman.h>
#include <fcntl.h>

#define PAGE_SIZE 4096
#define MAX_PAGES 1024

typedef struct {
    int fd;                  // 文件描述符
    size_t total_size;       // 总大小
    void *base_addr;         // 映射基址

    // 页面元数据
    struct {
        int is_dirty;        // 是否脏页
        int pin_count;       // 固定计数
        uint16_t lsn;        // 日志序列号
    } page_meta[MAX_PAGES];
} MmapBufferPool;

/* 初始化 mmap 存储引擎 */
MmapBufferPool* mmap_open(const char *path, size_t size) {
    MmapBufferPool *pool = calloc(1, sizeof(*pool));

    // 打开文件
    pool->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (pool->fd < 0) {
        free(pool);
        return NULL;
    }

    // 扩展文件
    if (ftruncate(pool->fd, size) < 0) {
        close(pool->fd);
        free(pool);
        return NULL;
    }

    // mmap 映射
    pool->base_addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, pool->fd, 0);
    if (pool->base_addr == MAP_FAILED) {
        close(pool->fd);
        free(pool);
        return NULL;
    }

    pool->total_size = size;
    return pool;
}

/* 获取页面（直接返回映射地址）*/
void* mmap_get_page(MmapBufferPool *pool, PageID page_id) {
    if (page_id >= pool->total_size / PAGE_SIZE) {
        return NULL;  // 越界
    }

    pool->page_meta[page_id].pin_count++;
    return (char *)pool->base_addr + page_id * PAGE_SIZE;
}

/* 释放页面固定 */
void mmap_unpin_page(MmapBufferPool *pool, PageID page_id) {
    if (page_id < MAX_PAGES && pool->page_meta[page_id].pin_count > 0) {
        pool->page_meta[page_id].pin_count--;
    }
}

/* 标记页面为脏 */
void mmap_mark_dirty(MmapBufferPool *pool, PageID page_id) {
    if (page_id < MAX_PAGES) {
        pool->page_meta[page_id].is_dirty = 1;
    }
}

/* 刷脏页到磁盘 */
int mmap_flush_dirty_pages(MmapBufferPool *pool) {
    int flushed = 0;

    for (int i = 0; i < MAX_PAGES; i++) {
        if (pool->page_meta[i].is_dirty) {
            void *page_addr = (char *)pool->base_addr + i * PAGE_SIZE;

            // 刷单页（Linux 自动按页对齐）
            if (msync(page_addr, PAGE_SIZE, MS_SYNC) == 0) {
                pool->page_meta[i].is_dirty = 0;
                flushed++;
            }
        }
    }

    return flushed;
}
```

### mmap vs Buffer Pool

| 特性 | MMAP | Buffer Pool |
|------|------|-------------|
| 页面管理 | 操作系统 | 数据库自管理 |
| 缓存淘汰 | 页面换出由 OS 决定 | LRU/Clock 显式控制 |
| 性能一致性 | 取决于工作集大小 | 稳定可预测 |
| 崩溃恢复 | 需要 WAL | 内置机制 |
| 控制力 | 低 | 高 |

### mmap 存储引擎的挑战

```c
// 挑战 1: 页面错误延迟不可预测
void unpredictable_page_fault(void) {
    void *page = mmap_get_page(pool, cold_page_id);
    // 首次访问触发 page fault → 磁盘 I/O → 高延迟
}

// 挑战 2: 预读策略
void prefetch_pages(MmapBufferPool *pool, PageID start, int count) {
    for (int i = 0; i < count; i++) {
        // 触摸页面触发预读
        volatile char c = *((char *)pool->base_addr + (start + i) * PAGE_SIZE);
        (void)c;
    }
}

// 挑战 3: 崩溃一致性
void crash_consistent_update(MmapBufferPool *pool, PageID page_id) {
    // 1. 先写 WAL
    wal_append(pool->wal_fd, page_id, data);

    // 2. 再更新 mmap 数据
    void *page = mmap_get_page(pool, page_id);
    memcpy(page, data, PAGE_SIZE);

    // 3. 标记脏页
    mmap_mark_dirty(pool, page_id);

    // 4. msync 刷盘（可选，由 WAL 保证持久性）
}
```

### MMAP 配置建议

| 参数 | 建议值 | 说明 |
|------|--------|------|
| mmap_size | < 50% 物理内存 | 避免 OOM |
| work_mem | 足够放下热点数据 | 减少页面错误 |
| effective_cache_size | 估算 OS 缓存 | 优化器参考 |

### 性能影响

- 命中工作集：延迟极低（~100ns）
- 缺页中断：首次访问约 10-100ms
- 顺序扫描：自动预读，接近内存带宽
- 随机访问：缺页开销显著

### 扩展思考

- PostgreSQL 的 `effective_io_concurrency` 参数
- Linux transparent huge pages 对 mmap 的影响
- io_uring + mmap 的零拷贝 I/O 组合
- 大数据时代的 direct I/O vs mmap
