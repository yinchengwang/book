# 每模态构建隔离（per-modality build isolation）实测报告

> 2026-10-28 收尾。战役计划：`docs/superpowers/plans/2026-10-27-per-modality-build-isolation.md`。
> 基线对照：`docs/arch/db/build-baseline-2026-10-27.md`。

## 1. 架构分层（最终态）

```
L0  mm_base          errors/log/guc/metrics/structured_log/读写锁/mm_record/sha256
                     只依赖 multimodal_config_gen + Threads
L1  mm_storage_api   engine_registry.c + mm_storage.c + func_registry.c
                     + 构建期生成的注册收集器（mm_builtin_engines.c）
L2  mm_<modality>    每模态一个独立静态库，显式链接所需 L1 设施
L3  db_core / relational 聚合（sql/parser/optimizer/validator/executor）
```

- 公共层 → 模态：仅经 `include/db/mm_engine_register.h` 的注册入口反转，
  由 `cmake/gen_builtin_engines.cmake` 按 profile 生成收集器（空 profile
  产出空函数体，不用弱符号，兼容 MSVC）。
- 模态 → 公共层：`mm_<modality>_register()`（引擎）与
  `mm_<modality>_sql_function_table()`（SQL 函数表，纯数据 getter，
  由 `func_registry_register_table` 下行注册，L1↔L2 无双向符号引用）。
- 去环门禁：`python scripts/check_layering.py src/db/core` → OK（core 零模态私有 include）。

## 2. 白名单依赖（硬编码于 ModalityProfiles / 各库 CMakeLists）

| 模态 | 依赖 |
|---|---|
| graph | kv |
| st | spatial |
| sparse | vector |
| doc | vector |
| 其余 | 无模态间依赖 |

`cmake/ModalityProfiles.cmake` 配置期校验闭包封闭，不封闭直接 FATAL_ERROR。

## 3. MMDB_MODALITY profile 与冒烟实测

| profile | 模态 | 编译单元 | 库 | 冒烟 | 引擎数 |
|---|---|---|---|---|---|
| kv | kv | 509 | 6 / 30 MiB | mm_kv_smoke OK | 1 |
| vector | vector | 676 | 7 / 24 MiB | mm_vector_smoke OK | 1 |
| graph | graph;kv | — | — | mm_graph_smoke OK | 2 |
| spatial | spatial | — | — | mm_spatial_smoke OK | 1 |
| tree | tree(yang) | — | — | mm_yang_smoke OK | 1 |
| sparse | sparse;vector | — | — | mm_sparse_smoke OK | 1（SPARSE 不注册引擎） |
| doc | doc;vector | — | — | mm_doc_smoke OK | 2 |
| ts | ts | — | — | mm_ts_smoke OK | 1 |
| st | st;spatial | — | — | mm_st_smoke OK | 2 |
| rdf | rdf | — | — | mm_rdf_smoke OK | 1 |
| relational | relational | 574 | 11 / 35 MiB | mm_relational_smoke OK | 0（无 storage_ops_t 引擎） |
| legacy（默认） | relational;kv;graph;vector;spatial;tree;sparse | 764 | 111 / 179 MiB | — | 5 |
| full | 全部 13 | — | — | — | — |

冒烟目标链接闭包不含 `db_storage` / `db_core`（链接期独立性机械证明）。
冒烟期望引擎数注意点：SPARSE 不实现 storage_ops_t（无引擎注册）；
relational 不通过 storage_ops_t 注册（冒烟钉住解析层，见 mm_relational_smoke.c 注释）。

### 3.1 冒烟断言的 profile 感知

冒烟断言**不能**硬编码引擎计数：单模态 profile 下「只注册 N 个引擎」成立，
但 legacy/full profile 会注册多个引擎，写死计数会让冒烟必然失败。

当前实现（`cmake/ModalityProfiles.cmake` + `cmake/mm_smoke_expect.h.in` +
`test/smoke/mm_smoke_assert.h`）：

- 构建期按 profile 模态列表算出期望引擎位掩码
  `MMDB_EXPECTED_ENGINE_MASK`（位序 = `DataModel` 枚举值）；
- 生成 `mm_smoke_expect.h`，冒烟断言「注册表非空槽位集合**恰好等于**该掩码」
  且「本模态引擎已注册」。

