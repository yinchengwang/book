# Pipeline 详细指南

> 版本：1.0
> 日期：2026-09-04

---

## 目录

1. [Naive RAG（基础）](#1-naive-rag基础）
2. [Advanced RAG（高级）](#2-advanced-rag高级）
3. [Hybrid RAG（混合）](#3-hybrid-rag混合)
4. [HyDE RAG（假设答案）](#4-hyde-rag假设答案)
5. [Graph RAG（知识图谱）](#5-graph-rag知识图谱)
6. [Corrective RAG（纠正）](#6-corrective-rag纠正)
7. [ReAct RAG（推理+行动）](#7-react-rag推理行动)
8. [Iterative RAG（迭代）](#8-iterative-rag迭代)
9. [Recursive RAG（递归分解）](#9-recursive-rag递归分解)

---

## 1. Naive RAG（基础）

### 1.1 流程图

```
┌─────────┐     ┌─────────────┐     ┌──────────┐     ┌─────────┐
│  Query  │ ──▶ │ Embedding   │ ──▶ │ Vector   │ ──▶ │   LLM   │
│ (用户)  │     │ (向量化)    │     │ (检索)   │     │ (生成)  │
└─────────┘     └─────────────┘     └──────────┘     └─────────┘
```

### 1.2 详细流程

1. **Query 输入**：用户输入自然语言查询
2. **Embedding**：使用 embedding 模型将查询向量化
3. **Vector 检索**：在向量数据库中执行 ANN 检索，返回 Top-K 相关文档
4. **Context 组装**：将检索结果组装为 prompt context
5. **LLM 生成**：调用 LLM 生成最终回答

### 1.3 适用场景

- 快速原型验证
- 简单问答系统
- 单文档检索场景
- 延迟敏感场景（最低延迟）

### 1.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/naive_pipeline.h>

modular_rag::NaivePipelineConfig config;
config.top_k = 5;
config.embedding_model = "nomic-embed-text";

auto pipeline = modular_rag::NaivePipelineFactory::create(config);

// YAML 配置
// rag:
//   pipeline: "naive"
// retrieval:
//   top_k: 5
```

### 1.5 优缺点

| 优点 | 缺点 |
|------|------|
| 实现简单 | 检索质量依赖 embedding 模型 |
| 延迟最低 | 无法处理复杂查询 |
| 资源消耗低 | 上下文窗口利用不充分 |

---

## 2. Advanced RAG（高级）

### 2.1 流程图

```
┌─────────┐  ┌────────────┐  ┌───────────┐  ┌───────┐  ┌─────────┐  ┌─────────┐
│  Query  │─▶│ QueryExp  │─▶│ Vector+   │─▶│  RRF  │─▶│ Rerank  │─▶│   LLM   │
│         │  │ (查询扩展) │  │ BM25      │  │(融合) │  │ (重排)  │  │ (生成)  │
└─────────┘  └────────────┘  └───────────┘  └───────┘  └─────────┘  └─────────┘
```

### 2.2 详细流程

1. **Query 输入**：用户输入自然语言查询
2. **Query Expansion**：对查询进行扩展（同义词扩展、伪文档生成）
3. **混合检索**：
   - Vector 检索：向量相似度检索
   - BM25 检索：关键词精确匹配
4. **RRF 融合**：使用 Reciprocal Rank Fusion 融合多路检索结果
5. **Rerank**：使用重排序模型进一步优化结果顺序
6. **Context 组装**：组装高质量 context
7. **LLM 生成**：调用 LLM 生成最终回答

### 2.3 适用场景

- 生产级文档问答
- 企业知识库检索
- 需要平衡精确性和语义理解
- 多文档综合问答

### 2.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/advanced_pipeline.h>

modular_rag::AdvancedPipelineConfig config;
config.top_k = 20;           // 初始检索数量
config.rerank_top_k = 5;    // 重排后保留数量
config.hybrid_alpha = 0.7;  // Vector/BM25 权重 (0-1)
config.enable_query_expansion = true;

auto pipeline = modular_rag::AdvancedPipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "advanced"
retrieval:
  top_k: 20
  rerank_top_k: 5
  hybrid_alpha: 0.7
  vector_weight: 0.7
  bm25_weight: 0.3
pre_retrieval:
  query_expansion: true
  expansion_terms: 5
post_retrieval:
  rerank: true
  rerank_model: "cross-encoder/ms-marco"
```

### 2.5 优缺点

| 优点 | 缺点 |
|------|------|
| 检索质量高 | 实现复杂 |
| 多路召回，取长补短 | 延迟较高 |
| 重排提升相关性 | 资源消耗较大 |

---

## 3. Hybrid RAG（混合）

### 3.1 流程图

```
┌─────────┐
│  Query  │
└────┬────┘
     │
     ▼
┌────────────┐  ┌────────────┐  ┌────────────┐
│   Vector   │  │    BM25    │  │   Graph    │
│  (向量检索) │  │ (关键词检索)│  │  (图检索)  │
└─────┬──────┘  └─────┬──────┘  └─────┬──────┘
      │                │                │
      └───────────┬────┴───────────────┘
                  ▼
            ┌─────────┐
            │   RRF   │
            │  (融合) │
            └────┬────┘
                 ▼
            ┌─────────┐
            │   LLM   │
            │  (生成) │
            └─────────┘
```

### 3.2 详细流程

1. **Query 输入**：用户输入查询
2. **三路并行检索**：
   - Vector 检索：语义相似度检索
   - BM25 检索：关键词精确匹配
   - Graph 检索：实体关系检索
3. **RRF 融合**：三路结果使用 RRF 算法融合
4. **Context 组装**：组装融合后的 context
5. **LLM 生成**：调用 LLM 生成回答

### 3.3 适用场景

- 综合知识库
- 需要关系推理的查询
- 学术文献检索
- 多模态知识管理

### 3.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/hybrid_pipeline.h>

modular_rag::HybridPipelineConfig config;
config.vector_top_k = 10;
config.bm25_top_k = 10;
config.graph_top_k = 10;
config.enable_entity_extraction = true;
config.fusion_method = "rrf";  // 支持 "rrf", "weighted", "coeff"

auto pipeline = modular_rag::HybridPipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "hybrid"
retrieval:
  vector_top_k: 10
  bm25_top_k: 10
  graph_top_k: 10
  fusion_method: "rrf"
  rrf_k: 60
graph:
  enabled: true
  entity_extraction: true
  relation_extraction: true
```

### 3.5 优缺点

| 优点 | 缺点 |
|------|------|
| 三路互补，覆盖全面 | 系统复杂度高 |
| 支持关系推理 | 延迟较高 |
| 适合复杂查询 | 资源消耗大 |

---

## 4. HyDE RAG（假设答案）

### 4.1 流程图

```
┌─────────┐     ┌───────────┐     ┌────────────┐     ┌───────────┐     ┌─────────┐
│  Query  │ ──▶ │    LLM    │ ──▶ │  假设答案   │ ──▶ │  向量检索  │ ──▶ │   LLM   │
│         │     │ (生成假设) │     │ (伪文档)   │     │ (用假设引导)│     │ (最终)  │
└─────────┘     └───────────┘     └────────────┘     └───────────┘     └─────────┘
```

### 4.2 详细流程

1. **Query 输入**：用户输入查询
2. **假设答案生成**：使用 LLM 生成一个"假设性答案"（可能是错误的，但语义相关）
3. **假设答案检索**：将假设答案向量化，执行检索
4. **真实文档检索**：基于假设答案的检索结果，提取真实相关文档
5. **最终生成**：使用真实文档 context 调用 LLM 生成最终回答

### 4.3 适用场景

- 模糊查询、长尾问题
- 语义空间复杂难以直接检索
- 需要扩展查询语义范围
- 探索性问答

### 4.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/hyde_pipeline.h>

modular_rag::HyDEPipelineConfig config;
config.hyde_model = "llama-2-7b-chat";
config.hyde_prompt_template = "请生成一个简短的回答，说明: {query}";
config.retrieval_top_k = 10;
config.enable_fallback = true;  // 如果假设答案检索失败，回退到直接检索

auto pipeline = modular_rag::HyDEPipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "hyde"
hyde:
  model: "llama-2-7b-chat"
  prompt_template: "请生成一个简短的回答，说明: {query}"
  temperature: 0.8
  max_tokens: 256
retrieval:
  top_k: 10
  fallback_to_direct: true
```

### 4.5 优缺点

| 优点 | 缺点 |
|------|------|
| 扩展查询语义 | 假设答案可能误导 |
| 适合长尾问题 | 额外 LLM 调用增加延迟 |
| 捕捉隐含语义 | 实现复杂度中等 |

---

## 5. Graph RAG（知识图谱）

### 5.1 流程图

```
┌─────────┐     ┌────────────┐     ┌──────────┐     ┌─────────┐     ┌─────────┐
│  Query  │ ──▶ │  实体提取   │ ──▶ │  图检索   │ ──▶ │  子图   │ ──▶ │   LLM   │
│         │     │  (NER)     │     │(Graph)   │     │ (构建)  │     │  (生成) │
└─────────┘     └────────────┘     └──────────┘     └─────────┘     └─────────┘
```

### 5.2 详细流程

1. **Query 输入**：用户输入查询
2. **实体提取（NER）**：从查询中提取命名实体
3. **实体链接**：将提取的实体链接到知识图谱中的节点
4. **图检索**：从知识图谱中检索相关的实体和关系
5. **子图构建**：将检索到的实体和关系构建为子图
6. **Context 组装**：将子图转换为文本描述作为 context
7. **LLM 生成**：调用 LLM 生成回答

### 5.3 适用场景

- 关系推理问答
- 人物/事件关系查询
- 家族谱系、社交网络
- 知识库关系分析

### 5.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/graph_pipeline.h>

modular_rag::GraphPipelineConfig config;
config.entity_types = {"PERSON", "ORG", "LOC", "EVENT"};
config.max_hops = 2;           // 最大跳数
config.include_attributes = true;
config.subgraph_size = 50;     // 子图节点数量上限

auto pipeline = modular_rag::GraphPipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "graph"
graph:
  enabled: true
  entity_types:
    - PERSON
    - ORG
    - LOC
    - EVENT
  max_hops: 2
  include_attributes: true
  subgraph_size: 50
ner:
  model: "bert-base-chinese-ner"
  confidence_threshold: 0.7
```

### 5.5 优缺点

| 优点 | 缺点 |
|------|------|
| 支持关系推理 | 需要额外构建知识图谱 |
| 可解释性强 | NER/实体链接可能出错 |
| 适合复杂关系查询 | 知识图谱维护成本高 |

---

## 6. Corrective RAG（纠正）

### 6.1 流程图

```
┌─────────┐     ┌──────────┐     ┌───────────┐     ┌────────────┐     ┌─────────┐
│  Query  │ ──▶ │   检索   │ ──▶ │ LLM判断   │ ──▶ │  质量评估  │ ──▶ │   LLM   │
│         │     │         │     │  质量     │     │  (决定)    │     │  (生成) │
└─────────┘     └──────────┘     └───────────┘     └─────┬──────┘     └─────────┘
                                                         │
                                    ┌────────────────────┴────────────────────┐
                                    │                                         │
                                    ▼                                         ▼
                             ┌──────────┐                              ┌──────────┐
                             │  质量差   │                              │  质量好   │
                             │ 纠正/重检 │                              │ 直接生成  │
                             └──────────┘                              └──────────┘
```

### 6.2 详细流程

1. **Query 输入**：用户输入查询
2. **初始检索**：执行基础检索获取候选文档
3. **质量评估**：使用 LLM 评估检索结果的质量
4. **决策分支**：
   - **质量好**：直接使用检索结果
   - **质量差**：执行纠正策略（查询改写、扩展检索源）
5. **重新检索（如需要）**：执行纠正后的检索
6. **最终生成**：调用 LLM 生成回答

### 6.3 适用场景

- 高精度问答系统
- 事实核查场景
- 医疗、法律等专业领域
- 防止幻觉生成

### 6.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/corrective_pipeline.h>

modular_rag::CorrectivePipelineConfig config;
config.quality_threshold = 0.6;
config.max_retries = 2;
config.correction_strategies = {
    modular_rag::CorrectionStrategy::QUERY_REWRITE,
    modular_rag::CorrectionStrategy::EXPAND_SOURCE
};

auto pipeline = modular_rag::CorrectivePipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "corrective"
corrective:
  quality_threshold: 0.6
  max_retries: 2
  strategies:
    - query_rewrite
    - expand_source
    - use_external_knowledge
```

### 6.5 优缺点

| 优点 | 缺点 |
|------|------|
| 提高答案准确性 | 延迟较高（多次评估） |
| 减少幻觉生成 | 实现复杂度高 |
| 适合高风险场景 | 资源消耗大 |

---

## 7. ReAct RAG（推理+行动）

### 7.1 流程图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ReAct 循环                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌─────────┐     ┌───────────┐     ┌──────────┐     ┌──────────┐ │
│   │  Thought │ ──▶ │  Action   │ ──▶ │  Observe │ ──▶ │  Next    │ │
│   │ (推理)   │     │  (行动)   │     │  (观察)  │     │ (决策)   │ │
│   └─────────┘     └─────┬─────┘     └──────────┘     └────┬─────┘ │
│                          │                                │       │
│                          ▼                                │       │
│                    ┌──────────┐                           │       │
│                    │ Retrieve │                           │       │
│                    │  (检索)   │                           │       │
│                    └──────────┘                           │       │
│                                                             ▼       │
│                                                      ┌──────────┐  │
│                                                      │  Finish  │  │
│                                                      │ (结束)   │  │
│                                                      └──────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.2 详细流程

1. **Query 输入**：用户输入查询
2. **ReAct 循环**：
   - **Thought**：LLM 分析当前状态，决定下一步行动
   - **Action**：执行行动（如检索、查表、计算）
   - **Observation**：观察行动结果
   - **Next**：基于观察结果决定继续或结束
3. **多步推理**：循环执行直到得到满意答案或达到最大步数
4. **最终生成**：基于所有中间结果生成最终回答

### 7.3 适用场景

- 复杂多步推理
- 需要主动检索的场景
- 智能问答助手
- 需要规划能力的任务

### 7.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/react_pipeline.h>

modular_rag::ReActPipelineConfig config;
config.max_steps = 10;
config.max_tokens_per_step = 256;
config.tools = {
    modular_rag::ToolRegistry::get("retrieve"),
    modular_rag::ToolRegistry::get("knowledge_graph"),
    modular_rag::ToolRegistry::get("calculator")
};
config.early_stopping = true;

auto pipeline = modular_rag::ReActPipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "react"
react:
  max_steps: 10
  max_tokens_per_step: 256
  early_stopping: true
  timeout_ms: 30000
tools:
  - name: retrieve
    description: "检索文档"
    enabled: true
  - name: knowledge_graph
    description: "查询知识图谱"
    enabled: true
  - name: calculator
    description: "执行计算"
    enabled: true
```

### 7.5 优缺点

| 优点 | 缺点 |
|------|------|
| 支持复杂推理 | 实现复杂度高 |
| 可解释性强 | 延迟可能很高 |
| 动态规划 | 需要设计 Tool 系统 |

---

## 8. Iterative RAG（迭代）

### 8.1 流程图

```
┌─────────┐     ┌──────────┐     ┌───────────┐     ┌────────────┐
│  Query  │ ──▶ │   检索    │ ──▶ │ LLM评估   │ ──▶ │  满意？    │
│         │     │          │     │  质量     │     │            │
└─────────┘     └──────────┘     └───────────┘     └─────┬──────┘
                                                         │
                                    ┌────────────────────┴────────────────────┐
                                    │                                         │
                                    ▼                                         ▼
                             ┌──────────┐                              ┌──────────┐
                             │   否     │                              │   是     │
                             │ 改写Query│                              │   结束   │
                             └────┬─────┘                              └────┬─────┘
                                  │                                         │
                                  ▼                                         ▼
                             ┌──────────┐                            ┌──────────┐
                             │  重新    │                            │   LLM    │
                             │  检索    │                            │  生成    │
                             └──────────┘                            └──────────┘
```

### 8.2 详细流程

1. **Query 输入**：用户输入查询
2. **初始检索**：执行基础检索
3. **质量评估**：LLM 评估当前检索结果是否满足查询需求
4. **迭代决策**：
   - **不满意**：改写查询，重新检索（回到步骤 2）
   - **满意**：进入生成阶段
5. **最终生成**：使用最佳检索结果生成回答

### 8.3 适用场景

- 复杂问题需要多角度检索
- 信息不完整需要补充
- 开放式问答
- 研究性查询

### 8.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/iterative_pipeline.h>

modular_rag::IterativePipelineConfig config;
config.max_iterations = 3;
config.satisfaction_threshold = 0.7;
config.query_rewrite_prompt = "请改写以下查询，使其更加清晰和具体: {query}";

auto pipeline = modular_rag::IterativePipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "iterative"
iterative:
  max_iterations: 3
  satisfaction_threshold: 0.7
  query_rewrite_prompt: "请改写以下查询，使其更加清晰和具体: {query}"
  evaluation_prompt: "评估以下检索结果是否满足查询需求: {query}"
```

### 8.5 优缺点

| 优点 | 缺点 |
|------|------|
| 提高检索召回 | 延迟不确定 |
| 适应复杂查询 | 可能无限迭代 |
| 动态调整查询 | 资源消耗波动大 |

---

## 9. Recursive RAG（递归分解）

### 9.1 流程图

```
┌─────────┐     ┌────────────┐     ┌─────────────┐     ┌────────────┐
│  Query  │ ──▶ │   问题     │ ──▶ │   子问题    │ ──▶ │   递归     │
│         │     │   分解     │     │   列表      │     │   求解     │
└─────────┘     └────────────┘     └─────────────┘     └─────┬──────┘
                                                              │
                            ┌─────────────────────────────────┴──────────┐
                            ▼                                                    ▼
                     ┌────────────┐                                     ┌────────────┐
                     │  子问题1   │                                     │  子问题N   │
                     │   RAG      │         ...                        │   RAG      │
                     └─────┬──────┘                                     └─────┬──────┘
                           │                                                   │
                           ▼                                                   ▼
                     ┌────────────┐                                     ┌────────────┐
                     │  子答案1   │                                     │  子答案N   │
                     └────────────┘                                     └────────────┘
                            │                                                   │
                            └───────────────────┬───────────────────────────────┘
                                                ▼
                                         ┌────────────┐
                                         │   答案     │
                                         │   合并     │
                                         └─────┬──────┘
                                               ▼
                                         ┌────────────┐
                                         │    LLM     │
                                         │   生成     │
                                         └────────────┘
```

### 9.2 详细流程

1. **Query 输入**：用户输入复杂查询
2. **问题分解**：使用 LLM 将问题分解为多个子问题
3. **递归求解**：对每个子问题递归调用 RAG（可以是相同或不同 Pipeline）
4. **答案合并**：将所有子问题的答案合并
5. **最终生成**：使用合并的答案调用 LLM 生成最终回答

### 9.3 适用场景

- 复杂问题分解
- 综述类问答
- 需要综合多方面信息
- 教学/培训场景

### 9.4 配置示例

```cpp
// C++ API
#include <modular_rag/pipeline/recursive_pipeline.h>

modular_rag::RecursivePipelineConfig config;
config.max_subproblems = 5;
config.subproblem_pipeline = "naive";  // 子问题使用 Naive RAG
config.enable_parallel = true;         // 并行求解子问题
config.merging_strategy = "llm_merge"; // LLM 合并答案

auto pipeline = modular_rag::RecursivePipelineFactory::create(config);
```

```yaml
# YAML 配置示例
rag:
  pipeline: "recursive"
recursive:
  max_subproblems: 5
  subproblem_pipeline: "naive"
  enable_parallel: true
  merging_strategy: "llm_merge"
  decomposition_prompt: "将以下问题分解为多个独立子问题: {query}"
  merging_prompt: "将以下子问题的答案合并为一个完整回答: {answers}"
```

### 9.5 优缺点

| 优点 | 缺点 |
|------|------|
| 处理复杂问题 | 延迟较高 |
| 分解后更易处理 | 子问题可能相关 |
| 可并行求解 | 合并可能丢失细节 |

---

## 10. Pipeline 选择指南

| 场景 | 推荐 Pipeline | 原因 |
|------|-------------|------|
| 快速原型 | Naive RAG | 实现简单，延迟最低 |
| 生产问答 | Advanced RAG | 质量稳定，延迟可接受 |
| 关系推理 | Hybrid/Graph RAG | 支持图检索和关系推理 |
| 长尾问题 | HyDE RAG | 扩展语义空间 |
| 高精度场景 | Corrective RAG | 质量评估和纠正 |
| 复杂推理 | ReAct RAG | 多步推理和规划 |
| 多角度查询 | Iterative RAG | 动态调整查询 |
| 问题分解 | Recursive RAG | 分解复杂问题 |

---

## 11. 相关文档

- [概述](./01-overview.md) - 项目概述、架构、9 种 Pipeline 总览
- [Agent 框架指南](./03-agent-guide.md) - Tool 系统、Memory 系统、ReAct 循环
- [API 参考文档](./04-api-reference.md) - REST API 端点、CLI 命令
