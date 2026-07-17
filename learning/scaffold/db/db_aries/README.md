# ARIES 算法学习卡

## 简介

**ARIES** (Algorithms for Recovery and Isolation Exploiting Semantics) 是 IBM Research 于 1992 年提出的数据库故障恢复算法，是现代关系数据库（PostgreSQL、MySQL InnoDB、SQL Server、DB2）的恢复基石。

### 核心论文

- Chandra Bore, Don H. B. Johnson, Robert M. Selinger.
  *"ARIES: A Transaction Recovery Method Supporting Fine-Grained Locking and Partial Rollback."*
  IBM Research, 1992.

### 核心思想

1. **Write-Ahead Logging (WAL)**：所有修改在刷盘前必须先写日志
2. **物理逻辑结合**：日志记录采用物理页偏移 + 逻辑操作的混合方式
3. **REDO 优先**：恢复时先重做所有已落盘的动作，避免重复 I/O
4. **物理回滚**：通过补偿日志（CLR）实现精确回滚，支持部分回滚

## 三阶段恢复流程

```
崩溃 ──→ [Analysis] ──→ [Redo] ──→ [Undo] ──→ 一致状态
            │           │           │
        重建活跃      从检查点      沿 prev_lsn
        事务表       起重放日志      链回滚
        + 脏页表
```

### 阶段 1: Analysis（分析）

扫描日志文件（从最近的检查点开始），重建两个核心数据结构：

- **Active Transaction Table (ATT)**：记录所有未提交事务及其最后一条 LSN
- **Dirty Page Table (DPT)**：记录所有脏页及其最早变脏的 LSN（rec_lsn）

rec_lsn 非常重要：它告诉 Redo 阶段从哪个位置开始扫描即可覆盖所有未刷盘的修改。

### 阶段 2: Redo（重做）

从脏页表的 rec_lsn 最小值开始，逐条扫描 UPDATE 日志：

```
if (page_lsn < record_lsn) {
    // 页上还没有这条修改，需要重做
    apply_update(page, record);
    page_lsn = record_lsn;
}
```

Redo 阶段确保所有已记录在日志中的修改都反映到磁盘页上。

### 阶段 3: Undo（回滚）

对 Analysis 阶段发现的每个未提交事务，沿 `prev_lsn` 链反向遍历：

```
for each UPDATE log of uncommitted_txn:
    reverse_update(page, old_value);
    write CLR(page, compensation_log_record);  // 写入补偿日志
```

CLR 记录了回滚操作本身，即使系统再次崩溃，也能通过 CLR 继续恢复。

## 关键数据结构

| 结构 | 字段 | 用途 |
|------|------|------|
| LogRecord | lsn, txn_id, type, page_id, old_val, new_val, prev_lsn | 日志记录单元 |
| ATT | txn_id, last_lsn, status | 追踪活跃事务 |
| DPT | page_id, rec_lsn | 追踪脏页及最小恢复 LSN |
| CLR | lsn, txn_id, undo_next | 补偿日志，可从 Undo 恢复点继续 |

## 编译与运行

```bash
make        # 编译
make run    # 编译并运行
make clean  # 清理
```

输出包含三阶段恢复的完整日志，验证未提交事务被正确回滚。

## 学习要点

1. **为什么需要检查点（Checkpoint）？**
   检查点将 ATT 和 DPT 持久化到磁盘，使恢复不必从日志起点开始扫描。

2. **为什么 Redo 从 rec_lsn 开始而不是检查点开始？**
   rec_lsn 是页首次变脏的 LSN，之前该页一定是干净的，无需重做。

3. **CLR 的 prev_lsn 链有什么作用？**
   CLR 包含 `undo_next` 指向回滚的下一个位置，即使恢复中断也能继续完成回滚。

4. **与 PostgreSQL 恢复的关系**
   PostgreSQL 使用类似的逻辑：分析阶段（StartupXLOG）→ Redo（XLOG 恢复）→ Undo（事务回滚）。

5. **与 InnoDB 的区别**
   InnoDB 使用 `lsn`（Log Sequence Number）作为全局序列号，恢复时从 `checkpoint_lsn` 开始 Redo。

## 参考

- [ARIES 原始论文](https://www.vldb.org/conf/1992/P089.pdf)
- [PostgreSQL 恢复源码：src/backend/access/transam/xlog.c](https://github.com/postgres/postgres)
- [MySQL InnoDB 恢复机制](https://dev.mysql.com/doc/internals/en/innodb-recovery.html)
