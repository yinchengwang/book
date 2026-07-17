# Linux 内存诊断学习笔记

本文档记录内存管理在项目工程代码中的实际应用对照。

## 工程对照

### 1. 内存分配对照

在项目 `engineering/src/db/storage/buffer/bufmgr.c` 中，Buffer Pool 使用 `calloc` 分配页面控制块数组：

```c
// bufmgr.c 第 156 行
buf_block_t *blocks = (buf_block_t *)calloc(pool->max_size, sizeof(buf_block_t));
if (!blocks) {
    return NULL;  // 分配失败处理
}
```

**对比学习：**

| 特性 | 学习卡片 | 工程实现 |
|------|----------|----------|
| 分配方式 | malloc/calloc | calloc |
| 元素类型 | void*/char* | buf_block_t 结构体 |
| 错误处理 | perror + return | NULL 检查 |
| 零初始化 | calloc 自动清零 | calloc 自动清零 |

**calloc vs malloc + memset：**

```c
// calloc（推荐）
buf_block_t *blocks = calloc(n, sizeof(buf_block_t));

// 等价于（不推荐）
buf_block_t *blocks = malloc(n * sizeof(buf_block_t));
memset(blocks, 0, n * sizeof(buf_block_t));
```

### 2. 页面控制块结构

工程中 Buffer Pool 的页面控制块结构：

```c
// bufmgr.h 第 23-35 行
typedef struct buf_block {
    page_id_t page_id;          // 页面 ID
    uint16_t ref_count;         // 引用计数
    uint8_t is_dirty;           // 脏页标志
    uint8_t is_pinned;          // 钉住标志
    char data[PAGE_SIZE];       // 页面数据
} buf_block_t;
```

**内存布局分析：**

- 每个页面控制块大小 = sizeof(buf_block_t)
- 对于 8KB 页面：8KB = 8192 字节控制块 + 8192 字节数据
- `calloc` 确保 data 区域初始为零，避免未定义行为

### 3. 内存泄漏防护模式

工程代码采用严格的资源管理模式：

```c
// bufmgr.c 第 98-115 行
int buffer_pool_init(buffer_pool_t *pool, size_t max_size) {
    // 1. 分配页面数组
    pool->blocks = calloc(max_size, sizeof(buf_block_t));
    if (!pool->blocks) {
        return -1;  // 立即返回错误
    }

    // 2. 初始化哈希表
    if (buf_hash_init(&pool->hash, max_size * 2) != 0) {
        free(pool->blocks);  // 清理已分配资源
        return -1;
    }

    // 3. 初始化 Clock-Sweep 指针
    pool->clock_hand = 0;
    pool->max_size = max_size;
    return 0;
}
```

**关键工程经验：**

1. **失败时清理**：每次分配失败必须回滚之前的分配
2. **零初始化**：使用 `calloc` 避免结构体字段未初始化问题
3. **边界检查**：分配后立即检查返回值

### 4. 内存对齐

工程代码注意内存对齐：

```c
// buf_block_t 结构体设计
typedef struct __attribute__((aligned(64))) buf_block {
    // 对齐到 Cache Line (64 字节)
    // 避免 False Sharing
} buf_block_t;
```

**__attribute__((aligned(N))) 的作用：**

- 确保结构体起始地址是 N 的倍数
- 防止跨 Cache Line 访问导致性能下降
- 在多线程场景下尤为重要

### 5. 虚拟内存与物理内存

工程代码通过 mmap 实现大页面支持：

```c
// bufmgr.c 第 145 行
#ifdef USE_HUGEPAGE
    pool->blocks = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
#endif
```

**学习要点：**

| 技术 | 用途 | 优势 |
|------|------|------|
| 匿名映射 | 分配内存 | 无需文件支持 |
| HUGETLB | 大页面 | TLB Miss 减少 |
| MAP_LOCKED | 锁入内存 | 防止换出 |

## 参考源码

- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 实现
- `engineering/src/db/storage/buffer/bufmgr.h` — 页面控制块定义
- `engineering/include/db/types.h` — page_id_t 等类型定义
