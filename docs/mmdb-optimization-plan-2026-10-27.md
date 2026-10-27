# 多模态数据库引擎 · 优化分析与执行计划

> 评审对象：`StillEvolving/projects/engine`（约 9.4 万行 C，537 个源/头文件，305 个测试文件）
> 评审日期：2026-10-27
> 评审方式：只读代码审查 + 交叉复核。**每条结论均附可复现的证据**；未核实的条目已显式标注。
> 基线构建：`cmake -B build/baseline -S . -G Ninja -DBUILD_TESTING=ON` 配置成功（22.3s，无错误）。

---

## 零、方法与可信度说明

本轮评审由架构评审与质量评审两路并行完成，主理人对所有 P0 结论做了独立复核。

**必须记录的纠错**：架构评审最初报告"模态开关宏名错配导致 27.5K 行 SQL 层永不生效"（P0）。
复核后**该结论不成立**，原因如下：

- `cmake/MultimodalConfig.cmake:43` 的 `configure_file` 输入是 `${CMAKE_CURRENT_LIST_DIR}/multimodal_config.h.in`，
  即 **`cmake/multimodal_config.h.in`**；
- 该文件第 5–12 行正确产出 `MMDB_ENABLE_RELATIONAL / _TIMESERIES / _DOCUMENT / _TREE`，与 CMake option 完全对应；
- `include/db/multimodal_config.h.in` 是**全仓零引用的陈旧孤儿模板**（`grep -rn "include/db/multimodal_config.h.in"` 无命中），
  其 `_REL/_YANG/_TS/_DOC` 宏名与 `MMDB_MODEL_COUNT` 均不参与任何构建。

**教训**：判断"某宏是否被定义"必须确认 `configure_file` 的输入路径，不能凭文件名推断。
该孤儿模板已作为 P2-18 列入待删清单——它已经实际误导过一次评审。

**另一处需修正的表述**：`mm_cross_modal_search` 曾被描述为"导出 API（`multimodal_object.h`）"。
实测 `grep -rn "mm_cross_modal_search" include/` **零命中**，它没有任何头文件声明，
仅以外部链接符号形式存在于 `cross_modal.c`。这不改变"应删除"的结论，但改变了风险评估：删除不会破坏任何已声明的接口。

---

## 一、P0 —— 正在造成数据错误或静默失效

### P0-1　checkpoint 先截断 WAL、后写快照 → 已确认的写入永久丢失

**现象**
`vector_api_save()` 的落盘顺序是：`flush → checkpoint → truncate → save_collections_meta`。
`truncate` 在快照写入之前执行。

**根因（两层）**

1. **手动 save 路径顺序错误**。`src/db/api/vector_api.c:1003-1033`：第 3 步 `vector_wal_truncate` 丢弃
   checkpoint 之前的所有 WAL 记录，第 4 步才 `save_collections_meta` 写 `collections.meta` 快照。
   若第 4 步失败（磁盘满、`fopen` 失败、进程被杀），WAL 已被截断、快照是旧的 → 数据不可恢复。
2. **后台 CheckpointManager 根本不具备持久化能力**。`src/db/storage/vector/checkpoint_manager.c:27-38`
   的结构体只持有 `vector_wal_t *wal`，**没有集合数据句柄**；`:76-88` 却每周期无条件执行
   `flush → checkpoint → truncate`。它在物理上无法把向量落盘，却每周期销毁唯一副本。

**证据**
- `src/db/api/vector_api.c:1017-1029`：checkpoint 与 truncate 在 `save_collections_meta` 之前。
- `src/db/storage/vector/vector_wal.c:446-493`：`vector_wal_checkpoint` 只把 offset 写回文件头，**不落任何索引或向量**。
- `src/db/storage/vector/checkpoint_manager.c:77-87`：只有 `mgr->wal`，无快照动作。
- `src/db/api/vector_api.c:938-947` 注释自述："WAL replay 是 source of truth，metadata 只是注册"。
  → 既然 WAL 是唯一真相，截断它而不落快照，等价于删除数据。

**影响**：进程崩溃或 save 失败即丢失自上次成功 save 以来的全部插入。**现有测试未覆盖**——
`chaos_e2e_test.cpp:524` 的 saver 线程是"每 200ms 调 `vector_api_save`"，恰好用高频 save 掩盖了这个窗口。

**修法**
1. `vector_api_save` 改为：`flush → checkpoint → save_collections_meta（校验返回值）→ 成功后才 truncate`。
2. `CheckpointManager` 增加持久化回调 `checkpoint_manager_set_persist_callback(mgr, fn, ctx)`；
   线程顺序改为 `flush → persist → checkpoint → truncate`，**persist 未设置或失败时禁止 truncate**。

