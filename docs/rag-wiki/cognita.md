# cognita Wiki（TrueFoundry 出品的模块化 RAG 框架）

> 参考版本：Cognita 0.x（2024-2026），由 TrueFoundry 出品的开源模块化 RAG 框架。本 wiki 重点介绍 cognita 作为「模块化、生产级 RAG」的定位、RAG 四维度扩展、模块化架构、源码学习路径与典型陷阱。

## 一、项目定位

cognita 是由 **TrueFoundry**（印度 ML 平台公司，创始人 Nimish Adani / Shubham Naik）于 **2024 年 5 月开源**的 **「模块化、生产级 RAG 框架」**，定位是「**为生产 RAG 应用提供模块化、可扩展、可观测的组件化基础**」。cognita 强调「**TrueFoundry 内部生产 RAG 的开源版**」，使用 FastAPI + Ray Serve，提供完整的部署 / 监控 / 扩展能力。

- **核心痛点**：
  - 现有 RAG 框架（langchain / haystack / llama-index）偏研究 / Demo，难以直接生产；
  - 缺乏**模块化** RAG 框架：每个组件可独立替换；
  - 缺乏**生产级** RAG 框架：监控 / 扩展 / 部署 / 权限；
  - 缺乏**多模态** RAG：文本 / 图像 / 音频 / 视频混合；
  - 缺乏**向量库无关**的 RAG：切换 Milvus / Qdrant / Weaviate 不需要重写业务代码；
  - TrueFoundry 内部生产 RAG 经验沉淀 → 开源；
- **典型用户**：
  - **企业 RAG 团队**：需要生产级、可监控、可扩展；
  - **MLOps 工程师**：需要部署 / 监控 / 扩展；
  - **数据科学家**：需要模块化实验；
  - **多模态应用**：文本 + 图像 + 音频；
  - **大模型平台公司**：需要私有部署 RAG 框架；

## 二、发展历史与社区

- **起源**（2024-05）：TrueFoundry 在内部生产 RAG 经验沉淀后，开源 cognita v0.1；
- **里程碑**：
  - 2024-05：v0.1，FastAPI + Ray Serve + Milvus / Qdrant；
  - 2024-08：v0.3，模块化 Parser（PDF / HTML / Docx / Audio / Video）；
  - 2024-10：v0.5，Multi-modal Parser（图像 / 视频帧）、MongoDB Metadata Store；
  - 2024-12：v0.6，Weaviate / SingleStore / LanceDB 集成；
  - 2025-03：v0.7，Example Query Controllers、Custom Indexing；
  - 2025-06：v0.8，Custom Embedding、Custom Rerank、Custom LLM；
- **维护状态**：**活跃**（maintained by TrueFoundry），社区 30+ 贡献者，月均 30+ commits；
- **社区**：
  - GitHub 2k+ star、Apache 2.0 License；
  - Discord 1k+ 成员、Twitter @ truefoundry；
  - 商业产品 TrueFoundry Platform（闭源 SaaS，MLOps / LLMOps 平台）；
- **关键贡献者**：Nimish Adani（CEO / Founder）、Shubham Naik（CTO）、Hardik Singh（核心架构）、社区贡献者 30+；

## 三、许可与商业限制

- **核心代码**：**Apache 2.0**（`backend/`、`frontend/`、`README.md`、`docker-compose.yaml`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **TrueFoundry Platform**（闭源 SaaS）：LLMOps / MLOps 平台，包含 LLM 路由、监控、A/B 测试、模型部署；
- **生态关系**：
  - **上游依赖**：`fastapi`、`ray`、`pydantic`、`pymongo`、`qdrant-client`、`milvus-sdk`、`weaviate-client`、`unstructured`、`pypdf`、`openai`、`cohere`、`voyageai`、`huggingface-hub`、`sentence-transformers`、`ollama`；
  - **下游生态**：约 **500+ 项目使用 cognita**，包括企业 RAG、MLOps 团队、TrueFoundry 用户；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **TrueFoundry 客户**：TrueFoundry 平台用户使用 cognita 搭建 RAG；
  - **多家企业 RAG 团队**：私有部署；
  - **教育 / 教学机构**：生产级 RAG 教学；
  - **多模态应用开发者**：图像 / 音频 RAG；
- **国内**：
  - 多家企业 RAG 团队；
- **典型场景**：
  - **企业生产 RAG**：私有部署 / 监控 / 扩展；
  - **多模态 RAG**：文本 + 图像 + 音频 + 视频；
  - **MLOps RAG**：部署 / 监控 / 升级；
  - **向量库无关 RAG**：切换 Milvus / Qdrant / Weaviate；
  - **LLM 多 Provider 切换**：OpenAI / Anthropic / Cohere / 自研 LLM；

## 五、一句话对比定位

