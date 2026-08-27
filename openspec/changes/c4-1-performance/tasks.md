# C4-1 性能专项 任务清单

## 任务列表

- [ ] **T1** SIMD 距离族与 faiss_hnsw_search.c compute_distance_internal 接入（与 search_layer 共用）
- [ ] **T2** 内部积 + 余弦 SIMD 覆盖确认（与 P5 整合）
- [ ] **T3** TupleBatch 中间表示（向量化执行基础）
- [ ] **T4** 向量化 SeqScan 算子
- [ ] **T5** 向量化 HashAgg 算子
- [ ] **T6** 向量化基准（vs 行式 Volcano）
- [ ] **T7** engineering/test/db/benchmark/ 每模态基准可执行文件 + ctest 标签
- [ ] **T8** GitHub Actions 基准 CI（回归报警）
- [ ] **T9** docs/perf-{vector,relational,ts,graph,kv,doc,spatial,tree}.md 基准报告
- [ ] **T10** Verify + Archive
