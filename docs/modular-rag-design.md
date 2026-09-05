# Modular RAG 框架设计方案

> 版本：1.0
> 日期：2026-09-04
> 状态：设计完成，待实现

---

## 目标

构建一个**生产级**的模块化 RAG 框架，使用**纯 C++** 实现，集成**内置 llama.cpp**，支持 **9 种 RAG 类型**，包含**自研 Agent 框架**。

### 技术选型

| 维度 | 选择 |
|------|------|
| 技术栈 | 纯 C++ |
| LLM | 内置 llama.cpp |
| 数据库 | 自研数据库引擎（D-code-book DB） |
| Agent | 自研完整框架 |
| 覆盖范围 | 9 种 RAG 类型（全部） |

---

## 第一部分：整体架构

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

---

## 第二部分：9 种 RAG 类型映射

```
┌───────────────────────────────────────────────────────────────────────────┐
│                    9 种 RAG 类型 ──→ 模块组合                              │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  1️⃣ Naive RAG（基础）                                                    │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → Vector检索 → Context → LLM                             │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  2️⃣ Advanced RAG（高级）                                                │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → QueryExp → Vector+BM25 → RRF → Rerank → LLM          │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  3️⃣ Hybrid RAG（混合）                                                  │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → Vector + BM25 + Graph → RRF融合 → LLM                 │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  4️⃣ HyDE RAG（假设答案）                                                │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → LLM生成假设答案 → 用假设答案检索 → LLM最终生成          │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  5️⃣ Graph RAG（知识图谱）                                               │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → 实体提取 → 图检索 → 子图 → Context → LLM               │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  6️⃣ Corrective RAG（纠正）                                              │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → 检索 → LLM判断质量 → 质量差→纠正/重检 → LLM           │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  7️⃣ ReAct RAG（推理+行动）                                              │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → LLM推理 → 决定行动 → 执行检索 → 观察结果 → ... 循环    │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  8️⃣ Iterative RAG（迭代）                                               │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → 检索 → LLM评估 → 不满意 → 改写Query → 再检索 → ...      │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
│  9️⃣ Recursive RAG（递归分解）                                           │
│  ┌─────────────────────────────────────────────────────────────────┐     │
│  │  Query → 分解为子问题 → 各子问题RAG → 合并答案 → LLM            │     │
│  └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 第三部分：数据层设计

```
┌─────────────────────────────────────────────────────────────────────┐
│                    自研数据库引擎（D-code-book DB）                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Storage Engine Layer                       │   │
│  │                                                             │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │   │
│  │  │ KV Store │  │VectorStore│  │ DocStore │  │ GraphStore│   │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘    │   │
│  │                                                             │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                 │   │
│  │  │ Buffer   │  │ B+Tree   │  │  HNSW   │                 │   │
│  │  │ Pool     │  │ Index    │  │ Index   │                 │   │
│  │  └──────────┘  └──────────┘  └──────────┘                 │   │
│  │                                                             │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐                 │   │
│  │  │   WAL    │  │ Catalog  │  │  BM25   │                 │   │
│  │  └──────────┘  └──────────┘  └──────────┘                 │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    RAG Data Layer                            │   │
│  │                                                             │   │
│  │  Document Table | Chunk Table | Vector Index | BM25 Index   │   │
│  │  Knowledge Graph (entity/relation)                          │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 第四部分：目录结构

