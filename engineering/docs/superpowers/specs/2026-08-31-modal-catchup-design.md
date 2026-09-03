# 多模态存储引擎功能与性能补齐方案

> **目标：** 将 15 个存储模态 + 12 个基础设施组件从原型状态提升到商业可用标准

**架构：** 分层依赖方案，Phase 0 基础设施 → Phase 1 数据破坏修复 → Phase 2 功能补完 → Phase 3 性能优化 → Phase 4 新增特性

**技术栈：** C14, GTest, POSIX/Win32 双平台, WAL-first 恢复, MVCC 隔离

---

## 全局约束

1. 所有模态使用统一 `storage_ops_t` 接口注册
2. 统一错误码 `storage_result_t`，禁止使用裸 int 返回值
3. WAL-first 模式：任何数据修改先写 WAL 再写数据页
4. 所有全局状态必须有 mutex 保护
5. 编译标准: C14, C++14 (测试), GCC/G++ 手动编译
6. 每个模态必须实现 `wal_redo` / `wal_undo` 回调
7. 测试使用 GTest 框架，桩函数解决深层依赖

---

## 审查发现摘要

### 基础设施（12 组件）

| 组件 | 状态 | 核心问题 |
|------|------|---------|
| page | ⚠️ 弱 | XOR 校验、无磁盘 I/O、page_get_checksum 返回 0 |
| bufmgr | ❌ 空操作 | buf_read/buf_write 不执行真实 IO |
| catalog | ⚠️ 内存 | 无持久化、O(n) 查找、内存泄漏 |
| wal | ⚠️ 不完整 | redo 只有 INSERT/UPDATE、undo 正向扫描、LSN 固定偏移 |
| txn | ❌ 无隔离 | 无 MVCC、dirty read、无锁集成 |
| lock_mgr | ⚠️ 低效 | busy-wait Sleep(1)、release_all 迭代修改 |
| access | ⚠️ 危险 | heap_getnext 递归（栈溢出）、无可见性检查 |
| mm | ⚠️ 泄漏 | arena free 是 no-op、slab double-free O(n) |
| integrity | ❌ 全 stub | 所有检查返回 OK |
| log | ⚠️ 不完整 | aggregate 返回常数、索引不持久 |
| mmview | ❌ stub | refresh 不执行查询、cycle 检测 stub |
| multimodal | ❌ stub | cross_modal_search 返回零 |

### 存储模态（15 模态）

| 模态 | 状态 | 核心问题 |
|------|------|---------|
| kv | ⚠️ | LSM flush/compaction 未实现、链表 O(n) |
| spatial | ❌ 数据破坏 | update/delete 破坏数据、R-Tree split 未激活 |
| yang/tree | ⚠️ | ancestors/descendants NULL crash |
| ts | ❌ 缺失 | partition.c 缺失、tag_index 无过滤 |
| doc | ❌ 空函数 | doc_engine_get 是空 stub、索引不加载 |
| vector | ⚠️ | 选择排序 O(nk)、brute-force O(nd) |
| graph | ❌ | CSR 反向索引未构建、compaction 错误 |
| rel | ⚠️ | relation_create stub、无 crash recovery |
| blob | ⚠️ | 代码重复、multipart session 泄漏 |
| columnar | ⚠️ | 内存泄漏、指针序列化、无压缩 |
| rdf | ❌ | 索引从未填充（死代码）、system("rm") |
| sparse | ⚠️ | 全局状态、O(n) term 查找 |
| stream | ⚠️ | 指针序列化、consumer 绑定 partition 0 |
| cf | ⚠️ | 无 SSTable 实现、stats O(n) |
| st | ❌ | 无索引、全表扫描、kNN 错误 |

---

## Phase 0: 基础设施修复

### 依赖图

```
lock_mgr ──┐
            ├──→ txn (MVCC) ──→ catalog (持久化)
page ──→ bufmgr ──┘                      ↑
                              wal ───────┘
```

### T0.1 Page 层修复

**文件：** `src/db/storage/page/page.c`, `src/db/storage/page/disk.c`