单模态 profile 下掩码只有本模态一位，与原「只注册 N 个」断言语义等价：

| profile | 期望掩码 | 含义 |
|---|---|---|
| kv | 2 | KV |
| vector | 8 | VECTOR |
| graph | 6 | GRAPH \| KV（闭包带入 kv） |
| sparse | 8 | VECTOR（SPARSE 不注册引擎） |
| legacy | 206 | KV\|VECTOR\|GRAPH\|SPATIAL\|TREE |

实测：legacy 下 6 个冒烟全通过；kv/vector/graph/sparse 单模态冒烟全通过，
且 kv profile 的链接闭包仅 `mm_base + mm_storage_api + mm_kv`，隔离未被破坏。

## 4. 改造前后对比（legacy/full 口径）

| 指标 | 改造前（build/base） | 改造后（build/t7，legacy profile） |
|---|---|---|
| 编译单元 | 749 | 764（+生成收集器/新测试，纯搬迁任务均未增减） |
| 静态库 | 101 / 149 MiB | 111 / 179 MiB（含 12 个独立模态库，模态对象与聚合库存在 pull-once 去重副本） |
| 可执行 | 168 / 2240 MiB | 177 / 2427 MiB（+12 个模态冒烟） |
| ctest 用例 | 846 | 618（去重，见下） |

### 4.1 ctest 用例数从 846 降到 618 的原因

**不是丢失覆盖，而是去重。**

`cmake/ProjectUtils.cmake` 的 `add_project_test` 默认 GLOB 当前目录下所有
`.cpp/.c` 编入目标。在「一个目录多个源文件、多个目标」的场景下，同一源文件
会被编进每个目标，使同一 gtest 用例在 ctest 中注册多份：

| 目录 | 源文件数 | 目标数 | 改造前重复倍数 |
|---|---|---|---|
| `test/sdk/memory` | 6 | 6 | 6× |
| `test/db/bgworker` | 2 | 2 | 2× |

这些副本共用固定数据文件（`test_leak_detection.db` / `test_root_context.db` /
`test_request_scope.db`），并行执行时互相踩踏，表现为随机失败——最终回归中
`MmdbRequestScopeTest.AllocationsFreedOnEnd` 与
`MmdbRootContextTest.CloseDeletesRootContext` 各有 1/6 副本失败，单独重跑全部通过。

修复：`add_project_test` 增加 `SOURCES` 关键字显式指定源文件，
两个目录各自改为「一个目标一份源」。改造后 ctest 用例名无任何重复，
618 个用例每个恰好执行一次，覆盖未减少。

单模态裁剪效果（Task 16 守卫后）：
- kv：509 单元 / 3 可执行 / 5 MiB / 101 测试（全量 618）。
- vector：676 单元（相对 kv 树 +167 ≈ vector_index 子树）。
- relational：574 单元（不含 vector_index 全树）。

## 5. 已知事项与后续跟进

1. **过渡期循环链接**：`db_core → db_storage`（PUBLIC）与
   `mm_storage_api → (mm_kv/…/db_storage)`（PRIVATE，SCC）保留至全部模态
   独立——Task 9-14 已把 12 个模态全部独立并直链，SCC 中仅剩
   db_core↔db_storage 一条边；彻底删除需让 `db_storage` 聚合内不再有
   引擎对象（buffer/page 等公共设施升 L1 独立库，见架构文档 L1 规划）。
2. **Linux 侧验证**：st/rdf 的修复在 Windows/MinGW 实测 0 错误；
   Linux 侧（原 nftw 路径已改为 dirent 统一实现）留 CI 跟进。
3. **bm25 双实现**：sparse/bm25_index.c（sparse 家族签名）与
   vector_index/BM25（rest_api 实际使用）同名不同签名，已物理分离
   （storage_sparse_bm25 独立 OBJECT）；彻底合并为后续技术债。
4. **SQL 函数注册顺序**：生成器按改造前顺序 vector→ts→graph→doc→
   spatial→kv→tree 发射，顺序保留。

### 5.1 回归中定位并修复的两个既存缺陷

