# Undo Log 工程对照笔记

## 概述

本文档记录学习代码与工程实现（`engineering/src/db/`）之间的对照关系。

## 工程源码位置

### 1. MVCC Undo 实现

**文件**：`engineering/src/db/concurrency/mvcc_version.c`

**核心函数**：

| 函数 | 行号 | 功能 |
|------|------|------|
| `mvcc_undo_create` | 96 | 创建 Undo 记录 |
| `mvcc_undo_destroy` | 136 | 销毁 Undo 记录 |
| `mvcc_undo_apply` | 148 | 应用 Undo 回滚 |

**数据结构**：

```c
// mvcc_undo_record_t 结构（简化）
typedef struct {
    mvcc_txn_id_t     txn_id;        /* 事务 ID */
    mvcc_undo_type_t  type;          /* UNDO_INSERT/UPDATE/DELETE */
    mvcc_ctid_t       row_ptr;       /* 行指针（block_num, offset） */
    void             *old_data;      /* 旧数据副本 */
    size_t            old_data_size; /* 旧数据大小 */
} mvcc_undo_record_t;
```

### 2. WAL Undo 支持

**文件**：`engineering/src/db/storage/wal/wal.c`

**关键函数**：

- `wal_undo`（行 582）：WAL 层 Undo 操作入口
- `wal_buf_do_undo`（行 321）：WAL Buffer 层 Undo 执行

### 3. Undo 垃圾回收

**文件**：`engineering/src/db/concurrency/mvcc_gc.c`

**函数**：`mvcc_gc_cleanup_undo`（行 143）

清理策略：
1. Undo 日志按事务分组
2. 标记已提交事务的 Undo 为可回收
3. 当所有活跃事务的最小 xmin 大于 Undo 记录的事务号时，该 Undo 可清理

## 学习代码与工程实现对照

| 学习代码概念 | 工程实现对应 |
|-------------|-------------|
| `undo_record_t` | `mvcc_undo_record_t` |
| `undo_type_t` | `mvcc_undo_type_t`（UNDO_INSERT/UPDATE/DELETE） |
| `undo_create()` | `mvcc_undo_create()` |
| `undo_destroy()` | `mvcc_undo_destroy()` |
| `undo_apply()` | `mvcc_undo_apply()` |

## 关键差异

1. **指针类型**：工程使用 `mvcc_ctid_t`（行指针），学习代码使用 page_id + offset 分开存储
2. **内存管理**：工程 Undo 数据与页面关联，学习代码演示简化版本
3. **版本链**：工程实现 Undo 版本链（`prev_undo`），支持 MVCC 多版本追溯
4. **持久化**：工程 Undo 需要与 WAL 配合实现持久化，学习代码仅演示内存操作

## 深入学习建议

1. 阅读 `mvcc_version.c` 中版本链的构建逻辑
2. 理解 Undo 在 MVCC 快照读取中的作用
3. 研究 Undo 空间管理（段文件、链表）
4. 分析崩溃恢复时 Undo 的执行顺序
