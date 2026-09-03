# Linux 文件系统缓存学习笔记

本文档记录文件系统缓存机制在项目工程代码中的实际应用对照。

## 工程对照

### 1. 内核页缓存 vs 数据库 Buffer Pool

数据库存储引擎维护自己的缓存层（Buffer Pool），与内核页缓存形成独立的两级缓存架构：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// Buffer Pool 使用 Clock-Sweep 算法管理页面缓存
// 与内核页缓存（双 LRU 链表）相互独立但又互补
```

**两级缓存对比：**

| 特性 | 内核页缓存 | Buffer Pool |
|------|-----------|-------------|
| 管理方 | 内核 | 数据库进程 |
| 淘汰算法 | 双 LRU（Active/Inactive） | Clock-Sweep（LRU 近似） |
| 粒度 | 4KB 页 | 可配置（8KB 默认） |
| 脏页回写 | pdflush 后台线程 | 主动 buf_flush_all() |
| 缓存清理 | drop_caches | buffer_flush_all() |

### 2. Clock-Sweep 与内核 LRU 算法对比

```c
// engineering/src/db/storage/buffer/bufmgr.c:376-417
/**
 * Clock-Sweep（又称 Second-Chance）是 LRU 的近似实现，
 * 在 O(1) 时间复杂度内找到可置换的 Buffer，避免全表扫描。
 *
 * 与 LRU 的对比：
 * | 特性     | LRU          | Clock-Sweep   |
 * | 时间复杂度 | O(1)        | O(1)          |
 * | 空间复杂度 | O(n)        | O(1)          |
 * | 实现复杂度 | 复杂（双向链表）| 简单          |
 */
```

内核页缓存使用更精细的双 LRU（Active/Inactive 链表），而数据库 Buffer Pool 使用更简单的 Clock-Sweep。两者设计目标不同：内核需要处理多种工作负载（文件 I/O、mmap、swap），而数据库 Buffer Pool 只关心数据库页面，可以做出更强的访问模式假设。

### 3. 脏页管理策略对照

```c
// engineering/src/db/storage/buffer/bufmgr.c:743-755
void buf_dirty(BufferDesc *buf) {
    // 标记页面为脏页，等待后续 Clock-Sweep 回收时刷盘
    // 或主动调用 buf_flush_all() 批量刷盘
}

// engineering/src/db/storage/buffer/bufmgr.c:791-819
int buf_flush_all(void) {
    // 遍历所有 buffer，将脏页刷入磁盘
    // 相当于内核的 sync() + drop_caches 组合操作
}
```

**内核脏页回写触发条件：**

| 参数 | 含义 | 默认值 |
|------|------|--------|
| dirty_background_ratio | 后台回写阈值 | 10% |
| dirty_ratio | 同步回写阈值 | 20% |
| dirty_expire_centisecs | 脏页过期时间 | 3000（30s） |

数据库 Buffer Pool 的回写由应用层控制，可以更精确地决定何时刷盘（检查点、事务提交、Buffer 不足时）。

### 4. 直接 I/O 绕过内核缓存

```c
// engineering/src/db/storage/page/disk.c:171
// 默认使用缓存 I/O，但可通过 O_DIRECT 绕过
file->fd = open(path, O_RDWR | O_CREAT, 0644);

// 若启用直接 I/O：
// file->fd = open(path, O_RDWR | O_CREAT | O_DIRECT, 0644);
```

使用 O_DIRECT 可以避免双重缓存（内核页缓存 + Buffer Pool 同时缓存相同数据），减少内存浪费和数据复制开销。但代价是失去了内核预读和写入合并等优化。

### 5. 缓存性能测试建议

基准测试时清空缓存的标准流程：

```bash
# 1. 刷盘所有 dirty 页
sync

# 2. 清空内核缓存
echo 3 > /proc/sys/vm/drop_caches

# 3. 确认缓存已清空
cat /proc/meminfo | grep -E "^(Cached|Dirty|MemFree)"
```

## 参考源码

- `engineering/src/db/storage/buffer/bufmgr.c` — Buffer Pool 缓存管理（Clock-Sweep 置换）
- `engineering/src/db/storage/buffer/buffer.c` — 缓存池实现（LRU 链表版本）
- `engineering/src/db/storage/page/disk.c` — 磁盘 I/O 层（缓存 I/O 与直接 I/O）
