# RAG 系统增强提案

> 状态: 草稿 | 创建日期: 2026-07-07

## 1. 概述

### 1.1 背景

当前 RAG 模块已完成基础实现，包括：
- 混合检索 (HNSW + BM25 + RRF)
- 多种分块策略 (Fixed/Recursive/Code)
- CLI 和 REST API 接口
- 简化版 Embedding 和 LLM 服务

### 1.2 问题陈述

现有实现的不足：
1. **Embedding 质量**: 使用随机向量，无法捕捉语义
2. **检索精度**: 缺乏重排序机制，Top-K 结果可能不理想
3. **多样性**: 检索结果可能重复，缺乏多样性策略
4. **关系推理**: 不支持多跳问答和关系推理
5. **可扩展性**: 架构不支持高级 RAG 模式

### 1.3 目标

在不破坏现有接口的前提下，增强 RAG 系统能力：
- Phase 1: 增强 RAG (检索质量优化)
- Phase 2: Graph RAG (知识图谱集成)

## 2. What Changes

### 2.1 Phase 1: 增强 RAG

#### 新增模块

| 模块 | 文件 | 说明 |
|------|------|------|
| Embedding 服务 | `rag/include/rag/embedding.h` | 真实模型推理接口 |
| BGE Embedding | `rag/src/rag/embedding/bge_embedding.cpp` | bge-m3 模型实现 |
| 重排序器 | `rag/include/rag/reranker.h` | Cross-Encoder 接口 |
| 轻量级重排序 | `rag/src/rag/reranker/lightweight_reranker.cpp` | 无外部依赖实现 |
| 增强检索器 | `rag/include/rag/enhanced_retriever.h` | 集成重排序 + MMR |
| 语义分块器 | `rag/include/rag/semantic_chunker.h` | 基于相似度的分块 |

#### 修改模块

| 模块 | 文件 | 变更 |
|------|------|------|
| RAG 引擎 | `rag/src/rag/engine/engine.cpp` | 支持切换检索器类型 |
| 配置 | `rag/include/rag/config.h` | 添加新配置项 |

### 2.2 Phase 2: Graph RAG

#### 新增模块

| 模块 | 文件 | 说明 |
|------|------|------|
| 知识图谱 | `rag/include/rag/knowledge_graph.h` | 图数据结构 |
| 实体提取器 | `rag/include/rag/entity_extractor.h` | LLM/规则提取 |
| 图检索器 | `rag/include/rag/graph_retriever.h` | 子图查询 |
| 图存储 | `rag/src/rag/knowledge_graph/graph_storage.cpp` | 基于 graph_engine |

#### 集成点

```
RAG Engine
    │
    ├── EnhancedRetriever (Phase 1)
    │       └── Reranker + MMR
    │
    └── GraphRetriever (Phase 2)
            ├── KnowledgeGraph
            │       └── graph_engine (现有)
            ├── EntityExtractor
            └── EmbeddingService
```

## 3. Scope

### 3.1 包含

- 真实模型集成 (bge-m3, bge-reranker)
- 重排序和多樣性增强
- 知识图谱构建和查询
- 与现有 graph_engine 集成

### 3.2 排除

- LLM 服务真实集成 (保持简化版)
- 多模态支持 (图像/音视频)
- Agentic RAG
- 分布式部署

### 3.3 向后兼容

- 现有 API 接口保持不变
- 配置项添加可选字段，不影响默认行为
- SimpleEmbeddingService 保留作为 fallback

## 4. 架构设计

### 4.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         RAG 系统增强架构                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                      应用层                                     │ │
│  │  CLI / REST API / SDK                                          │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                      RAG 引擎                                   │ │
│  │  ┌─────────────────────────────────────────────────────────┐   │ │
│  │  │              检索策略层                                   │   │ │
│  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │   │ │
│  │  │  │  Enhanced   │  │   Graph     │  │   Hybrid    │     │   │ │
│  │  │  │  Retriever  │  │  Retriever  │  │  Retriever  │     │   │ │
│  │  │  └─────────────┘  └─────────────┘  └─────────────┘     │   │ │
│  │  └─────────────────────────────────────────────────────────┘   │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│  ┌───────────────────────────┼───────────────────────────────────┐ │
│  │                      索引层              │                      │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │ │
│  │  │  HNSW 索引   │  │  BM25 索引   │  │  知识图谱    │           │ │
│  │  └─────────────┘  └─────────────┘  └──────┬──────┘           │ │
│  │                                            │                  │ │
│  └────────────────────────────────────────────┼──────────────────┘ │
│                                               ▼                     │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                      存储层                                     │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │ │
│  │  │   SQLite    │  │   向量文件   │  │ graph_engine│           │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘           │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                     │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                      模型层                                     │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │ │
│  │  │  BGE-M3     │  │ BGE-Reranker│  │   LLM       │           │ │
│  │  │  Embedding  │  │   重排序     │  │  (简化版)   │           │ │
│  │  └─────────────┘  └─────────────┘  └─────────────┘           │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 数据流

