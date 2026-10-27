# 按模态独立构建 — 改造前基线快照（2026-10-27）

> 本文档由实施计划《2026-10-27-per-modality-build-isolation》Task 0 产出。
> 所有后续 Task 的「编译单元数不增加 / 产物规模对比」均以本文数字为准。
> 测量口径以 `scripts/measure_build.py` 的 docstring 为唯一权威定义。

## 1. 测量口径

- `compile_units`：解析 `build.ninja` 中形如 `build <path>.o(bj): <RULE含COMPILER> …`
  的编译行计数（一个编译行 = 一个编译单元）。`build.ninja` 不存在时退回磁盘 `*.o/*.obj` 计数。
- `static_libs` / `static_lib_bytes`：递归 `*.a` + `*.lib`。
- `executables` / `executable_bytes`：递归 `*.exe`（Windows）；Unix 上为有可执行位的非库/对象文件。
- **注意**：Windows/MinGW 下目标文件后缀是 `.obj`，ninja 规则名形如
  `C_COMPILER__<target>_<config>`（COMPILER 后跟下划线），解析正则必须兼容这两点。

## 2. 改造前基线（`build/base`，MinGW / Ninja / Debug，BUILD_TESTING=ON）

| 指标 | 数值 |
| --- | --- |
| compile_units（总计） | 749 |
| ├ 引擎自身（src/include/sdk/drivers） | 515 |
| ├ 测试（test/） | 228 |
| ├ third_part（cjson/gtest/gmock） | 5 |
| └ 其他（生成代码） | 1 |
| static_libs | 101 |
| static_lib_bytes | 155,791,346（≈149 MiB） |
| executables | 168 |
| executable_bytes | 2,348,676,360（≈2.19 GiB） |

构建验证：`ninja -C build/base -k 0` 二次运行报 `no work to do`，即基线全新构建 **0 失败目标**
（注意：`MMDB_ENABLE_DOCUMENT/TIMESERIES/STREAM/COLUMNAR/ST/RDF` 等开关默认 OFF，
5 个未启用模态的 59 个既有编译错误不在默认构建内，详见设计文档 §5.10）。

基线提交：`511d0536b`（代码状态）、`43e021d93`（执行前固化的向量检索重构/save 修复/注册槽位修正）。

## 3. ctest 失败用例基线（两次运行取交集）

**比较口径：比较的是失败用例名集合，不是失败总数。**
该套件噪声大（历史观测失败总数 61–68 波动），flaky 用例每次不同；
只有**两次运行都失败**的用例才进入「稳定失败」集合，作为 Task 17 回归的对照基线。

- run1: `build/base-run1.xml`（`ctest -j8 --output-junit`）
- run2: `build/base-run2.xml`（同上，独立第二次运行）

实测：run1 失败 **58**、run2 失败 **59**，稳定失败（交集）**52**、flaky（并集−交集）**13**。
原始清单：`build/base-stable-failed.txt`、`build/base-flaky.txt`（不入库，重跑可再生成）。

### 3.1 稳定失败（两次都失败，共 52）

- backup_test
- BenchmarkVectorTest.SearchK10 / SearchK100
- BgWorkerTest.DuplicateRegistration
- CliSmoke.ExecCreateInsertSelect
- cross_lang_consistency_test
- cross_modality_test
- DriverSmoke.DeleteThenSelect / InsertThenSelect / InsertWithColumnList /
  SelectMultipleRowsAndColumnOrder / UpdateThenSelect
- E2ECapability.GroupByAggregate / InnerJoin / LimitOffset / OrderByLimit / SubqueryInFrom
- HNSWBenchmark.BasicSearch / ParameterSweep / RecallTest
- hnsw_filter_test
- hybrid_dual_channel_enabled_test / hybrid_dual_channel_test
- hybrid_search_test
- IVFBenchmark.RecallTest
- LSHBenchmark.RecallTest
- MemoryLeakDetectionTest.RepeatedOpenCloseNoLeak
- mmdb_raii_test
- mmdb_text_test
- rag_embedding_test / rag_test / rerank_test
- raft_test
- SdkVectorsMemctxTest.DeleteReducesResults / EmptyCollectionReturnsZeroResults /
  ErrorPropagationWithContext / RepeatedCyclesDoNotBreakContext / UpsertReplacesVector
- security_test
- selector_integration_test
- SSGTest.RecallTest
- staircase_benchmark（Timeout）
- SQLIntegrationTest.Insert_WithColumnList / Select_AllColumns / Select_EmptyTable /
  Select_SpecificColumns
- test_doc_pipeline
- test_rest_api
- test_storage_wal
- test_vector_index
- xquery_id_len_test / xquery_test

### 3.2 flaky（仅一次失败，共 13）

- MmdbRequestScopeTest.BeginCreatesContext / EndRestoresPreviousContext /
  InnerAllocationsFreedOnInnerEnd / NestedScopes
- MmdbRootContextTest.ContextHierarchyIsCorrect / ErrorMessagesAreTracked /
  LongErrorMessageIsTruncated / PathAllocatedFromContext
- SdkCoreMemctxTest.ErrorMessagesUseContext / RepeatedCollectionLifecycle
- SdkVectorsMemctxClose.CloseDeletesRootContext
- SdkVectorsMemctxTest.BatchInsertAndSearch
- StorageBackendTest.MMapBackendReadWritePage

## 4. 口径验证记录

脚本在旧树 `projects/build/verify-debt`（产物已清理的 ninja 树）上验证：
`static_libs == 101`、`executables == 165`，与人工清点一致；`compile_units == 746`（该树 build.ninja 仍在，
含 3 个当时尚未加入的测试文件，与 `build/base` 的 749 差 3，差值即 43e021d93 新增测试的编译单元）。
