# 工程对照笔记

## 概述

本学习卡片演示的数组概念直接对应工程代码中的实际应用。以下对照工程源码中的数组使用模式，帮助理解理论与实践的关联。

---

## 对照一：Buffer Pool 的 page array

**源文件**：`engineering/src/db/storage/bufmgr.c`

Buffer Pool 使用固定大小的 page 数组来管理内存页面：

```c
// bufmgr.c 中的页面数组
typedef struct {
    PageDescriptor pages[MAX_BUFFERS];  // 静态 page array
    int num_pages;
} BufferPool;
```

**对照学习点**：
- 这里的 `pages[]` 是静态数组，大小在编译时由 `MAX_BUFFERS` 宏确定
- 页面查找使用 hash table + 线性探测，而不是遍历数组（优化 O(n) 到 O(1)）
- 与本卡片静态数组的区别：Buffer Pool 的 page 数组元素是结构体，而非基本类型

---

## 对照二：Vector Segment 的动态 page 数组

**源文件**：`engineering/src/db/storage/vector/vector_segment.c`

向量段使用动态扩展的 page 数组存储向量数据：

```c
// vector_segment.c 中的动态 page 数组
typedef struct {
    vector_page_t **pages;  // 动态分配的 page 指针数组
    size_t num_pages;
    size_t page_capacity;   // 对应 capacity
} VectorSegment;

// 扩容模式（简化）
if (seg->num_pages >= seg->page_capacity) {
    size_t new_cap = seg->page_capacity * 2;  // 2倍扩容
    seg->pages = realloc(seg->pages, new_cap * sizeof(vector_page_t*));
    seg->page_capacity = new_cap;
}
```

**对照学习点**：
- 使用二级指针 `vector_page_t **pages` 实现动态数组
- 扩容策略与本卡片完全一致：2 倍扩展
- `realloc` 失败时需要回滚（工程代码更严谨）

---

## 对照三：WAL Buffer 的 dirty_pages 数组

**源文件**：`engineering/src/db/storage/wal/wal_buf.c`

WAL Buffer 使用固定数组记录脏页面：

```c
// wal_buf.c 中的脏页记录
typedef struct {
    uint32_t dirty_pages[MAX_DIRTY_PAGES];  // 静态数组
    size_t dirty_count;
} WALBuffer;
```

**对照学习点**：
- 这是静态数组的实际应用，大小由 `MAX_DIRTY_PAGES` 限定
- 用途是记录哪些页面被修改，而非存储页面本身
- `dirty_pages[i]` 的访问模式与本卡片的 `arr[i]` 完全一致

---

## 对照四：graph_dedup 的 capacity 管理

**源文件**：`engineering/src/db/storage/vector/graph_dedup.c`

图去重器展示了完整的 capacity 管理模式：

```c
// 动态数组的典型模式
graph_dedup_t *graph_dedup_create(int32_t capacity, int dims) {
    graph_dedup_t *dedup = malloc(sizeof(graph_dedup_t));
    dedup->fingerprints = malloc(capacity * sizeof(dedup_fingerprint_t));
    dedup->capacity = capacity;
    dedup->size = 0;
    return dedup;
}
```

**对照学习点**：
- 结构体包含 `capacity` 和实际使用的计数
- `malloc` 分配原始内存，`realloc` 扩容
- 与本卡片 `DynArray` 结构的对应关系

---

## 工程代码模式总结

| 组件 | 数组类型 | 容量管理 | 用途 |
|------|---------|---------|------|
| Buffer Pool | 静态 `[]` | 编译时固定 | 管理内存页描述符 |
| Vector Segment | 动态 `**` | 2x 扩容 | 存储向量数据页 |
| WAL Buffer | 静态 `[]` | 编译时固定 | 记录脏页 ID |
| Graph Dedup | 动态 `*` | 2x 扩容 | 存储指纹和向量 |

---

## 延伸阅读

- `engineering/src/db/storage/bufmgr.c` - Buffer Pool 实现
- `engineering/src/db/storage/vector/vector_segment.c` - 向量存储实现
- `engineering/src/db/storage/wal/wal_buf.c` - WAL 缓冲实现