> cognita 是 **「TrueFoundry 出品 + 模块化 + 生产级 + Ray Serve + 多模态 Parser + 多种 Vector DB + Apache 2.0 + TrueFoundry 商业矩阵 + MLOps 友好」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**txtai（一体化）**、**Verba（Weaviate 出品）**、**Quivr（个人知识库）** 构成 RAG 应用生态。cognita 的差异化是 **「TrueFoundry 出品 + 模块化 + Ray Serve + 多模态 + 生产级 + MLOps 友好」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（apps）                                   │
│        FastAPI / Streamlit / Web UI / Cockpit / Ray Dashboard       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Query Controllers（modules/query_controllers/）    │
│   QueryController.search / QueryController.process_query           │
│   Multimodal / Hybrid / Rerank / Custom                             │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  模块化抽象（backend/modules/）                      │
│   parsers / dataloaders / vector_db / metadata_store / model_gateway │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Parsers        │ │  Vector DB       │ │  Metadata Store   │ │ Model GW   │
│  Parser         │ │  Qdrant / Milvus │ │  MongoDB          │ │ OpenAI /   │
│  UnstructuredIO │ │  Weaviate / Mongo│ │  Postgres         │ │ Anthropic   │
│  WebParser      │ │  SingleStore     │ │  MySQL / LanceDB  │ │ Cohere /   │
│  AudioParser    │ │  （modules/      │ │                  │ │ HuggingFace│
│  VideoParser    │ │   vector_db/）   │ │ （modules/metadata│ │ Ollama /   │
│  MultiModalPars │ │                 │ │    _store/）      │ │ 自定义LLM  │
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────────────────┐
│                    Indexer Layer（backend/indexer/）                                  │
│    Embedding / Chunking / Uploading / Metadata / Custom Indexer                       │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Ray Serve / FastAPI 部署层                          │
│   Ray Actor / Ray Serve / Docker / K8s / TrueFoundry Platform       │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：FastAPI + Ray Serve；Ray Actor 处理 RAG 请求；Vector DB / MongoDB 外部；可独立扩展。

## 七、嵌入模型适配（EMB）

cognita 通过 `Embedder` 抽象（`backend/indexer/` 和 Model Gateway）暴露接口：

- **OpenAI**：`text-embedding-3-small/large`（1536/3072 维）、`AzureOpenAIEmbedder`；
- **Cohere**：`embed-english-v3.0` / `embed-multilingual-v3.0`（1024 维）；
- **HuggingFace / sentence-transformers**：`all-MiniLM-L6-v2`（384 维）、`bge-large-zh-v1.5`（1024 维）、`intfloat/e5-large-v2`；
- **Voyage AI**：`voyage-3`（1024 维）；
- **Google Vertex AI**：`text-embedding-005`；
- **Anthropic**：无原生 embedding（使用 Voyage AI）；
- **Ollama**：`nomic-embed-text` / `mxbai-embed-large`（本地）；
- **智源 BGE**：`BAAI/bge-large-zh-v1.5`（中文首选）；
- **通义 DashScope**：`text-embedding-v3`（1024 维）；
- **DeepSeek**：无原生 embedding（用 bge / m3e）；
- **Custom Embedding**：用户自定义 `BaseEmbedder`；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - `TokenChunker`（按 token 切，tiktoken）；
  - `SentenceChunker`（按句子切）；
  - `SemanticChunker`（基于嵌入相似度的语义切分）；
  - **推荐参数**：chunk_size 500-1500 token、overlap 10-20%；

## 八、文档解析器（LOAD）

cognita 通过 `Parser` 抽象（`backend/modules/parsers/`）暴露接口，下游 6+ 实现：

- **Parser**（`parser.py`，基础解析）：支持 TXT / Markdown / CSV；
- **UnstructuredIO**（`unstructured_io.py`，基于 `unstructured[all-in-one]`）：
  - PDF（PyMuPDF / pdfplumber / pdfminer）；
  - HTML（BeautifulSoup / selectolax）；
  - Docx（基于 `python-docx`）；
  - 图片（基于 `unstructured` 图像处理）；
  - 表格 / 公式 / 元数据抽取；
- **WebParser**（`web_parser.py`，基于 `trafilatura` / `selectolax` / `firecrawl`）：网页爬取 + 解析；
- **AudioParser**（`audio_parser.py`，基于 OpenAI Whisper / AssemblyAI）：音频转录；
- **VideoParser**（`video_parser.py`，基于 ffmpeg + Whisper）：视频帧抽取 + 音频转录；
- **MultiModalParser**（`multi_modal_parser.py`，基于 GPT-4V / Claude Vision / LLaVA）：图像 + 文本联合解析；
- **支持格式**：PDF / HTML / Docx / Markdown / TXT / CSV / JSON / 图像（PNG / JPG）/ 音频（MP3 / WAV）/ 视频（MP4）；