**修改：**
- `page_get_checksum`: XOR → CRC32
- `page_is_corrupted`: 真实校验 magic + checksum
- 新增 `page_read(int fd, uint32_t page_id, page_t *page)`
- 新增 `page_write(int fd, uint32_t page_id, const page_t *page)`
- 新增 `page_compute_checksum(const page_t *page)`

**测试：** `tests/test_page.c` — 校验和计算、读写往返、损坏检测

### T0.2 Buffer Pool 修复

**文件：** `src/db/storage/buffer/bufmgr.c`, 删除 `src/db/storage/buffer/buffer.c`

**修改：**
- `buf_read`: 调用 `page_read` 加载真实数据
- `buf_write`: 调用 `page_write` 刷脏页
- 新增 `buf_flush_page(buffer_desc *buf)` 单页刷盘
- 移除重复的 `buffer.c`

**依赖：** T0.1

**测试：** `tests/test_bufmgr.c` — 读写往返、LRU 淘汰、脏页刷盘

### T0.3 WAL 完善

**文件：** `src/db/storage/wal/wal.c`, `src/db/storage/wal/wal_recover.c`

**修改：**
- `wal_redo`: 增加 DELETE/COMMIT/ABORT/CHECKPOINT 回放
- `wal_undo`: 逆序回滚替代正向扫描
- `wal_analyze`: 填充 active_txns 数组
- LSN offset: 固定 1024 → `sizeof(wal_record_t) + data_len`

**新增 API：**
```c
int wal_redo_delete(wal_t *wal, const wal_record_t *rec);
int wal_redo_commit(wal_t *wal, const wal_record_t *rec);
int wal_redo_checkpoint(wal_t *wal, const wal_record_t *rec);
int wal_undo_reverse(wal_t *wal, uint64_t from_lsn, uint64_t to_lsn);
```

**测试：** `tests/test_wal.c` — redo 全类型、undo 逆序、LSN 正确性

### T0.4 Catalog 持久化

**文件：** `src/db/storage/catalog/catalog.c`

**修改：**
- `catalog_add_column`: 修复内存泄漏（free 旧 columns）
- `catalog_lookup_table`: 链表 → hash table O(1)
- 新增 `catalog_persist(catalog_t *cat, wal_t *wal)`
- 新增 `catalog_recover(catalog_t *cat, wal_t *wal)`

**依赖：** T0.3

**测试：** `tests/test_catalog.c` — CRUD、持久化往返、重启恢复

### T0.5 MVCC 事务

**文件：** `src/db/storage/txn/txn.c`, `src/db/storage/txn/mvcc.c`

**修改：**
- `txn_begin`: 记录快照 LSN
- `txn_get`: 可见性检查替代 dirty read
- 与 lock_mgr 集成

**新增 API：**
```c
int txn_begin_mvcc(txn_t *txn, isolation_level_t level);
int txn_visibility_check(txn_t *txn, const tuple_header *tuple);
int txn_commit_mvcc(txn_t *txn);
```

**依赖：** T0.1, T0.3

**测试：** `tests/test_txn.c` — 快照隔离、可见性、提交回滚

### T0.6 Lock Manager 修复

**文件：** `src/db/storage/lock/lock_mgr.c`

**修改：**
- `lock_acquire`: busy-wait → pthread_cond_t / ConditionVariable
- `lock_release_all`: 先收集再释放（修复 use-after-free）
- `wake_up_next_waiter`: 真实唤醒
- 锁升级/降级: stub → 真实实现

**依赖：** 无

**测试：** `tests/test_lock.c` — 并发加锁、死锁检测、升级降级

---

## Phase 1: 数据破坏性 Bug 修复

### 依赖图

4 个修复互不依赖，可并行。

### T1.1 Spatial 数据破坏修复

**文件：** `src/db/storage/spatial/spatial_engine.c`, `src/db/storage/spatial/rtree.c`

**修改：**
- `spatial_tuple_update`: WAL 先写旧值 → 更新 R-Tree 索引 → 写新值
- `spatial_tuple_delete`: WAL 先写旧值 → 从 R-Tree 移除 → 标记删除
- `rtree_insert`: 检查节点满时调用 `rtree_split_root`

**测试：** `tests/test_spatial.c` — update 后数据完整、delete 后查询不到、split 触发

