# Modular RAG 框架概述

> 版本：1.0
> 日期：2026-09-04

---

## 1. 项目概述

Modular RAG 是一个**生产级模块化检索增强生成（RAG）框架**，使用纯 C++ 实现，集成内置 llama.cpp 推理引擎，支持 9 种 RAG Pipeline 类型，并包含自研 Agent 框架。

### 1.1 核心特性

| 特性 | 说明 |
|------|------|
| **9 种 Pipeline** | Naive、Advanced、Hybrid、HyDE、Graph、Corrective、ReAct、Iterative、Recursive |
| **自研 Agent 框架** | 支持 Tool、Memory、ReAct 循环 |
| **内置 LLM** | llama.cpp GGML/GGUF 量化推理 |
| **自研数据库** | D-code-book DB（KV、Vector、Doc、Graph 存储） |
| **模块化设计** | 分块器、检索器、重排序、预/后处理均可独立配置 |

---

## 2. 架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Modular RAG 框架                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              Agent Orchestrator（Agent 编排层）              │   │
│  │    • LLM 驱动的动态流程编排                                  │   │
│  │    • Tool Registry & Executor                                │   │
│  │    • Memory & Context Manager                                │   │
│  │    • Planning & ReAct Loop                                   │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              ▼                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              RAG Pipeline Layer（RAG 流程层）                │   │
│  │                                                             │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                 │   │
│  │  │ Pre-Ret  │→ │ Retrieve │→ │ Post-Ret│                 │   │
│  │  │ (检索前)  │  │ (检索)   │  │ (检索后) │                 │   │
│  │  └──────────┘  └──────────┘  └──────────┘                 │   │
│  │       │              │              │                       │   │
│  │  [QueryExp]    [Vector]       [Rerank]                     │   │
│  │  [HyDE]        [BM25]        [Correct]                    │   │
│  │  [Decompose]   [Graph]       [Context]                   │   │
│  │  [QueryRewrite][Hybrid]       [Filter]                     │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              ▼                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              Shared Module Layer（共享模块层）              │   │
│  │                                                             │   │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐         │   │
│  │  │  Chunkers  │  │ Embedders  │  │   LLM      │         │   │
│  │  │  (4种)    │  │  (nomic)   │  │(llama.cpp) │         │   │
│  │  └────────────┘  └────────────┘  └────────────┘         │   │
│  │                                                             │   │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐         │   │
│  │  │  Parsers   │  │  Storage   │  │   Utils    │         │   │
│  │  │  (MD/PDF)  │  │ (自研DB)  │  │ (Config)   │         │   │
│  │  └────────────┘  └────────────┘  └────────────┘         │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.1 分层说明

| 层级 | 职责 |
|------|------|
| **Agent Orchestrator** | LLM 驱动的动态流程编排、Tool 执行、Memory 管理、ReAct 循环 |
| **RAG Pipeline Layer** | 检索前处理（QueryExp、HyDE、Decompose、Rewrite）、检索（Vector、BM25、Graph、Hybrid）、检索后处理（Rerank、Correct、Context、Filter） |
| **Shared Module Layer** | Chunkers（4种）、Embedders（nomic）、LLM（llama.cpp）、Parsers（MD/PDF）、Storage（自研DB） |

---

## 3. 9 种 Pipeline 总览表格

| # | Pipeline | 描述 | 核心流程 | 适用场景 |
|---|----------|------|----------|----------|
| 1 | **Naive RAG** | 基础检索生成 | Query → Vector检索 → Context → LLM | 简单问答、原型验证 |
| 2 | **Advanced RAG** | 高级检索增强 | Query → QueryExp → Vector+BM25 → RRF → Rerank → LLM | 生产级问答、文档检索 |
| 3 | **Hybrid RAG** | 混合多路检索 | Query → Vector + BM25 + Graph → RRF融合 → LLM | 综合知识库、关系查询 |
| 4 | **HyDE RAG** | 假设答案引导 | Query → LLM生成假设答案 → 用假设答案检索 → LLM最终生成 | 模糊查询、长尾问题 |
| 5 | **Graph RAG** | 知识图谱增强 | Query → 实体提取 → 图检索 → 子图 → Context → LLM | 关系推理、复杂问答 |
| 6 | **Corrective RAG** | 检索结果纠正 | Query → 检索 → LLM判断质量 → 质量差→纠正/重检 → LLM | 高精度问答、事实核查 |
| 7 | **ReAct RAG** | 推理+行动 | Query → LLM推理 → 决定行动 → 执行检索 → 观察结果 → ... 循环 | 复杂推理、多步问答 |
| 8 | **Iterative RAG** | 迭代改进检索 | Query → 检索 → LLM评估 → 不满意→改写Query → 再检索 → ... | 复杂问题、多角度检索 |
| 9 | **Recursive RAG** | 递归问题分解 | Query → 分解为子问题 → 各子问题RAG → 合并答案 → LLM | 复杂问题分解、综述生成 |

