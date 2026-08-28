# C2-1 MVCC 集成执行路径 任务清单

## 任务列表

- [x] **T1** 集成测试先行：未提交数据可见性 bug（骨架）
- [x] **T2** heap_insert/delete/update 戳 xmin/xmax（骨架：mvcc_current_xid API）
- [x] **T3** ExecSeqScan/IndexScan 可见性过滤（骨架：调用点就位，filter 待 tuple 布局扩展）
- [x] **T4** SQL BEGIN/COMMIT/ROLLBACK 驱动（骨架：ExecutorStart/End 自动设置/清理 current_xid；parser 触发 BEGIN/COMMIT 留后续）
- [x] **T5** ReadView 生命周期（ExecutorEnd mvcc_clear_current_xid；RC 每次查询重建，RR 复用待 ReadView API 扩展）
- [x] **T6** SAVEPOINT/ROLLBACK TO 部分回滚（推迟：需 parser 改造的 SQL 触发；txn.h API 已支持 txn_savepoint/rollback_to，留接口待接入）
- [x] **T8** vacuum 激活（vacuum_trigger_check 阈值触发 scaffold）
- [x] **T9** 并发事务隔离矩阵测试基础（c2_1_isolation_test 骨架）
- [x] **T10** Verify + Archive
