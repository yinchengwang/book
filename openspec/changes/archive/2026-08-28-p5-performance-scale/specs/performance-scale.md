# P5 性能规模化 规格

## 能力: cross-platform-simd

跨平台 SIMD 检测与 AVX2 距离函数族。

### ADDED Requirements

#### Requirement: SIMD CPU 检测
系统 SHALL 在 Windows (MSVC `__cpuidex`)、Linux/macOS (GCC/Clang `__get_cpuid_count`) 上
正确检测 AVX2 支持，且提供统一的运行时 fallback 路径。

#### Requirement: AVX2 距离函数族
系统 SHALL 提供 AVX2 加速的 L2、内积、余弦三种距离函数，且在不支持 AVX2 的 CPU
上自动 fallback 到标量实现。

#### Requirement: HNSW+filter SIMD 集成
HNSW filtered search 路径 SHALL 调用 SIMD 距离函数对候选重排。

## 能力: cross-platform-lock

#### Requirement: pthread_rwlock 跨平台 Wrapper
系统 SHALL 提供 `mmdb_rwlock_t` 跨平台包装（Windows SRWLOCK / POSIX pthread_rwlock），
所有读写锁调用走统一 API。

## 能力: vector-index-selector

#### Requirement: selector 集成
`vectors.c` SHALL 调用 `vector_index_selector` 决策 HNSW / IVF-PQ / IVF / flat，
不再使用硬编码阈值。索引创建失败时 SHALL 回退到 flat。

## 能力: vector-recall-validation

#### Requirement: 1M Recall@10 ≥ 0.85
1M×128 数据集 SHALL 达成 Recall@10 ≥ 0.85。
100K 数据集 SHALL 达成 Recall@10 ≥ 0.95。

#### Requirement: 阶梯基准报告
系统 SHALL 输出 100K / 1M / 10M 阶梯基准数据到 `docs/performance-scale-report.md`，
包含 qps、Recall@10、内存占用。

## 能力: roaring-bitmap

#### Requirement: roaring bitmap 亿级压缩
当 bitmap 元素超过 ROARING_THRESHOLD（默认 100000）时，系统 SHALL 自动切换到 CRoaring 路径，
内存占用相比 sorted array SHALL 降低 ≥ 50%。

## 能力: dual-modality-collection

#### Requirement: 同集合多索引并存
单个 collection SHALL 同时支持 VECTOR 与 TEXT 索引并存；
hybrid 查询 SHALL 走 RRF 双通道融合，而非单通道。

#### Scenario: hybrid RRF 双通道
- **WHEN** collection 同时启用 VECTOR + TEXT 索引并发起 hybrid 查询
- **THEN** 系统分别召回 VECTOR_TOPK 和 TEXT_TOPK，按 RRF 融合打分

## 性能优化

#### Requirement: build_filter_ctx O(1) 哈希查找
filter 上下文构建 SHALL 用哈希表替换线性扫描，时间复杂度从 O(N) 降到 O(1)。

#### Requirement: 最小堆候选排序
TopK 选择 SHALL 用最小堆替换选择排序。

#### Requirement: 0 候选排查
filtered search 在候选数为 0 时 SHALL 执行 greedy descent，而非直接返回空集。