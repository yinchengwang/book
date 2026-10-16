# quivr Wiki（个人知识库 RAG / Quivr V2 内容引擎）

> 参考版本：Quivr V2（2025-2026，Go 重写版，从原 Python 架构演进而来）。本 wiki 重点介绍 quivr 作为「个人知识库 RAG 应用」的定位、RAG 四维度扩展、Go 内容引擎架构、源码学习路径与典型陷阱。

## 一、项目定位

quivr（原名 Quivr）是由 **Stan Girard**（法国开发者）于 **2023 年 5 月开源**的 **「个人知识库 RAG 应用」**，最初是「**second brain / personal ChatGPT**」定位。**2025 年项目重写为 Quivr V2**，定位升级为「**开源内容引擎 + 持续内容流的搜索与监控**」（turning continuous content streams into search and monitoring），技术栈从 Python / FastAPI 演进为 **Go / PostgreSQL / Weaviate / Temporal / TEI** 的工业级引擎。

- **核心痛点（V1）**：
  - 个人知识管理工具（Notion / Roam Research）缺少 LLM 接入；
  - 上传 PDF / 网页 → 智能问答缺少统一 UI；
  - 个人用户想要 ChatGPT-like 体验 + 私有知识库；
  - LangChain / LlamaIndex 偏研究 / Demo，个人用户难上手；
- **核心痛点（V2 升级）**：
  - 现有 RAG 系统缺少 **Durability**（写一次不丢）；
  - 缺少 **Idempotency**（重放幂等性）；
  - 缺少 **Plugin-based Extensibility**（连接器 / 规范化 / 富化 / 投放都可扩展）；
  - 缺少 **Honest Search**（每个 hit 都要 rehydrate + re-authorize）；
  - 缺少 **Rebuildable Indexes**（PostgreSQL + S3 是 canonical source，搜索索引可重建）；
- **典型用户**：
  - **个人知识工作者**：上传 PDF / 笔记 / 网页 → 问答；
  - **团队 / 企业**：构建知识库；
  - **V2 时代**：内容流监控（持续摄入 + 实时搜索 + 时序跟踪）；

## 二、发展历史与社区

- **起源（V1，2023-05）**：Stan Girard 在法国开源 quivr v1.0，定位「Personal Assistant」（个人 ChatGPT）；
- **里程碑（V1 Python 时代）**：
  - 2023-05：v1.0，FastAPI + Supabase + OpenAI + Chroma + React 前端；
  - 2023-08：v2.0+，集成 LangChain、Streamlit、PDF 解析；
  - 2023-12：v3.0+，Multi-Doc、Chat History、Auth；
  - 2024-03：v5.0+，Plugin 系统、Multi-User；
  - 2024-09：v6.0+，性能优化、SQLAlchemy 2.0；
  - 2024-12：v6.x，Quivr Cloud（商业版）发布；
- **里程碑（V2 Go 重写）**：
  - 2025-04：V2 alpha 公开，定位升级为「Content Engine」；
  - 2025-07：V2 beta，PostgreSQL + Weaviate + Temporal + TEI + S3；
  - 2025-09：V2 v0，Go API + Worker + Migrate 单二进制；
  - 2025-10：V2 v0.x 持续完善（Plugin 框架 / Audit / 监控 / 部署）；
- **维护状态**：**活跃**（maintained by Quivr team），社区 100+ 贡献者，月均 50+ commits（V1 + V2 合计）；
- **社区**：
  - GitHub 36k+ star（V1）、2k+ star（V2 仓库）；
  - Discord 3k+ 成员、Twitter @ quivr_app；
  - 商业产品 Quivr Cloud（闭源 SaaS）；
- **关键贡献者**：Stan Girard（创始人）、Quivr 团队 15+ 工程师、社区贡献者 100+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`internal/`、`cmd/`、`client/`、`sdks/`、`docs/`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **Quivr Cloud**（闭源 SaaS）：托管 Quivr V2 服务、Multi-Tenant、Auth、可观测性；
- **生态关系**：
  - **上游依赖（V2）**：
    - **Go 1.22+**：核心语言；
    - **PostgreSQL**：catalog、receipts、changes（canonical source）；
    - **Weaviate**：向量 + lexical 搜索；
    - **S3**：canonical bytes + artifacts；
    - **Temporal**：分布式工作流；
    - **TEI（Text Embeddings Inference）**：E5 embeddings；
    - **armada**：构建系统（`armada.toml`）；
    - **release-please**：发布管理；
    - **OpenTelemetry**：observability；
  - **下游生态**：约 **10000+ 项目使用 Quivr**，包括个人知识工作者、教育 / 教学、Quivr Cloud 用户；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **Quivr Cloud**：托管用户；
  - **多家个人知识工作者**：上传 PDF / 笔记 / 网页 → 问答；
  - **V2 时代**：内容流监控（媒体监控 / 客户支持监控）；
  - **教育 / 教学机构**：个人 / 团队知识库；