**代价**：约 60 行（2 个文件 + 头文件）。**风险**：中，需补崩溃恢复测试。

---

### P0-2　`mm_cross_modal_search` 返回伪造数据

**现象**
`src/db/storage/multimodal/cross_modal.c:395-399`：

```c
for (int i = 0; i < search_k; i++) {
    /* Placeholder: generate synthetic results */
    ids[i] = (int32_t)i;
    dists[i] = (float)(search_k - i);
}
```

函数全程不调用任何索引，返回的是 `0,1,2,...,k-1` 的合成 ID 序列，且 RRF 分数由合成距离推导。
注释自认 "For demonstration"、"In real implementation would call..."。

**证据**
- `cross_modal.c:368-381`：注释明确写 "In a full implementation, we would look up the appropriate collection"、
  "Here we demonstrate the RRF computation pattern"。
- `grep -rn "mm_cross_modal_search" include/ src/ test/` → 除定义处外**零命中**（无声明、无调用者）。
- 实际做的是**单模态**检索（`strncmp` 只匹配一个 named vector，`cross_modal.c:337-343`），不是跨模态。

**影响**：一个名为 `cross_modal_search` 的外部链接符号留在库里，任何后续接入者都会拿到"看起来合理"的
假 ID 序列——比崩溃更难排查。这是"假正确"型缺陷。

**修法**：删除 `cross_modal.c:320-446` 整个函数（-127 行）。
**代价**：纯删除。**风险**：零（无声明、无调用者）。

---

### P0-3　跨模态融合：无快照隔离 + 裸 id 去重 + metadata filter 空操作

**现象**（三个独立缺陷叠在同一函数）

1. **无快照**：`multimodal_search_v2.c:57-87` 对每个 named vector 串行调用 `faiss_hnsw_collection_search`，
   各 space 之间无事务、无快照，读到的是不同时刻的数据。
2. **裸 id 去重导致跨集合撞号**：`:18` 注释自述 "id 简化为 segment offset + local idx"；
   `:74-76` 按 `cands[j].id == id` 去重累加 RRF。两个不同 collection 的局部 id=0 会被合并成同一候选，
   RRF 分数被错误叠加。
3. **filter 空操作**：`:90-99` 的 haystack 是 `query->metadata`（**查询对象自己的**元数据），
   不是候选对象的。过滤器与候选完全无关，结果恒真或恒假。

**证据**
- `multimodal_search_v2.c:93`：`memmem(query->metadata, query->metadata_len, filter_json, ...)`。
- `mm_hooks.c:18-26`：**正确的** `mm_multimodal_metadata_match(obj, filter)` 早已存在，v2 却没有调用它。
- `test/db/storage/c5_vector_multimodal_test.cpp:53`：唯一调用者传 `n_spaces=1` + `filter_json=nullptr`，
  恰好同时绕过缺陷 2 和缺陷 3 —— 用例形状与实现缺陷互补，是"照着实现写的测试"。

**影响**：跨集合检索结果错乱（静默错误，不报错）；filter 参数形同虚设。
**现有测试无法发现**。

**修法**
- 候选 key 改为 `(collection_idx, local_id)` 复合键；
- filter 改为对每个候选调用 `mm_multimodal_metadata_match`（需先补 candidate→object 映射，属 API 缺口）；
- 补齐 2 个负向用例：多集合同局部 id 不应合并；非空 filter 应真正过滤。

**代价**：约 40 行重写 + 2 个测试。**风险**：中，改变 v2 语义，须同步更新测试。
**本次不执行**（需先补 candidate→object 映射的接口决策），列入后续计划。

---

### P0-4　引擎注册静默覆盖，ST / RDF 复用了别人的 enum 槽位

**现象**
`src/db/storage/mm/mm_storage.c:183-189` 的 `storage_register_engine` 直接 `g_engines[model] = ops;`，
**无重复检查**，后注册者静默覆盖先注册者。
而 `src/db/core/engine_registry.c:172` 把时空引擎注册到 `MODEL_SPATIAL`、`:183` 把 RDF 注册到 `MODEL_GRAPH`。

**证据**
- `include/db/storage_engine.h:37-38` 已定义 `MODEL_RDF = 11`、`MODEL_SPATIOTEMPORAL = 12`，**全仓无人使用**。
- `engine_registry.c:172`、`:183` 分别传入 `MODEL_SPATIAL`、`MODEL_GRAPH`。

**影响**：同时开启 `MMDB_ENABLE_SPATIAL=ON` 与 `MMDB_ENABLE_ST=ON` 时，时空引擎静默顶掉空间引擎，
空间查询走错引擎且无任何报错。当前两开关默认 OFF，属潜伏雷。

