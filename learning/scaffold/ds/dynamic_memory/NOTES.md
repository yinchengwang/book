# 工程对照笔记

## 概述

动态内存管理是存储引擎的核心能力。本卡片演示的概念直接对应工程中的 Buffer Pool 内存管理、动态数组扩展、页面缓存等实际场景。以下对照工程源码中的内存管理模式。

---

## 对照一：Buffer Pool 的页面内存分配

**源文件**：`engineering/src/db/storage/bufmgr.c`

Buffer Pool 使用动态分配的页面数组管理内存：

```c
/* Buffer Pool 结构 */
typedef struct {
    PageDescriptor *pages;      /* 页面描述符数组 */
    char *page_data;           /* 实际页面数据缓冲区 */
    int num_pages;            /* 页面数量 */
    int pool_size;             /* 缓冲池大小 */
    hash_table_t *hash;        /* 页面 Hash 表 */
} BufferPool;

/* 缓冲池初始化 */
int buf_init(BufferPool *pool, int num_pages, int page_size) {
    /* 分配页面描述符数组 */
    pool->pages = (PageDescriptor *)calloc(num_pages, sizeof(PageDescriptor));
    if (!pool->pages) {
        return -1;  /* 分配失败 */
    }

    /* 分配页面数据缓冲区 */
    pool->page_data = (char *)malloc((size_t)num_pages * page_size);
    if (!pool->page_data) {
        free(pool->pages);
        return -1;  /* 回滚分配 */
    }

    pool->num_pages = num_pages;
    pool->pool_size = num_pages * page_size;
    return 0;
}

/* 缓冲池销毁 - 配对释放 */
void buf_shutdown(BufferPool *pool) {
    if (pool->page_data) {
        free(pool->page_data);
        pool->page_data = NULL;
    }
    if (pool->pages) {
        free(pool->pages);
        pool->pages = NULL;
    }
}
```

**对照学习点**：
- 与本卡片相同的分配/释放配对模式
- 分配失败时的回滚处理
- 使用 calloc 初始化描述符数组（元素置零）
- shutdown 函数释放所有资源

---

## 对照二：Vector Segment 的动态 page 数组

**源文件**：`engineering/src/db/storage/vector/vector_segment.c`

向量段使用动态扩展的 page 数组存储向量：

```c
/* Vector Segment 结构 */
typedef struct {
    vector_page_t **pages;    /* 动态 page 指针数组 */
    size_t num_pages;         /* 当前页数 */
    size_t page_capacity;      /* 数组容量 */
    size_t page_size;         /* 每页向量数 */
} VectorSegment;

/* 创建向量段 */
VectorSegment *vector_segment_create(size_t initial_capacity) {
    VectorSegment *seg = (VectorSegment *)malloc(sizeof(VectorSegment));
    if (!seg) return NULL;

    /* 分配 page 指针数组 */
    seg->pages = (vector_page_t **)calloc(initial_capacity,
                                          sizeof(vector_page_t *));
    if (!seg->pages) {
        free(seg);
        return NULL;
    }

    seg->num_pages = 0;
    seg->page_capacity = initial_capacity;
    seg->page_size = VECTOR_PAGE_SIZE;
    return seg;
}

/* 动态扩展 page 数组 */
int vector_segment_grow(VectorSegment *seg) {
    size_t new_cap = seg->page_capacity * 2;

    vector_page_t **new_pages = (vector_page_t **)realloc(
        seg->pages, new_cap * sizeof(vector_page_t *));

    if (!new_pages) {
        return -1;  /* 扩容失败，原数组仍有效 */
    }

    seg->pages = new_pages;
    seg->page_capacity = new_cap;
    return 0;
}
```

**对照学习点**：
- 与本卡片 realloc 演示完全相同的 2x 扩容模式
- realloc 失败时原内存仍然有效
- 使用 calloc 初始化指针数组（NULL 填充）
- 这就是数组动态扩展的工程实现

