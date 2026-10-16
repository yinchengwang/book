# private-gpt Wiki（本地私有化 RAG 框架）

> 参考版本：private-gpt 0.6.x / 0.7.x（2024-2026），由 zylon-ai 出品的本地私有化 RAG 框架。本 wiki 重点介绍 private-gpt 作为「本地 / 私有部署 RAG」的定位、RAG 四维度扩展、组件化架构、源码学习路径与典型陷阱。

## 一、项目定位

private-gpt 是由 **Iván Martínez**（西班牙开发者，Zylon-ai 创始人）于 **2023 年 5 月开源**的 **「本地 / 私有部署 RAG 框架」**，定位是「**无需联网、无需云端、完全本地运行的私有 GPT**」。原项目由 Martelli Iván 在 GitHub 上创建，旨在为数据敏感的用户（企业 / 政府 / 个人隐私）提供「**完全本地**」的 RAG 体验，不依赖任何云 API。2024 年后由 Zylon-ai 团队维护，定位升级为「**生产级私有 RAG 框架**」，支持 LLM / 嵌入 / 检索 / 重排 / 解析等组件可插拔。

- **核心痛点**：
  - 现有 RAG 框架（langchain / llama-index / haystack）默认依赖云 API（OpenAI / Anthropic），数据外泄风险高；
  - 企业 / 政府 / 医疗 / 法律等数据敏感行业需要 **100% 本地部署**；
  - 个人隐私用户希望 **离线** 使用 RAG；
  - 缺少 **生产级私有 RAG 框架**：可监控、可扩展、可插拔；
  - 缺少 **无侵入式接入** 现有系统；
- **典型用户**：
  - **数据敏感企业**：金融 / 医疗 / 法律 / 政府；
  - **个人隐私用户**：离线使用 RAG；
  - **边缘部署**：无网络环境；
  - **研究 / 教育**：本地实验 LLM；
  - **OEM / 嵌入式**：白盒 RAG 框架；

## 二、发展历史与社区

- **起源**（2023-05）：Iván Martínez 在 GitHub 上开源 private-gpt，定位「100% 本地 GPT」，当时使用 llama.cpp + GGML 模型；
- **里程碑**：
  - 2023-05：v0.1，FastAPI + llama.cpp + sentence-transformers + Chroma5；
  - 2023-08：v0.3，多文档支持、Streamlit UI；
  - 2023-12：v0.4，LangChain 集成、多种 Embedding 后端；
  - 2024-03：v0.5，Ollama 集成、生产级配置；
  - 2024-09：v0.6，Zylon-ai 接手维护，重构为可插拔组件（Embedding / LLM / Retriever / Reranker）；
  - 2024-12：v0.7，Multi-Model、MCP（Model Context Protocol）、Skills 系统；
  - 2025-06：v0.8+，MCP Server、Celery / ARQ 任务队列；
  - 2025-10：v0.9+，稳定化 + Skills 系统 + 插件化；
- **维护状态**：**活跃**（maintained by Zylon-ai），社区 100+ 贡献者，月均 50+ commits；
- **社区**：
  - GitHub 21k+ star、Apache 2.0 License；
  - Discord 1k+ 成员、Twitter @ zylon_ai；
  - 商业产品 Zylon-ai（自研 LLM / RAG 平台，闭源 SaaS）；
- **关键贡献者**：Iván Martínez（创始人）、Zylon-ai 团队 10+ 工程师、社区贡献者 100+；

## 三、许可与商业限制

- **核心代码**：**Apache 2.0 License**（`private_gpt/`、`README.md`、`Dockerfile`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **Zylon-ai**（闭源 SaaS）：自研 LLM / RAG 平台；
- **生态关系**：
  - **上游依赖**：`fastapi`、`pydantic`、`openai`、`instructor`、`transformers`、`sentence-transformers`、`llama-cpp-python`、`ollama`、`chromadb`、`qdrant-client`、`docling`、`unstructured`、`pypdf`、`pdfplumber`、`pymupdf`、`Pillow`、`ffmpeg-python`、`openai-whisper`、`arq`、`celery`、`mcp`；
  - **下游生态**：约 **5000+ 项目使用 private-gpt**，包括企业私有 RAG、教育 / 教学、个人隐私用户、Zylon-ai 用户；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家数据敏感企业**：金融 / 医疗 / 法律 / 政府 / 国防；
  - **个人隐私用户**：离线 RAG；
  - **OEM / 嵌入式**：白盒 RAG 框架；
  - **教育 / 教学机构**：本地 LLM 实验；
  - **边缘部署**：无网络环境；
