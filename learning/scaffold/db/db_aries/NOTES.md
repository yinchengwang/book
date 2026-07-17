# ARIES 学习笔记：工程对照

## 工程轨现状

截至本 learning scaffold 创建时，工程轨（engineering/）尚未实现 ARIES 算法。
相关的已有基础设施包括：

- `engineering/src/db/core/wal.c` / `wal.h`：WAL（Write-Ahead Logging）基础实现
- `engineering/src/db/core/wal_buf.c`：WAL 缓冲区管理
- `engineering/src/db/core/bufmgr.c`：Buffer Pool 脏页管理
- `engineering/src/db/core/checkpoint.c`：检查点实现

## 待实现对照

### WAL 层 → 日志记录

工程轨 WAL 需要扩展以支持 ARIES 风格的日志格式：

```c
// 工程轨现有 WAL 日志结构（待扩展）
typedef struct WALRecord {
    XLogRecPtr  rec_ptr;      // LSN
    TransactionId txn_id;    // 事务 ID
    uint8       info;        // 日志类型
    Page        page;        // 受影响页（可选）
    /* ARIES 扩展： */
    int         prev_lsn;    // prev_lsn 链
    char        old_data[BLKSZ];
    char        new_data[BLKSZ];
} WALRecord;
```

### Buffer Pool → 脏页表

工程轨 `bufmgr.c` 的 `BufferDesc` 结构与 DPT（Dirty Page Table）对应：

```c
// 工程轨现有脏页跟踪（待扩展为完整 DPT）
typedef struct {
    int         page_id;
    XLogRecPtr  rec_lsn;      // 等价于 ARIES 的 rec_lsn
    bool        dirty;
    int         pin_count;
} BufferDesc;
```

### Checkpoint → 检查点日志

工程轨 `checkpoint.c` 需输出 ATT + DPT 到检查点日志：

```c
// ARIES 检查点格式
typedef struct {
    XLogRecPtr  checkpoint_lsn;
    /* ATT */
    int         active_txn_count;
    TransactionEntry active_txns[MAX_TRANSACTIONS];
    /* DPT */
    int         dirty_page_count;
    DirtyPageEntry dirty_pages[MAX_BUFFERS];
} ARIESCheckpoint;
```

### 恢复流程

```
工程轨现状：
  crash → check_wal() → replay_redo() → 简单回滚

ARIES 实现目标：
  crash → Analysis() → Redo() → Undo() → 一致状态
```

## 扩展方向

1. **LogRecord 结构**：添加 `prev_lsn`、`old_data`、`new_data` 字段
2. **脏页表**：BufferDesc 关联 rec_lsn，支持 `buf_get_rec_lsn()`
3. **恢复模块**：`recovery.c`，实现 ARIES 三阶段
4. **CLR 支持**：Undo 时写补偿日志，支持中断恢复
5. **部分回滚**：通过 `savepoint` 支持事务内部分回滚

## 学习建议

1. 先运行本 scaffold 的演示程序，理解三阶段的输入输出
2. 阅读 PostgreSQL 源码 `src/backend/access/transam/xlog.c` 中的 `xlog_redo()`
3. 对照工程轨 `wal.c` 理解当前 WAL 实现，逐步扩展为 ARIES 兼容格式
4. 参考 PostgreSQL 的 `TransamAnalysis`、`record-redo` 函数族
