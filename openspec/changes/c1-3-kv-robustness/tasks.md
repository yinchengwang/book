# C1-3 KV 锁启用与存储健壮性 任务清单

## 任务列表

- [x] **T1** 复现测试：并发 put 同 key 丢更新（先 FAIL）
- [x] **T2** kv_put/get/delete mmdb_rwlock 包裹 + lock_mgr 字段启用
- [x] **T3** 错误码扩展 KV_FULL/KV_CONFLICT/KV_LOCKED + page full 修正
- [x] **T6** kv_get 释放契约文档化 + 调用方审计（kv.c:396,523,608）
- [ ] **T4** 页分裂实现（半满分裂 + 上提）+ 批量插入测试
- [ ] **T5** Value 16MB 溢出页支持
- [ ] **T7** TTL tombstone 进 WAL
- [ ] **T8** 测试转 PASS + 回归
- [ ] **T9** Verify + Archive