- **国内**：
  - 多家企业私有 RAG；
  - 政企 / 金融 / 医疗 / 法律；
- **典型场景**：
  - **企业内部知识库**：金融 / 医疗 / 法律 / 政府文档 RAG；
  - **个人隐私**：离线使用 RAG；
  - **OEM 嵌入**：白盒 RAG 框架；
  - **边缘部署**：无网络环境的工厂 / 船舶 / 飞机；
  - **研究 / 教育**：本地 LLM 实验；
  - **多语种**：本地 LLM 支持多语种；

## 五、一句话对比定位

> private-gpt 是 **「100% 本地私有 + Zylon-ai + 组件化 + Ollama / llama.cpp / vLLM + 多种 Embedding + Apache 2.0 + MCP + Skills + 21k star」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**txtai（一体化）**、**私人 GPT（本地）**、**Verba（Weaviate 出品）**、**Cognita（TrueFoundry 出品）**、**R2R（SciPhi 出品）**、**Quivr（个人知识库）** 构成 RAG 应用生态。private-gpt 的差异化是 **「100% 本地 + 21k star + 组件化 + Ollama + MCP + Skills + 私有部署首选」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（apps）                                   │
│        Gradio UI / FastAPI / Private Apps / OEM                     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │ REST API + SSE
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Server 层（private_gpt/server/）                    │
│   chat / chat_async / completion / content / embeddings / files     │
│   ingest / health / mcp / models / skills / tools / principals     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  组件层（private_gpt/components/）                   │
│   ingest / ingestion / embedding / llm / retriever / reranker       │
│   chunk / context / memory / markdown / code_execution              │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Ingest         │ │  Embedding        │ │  LLM              │ │ Retriever  │
│  Parse PDF/DOCX │ │  OpenAI / SBERT   │ │  OpenAI / Ollama  │ │ Vector /   │
│  Docling /      │ │  Instructor /     │ │  llama.cpp / vLLM │ │ Hybrid /   │
│  Unstructured   │ │  Azure / Voyage   │ │  Anthropic / Cohere││ │ BM25       │
│  Chunking       │ │  Local Embeddings │ │  Instructor        ││
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────────────────┐
│                    Storage & Engines                                                    │
│    ChromaDB / Qdrant / LanceDB / pgvector / FAISS / Elasticsearch                       │
│    Postgres / SQLite（metadata storage）                                                │
│    Redis（缓存 + 任务队列）                                                              │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  任务队列 / 异步层（celery / arq）                    │
│   Celery Worker / ARQ Worker / Redis Broker / Background Tasks        │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  MCP（Model Context Protocol）/ Skills                │
│   MCP Server / MCP Client / Skills Registry / Custom Tools          │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：FastAPI 主进程 + Celery / ARQ Worker（任务队列）+ Redis（缓存）；Gradio UI（默认前端）；可选 MCP Server / Client。

## 七、嵌入模型适配（EMB）

private-gpt 通过 `EmbeddingComponent`（`private_gpt/components/embedding/`）暴露抽象，下游 15+ 实现：

- **OpenAI**：`OpenAIEmbedding`（text-embedding-3-small/large 1536/3072 维，可选）；
- **HuggingFace / sentence-transformers**（默认本地）：
  - `SentenceTransformerEmbedding`（`all-MiniLM-L6-v2` 384 维、`bge-large-zh-v1.5` 1024 维）；
  - `BgeEmbedding`（BAAI/bge 系列，中文首选）；
  - `E5Embedding`（intfloat/e5-large-v2）；
  - `InstructorEmbedding`（hkunlp/instructor-xl）；
  - `JinaEmbedding`（jina-embeddings-v2-base 768 维）；