```
engineering/rag/modular/
├── include/
│   └── modular_rag/
│       ├── rag.h                    # 主头文件
│       ├── types.h                  # 基础类型定义
│       ├── config.h                 # 配置结构
│       │
│       ├── chunker/                 # 分块器模块
│       │   ├── chunker.h
│       │   └── chunker_factory.h
│       │
│       ├── retriever/               # 检索器模块
│       │   ├── retriever.h
│       │   ├── vector_retriever.h
│       │   ├── bm25_retriever.h
│       │   ├── graph_retriever.h
│       │   └── hybrid_retriever.h
│       │
│       ├── reranker/                # 重排序模块
│       │   └── reranker.h
│       │
│       ├── pre_retrieval/           # 检索前处理
│       │   ├── query_expander.h
│       │   ├── hyde.h
│       │   ├── query_decomposer.h
│       │   └── query_rewriter.h
│       │
│       ├── post_retrieval/          # 检索后处理
│       │   ├── context_builder.h
│       │   ├── corrective.h
│       │   └── result_filter.h
│       │
│       ├── storage/                 # 自研数据库存储
│       │   ├── storage.h
│       │   ├── vector_index.h
│       │   ├── bm25_index.h
│       │   └── graph_store.h
│       │
│       ├── llm/                     # LLM 服务
│       │   ├── llm_service.h
│       │   ├── llama_service.h
│       │   └── embedding_service.h
│       │
│       ├── agent/                   # Agent 框架
│       │   ├── agent.h
│       │   ├── tool.h
│       │   ├── memory.h
│       │   ├── planner.h
│       │   └── react.h
│       │
│       ├── pipeline/                # Pipeline 层
│       │   ├── pipeline.h
│       │   ├── naive_pipeline.h
│       │   ├── advanced_pipeline.h
│       │   ├── hybrid_pipeline.h
│       │   ├── hyde_pipeline.h
│       │   ├── graph_pipeline.h
│       │   ├── corrective_pipeline.h
│       │   ├── react_pipeline.h
│       │   ├── iterative_pipeline.h
│       │   ├── recursive_pipeline.h
│       │   └── pipeline_factory.h
│       │
│       ├── api/                     # API 层
│       │   ├── server.h
│       │   ├── router.h
│       │   └── cli.h
│       │
│       └── web/                     # Web UI（前端独立项目）
│
├── src/
│   ├── chunker/
│   ├── retriever/
│   ├── reranker/
│   ├── pre_retrieval/
│   ├── post_retrieval/
│   ├── storage/
│   ├── llm/
│   ├── agent/
│   ├── pipeline/
│   └── api/
│
├── test/
│   ├── test_chunker.cpp
│   ├── test_retriever.cpp
│   ├── test_pipeline.cpp
│   └── ...
│
├── CMakeLists.txt
└── README.md
```

---

## 第五部分：实现顺序

### Phase 1: 基础设施（4 周）

| 序号 | 任务 | 说明 |
|------|------|------|
| 1 | LLM Service（llama.cpp 集成） | 模型加载、文本生成、Embedding |
| 2 | 存储层扩展 | VectorStore、GraphStore 接入现有 DB |
| 3 | 分块器实现 | 4 种分块器 |
| 4 | 基础检索器 | Vector、BM25 检索器 |

### Phase 2: 核心 RAG（3 周）

| 序号 | 任务 | 说明 |
|------|------|------|
| 5 | Naive RAG Pipeline | 基础检索-生成流程 |
| 6 | Advanced RAG Pipeline | 混合检索 + RRF + 重排序 |
| 7 | Hybrid RAG Pipeline | 三路召回 |
| 8 | HyDE RAG Pipeline | 假设答案引导检索 |

### Phase 3: 高级 RAG（3 周）

| 序号 | 任务 | 说明 |
|------|------|------|
| 9 | Graph RAG Pipeline | 知识图谱 + 实体提取 |
| 10 | Corrective RAG Pipeline | 检索结果质疑纠正 |
| 11 | Iterative RAG Pipeline | 迭代检索 refinement |
| 12 | Recursive RAG Pipeline | 递归问题分解 |

### Phase 4: Agent 框架（4 周）

| 序号 | 任务 | 说明 |
|------|------|------|
| 13 | Tool 系统 | Tool 基类、注册、调用 |
| 14 | Memory 系统 | 短/长/工作记忆 |
| 15 | Planner | 规划器、ReAct 循环 |
| 16 | ReAct RAG Pipeline | Agent 驱动检索 |

### Phase 5: 接口与集成（2 周）

| 序号 | 任务 | 说明 |
|------|------|------|
| 17 | REST API | HTTP 服务器、路由 |
| 18 | CLI 工具 | 命令行接口 |
| 19 | Web UI | 前端界面（独立项目） |

### Phase 6: 测试与优化（2 周）

| 序号 | 任务 | 说明 |
|------|------|------|
| 20 | 单元测试 | 各模块测试 |
| 21 | 集成测试 | Pipeline 集成测试 |
| 22 | Benchmark | 性能评测 |
| 23 | 优化 | 性能调优 |

---

## 第六部分：关键接口

### 核心类型

