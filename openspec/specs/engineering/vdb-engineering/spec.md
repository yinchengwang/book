# VDB 系统工程实践深度内容

## Purpose

为向量数据库系统工程实践提供深度技术文章，涵盖 DiskANN、Milvus 系列（架构/段管理/索引构建/混合查询）、Pinecone Serverless 等核心知识点，每篇文章遵循 8 段式模版，强调架构分析、参数调优与竞品对比。

## Requirements

### Requirement: VDB 系统工程实践深度文章

VDB 系统工程实践的每篇深度文章 SHALL 覆盖以下知识点：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-diskann` | DiskAnn | advanced |
| `db-milvus-arch` | Milvus 架构 | intermediate |
| `db-milvus-segment` | Milvus 段管理 | intermediate |
| `db-milvus-index` | Milvus 索引构建 | advanced |
| `db-milvus-search` | Milvus 混合查询 | advanced |
| `db-hybrid-search` | 混合查询 | advanced |
| `vdb-pinecone-serverless` | Pinecone Serverless | basic |
| `vdb-pinecone-storage` | Pinecone 存储索引 | intermediate |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：系统工程文章 SHALL 额外包含

- 架构图/流程图描述（ASCII 图或文字描述）
- 组件的职责边界和通信协议
- 核心参数及调优经验（如 nprobe、efSearch、M 等）
- 与竞品/其他方案的对比表

#### Scenario: Milvus 文章覆盖

- **WHEN** 用户阅读 Milvus 系列文章（4 篇）
- **THEN** 4 篇文章 SHALL 共同覆盖 Milvus 从架构→存储→索引→查询的完整链路

#### Scenario: 混合查询文章覆盖

- **WHEN** 用户阅读 `db-hybrid-search`
- **THEN** 文章 SHALL 覆盖 Pre-filtering / Post-filtering / Single-stage 三种策略的对比，以及 SPANN 的 SSD+内存混合方案
