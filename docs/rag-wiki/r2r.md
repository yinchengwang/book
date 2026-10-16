# r2r Wiki（SciPhi 出品的 RAG-as-a-Service 框架）

> 参考版本：R2R 3.x（2025-2026），由 SciPhi（AISquared 出品）出品发布的开源 RAG-as-a-Service 框架。本 wiki 重点介绍 r2r 作为「**云原生 RAG 平台**」的定位、RAG 四维度扩展、Pipe / Service 架构、Hatchet 编排、源码学习路径与典型陷阱。

## 一、项目定位

r2r（RAG to Riches）是 **SciPhi**（原 AISquared，创始人 Owen Colegrove）于 **2024 年 3 月开源**的 **「云原生 RAG-as-a-Service 框架」**，定位是「**提供生产级、可扩展、可观测的 RAG 平台**」。r2r 内置 Multi-Modal RAG、GraphRAG、Agent、Hybrid Search、Auth、Document Management、REST API、JS SDK，是当前最完整的「**全栈 RAG 平台**」之一。

- **核心痛点**：
  - RAG 应用缺少**云原生**架构（auth / multi-tenancy / observability）；
  - 缺少**Multi-Modal RAG** 一体化框架（文本 / 图像 / 音频 / 视频）；
  - 缺少**Hybrid Retrieval + Rerank + KG** 完整方案；
  - 缺少**Production-ready RAG**：Auth / Audit / Quota / Rate Limit；
  - 缺少**Pipe 抽象**：可组合的 RAG Pipeline（Ingestion Pipe / Embedding Pipe / Search Pipe / RAG Pipe / Agent Pipe）；
  - 缺少**Hatchet 编排**：基于 Hatchet 工作流引擎的分布式编排；
- **典型用户**：
  - **企业 RAG 团队**：需要生产级、可扩展；
  - **多租户 SaaS**：需要 Auth / 多租户隔离；
  - **多模态应用**：文本 + 图像 + 音频 + 视频；
  - **GraphRAG 应用**：需要 KG + 向量融合；
  - **RAG 平台公司**：需要 Multi-Tenant RAG 平台；
  - **数据科学家 / ML 工程师**：需要 Pipe 抽象做实验；

## 二、发展历史与社区

- **起源**（2024-03）：SciPhi 在 LLM Ops 趋势下开源 r2r v0.1，定位「**生产 RAG 平台**」；
- **里程碑**：
  - 2024-03：v0.1，FastAPI + LanceDB + OpenAI + Ingestion Pipe + Search Pipe + RAG Pipe；
  - 2024-06：v0.5，Multi-Modal RAG（图像 / 音频 / 视频）；
  - 2024-09：v1.0 GA，Pipe 抽象稳定，JS SDK；
  - 2024-12：v2.0，GraphRAG、KG、Hybrid Retrieval、Hatchet 编排；
  - 2025-03：v3.0，Auth / Multi-Tenancy / Audit / Observability；
  - 2025-06：v3.5，Agent Pipe、Tool Calling、Knowledge Graph；
  - 2025-09：v3.6+，Pipe 重构、Performance 优化；
- **维护状态**：**活跃**（maintained by SciPhi, Inc.），社区 50+ 贡献者，月均 50+ commits；
- **社区**：
  - GitHub 6k+ star、MIT License；
  - Discord 3k+ 成员、Twitter @ sciphi_ai；
  - 商业产品 SciPhi Cloud（闭源 SaaS，托管 R2R 服务 + Multi-Tenant）；
- **关键贡献者**：Owen Colegrove（CEO / Founder）、Nicola Evans（CTO）、SciPhi 团队 20+ 工程师、社区贡献者 30+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`py/core/`、`js/`、`README.md`、`Dockerfile`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **SciPhi Cloud**（闭源 SaaS）：托管 R2R 服务、Multi-Tenant、Auth、Observability、计费；
- **生态关系**：
  - **上游依赖**：`fastapi`、`pydantic`、`openai`、`anthropic`、`cohere`、`huggingface-hub`、`sentence-transformers`、`ollama`、`unstructured`、`pypdf`、`pymupdf`、`Pillow`、`opencv-python`、`ffmpeg-python`、`openai-whisper`、`qdrant-client`、`pgvector`、`lancedb`、`hatchet-sdk`、`neo4j`、`huggingface`；
  - **下游生态**：约 **1000+ 项目使用 R2R**，包括企业 RAG 团队、SaaS 公司、教育 / 教学机构、SciPhi Cloud 用户；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **SciPhi Cloud**：托管用户；
  - **多家企业 RAG 团队**：私有部署；
  - **SaaS 公司**：多租户 RAG 平台；
  - **教育 / 教学机构**：全栈 RAG 教学；
  - **多模态应用开发者**：文本 + 图像 + 音频；