```cpp
// Query
struct Query {
    std::string text;
    std::string intent;       // 可选：意图
    json metadata;
};

// RetrievalResult
struct RetrievalResult {
    std::string chunk_id;
    std::string content;
    double score;
    std::string source;
    json metadata;
};

// PipelineResult
struct PipelineResult {
    std::string answer;
    std::vector<RetrievalResult> context;
    int retrieval_time_ms;
    int generation_time_ms;
    int total_tokens;
};
```

### Pipeline 基类

```cpp
class RAGPipeline {
public:
    virtual ~RAGPipeline() = default;
    virtual std::string type() const = 0;
    virtual PipelineResult query(const Query& query) = 0;
    virtual void build_index(const std::vector<Document>& docs) = 0;
};
```

---

## 第七部分：API 端点

```
POST   /api/v1/query           # RAG 查询
POST   /api/v1/query/stream    # 流式查询
POST   /api/v1/index           # 构建索引
GET    /api/v1/status          # 系统状态
GET    /api/v1/metrics         # 指标数据
GET    /api/v1/pipelines       # Pipeline 列表
POST   /api/v1/agent/execute   # Agent 执行
GET    /api/v1/health          # 健康检查
```

---

## 第九部分：Web UI 与评估系统

### 一、技术选型

| 维度 | 选择 |
|------|------|
| 框架 | React 18 + TypeScript |
| 构建工具 | Vite |
| UI 组件库 | Ant Design 5 |
| 状态管理 | Zustand |
| 实时通信 | WebSocket + SSE |
| 图表可视化 | ECharts |

### 二、功能模块

#### 1. 对话界面 (Chat)
- 流式响应显示
- 检索结果来源高亮
- 相关度分数可视化
- 性能指标展示
- 多轮对话上下文

#### 2. 知识库管理 (Knowledge)
- 多格式文档上传
- 分块策略配置
- 索引状态监控
- 文档预览与编辑

#### 3. Pipeline 配置 (Pipeline)
- 9 种 Pipeline 类型选择
- 检索/重排序/生成参数配置
- 实时测试验证

#### 4. Agent 配置 (Agent)
- 工具选择与配置
- 记忆系统配置
- ReAct 参数设置

#### 5. 监控面板 (Dashboard)
- 查询趋势图表
- Pipeline 使用分布
- 延迟分布统计
- 最近查询日志

#### 6. 评估测试 (Evaluation)
- 测试集管理
- 评估运行配置
- 评估结果详情
- Pipeline 对比评估
- 版本趋势分析

### 三、评估指标体系（业界完整标准）

#### A. 检索质量指标

| 指标 | 定义 | 计算方式 | 适用场景 |
|------|------|---------|---------|
| **Precision@K** | Top-K 结果中相关文档占比 | P@K = \|relevant ∩ retrieved@K\| / K | 衡量检索精度 |
| **Recall@K** | 所有相关文档被检索到的比例 | R@K = \|relevant ∩ retrieved@K\| / \|relevant\| | 衡量召回能力 |
| **Reciprocal Rank (RR)** | 第一个相关文档排名的倒数 | RR = 1 / rank(first relevant) | 衡量首个结果质量 |
| **MRR** | 多个查询的平均 RR | MRR = Σ RR(q) / \|Q\| | 多查询整体质量 |
| **NDCG@K** | 考虑排序质量的综合指标 | NDCG@K = DCG@K / IDCG@K | 衡量排序优劣 |
| **Hit Rate@K** | 是否至少命中一个相关文档 | Hit@K = \|queries with hit\| / \|Q\| | 衡量覆盖率 |
| **MAP@K** | 平均精度的均值 | MAP@K = Σ AP@K(q) / \|Q\| | 综合排序质量 |
| **F1@K** | Precision 和 Recall 的调和均值 | F1@K = 2 × P@K × R@K / (P@K + R@K) | 精度与召回平衡 |
| **R-Precision** | 相关文档数目的精度 | R-P = \|relevant ∩ top-R\| / R | R = \|relevant\| 时的精度 |
| **Context Recall** | 上下文对 ground truth 的覆盖度 | 上下文与标准答案的匹配程度 | RAGAS 专用 |
| **Context Precision** | 检索结果中与答案相关的比例 | 上下文中相关部分的占比 | RAGAS 专用 |

#### B. 生成质量指标