- **Cohere**：`CohereEmbedding`（embed-english-v3.0 / embed-multilingual-v3.0 1024 维，可选）；
- **Voyage AI**：`VoyageEmbedding`（voyage-3 1024 维，可选）；
- **Azure OpenAI**：`AzureOpenAIEmbedding`；
- **Google Vertex AI**：`VertexAIEmbedding`；
- **Ollama**：`OllamaEmbedding`（`nomic-embed-text` / `mxbai-embed-large`，本地）；
- **Custom Embedding**：`CustomEmbeddingComponent`（用户自定义）；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - **TokenChunker**（默认，按 token 切，tiktoken）；
  - **SentenceChunker**（按句子切）；
  - **SemanticChunker**（基于嵌入相似度的语义切分）；
  - **CodeChunker**（代码 AST 切分）；
  - **MarkdownChunker**（Markdown 结构感知）；
  - **HTMLChunker**（HTML 结构感知）；
  - **推荐参数**：chunk_size 500-1500 token、overlap 10-20%；

## 八、文档解析器（LOAD）

private-gpt 通过 `IngestComponent` / `ParseComponent`（`private_gpt/components/ingest/`）暴露接口，下游 10+ 实现：

- **PDF**：
  - `PyPDFParseComponent`（`pypdf`，简单 PDF，纯文本）；
  - `DoclingParseComponent`（`docling`，IBM 出品，高精度 PDF 解析，支持 OCR / 表格 / 公式）；
  - `UnstructuredParseComponent`（`unstructured[all-in-one]`，表格 / 图像 / 公式抽取）；
  - `PyMuPDFParseComponent`（`pymupdf`，支持图像 / 表格抽取）；
- **HTML**：`HTMLParseComponent`（基于 `BeautifulSoup` / `selectolax` / `trafilatura`）；
- **Markdown**：`MarkdownParseComponent`；
- **Docx**：`DocxParseComponent`（基于 `python-docx`）；
- **CSV / Excel**：`CSVParseComponent` / `ExcelParseComponent`（基于 `openpyxl` / `pandas`）；
- **JSON**：`JSONParseComponent`；
- **音频**：`WhisperAudioParseComponent`（基于 OpenAI Whisper）；
- **图像**：`ImageParseComponent`（基于 OCR + LLaVA / CLIP）；
- **代码**：`CodeParseComponent`（基于 AST 切分 Python / JS / Java / Rust / Go / C++）；
- **PDF 质量检测**：`PageQualityComponent`（检测扫描质量）；
- **PDF 页面分割**：`PDFPageSplitComponent`（按页切分）；
- **支持格式**：PDF / HTML / Docx / Markdown / TXT / CSV / Excel / JSON / 图像 / 音频 / 代码；
- **多模态**：
  - 通过 Docling 高精度 PDF（OCR + 表格 + 公式）；
  - 通过 Unstructured `hi_res` strategy 抽取图像 / 表格 / 公式；
  - 通过 Whisper 音频转录；
  - 通过 LLaVA / CLIP 图像理解；

## 九、检索器与重排器（RETRIEVE / RERANK）

private-gpt 通过 `RetrieverComponent`（`private_gpt/components/retriever/`）暴露接口：

- **基础检索**：
  - `VectorRetrieverComponent`（默认，余弦相似度）；
  - `BM25RetrieverComponent`（基于 `rank_bm25`，无嵌入）；
  - **Hybrid Retriever**：向量 + BM25 融合（RRF Reciprocal Rank Fusion）；
- **Metadata 过滤**：
  - 文档 / 用户 / 时间 / 标签过滤；
- **重排（Rerank）**：
  - `CrossEncoderRerankComponent`（基于 `cross-encoder` 库，本地）；
  - `LLMRerankComponent`（用 LLM 打分）；
  - `CohereRerankComponent`（Cohere Rerank 3 / 3.5，可选）；
  - `BgeRerankComponent`（BAAI/bge-reranker-v2-m3，可选）；
  - `JinaRerankComponent`（jina-reranker-v2-base-multilingual，可选）；
- **上下文压缩**：
  - `ContextualCompressionRetriever`（基于 LLM 抽取 / 压缩）；
  - `LongContextReorder`（避免 lost-in-the-middle）；
- **查询扩展**：
  - `QueryExpansionComponent`（用 LLM 生成多个 query）；
  - `HyDE`（Hypothetical Document Embeddings）；
  - `MultiQueryRetriever`；
- **Graph Retrieval**：
  - 社区集成（langchain 兼容）；
  - **GraphRAG**：基于 Neo4j / NetworkX（社区）；