**修法**：`:172` → `MODEL_SPATIOTEMPORAL`，`:183` → `MODEL_RDF`；`storage_register_engine` 增加
`if (g_engines[model] != NULL) return -1;`。
**代价**：4 行。**风险**：极低。

---

### P0-5　`storage_model_name` 数组越界与错位

**现象**
`mm_storage.c:222-231`：

```c
static const char *names[MODEL_COUNT] = {          /* MODEL_COUNT == 13 */
    "relational","kv","graph","vector","timeseries","document",
    "spatial","tree","stream","columnar","rdf","spatiotemporal"
};                                                  /* 仅 12 项 */
```

`MODEL_COUNT = 13`，数组只有 12 个初始化器 → `names[12] == NULL`。

**证据**
- `include/db/storage_engine.h:25-40`：枚举 `MODEL_COLUMNAR = 9` 之后直接跳到 `MODEL_RDF = 11`，
  10 号槽位为空；`MODEL_SPATIOTEMPORAL = 12`，`MODEL_COUNT = 13`。
- 后果 A：`storage_model_name(MODEL_RDF /*11*/)` 返回 `"spatiotemporal"`（错位）。
- 后果 B：`storage_model_name(MODEL_SPATIOTEMPORAL /*12*/)` 返回 `NULL`，调用方 `printf("%s")` 即 UB。

**修法**：改用指定初始化器 `[MODEL_KV] = "kv", ...`，彻底消除对位置顺序的依赖。
**代价**：1 个函数。**风险**：无。

---

### P0-6　读写锁契约与实现直接矛盾

**现象**
`include/db/common_rwlock.h:6` 文档写"支持读锁（**可重入**）和写锁（独占）"；
`src/db/common_rwlock.c:14` 在 Linux 上设置 `PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP`
—— **明确非递归**。

**影响**：按头文件契约编写重入读锁的代码会自死锁（同一线程二次 `rdlock` 在 non-recursive 下未定义/阻塞）。
**修法**：统一为"不可重入"，修正头文件注释并写明契约（不要试图改成可重入——写者优先 + 可重入在 POSIX 下无法同时满足）。
**代价**：注释 3 行。**风险**：无。

---

### P0-7　跨模态事务管理器懒初始化存在竞态

**现象**
`cross_modal.c:63-72`：

```c
static int cross_txn_manager_init(void) {
    if (g_cross_mgr_initialized) return 0;        /* 无锁读 */
    memset(&g_cross_mgr, 0, sizeof(g_cross_mgr)); /* 含 mutex！ */
    pthread_mutex_init(&g_cross_mgr.mutex, NULL);
    ...
}
```

`memset` 会把整个 manager（包括 `pthread_mutex_t`）清零。两个线程并发首调时，
后进入者会清零前者已经 `lock` 的 mutex → 未定义行为。

**影响**：并发首次使用时可能死锁或破坏互斥语义。当前 2PC 无生产调用者，属潜伏缺陷。

**修法**：改用 `pthread_once`。
**代价**：约 10 行。**风险**：低。

**附带结论**：`cross_txn_prepare` 对任何 participant 无条件返回成功，`cross_modal.c:177-183` 注释自认只写一条
汇总 WAL 记录，**无 per-modality 投票、无加锁、无 in-doubt 恢复**。即 2PC 整体是空壳，
不应计入任何"跨模态事务可用"的结论。唯一测试 `test/db/storage/legacy_tests/test_cross_txn.c`
**从未被任何 CMakeLists 引用**（见 P2-13）。

---

## 二、P1 —— 显著性能与维护负担

### P1-8　向量检索默认路径是全场最差的一份

**现象**：`vector_engine.c:733` 的 `vector_engine_search`：

- 不判断 `index_built`，永远走暴力全扫，**40+ 个索引实现全部绕过**；
- `:867` 对**每个向量** `malloc(dim*sizeof(float))` 并 `memcpy` 一份；
- `:801` 用**选择排序**取 top-k，复杂度 O(n·k)；
- `:823-825` 只 `free` 了未入选的向量，**入选 top-k 的向量永不释放** → 每次查询泄漏 `min(top_k, n) × dim × 4` 字节。

**量级**：1M 向量 × 768 维 → 每查询约 3 GB 临时分配 + 100 万次 `malloc`。

**讽刺点**：同文件 `:139-184` 的 `vector_search()` 用正确的最小堆做 O(n·log k) top-k，
但 `grep "vector_search("` **只命中定义**——好实现是死代码，坏实现在线上。
且该死代码 `:145` 的 `float vec_buf[256]` 在 `dimension > 256`（常见 768/1536）时栈溢出，
一旦有人"复用"它会立即踩雷。

