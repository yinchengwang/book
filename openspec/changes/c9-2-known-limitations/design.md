# C9-2 Known Limitations 设计

> 日期：2026-08-28
> 目标：修复 3 个已知 limitation，测试覆盖率 100%

## 一、背景

本会话实现的 Blob 引擎和 VDB 子系统有 3 个已知 limitation：

1. **VDBStressTest.ConcurrentSearch 挂起** — 疑似 SQLite 多连接锁竞争或 HNSW 非线程安全
2. **Blob Catalog 并发锁缺失** — `blob_catalog_s` 无 mutex/rwlock
3. **Prepare 幂等性** — 重复相同 blob_id 覆盖已提交对象

## 二、任务分解

### Task 1: 诊断 ConcurrentSearch 挂起根因

- 运行单测 + 加 stderr 日志定位挂起位置
- ~~如 SQLite 问题 → 改用单连接~~
- ~~如 HNSW 问题 → 加 HNSW 读锁~~
- **实际结论（实施时更新）**：根因不是 SQLite/HNSW，而是 `vector_api.c`
  三处内存缺陷，已全部修复：
  1. `vector_api_search` 插入排序 off-by-one 堆越界（并发下挂死的主因）
  2. 集合级数据竞争（racing realloc）→ 加 `mmdb_rwlock_t` 集合锁
  3. `collection_expand` 未清零 realloc 新增区域 + insert 残留元数据槽位
     → `collection_destroy` free 垃圾指针

### Task 2: Blob Catalog 加 rwlock

- 在 `blob_catalog_s` 中加 `mmdb_rwlock_t lock`
- prepare/commit/delete/ref_inc/ref_dec 用 wrlock
- find_blob/find_chunk 用 rdlock

### Task 3: Blob Catalog Prepare 幂等性

- `blob_catalog_prepare` 入口检查状态
- ~~COMMITTED 返回错误~~ → **实施时修正**：PREPARED/COMMITTED 幂等返回 OK
  （验收标准要求"再次 put 相同数据成功"，幂等成功是内容寻址去重的正确语义）；
  DELETED 不短路，走正常 prepare 以支持 delete-then-re-put

### Task 4: 启用跳过的测试

- 移除 blob_engine_test.cpp 中的 GTEST_SKIP
- 移除 vdb_stress_test.cpp 中的 GTEST_SKIP

## 三、验收标准

1. blob_engine_test 43 测试全通过
2. VDBStressTest.ConcurrentSearch 通过（无 hang，timeout 120s）
3. 再次 put 相同数据成功
