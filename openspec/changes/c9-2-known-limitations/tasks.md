# C9-2 Known Limitations 任务清单

## 任务列表

### Task #1: 诊断 ConcurrentSearch 挂起
- **状态**: completed
- **描述**: 定位挂起根因（SQLite 锁竞争 or HNSW 非线程安全）
- **验收**: 明确根因并修复
- **结论**: 根因并非 SQLite/HNSW，而是 vector_api.c 三处内存缺陷：
  1. `vector_api_search` 插入排序 off-by-one：结果集满时写越界 `results[top_k]`，
     并发下损坏堆元数据 → malloc/free 内部锁挂死（表现为"偶发挂起"）
  2. 集合无锁：并发 insert 的 racing realloc / search 读已释放缓冲（ReadWriteMix c0000374）
     → 加集合级 `mmdb_rwlock_t`（insert/delete 写锁，search/size 读锁）
  3. `collection_expand` realloc 新增区域未清零 + insert 无元数据时槽位残留
     → `collection_destroy` free(0xBAADF00D 垃圾)（InsertPerformance/SearchLatency 必现）
     → expand 清零新增区域；insert 无元数据显式置 NULL

### Task #2: Blob Catalog 加 rwlock
- **状态**: completed
- **描述**: blob_catalog_s 加 mmdb_rwlock_t lock
- **验收**: 并发测试通过

### Task #3: Prepare 幂等性
- **状态**: completed
- **描述**: blob_catalog_prepare 检查状态
- **验收**: 重复 put 相同数据成功
- **结论**: 实现为幂等成功（PREPARED/COMMITTED 直接返回 OK），而非 design.md
  最初写的"COMMITTED 返回错误"——验收标准"再次 put 相同数据成功"优先；
  DELETED 状态回退正常 prepare 流程以支持 delete-then-re-put

### Task #4: 启用跳过的测试
- **状态**: completed
- **描述**: 移除 GTEST_SKIP
- **验收**: 43 测试全通过
- **结论**: blob_engine_test 43/43 通过；vdb_stress_test 5/5 通过（含
  ConcurrentSearch，120s 内无 hang，重复 10+ 次稳定）

## 完成状态

- [x] Task #1: 诊断 ConcurrentSearch 挂起
- [x] Task #2: Blob Catalog 加 rwlock
- [x] Task #3: Prepare 幂等性
- [x] Task #4: 启用跳过的测试

## 范围外发现（转入后续变更）

以下测试在本环境从未成功构建（HEAD 处 vector_api.c 的 `_mkdir` 隐式声明即
编译失败），修复编译后暴露的失败为历史遗留，不属于 C9-2 验收范围：

- `vdb_e2e_test` 6 个失败：SearchVectors（测试期望降序 vs 实现升序 k-NN，
  契约未在头文件约定）、DropCollection、PersistAndReload、LargeScalePersist、
  WALRecovery、CheckpointTest（后两者依赖未实现的 WAL/Checkpoint 持久化）
- `vdb_chaos_test` 5 个失败：WriteKillRecovery、IndexRebuildAfterKill、
  MultipleCrashRecovery、ConcurrentWriteWithCrash、SaveLoadCorrectness
  （均依赖未实现的崩溃恢复路径）