**修法**：`vector_engine_search` 开头加索引分派；无索引时改用堆 top-k + 单块复用缓冲，
删除逐向量 `malloc` 与泄漏。
**代价**：约 120 行。**风险**：中，需回归 `test/db/storage/` 向量用例。

### P1-9　PQ 编码逐次 `realloc` 整个数组
`vector_engine.c:561-570` 在 `tuple_insert` 内按 `(n+1)*code_size` 逐次 `realloc`。
1M 向量 × 64B → 最坏每次插入拷贝 64 MB，整体 O(n²)。
**修法**：2 倍容量增长 + `compressed_capacity` 字段。**代价**：约 20 行。

### P1-10　融合逻辑 9 份实现、3 套类型、3 种公式
`multimodal_search_v2.c:77`、`cross_modal.c:414`、`cross_modal.c:455`（`mm_rrf_score`，`rank=0,k=0` 时除零）、
`core/query_fusion.c:255/274/294`（加权 RRF）与 `:403/424/442`（**归一化分数融合，不是 RRF，却写入同名字段 `rrf_score`**）、
`executor/rag/graphrag_hybrid_search.c:44`、`executor/rag/retrieve_op.c:338`、
`sdk/extra/hybrid_search.c`、`storage/sparse/hybrid_retrieval.c:158`（alpha/beta/gamma 加权线性，明确不是 RRF）。
**修法**：以 `query_fusion.c` 为唯一实现，抽出 `fusion_rrf` / `fusion_weighted_score` 两个纯函数，其余改为调用。
**风险**：**会改变现有排序结果**，必须逐路径补基准后再切。**本次不执行**。

### P1-11　重复文件：92 组重名 basename、19 组字节相同、`faiss_hnsw.h` 两条血缘
`include/db/index/vector_index/faiss_hnsw/faiss_hnsw.h`（155 行，不透明类型）
vs `include/db/index/vector_index/hnsw/faiss_hnsw.h`（194 行，展开完整结构体）——
同一类型两种定义，19 个文件 include 后者、8 个 include 前者，同一 TU 同时包含即 redefinition。
另有 `vector_index_persist.c`（455 行 vs 330 行）、`graph_index.c`（679 vs 481）、`lock_mgr.c`（930 vs 768）同名不同体。
**建议**：先删 19 组字节相同的副本（零风险）；两条 `faiss_hnsw` 血缘的裁决需业务判断，**延后**。

### P1-12　构建：64 处 `GLOB_RECURSE` + 指向自己产物的 `IMPORTED` 兜底
`src/db/storage/CMakeLists.txt:250-251` 在无源文件分支里
`add_library(db_storage STATIC IMPORTED)` + `IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/src/db/libdb_storage.a`
—— 指向本应由自己生成的文件，会静默取上一次构建的陈旧 `.a`。
**修法**：该分支改为 `message(FATAL_ERROR ...)`。**代价**：2 行。**风险**：会让某些裁剪配置从"能链接"变"报错"，这是正确的。

---

## 三、P2 —— 整洁度与工程卫生

| 编号 | 问题 | 证据 | 建议 |
|---|---|---|---|
| P2-13 | 测试写了不跑 | 16 处 `# add_executable` 被注释；`test/db/storage/legacy_tests/` 10 个文件 `grep legacy_tests --include=CMakeLists.txt` 零命中 | 逐个评估：能修的恢复，不能修的删除 |
| P2-14 | memctx 失败是测试 UAF 误报 | `memctx.c:462` 先 `free(set)`；测试在 `mmdb_close()` 后读 `ctx->is_deleted`（`memory_leak_detection_test.cpp:53,92` 等）；产品侧 `memctx.c:473-479` 已加 `g_memctx_delete_generation` 但测试零使用 | 测试改用世代计数器比对 |
| P2-15 | 覆盖最薄 | graph 20%、text 19%（src/test 行数比） | 补核心路径用例 |
| P2-16 | `memmem` 不可移植 | `mm_hooks.c:23`、`multimodal_search_v2.c:93` 使用 GNU `memmem`，全仓无 `_GNU_SOURCE` | 抽 `db_memmem()` 带非 GNU 兜底 |
| P2-17 | `pthread_mutex_t` 三处各自重定义 | `mm_pool.c:16`、`sql/materialized_view.c`、`sql/tuple_queue.c` 各自 `typedef CRITICAL_SECTION pthread_mutex_t` | 收敛到单一 `platform_compat.h` |
| P2-18 | 孤儿配置模板 | `include/db/multimodal_config.h.in` 全仓零引用，宏名与生效模板不一致 | **删除** |
| P2-19 | 中文标识符 | `include/db/sql/memctx.h:58` `ALLOCSET_PRESET_SMALL高频` | 改名 |
| P2-20 | 上帝文件与技术债 | `vector_engine.c` 2559 行 / 85 函数；308 处 TODO/FIXME | 待接口定型后按职责拆分 |

