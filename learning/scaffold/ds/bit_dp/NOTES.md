# 工程对照说明

## 与 engineering/src/db/ 的关联

### 状态机实现

在数据库并发控制中，事务状态机使用位掩码表示状态：

```c
// txn.c 中的事务状态
typedef enum {
    TXN_ACTIVE    = 0x01,
    TXN_COMMITTED = 0x02,
    TXN_ABORTED   = 0x04,
    TXN_IN_VOID   = 0x08  /* 等待 GC */
} TxnState;

/* 状态转换检查 */
bool txn_can_commit(TxnState state)
{
    return (state & TXN_ACTIVE) && !(state & (TXN_COMMITTED | TXN_ABORTED));
}

/* 批量状态更新 */
TxnState new_state = (old_state | TXN_COMMITTED) & ~TXN_ACTIVE;
```

这与本模块的状态压缩思想一致：用位运算高效管理组合状态。

### 位图索引 (Bitmap Index)

数据库使用位图索引加速等值查询：

```c
// bitmap_index.c 中的位图操作
typedef struct {
    int cardinality;     /* 列基数 */
    void *bitmaps[];     /* 每个唯一值的位图 */
} BitmapIndex;

/* 等值查询: sex = 'M' */
Bitmap *result = bitmap_alloc(table->row_count);
bitmap_copy(result, index->bitmaps[male_index]);

/* AND 查询: sex = 'M' AND age = 25 */
bitmap_and(result, index->bitmaps[age_25_index]);

/* COUNT 查询: 只需 popcount */
int count = bitmap_popcount(result);
```

位图索引的优势：
- 压缩率高（Run-Length Encoding）
- 位运算加速集合操作
- 适合低基数列（性别、状态码）

### Buffer Pool 页面状态追踪

数据库 Buffer Pool 使用位图追踪页面状态：

```c
// bufmgr.c 中的页面管理
typedef struct {
    int page_count;
    unsigned long *dirty_bits;  /* 脏页位图 */
    unsigned long * pinned_bits; /* 固定计数位图 */
} BufferPool;

/* 检查页面是否脏 */
bool page_is_dirty(BufferPool *pool, int page_id)
{
    int word = page_id / (sizeof(unsigned long) * 8);
    int bit = page_id % (sizeof(unsigned long) * 8);
    return (pool->dirty_bits[word] >> bit) & 1UL;
}

/* 批量刷脏页 */
for (int i = 0; i < pool->page_count / 64; i++) {
    if (pool->dirty_bits[i]) {
        flush_pages(pool, i * 64, pool->dirty_bits[i]);
        pool->dirty_bits[i] = 0;
    }
}
```

### MVCC 可见性判断

多版本并发控制使用位掩码快速判断记录可见性：

```c
// mvcc.c 中的可见性判断
typedef struct {
    uint64_t xmin;   /* 插入事务 ID */
    uint64_t xmax;   /* 删除事务 ID */
    uint8_t t_infomask;  /* 标志位 */
} HeapTupleFields;

/* 快照结构 */
typedef struct {
    uint64_t xmin;
    uint64_t xmax;
    uint64_t xcnt;
    uint64_t *xip_list;  /* 正在运行的事务列表 */
} Snapshot;

bool tuple_visible(HeapTuple t, Snapshot snap)
{
    /* 规则：
     * 1. xmin 未提交 → 不可见
     * 2. xmax 已提交且在快照后 → 不可见（被删除）
     * 3. 其他 → 可见
     */
    if (t->xmin == snap->xmin) return false;  /* 自己未提交的插入不可见 */
    if (t->xmax != 0 && t->xmax < snap->xmax) return false;
    return true;
}
```

### 布隆过滤器 (Bloom Filter)

布隆过滤器使用多个哈希函数映射到位数组：

```c
// bloom_filter.c
typedef struct {
    size_t size;           /* 位数组大小（位数） */
    unsigned char *bits;   /* 位数组 */
    int hash_count;        /* 哈希函数数量 */
} BloomFilter;

bool bloom_check(BloomFilter *bf, const char *key)
{
    for (int i = 0; i < bf->hash_count; i++) {
        size_t idx = hash_i(key, i) % bf->size;
        if (!bitmap_get(bf->bits, idx)) return false;
    }
    return true;  /* 可能存在 */
}

void bloom_add(BloomFilter *bf, const char *key)
{
    for (int i = 0; i < bf->hash_count; i++) {
        size_t idx = hash_i(key, i) % bf->size;
        bitmap_set(bf->bits, idx);
    }
}
```

数据库使用布隆过滤器快速判断键是否可能存在于索引中，避免不必要的磁盘访问。

### 性能优化：SIMD 加速 popcount

现代 CPU 提供硬件级 popcount 指令：

```c
// simd_popcount.c
#include <immintrin.h>

size_t fast_popcount(const unsigned char *data, size_t len)
{
    size_t count = 0;
    for (size_t i = 0; i + 32 <= len; i += 32) {
        __m256i v = _mm256_loadu_si256((__m256i *)(data + i));
        count += __builtin_popcountll(_mm256_extract_epi64(v, 0));
        count += __builtin_popcountll(_mm256_extract_epi64(v, 1));
        count += __builtin_popcountll(_mm256_extract_epi64(v, 2));
        count += __builtin_popcountll(_mm256_extract_epi64(v, 3));
    }
    // 处理剩余字节...
    return count;
}
```