- **国内**：
  - 多家个人 / 团队知识库；
- **典型场景**：
  - **个人知识库**：上传 PDF / 笔记 → 问答；
  - **团队 / 企业知识库**：Multi-User；
  - **内容流监控**（V2）：持续摄入（RSS / Slack / Confluence）→ 实时搜索 → 主题跟踪；
  - **客服 RAG**：工单 / 邮件 / Slack 流监控；
  - **媒体 / 舆情监控**：新闻流 → 实时检索；
  - **学术 RAG**：论文 / 文献库 → 检索；

## 五、一句话对比定位

> quivr 是 **「Quivr 出品 + Personal Knowledge RAG + Content Engine + Go V2 + Durable + Idempotent + Plugin-based + PostgreSQL + Weaviate + Temporal + MIT + 36k star + 个人 / 团队 / 监控全场景」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**txtai（一体化）**、**Verba（Weaviate 出品）**、**Cognita（TrueFoundry 出品）**、**R2R（SciPhi 出品）**、**私人 GPT（本地）** 构成 RAG 应用生态。quivr 的差异化是 **「36k star + 法国背景 + 个人知识库流行 + V2 Go 重写 + Content Engine + Durable + Plugin-based」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（apps）                                   │
│        Web App / Mobile App / Connector / OpenAPI / REST API       │
│        Armada Build（armada.toml）                                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │ REST API v0
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Go API（cmd/api/）                                  │
│   Gin / Echo HTTP Server / OpenAPI / Auth Middleware / Plugins       │
│   ──> PostgreSQL（catalog / receipts / changes）──> Temporal        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Worker（cmd/worker/）                                │
│   Temporal Workflow / Step / Activity / Retry / Compensation        │
│   ──> S3（canonical bytes / artifacts）                              │
│   ──> Weaviate（lexical + vector search）                            │
│   ──> TEI（E5 embeddings）                                           │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Core + Plugin（internal/ + plugins/）               │
│   core.ingest / core.connector / core.normalizer / core.enricher      │
│   core.retriever / core.delivery（plugin capabilities）            │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  PostgreSQL    │ │  Weaviate        │ │  Temporal         │ │  S3        │
│  Catalog       │ │  Lexical + Vector│ │  Workflow Engine  │ │  Canonical │
│  Receipts      │ │  Hybrid Search   │ │  Step / Activity  │ │  Artifacts │
│  Changes       │ │  Schema          │ │  Retry / History  │ │            │
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────────────────┐
│                    Search / Rehydrate / Re-check（API 层）                             │
│    Search → Weaviate → Hit → Rehydrate from S3 → Re-authorize → Return               │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Audit / Monitoring（internal/audit/ + monitoring/）  │
│   OpenTelemetry / Prometheus / Audit Log / Change Feed              │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：V2 是**单二进制 + 多进程**架构（`quivr api` / `quivr worker` / `quivr migrate`）；PostgreSQL + Weaviate + S3 + Temporal 是外部依赖。

## 七、嵌入模型适配（EMB）

quivr V2 主要通过 **TEI（Text Embeddings Inference）** 服务暴露嵌入（`internal/keywords/`、`internal/content/`）：

- **TEI（HuggingFace Text Embeddings Inference）**：
  - **E5**：`intfloat/e5-large-v2`（1024 维）、`intfloat/multilingual-e5-large`（多语，1024 维）；
  - **BGE**：`BAAI/bge-large-en-v1.5`（1024 维）、`BAAI/bge-large-zh-v1.5`（中文，1024 维）；
  - **GTE**：`Alibaba-NLP/gte-large-en-v1.5`（1024 维）；
  - **MPNet**：`all-mpnet-base-v2`（768 维）；
  - **MiniLM**：`all-MiniLM-L6-v2`（384 维）；
  - **Instructor**：`hkunlp/instructor-xl`（指令嵌入）；
- **本地部署**：
  - TEI 服务（Docker / Kubernetes）；
  - **GPU 加速**：CUDA / ROCm；
  - **CPU 加速**：ONNX / Optimum；