- **国内**：
  - 多家企业 RAG 团队；
- **典型场景**：
  - **企业生产 RAG**：私有部署 / Auth / Multi-Tenancy；
  - **SaaS 多租户 RAG**：Auth / Quota / Billing；
  - **多模态 RAG**：文本 + 图像 + 音频 + 视频；
  - **GraphRAG**：KG + 向量融合；
  - **Hybrid Retrieval**：BM25 + 向量 + Rerank + KG；
  - **Agent RAG**：Tool Calling + Knowledge Graph；
  - **RAG 平台**：SciPhi Cloud 替代品；

## 五、一句话对比定位

> r2r 是 **「SciPhi 出品 + 云原生 + Multi-Tenant + Pipe 抽象 + Hatchet 编排 + Multi-Modal + GraphRAG + MIT + JS SDK + SciPhi Cloud 商业矩阵」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**txtai（一体化）**、**Verba（Weaviate 出品）**、**Cognita（TrueFoundry 出品）**、**Quivr（个人知识库）** 构成 RAG 平台生态。r2r 的差异化是 **「SciPhi 出品 + 云原生 + Multi-Tenant + Pipe + Hatchet + Multi-Modal + GraphRAG + JS SDK」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（apps）                                   │
│        Web App / JS SDK / REST API / OpenAPI / Hatchet Dashboard     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │ REST API + WebSocket
┌─────────────────────────────▼───────────────────────────────────────┐
│                  API 层（py/core/main/api/）                          │
│   FastAPI Router / OpenAPI / Auth Middleware / Multi-Tenant        │
│   v3 API / Legacy v1 API / Auth (JWT / API Key)                     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Pipe 抽象（py/core/main/assembly/）                 │
│   IngestionPipe / EmbeddingPipe / SearchPipe / RAGPipe / AgentPipe    │
│   MultiModalPipe / GraphRAGPipe / HybridSearchPipe                 │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────────┬───────────────┼────────────────┬──────────────────────┐
│              │               │                │                │
┌──────────────▼────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌────▼─────┐
│  Services       │ │  Providers        │ │  Parsers          │ │ Pipe Run │
│  IngestionService│ │  Embedding        │ │  PDF / HTML       │ │ Hatchet  │
│  SearchService  │ │  LLM              │ │  Docx / Audio     │ │ Simple   │
│  AuthService    │ │  Database         │ │  Video / Image    │ │ （orchest│
│  DocumentService│ │  Orchestration    │ │  OCR / MultiModal │ │  ration/）│
│  CollectionService│ │ Auth / Crypto    │ │                   │ │          │
│  AgentService   │ │ Email / OCR       │ │  （base/parsers/） │ │          │
└────────┬─────────┘ └─────────┬────────┘ └────┬──────────────┘ └────┬─────┘
         │                     │                │                    │
┌────────▼─────────────────────▼────────────────▼────────────────────▼─────┐
│                            Providers / Orchestration                        │
│   OpenAI / Anthropic / Cohere / HuggingFace / Ollama / Postgres / LanceDB  │
│   Qdrant / pgvector / Neo4j / Hatchet / Crypto / Email / OCR                │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Hatchet 工作流引擎（orchestration/hatchet/）        │
│   Distributed DAG / Workflow Step / Retry / Observability            │
│   替代实现：orchestration/simple/（轻量级 orchestrator）               │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：FastAPI（API 层）+ Hatchet（分布式工作流）+ Services（业务逻辑）+ Providers（基础设施）；支持 Simple Orchestrator（单机）+ Hatchet Orchestrator（分布式）。

## 七、嵌入模型适配（EMB）

r2r 通过 `EmbeddingProvider` 抽象（`py/core/base/providers/embedding.py`）暴露接口，下游 15+ 实现：