| 指标 | 定义 | 评估方法 | 来源 |
|------|------|---------|------|
| **Faithfulness** | 答案是否忠实于上下文 | LLM-as-Judge / NLI | RAGAS, DeepEval |
| **Answer Relevancy** | 答案与问题的相关程度 | 逆向问题生成 + 语义相似度 | RAGAS |
| **Answer Correctness** | 答案与 ground truth 的事实一致性 | 关键事实匹配 + 语义相似度 | RAGAS, DeepEval |
| **Answer Similarity** | 答案与 ground truth 的语义相似度 | Embedding 余弦相似度 | RAGAS |
| **Completeness** | 答案信息覆盖的完整程度 | LLM-as-Judge | DeepEval |
| **Relevancy** | 生成内容与问题的关联度 | LLM-as-Judge | DeepEval |
| **Correctness** | 答案的事实正确性 | LLM-as-Judge / 人工标注 | 通用 |
| **Coherence** | 答案的逻辑连贯性 | LLM-as-Judge | 通用 |
| **Fluency** | 答案的语言流畅度 | LLM-as-Judge | 通用 |

#### C. RAG 端到端指标

| 指标 | 定义 | 计算方式 |
|------|------|---------|
| **Hallucination Rate** | 生成中虚构内容的比例 | LLM-as-Judge 判断 |
| **Groundedness** | 答案是否能追溯到上下文 | 语句级证据匹配 |
| **Rejection Rate** | 无法回答时正确拒绝的比例 | 正确拒绝 / 本应拒绝总数 |
| **Faithfulness Score** | 答案基于上下文的完整程度 | RAGAS 计算 |
| **Context Relevancy** | 检索上下文与问题的相关度 | RAGAS 计算 |

#### D. 性能指标

| 指标 | 定义 | 目标值 |
|------|------|--------|
| **Latency P50/P99** | 查询延迟分布 | P50 < 500ms, P99 < 2s |
| **Throughput (QPS)** | 每秒查询数 | > 10 req/s |
| **Token Usage** | 平均 token 消耗 | < 2048 tokens/query |
| **Index Time** | 文档索引速度 | > 100 docs/s |
| **Retrieval Time** | 检索耗时 | < 100ms |
| **Generation Time** | 生成耗时 | < 2s |
| **Memory Usage** | 内存占用 | < 4GB |
| **GPU Utilization** | GPU 利用率 | > 80%（如有 GPU） |

#### E. 稳定性与鲁棒性指标

| 指标 | 定义 | 测试方法 |
|------|------|---------|
| **Query Consistency** | 相同查询结果一致性 | 多次查询比较 |
| **Adversarial Robustness** | 对抗样本鲁棒性 | 注入噪声/误导性文档 |
| **Noise Tolerance** | 噪声文档干扰抵抗 | 混入无关文档测试 |
| **Edge Case Handling** | 边界情况处理能力 | 空查询/超长查询/特殊字符 |
| **Degradation Recovery** | 服务降级后恢复能力 | 故障注入测试 |

#### F. LLM-as-Judge 评估标准（RAGAS/DeepEval 通用）

```cpp
// 评估提示词模板（Faithfulness）
const std::string FAITHFULNESS_PROMPT = R"PROMPT(
请根据以下上下文和生成的答案，评估答案的忠实度。

上下文: {context}
答案: {answer}

请为以下维度评分（1-5分）：
1. 事实一致性：答案中的事实是否都能从上下文中找到依据
2. 无虚构性：答案是否没有编造上下文中不存在的信息
3. 完整性：答案是否充分利用了上下文中的相关信息

请输出 JSON 格式：
{"score": 1-5, "reason": "评分理由"}
)PROMPT";
```

### 四、测试数据持久化与版本管理

#### 1. Golden Dataset 格式

```json
{
  "version": "1.0",
  "created_at": "2026-09-05",
  "description": "知识库质量测试集",
  "questions": [
    {
      "id": "q001",
      "query": "什么是RAG？",
      "category": "概念解释",
      "difficulty": "easy",
      "ground_truth": {
        "answer": "RAG（Retrieval-Augmented Generation）是一种结合检索和生成的架构...",
        "relevant_doc_ids": ["doc001", "doc012"],
        "key_facts": ["检索增强生成", "结合检索和生成", "减少幻觉"],
        "expected_answer_type": "explanation"
      },
      "metadata": {
        "domain": "AI",
        "tags": ["RAG", "概念"],
        "author": "admin",
        "verified": true
      }
    }
  ]
}
```

#### 2. 评估运行记录格式