- **V1 时代支持更多**：
  - **OpenAI**：`text-embedding-3-small/large`；
  - **Cohere**：`embed-english-v3.0`；
  - **HuggingFace**：`SentenceTransformers`；
  - **Ollama**：`nomic-embed-text`；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - **core.ingest plugin**（V2 默认）：
      - `RecursiveCharacterTextSplitter`（基于 token 切分）；
      - `SemanticSplitter`（基于嵌入相似度）；
      - `SentenceSplitter`（按句子）；
      - `TokenSplitter`（tiktoken）；
  - **推荐参数**：chunk_size 500-1500 token、overlap 10-20%；

## 八、文档解析器（LOAD）

quivr V2 通过 **plugin capability** 暴露文档解析（`internal/connectors/`、`plugins/`）：

- **基础 Parser（V2 Plugin）**：
  - **PDF**（基于 `PyMuPDF` / `pdfplumber` / `pdfminer`）；
  - **HTML**（基于 `trafilatura` / `selectolax`）；
  - **Markdown**（基于 `markdown-it-py`）；
  - **Docx**（基于 `python-docx`）；
  - **CSV / JSON / TXT**；
- **多模态 Parser（V2 Plugin）**：
  - **图像**（基于 `PIL` / `opencv-python`，OCR via Tesseract / PaddleOCR）；
  - **音频**（基于 Whisper）；
  - **视频**（基于 `ffmpeg-python` + Whisper）；
- **特殊 Parser**：
  - **Web Crawler**（基于 `firecrawl` / `crawl4ai`）；
  - **Notion Parser**（社区）；
  - **Confluence Parser**（社区）；
  - **GitHub Parser**（社区）；
  - **Database Parser**（Postgres / MySQL）；
  - **RSS / Atom**（V2 监控用）；
  - **Slack / Discord**（V2 监控用）；
- **Plugin 系统**：
  - **Connectors**（数据源）；
  - **Normalizers**（数据规范化）；
  - **Enrichers**（数据富化）；
  - **Retrievers**（检索策略）；
  - **Delivery Channels**（结果输出）；

## 九、检索器与重排器（RETRIEVE / RERANK）

quivr V2 通过 **core.retriever plugin** 暴露检索：

- **基础检索**：
  - **Hybrid Search**：Weaviate lexical + vector hybrid search；
  - **Vector Search**：纯向量检索；
  - **Lexical Search**：纯 BM25 检索；
- **Honest Search**：
  - **每个 hit 都要 rehydrate**（从 S3 拉取 canonical bytes）；
  - **每个 hit 都要 re-authorize**（基于 ACL）；
- **Graph Retrieval**：
  - **Entity Linking**（V2 增强）；
  - **Topic Tracking**（V2 监控用，跟踪主题时间线）；
- **Rerank**：
  - **Cohere Rerank**（社区集成）；
  - **BGE Rerank**（社区集成）；
  - **LLM-based Rerank**（社区集成）；
  - **Custom Rerank**：用户自定义；
- **过滤**：
  - **ACL**（V2 核心，每个 hit 都要授权）；
  - **Metadata 过滤**；
  - **Temporal 过滤**（V2 监控用，时间窗口）；
- **Advanced**：
  - **Query Expansion**：用 LLM 生成多个 query；
  - **HyDE**：先 LLM 生成假设答案再检索；
  - **Change Feed**（V2 核心）：持续变化内容流推送（SSE / polling）；

## 十、向量库后端（VECTOR STORE）

quivr V2 通过 **Weaviate**（默认）作为向量库：

- **Weaviate**（默认，`internal/` 通过 Weaviate Client 暴露）：
  - **Hybrid Search**（默认）；
  - **Vector Search**；
  - **Lexical Search**；
  - **Schema Management**；
  - **Multi-Tenancy**（社区）；
- **PostgreSQL + pgvector**（社区，可选）：
  - **适合小型部署**；
- **Pinecone**（社区，可选）：
  - **Serverless**；
- **LanceDB**（社区，可选）：
  - **嵌入式**；
- **配置**：
  - `WEAVIATE_URL`：Weaviate Server URL；
  - `WEAVIATE_API_KEY`：API key；
  - **Plugin 模式**：可以自定义 Vector DB；
- **选型建议**：
  - 默认：Weaviate（V2 推荐）；
  - 小型：pgvector；
  - 大型：Weaviate Cluster / Pinecone；
