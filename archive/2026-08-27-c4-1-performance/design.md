# C4-1 性能专项设计文档

## 设计目标

SIMD 补全 + 向量化执行试点 + 基准进 CI + 各模态基准报告。

## 方案

1. **SIMD 补全**：compute_distance_internal 接入 AVX2 L2/IP/Cosine（search_layer 已有，上层补齐）
2. **向量化执行**：TupleBatch 中间表示 + SeqScan/HashAgg 两算子向量化
3. **基准进 CI**：engineering/test/db/benchmark/ 每模态基准 + ctest 标签 `--benchmark`
4. **各模态基准报告**：docs/perf-{vector,rel,ts,graph,kv,doc,spatial,tree}.md

## 文件

- `engineering/test/db/benchmark/` 新增向量/SQL 基准可执行文件
- `docs/perf-*.md` 基准报告（8 个文件）