最终回归（618 用例）相对基线多出 1 个失败用例 `test_spatial_engine`。
经 gdb 定位为**两个互相独立的既存内存缺陷**，与模态隔离改造无因果关系
（基线构建 `build/base` 的同一用例崩溃率 40%，改造后 25%）。修复前基线
两次 ctest 恰好都躲过了。

**缺陷一：`rtree_insert` 根分裂的 use-after-free**
（`src/db/storage/spatial/rtree.c`）

`rtree_quadratic_split` 对叶子节点是「拷贝条目」、对内部节点是「**移交子节点
指针所有权**」。而 `rtree_insert` 在分裂根之后调用 `rtree_node_free(old_root)`，
该函数会递归释放 old_root 的子节点 —— 正是刚被移交的那批指针。新根子树因此
挂着已释放的悬垂指针，后续插入访问该节点即崩溃（gdb 现场：崩溃节点
`is_leaf=false / num_entries=0 / children=NULL / bbox` 全 0，是被分配器清零的
已释放堆块）。触发条件是根被分裂两次（`tree->height` 达到 3），此时 old_root
已是内部节点。

修复：释放旧根前先 `old_root->num_entries = 0`，使 `rtree_node_free` 只回收
节点自身的数组，不再递归释放已移交的子节点。

**缺陷二：`hilbert_index_knn` 的结果数越界**
（`src/db/index/hilbert/hilbert.c`）

`items` 数组按 `index->count` 分配，但结果条数 `n = min(k, max_results)` 未与
`count` 取小。当索引实际记录数少于 `k`（例如 `count=4` 而 `k=5`）时，
`items[i]` 越界读到垃圾 `idx`，随后 `index->records[垃圾 idx]` 触发越界访问
而崩溃。

修复：`n` 增加 `if (n > index->count) n = index->count;` 钳位。

修复后 `test_spatial_engine` 连续 40 轮全量运行 0 失败。

### 5.2 最终回归结论（Task 17/20 收口）

以 legacy profile（`build/t7`）跑全量 ctest，与基线（`build/base` 两次运行的
失败集并集）做**失败用例名集合**比对：

| 项 | 数量 |
|---|---|
| 最终回归用例总数 | 618 |
| 最终回归失败用例数 | 46 |
| 基线失败用例数（两次并集） | 58 |
| **新增失败（回归中多出来的）** | **0** |
| 修复（基线失败、现在通过） | 12 |

46 个失败全部是基线中已存在的既有失败（benchmark 召回率、raft、rag、xquery、
sql 集成、doc pipeline 等），与按模态构建隔离改造无因果关系；`test_spatial_engine`
已从失败列表中消失。

12 个「修复」项全部来自 §4.1 的用例去重：去重前同一用例在 ctest 中有 6 份
副本并行抢同一数据文件，偶发失败；去重后每例只跑一次，踩踏消失。典型如
`MemoryLeakDetectionTest.RepeatedOpenCloseNoLeak`、
`SdkVectorsMemctxTest.*`（5 项）、`MmdbRootContextTest.ErrorMessagesAreTracked`。

## 6. 如何新增一个模态

1. `src/db/storage/<mod>/` 建独立 STATIC 库 `mm_<mod>`（模板：storage/kv），
   PUBLIC 链 mm_base + 所需 L1 设施（`nm -u` 实测）+ 白名单依赖模态。
2. `engine_registry.c` 加 `mm_<mod>_register()`（`#ifdef MMDB_ENABLE_<MOD>`），
   `include/db/mm_engine_register.h` 加声明。
3. SQL 函数表：模态目录建 `*_sql_functions_register.c`，导出
   `mm_<mod>_sql_function_table(size_t*)`（纯数据，不调 L1）。
4. `cmake/gen_builtin_engines.cmake` 两个列表与 `cmake/ModalityProfiles.cmake`
   的映射表登记短名；如需新 profile，按闭包规则配 `MMDB_PROFILE_<name>`。
5. `test/smoke/mm_<mod>_smoke.c` + `test/smoke/CMakeLists.txt` 注册
   （按 TARGET 守卫）；跑 `check_layering.py src/db/core` 门禁。
   冒烟断言用 `mm_smoke_assert_engine_set(<DataModel>, 1)`，
   **不要**写死引擎计数（新模态的槽位需登记进
   `MMDB_MODALITY_ENGINE_<mod>` 映射表）。
