# C0-1 统一并发原语推广 任务清单

## 任务列表

- [ ] **T1** mmdb_lock.h 迁移到 include/db/（SDK 兼容 include 保留）
- [ ] **T2** vector_engine.c 替换 simple_rwlock → mmdb_rwlock（删除 :1552-1606 复刻实现）
- [ ] **T3** ts_engine.c 替换 ts_rwlock → mmdb_rwlock
- [ ] **T4** doc_engine.c 替换 doc_rwlock → mmdb_rwlock
- [ ] **T5** graph_csr.c + graph_engine.c 接入 mmdb_rwlock（新增，含 CSR 双视图基础）
- [ ] **T6** rtree.c + spatial_engine.c 接入 mmdb_rwlock（新增）
- [ ] **T7** use_lock 默认值反转（false → true）+ mm_disable_lock() API
- [ ] **T8** 每模态并发回归测试（双线程读写混合 30s 无死锁无崩溃）
- [ ] **T9** Verify + Archive
