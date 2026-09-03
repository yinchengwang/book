# RAG 系统设计

> 本地 RAG（检索增强生成）系统，专为 C/C++ 算法与数据结构练习项目设计。

## 系统概览

```
┌─────────────────────────────────────────────────────────────┐
│                     RAG 系统                                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  入库流程：                                                  │
│  文档源 ──→ 解析 ──→ 分块 ──→ Embedding ──→ 向量存储       │
│                        │                                    │
│                        └──→ 实体提取 ──→ 知识图谱           │
│                                                             │
│  查询流程：                                                  │
│  用户 ──→ 实体链接 ──→ Graph/Vector 检索 ──→ LLM 生成      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 核心特性

- **多格式支持**：Markdown、PDF、DOCX、代码（C/C++、Python、Java）等
- **本地模型**：支持 Ollama Embedding (nomic-embed-text) + LLM (llama3.2/qwen2.5)
- **Ollama 集成**：通过 HTTP API 调用本地模型，无需额外依赖
- **混合检索**：HNSW 向量检索 + BM25 全文检索 + RRF 融合
- **智能分块**：递归字符分块、语义分块、代码感知分块
- **Graph RAG**：知识图谱 + 实体提取 + 子图检索
- **RAGAS 评估**：完整的检索和生成质量评估框架
- **灵活接口**：CLI 命令行 + REST API

## 文档结构

```
rag/
├── README.md                    # 本文档
├── docs/
│   ├── 01-data-layer.md        # 数据层设计
│   ├── 02-api-design.md        # 接口设计
│   ├── 03-config-design.md     # 配置设计
│   ├── 04-model-service.md     # 模型服务设计
│   ├── 05-chunking-strategy.md # 分块策略设计
│   ├── 06-retrieval-strategy.md# 检索策略设计
│   ├── 07-prompt-engineering.md# Prompt 工程设计
│   ├── 08-observability.md     # 可观测性设计
│   ├── 09-error-handling.md    # 错误处理设计
│   ├── 10-engineering.md       # 工程化设计
│   └── uml-design.md           # UML 设计汇总
└── diagrams/                    # UML 图
    ├── system-overview.excalidraw.json
    ├── architecture.excalidraw.json
    ├── data-flow.excalidraw.json
    ├── use-case.excalidraw.json
    ├── deployment.excalidraw.json
    ├── component.excalidraw.json
    ├── class-diagram.excalidraw.json
    ├── object-diagram.excalidraw.json
    ├── sequence-indexing.excalidraw.json
    ├── sequence-query.excalidraw.json
    ├── activity-diagram.excalidraw.json
    ├── state-diagram.excalidraw.json
    └── collaboration-diagram.excalidraw.json