- **OpenAI**：`OpenAIEmbeddingProvider`（text-embedding-3-small/large 1536/3072 维）、`AzureOpenAIEmbeddingProvider`；
- **Anthropic**：无原生 embedding（推荐 Voyage AI）；
- **HuggingFace / sentence-transformers**：`SentenceTransformersEmbeddingProvider`（`all-MiniLM-L6-v2` 384 维、`bge-large-zh-v1.5` 1024 维）、`HuggingFaceTEIProvider`（TEI 服务端）；
- **Cohere**：`CohereEmbeddingProvider`（embed-english-v3.0 / embed-multilingual-v3.0 1024 维）；
- **Voyage AI**：`VoyageEmbeddingProvider`（voyage-3 1024 维）；
- **Google Vertex AI**：`VertexAIEmbeddingProvider`（textembedding-gecko / text-embedding-005）；
- **AWS Bedrock**：`BedrockEmbeddingProvider`；
- **Ollama**：`OllamaEmbeddingProvider`（`nomic-embed-text` / `mxbai-embed-large`）；
- **智源 BGE**：`BgeEmbeddingProvider`（中文首选）；
- **通义 DashScope**：`DashScopeEmbeddingProvider`；
- **DeepSeek**：无原生 embedding（用 bge / m3e）；
- **FastEmbed**：`FastEmbedEmbeddingProvider`（Qdrant 出品，ONNX 加速）；
- **Jina**：`JinaEmbeddingProvider`（jina-embeddings-v2-base 768 维）；
- **Custom**：`CustomEmbeddingProvider`（用户自定义）；
- **配置**：`all_possible_config.toml`；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - `RecursiveCharacterTextSplitter`（基于 langchain）；
  - `SemanticSplitter`（基于嵌入相似度）；
  - `TokenTextSplitter`（按 token 切，tiktoken）；
  - **推荐参数**：chunk_size 500-1500 token、overlap 10-20%；

## 八、文档解析器（LOAD）

r2r 通过 `Parser` 抽象（`py/core/base/parsers/`）暴露接口，下游 15+ 实现：

- **基础 Parser**：
  - `PDFParser`（基于 `pypdf` / `PyMuPDF`）；
  - `HTMLParser`（基于 `BeautifulSoup` / `selectolax` / `trafilatura`）；
  - `DocxParser`（基于 `python-docx`）；
  - `MarkdownParser`；
  - `CSVParser` / `JSONParser`；
  - `TextParser`；
- **多模态 Parser**：
  - `ImageParser`（基于 `PIL` / `opencv-python`）；
  - `AudioParser`（基于 `openai-whisper`）；
  - `VideoParser`（基于 `ffmpeg-python` + Whisper）；
  - `MultiModalParser`（基于 GPT-4V / Claude Vision / LLaVA）；
- **OCR Parser**：
  - `TesseractOCRProvider`（`tesseract` / `pytesseract`）；
  - `PaddleOCRProvider`（中文 OCR 首选）；
  - `EasyOCRProvider`（多语 OCR）；
- **特殊 Parser**：
  - `NotionParser`（社区）；
  - `ConfluenceParser`（社区）；
  - `GitHubParser`（社区）；
  - `WebCrawlerParser`（基于 `firecrawl` / `crawl4ai`）；
  - `DatabaseParser`（Postgres / MySQL）；
- **文件管理**：
  - `FileProvider`（`py/core/base/providers/file.py`）；
  - 支持 100+ 文件格式；
  - 自动检测 MIME 类型；

## 九、检索器与重排器（RETRIEVE / RERANK）

r2r 通过 `SearchService`（`py/core/main/services/search_service.py`）暴露检索：

- **基础检索**：
  - 向量检索（默认）；
  - **Hybrid Retrieval**：向量 + 全文检索融合；
  - **RRF Reciprocal Rank Fusion**：内置融合；
- **Graph Retrieval**：
  - **GraphRAG**：基于 Neo4j 的图检索；
  - **KG Pipe**：抽取三元组 → 写入 KG → 图遍历检索；
  - **Entity Retrieval**：实体识别 + 关系遍历；
- **Rerank**：
  - **Cohere Rerank**（Cohere Rerank 3 / 3.5）；
  - **BGE Rerank**（BAAI/bge-reranker-v2-m3）；
  - **LLM-based Rerank**：用 LLM 打分；
  - **Custom Rerank**：用户自定义；
- **过滤**：
  - Vector DB where filter；
  - Metadata 过滤；
  - **Multi-Tenant 过滤**：基于 user_id / tenant_id；
- **Advanced**：
  - **Query Expansion**：用 LLM 生成多个 query；
  - **HyDE**：先 LLM 生成假设答案再检索；
  - **Self-RAG**：检索 → 评估 → 重检索；
  - **CRAG**：Corrective RAG；
- **Agent Retrieval**：
  - **AgentService**（`py/core/main/services/agent_service.py`）；
  - Tool Calling + Knowledge Graph 检索；
  - Multi-Step Reasoning；

## 十、向量库后端（VECTOR STORE）

r2r 通过 `DatabaseProvider` 抽象（`py/core/base/providers/database.py`）暴露接口，下游 8+ 后端：