### T1.2 Graph CSR 修复

**文件：** `src/db/storage/graph/graph_csr.c`

**修改：**
- compact 后调用 `graph_csr_build_reverse_index`
- compact 重新分配连续内存（修复指针失效）
- scan 实现（当前未实现）

**测试：** `tests/test_graph.c` — compact 后反向查询、scan 遍历

### T1.3 Yang NULL handle 修复

**文件：** `src/db/storage/yang/yang_tree.c`

**修改：**
- `yang_sql_ancestors/descendants`: 检查 `tree` 和 `tree->root` 非 NULL

**测试：** `tests/test_tree_yang.c` — NULL handle 不 crash

### T1.4 Timeseries partition + tag_index

**文件：** 新增 `src/db/storage/ts/ts_partition.c`, 修改 `src/db/storage/ts/ts_tag_index.c`

**修改：**
- `ts_tag_index_query`: 用 hash 查找匹配 series（当前返回全部）
- `ts_partition_create/insert/query`: 新增 partition 实现

**测试：** `tests/test_timeseries.c` — tag 过滤正确、partition 查询

---

## Phase 2: 模态核心功能补完

### 2A 组：存储引擎补完

**T2A.1 KV — LSM flush + skip list**

**文件：** `src/db/storage/kv/lsm/lsm_tree.c`, `src/db/storage/kv/kv_ordered.c`

**修改：**
- `lsm_flush`: memtable → SSTable flush 实现
- `lsm_compact`: SSTable 合并
- `kv_ordered`: linked list → skip list

**测试：** `tests/test_kv.c` — flush 后数据可读、compaction 正确、skip list O(log n)

---

**T2A.2 Columnar — 持久化 + 压缩 + chunk 扩展**

**文件：** `src/db/storage/columnar/columnar_engine.c`

**修改：**
- `columnar_table_create`: 指针序列化 → 偏移量序列化
- `columnar_table_open`: 从偏移量反序列化重建指针
- `columnar_column_append`: 容量满时创建新 chunk
- `columnar_compress`: LZ4/ZSTD/Delta 真实实现
- `destroy_chunk`: 修复双重 free

**测试：** `tests/test_columnar.c` — 创建/打开/追加/压缩往返

---

**T2A.3 Blob — 代码去重 + multipart 修复**

**文件：** `src/db/storage/blob/blob_engine.c`, `src/db/storage/blob/blob_gc.c`, `src/db/storage/blob/blob_multipart.c`

**修改：**
- 提取 engine 和 gc 共用的 chunk 操作为公共函数
- `blob_multipart_abort`: 释放 session（修复泄漏）
- hash 表扩容：固定 1024 → 动态 resize

**测试：** `tests/test_blob.c` — put/get/abort 无泄漏

---

### 2B 组：索引/查询补完

**T2B.1 Document — doc_engine_get + inverted index load**

**文件：** `src/db/storage/doc/doc_engine.c`, `src/db/storage/doc/doc_inverted.c`

**修改：**
- `doc_engine_get`: 空函数 → 真实实现
- `doc_inverted_load`: 从文件加载到内存
- BM25 scoring: 使用真实 TF-IDF 计算

**测试：** `tests/test_document.c` — get 返回正确文档、索引持久化

---

**T2B.2 Vector — top-k heap + persist**

**文件：** `src/db/storage/vector/vector_engine.c`, `src/db/storage/vector/vector_index_persist.c`

**修改：**
- 选择排序 → min-heap O(n log k)
- DiskANN/IVF 持久化实现

**测试：** `tests/test_vector.c` — 搜索结果正确、持久化往返

---

**T2B.3 RDF — 索引激活 + 安全操作**

**文件：** `src/db/storage/rdf/rdf_engine.c`, `src/db/storage/rdf/rdf_index.c`

**修改：**
- insert 路径调用 `rdf_index_add_triple`（激活死代码）
- `system("rm -rf")` → 平台无关递归删除
- hash_term: snprintf %d → 正确类型格式

**测试：** `tests/test_rdf.c` — 插入后索引可查、drop 安全

---

**T2B.4 Relational — relation_create + crash recovery**

**文件：** `src/db/storage/rel/rel_engine.c`