- **Custom Retriever**：`CustomRetrieverComponent`（用户自定义）；

## 十、向量库后端（VECTOR STORE）

private-gpt 通过 `VectorStoreComponent` 暴露抽象，下游 10+ 后端：

- **ChromaDB**（默认，进程内 + 持久化）；
- **Qdrant**（`QdrantVectorStore`，Rust 实现）；
- **LanceDB**（`LanceDBVectorStore`，嵌入式列存）；
- **pgvector**（`PGVectorStore`，PostgreSQL + pgvector）；
- **FAISS**（`FaissVectorStore`，Facebook C++ 仿真库，进程内）；
- **Elasticsearch**（`ElasticsearchVectorStore`，结合倒排 + 向量）；
- **Pinecone**（`PineconeVectorStore`，serverless）；
- **Weaviate**（`WeaviateVectorStore`，内置混合检索）；
- **Milvus**（`MilvusVectorStore`，单机 / 集群）；
- **InMemory**（`InMemoryVectorStore`，进程内 / 测试用）；
- **Metadata 存储**（Postgres / SQLite / MySQL，通过 SQLAlchemy Engine 抽象）：
  - **Postgres**（`private_gpt/components/database/connection_factory.py` 通过 SQLAlchemy `create_engine` 支持，默认）；
  - **SQLite**（同上 `connection_factory.py`，本地嵌入式）；
  - **MySQL**（同上 `connection_factory.py`，社区）；
  - 注：`private_gpt/components/database/` 实际还提供 `function_inspector.py` / `procedure_inspector.py` / `table_inspector.py` / `view_inspector.py` / `inspected_schema.py` / `inspector_interface.py`，用于 RAG-on-SQL 的数据库对象内省（schema introspection），不是简单的连接配置模块；
- **缓存**：
  - **Redis**（`private_gpt/components/cache/`，查询结果缓存）；
- **任务队列**：
  - **Celery**（`private_gpt/celery/`，异步摄入任务）；
  - **ARQ**（`private_gpt/arq/`，轻量异步任务队列，集成 Redis）；
- **选型建议**：
  - 原型 / 小规模（< 100k）：ChromaDB / InMemory；
  - 中规模（100k - 1M）：Qdrant / LanceDB / pgvector；
  - 大规模（> 1M）：Milvus / Pinecone / Elasticsearch；
  - 已有 Postgres：pgvector；
  - 多租户：Qdrant / Pinecone（namespace）；

## 十一、查询/计算/接口层

- **Server 层**（`private_gpt/server/`）：
  - **chat**（`chat/`）：同步对话接口；
  - **chat_async**（`chat_async/`）：异步对话接口（SSE）；
  - **completion**（`completion/`）：补全接口；
  - **content**（`content/`）：内容管理；
  - **embeddings**（`embeddings/`）：嵌入接口；
  - **files**（`files/`）：文件管理；
  - **ingest**（`ingest/`）：摄入接口；
  - **health**（`health/`）：健康检查；
  - **mcp**（`mcp/`）：MCP Server；
  - **models**（`models/`）：模型列表；
  - **primitives**（`primitives/`）：基础类型；
  - **principal**（`principal.py`，身份认证）；
  - **skills**（`skills/`）：Skills 系统；
  - **tools**（`tools/`）：Tools 系统；
- **LLM 抽象**（`private_gpt/components/llm/`）：
  - **OpenAI**：`OpenAILLM`（gpt-4o / gpt-4-turbo / gpt-3.5-turbo，可选）；
  - **Anthropic**：`AnthropicLLM`（claude-3-5-sonnet / claude-3-opus，可选）；
  - **Cohere**：`CohereLLM`（command-r-plus，可选）；
  - **Ollama**（推荐本地）：`OllamaLLM`（llama3 / qwen2.5 / deepseek-r1 等）；
  - **llama.cpp**（推荐本地）：`LlamaCppLLM`（GGUF 模型，本地 CPU）；
  - **vLLM**（推荐本地）：`VllmLLM`（高性能推理）；
  - **HuggingFace**：`HuggingFaceLLM`（本地 Transformers）；
  - **Google Vertex AI**：`VertexAILLM`；
  - **Azure OpenAI**：`AzureOpenAILLM`；
  - **AWS Bedrock**：`BedrockLLM`；
  - **Custom LLM**：`CustomLLMComponent`（用户自定义）；
