# Linux 文件系统日志学习笔记

本文档记录文件系统日志机制在项目工程代码中的实际应用对照。

## 工程对照

### 1. 数据库日志 vs 文件系统日志

数据库的 WAL（Write-Ahead Logging）与文件系统的 Journal 在核心理念上高度一致，但在实现细节上有重要差异。工程代码中 WAL 的实现位于 `c:\code\book\engineering\src\db\storage\wal\wal.c`。

**共性：**

| 特性 | 文件系统 Journal | 数据库 WAL |
|------|-----------------|------------|
| 先写日志后写数据 | ext4 journal_commit 保证元数据原子性 | WAL 保证事务 ACID |
| 日志记录类型 | DESCRIPTOR / COMMIT / REVOKE | BEGIN / INSERT / UPDATE / DELETE / COMMIT |
| 恢复机制 | Redo 已提交事务 | Redo 已提交 + Undo 未提交 |
| 检查点 | 回收日志空间 | 减少恢复时间 |

**差异：**

| 维度 | 文件系统 Journal | 数据库 WAL |
|------|-----------------|------------|
| 粒度 | 块级别（4KB） | 逻辑操作（INSERT/UPDATE/DELETE） |
| 内容 | 完整的块副本（物理日志） | 操作描述 + 旧值/新值（逻辑+物理混合） |
| Undo | 不显式 Undo（未提交的不写回） | 显式 Undo（通过旧值回滚） |
| 事务管理 | 隐式（基于操作批次） | 显式（txn_begin/commit/rollback） |

### 2. WAL 日志类型与文件系统对照

工程代码定义了 7 种 WAL 日志类型（`c:\code\book\engineering\include\db\wal.h`）：

```c
// wal.h — WAL 日志记录类型
typedef enum wal_log_type_e {
    WAL_LOG_UPDATE = 1,      // 对应 ext4 的元数据更新
    WAL_LOG_INSERT = 2,      // 对应文件系统的块分配
    WAL_LOG_DELETE = 3,      // 对应文件系统的块释放（可类比 journal_revoke）
    WAL_LOG_COMMIT = 4,      // 对应 journal_commit 块
    WAL_LOG_ABORT = 5,       // 回滚标记（文件系统无此概念）
    WAL_LOG_CHECKPOINT = 6,  // 对应 journal_checkpoint
    WAL_LOG_BEGIN = 7        // 事务开始（文件系统隐式标记）
} wal_log_type_t;
```

### 3. 日志恢复流程对照

文件系统的恢复流程与 WAL 恢复在 `c:\code\book\engineering\src\db\storage\wal\wal.c` 中有直接对应：

**文件系统恢复：**
1. 扫描日志，找 CHECKPOINT
2. 对已 COMMIT 的事务 Redo（写回数据块）
3. 丢弃未 COMMIT 的事务

**WAL 恢复（更复杂）：**
1. `wal_analyze()` — 分析日志，找检查点和活动事务
2. `wal_redo()` — 重做已提交事务的所有修改
3. `wal_undo()` — 撤销未提交事务的修改（用旧值恢复）

```c
// wal.h — 恢复接口
int wal_analyze(const char *path, wal_recovery_info_t *info);
int wal_redo(const char *path, uint64_t start_lsn, wal_apply_fn apply_fn, void *ctx);
int wal_undo(const char *path, uint32_t txn_id, uint64_t start_lsn,
             wal_apply_fn apply_fn, void *ctx);
```

### 4. 数据模式与数据库持久性策略

文件系统的三种数据模式（ordered/writeback/data）在数据库中有对应的持久性配置：

| 文件系统模式 | 数据库对照 | 说明 |
|-------------|-----------|------|
| data=journal | `fsync=on, wal_level=replica` | 最高安全，双写 |
| data=ordered | `fsync=on`（默认） | 平衡安全与性能 |
| data=writeback | `fsync=off` | 高性能，可能丢数据 |

### 5. journal_revoke 与数据库中的类比

文件系统的 `journal_revoke` 机制在数据库中可以通过以下方式体现：

- **DROP TABLE 场景**：表被删除后，该表相关的 WAL 日志在恢复时不再有意义
- **TRUNCATE 场景**：数据被截断，相关页面无需恢复
- 工程中的 `WAL_LOG_DELETE` 类型本质上起到了类似 revoke 的作用：标记某数据已被删除

```c
// wal.c 中的删除日志（类比 journal_revoke）
uint64_t wal_write_delete(wal_t *wal, uint32_t txn_id,
                          const void *key, size_t key_len,
                          const void *old_value, size_t old_len);
// 恢复时：如果遇到 DELETE 日志，旧值用于 Undo 回滚
// 类似于 revoke 标记"此块不再有效"
```

## 参考源码

- `c:\code\book\engineering\src\db\storage\wal\wal.c` — WAL 日志核心实现
- `c:\code\book\engineering\src\db\storage\wal\wal_buf.c` — WAL Buffer 管理
- `c:\code\book\engineering\include\db\wal.h` — WAL 接口定义
- `c:\code\book\engineering\include\db\txn.h` — 事务管理器接口
- `c:\code\book\engineering\src\db\storage\buffer\bufmgr.c` — Buffer Pool（对应页缓存回写）