**修改：**
- `relation_create`: stub → 真实实现（WAL + catalog + 文件）
- `relation_open`: 从 WAL 恢复 row_id
- heap_getnext: 递归 → 迭代

**测试：** `tests/test_relational.c` — create/open/close 往返、crash recovery

---

**T2B.5 Log — aggregate 真实计算**

**文件：** `src/db/storage/log/log_aggregate.c`, `src/db/storage/log/log_engine.c`

**修改：**
- `log_rate`: 真实实现（时间窗口计数）
- `log_aggregate_sum/max/min`: 解析日志行中的数值
- `log_query`: 使用 selector 参数过滤

**测试：** `tests/test_log.c` — 聚合结果正确

---

### 2C 组：持久化修复

**T2C.1 Stream — 持久化修复**

**文件：** `src/db/storage/stream/stream_engine.c`

**修改：**
- `stream_open`: 从偏移量重建链表指针
- `stream_consume`: offset 索引替代线性扫描
- consumer 绑定正确 partition

**测试：** `tests/test_stream.c` — open 后数据完整、consume 正确分区

---

**T2C.2 CF — SSTable 或移除声明**

**文件：** `src/db/cf/cf_engine.c`

**修改：**
- 选项 A: 实现简单 SSTable（LSM 风格）
- 选项 B: 从 header 移除 SSTable 声明，记录为 future work
- `cf_family_stats`: 增量统计替代全扫描
- `cf_iter_next`: 指针偏移替代全行重载

**测试：** `tests/test_cf.c` — CRUD 正确、stats 正确

---

**T2C.3 ST — R-Tree 索引集成**

**文件：** `src/db/storage/st/st_engine.c`

**修改：**
- 使用已有的 `rtree.h` 构建空间索引
- `st_engine_nearest_time`: 修复 kNN（取全局 top-k 而非前 k 个）
- `system("rm -rf")` → 平台无关删除

**测试：** `tests/test_st.c` — 查询使用索引、kNN 结果正确

---

**T2C.4 Sparse — 线程安全 + 效率**

**文件：** `src/db/storage/sparse/bm25_index.c`

**修改：**
- 全局状态 → 结构体封装 + mutex
- `bm25_find_term`: 线性扫描 → hash map
- token 数组: 栈 1024 → 动态分配

**测试：** `tests/test_sparse.c` — 多线程安全、查找正确

---

**T2C.5 MMView — refresh 实现**

**文件：** `src/db/storage/mmview/mview.c`

**修改：**
- `mview_refresh_complete`: 执行查询并写入数据
- `mview_has_cycle`: DFS 拓扑排序检测环
- `mview_can_rewrite`: 查询匹配逻辑
- 编译错误修复: `stats.refreshing_mviews` → `g_stats.refreshing_mviews`

**测试：** `tests/test_mmview.c` — refresh 执行、cycle 检测

---

**T2C.6 Multimodal — 联合搜索**

**文件：** `src/db/storage/multimodal/cross_modal.c`, `src/db/storage/multimodal/multimodal_search_v2.c`

**修改：**
- `cross_modal_search`: 调用实际 vector/graph/doc 引擎
- RRF score 真实计算
- 集成 named vector indexes

**测试：** `tests/test_multimodal.c` — 跨模态搜索返回非零结果

---

**T2C.7 Integrity — 真实校验**

**文件：** `src/db/storage/integrity/data_integrity.c`

**修改：**
- 所有引擎检查: stub → 真实校验
- `page_verify_and_repair`: 真实检查 + 修复
- WAL 校验: 真实实现

**测试：** `tests/test_integrity.c` — 损坏检测、修复

---

## Phase 3: 性能优化

### T3.1 O(n) → O(log n) 算法替换

| 任务 | 文件 | 替换 |
|------|------|------|
| T3.1a | kv_ordered.c | 链表 → skip list |
| T3.1b | bm25_index.c | 线性扫描 → hash map |
| T3.1c | sparse hybrid_retrieval.c | 选择排序 → min-heap |
| T3.1d | st_engine.c | 全表扫描 → R-Tree + heap |
| T3.1e | cf_engine.c | stats 全扫描 → 增量缓存 |
| T3.1f | catalog.c | 链表 → hash table |

