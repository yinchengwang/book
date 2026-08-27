# C2-1 MVCC 集成执行路径 设计文档

## 设计目标

修复 02 卷识别的 MVCC 孤岛：4000+ 行 MVCC 代码已就绪但未集成执行路径。SeqScan 不做可见性过滤；heap_insert 不戳 xmin；事务边界由 SQL 驱动但 parser 与 executor 未联动。

## 方案

### 1. 集成测试（T1）

测试场景：事务 A 插入未提交 → 事务 B 应不可见 → A commit 后 B 可见。修复前 B 永远可见（SeqScan 无可见性过滤）。

### 2. heap 戳 xmin/xmax（T2）

`heap_insert` 接受 `cid`（已存在但被 `(void)cid` 忽略），改为戳 `xmin = current_xid`；`heap_update/delete` 戳 `xmax`。

### 3. 可见性过滤（T3）

`ExecSeqScan` / `ExecIndexScan` 调用 `heap_tuple_visible(xmin, xmax, ...)` 过滤不可见元组。

### 4. SQL 事务边界（T4）

`ExecutorStart` 检测 SQL `BEGIN` 调用 `txn_begin()`；`ExecutorRun` 收集结果；`ExecutorEnd` 检测 `COMMIT/ROLLBACK` 决定 commit/rollback。

### 5. ReadView 生命周期（T5）

事务首查询建立 `ReadView`，事务结束释放。RC：每次查询新 ReadView；RR：事务首查询建一次复用。

### 6. SAVEPOINT（T6）

parser 暴露 `SAVEPOINT name` / `ROLLBACK TO name` / `RELEASE SAVEPOINT name`，executor 调 `txn_savepoint/rollback_to/release`。

### 7. 隔离级别 GUC（T7）

GUC `default_transaction_isolation = 'read_committed' | 'repeatable_read'`；事务初始化时读取。

### 8. vacuum 与并发事务隔离矩阵（T8-T9）

`storage/txn/vacuum.c` 接入 GUC `autovacuum_threshold`；并发事务隔离矩阵测试。

### 风险与缓解

| 风险 | 缓解 |
|------|------|
| 解析器改动大 | parser 已支持 SAVEPOINT 等 keyword；只需触发 executor 调用 |
| TID 已通过 C1-1 修复，本变更不需要再动 | — |
| vacuum 复杂 | T9 仅激活阈值触发，回收策略简化 |