---

## 4. 快速开始指南

### 4.1 环境要求

| 组件 | 要求 |
|------|------|
| 编译器 | GCC 11+ / Clang 15+ / MSVC 2022+ |
| CMake | 3.24+ |
| 内存 | 最低 8GB，推荐 16GB+ |
| 磁盘 | 10GB+ 可用空间 |

### 4.2 编译构建

```bash
# 克隆项目
git clone https://github.com/your-org/modular-rag.git
cd modular-rag

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . -j$(nproc)

# 安装
cmake --install .
```

### 4.3 基础使用示例

#### 4.3.1 C++ API 使用

```cpp
#include <modular_rag/modular_rag.h>

int main() {
    // 初始化 RAG 引擎
    modular_rag::RAGEngine engine;

    // 加载配置
    engine.init("config/default.yaml");

    // 构建索引（文档列表）
    std::vector<modular_rag::Document> docs = {
        {"doc1", "C++ 是一种高性能编程语言。"},
        {"doc2", "Python 适合快速开发和数据科学。"},
        {"doc3", "Rust 注重内存安全和并发性能。"}
    };
    engine.build_index(docs);

    // 创建 Naive RAG Pipeline
    auto pipeline = modular_rag::PipelineFactory::create("naive");

    // 执行查询
    modular_rag::Query query{"什么是 C++？"};
    auto result = pipeline->query(query);

    // 输出结果
    std::cout << "Answer: " << result.answer << std::endl;
    std::cout << "Retrieval time: " << result.retrieval_time_ms << "ms" << std::endl;

    return 0;
}
```

#### 4.3.2 配置文件示例（YAML）

```yaml
# config/default.yaml
rag:
  pipeline: "advanced"  # 选择 Pipeline 类型

llm:
  model_path: "./models/llama-2-7b-chat.gguf"
  n_ctx: 4096
  n_threads: 4
  temperature: 0.7

embedding:
  model: "nomic-embed-text"
  dimension: 768
  batch_size: 32

retrieval:
  top_k: 10
  rerank_top_k: 5
  hybrid_alpha: 0.7  # Vector/BM25 权重比例

storage:
  db_path: "./data/rag.db"
  vector_index: "hnsw"
  bm25_enabled: true
```

### 4.4 REST API 使用

```bash
# 启动服务
./build/bin/modular-rag-server --config config/default.yaml

# 构建索引
curl -X POST http://localhost:8080/api/v1/index \
  -H "Content-Type: application/json" \
  -d '{"documents":[{"id":"1","content":"C++ 是一种编程语言"},{"id":"2","content":"Python 是脚本语言"}]}'

# 执行查询
curl -X POST http://localhost:8080/api/v1/query \
  -H "Content-Type: application/json" \
  -d '{"text":"什么是 C++？","pipeline":"naive"}'
```

### 4.5 CLI 工具使用

```bash
# 索引文档
modular-rag index --input ./docs --output ./data/index

# 执行查询
modular-rag query --text "什么是 C++？" --pipeline naive

# 列出可用 Pipeline
modular-rag list-pipelines

# 查看系统状态
modular-rag status
```

### 4.6 选择合适的 Pipeline

| 场景 | 推荐 Pipeline |
|------|---------------|
| 快速原型验证 | Naive RAG |
| 生产级文档问答 | Advanced RAG |
| 需要关系推理 | Hybrid RAG / Graph RAG |
| 模糊查询、长尾问题 | HyDE RAG |
| 高精度问答、事实核查 | Corrective RAG |
| 复杂多步推理 | ReAct RAG / Iterative RAG |
| 复杂问题分解、综述 | Recursive RAG |

---

## 5. 目录结构

```
engineering/rag/modular/
├── include/modular_rag/
│   ├── rag.h                    # 主头文件
│   ├── types.h                  # 基础类型定义
│   ├── config.h                 # 配置结构
│   ├── chunker/                 # 分块器模块
│   ├── retriever/               # 检索器模块
│   ├── reranker/                # 重排序模块
│   ├── pre_retrieval/           # 检索前处理
│   ├── post_retrieval/          # 检索后处理
│   ├── storage/                 # 自研数据库存储
│   ├── llm/                     # LLM 服务
│   ├── agent/                   # Agent 框架
│   ├── pipeline/                # Pipeline 层
│   └── api/                     # API 层
├── src/                         # 实现源文件
├── test/                        # 单元测试
└── docs/                        # 文档
```

---

## 6. 后续内容

- [Pipeline 详细指南](./02-pipeline-guide.md) - 9 种 Pipeline 的详细介绍、流程图、配置示例
- [Agent 框架指南](./03-agent-guide.md) - Tool 系统、Memory 系统、ReAct 循环说明
- [API 参考文档](./04-api-reference.md) - REST API 端点、CLI 命令、配置项说明