---

## 对照三：WAL 的环形缓冲区

**源文件**：`engineering/src/db/storage/wal/wal_buf.c`

WAL Buffer 使用动态分配的环形缓冲区：

```c
/* WAL Buffer 结构 */
typedef struct {
    char *buffer;              /* 环形缓冲区 */
    size_t buffer_size;        /* 缓冲区大小 */
    size_t write_pos;          /* 写位置 */
    size_t read_pos;           /* 读位置 */
    int is_dirty;              /* 是否需要刷盘 */
} WALBuffer;

/* 创建 WAL Buffer */
WALBuffer *wal_buf_create(size_t size) {
    WALBuffer *buf = (WALBuffer *)malloc(sizeof(WALBuffer));
    if (!buf) return NULL;

    buf->buffer = (char *)malloc(size);
    if (!buf->buffer) {
        free(buf);
        return NULL;
    }

    buf->buffer_size = size;
    buf->write_pos = 0;
    buf->read_pos = 0;
    buf->is_dirty = 0;

    return buf;
}

/* 销毁 WAL Buffer */
void wal_buf_destroy(WALBuffer *buf) {
    if (buf) {
        free(buf->buffer);   /* 先释放内部缓冲区 */
        free(buf);           /* 再释放结构体本身 */
    }
}
```

**对照学习点**：
- 嵌套分配：`malloc` 结构体 + `malloc` 缓冲区
- 销毁时按逆序释放
- 分配失败回滚示例

---

## 对照四：内存池分配器（Object Pool）

**源文件**：`engineering/src/db/utils/mempool.c`

内存池避免频繁 malloc/free 的开销：

```c
/* 内存池结构 */
typedef struct MemPool {
    void *free_list;           /* 空闲对象链表 */
    size_t object_size;        /* 每个对象大小 */
    size_t chunk_size;         /* 每次分配的块大小 */
    int num_objects;           /* 每块的 объектов数 */
} MemPool;

/* 从池中分配对象 */
void *mempool_alloc(MemPool *pool) {
    if (pool->free_list == NULL) {
        /* 池空，分配新块 */
        void *chunk = malloc(pool->chunk_size);
        if (!chunk) return NULL;

        /* 将新块切分成对象，加入空闲链表 */
        char *ptr = (char *)chunk;
        for (int i = 0; i < pool->num_objects - 1; i++) {
            void *next = (char *)ptr + pool->object_size;
            *(void **)ptr = next;
            ptr += pool->object_size;
        }
        *(void **)ptr = NULL;  /* 最后一个 */

        pool->free_list = chunk;
    }

    /* 从空闲链表取出一个对象 */
    void *obj = pool->free_list;
    pool->free_list = *(void **)obj;
    return obj;
}

/* 归还对象到池 */
void mempool_free(MemPool *pool, void *obj) {
    *(void **)obj = pool->free_list;
    pool->free_list = obj;
}
```

**对照学习点**：
- 内存池避免频繁 malloc/free 的系统调用开销
- 使用空闲链表管理已释放的对象
- 分配 O(1)，无需操作系统参与
- 数据库连接池、线程池使用相同模式

---

## 工程代码模式总结

| 组件 | 内存模式 | 关键点 |
|------|---------|--------|
| Buffer Pool | 动态页面数组 | 分配失败回滚 |
| Vector Segment | 2x 扩容数组 | realloc 失败处理 |
| WAL Buffer | 嵌套分配 | 逆序释放 |
| Memory Pool | 对象池 | 空闲链表复用 |

---

## 延伸阅读

- `engineering/src/db/storage/bufmgr.c` - Buffer Pool 实现
- `engineering/src/db/storage/vector/vector_segment.c` - 向量存储实现
- `engineering/src/db/storage/wal/wal_buf.c` - WAL 缓冲实现
- `engineering/src/db/utils/mempool.c` - 内存池实现
