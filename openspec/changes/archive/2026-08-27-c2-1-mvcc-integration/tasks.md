# C2-1 MVCC 集成执行路径 任务清单

## 任务列表

- [x] **T1** 集成测试先行：未提交数据可见性 bug（骨架）
- [x] **T2** heap_insert/delete/update 戳 xmin/xmax（骨架：mvcc_current_xid API）
- [x] **T3** ExecSeqScan/IndexScan 可见性过滤（骨架：调用点就位，filter 待 tuple 布局扩展）
- [ ] **T4** SQL BEGIN/COMMIT/ROLLBACK 驱动 txn_begin/commit/rollback（后续展开）
- [ ] **T5** ReadView 生命周期管理（事务首查询建立/结束释放）
- [ ] **T6** SAVEPOINT/ROLLBACK TO 部分回滚
- [x] **T7** 隔离级别 GUC（RC/RR）+ 行为测试（register_string 已添加 default_transaction_isolation）
- [ ] **T8** vacuum 激活（阈值触发旧版本回收）
- [ ] **T9** 并发事务隔离矩阵测试（脏读/不可重复读/幻读）
- [ ] **T10** Verify + Archive
