# 工程对照笔记

## 概述

本学习卡片演示的位运算概念直接对应工程代码中的实际应用。以下对照工程源码中的位操作使用模式，帮助理解理论与实践的关联。

---

## 对照一：GUC 参数标志位

**源文件**：`engineering/include/db/guc.h`

GUC（Grand Unified Configuration）系统使用位标志管理参数属性：

```c
// guc.h 中的参数标志定义
#define GUC_FLAGS_NONE          0
#define GUC_FLAGS_NO_SHOW       (1U << 0)   /* 不在 SHOW 命令中显示 */
#define GUC_FLAGS_NO_RESET      (1U << 1)   /* 不能通过 ALTER SYSTEM 重置 */
#define GUC_FLAGS_RUNTIME_COMPUTED (1U << 2) /* 运行时计算的值 */
#define GUC_FLAGS_NEED_RELOAD   (1U << 3)   /* 需要重载配置文件 */
```

**对照学习点**：
- `set_bit` 模式：`flags |= GUC_FLAGS_NO_SHOW`
- `clear_bit` 模式：`flags &= ~GUC_FLAGS_NO_SHOW`
- `check_bit` 模式：`flags & GUC_FLAGS_NO_SHOW`

---

## 对照二：WAL 日志块标志

**源文件**：`engineering/include/db/wal.h`

WAL 日志块使用位标志表示不同的日志类型和状态：

```c
// wal.h 中的日志块标志
typedef struct {
    uint8_t flags;
    // ...
} WALBlock;

#define WAL_BLOCK_ORIGIN   (1U << 0)  /* 包含原始数据 */
#define WAL_BLOCK_HOT      (1U << 1)  /* 热页更新 */
#define WAL_BLOCK_FULL     (1U << 2)  /* 完整页面镜像 */
#define WAL_BLOCK_COMPRESS  (1U << 3) /* 页面被压缩 */
```

**对照学习点**：
- 使用位标志而非枚举，可以组合多个标志
- 检查多个条件：`if (block->flags & (WAL_BLOCK_ORIGIN | WAL_BLOCK_HOT))`
- 节省内存：一个 uint8_t 可表示8种不同状态

---

## 对照三：Buffer Pool 的 page 状态位图

**源文件**：`engineering/src/db/storage/bufmgr.c`

Buffer Pool 使用位图跟踪页面状态：

```c
// bufmgr.c 中的页面状态管理（简化）
typedef struct {
    uint32_t dirty_bits[NUM_BUF_PAGES / 32];  /* 脏页位图 */
    uint32_t pinned_bits[NUM_BUF_PAGES / 32]; /* pin计数位图 */
    // ...
} BufferPoolMetadata;

// 检查页面是否脏
static inline int buf_is_dirty(BufferPoolMetadata *meta, int page_id) {
    return (meta->dirty_bits[page_id / 32] >> (page_id % 32)) & 1U;
}

// 标记页面为脏
static inline void buf_set_dirty(BufferPoolMetadata *meta, int page_id) {
    meta->dirty_bits[page_id / 32] |= (1U << (page_id % 32));
}
```

**对照学习点**：
- 位图的典型工程应用：跟踪数千个页面的状态
- `page_id / 32` 定位字，`page_id % 32` 定位位
- 比 `bool dirty_pages[NUM_BUF_PAGES]` 节省 32 倍空间

---

## 对照四：LSM 树布隆过滤器

**源文件**：`engineering/include/db/sstable.h`

LSM 树 SSTable 使用布隆过滤器快速判断键是否存在：

```c
// sstable.h 中的布隆过滤器接口
typedef struct {
    uint8_t *bits;        /* 位数组 */
    size_t num_bits;      /* 位数 */
    uint32_t num_hashes;  /* 哈希函数数量 */
} BloomFilter;

// 添加键到布隆过滤器
int bloom_filter_add(BloomFilter *bf, const void *key, size_t key_len);

// 检查键是否可能在布隆过滤器中
int bloom_filter_might_contain(const BloomFilter *bf, const void *key, size_t key_len);
```

**对照学习点**：
- 读取 SSTable 前先用布隆过滤器过滤不存在的键
- 避免不必要的磁盘 I/O（布隆过滤器说"不存在"就一定不存在）
- 减少磁盘查询次数，提升 LSM 树读取性能

---

## 工程代码模式总结

| 组件 | 位操作模式 | 用途 |
|------|-----------|------|
| GUC 参数 | 标志位组合 | 管理参数属性（只读/可重置等） |
| WAL 日志 | 块类型标志 | 表示日志块类型和状态 |
| Buffer Pool | 位图跟踪 | 脏页、pin计数、引用计数 |
| SSTable | 布隆过滤器 | 快速判断键是否存在 |
| HNSW 图 | 层级位图 | 跟踪顶点所属层级 |

---

## 延伸阅读

- `engineering/include/db/guc.h` - GUC 参数系统（标志位模式）
- `engineering/include/db/wal.h` - WAL 日志块结构
- `engineering/src/db/storage/bufmgr.c` - Buffer Pool 实现（页面状态位图）
- `engineering/include/db/sstable.h` - LSM 树 SSTable（布隆过滤器）
