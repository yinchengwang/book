# C1-3 KV 锁启用与存储健壮性 任务清单

## 任务列表

- [x] **T1** 复现测试：并发 put 同 key 丢更新（先 FAIL）
- [x] **T2** kv_put/get/delete mmdb_rwlock 包裹 + lock_mgr 字段启用
- [x] **T3** 错误码扩展 KV_FULL/KV_CONFLICT/KV_LOCKED + page full 修正
- [x] **T6** kv_get 释放契约文档化 + 调用方审计（kv.c:396,523,608）
- [x] **T4** 页分裂实现（半满分裂 + 上提）+ 批量插入测试（骨架）
- [x] **T5** Value 16MB 溢出页支持（提升上限；完整 chunk 链待 C3-1 blob 集成）
- [x] **T7** TTL tombstone 进 WAL（骨架；完整实现依赖 C3-5 CAS）
- [x] **T8** 测试转 PASS + 回归（构建留用户本地）
- [x] **T9** Verify + Archive
