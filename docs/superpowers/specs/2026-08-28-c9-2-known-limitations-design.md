# C9-2 Known Limitations 设计

> 日期：2026-08-28
> 目标：修复已知的 3 个限制项，使 blob_engine_test 和 vdb_stress_test 测试全通过

## 一、背景

本会话实现的 Blob 引擎和 VDB 子系统有 3 个已知 limitation，以 `GTEST_SKIP` 标记。
这些 limitation 有明确根因和修复路径。本变更的目标：
1. 修复 HNSW search 路径的锁泄漏（VDBStressTest.ConcurrentSearch 挂起）
2. 给 blob_catalog_t 加 rwlock（解决并发上传冲突）
3. 给 blob_catalog_prepare 加幂等性检查（重复相同 blob_id 不再覆盖已提交对象）

## 二、根因（调研结果）

### 2.1 VDBStressTest.ConcurrentSearch 挂起

**位置**：`engineering/src/sdk/vectors/vectors.c` 第 1148 行

**根因**：在 HNSW 搜索路径上，`mmdb_rwlock_rdlock(c->coll_lock)` 在第 1148 行获取，
但第 1144 行（在 rdlock 之前）的 `calloc(distances/hnsw_ids)` 失败时 return MMDB_ERR_NOMEM。
锁已在此路径获取，但未释放就返回。

**Wait！** 重新分析：
- 第 1135-1141 行：分配 `distances` 和 `hnsw_ids`（**无锁**）
- 第 1143 行：失败 return MMDB_ERR_NOMEM（**无锁**，正确）
- 第 1148 行：`mmdb_rwlock_rdlock(c->coll_lock)`
- 第 1149-1155 行：执行搜索 + unlock

**重新确认**：agent 报告的"第1144行 return 在持锁后"有误。第 1144 行的 return 在 rdlock（1148行）**之前**。
需要重新调查挂起根因——可能在其他地方（SQLite 内部锁竞争，或 faiss_hnsw_search_filtered 内部死锁）。

**修正后的假设**：
挂起可能源于：
1. `faiss_hnsw_search_filtered` 内部使用了未保护的全局资源
2. SQLite 多个连接在同一 DB 文件上的锁竞争
3. hnr_wrapper_t 内的索引数据结构非线程安全

### 2.2 Blob Catalog 并发锁缺失

**位置**：`engineering/src/db/storage/blob/blob_catalog.c`

**根因**：`blob_catalog_s` struct 中无任何 mutex/rwlock 字段。
5 个修改函数（prepare/commit/delete/ref_inc/ref_dec）直接线性探测写哈希表，
多线程并发调用存在数据竞争。

**修复方案**：
- 在 `blob_catalog_s` 中加 `mmdb_rwlock_t lock`
- 在 `blob_catalog_open` 中 `mmdb_rwlock_init`
- 在 `blob_catalog_close` 中 `mmdb_rwlock_destroy`
- 在 5 个修改函数中，进入事务前获取写锁，退出时释放
- 在 find_blob/find_chunk 中获取读锁

### 2.3 Blob Catalog Prepare 幂等性

**位置**：`engineering/src/db/storage/blob/blob_catalog.c` `upsert_blob_entry` 第 256-258 行

**根因**：当找到相同 blob_id 时直接覆盖 entry（含 state 字段），
不检查当前状态，不检查是否已有 WAL 记录。

**修复方案**：
- `blob_catalog_prepare` 入口检查：如果 entry 已存在且 state == COMMITTED，返回错误（已提交）
- `blob_catalog_prepare` 入口检查：如果 entry 已存在且 state == PREPARED，追加一条新的 PREPARE WAL 记录（幂等）
- `upsert_blob_entry` 保持现有行为（直接覆盖），但调用方负责保证语义

## 三、任务分解

### Task 1: 诊断 ConcurrentSearch 挂起根因

**步骤**：
1. 运行 `VDBStressTest.ConcurrentSearch` 单独测试（不与其他测试并发）
2. 加入 stderr 日志，确认在哪一行挂起
3. 根据挂起位置决定修复策略
4. 如怀疑 SQLite 多连接问题，改用单连接或连接池
5. 如怀疑 HNSW 非线程安全，加 HNSW 读锁

**验收**：10 线程 × 50 查询全部完成，无 hang。

### Task 2: Blob Catalog 加 rwlock

**修改文件**：
- `engineering/include/db/blob_catalog.h` — blob_catalog_s 加 rwlock 字段（前向声明保持不透明）
- `engineering/src/db/storage/blob/blob_catalog.c` — 实现加锁逻辑
- `engineering/include/db/mmdb_lock.h` — 确认 mmdb_rwlock_t API 兼容

**具体改动**：
```c
struct blob_catalog_s {
    ...
    mmdb_rwlock_t lock;  /* 新增 */
};
```

**lock 使用规则**：
- 写操作（prepare/commit/delete/ref_inc/ref_dec）：wrlock
- 读操作（find_blob/find_chunk）：rdlock
- 事务begin/end：wrlock + WAL 写入

### Task 3: Blob Catalog Prepare 幂等性

**修改文件**：
- `engineering/src/db/storage/blob/blob_catalog.c` — `blob_catalog_prepare` 加前置检查

**具体改动**：
```c
int blob_catalog_prepare(catalog, blob_id, size, count) {
    // 检查是否已存在
    blob_entry_t existing;
    if (blob_catalog_find_blob(catalog, blob_id, &existing) == BLOB_CATALOG_OK) {
        if (existing.state == BLOB_STATE_COMMITTED) {
            return BLOB_CATALOG_ERR_STATE;  // 已提交，不允许重复 prepare
        }
        if (existing.state == BLOB_STATE_PREPARED) {
            // 幂等：重复 prepare 合法，追加 WAL 记录
            return write_wal_record(...);
        }
    }
    // 正常 prepare 逻辑...
}
```

### Task 4: 启用跳过的测试

- `engineering/test/db/storage/blob_engine_test.cpp` 移除 `DuplicateContentDedup` / `ConcurrentDifferentUploads` / `ConcurrentSameContentUpload` 的 GTEST_SKIP
- `engineering/test/db/integration/vdb_stress_test.cpp` 移除 `ConcurrentSearch` 的 GTEST_SKIP
- 验证测试通过

## 四、验收标准

1. `cmake --build build/engineering --target cf_engine` 通过（无新错误）
2. 手动链接的 blob_engine_test 全通过（43 测试，0 失败）
3. 手动链接的 vdb_stress_test 的 `ConcurrentSearch` 通过（无 hang，timeout 120s）
4. 再次 put 相同数据产生相同 blob_id 不再报错（幂等）

## 五、风险与缓解

| 风险 | 缓解 |
|------|------|
| rwlock 加锁影响性能 | 只在修改函数加 wrlock，读取用 rdlock；性能回归 ≤ 10% |
| ConcurrentSearch 挂起根因难以定位 | 先加超时断言，超时打印栈，逐步缩小范围 |
| Prepare 幂等性与其他路径冲突 | 只在 blob_put 调用路径验证，其他路径不受影响 |

## 六、文件清单

修改：
- `engineering/src/db/storage/blob/blob_catalog.c`（加 rwlock + 幂等性）
- `engineering/test/db/storage/blob_engine_test.cpp`（移除 GTEST_SKIP）
- `engineering/test/db/integration/vdb_stress_test.cpp`（移除 GTEST_SKIP）
