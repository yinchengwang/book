# C9-2 Known Limitations 任务清单

## 任务列表

### Task #1: 诊断 ConcurrentSearch 挂起
- **状态**: pending
- **描述**: 定位挂起根因（SQLite 锁竞争 or HNSW 非线程安全）
- **验收**: 明确根因并修复

### Task #2: Blob Catalog 加 rwlock
- **状态**: pending
- **描述**: blob_catalog_s 加 mmdb_rwlock_t lock
- **验收**: 并发测试通过

### Task #3: Prepare 幂等性
- **状态**: pending
- **描述**: blob_catalog_prepare 检查状态
- **验收**: 重复 put 相同数据成功

### Task #4: 启用跳过的测试
- **状态**: pending
- **描述**: 移除 GTEST_SKIP
- **验收**: 43 测试全通过

## 完成状态

- [ ] Task #1: 诊断 ConcurrentSearch 挂起
- [ ] Task #2: Blob Catalog 加 rwlock
- [ ] Task #3: Prepare 幂等性
- [ ] Task #4: 启用跳过的测试