### T3.2 线程安全

| 任务 | 文件 | 修复 |
|------|------|------|
| T3.2a | bm25_index.c | 全局状态 → 结构体 + mutex |
| T3.2b | graph_csr.c | use_lock → 真实 rwlock |
| T3.2c | kv_engine.c | g_engine 单例 → per-instance |
| T3.2d | rel_engine.c | g_active_txns → per-instance |

### T3.3 压缩启用

| 任务 | 文件 | 方案 |
|------|------|------|
| T3.3a | columnar_engine.c | LZ4/ZSTD/Delta |
| T3.3b | ts_compress.c | Gorilla 编码激活 |
| T3.3c | doc_inverted.c | posting list 压缩 |
| T3.3d | blob_engine.c | 透明压缩 |

---

## Phase 4: 新增特性

### T4.1 跨模态事务

**文件：** `src/db/storage/txn/txn.c`, `src/db/storage/multimodal/cross_modal.c`

**实现：**
- 两阶段提交协议
- 跨模态 WAL 记录（关联多个模态的修改）
- 死锁检测扩展到跨模态

### T4.2 物化视图完整实现

**文件：** `src/db/storage/mmview/mview.c`

**实现：**
- `mview_refresh_fast`: 增量刷新（只处理新增数据）
- `mview_schedule`: 基于时间的自动刷新
- `mview_invalidate`: CDC 事件驱动失效

### T4.3 RDF SPARQL 增强

**文件：** `src/db/storage/rdf/sparql_parser.c`

**实现：**
- FILTER 表达式支持
- OPTIONAL 模式匹配
- GROUP BY + 聚合
- PREFIX 应用到 URI

### T4.4 分布式协调基础

**文件：** 新增 `src/db/storage/multimodal/distributed_coord.c`

**实现：**
- 跨节点一致性哈希
- 分布式事务协调器
- 复制日志同步

---

## 任务总览

| Phase | 任务数 | 依赖 |
|-------|--------|------|
| Phase 0 | 6 | 锁定序 |
| Phase 1 | 4 | 并行 |
| Phase 2 | 15 | 部分依赖 Phase 0 |
| Phase 3 | 10 | 依赖 Phase 2 |
| Phase 4 | 4 | 依赖 Phase 3 |
| **总计** | **39** | |

## 测试文件清单

| 测试文件 | 覆盖模态 |
|---------|---------|
| tests/test_page.c | Page 层 |
| tests/test_bufmgr.c | Buffer Pool |
| tests/test_wal.c | WAL |
| tests/test_catalog.c | Catalog |
| tests/test_txn.c | Transaction |
| tests/test_lock.c | Lock Manager |
| tests/test_spatial.c | Spatial |
| tests/test_graph.c | Graph |
| tests/test_tree_yang.c | Yang/Tree |
| tests/test_timeseries.c | Timeseries |
| tests/test_kv.c | KV |
| tests/test_columnar.c | Columnar |
| tests/test_blob.c | Blob |
| tests/test_document.c | Document |
| tests/test_vector.c | Vector |
| tests/test_rdf.c | RDF |
| tests/test_relational.c | Relational |
| tests/test_log.c | Log |
| tests/test_stream.c | Stream |
| tests/test_cf.c | Column Family |
| tests/test_st.c | Spatio-Temporal |
| tests/test_sparse.c | Sparse |
| tests/test_mmview.c | Materialized View |
| tests/test_multimodal.c | Multimodal |
| tests/test_integrity.c | Integrity |

## 编译说明

所有测试使用 gcc/g++ 手动编译（CMake 有预存构建错误）。

```bash
# 编译 C 源文件
gcc -c src/db/storage/<modality>/<source>.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -o <output>.o

# 编译 C++ 测试文件
g++ -std=c++14 -c tests/test_<modality>.c -ID:/code/book/engineering/include -ID:/code/book/engineering/include/db -ID:/code/book/engineering/third_part/googletest/googletest/include -o test_<modality>.o

# 链接
g++ -std=c++14 -o test_<modality>.exe test_<modality>.o <source>.o gtest-all.o gmock-all.o -lpthread
```
