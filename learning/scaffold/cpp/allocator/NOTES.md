# 工程对照：PostgreSQL 风格 Buffer 分配

## 概述

本笔记对照学习材料中的分配器概念与 engineering/src/db/storage/buffer/bufmgr.c 中 Buffer Pool 的内存分配实现。

## Buffer Pool 核心设计

PostgreSQL 的 Buffer Pool 采用与内存池分配器相似的思想，但针对数据库页面这一特定对象进行了优化：

### 1. 固定大小的内存块

```c
// bufmgr.c 第 109-119 行
buffer_pool->buffers = (char **)calloc(nbuffers, sizeof(char *));
for (int i = 0; i < nbuffers; i++) {
    buffer_pool->buffers[i] = (char *)calloc(1, BUF_PAGE_SIZE);
    // ...
}
```

**与内存池的对照**：数据库页面大小固定（通常 8KB），因此可以预先分配固定数量的 Buffer 帧，类似于内存池预先切分固定大小的 slot。

### 2. Hash 表 + 空闲链表

```c
// bufmgr.c 第 26-31 行
typedef struct BufHashEntry_s {
    uint32_t    relfilenode;      // 物理文件节点
    BlockNumber blocknum;         // 块号
    uint32_t    buf_id;           // Buffer ID
    struct BufHashEntry_s *next;  // 链表下一项
} BufHashEntry;
```

**与内存池的对照**：Hash 表用于 O(1) 查找页面是否已在 Buffer Pool 中（命中），空闲链表用于选择被淘汰的 Buffer（未命中时）。

### 3. Clock-Sweep 置换算法

```c
// bufmgr.c 第 268-306 行
static uint32_t clock_sweep(void) {
    // 1. 跳过正在使用的 Buffer（refcount > 0）
    // 2. 减少 usage_count（近似 LRU）
    // 3. 找到可置换的 Buffer
    // 4. 如果脏，写回磁盘
    // 5. 从 Hash 表移除
    // 6. 返回 Buffer ID
}
```

**与 Arena 分配器的对照**：Clock-Sweep 是一种"批量淘汰"策略 —— 不单独释放单个 Buffer，而是批量扫描找到可复用的帧。这与 Arena 的 reset 理念相似，但更复杂，因为需要考虑脏页回写。

### 4. Pin/Unpin 机制

```c
// bufmgr.c 第 439-450 行
void buf_pin(BufferDesc *buf) {
    buf->refcount++;      // 使用计数 +1
    buf->usage_count++;   // 热度计数 +1
}

void buf_unpin(BufferDesc *buf) {
    buf->refcount--;      // 使用计数 -1
}
```

**关键概念**：
- **Pin**: 正在被使用，不能淘汰
- **Unpin**: 使用完毕，可参与置换
- **refcount**: 实际使用者数量（防止正在 IO 时被淘汰）
- **usage_count**: 历史使用频率（用于 Clock-Sweep 近似 LRU）

## 设计对比

| 维度 | 学习代码（内存池） | 工程代码（Buffer Pool） |
|------|-------------------|------------------------|
| 分配粒度 | 固定大小的 slot | 固定大小的 Buffer 帧 |
| 查找方式 | 无（简单池） | Hash 表 O(1) |
| 淘汰策略 | 空闲链表直接复用 | Clock-Sweep 近似 LRU |
| 释放方式 | 归还空闲链表 | 脏页写回 + Hash 移除 |
| 并发安全 | 无 | refcount + 锁 |

## 核心启示

1. **分配器是领域的抽象**：数据库页面分配器与通用内存池原理相同，但针对"页面"这一特定对象优化
2. **淘汰策略的重要性**：Buffer Pool 的核心问题不是分配，而是选择淘汰哪个页面（类似 Arena 选择何时 reset）
3. **脏页处理**：与普通内存池不同，Buffer 需要处理脏页写回，这是数据库持久化的关键
4. **Hash 表的双重作用**：既用于快速查找（缓存命中），也用于维护 Buffer 与页面的映射关系

## 代码位置

- 主实现：`engineering/src/db/storage/buffer/bufmgr.c`
- 头文件：`engineering/include/db/buf.h`
- 相关模块：`engineering/src/db/storage/buffer/buffer.c`