#### Phase 1: 增强检索

```
查询输入
    │
    ▼
┌─────────────┐
│   扩展查询   │ (可选: HyDE)
└──────┬──────┘
       │
       ▼
┌─────────────┐     ┌─────────────┐
│   HNSW 检索  │────▶│  BM25 检索   │
│  Top-50     │     │   Top-50    │
└──────┬──────┘     └──────┬──────┘
       │                   │
       └─────────┬─────────┘
                 ▼
         ┌─────────────┐
         │  RRF 融合   │ ──▶ 候选集 (50)
         └──────┬──────┘
                │
                ▼
         ┌─────────────┐
         │   重排序    │ ──▶ Cross-Encoder
         └──────┬──────┘
                │
                ▼
         ┌─────────────┐
         │  MMR 去重   │ (可选)
         └──────┬──────┘
                │
                ▼
         ┌─────────────┐
         │  Top-K 输出 │
         └─────────────┘
```

#### Phase 2: Graph RAG

```
查询输入
    │
    ▼
┌─────────────┐
│  实体链接   │ ──▶ 从知识图谱匹配查询中的实体
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  子图扩展   │ ──▶ BFS 扩展 N 跳邻居
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   图检索    │ ──▶ 基于向量相似度排序
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  混合融合   │ ──▶ Graph + Vector RRF
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Top-K 输出 │
└─────────────┘
```

## 5. 依赖关系

### 5.1 外部依赖

| 依赖 | 用途 | 版本 | 必需 |
|------|------|------|------|
| bge-m3 | Embedding 模型 | - | 可选 |
| bge-reranker | 重排序模型 | - | 可选 |
| transformers | 模型推理 | 4.x+ | 可选 |
| onnxruntime | ONNX 推理 | 1.15+ | 可选 |

### 5.2 内部依赖

```
rag-enhanced
├── rag (现有)
│       ├── RAGEngine
│       ├── VectorIndex (HNSW)
│       └── BM25Index
│
└── db/graph_engine (现有)
        └── 图存储
```

## 6. 配置变更

### 6.1 新增配置项

```yaml
# rag/config.yaml

retrieval:
  type: "enhanced"  # "basic", "enhanced", "graph"
  
  # 增强检索配置
  enhanced:
    enable_rerank: true
    reranker_type: "lightweight"  # "lightweight", "bge"
    reranker_model: "./models/bge-reranker"
    recall_top_k: 50
    return_top_k: 10
    
    # MMR 配置
    enable_mmr: false
    mmr_lambda: 0.7
    
    # 查询扩展
    enable_query_expansion: false
    expansion_method: "hyde"
  
  # Graph RAG 配置
  graph:
    enable_graph: true
    entity_types: ["PERSON", "ORG", "LOC", "CONCEPT"]
    max_hops: 2
    graph_weight: 0.5

embedding:
  type: "simple"  # "simple", "bge-m3"
  bge:
    model_path: "./models/bge-m3"
    device: "cpu"
    batch_size: 32
    dimension: 1024
```

## 7. 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 模型推理延迟 | 中 | 中 | 缓存、批处理、ONNX 量化 |
| 重排序性能下降 | 低 | 中 | 控制召回数量、异步处理 |
| 实体提取质量 | 中 | 高 | 置信度过滤、人工审核 |
| 图谱规模膨胀 | 高 | 中 | 限制数量、定期清理 |

## 8. 测试计划

### 8.1 单元测试

- Embedding 服务测试
- 重排序器测试
- 知识图谱 CRUD 测试
- 图检索器测试

### 8.2 集成测试

- 完整检索流程
- 文档入库到图谱构建
- 混合检索结果验证

### 8.3 效果评估

- 检索召回率对比
- 答案质量人工评估
- 性能基准测试

## 9. 里程碑

| 阶段 | 目标 | 周期 |
|------|------|------|
| M1 | 完成 Phase 1 核心 (Embedding + 重排序) | 2 周 |
| M2 | 完成 Phase 1 增强 (MMR + 查询扩展) | 1 周 |
| M3 | 完成 Phase 2 基础 (知识图谱) | 2 周 |
| M4 | 完成 Phase 2 检索 (Graph Retriever) | 2 周 |

## 10. 附录

### 10.1 参考资料

- [BGE-M3 模型](https://github.com/FlagOpen/FlagEmbedding)
- [BGE-Reranker](https://github.com/FlagOpen/FlagEmbedding)
- [GraphRAG 论文](https://arxiv.org/abs/2304.07193)
- [HyDE 论文](https://arxiv.org/abs/2212.10496)

### 10.2 相关文档

- `rag/README.md` - RAG 模块概览
- `rag/docs/06-retrieval-strategy.md` - 现有检索策略
- `docs/storage-architecture.md` - 存储架构
