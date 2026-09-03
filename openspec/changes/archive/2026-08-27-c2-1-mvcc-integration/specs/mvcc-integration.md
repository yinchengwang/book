# MVCC 可见性规范（新增）

## 目的

让已有的 MVCC 模块族真正生效：SeqScan/IndexScan 做可见性过滤；heap 戳 xmin/xmax；事务边界由 SQL 驱动。

## 要求

### REQ-1：xmin/xmax 戳记

`heap_insert` 必须戳 `xmin = current_xid`；`heap_update` 戳新行 `xmin = current_xid` + 旧行 `xmax = current_xid`；`heap_delete` 戳 `xmax = current_xid`。移除 `heapam.c:300` 的 `(void)cid` 忽略。

### REQ-2：可见性过滤

`ExecSeqScan` 与 `ExecIndexScan` 必须调用 `heap_tuple_visible()` 过滤不可见元组。ReadView 在事务首查询建立。

### REQ-3：事务边界

SQL `BEGIN` 触发 `txn_begin`；`COMMIT`/`ROLLBACK` 触发 `txn_commit`/`txn_rollback`；`END` 隐式 commit。

### REQ-4：隔离级别

GUC `default_transaction_isolation` 取值 `'read_committed'`（默认）或 `'repeatable_read'`。

### REQ-5：SAVEPOINT

`SAVEPOINT name` / `ROLLBACK TO name` / `RELEASE SAVEPOINT name` 通过 parser → executor 链路调 `txn_savepoint` / `txn_rollback_to` / `txn_release_savepoint`。

## 实现文件

- `engineering/src/db/storage/access/heap/heapam.c`（xmin/xmax 戳记）
- `engineering/src/db/sql/nodeSeqscan.c`、`nodeIndexscan.c`（可见性过滤）
- `engineering/src/db/sql/executor.c`（事务生命周期）
- `engineering/src/db/sql/parser/`（SAVEPOINT 语法）
- `engineering/src/db/core/guc.c`（隔离级别 GUC）