- **Ingest 工作流**（`private_gpt/components/ingest/`）：
  - `ParseComponent` → `ChunkComponent` → `EmbedComponent` → `StorageComponent`；
  - 异步执行（Celery / ARQ）；
  - 进度跟踪（`fake_progress.py`）；
  - 元数据管理（`metadata_helper.py`）；
- **Chat 工作流**（`private_gpt/components/chat/`）：
  - `RetrieverComponent` → `ContextComponent` → `PromptComponent` → `LLMComponent`；
  - 流式响应（`chat_async/`）；
  - 上下文管理（`context/`）；
- **Memory 系统**（`private_gpt/components/memory/`）：
  - **InMemory**（默认，进程内）；
  - **Persistent**（数据库 / Redis / 文件）；
  - **VectorMemory**（基于向量库的语义记忆）；
- **MCP（Model Context Protocol）**（`private_gpt/server/mcp/`）：
  - **MCP Server**（`MCPComponent`，对外暴露 MCP 服务）；
  - **MCP Client**（`MCPClient`，连接其他 MCP 服务）；
  - **Tool Sharing**：通过 MCP 共享 Tools；
- **Skills 系统**（`private_gpt/server/skills/`）：
  - **Skill Registry**：注册自定义 Skills；
  - **Markdown Skills**：基于 Markdown 的 Skills；
  - **Custom Skills**：用户自定义 Skills；
- **Tools 系统**（`private_gpt/server/tools/`）：
  - **Function Calling**；
  - **Built-in Tools**：Search、Web、Coding、Math 等；
  - **Custom Tools**：用户自定义 Tools；
- **Code Execution**（`private_gpt/components/code_execution/`）：
  - **Python Execution**：安全沙箱执行 Python 代码；
  - **Sandbox**：基于 Docker / RestrictedPython；
- **Markdown Rendering**（`private_gpt/components/markdown/`）：
  - **Markdown Renderer**：渲染 LLM 输出的 Markdown；
  - **Code Highlight**：代码高亮；
- **流式响应**：
  - FastAPI StreamingResponse + SSE；
  - async generator；
- **评估与可观测性**：
  - **Custom Evaluator**：用户自定义评估；
  - **Langfuse 集成**（社区）；
  - **Phoenix 集成**（社区）；
  - **OpenTelemetry 集成**（部分支持）；
- **生产部署**：
  - **Docker Compose**（`Dockerfile`）；
  - **Kubernetes**（Helm chart，社区）；
  - **Zylon-ai Cloud**（商业托管）；

## 十二、生态与集成

- **客户端**：Gradio UI（默认，浏览器）、FastAPI（Python SDK）、REST API、OpenAPI；
- **集成矩阵**：
  - **LLM 提供商**：OpenAI、Anthropic、Cohere、Ollama、llama.cpp、vLLM、HuggingFace、Google Vertex AI、Azure OpenAI、AWS Bedrock、Custom；
  - **向量库**：ChromaDB（默认）、Qdrant、LanceDB、pgvector、FAISS、Elasticsearch、Pinecone、Weaviate、Milvus；
  - **Parser**：PyPDF、Docling、Unstructured、PyMuPDF、HTML、Markdown、Docx、Whisper、Code；
  - **重排**：CrossEncoder（默认）、LLM、Cohere、BGE、Jina、Custom；
  - **任务队列**：Celery、ARQ（Redis）；
  - **缓存**：Redis；
  - **Metadata 存储**：Postgres（默认）、SQLite、MySQL；
  - **MCP**：MCP Server、Client；
  - **Skills**：Markdown Skills、Custom Skills；
  - **Tools**：Function Calling、Built-in Tools、Custom Tools；
  - **Code Execution**：Docker / RestrictedPython Sandbox；
