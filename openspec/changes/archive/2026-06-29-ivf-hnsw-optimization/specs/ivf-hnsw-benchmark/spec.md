# IVF-HNSW 基准测试规格

## Purpose

定义 HNSW-IVF 索引的性能和精度基准测试需求。

## ADDED Requirements

### Requirement: 精度测试

HNSW-IVF SHALL 提供 recall@k 精度测试，与暴力搜索和现有 IVF 对比。

#### Scenario: Recall@K 对比测试

- **WHEN** 在相同数据集上执行 recall@10 测试
- **THEN** 输出 HNSW-IVF、IVF、暴力搜索的 recall 率

#### Scenario: Multi-assignment 召回率提升

- **WHEN** 对比 k=1 和 k>1 的 recall@10
- **THEN** k>1 时召回率应高于或等于 k=1

### Requirement: 性能测试

HNSW-IVF SHALL 提供 QPS 和延迟测试。

#### Scenario: QPS 测试

- **WHEN** 执行 1000 次随机查询
- **THEN** 输出平均 QPS

#### Scenario: 延迟分布测试

- **WHEN** 执行多次查询
- **THEN** 输出 P50、P90、P99 延迟

### Requirement: 粗排性能对比

HNSW-IVF SHALL 提供粗排阶段的性能对比。

#### Scenario: 粗排时间对比

- **WHEN** 在 nlist=10000 的索引上执行 100 次查询
- **THEN** 输出 HNSW 粗排 vs 暴力粗排的平均耗时

### Requirement: 内存测试

HNSW-IVF SHALL 提供内存占用测试。

#### Scenario: 索引内存大小

- **WHEN** 创建索引后查询内存大小
- **THEN** 输出 HNSW 图、一级中心、二级桶、分配表的内存占用

#### Scenario: Multi-assignment 内存开销

- **WHEN** 对比 k=1 和 k=2 的内存占用
- **THEN** k=2 时额外增加约 100% 的分配表内存
