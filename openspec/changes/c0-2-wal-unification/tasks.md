# C0-2 共享 WAL 统一覆盖 任务清单

## 任务列表

- [ ] **T1** wal_flush_policy_t 四级策略 + GUC wal_sync_mode + fsync 封装（Windows FlushFileBuffers）
- [ ] **T2** 共享 WAL 记录类型扩展（heap/ts/spatial/yang 四族）
- [ ] **T3** heapam.c DML 接入 WAL（insert/delete/update 三记录，WAL-first 顺序）
- [ ] **T4** ts_engine 接入 WAL（append 记录）
- [ ] **T5** spatial_engine 接入 WAL（upsert 记录）
- [ ] **T6** yang datastore 写接入 WAL（依赖 C2-5 datastore 落地的部分后置）
- [ ] **T7** KV wal_write_* 返回值检查 + 失败中止（kv.c:445,459）
- [ ] **T8** vector_wal.c 修复：VLA → 堆缓冲（:261）、异步 memcpy 边界（:292-298）、SYNC 接统一策略
- [ ] **T9** db_startup_recover() 统一恢复入口 + 模态 apply 回调注册
- [ ] **T10** 崩溃恢复集成测试（五模态 kill -9 重启数据完整）
- [ ] **T11** Verify + Archive