- **LanceDB**（默认，`LanceDBProvider`，嵌入式列存）；
- **Qdrant**（`QdrantProvider`，Rust 实现）；
- **pgvector**（`PGVectorProvider`，PostgreSQL + pgvector）；
- **Pinecone**（`PineconeProvider`，serverless / pod-based）；
- **Weaviate**（`WeaviateProvider`，内置混合检索）；
- **Milvus**（`MilvusProvider`，单机 / 集群）；
- **Chroma**（`ChromaProvider`，嵌入式 / 客户端-服务器）；
- **Neo4j**（`Neo4jProvider`，向量 + 图）；
- **MongoDB Atlas**（社区）；
- **配置**：`all_possible_config.toml`；
- **选型建议**：
  - 原型 / 小规模：LanceDB / Chroma；
  - 中规模（10k - 1M）：Qdrant / pgvector / LanceDB；
  - 大规模（> 1M）：Milvus / Pinecone / Weaviate；
  - GraphRAG：Neo4j + 向量库（双写）；
  - 多租户：Qdrant（payload 隔离）/ Pinecone（namespace）；

## 十一、查询/计算/接口层

- **Pipe 抽象**（`py/core/main/assembly/`）：
  - **IngestionPipe**：Parser → Chunker → Embedder → Uploader；
  - **EmbeddingPipe**：单独嵌入管道；
  - **SearchPipe**：向量 + Hybrid Search；
  - **RAGPipe**：SearchPipe + LLM 生成；
  - **AgentPipe**：Tool Calling + KG + Multi-Step；
  - **MultiModalPipe**：图像 + 文本 + 音频联合；
  - **GraphRAGPipe**：KG 抽取 + 图检索 + LLM 生成；
  - **HybridSearchPipe**：向量 + 全文 + Rerank；
  - **Custom Pipe**：用户自定义；
- **Orchestration**：
  - **Simple Orchestrator**（`orchestration/simple/`，单机 / 同步执行）；
  - **Hatchet Orchestrator**（`orchestration/hatchet/`，分布式 DAG + Workflow Step + Retry + Observability）；
- **Services**（`py/core/main/services/`）：
  - `IngestionService`（文档管理）；
  - `SearchService`（检索）；
  - `RAGService`（生成）；
  - `AgentService`（Agent）；
  - `DocumentService`（文档 CRUD）；
  - `CollectionService`（Collection 管理）；
  - `UserService`（用户管理）；
  - `AuthService`（认证）；
- **Auth & Multi-Tenant**（`py/core/base/providers/auth.py`）：
  - **JWT**（默认）；
  - **API Key**；
  - **OAuth 2.0**（社区）；
  - **Multi-Tenant 隔离**（基于 tenant_id / user_id）；
- **API 层**（`py/core/main/api/`）：
  - **FastAPI Router**；
  - **OpenAPI**；
  - **v3 API**（推荐）；
  - **Legacy v1 API**（兼容）；
  - **REST + WebSocket**；
- **流式响应**：
  - FastAPI StreamingResponse + SSE；
  - Hatchet streaming；
- **JS SDK**（`js/`）：
  - `@r2r/sdk`（TypeScript / JavaScript）；
  - OpenAPI 自动生成；
  - Browser / Node.js；
- **评估与可观测性**：
  - **Hatchet Dashboard**（工作流可视化）；
  - **Phoenix**（Arize）；
  - **Langfuse**；
  - **OpenTelemetry**；
  - **Custom Evaluator**；
- **生产部署**：
  - **Docker Compose**（`py/Dockerfile`、`deployment/docker/`）；
  - **Kubernetes**（Helm chart）；
  - **SciPhi Cloud**（官方托管）；
  - **Hatchet Distributed**（工作流分布式部署）；

## 十二、生态与集成

- **客户端**：Python SDK、JavaScript / TypeScript SDK（`js/`）、REST API、OpenAPI；
- **集成矩阵**：
  - **LLM 提供商**：上文 § 七节 15+；
  - **Vector DB**：上文 § 十节 8+；
  - **Parser**：上文 § 八节 15+；
  - **OCR**：Tesseract、PaddleOCR、EasyOCR；
  - **Orchestration**：Simple / Hatchet；
  - **Auth**：JWT / API Key / OAuth 2.0；
  - **Database**：Postgres（默认）/ MySQL / SQLite；
  - **Crypto**：用户密码加密 / API Key 生成；
  - **Email**：用户验证邮件；
  - **OCR Provider**（`py/core/base/providers/ocr.py`）；
  - **Scheduler**（`py/core/base/providers/scheduler.py`，任务调度）；