---

## 四、本次执行范围

按"投入产出比"排序，本轮只做**证据确凿、可被构建与测试验证**的项：

| 任务 | 内容 | 预期改动 | 验证方式 |
|---|---|---|---|
| #1 | 删 `mm_cross_modal_search` 伪造实现 + 删孤儿模板 | -143 行 | 构建通过 + grep 无残留 |
| #2 | 注册槽位改指 + 注册去重 + `storage_model_name` 指定初始化器 | ~20 行 | 新增单元测试 |
| #3 | `cross_modal` 改 `pthread_once` + rwlock 契约对齐 | ~15 行 | 构建 + 注释一致性 |
| #4 | checkpoint 先快照后截断 + 后台回调门控 | ~60 行 | 崩溃恢复测试 |
| #5 | `vector_engine_search` 索引分派 + 堆 top-k + 消泄漏 | ~120 行 | 向量召回测试 + 内存检查 |
| #6 | 全量构建 + 回归 | — | 与基线失败集合对比 |

**明确不在本轮**：P0-3（跨模态融合语义重写，需先定 candidate→object 映射接口）、
P1-10（融合收敛，会改变排序结果）、P1-11 血缘裁决、P2-20 拆分。
理由：这四项都需要接口层决策或业务语义裁决，在 `storage_ops_t` 定型前动手只会制造二次返工。

---

## 五、验证判据

沿用仓库既有判据（`projects/docs/build-debt-fixes.md` §8）：

```bash
# 配置
cmake -B build/baseline -S . -G Ninja -DBUILD_TESTING=ON
# 构建（并行先看，必要时串行 -j1 -k 0 复现完整失败集合）
ninja -C build/baseline -k 0
# 测试
ctest --test-dir build/baseline -j8 --timeout 30
```

**判据**：改动后失败集合不得超出改动前基线；新增的失败必须有明确归因。
注意该仓 `ctest` 用例为混合粒度（部分测试整个二进制只算 1 条），
**通过率数字不能单独作为质量证据**，须结合具体失败用例名判断。

---

## 六、执行结果（2026-10-27 已完成）

### 6.1 实际改动

```
 include/db/common_rwlock.h                  |   6 +-
 include/db/multimodal_config.h.in           |  16 --   (删除)
 include/db/storage/vector/checkpoint_manager.h |  6 +-
 src/db/api/vector_api.c                     |  44 +++-
 src/db/common_rwlock.c                      |   3 +-
 src/db/core/engine_registry.c               |   7 +-
 src/db/storage/mm/mm_storage.c              |  60 +++--
 src/db/storage/multimodal/cross_modal.c     | 178 ++-----------
 src/db/storage/vector/checkpoint_manager.c  |  11 +-
 src/db/storage/vector/vector_engine.c       | 289 +++++++--------------
 test/db/core/engine_registry_test.cpp       |  22 +-
 11 files changed, 229 insertions(+), 413 deletions(-)
```

净减 184 行。

### 6.2 逐项结果

| 任务 | 结果 | 验证 |
|---|---|---|
| #1 删伪造 API + 孤儿模板 | 完成，`cross_modal.c` -178 行；`mm_cross_modal_search`、`crossmodal_candidate_t`、`compare_candidates`、`MAX_CANDIDATES`、`RRF_K` 全部清除 | 全仓 grep 无残留；构建通过 |
| #2 注册槽位 + model_name | 完成。ST→`MODEL_SPATIOTEMPORAL`、RDF→`MODEL_RDF`；注册表增加重复注册拒绝（同指针幂等）；`g_model_dir_names` 与 `storage_model_name` 改指定初始化器（**顺带修掉目录名错位：RDF 原本会写进 spatiotemporal 目录**） | `engine_registry_test` 14/14 通过 |
| #3 cross_modal 竞态 + rwlock 契约 | 完成。懒初始化改 `pthread_once`，不再 `memset` 整个 manager；`cross_txn_begin` 检查初始化返回值；rwlock 头文件明确"不可重入" | 构建通过；注释与实现一致 |
| #4 checkpoint 丢数据 | 完成。`vector_api_save` 改为 `flush → 快照 → checkpoint → truncate`，快照失败则不截断；后台 CheckpointManager 改为只 `flush + checkpoint`，**不再 truncate** | 见 6.3 关于中途方案的说明 |
| #5 向量检索重构 | 完成。加索引分派；无索引时用堆 top-k + 单块复用缓冲；消除逐向量 malloc 与 top-k 泄漏；删除带 `vec_buf[256]` 栈溢出的死代码 `vector_search()` | 构建通过；**但无测试覆盖**，见 6.4 |
| #6 构建与回归 | 完成 | 见 6.3 |

