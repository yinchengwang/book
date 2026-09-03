# 工程对照笔记

## 概述
本文档对照 `engineering/src/db/index/hash/` 目录下的工程实现，分析线性哈希与可扩展哈希的生产级实现差异。

## 1. pg_hash：PostgreSQL 风格线性哈希

### 核心数据结构（`pg_hash_private.h`）

```c
struct pg_hash {
    uint32_t         n_total;        /* 总元组数 */
    uint32_t         max_bucket;     /* 最大已分配桶号 */
    uint32_t         high_mask;      /* 当前层掩码 = 2^k - 1 */
    uint32_t         low_mask;       /* 前一层掩码 = 2^(k-1) - 1 */
    uint32_t         fill_factor;    /* 填充因子（默认 75%） */
    pg_hash_page_t **buckets;        /* 桶数组 */
};
```

### 关键实现差异

| 演示代码 | 工程实现 (`pg_hash_core.c`) |
|----------|---------------------------|
| 分裂指针 `split_pointer` | 使用 `max_bucket` 记录最高桶号，隐式确定分裂位置 |
| 固定轮次 `level` | 通过 `high_mask / low_mask` 双掩码机制实现 |
| 简单链表桶 | `pg_hash_page_t` 页面结构 + 溢出链，支持持久化 |

### 分裂算法（`pg_hash_split.c`）

工程实现的核心逻辑：

```c
/* 新桶号 = max_bucket + 1 */
new_bucket = idx->max_bucket + 1;
/* 旧桶号 = 新桶号 & low_mask */
old_bucket = new_bucket & idx->low_mask;

/* 一轮结束时更新掩码 */
if (new_bucket == idx->high_mask) {
    idx->low_mask  = idx->high_mask;
    idx->high_mask = (idx->high_mask << 1) | 1u;
}
```

**关键洞察**：工程实现不需要显式维护 `split_pointer`，而是通过 `max_bucket` 和掩码组合自动计算分裂位置。这避免了额外的状态变量，同时保持线性哈希的核心语义。

## 2. CCEH：并发可扩展哈希

### 核心数据结构（`cceh_hash_private.h`）

```c
struct cceh_index {
    atomic_uint                  n_total;              /* 原子计数 */
    uint32_t                     segment_capacity;     /* 段容量 */
    atomic_uint                  segment_count;        /* 段数 */
    atomic_flag                  directory_latch;      /* 目录锁 */
    _Atomic(cceh_directory_t *)  directory_root;       /* 目录根指针 */
    cceh_thread_epoch_t         *thread_epochs;        /* 线程 epoch 注册 */
    cceh_retired_directory_t    *retired_directories;  /* 延迟回收队列 */
};
```

### 工程级特性

| 特性 | 演示代码 | 工程实现 |
|------|----------|----------|
| 并发安全 | 无 | 目录锁 + 段锁 + epoch-based reclamation |
| 持久化 | 无 | CLWB/CLFLUSHOPT 指令支持 PMEM |
| 内存对齐 | 无 | 64 字节缓存行对齐，避免 false sharing |
| 指纹优化 | 无 | 8 位 fingerprint 快速排除不匹配键 |
| 延迟回收 | 无 | epoch 保护下的安全内存回收 |

### 段分裂算法（`cceh_hash_insert.c`）

工程实现的分裂逻辑与演示代码类似，但增加了并发控制：

1. **Copy-on-Write**：克隆整个 segment，修改后原子替换
2. **目录扩展**：如果局部深度 == 全局深度，目录翻倍
3. **Epoch 保护**：旧结构进入 retired 队列，等待所有读者离开后回收

## 3. 关键差异总结

### 演示代码侧重
- 清晰展示线性哈希和可扩展哈希的核心思想
- 最小化代码，便于理解分裂指针、全局深度等概念
- 忽略持久化、并发等工程细节

### 工程实现侧重
- **pg_hash**：PostgreSQL 兼容的磁盘哈希索引，支持 WAL、页面溢出
- **CCEH**：面向 PMEM 的高并发哈希索引，支持无锁读、epoch-based 回收
- 两者都实现了生产级的错误处理、内存管理、性能优化

### 学到的设计模式
1. **掩码代替显式状态**：`high_mask/low_mask` 组合比 `split_pointer + level` 更紧凑
2. **溢出链**：页面级溢出链比简单链表更适合磁盘存储
3. **指纹优化**：8 位 fingerprint 可快速排除 99%+ 的不匹配键
4. **分离锁粒度**：目录锁 + 段锁实现高并发，避免全局锁竞争
