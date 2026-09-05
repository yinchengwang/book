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

### 三、评估指标体系

#### 检索指标
| 指标 | 定义 |
|------|------|
| Precision@K | Top-K 结果中相关文档占比 |
| Recall@K | 相关文档被检索到的比例 |
| MRR | 第一个相关文档的排名倒数 |
| NDCG@K | 考虑排序质量的综合指标 |
| Hit Rate | 是否至少命中一个相关文档 |

#### 生成指标
| 指标 | 定义 |
|------|------|
| Faithfulness | 答案是否基于上下文 |
| Relevancy | 答案是否与问题相关 |
| Correctness | 事实是否正确 |
| Completeness | 信息覆盖度 |

#### 端到端指标
| 指标 | 定义 |
|------|------|
| Answer Relevancy | 最终答案相关程度 |
| Context Precision | 上下文与答案相关性 |
| Context Recall | 所需信息覆盖度 |

### 四、API 端点

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

# 评估
POST   /api/v1/eval/datasets         # 创建测试集
POST   /api/v1/eval/runs             # 创建评估任务
GET    /api/v1/eval/runs/:id/results # 获取结果
GET    /api/v1/eval/runs/:id/stream  # SSE 实时进度

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

---

## 附录：参考资料

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - GGML/GGUF 量化推理
- [LangChain](https://github.com/langchain-ai/langchain) - RAG 架构参考
- [Microsoft Graph RAG](https://github.com/microsoft/graphrag) - Graph RAG 参考
- [Self-RAG Paper](https://arxiv.org/abs/2310.11511) - (不实现，仅参考)
