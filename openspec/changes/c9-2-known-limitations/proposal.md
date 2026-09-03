# C9-2 Known Limitations 提案

## Why

本会话实现的 Blob 引擎和 VDB 子系统有 3 个已知 limitation，以 `GTEST_SKIP` 或 `LOG_WARN` 标记。
这些 limitation 有明确根因和修复路径，修复后测试覆盖率可达 100%。

## What Changes

### Task 1: 诊断 ConcurrentSearch 挂起根因

修复 `VDBStressTest.ConcurrentSearch` 并发测试挂起问题。

### Task 2: Blob Catalog 加 rwlock

在 `blob_catalog_s` 中加 `mmdb_rwlock_t lock`，保护并发写操作。

### Task 3: Blob Catalog Prepare 幂等性

`blob_catalog_prepare` 入口检查 blob 状态，防止重复 prepare 覆盖已提交对象。

### Task 4: 启用跳过的测试

移除各测试中的 GTEST_SKIP 标记。

## Capabilities

| 能力 | 交付 |
|------|------|
| 并发搜索稳定 | 10 线程 × 50 查询无 hang |
| Catalog 线程安全 | 并发上传同一内容幂等一致 |
| 测试覆盖率 100% | 所有 GTEST_SKIP 移除 |

## Impact

- 修改 blob_catalog.c、blob_engine_test.cpp、vdb_stress_test.cpp
- 新增 HNSW 搜索路径锁诊断代码（如需要）

## 验收标准

- blob_engine_test 43 测试全部通过（之前 38 pass / 5 skip）
- VDBStressTest.ConcurrentSearch 通过（之前 skip）
- 再次 put 相同数据产生相同 blob_id 成功