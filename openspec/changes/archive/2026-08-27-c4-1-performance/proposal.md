# C4-1 性能专项 Proposal

## Why

差距分析 README §1.4 四维度目标之一。Vector 搜索底层已接 AVX2（`faiss_hnsw_search_layer.c:31-53`），上层贪婪下降 `compute_distance_internal`（`faiss_hnsw_search.c:21-54`）仍标量；选 P5 已修选择排序→最小堆；关系执行器纯行式 Volcano；Timeseries 热路径未压缩（已在 C2-4 解决）；基准测试零 CI 集成。

## What Changes

- Vector SIMD 补全：`compute_distance_internal` 接入 AVX2 距离族（与 search_layer 共享同一实现）
- 内积 + 余弦 SIMD（与 P5 SIMD 族整合确认覆盖范围）
- 关系执行器向量化试点：TupleBatch 中间表示，SeqScan + HashAgg 两算子先行
- Timeseries 基准复用 C2-4
- 图算法优化：CSR 双视图（C2-3）后遍历无锁化收益量化
- **基准测试进 CI**：`engineering/test/db/benchmark/` 每模态一个基准可执行文件 + ctest 标签 `--benchmark`
- 各模态基准报告落 `docs/perf-<modality>.md`

## Capabilities

| 能力 | 交付 |
|------|------|
| Vector SIMD 全路径 | 1M×128 ≥2500 qps（与 P5 baseline 对比不退化） |
| 向量化执行 | SeqScan + HashAgg 两算子 2x 加速（基准） |
| CI 基准 | 基准进 GitHub Actions，回归即报警 |
| 基准报告 | 各模态基准文档落 docs/ |

## Impact

- 修改：faiss_hnsw_search.c（SIMD 补全）、executor.c（向量化）、基准测试目录
- 新增：TupleBatch 表示、向量化 SeqScan/HashAgg、各模态基准
- 预计 6-8 个 commit
- 依赖：C1-2