### 6.3 回归对照（重要）

构建：`ninja -C build/baseline -k 0` 全新构建，165 个可执行文件，**0 失败**。

测试对照采用「同一构建树 + 清空 `test-results` + 同一 ctest 参数」，通过 `git stash` 回退源码得到基线：

| | 基线 | 改动后 |
|---|---|---|
| 执行用例 | 800 | 800 |
| 失败（ctest 计数） | 75 | 94 |
| 失败（唯一用例名） | 67 | 65 |
| 通过率 | 91% | 88% |

**计数差异的解释**：该仓同一用例名会被多个测试二进制重复注册，ctest 的"失败数"含重复项，因此不等于唯一失败数。
按唯一用例名比较，改动后失败集合**反而少 2 个**。集合差异全部落在两类：

1. **memctx 一族**（`MmdbRootContextTest` / `MmdbRequestScopeTest` / `SdkVectorsMemctxTest` / `SdkCoreMemctxTest` / `MemoryLeakDetectionTest`）—— 两个方向来回抖动（新增 9、消失 11）。该类经 QA 独立核实为**测试自身 UAF 误报**（`memctx.c:462` free 后测试仍读 `is_deleted`），本次改动完全没有触碰 memctx。
2. **`test_vdb_api`** —— 已用实验证伪回归：把源码回退到基线重新构建后，直接运行该二进制**同样 3/3 段错误**；经 ctest 连跑三次结果为「过/败/过」。属既有的状态与时序相关缺陷。

**结论：未发现由本次改动引入的新失败。**

### 6.4 必须记录的局限

1. **`vector_engine_search` 的改动没有测试覆盖。** 全仓唯一调用它的测试
   `test/db/storage/vector_query_hybrid_memory_test.cpp` **未被任何 CMakeLists 引用**（从未编译）。
   因此该改动只经过了"编译通过 + 全量回归无新增失败"的间接验证，**没有针对性用例证明索引分派与堆 top-k 的行为正确**。这是下一步最该补的测试。
2. **中途方案的自我纠错**：最初给 CheckpointManager 加了"持久化回调 + 回调成功才 truncate"的设计，
   并把回调接到了 `save_collections_meta`。回归时 `test_vdb_api` 出现新失败，定位为
   **后台线程读集合数据与 `vector_api_insert` 写入无互斥**（快照锁只串行化了快照之间，没保护集合本身）。
   该设计已回退为"后台线程不碰集合数据"，避免引入新的并发缺陷。
3. **后台不再周期截断 WAL**，意味着 WAL 会在两次显式 `vector_api_save` 之间持续增长。
   这是用磁盘增长换数据安全的刻意取舍；若要恢复周期截断，需要先让快照路径对集合数据加读锁，
   并解决 `api->collections` 数组本身无保护的问题——属独立任务。
4. **`engine_registry_test` 从未被 ctest 收集**，只在手工运行时才暴露。本次修正了其中
   `ModelCount` 的假前提（枚举在 10 号有保留空槽，原用例遍历全部整数并断言"都有名字"，
   实为被位置式初始化的错位 bug 掩盖）。

### 6.5 建议的下一步

1. 把 `vector_query_hybrid_memory_test.cpp` 接入 CMake，为检索重构补回覆盖（最高优先）。
2. 补 checkpoint 崩溃恢复用例：模拟「快照写入失败」与「truncate 后立即崩溃」两种场景。
3. 把 `engine_registry_test` 注册进 ctest。
4. 按 P0-3 补齐多模态 v2 的负向用例（多集合同局部 id、非空 filter），再改语义。

---

## 七、第三轮：补测试覆盖（同日完成）

上面 6.5 的 1–3 项已完成，并在此过程中**发现并修复了一个由上一轮重构引入的真实缺陷**。

### 7.1 新增测试