```json
{
  "run_id": "run_20260905_1430",
  "timestamp": "2026-09-05T14:30:00Z",
  "dataset_id": "ds_001",
  "dataset_version": "1.0",
  "pipeline_config": {
    "type": "advanced",
    "retrieval": {"top_k": 10, "rrf_k": 60},
    "reranker": true,
    "llm": {"model": "qwen2.5-7b", "temperature": 0.7}
  },
  "results": {
    "total_questions": 100,
    "metrics": {
      "precision_at_5": 0.82,
      "recall_at_10": 0.78,
      "mrr": 0.85,
      "ndcg_at_10": 0.81,
      "hit_rate": 0.95,
      "faithfulness": 0.88,
      "answer_relevancy": 0.86,
      "hallucination_rate": 0.08
    },
    "details": [
      {
        "question_id": "q001",
        "query": "什么是RAG？",
        "retrieved_docs": ["doc001", "doc012", "doc005"],
        "retrieval_scores": [0.95, 0.89, 0.72],
        "answer": "RAG是一种结合检索和生成的架构...",
        "metrics": {
          "precision_at_5": 1.0,
          "recall_at_10": 1.0,
          "faithfulness": 0.92,
          "correctness": 0.95
        },
        "latency_ms": 342,
        "token_usage": 256
      }
    ],
    "errors": [],
    "warnings": []
  },
  "summary": {
    "best_pipeline": "advanced",
    "worst_category": "多跳推理",
    "improvement_suggestions": [
      "Graph RAG 在多跳问题上表现更好",
      "建议调整 reranker 的 top_k 参数"
    ]
  }
}
```

#### 3. 数据存储结构

```
eval_data/
├── datasets/                    # 测试集版本管理
│   ├── ds_001/
│   │   ├── v1.0.json           # 版本 1.0
│   │   ├── v1.1.json           # 版本 1.1（修改后）
│   │   └── metadata.json       # 数据集元信息
│   └── ds_002/
│       └── ...
├── runs/                        # 评估运行记录
│   ├── run_20260905_1430.json   # 每次运行的完整数据
│   ├── run_20260905_1500.json
│   └── ...
├── comparisons/                 # 对比分析
│   ├── comp_20260905.json       # 多次运行的对比结果
│   └── ...
└── optimization/                # 优化记录
    ├── optimization_log.json    # 优化建议与实施记录
    └── ...
```

#### 4. 版本管理策略

| 版本管理维度 | 策略 |
|-------------|------|
| **数据集版本** | 语义化版本（v1.0, v1.1, v2.0），每次修改需记录变更日志 |
| **运行记录** | 永久保存，不可修改，唯一ID标识 |
| **Pipeline配置** | 快照存储，每次评估保存完整配置副本 |
| **指标历史** | 时序存储，支持趋势分析 |
| **优化建议** | 链式记录，每次优化关联前次评估结果 |

#### 5. 优化反馈循环

```
┌─────────────────────────────────────────────────────────────────────┐
│                     优化反馈循环                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌────────────┐     ┌────────────┐     ┌────────────┐             │
│  │  评估运行   │────→│  问题诊断   │────→│  优化建议   │             │
│  │ (Eval Run) │     │ (Diagnosis)│     │(Optimization)│            │
│  └────────────┘     └────────────┘     └────────────┘             │
│       │                  │                    │                     │
│       │                  │                    │                     │
│       ▼                  ▼                    ▼                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                     数据存储与分析                            │   │
│  │  - 历史评估结果                                               │   │
│  │  - 优化建议记录                                               │   │
│  │  - 趋势图表数据                                               │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              ▼                                      │
│  ┌────────────┐     ┌────────────┐     ┌────────────┐             │
│  │  验证测试   │────→│  A/B 对比   │────→│  部署上线   │             │
│  │ (Verify)   │     │(A/B Test)  │     │ (Deploy)   │             │
│  └────────────┘     └────────────┘     └────────────┘             │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

#### 6. 问题诊断维度

| 问题类型 | 诊断指标 | 优化方向 |
|---------|---------|---------|
| **检索不足** | Recall@K < 0.7 | 调整 top_k, 优化 Embedding |
| **检索噪音** | Precision@K < 0.6 | 优化 Reranker, 调整阈值 |
| **幻觉问题** | Hallucination > 0.15 | 优化 Prompt, 调低 temperature |
| **不完整回答** | Completeness < 0.7 | 扩大 context window |
| **相关性差** | Relevancy < 0.7 | 优化 Query Expansion |
| **响应过慢** | Latency P99 > 3s | 优化检索, 缓存热点 |

#### 7. 优化建议生成规则

```cpp
struct OptimizationRule {
    std::string condition;      // 触发条件
    std::string diagnosis;      // 问题诊断
    std::string suggestion;     // 优化建议
    std::string priority;       // 优先级 (high/medium/low)
    std::string expected_impact;// 预期效果
};