## 九、检索器与重排器（RETRIEVE / RERANK）

cognita 通过 `QueryController` 抽象（`backend/modules/query_controllers/`）暴露接口：

- **基础检索**（`base.py`）：
  - `QueryController.search(query)` → 向量检索；
  - `QueryController.process_query(query)` → 检索 + 重排 + 生成；
- **Multimodal**（`multimodal/`）：
  - 图像 + 文本联合检索；
  - 视频帧 + 音频转录检索；
- **Example**（`example/`）：
  - 标准 RAG 模板；
  - Hybrid Retrieval；
  - ReRank + Query Expansion；
- **Hybrid Retrieval**：
  - 向量 + BM25 融合；
  - RRF Reciprocal Rank Fusion；
- **重排（Rerank）**：
  - **Cohere**（`CohereRerank`，Cohere Rerank 3 / 3.5）；
  - **BGE**（`BgeRerank`，BAAI/bge-reranker-v2-m3）；
  - **Custom Rerank**：用户自定义；
  - **LLM-based Rerank**：用 LLM 打分；
- **过滤（Filters）**：
  - Vector DB where filter；
  - Metadata 过滤；
- **Custom Query Controller**：继承 `BaseQueryController`；

## 十、向量库后端（VECTOR STORE）

cognita 通过 `VectorDB` 抽象（`backend/modules/vector_db/`）暴露接口，下游 6+ 后端：

- **Qdrant**（`qdrant.py`，默认）；
- **Milvus**（`milvus.py`，单机 / 集群）；
- **Weaviate**（`weaviate.py`，内置混合检索 / rerank）；
- **MongoDB Atlas**（`mongo.py`，结合 Atlas Search）；
- **SingleStore**（`singlestore.py`，分布式 SQL）；
- **LanceDB**（社区，实验性）；
- **Client**（`client.py`，VectorDBClient 抽象）；
- **Base**（`base.py`，VectorDB 抽象类）；
- **配置**：
  - `QDRANT_URL` / `QDRANT_API_KEY`；
  - `MILVUS_URL` / `MILVUS_TOKEN`；
  - `WEAVIATE_URL` / `WEAVIATE_API_KEY`；
  - `MONGO_URL`；
  - `SINGLESTORE_URL`；
- **选型建议**：
  - 原型 / 小规模：Qdrant；
  - 中规模（10k - 1M）：Qdrant / Weaviate / Milvus；
  - 大规模（> 1M）：Milvus Cluster / Weaviate Cluster / SingleStore；
  - 已有 MongoDB：MongoDB Atlas；

## 十一、查询/计算/接口层

- **Query Controller**（`backend/modules/query_controllers/`）：
  - `BaseQueryController`（`base.py`，检索 + 生成抽象）；
  - `ExampleQueryController`（`example/`，标准 RAG 模板）；
  - `MultimodalQueryController`（`multimodal/`，多模态 RAG）；
  - **配置**：每个 Controller 独立配置 LLM / Embedder / Retriever / Rerank；
- **Model Gateway**（`backend/modules/model_gateway/`）：
  - LLM 抽象（OpenAI / Anthropic / Cohere / HuggingFace / Ollama / Custom）；
  - Embedder 抽象（同上）；
  - Reranker 抽象（Cohere / BGE / Custom）；
- **Indexer**（`backend/indexer/`）：
  - `Embedder` 抽象；
  - `Chunker` 抽象；
  - `Uploader` 上传到 Vector DB；
  - `Custom Indexer`：用户自定义 Indexer；
- **Metadata Store**（`backend/modules/metadata_store/`）：
  - MongoDB / Postgres / MySQL；
  - 存储文档元数据 / 用户信息 / 权限；
- **Database**（`backend/database/`）：
  - Metadata 持久化；
  - 用户 / 文档 / 收藏夹 / 评分；
- **Server**（`backend/server/`）：
  - FastAPI 路由；
  - 上传 / 检索 / 反馈 / 用户管理；
  - Cockpit 集成（TrueFoundry UI）；
- **Migration**（`backend/migration/`）：
  - 数据库 Schema 迁移；
- **流式响应**：
  - FastAPI StreamingResponse + SSE；
  - Ray Serve streaming；
- **评估与可观测性**：
  - **TrueFoundry Platform**：LLMOps / MLOps 监控；
  - **Phoenix**（Arize）；
  - **Langfuse**；
  - **Custom Evaluator**；
- **生产部署**：
  - **Ray Serve**（默认，分布式部署）；
  - **Docker Compose**（`backend/Dockerfile`、`deployment/`）；
  - **Kubernetes**（Helm chart，社区）；
  - **TrueFoundry Platform**（商业托管）；

## 十二、生态与集成