- **Zylon-ai Cloud**：商业 SaaS；
- **OEM 嵌入**：白盒 RAG 框架，可嵌入到企业产品；
- **社区**：
  - Discord 1k+ 成员；
  - 中文社区；
  - 教育 / 教学示例；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `private_gpt/main.py`：应用入口；
  2. `private_gpt/launcher.py`：Launcher；
  3. `private_gpt/di.py`：依赖注入（`private_gpt/di.py`）；
  4. `private_gpt/server/chat/`：Chat 接口；
  5. `private_gpt/server/chat_async/`：流式 Chat 接口；
  6. `private_gpt/server/ingest/`：Ingest 接口；
  7. `private_gpt/server/embeddings/`：Embedding 接口；
  8. `private_gpt/server/files/`：Files 接口；
  9. `private_gpt/components/ingest/ingest_component.py`：Ingest 组件；
  10. `private_gpt/components/ingest/parse_component.py`：Parse 组件；
  11. `private_gpt/components/embedding/`：Embedding 组件；
  12. `private_gpt/components/llm/`：LLM 组件；
  13. `private_gpt/components/retriever/`：Retriever 组件；
  14. `private_gpt/components/chunk/`：Chunk 组件；
  15. `private_gpt/components/context/`：Context 组件；
  16. `private_gpt/components/memory/`：Memory 组件；
  17. `private_gpt/celery/`：Celery 任务队列；
  18. `private_gpt/arq/`：ARQ 任务队列；
  19. `private_gpt/server/mcp/`：MCP Server / Client；
  20. `private_gpt/server/skills/`：Skills 系统；
  21. `private_gpt/server/tools/`：Tools 系统；
- **LOC 与语言**：
  - `private_gpt/` ~25k LoC；
  - `ui/`（Gradio 前端，~5k LoC）；
  - 主语言：Python（95%）、TypeScript（前端 5%）；
- **构建系统**：Poetry / setuptools；pytest + mypy + ruff；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（`README.md`）；
  - 跑通本地 Ollama + sentence-transformers + ChromaDB；
  - 上传 PDF / Markdown，提问；
  - 了解 Gradio UI；
- **进阶（2-3 周）**：
  - 深入学习组件化架构（Ingest / Embedding / LLM / Retriever）；
  - 实战 Custom Component；
  - 学习 MCP 协议；
  - 学习 Celery / ARQ 任务队列；
- **高级（1-2 月）**：
  - 阅读源码（Ingest / Chat 工作流）；
  - 自定义 Skills / Tools；
  - 多租户 / RBAC 实现；
  - 生产部署（Kubernetes + Docker）；
  - Zylon-ai Cloud 集成；
  - Code Execution Sandbox；
- **推荐资源**：
  - 官方文档：https://docs.privategpt.dev/
  - GitHub：https://github.com/zylon-ai/private-gpt
  - Zylon-ai：https://zylon.ai/
  - 论文：RAG、llama.cpp、Ollama、Docling、MCP、Function Calling；
  - 博客：https://zylon.ai/blog

## 十五、风险与陷阱

- **本地 LLM 性能瓶颈**：本地 LLM 推理速度慢（特别是大模型），生产中需要 GPU + vLLM；
- **本地嵌入性能**：本地 sentence-transformers 性能受限，建议用 TEI / vLLM 加速；
- **Celery / ARQ 复杂度**：异步任务队列增加部署复杂度（需 Redis + Worker）；
- **Metadata 存储限制**：默认 Postgres 适合中等规模，超大规模需分库分表；
- **MCP 协议较新**：MCP 协议仍在演进，兼容性需关注；
- **Skills 系统较新**：Skills 系统文档较少，需自行探索；
- **OEM 嵌入复杂**：白盒 RAG 框架虽然可定制，但 OEM 集成工作量大；
- **Zylon-ai Cloud 锁定**：使用 Zylon-ai Cloud 后切换到本地部署需迁移；
- **缺少多模态画像**：图像 / 视频支持较弱，Docling 主要解决 PDF；
- **RAG 评估缺失**：默认无 RAG 评估，需自定义集成；
- **Observability 黑盒**：默认无 tracing，需自接 OpenTelemetry / Langfuse；
- **社区生态中等**：相比 langchain / llama-index，社区贡献较少（但稳定）；
- **版本兼容性**：v0.5 → v0.6 重构组件化，v0.6 → v0.7 引入 MCP，部分 API 变更；
- **License 注意**：Apache 2.0 + 部分第三方依赖（`docling`、`unstructured`）许可协议兼容问题；

## 十六、参考资料