| 文件 | 用例数 | 作用 |
|---|---|---|
| `test/db/storage/vector_engine_search_test.cpp`（新增） | 7 | 锁定 `vector_engine_search` 行为：空集合、维度不匹配、top-k 升序与条数上限、重复查询稳定性、文件路径 id 映射、非法参数 |
| `test/db/api/vector_durability_test.cpp`（新增） | 3 | 锁定持久化不变式：save 后重载不丢、不 save 靠 WAL replay 恢复、**快照失败时不得截断 WAL** |
| `test/db/storage/vector_query_hybrid_memory_test.cpp`（接入） | 4 | 此前文件存在但从未被任何 CMakeLists 引用，等于从未编译 |
| `test/db/core/engine_registry_test.cpp`（注册进 ctest） | 14 | 此前只构建、未注册，断言长期不生效 |

`vector_engine_search_test` 与 `vector_durability_test` 的工作目录设在构建树，避免像 `test_vdb_api` 那样把状态留在源码树里互相污染。

### 7.2 新测试抓到的真实缺陷（已修）

`vector_engine_search` 重构时，最小堆的弹出顺序用错了方向：

```c
/* 错误：最小堆先弹出最小值，倒序写回 → 结果变成"最远优先" */
for (int32_t i = n - 1; i >= 0; i--) { results[i] = heap_pop(); }
```

`vector_engine_search_test.ResultsOrderedAndLimitedByTopK` 直接失败：
结果距离序列为 `1.349 / 1.272 / 0.141`，即**完全反序**，最近邻排在最后。

已改为顺序写回。注意：被删除的死代码 `vector_search()` 里也是同样的倒序写法——说明这个错误长期存在于无人调用的实现中，从未被测出。原选择排序实现是升序的，因此这属于**重构引入的语义回归**，靠新用例才拦住。

### 7.3 证明用例真的能抓到缺陷（不是摆设）

把 `src/db/api/vector_api.c` 单独回退到旧实现（先 truncate 后写快照）后重跑：

```
旧代码：SnapshotFailureDoesNotTruncateWal  FAILED
        Expected equality: 5  实际: 3      ← 新增的 2 条被截断丢弃
新代码：SnapshotFailureDoesNotTruncateWal  PASSED
```

用例对修复点有判别力，已实证。

### 7.4 最终回归

| | 基线 | 最终 |
|---|---|---|
| ctest 用例数 | 800 | 817 |
| 失败（ctest 计数） | 75 | 72 |
| 失败（唯一用例名） | 67 | 61 |
| 通过率 | 91% | 91% |

在**多注册 28 个用例**的前提下，失败数反而下降。新增用例全部通过。

与基线相比新出现的失败共 3 个，均已归因，**没有一个是本次改动引入的**：

- `SdkVectorsMemctxTest.DeleteReducesResults` —— memctx 一族已知的 flaky（测试自身 UAF）。
- `test_vdb_api` —— 既有抖动：ctest 连跑为「过/败/过」，源码回退到基线后直接运行同样 3/3 段错误。
- `test_spatial_engine` —— 仅在 `ctest -j8` 并行下 SEGFAULT，单独运行 3/3 通过；仓库自身的
  `build-debt-fixes.md` 已把它列为已知 SEGFAULT 项。属并行执行竞态。

### 7.5 仍未解决

- 页面池路径下 `vector_engine_search` 返回的是**页内索引**而非插入时的用户 id，与文件路径语义不一致。
  这是既有行为（旧实现同样如此），本次未改，已在新用例中注明并以 `FileBackedSearchReturnsInsertedIds`
  单独覆盖文件路径的 id 映射。
- `test_vdb_api` 的状态污染与并行不稳定仍未修（它把测试数据写在源码树 `test-results/` 下）。

---

## 八、第四轮：修掉上面两项（同日完成）

7.5 的两项都处理完了，过程中又挖出一个**导致偶发段错误的未初始化内存读**。

### 8.1 `test_vdb_api` 偶发段错误：根因是未初始化字段被当指针用

**现象**：`VdbApiTest.InsertAndSearch` 单独运行也会段错误；`--gtest_repeat=2` 必崩；
但**在 gdb 下从不复现**，输出又因 stdout 缓冲而丢失，一度无法定位。

**定位方法**（可复用）：
1. 用 `--gtest_repeat=N` 把"偶发"变成"必现"；
2. 在测试与产品代码里临时插 `fprintf(stderr, ...)` + `fflush`（stderr 无缓冲），
   逐层缩小到 `vdb_search()` 内部；
3. 定位后还原插桩（本次用 `/tmp` 备份 + `cp` 还原）。

**根因**：`src/db/api/vector_api.c` 的 `vector_api_search()` 用 `malloc` 分配结果数组，
只填 `id` / `distance`，而 `VectorSearchResult` 还有 `void *metadata` 与 `int32_t metadata_size` 两个字段。
调用方 `vdb_search()`（`src/db/api/vdb_api.c:413`）**无条件**读这两个字段：

