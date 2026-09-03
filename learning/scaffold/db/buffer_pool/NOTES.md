# Buffer Pool 核心逻辑笔记

本文档对照 `engineering/src/db/storage/buffer/bufmgr.c`，描述 Buffer Pool 的页面缓存机制。

## 1. 页面缓存概述

Buffer Pool 是数据库存储引擎的核心组件，负责在内存中缓存磁盘页面，减少磁盘 IO。

核心思想：
- 磁盘页面加载到内存 Buffer
- 通过 Hash 表快速定位页面
- 内存不足时，Clock-Sweep 算法置换页面

## 2. Hash 表查找

### 简化版实现

```c
// 简化：直接用 page_id 取模定位
uint32_t hash_page(uint32_t page_id) {
    return page_id % pool.hash_size;
}

BufferDesc *hash_lookup(uint32_t page_id) {
    return pool.hash_table[hash_page(page_id)];
}
```

### bufmgr.c 实际实现

```c
// buf_hash: 组合 hash 函数
static uint32_t buf_hash(uint32_t relfilenode, BlockNumber blocknum) {
    uint64_t key = ((uint64_t)relfilenode << 32) | blocknum;
    return ((key * 2654435761UL) >> 16) % buffer_pool->hash_size;
}

// hash_lookup: 在桶链表中查找
static BufHashEntry *hash_lookup(uint32_t relfilenode, BlockNumber blocknum) {
    uint32_t bucket = buf_hash(relfilenode, blocknum);
    BufHashEntry *entry = buffer_pool->hash_table[bucket];
    while (entry) {
        if (entry->relfilenode == relfilenode && entry->blocknum == blocknum) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}
```

关键点：
- 使用 `(relfilenode, blocknum)` 复合键
- 乘法 hash 减少冲突
- Hash 表用链表解决冲突

## 3. Clock-Sweep 置换算法

### 算法原理

Clock-Sweep（时钟扫描）是近似 LRU 的置换算法，避免真正的 LRU 带来的修改开销。

```
      ┌─────────────────────────────────────────┐
      │            Clock Hand                   │
      │                 ↓                       │
      │   [B0] [B1] [B2] [B3] [B4] [B5] [B6]   │
      │                 ↑                       │
      │            next_victim                  │
      └─────────────────────────────────────────┘
```

### 置换流程

```
1. 从 next_victim 位置开始扫描
2. 如果 refcount > 0（被 pin），跳过，指针+1
3. 如果 usage_count > 0，减少 usage_count，指针+1
4. 否则，该 Buffer 可被置换：
   a. 如果是脏页，先刷盘
   b. 从 Hash 表移除旧条目
   c. 返回该 Buffer
```

### bufmgr.c 实际实现

```c
static uint32_t clock_sweep(void) {
    int n = buffer_pool->nbuffers;

    for (int tries = 0; tries < n * 2; tries++) {
        BufferDesc *buf = &buffer_pool->descriptors[buffer_pool->next_victim];

        // 跳过正在使用的 Buffer
        if (buf->refcount > 0) {
            buffer_pool->next_victim = (buffer_pool->next_victim + 1) % n;
            continue;
        }

        // 如果 usage_count > 0，减少并继续
        if (buf->usage_count > 0) {
            buf->usage_count--;
            buffer_pool->next_victim = (buffer_pool->next_victim + 1) % n;
            continue;
        }

        // 找到可置换的 Buffer
        uint32_t buf_id = buffer_pool->next_victim;
        buffer_pool->next_victim = (buffer_pool->next_victim + 1) % n;

        // 如果是脏页，先写回
        if (buf->state & BM_VALID) {
            if (buf->state & BM_DIRTY) {
                buf_write(buf);
            }
            hash_delete(buf->relfilenode, buf->blocknum);
        }

        return buf_id;
    }

    return 0;  // 兜底
}
```

### 近似 LRU 的原理

- `usage_count` 初始为 0
- 页面被访问时 `usage_count++`
- 扫描时递减 `usage_count`
- 多次访问的页面 `usage_count` 较高，不易被置换
- 这近似"最近最少使用"的语义，但无需记录精确时间戳

## 4. 页面读取流程

```c
BufferDesc *buf_read(uint32_t relfilenode, BlockNumber blocknum, int access_mode) {
    // 1. 先在 Hash 表中查找
    BufHashEntry *entry = hash_lookup(relfilenode, blocknum);
    if (entry) {
        // 命中
        buffer_pool->hits++;
        BufferDesc *buf = &buffer_pool->descriptors[entry->buf_id];
        buf->usage_count++;
        if (access_mode > 0) {
            buf_dirty(buf);  // 写访问标记脏
        }
        return buf;
    }

    // 2. 未命中，分配新 Buffer
    buffer_pool->misses++;
    uint32_t buf_id = clock_sweep();
    BufferDesc *buf = &buffer_pool->descriptors[buf_id];

    // 3. 清除旧内容
    if (buf->state & BM_VALID) {
        hash_delete(buf->relfilenode, buf->blocknum);
    }

    // 4. 设置新内容
    buf->relfilenode = relfilenode;
    buf->blocknum = blocknum;
    buf->usage_count = 1;
    buf->state = BM_VALID;
    buf->refcount = 1;
    if (access_mode > 0) {
        buf->state |= BM_DIRTY;
    }

    // 5. 从磁盘读取并插入 Hash 表
    hash_insert(relfilenode, blocknum, buf_id);
    buffer_pool->reads++;

    return buf;
}
```

## 5. 脏页管理

### 脏页标记

```c
void buf_dirty(BufferDesc *buf) {
    if (!(buf->state & BM_DIRTY)) {
        buf->state |= BM_DIRTY;
        printf("[buffer] 标记脏页: Buffer %d\n", buf->buf_id);
    }
}
```

### 脏页刷盘

```c
void buf_flush_all(void) {
    for (int i = 0; i < pool.nbuffers; i++) {
        if (pool.descriptors[i].state & BM_DIRTY) {
            // 写回磁盘
            pool.descriptors[i].state &= ~BM_DIRTY;
        }
    }
}
```

## 6. 总结

| 组件 | 作用 |
|------|------|
| Hash 表 | O(1) 查找页面是否在缓存中 |
| BufferDesc | 描述每个 Buffer 的状态和元数据 |
| Clock-Sweep | 近似 LRU，高效的页面置换 |
| refcount | 防止正在使用的页面被置换 |
| usage_count | 反映页面访问频率，影响置换优先级 |
| BM_DIRTY 标志 | 标识已修改页面，置换前需刷盘 |

Buffer Pool 通过这组机制，实现了内存缓存的高效管理，是数据库存储引擎的性能关键。