- **客户端**：Python SDK（FastAPI）、Web UI（Cockpit / Streamlit）；
- **集成矩阵**：
  - **LLM**：OpenAI、Anthropic、Cohere、HuggingFace、Ollama、Azure OpenAI、AWS Bedrock、Custom；
  - **Vector DB**：Qdrant、Milvus、Weaviate、MongoDB、SingleStore；
  - **Parser**：Unstructured、Whispered、ffmpeg、Firecrawl、Custom；
  - **Metadata Store**：MongoDB、Postgres、MySQL；
  - **Embedding**：OpenAI、Cohere、HuggingFace、Voyage AI、Vertex AI、Ollama、Custom；
  - **Rerank**：Cohere、BGE、Custom；
- **TrueFoundry Platform**：商业 LLMOps 平台；
- **Cockpit**：TrueFoundry 自带 UI（部分闭源）；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `backend/modules/query_controllers/base.py`：Query Controller 抽象；
  2. `backend/modules/query_controllers/example/`：标准 RAG 实现；
  3. `backend/modules/query_controllers/multimodal/`：多模态 RAG；
  4. `backend/indexer/`：索引抽象；
  5. `backend/modules/parsers/parser.py`：Parser 抽象；
  6. `backend/modules/parsers/unstructured_io.py`：Unstructured 集成；
  7. `backend/modules/parsers/multi_modal_parser.py`：多模态解析；
  8. `backend/modules/vector_db/base.py`：Vector DB 抽象；
  9. `backend/modules/vector_db/qdrant.py`：Qdrant 实现（默认）；
  10. `backend/modules/metadata_store/`：元数据存储；
  11. `backend/modules/model_gateway/`：LLM / Embedder / Rerank 抽象；
  12. `backend/server/`：FastAPI 路由；
  13. `backend/indexer.ts`：`ts` 索引器；
- **LOC 与语言**：
  - `backend/` ~10k LoC；
  - `frontend/` ~5k LoC（Cockpit / Streamlit）；
  - 主语言：Python（95%）、TypeScript（前端 5%）；
- **构建系统**：Python（Poetry / pip）、Frontend（npm）；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（`README.md`）；
  - 跑通 `docker-compose up`；
  - 上传 PDF / Markdown，提问；
  - 了解 Query Controller / Parser / Vector DB；
- **进阶（2-3 周）**：
  - 深入学习 Query Controller（标准 / 多模态）；
  - 实战 Custom Parser / Embedder / Rerank；
  - 学习 Ray Serve 部署；
  - 学习 Vector DB 切换；
- **高级（1-2 月）**：
  - 阅读源码（Query Controller + Parser + Vector DB）；
  - 自定义 Component；
  - 与 TrueFoundry Platform 集成；
  - 生产部署（Kubernetes / TrueFoundry）；
  - 多模态 RAG（图像 / 音频 / 视频）；
- **推荐资源**：
  - 官方文档：https://docs.truefoundry.com/cognita
  - GitHub：https://github.com/truefoundry/cognita
  - TrueFoundry 平台：https://truefoundry.com/
  - 论文：Ray Serve、Multi-modal RAG、HyDE、Self-RAG、GraphRAG；
  - 博客：https://www.truefoundry.com/blog

## 十五、风险与陷阱

- **项目相对早期**：cognita 仍在快速迭代，API 频繁变更；
- **Ray Serve 学习曲线**：Ray 框架本身学习曲线陡，新人难上手；
- **Vector DB 抽象不完整**：不同 Vector DB 的特性（如 Qdrant 的 payload 过滤）未完全抽象；
- **多模态解析性能**：图像 / 视频 RAG 处理时间长，需 GPU；
- **自定义 Parser 工作量大**：用户自定义 Parser / Embedder / Rerank 需要写大量样板代码；
- **Metadata Store 抽象有限**：MongoDB / Postgres / MySQL 切换不彻底；
- **评测 / 监控缺失**：默认无 RAG 评估，需自定义集成；
- **Observability 黑盒**：默认无 tracing，需自接 OpenTelemetry / Langfuse；
- **前端 Cockpit 闭源**：商业产品 TrueFoundry Platform 的 Cockpit 部分闭源；
- **依赖版本冲突**：Ray + FastAPI + Pydantic 版本升级时冲突；
- **社区生态较小**：相比 langchain / llama-index，社区贡献较少；

## 十六、参考资料

- 官方仓库：https://github.com/truefoundry/cognita
- 官方文档：https://docs.truefoundry.com/cognita
- TrueFoundry 平台：https://truefoundry.com/
- 论文：Ray Serve（Moritz et al. 2018, https://arxiv.org/abs/1712.05889）、Multi-modal RAG、HyDE（Gao et al. 2022, https://arxiv.org/abs/2212.10496）
- 博客：https://www.truefoundry.com/blog
- Tutorial：https://docs.truefoundry.com/cognita/getting-started
- 中文社区：https://docs.truefoundry.com/cognita