- **Honest Search**：
  - Weaviate 只是 **projection**（搜索索引），canonical data 在 PostgreSQL + S3；
  - **可重建**：从 PostgreSQL + S3 重建 Weaviate 索引；

## 十一、查询/计算/接口层

- **Go API**（`cmd/api/`）：
  - **Gin / Echo HTTP Server**；
  - **OpenAPI v0**；
  - **Auth Middleware**（JWT / API Key）；
  - **Multi-Tenant**；
  - **Audit**（`internal/audit/`）；
  - **Autoscaling**（`internal/autoscaling/`）；
  - **Rate Limiting**；
- **Worker**（`cmd/worker/`）：
  - **Temporal Workflow**（`internal/occluded/`）；
  - **Step / Activity / Retry / Compensation**；
  - **Backfill**（`internal/backfill/`，重建索引）；
  - **Lifecycle**（`internal/lifecycle/`）；
  - **History**（`internal/history/`，查询历史）；
- **Content Engine**（`internal/content/`）：
  - **Ingestion**（摄入）；
  - **Indexing**（索引）；
  - **Search**（检索）；
  - **Enrichment**（富化）；
  - **Delivery**（投放）；
- **Corpus**（`internal/corpus/`）：
  - **Corpus Management**（文档集管理）；
  - **Multi-Corpus**（多文档集）；
  - **ACL**（每个 corpus 独立权限）；
- **Changes**（`internal/changes/`）：
  - **Change Feed**（内容变更流）；
  - **SSE / Polling**（V2 监控核心）；
  - **Version Tracking**（每个版本独立索引）；
- **Connectors**（`internal/connectors/`）：
  - **Local File**（本地文件）；
  - **S3**（S3 对象存储）；
  - **Confluence / Notion / Slack / GitHub / RSS / Database**（V2 Plugin 扩展）；
- **Adapters**（`internal/adapters/`）：
  - **Adapter Pattern**（适配器模式封装第三方 API）；
- **Logging**（`internal/logging/`）：
  - **Structured Logging**；
  - **OpenTelemetry Tracing**；
- **Monitoring**（`internal/monitoring/`）：
  - **Prometheus Metrics**；
  - **Audit Log**；
- **Build & Release**：
  - **armada**（`armada.toml`，构建系统）；
  - **release-please**（`release-please-config.json`，发布管理）；
  - **version.txt**（版本号）；
- **SDK**：
  - **Go SDK**（`sdks/`）；
  - **TypeScript SDK**（`sdks/`）；
  - **Python SDK**（`sdks/`，社区）；
- **流式响应**：
  - **SSE**（Server-Sent Events，V2 核心，持续内容流推送）；
  - **WebSocket**；
- **评估与可观测性**：
  - **OpenTelemetry**（V2 默认）；
  - **Prometheus Metrics**；
  - **Audit Log**（V2 持久化）；
  - **Custom Evaluator**（用户自定义）；
- **生产部署**：
  - **Docker Compose**（`deploy/`）；
  - **Kubernetes**（Helm chart / Kustomize）；
  - **Quivr Cloud**（商业托管）；

## 十二、生态与集成

- **客户端**：Go SDK（`sdks/`）、TypeScript SDK（`sdks/`）、Python SDK（社区）、REST API、OpenAPI；
- **集成矩阵**：
  - **Vector DB**：Weaviate（默认）、pgvector（社区）、Pinecone（社区）、LanceDB（社区）；
  - **存储**：PostgreSQL（默认，canonical source）、S3（canonical bytes + artifacts）；
  - **工作流**：Temporal（V2 默认）；
  - **嵌入**：TEI（V2 默认，HuggingFace Text Embeddings Inference）；
  - **OCR**：Tesseract、PaddleOCR（社区）；
  - **音频**：Whisper（社区）；
  - **视频**：ffmpeg + Whisper（社区）；
  - **LLM**：OpenAI、Anthropic、Cohere、Ollama、HuggingFace（社区）；
  - **Auth**：JWT、API Key；
  - **Observability**：OpenTelemetry、Prometheus；