```

## 设计文档清单

| 序号 | 文档 | 内容 |
|------|------|------|
| 1 | [数据层设计](docs/01-data-layer.md) | SQLite Schema、索引格式、数据流 |
| 2 | [接口设计](docs/02-api-design.md) | CLI 命令、REST API |
| 3 | [配置设计](docs/03-config-design.md) | rag-config.yaml 完整配置 |
| 4 | [模型服务设计](docs/04-model-service.md) | Embedding、LLM 服务集成 |
| 5 | [分块策略设计](docs/05-chunking-strategy.md) | 固定/递归/语义/代码分块 |
| 6 | [检索策略设计](docs/06-retrieval-strategy.md) | HNSW、BM25、混合检索、RRF |
| 7 | [Prompt 工程设计](docs/07-prompt-engineering.md) | 系统提示词、用户模板 |
| 8 | [可观测性设计](docs/08-observability.md) | 日志、指标、健康检查 |
| 9 | [错误处理设计](docs/09-error-handling.md) | 错误码、异常、重试、降级 |
| 10 | [工程化设计](docs/10-engineering.md) | CMake、测试、CI/CD |

## UML 图清单

| 图类型 | 文件 |
|--------|------|
| 系统总览图 | [system-overview.excalidraw.json](diagrams/system-overview.excalidraw.json) |
| 架构图 | [architecture.excalidraw.json](diagrams/architecture.excalidraw.json) |
| 数据流图 | [data-flow.excalidraw.json](diagrams/data-flow.excalidraw.json) |
| 用例图 | [use-case.excalidraw.json](diagrams/use-case.excalidraw.json) |
| 部署图 | [deployment.excalidraw.json](diagrams/deployment.excalidraw.json) |
| 组件图 | [component.excalidraw.json](diagrams/component.excalidraw.json) |
| 类图 | [class-diagram.excalidraw.json](diagrams/class-diagram.excalidraw.json) |
| 对象图 | [object-diagram.excalidraw.json](diagrams/object-diagram.excalidraw.json) |
| 顺序图（入库） | [sequence-indexing.excalidraw.json](diagrams/sequence-indexing.excalidraw.json) |
| 顺序图（查询） | [sequence-query.excalidraw.json](diagrams/sequence-query.excalidraw.json) |
| 活动图 | [activity-diagram.excalidraw.json](diagrams/activity-diagram.excalidraw.json) |
| 状态图 | [state-diagram.excalidraw.json](diagrams/state-diagram.excalidraw.json) |
| 协作图 | [collaboration-diagram.excalidraw.json](diagrams/collaboration-diagram.excalidraw.json) |

## 技术栈

### 核心依赖

| 组件 | 技术选型 |
|------|----------|
| 编程语言 | C++17 |
| 构建系统 | CMake 3.20+ |
| JSON 解析 | nlohmann-json |
| 数据库 | SQLite3 |
| Embedding | sentence-transformers (bge-base-zh-v1.5) |
| LLM 推理 | llama.cpp (Qwen2.5-7B) |
| 向量索引 | hnswlib |
| 距离计算 | simsimd |

### 可选依赖

| 组件 | 技术选型 |
|------|----------|
| HTTP 服务 | httplib |
| HTTP 客户端 | cpr |
| 日志 | spdlog |
| 测试 | GoogleTest |

## 快速开始

### 1. 下载模型

```bash
# Embedding 模型
git clone https://huggingface.co/baai/bge-base-zh-v1.5 ./models/bge-base-zh-v1.5

# LLM 模型 (Q4 量化)
# 从 HuggingFace 下载 Qwen2.5-7B-Instruct-GGUF
```

### 2. 配置

```yaml
# rag-config.yaml
version: "1.0.0"

data_sources:
  - path: "./docs"
    patterns: ["**/*.md"]

embedding:
  model:
    path: "./models/bge-base-zh-v1.5"

llm:
  model:
    path: "./models/Qwen2.5-7B-Q4_K_M.gguf"
```

### 3. 构建与使用

```bash
# 构建
mkdir build && cd build
cmake .. && cmake --build .

# 构建索引
./bin/rag_cli index build --path ./docs