std::vector<OptimizationRule> rules = {
    {"recall@10 < 0.7 && precision@5 > 0.8",
     "检索召回率不足，但精度尚可",
     "增大 top_k，或优化 Embedding 模型",
     "high", "预计 recall 提升 15%"},

    {"hallucination_rate > 0.15",
     "幻觉率偏高",
     "优化 Prompt 指令，降低 temperature 至 0.3-0.5",
     "high", "预计 hallucination 降低 50%"},

    {"latency_p99 > 3000",
     "P99 延迟过高",
     "启用查询缓存，优化检索算法",
     "medium", "预计 P99 降至 2s 以内"},

    {"answer_relevancy < 0.7 && faithfulness > 0.9",
     "答案忠实但不够相关",
     "优化 Query Expansion 或调整上下文窗口",
     "medium", "预计 relevancy 提升 10-20%"}
};
```

### 五、API 端点（完整）

```
# 对话
POST   /api/v1/query                 # RAG 查询
POST   /api/v1/query/stream          # 流式查询

# 知识库
POST   /api/v1/knowledge/upload      # 文档上传
GET    /api/v1/knowledge/documents   # 文档列表
DELETE /api/v1/knowledge/documents/:id # 删除文档

# Pipeline
GET    /api/v1/pipelines             # Pipeline 列表
PUT    /api/v1/pipelines/:type       # 更新配置

# 评估系统
POST   /api/v1/eval/datasets                # 创建测试集
GET    /api/v1/eval/datasets                 # 获取测试集列表
PUT    /api/v1/eval/datasets/:id             # 更新测试集
DELETE /api/v1/eval/datasets/:id             # 删除测试集

POST   /api/v1/eval/runs                     # 创建评估任务
GET    /api/v1/eval/runs                     # 获取评估历史
GET    /api/v1/eval/runs/:id                 # 获取单次评估详情
GET    /api/v1/eval/runs/:id/results         # 获取评估结果
GET    /api/v1/eval/runs/:id/stream          # SSE 实时进度

POST   /api/v1/eval/compare                  # 对比多次评估结果
GET    /api/v1/eval/trends                   # 获取指标趋势数据
GET    /api/v1/eval/optimization-suggestions # 获取优化建议

# 监控
GET    /api/v1/status                # 系统状态
GET    /api/v1/metrics               # 指标数据
```

---

## 第十部分：验收标准更新

| 指标 | 目标值 |
|------|--------|
| 支持 Pipeline 数 | 9 种 |
| 索引速度 | 100 docs/s |
| 查询延迟 P50 | < 500ms |
| 查询延迟 P99 | < 2s |
| Token 利用率 | > 80% |
| 单元测试覆盖率 | > 80% |
| 文档完整性 | 100% |
| 评估指标完整度 | 覆盖 RAGAS + DeepEval 全部核心指标 |
| 测试数据持久化 | 支持版本管理与历史对比 |

---

## 附录：参考资料

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - GGML/GGUF 量化推理
- [LangChain](https://github.com/langchain-ai/langchain) - RAG 架构参考
- [Microsoft Graph RAG](https://github.com/microsoft/graphrag) - Graph RAG 参考
- [Self-RAG Paper](https://arxiv.org/abs/2310.11511) - (不实现，仅参考)
- [RAGAS](https://github.com/explodinggradients/ragas) - RAG 评估框架
- [DeepEval](https://github.com/confident-ai/deepeval) - LLM 评估框架
- [RAGFlow](https://github.com/infiniflow/ragflow) - RAG Web UI 参考
- [AnythingLLM](https://github.com/Mintplex-Labs/anything-llm) - 本地 RAG 应用参考
- [Dify](https://github.com/langgenius/dify) - 工作流 RAG 参考
- [Flowise](https://github.com/FlowiseAI/Flowise) - 可视化流程参考
- [LangFlow](https://github.com/langflow-ai/langflow) - 组件市场参考
