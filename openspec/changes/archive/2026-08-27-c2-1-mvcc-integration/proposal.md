# C2-1 MVCC 集成执行路径 Proposal

## Why

差距分析 02 卷核心发现：MVCC 模块族（txn.h 4000+ 行：xmin/xmax/CID/保存点/子事务/2PC）已完整实现但**零集成**——`heap_tuple_visible` 无任何调用方（`storage/txn/heap_visibility.c:30`）、`txn_begin` 只在 txn 模块内部出现、SeqScan 无可见性过滤（`nodeSeqscan.c:223-256` 仅 ExecQual）、heap_insert 不戳 xmin（`heapam.c:300` `(void)cid`）。模块完成 ≠ 系统完成，并发读写正确性保证实际不存在。

## What Changes

- ExecSeqScan/IndexScan 拉取元组后调用 `heap_tuple_visible` 过滤（不可见行跳过）
- heap_insert 戳 xmin（当前事务 id）、heap_delete/update 戳 xmax
- SQL 层 BEGIN/COMMIT/ROLLBACK/SAVEPOINT 语法驱动 `txn_begin/commit/rollback`（parser 已有基础）
- ReadView 生命周期：事务内首查询建立、事务结束释放
- vacuum 接入：旧版本回收（storage/txn/vacuum.c 激活，阈值触发）
- 隔离级别：Read Committed 与 Repeatable Read 两级（Serializable 后置）

## Capabilities

| 能力 | 交付 |
|------|------|
| 快照读 | 未提交数据对并发事务不可见（集成测试） |
| 事务回滚 | ROLLBACK 后数据恢复原状 |
| 保存点 | SAVEPOINT/ROLLBACK TO 部分回滚 |
| 隔离级别 | RC/RR 可选，行为符合 SQL 标准 |

## Impact

- 修改：nodeSeqscan.c、nodeIndexscan.c、heapam.c、executor.c、parser/（BEGIN 等语法）、txn 集成层
- 新增：MVCC 集成测试（并发事务隔离矩阵）
- 预计 6-8 个 commit
- 依赖：C0-2、C1-1