# 问答
./bin/rag_cli query "RAG 系统如何构建索引？"
```

## 架构设计

### 三层架构

```
┌─────────────────────────────────────────────────────────────┐
│                      表现层                                  │
│                 CLI / REST API                              │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                     业务逻辑层                               │
│    索引构建器  │  检索器  │  LLM 生成器                     │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                       数据层                                 │
│       向量存储 (HNSW)  │  文档存储  │  SQLite               │
└─────────────────────────────────────────────────────────────┘
```

### 核心组件

| 组件 | 职责 |
|------|------|
| `Parser` | 解析 Markdown、PDF、代码等文档 |
| `Chunker` | 将文档分割为小块 |
| `EmbeddingService` | 将文本转为向量 |
| `EntityExtractor` | 从文本中提取实体和关系 |
| `KnowledgeGraph` | 存储和管理知识图谱 |
| `GraphRetriever` | 基于知识图谱的检索 |
| `VectorStore` | 存储和检索向量 |
| `Retriever` | 混合检索（HNSW + BM25 + RRF） |
| `LLMService` | 生成答案 |
| `RAGEngine` | 协调上述组件 |

## Graph RAG

Graph RAG 通过知识图谱增强检索效果，特别适合多跳问答场景。

### 架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Graph RAG 架构                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  入库阶段：                                                  │
│  Chunk ──→ EntityExtractor ──→ KnowledgeGraph              │
│             (实体提取)        (知识图谱存储)                 │
│                                                             │
│  查询阶段：                                                  │
│  Query ──→ 实体链接 ──→ 子图扩展 ──→ RRF 融合 ──→ 结果     │
│            (link entities)  (BFS expand)  (混合排序)        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 核心数据结构

```cpp
// 实体类型
enum class EntityType {
    PERSON,           // 人物
    ORGANIZATION,     // 组织/公司
    LOCATION,         // 地点
    TECHNOLOGY,       // 技术
    CONCEPT,          // 概念
    PRODUCT,          // 产品
    TIME,             // 时间
    EVENT,            // 事件
    CUSTOM            // 自定义
};

// 实体
struct KGEntity {
    std::string id;
    std::string name;
    EntityType type;
    std::string description;
    std::vector<std::string> aliases;  // 别名
    float confidence = 1.0f;
};

// 关系
struct KGRelation {
    std::string id;
    std::string source_id;
    std::string target_id;
    std::string type;      // WORKS_AT, CREATES, LOCATED_IN, USES, RELATED_TO, KNOWS
    float weight = 1.0f;
};
```

### 配置示例

```yaml
# rag-config.yaml
graph:
  enabled: true                    # 启用 Graph RAG
  extractor_type: "rule"           # 提取器类型: rule / llm
  max_hops: 2                      # 子图扩展最大跳数
  incremental_update: true         # 增量更新
  kg_storage_path: "kg.json"       # 图谱存储路径
  entity_confidence_threshold: 0.5 # 实体置信度阈值
```

### 使用示例

```cpp
#include "rag/engine.h"

// 创建引擎
EngineConfig config;
config.config.graph.enabled = true;
config.config.graph.max_hops = 2;
auto engine = create_engine(config);

// 构建索引和知识图谱
engine->build_index();
engine->build_knowledge_graph();

// Graph 检索
auto result = engine->graph_retrieve("张三和李四有什么关系？", 10);

// 查看结果
for (const auto& entity : result.matched_entities) {
    std::cout << entity.name << " (" << entity.type << ")\n";
}

// 查看子图
for (const auto& rel : result.subgraph.relations) {
    std::cout << rel.source_id << " --[" << rel.type << "]--> "
              << rel.target_id << "\n";
}
```

### 检索器类型

| 类型 | 说明 |
|------|------|
| `BaseGraphRetriever` | 基础 Graph 检索，实体链接 + BFS 子图扩展 |
| `HybridGraphRetriever` | Graph + Vector 混合检索，RRF 融合 |

### 实体提取器类型

| 类型 | 说明 |
|------|------|
| `RuleBasedExtractor` | 基于正则表达式的规则提取 |
| `LLMBasedExtractor` | 基于 LLM 的智能提取 |

## 性能指标

| 指标 | 目标 |
|------|------|
| 索引速度 | 100 docs/s |
| 查询延迟 (P50) | < 500ms |
| 查询延迟 (P99) | < 2s |
| 向量检索吞吐量 | 1000 QPS |
| 内存占用 | < 16GB |

## 扩展阅读

- [完整 UML 设计汇总](docs/uml-design.md) - 所有 UML 图的详细说明
- [系统架构详解](diagrams/architecture.excalidraw.json) - 查看架构图
- [数据流图](diagrams/data-flow.excalidraw.json) - 理解数据处理流程

---

*文档版本：2.0.0*
*最后更新：2026-07-07*
