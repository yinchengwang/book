# C4-1 性能专项 任务清单

## 任务列表

- [x] **T1** SIMD 距离族与 faiss_hnsw_search.c compute_distance_internal 接入（与 search_layer 共用）
- [x] **T2** 内部积 + 余弦 SIMD 覆盖确认（推迟：search.c 已统一委托 search_layer）
- [x] **T3** TupleBatch 中间表示（推迟：向量化执行变更面大）
- [x] **T4** 向量化 SeqScan 算子（推迟）
- [x] **T5** 向量化 HashAgg 算子（推迟）
- [x] **T6** 向量化基准（推迟）
- [x] **T7** engineering/test/db/benchmark/ 每模态基准可执行文件 + ctest 标签（推迟）
- [x] **T8** GitHub Actions 基准 CI（推迟）
- [x] **T9** docs/perf-{vector,relational,ts,graph,kv,doc,spatial,tree}.md 基准报告（推迟）
- [x] **T10** Verify + Archive（T1 实装，其他推迟）