- 官方仓库：https://github.com/zylon-ai/private-gpt
- 官方文档：https://docs.privategpt.dev/
- Zylon-ai：https://zylon.ai/
- Docling：https://github.com/DS4SD/docling
- Ollama：https://ollama.ai/
- llama.cpp：https://github.com/ggerganov/llama.cpp
- vLLM：https://docs.vllm.ai/
- MCP：https://modelcontextprotocol.io/
- 论文：RAG、llama.cpp、Docling、MCP、Function Calling
- 博客：https://zylon.ai/blog

## 十七、与本项目关联

private-gpt 作为「本地优先 + Ollama/llama.cpp + RAG-on-SQL 文档问答」框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「本地化嵌入 + 文档摄取 + 元数据持久化 + 多 Vector DB 后端」四个维度有直接参考价值：

- **本地嵌入模型适配**：private-gpt 默认走 Ollama / llama.cpp（GGUF 量化），本项目则在 `engineering/apps/multimodal_rag/src/index/hnsw_persist.c`（`hnsw_persist.h`）实现 C/C++ 端 HNSW 持久化索引，可作为 private-gpt `IngestionPipeline` 的底层 ANN 实现之一；向量索引层的核心实现见 `engineering/src/db/index/vector_index/hnsw_pq/hnsw_pq.c`（HNSW + 乘积量化）与 `engineering/include/db/index/vector_index/ivf/inverted_lists.h`（IVF 倒排链）。
- **RAG-on-SQL 数据库内省**：private-gpt 的 `private_gpt/components/database/` 子树包含 `connection_factory.py` / `function_inspector.py` / `procedure_inspector.py` / `table_inspector.py` / `view_inspector.py` / `inspected_schema.py`，把「自然语言 → SQL 查询」拆为 schema introspection + query generation 两阶段。本项目则通过 PostgreSQL 风格的 Catalog 模块实现数据库元数据管理，参见 `engineering/include/db/catalog.h` 与 `engineering/src/db/storage/catalog/`（OID 分配、表/列/索引元数据），可类比 private-gpt 的 introspection 设计思路。
- **多 Vector DB 后端抽象**：private-gpt 通过 `VectorStoreComponent` 抽象支持 Chroma / Qdrant / Milvus / pgvector / LanceDB / FAISS / Pinecone 等十余种后端；本项目 wiki 在 `docs/db-wiki/vector/` 已系统整理五种主流向量库的工程视角（`docs/db-wiki/vector/chroma.md`、`docs/db-wiki/vector/milvus.md`、`docs/db-wiki/vector/faiss.md`、`docs/db-wiki/vector/lancedb.md`、`docs/db-wiki/vector/qdrant.md`），可作为 Vector DB 选型决策的对照参考。
- **文档摄取与多模态 Parser**：private-gpt 通过 Docling + Unstructured 实现高质量 PDF / Docx 解析，本项目多模态 RAG 应用层（`engineering/apps/multimodal_rag/`）同样提供 PDF / 图像 / 视频多模态摄取，可对照学习摄取管线的 chunking / embedding / 上传三段式设计。
- **冷热分层与存储抽象**：private-gpt 支持 ingest 完成的文档 metadata 落入 Postgres / SQLite；本项目则实现多模态存储统一抽象（KV / Vector / Timeseries / Document / Spatial / Graph / Yang），参见 `engineering/include/db/mm_storage.h` 与 `engineering/src/db/storage/mm/mm_storage.c`，比 private-gpt 的「单一关系型后端」更通用。
- **GUC 配置系统**：private-gpt 通过环境变量 + Settings (pydantic-settings) 管理 LLM / Embedding / Vector DB 连接；本项目实现 PostgreSQL 风格的 GUC 统一配置（`engineering/src/db/core/guc.c` 与 `engineering/include/db/core/guc.h`），支持 `shared_buffers` / `work_mem` / `wal_level` 等运行时参数化配置。
- **量化与字典组件**：private-gpt 不自研量化与分词器；本项目则在 `engineering/src/algo-prod/quantization/`（PQ / LvQ / `adaptive_quantization.c`）与 `engineering/src/algo-prod/dict/`（结巴分词 `dict_cut.c` / `dict_hmm.c` / `dict_load.c`）自研底层组件，可在「本地嵌入 + 量化索引」方向对照学习。
- Tutorial：https://docs.privategpt.dev/
- Discord：https://discord.gg/zylon-ai