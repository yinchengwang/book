# VDB 量化压缩技术深度内容

## Purpose

为向量数据库量化压缩技术提供深度技术文章，涵盖乘积量化 PQ、标量量化及量化技术全景等核心知识点，每篇文章遵循 8 段式模版，强调压缩比与召回率的权衡分析。

## Requirements

### Requirement: VDB 量化压缩技术深度文章

VDB 量化压缩技术的每篇深度文章 SHALL 覆盖以下知识点：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-pq-quant` | 乘积量化 PQ | intermediate |
| `db-quantization` | 量化技术全景 | advanced |
| `db-scalar-quant` | 标量量化 | intermediate |

每篇文章 SHALL 遵循 8 段式模版（同 vdb-core-index-algorithms 定义）。

#### 特殊要求：量化文章 SHALL 额外包含

- 压缩比 vs 召回率的 Pareto 曲线描述
- 训练（码本/K-Means）阶段的耗时分析
- SIMD 查表加速的伪代码或真实代码
- 在 Faiss 中的实际索引类型映射（如 IndexIVFPQ、IndexSQ8）

#### Scenario: 量化技术文章完整性

- **WHEN** 用户阅读量化相关文章
- **THEN** 文章 SHALL 包含压缩比的计算示例、精度损失的分析、参数量化/反量化的流程描述

#### Scenario: 量化全景文章覆盖度

- **WHEN** 用户阅读 `db-quantization` 文章
- **THEN** 文章 SHALL 覆盖 PQ/OPQ/SQ/AQ/RVQ 至少 5 种量化方法，并给出各自的适用场景对比