- **Quivr Cloud**：商业 SaaS（Multi-Tenant RAG 平台 + 持续内容流监控）；
- **Plugin Marketplace**（社区）：Connectors、Normalizers、Enrichers、Retrievers、Delivery；
- **armada 构建系统**：构建 / 部署一体化；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `cmd/api/main.go`：Go API 入口；
  2. `cmd/worker/main.go`：Go Worker 入口；
  3. `cmd/migrate/main.go`：数据库迁移；
  4. `internal/app/`：应用上下文（依赖注入）；
  5. `internal/content/`：内容引擎（Ingestion / Indexing / Search）；
  6. `internal/corpus/`：文档集管理；
  7. `internal/changes/`：变更流（V2 核心）；
  8. `internal/connectors/`：连接器；
  9. `internal/lifecycle/`：生命周期；
  11. `internal/history/`：查询历史；
  11. `internal/audit/`：审计日志；
  12. `internal/monitoring/`：监控指标；
  13. `internal/autoscaling/`：自动扩缩容；
  14. `internal/backfill/`：索引重建；
  15. `internal/keywords/`：关键词索引；
  16. `internal/adapters/`：适配器模式；
  17. `internal/ingestion/`：摄入（V1 兼容层）；
  18. `internal/logging/`：日志；
  19. `plugins/`：Plugin 框架；
  20. `sdks/`：客户端 SDK；
  21. `client/`：HTTP API 客户端；
- **LOC 与语言**：
  - `internal/` ~30k LoC；
  - `cmd/` ~5k LoC；
  - `plugins/` ~5k LoC；
  - `sdks/` ~5k LoC（Go + TypeScript + Python）；
  - 主语言：**Go**（V2 核心，95%）、TypeScript（SDK）、Python（V1 兼容层 / 解析器）；
- **构建系统**：
  - **armada**（`armada.toml`，构建 + 部署）；
  - **release-please**（版本管理）；
  - **Docker Compose**（`deploy/`）；
  - **Kubernetes**（社区）；
  - **Magefile**（社区）；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（`README.md`）；
  - 跑通 `docker compose up`（PostgreSQL + Weaviate + Temporal + TEI + S3）；
  - 上传 PDF / Markdown，提问；
  - 了解 Content Engine 概念；
- **进阶（2-3 周）**：
  - 深入学习 Content Engine（Ingestion / Indexing / Search）；
  - 实战 Custom Plugin（Connector / Normalizer / Enricher / Retriever）；
  - 学习 Temporal Workflow；
  - 学习 Change Feed（SSE 持续内容流推送）；
- **高级（1-2 月）**：
  - 阅读源码（Content Engine + Temporal Workflow + Plugin Framework）；
  - 自定义 Plugin；
  - Honest Search 优化（Rehydrate / Re-authorize 性能优化）；
  - 生产部署（Kubernetes + armada）；
  - Quirk Cloud 集成；
- **推荐资源**：
  - 官方文档：https://quivr.com/docs
  - GitHub V2：https://github.com/quivr/quivr
  - Quivr Cloud：https://cloud.quivr.com/
  - 论文：Temporal、TEI、Weaviate Hybrid Search、RAG、Honest Search；
  - 博客：https://quivr.com/blog

## 十五、风险与陷阱

- **V1 → V2 重写**：V1（Python）和 V2（Go）是两个不同项目，V2 仍在 v0 阶段，API 不稳定；
- **架构复杂**：V2 引入 Temporal / Weaviate / PostgreSQL / S3 / TEI 多依赖，部署复杂；
- **Temporal 学习曲线**：Temporal 是核心工作流引擎，新人难上手；
- **Honest Search 性能开销**：每次 search 都要 rehydrate + re-authorize，性能开销大；
- **Plugin 文档不全**：Plugin 框架较新，文档较少；
- **Weaviate 依赖**：Weaviate 是核心搜索依赖，迁移到其他向量库需重写 Plugin；
- **TEI 部署**：TEI 需要 GPU / 较大内存，部署复杂；
- **S3 依赖**：canonical bytes 在 S3，本地开发需 MinIO；
- **PostgreSQL Schema 复杂**：catalog / receipts / changes 多张表，迁移复杂；
- **V2 评估缺失**：默认无 RAG 评估，需自定义集成；
- **Observability 黑盒**：OpenTelemetry 集成有限，需自接更多可观测性；
- **Quivr Cloud 锁定**：使用 Quivr Cloud 后切换到本地部署困难；
- **社区生态分化**：V1 社区贡献者多，V2 社区较小；

## 十六、参考资料

- 官方仓库 V2：https://github.com/quivr/quivr
- 官方文档：https://quivr.com/docs
- Quivr Cloud：https://cloud.quivr.com/
- Weaviate：https://weaviate.io/
- Temporal：https://temporal.io/
- TEI：https://github.com/huggingface/text-embeddings-inference
- 论文：RAG、Hybrid Search、Honest Search、Temporal、TEI
- 博客：https://quivr.com/blog
- Tutorial：https://quivr.com/docs/quickstart
- Discord：https://discord.gg/quivr

