# Change Buffer 在 buf.h 中的抽象实现

## 概述

Change Buffer（变更缓冲）是数据库系统中用于优化二级索引写操作的核心组件。在 `engineering/include/db/storage/buffer/buf.h` 中，通过 Buffer Pool 的基础架构间接支持 Change Buffer 功能。

## 与 Buffer Pool 的关系

`buf.h` 定义了 Buffer Pool 的核心接口，Change Buffer 需要借助以下组件工作：

### 1. BufferDesc 描述符结构

```c
struct BufferDesc_s {
    uint32_t    buf_id;           // Buffer ID
    BufferState state;            // 状态标志
    uint32_t    relfilenode;      // 物理文件节点
    BlockNumber blocknum;         // 块号
    int         usage_count;      // Clock-Sweep 计数
    LSN         last_written;     // 最后写入的 LSN
    uint32_t    hash_prev;        // Hash 链表前驱
    uint32_t    hash_next;        // Hash 链表后继
    int         refcount;         // 引用计数
};
```

每个缓存的索引页面都对应一个 `BufferDesc`，用于追踪页面状态和实现置换策略。

### 2. Buffer 状态标志

`buf.h` 定义了完整的状态标志体系，Change Buffer 涉及的标志：

- `BM_VALID` — 页面数据有效
- `BM_DIRTY` — 页面被修改（脏页）
- `BM_PINNED` — 页面被 pin 住（正在使用）

### 3. 页面读写 API

Change Buffer 依赖以下核心 API：

| API | 作用 |
|-----|------|
| `buf_read()` | 将页面从磁盘读入 Buffer Pool |
| `buf_new()` | 分配新页面 |
| `buf_dirty()` | 标记页面为脏 |
| `buf_write()` | 将 Buffer 刷写到磁盘 |

## Change Buffer 工作流程

1. **写操作捕获**：当对非唯一二级索引执行 INSERT/UPDATE/DELETE 时，若对应页面不在 Buffer Pool 中，将操作记录到 Change Buffer。

2. **延迟应用**：Change Buffer 中的修改不会立即写入主索引，而是异步合并。

3. **Merge 触发**：当目标页面被读入 Buffer Pool 时（通过 `buf_read()`），Change Buffer 中的修改被合并到主索引页面。

4. **脏页刷盘**：合并后的页面标记为脏（`buf_dirty()`），最终通过 `buf_flush_relation()` 或 `buf_flush_all()` 刷写到磁盘。

## 与 buf.h 的集成点

在真实实现中，Change Buffer 与 Buffer Pool 的集成点包括：

1. **页面未命中时**：在 `buf_read()` 返回前检查 Change Buffer，若命中则应用待合并的修改。

2. **内存管理**：Change Buffer 条目存储在独立内存区域，不占用 Buffer Pool 空间。

3. **持久化**：Change Buffer 本身也需要持久化（类似 WAL），以支持崩溃恢复。

4. **统计信息**：通过 `buf_stats_t` 结构收集 Change Buffer 相关统计（命中次数、合并次数等）。

## 参考实现

MySQL InnoDB 的 Change Buffer 实现位于 `ibuf*` 系列文件。关键概念：
- **IBUF_BITMAP_PAGE**：记录哪些页面在 Change Buffer 中有数据
- **ibuf_insert()**：将操作写入 Change Buffer
- **ibuf_merge()**：将 Change Buffer 合并到主索引

本示例代码简化了这些概念，提供了一个可运行的 Change Buffer 演示。
