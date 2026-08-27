# C1-1 关系模态 TID 管道修复 任务清单

## 任务列表

- [x] **T1** 复现测试：多行表 UPDATE/DELETE 改错行 bug（先 FAIL）
- [ ] **T2** TupleTableSlot 增加 tts_tid + 扫描算子填充（nodeSeqscan/table_getnext）
- [ ] **T3** heap_insert 回填 tid、heap_update/delete 接受真实 tid
- [ ] **T4** nodeModifyTable.c 删除硬编码 TID（:70-75, 96-99）
- [ ] **T5** DML 错误传播 + mt_processed 真实化
- [ ] **T6** 测试转 PASS + 回归（SQL 集成测试全绿）
- [ ] **T7** Verify + Archive
