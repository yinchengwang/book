# C2-1 MVCC 集成执行路径 任务清单

## 任务列表

- [ ] **T1** 集成测试先行：未提交数据可见性 bug（先 FAIL）
- [ ] **T2** heap_insert/delete/update 戳 xmin/xmax（移除 heapam.c:300 的 `(void)cid`）
- [ ] **T3** ExecSeqScan/IndexScan 可见性过滤（heap_tuple_visible 接入）
- [ ] **T4** SQL BEGIN/COMMIT/ROLLBACK 驱动 txn_begin/commit/rollback
- [ ] **T5** ReadView 生命周期管理（事务首查询建立/结束释放）
- [ ] **T6** SAVEPOINT/ROLLBACK TO 部分回滚
- [ ] **T7** 隔离级别 GUC（RC/RR）+ 行为测试
- [ ] **T8** vacuum 激活（阈值触发旧版本回收）
- [ ] **T9** 并发事务隔离矩阵测试（脏读/不可重复读/幻读）
- [ ] **T10** Verify + Archive