- **SciPhi Cloud**：商业 SaaS（Multi-Tenant RAG 平台）；
- **Hatchet**：分布式工作流引擎（`hatchet.run`）；
- **Dashboard**：Hatchet Dashboard + R2R Dashboard（社区）；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `py/core/main/api/`：FastAPI 路由；
  2. `py/core/main/assembly/`：Pipe 抽象；
  3. `py/core/main/orchestration/simple/`：Simple Orchestrator；
  4. `py/core/main/orchestration/hatchet/`：Hatchet Orchestrator；
  5. `py/core/main/services/`：业务逻辑；
  6. `py/core/base/providers/embedding.py`：Embedding 抽象；
  7. `py/core/base/providers/llm.py`：LLM 抽象；
  8. `py/core/base/providers/database.py`：Database / Vector DB 抽象；
  9. `py/core/base/providers/ocr.py`：OCR 抽象；
  10. `py/core/base/providers/auth.py`：Auth 抽象；
  11. `py/core/base/parsers/`：Parser 抽象；
  12. `py/core/agent/`：Agent 模块；
  13. `js/`：JS 客户端 SDK；
- **LOC 与语言**：
  - `py/core/` ~30k LoC；
  - `py/sdk/` ~5k LoC；
  - `js/` ~10k LoC；
  - 主语言：Python（80%）、TypeScript（20%）；
- **构建系统**：uv（现代）/ Poetry；pytest + mypy + ruff + vitest；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（`py/README.md`）；
  - 跑通 `docker compose up`；
  - 上传 PDF / Markdown，提问；
  - 了解 Pipe 抽象；
- **进阶（2-3 周）**：
  - 深入学习 Pipe（Ingestion / Search / RAG / Agent）；
  - 实战 Custom Pipe / Parser / Embedder；
  - 学习 Orchestration（Simple / Hatchet）；
  - 学习 Auth / Multi-Tenant；
- **高级（1-2 月）**：
  - 阅读源码（Pipe + Orchestrator + Provider）；
  - 自定义 Component；
  - GraphRAG（KG 抽取 + 图检索）；
  - 多模态 RAG（图像 / 音频 / 视频）；
  - 生产部署（Hatchet Distributed + Kubernetes）；
  - SciPhi Cloud 集成；
- **推荐资源**：
  - 官方文档：https://r2r-docs.sciphi.ai/
  - GitHub：https://github.com/SciPhi-AI/R2R
  - SciPhi Cloud：https://app.sciphi.ai/
  - 论文：RAG、Hatchet、GraphRAG、Self-RAG、HyDE、Multi-Modal RAG；
  - 博客：https://www.sciphi.ai/blog

## 十五、风险与陷阱

- **项目早期快速迭代**：r2r 仍在快速迭代，API 频繁变更（v0.x → v3.x 多次重大变更）；
- **架构复杂**：Pipe + Service + Provider + Orchestration 四层抽象，新人难理解；
- **Hatchet 依赖**：Hatchet Orchestrator 是核心依赖，部署复杂；
- **JS SDK 不完整**：部分高级功能（如 Agent / GraphRAG）JS SDK 支持有限；
- **Multi-Tenant 文档不全**：Multi-Tenant 配置复杂，文档较少；
- **Vector DB 抽象有限**：不同 Vector DB 的特性（如 Qdrant 的 payload 过滤）未完全抽象；
- **多模态解析性能**：图像 / 视频 RAG 处理时间长，需 GPU；
- **GraphRAG 复杂**：Neo4j + 向量库双写，部署复杂；
- **评测 / 监控缺失**：默认无 RAG 评估，需自定义集成；
- **SciPhi Cloud 锁定**：使用 SciPhi Cloud 后切换到本地部署困难；
- **依赖版本冲突**：Hatchet + FastAPI + Pydantic 版本升级时冲突；
- **社区生态中等**：相比 langchain / llama-index，社区贡献较少；

## 十六、参考资料

- 官方仓库：https://github.com/SciPhi-AI/R2R
- 官方文档：https://r2r-docs.sciphi.ai/
- SciPhi Cloud：https://app.sciphi.ai/
- Hatchet：https://hatchet.run/
- 论文：RAG、GraphRAG、HyDE、Self-RAG、Hatchet、Multi-Modal RAG
- 博客：https://www.sciphi.ai/blog
- Tutorial：https://r2r-docs.sciphi.ai/getting-started
- 中文社区：https://r2r-docs.sciphi.ai/