## 十七、与本项目关联

quivr V2 作为「Go 重写 + Content Engine + PostgreSQL + Weaviate + Temporal + TEI + S3」个人知识库框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「多语言栈对照 + Content Engine + Temporal 异步 + 多存储后端」四个维度具有直接参考价值：

- **Go 重写 vs C/C++ 自研**：quivr V2 从 Python 重写为 Go（API + Worker + PostgreSQL + Weaviate + Temporal + TEI + S3 完整 Content Engine），体现「系统级语言 + 模块化服务」设计思路；本项目同样以 C/C++ 为主语言（`engineering/src/db/`、`engineering/src/algo-prod/`），通过 Phase 9 分布式演进（`engineering/src/db/distributed/`）实现类 Go 后端的工程化体系。
- **Content Engine 内容引擎**：quivr V2 定位升级为「Content Engine」，把个人知识库抽象为「上传 → 分块 → 嵌入 → 检索 → 生成」内容流水线，与本项目多模态 RAG 应用层（`engineering/apps/multimodal_rag/`）+ 多模态存储抽象（`engineering/include/db/mm_storage.h`）的设计理念一致——都是「内容 + 向量 + 元数据」三段式管线。
- **Temporal 异步任务编排**：quivr V2 通过 Temporal 实现 ingestion / embedding / 长期任务的可靠编排；本项目在 `engineering/src/db/distributed/` 实现分布式协调 + 异步任务调度能力（Phase 9），可在「可靠任务编排引擎」方向对照学习。
- **TEI 文本嵌入推理服务**：quivr V2 通过 HuggingFace TEI（Text Embeddings Inference，Rust 加速）做本地嵌入推理；本项目则通过 `engineering/apps/multimodal_rag/src/index/hnsw_persist.c`（`hnsw_persist.h`）实现 C/C++ 端 HNSW 持久化索引，可在「自研 vs TEI 集成」决策上提供参考。Rust 与 C 都是面向系统级性能的语言，本项目通过 C 端实现可在底层算法与数据结构优化上对照学习。
- **PostgreSQL 元数据存储**：quivr V2 使用 PostgreSQL 作为元数据后端；本项目则实现 PostgreSQL 风格的 Catalog 模块（`engineering/include/db/catalog.h`、`engineering/src/db/storage/catalog/`）+ `engineering/src/db/core/guc.c`（GUC 配置系统），可在「PG 兼容设计」方向对照学习。
- **Weaviate 向量库**：quivr V2 通过 Weaviate 做向量检索；本项目 wiki 在 `docs/db-wiki/vector/` 已系统梳理 Chroma / Milvus / FAISS / LanceDB / Qdrant 五种向量库的工程视角（`docs/db-wiki/vector/chroma.md`、`docs/db-wiki/vector/milvus.md`、`docs/db-wiki/vector/faiss.md`、`docs/db-wiki/vector/lancedb.md`、`docs/db-wiki/vector/qdrant.md`），可与 quivr V2 选型互补。
- **S3 文件存储**：quivr V2 通过 S3 兼容接口管理文件；本项目则在 `engineering/src/db/storage/blob/` 提供 Blob 存储能力，可在「大文件 + 元数据分离存储」模式上对照。
- **量化与字典组件**：quivr V2 通过 TEI 远程嵌入；本项目则在 `engineering/src/algo-prod/quantization/`（PQ / LvQ / `adaptive_quantization.c`）与 `engineering/src/algo-prod/dict/`（结巴分词 `dict_cut.c` / `dict_hmm.c`）自研底层组件，可在「自研 vs TEI 集成」决策上提供底层参考。
- **冷热分层与多模态存储**：quivr V2 通过 PostgreSQL + Weaviate + S3 三层存储实现冷热分层；本项目则实现多模态存储统一抽象（`engineering/include/db/mm_storage.h`、`engineering/src/db/storage/mm/mm_storage.c`），把 KV / Vector / Timeseries / Document / Spatial / Graph / Yang 七种数据模型统一管理，比 quivr V3 的「Postgres + Weaviate + S3」更通用。
- **学习建议**：阅读 quivr V2 源码重点关注「Go 微服务架构」「Content Engine 设计」「Temporal 异步任务编排」，与本项目 `engineering/src/db/distributed/` 分布式层 + `engineering/apps/multimodal_rag/` 应用层做架构对照，加深对「多存储引擎协同设计」的理解。