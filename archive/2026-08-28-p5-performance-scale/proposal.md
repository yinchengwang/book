# P5 性能规模化 Proposal

## Why

商业化路线图要求在 1M × 128 规模上达成搜索 ≥2000 qps + Recall@10 ≥0.95。P4 ARCHIVED 后，性能规模化是下一个优先级。当前基线：158K vec/s 插入、HNSW 搜索路由已就绪、SIMD L2 已实现。需要补齐：跨平台兼容（X1/X2）、SIMD 完整族、selector 集成、1M Recall 验证、性能阶梯 + 报告，以及 P5-2 roaring bitmap / P5-6 双模同集合架构扩展。

## What Changes

- **解 block 现有 bug**：embedding_test 期望值同步（P5-7）、MSVC `__cpuidex` 兼容（X1）、pthread_rwlock 跨平台 wrapper（X2）
- **SIMD 完整族**：L2 + 内积 + 余弦 AVX2 加速，HNSW 路径 + xquery 路径集成
- **selector 集成**：vectors.c 调用 vector_index_selector 替换硬编码阈值
- **1M Recall@10 验证**：1K 子集采样 GT，Recall@10 ≥ 0.85
- **100K/1M/10M 阶梯基准** + `docs/performance-scale-report.md`
- **性能优化**：build_filter_ctx 哈希查找（P5-1）、选择排序→最小堆（P5-3）、0 候选排查（P5-5）✅ 已完成
- **P5-2 roaring bitmap**：亿级 bitmap 压缩
- **P5-6 双模同集合 SDK 架构**：collection 支持多索引类型并存，激活 T4.2 次通道

## Capabilities

| 能力 | 交付 |
|------|------|
| 跨平台 SIMD | MSVC + GCC + Clang 均可编译运行 AVX2 距离函数 |
| 跨平台锁 | Windows SRWLOCK / POSIX pthread_rwlock 统一 wrapper |
| HNSW+filter qps | 1M×128 ≥ 2000 qps |
| Recall@10 | 1M≥0.85，100K≥0.95 |
| 性能报告 | docs/performance-scale-report.md（阶梯数据） |
| 双模同集合 | VECTOR+TEXT 并存，hybrid 次通道真正激活 |
| 亿级 bitmap | roaring bitmap 压缩，内存 -80% |

## Impact

- 11-13 个新 commit（每个 Task 单 commit）
- 修改文件集中在 engineering/src/sdk/vectors/vectors.c + faiss_hnsw + distance + collection
- 新增文件：mmdb_lock.h/c、selector_integration_test、staircase_benchmark、performance-scale-report.md
- ABI 零破坏：所有 mmdb_* 签名不变，结构体仅末尾 append

## Commit 总览

| # | 阶段 | 简述 |
|---|------|------|
| 1 | T5.0.1 | embedding_test 期望值修复 |
| 2 | T5.0.2 | MSVC __cpuidex 兼容 |
| 3 | T5.0.3 | pthread_rwlock → 跨平台 wrapper |
| 4 | T5.1 | SIMD 完整族 + HNSW 路径集成 |
| 5 | T5.2 | Windows SRWLOCK wrapper 集成（与 T5.0.3 合并后独立验证） |
| 6 | T5.3 | vector_index_selector 集成 |
| 7 | T5.4 | 1M Recall@10 验证 |
| 8 | T5.5 | 100K/1M/10M 阶梯 + 性能报告 |
| 9 | T5.6 | P5-1 哈希优化 + P5-3 最小堆 + P5-5 0 候选排查 |
| 10 | T5.7 | P5-2 roaring bitmap |
| 11 | T5.8 | P5-6 双模同集合 SDK 架构 |
| 12 | Review | Whole-branch review |
| 13 | Archive | 归档 |