```c
if (r->metadata && r->metadata_size > 0) {
    memcpy(malloc(r->metadata_size), r->metadata, r->metadata_size);  // 野指针 + 野长度
}
```

未请求元数据时这两个字段是堆上的残留值 → `memcpy` 野指针 → 段错误。
gdb 下新分配的页恰好是零，所以不复现；分配器复用脏内存时才崩——这解释了全部现象。

**修法**（两层）：
1. 根因：结果数组改用 `calloc`，保证结构体全字段初始化；
2. 防御：`vdb_search()` 增加 `sp.with_metadata &&` 前置判断，只在真正请求元数据时才读。

**验证**：修复前单次运行即崩；修复后单次、`--gtest_repeat=3`、连跑 5 次全部通过。
该用例已从失败列表消失。

### 8.2 页面池路径的 id 语义：改为保留写入时的真实 ID

**根因**：`vector_engine_tuple_insert()` 里 `ptr += sizeof(uint64_t); /* skip id */`
——用户给的 ID 被直接丢弃。页池其实**本来就存 ID**（`vector_page_t.vector_ids`），
只是插入时传了 `-1` 让页池自动生成页内序号，检索时又直接返回循环下标。

**修法**：
1. 新增 `vector_page_get_id()`（`vec_page.c` / `vec_page.h`），按与 `vector_page_get_vector`
   相同的 页/偏移 映射读出写入时的 ID；
2. 插入时把解析出的真实 ID 传给 `vector_page_append()`；
   页内 ID 槽位是 `int32`，超出范围时**明确报错并拒绝写入**，而不是静默替换成页内序号；
3. 检索时用 `vector_page_get_id()` 取真实 ID，读不到才退回页内序号。

**验证**：`vector_engine_search_test.ResultsOrderedAndLimitedByTopK` 断言
页面池路径返回 `id == 10`（写入时的 ID），通过；文件路径的 id 映射由
`FileBackedSearchReturnsInsertedIds` 单独覆盖。

### 8.3 测试隔离

`vdb_api_test.cpp` 的数据目录从固定的 `test-results/engineering/vdb_api`
改为**按 pid 隔离**的 `test_data/vdb_api_<pid>`，并把 ctest 的 `WORKING_DIRECTORY`
从 `${CMAKE_SOURCE_DIR}` 改为 `${CMAKE_BINARY_DIR}`：

- 不再往源码树写测试数据；
- 多轮/多进程执行互不可见，消除"上一轮残留集合被下一轮加载"的串扰；
- 清理改用 `error_code` 重载，失败时用 `EXPECT_FALSE` 显式报出，而不是静默残留。

### 8.4 顺带发现：页面池的数据在重开后读不到（既有缺陷，未修）

`vector_page_pool_load()`（`vec_page.c:189`）只恢复 `page_count`，注释写
"暂时不加载页数据，按需加载"；但 `vector_page_get_vector()` 在页不驻留时**直接返回 -1**，
并没有真正的按需加载实现。

后果：页池写入的数据在关闭再打开后完全读不到，检索恒返回 0 条。

已用 `DISABLED_IdsSurviveReopen` 用例把该缺口记录在案（附原因说明），
不留在默认测试集里制造红灯。**这是独立任务，需要实现按需加载或改回全量加载。**

### 8.5 关于测试套件的稳定性（重要）

这一轮反复跑全量 ctest，发现该套件的失败数**在完全相同的前提下会大幅波动**：

| 运行 | 用例数 | 失败（唯一名） |
|---|---|---|
| 基线（源码回退） | 800 | 67 |
| 第三轮结束 | 817 | 61 |
| 第四轮结束 | 817 | 68 |

波动来源已逐个确认，**均与本次改动无关**：

- **memctx 一族**（`MmdbRequestScopeTest` / `MmdbRootContextTest` / `SdkCoreMemctxTest` /
  `SdkVectorsMemctxTest` / `MemoryLeakDetectionTest`）：测试自身 UAF 误报，双向抖动。
- **`NSWTest.*`**：`nsw_vector_test.exe` 运行超过 60 秒被 ctest 判超时，属性能/挂起问题；
  在三次运行中出现次数为 13 / 10 / 13。
- **`test_spatial_engine`**：仅 `-j8` 并行下偶发 SEGFAULT，单独运行 3/3 通过。
- **`test_vdb_api`**：本轮已修复（见 8.1），从失败列表消失。

**结论**：这套测试目前只能作为"能构建 + 无确定性回归"的信号，
**失败总数的绝对值不具备可比性**。要得到可信的回归判据，需要先治理上述四类噪声
（尤其是 memctx 的 UAF 误报和 NSW 的超时），否则任何改动都会淹没在噪声里。


