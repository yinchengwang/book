# langchain Wiki（LLM 应用编排框架）

> 参考版本：langchain 0.3 / 1.0（2024-2026），从 `langchain_classic/` 整合而来，模块化 monorepo 结构。本 wiki 重点介绍 langchain 作为「LLM 应用编排底座」的定位、RAG 四维度扩展（嵌入模型 / 文档解析器 / 检索器 / 向量库）、架构实现、源码学习路径与典型陷阱。

## 一、项目定位

langchain 是由 **Harrison Chase**（原 Robust Intelligence）于 2022-10 开源的 **「LLM 应用编排框架」**，目标是把 LLM 应用中的「提示工程、上下文拼接、工具调用、外部数据接入、多步推理、记忆、可观测性」抽象为可组合的 Runnable 协议，让开发者用一套统一的 `prompt | parser | llm` LCEL（LangChain Expression Language）语法就能拼装出生产级 LLM 应用。

- **核心痛点**：
  - LLM 应用中**提示工程 + 上下文拼接 + 工具调用 + 外部知识接入**的样板代码庞杂；
  - LLM 提供商众多（OpenAI / Anthropic / Cohere / HuggingFace / 智源 / 通义 / DeepSeek），切换成本高；
  - 多步推理、ReAct、Function Calling、Agent 循环等模式无统一抽象；
  - 工具调用、记忆、向量检索等模块无标准化接口；
  - **RAG** 流水线（加载 → 切分 → 嵌入 → 检索 → 重排 → 生成）的「胶水代码」需要快速搭建；
  - 评测、可观测性、追踪缺少统一中间件；
- **典型用户**：
  - LLM 应用开发者（搭建 Agent / RAG / 对话系统）；
  - 企业 AI 平台团队（统一封装多家 LLM 提供商）；
  - 研究人员（快速实验 ReAct / Self-RAG / HyDE 等论文方法）；
  - 教学 / 教程作者（langchain 是 LLM 应用教学的事实标准）；
  - 生产 RAG 系统（langchain 是 RAG 工程师的入门首选框架）；

口号：**"Build production-ready applications with LLMs through composability."**

## 二、发展历史与社区

- **起源**（2022-10）：Harrison Chase 在 Robust Intelligence 工作期间开源 langchain，初始目标是简化 ChatGPT + 工具调用的样板代码；
- **里程碑**：
  - 2022-10：v0.0.1，LLM Chain、PromptTemplate、SimpleSequentialChain；
  - 2023-01：v0.0.50+，引入 Agent（ReAct）、Memory、VectorStoreRetriever；
  - 2023-03：v0.0.100+，LangChainHub、DocumentLoader 扩展；
  - 2023-06：v0.0.200+，LCEL（LangChain Expression Language）提出，`prompt | llm | parser`；
  - 2023-10：v0.1.0，第一个正式版本（monorepo 化），`langchain-core` / `langchain-community` / `langchain` 三层拆分；
  - 2024-01：v0.1.5，`langgraph` 拆分独立（用于复杂 Agent 编排）；
  - 2024-05：v0.2.x，集成 Pydantic v2、LCEL 流式优化；
  - 2024-09：v0.3.x，`langchain_v1` 引入，`langchain_classic` 保留老接口作为兼容层；
  - 2025-10：v1.0 GA，统一 LCEL 入口、Runnable 协议稳定、移除 Legacy 启动项；
- **维护状态**：**活跃**（maintained by LangChain, Inc.），社区核心维护者 30+，月均 100+ commits；
- **社区**：
  - GitHub 100k+ star、MIT License；
  - Discord 50k+ 成员、Twitter 100k+ 关注；
  - 官方博客（blog.langchain.dev）、LangSmith（可观测性 SaaS）、LangGraph Platform（Agent 平台）形成完整商业矩阵；
- **关键贡献者**：Harrison Chase（创始人）、Erik Björk（核心架构）、Bagatur Askaryan（多模态）、LangChain 团队 30+ 工程师；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`libs/langchain-core/`、`libs/langchain/langchain_classic/`、`libs/langchain_v1/`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **LangSmith**（闭源 SaaS）：Tracing、Evaluation、Prompt Hub、Dataset Management，提供 free tier + 企业版；
  - **LangGraph Platform**（闭源 SaaS）：托管 Agent runtime、checkpoint、长任务；
  - **LangChain Templates**（开源仓库 `langchain-ai/langchain-templates`）：官方模板；
- **生态关系**：
  - **上游依赖**：`langchain-core`（Runnable / Message / PromptTemplate / BaseChatModel 抽象）、`pydantic`（>=2.0）、`typing-extensions`、`packaging`；
  - **下游生态**：约 **5000+ 项目使用 langchain**，包括 Replit、Cisco、Salesforce、Klarna、Shopify、Pinterest 等头部公司；
  - **同生态项目**：`langgraph`（Agent runtime）、`langsmith-sdk`（Python SDK）、`langchain-community`（社区集成 700+）、`langchain-aws` / `langchain-openai` / `langchain-anthropic`（官方 provider）；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **Klarna**（客服 AI）：基于 langchain 构建客服 RAG；
  - **Replit**（代码助手）：Agent 框架基于 langchain 早期版本；
  - **Cisco Webex**（IT Helpdesk）：内部知识库 RAG；
  - **Shopify**（电商搜索）：商品 RAG 基于 langchain + OpenAI；
  - **Pinterest**（视觉搜索）：RAG 相似图检索；
  - **Elastic**：langchain-elasticsearch 提供官方集成；
  - **Databricks**：langchain-databricks 集成；
- **国内**：
  - **阿里云通义千问**：官方 qwen-agent 参考 langchain 架构；
  - **百度智能云**、**字节火山方舟**：langchain 集成；
  - **网易 QAnything**（RAG 产品）：基于 langchain 早期版本 fork；
  - **多数 RAG 初创公司**默认基于 langchain；
- **典型场景**：
  - **客服 RAG**：Slack / 邮件 / 工单的语义检索；
  - **代码助手**：Replit / Cursor / Continue.dev 类；
  - **企业知识库**：Confluence / Notion / SharePoint 文档 RAG；
  - **多步 Agent**：ReAct / Plan-and-Execute / Self-Ask；
  - **流式对话**：ChatGPT-like 应用的流式响应；
  - **工具调用生态**：联网搜索、代码执行、API 调用；

## 五、一句话对比定位

> langchain 是 **「LLM 应用编排 + Runnable LCEL 统一抽象 + 700+ 集成 + MIT 与 LangSmith 商业矩阵 + 100k star + 行业事实标准」**；与 **LlamaIndex（专注 RAG 数据框架）**、**Haystack（专注 NLP 管道）**、**txtai（轻量语义搜索）**、**Semantic Kernel（微软 C# .NET）**、**DSPy（程序化优化提示）**、**Guidance（微软结构化生成）**、**CrewAI（多 Agent 协作）**、**Autogen（微软多 Agent）** 构成 LLM 应用框架生态。langchain 的差异化是 **「最广泛集成 + Runnable 抽象 + LangSmith 商业闭环 + Agent/RAG 全面」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                    用户应用层（apps）                                  │
│        FastAPI / Flask / Streamlit / Next.js 接入 langchain          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  LCEL 链式组合层（langchain-core）                    │
│      prompt | llm | parser | retriever | tool  Runnable 协议        │
│      invoke / stream / batch / async / configurable_alternatives    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Chat Models   │ │   Retrievers     │ │  Embeddings       │ │  Tools     │
│  ChatOpenAI    │ │  VectorStore     │ │  OpenAIEmbed      │ │  BaseTool  │
│  ChatBedrock   │ │  BM25Retriever   │ │  HuggingFaceEmb   │ │  ToolCalls │
│  ChatVertexAI  │ │  MultiQuery      │ │  CohereEmbed      │ │  Structured│
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────────────────┐
│                       Vector Stores / Document Loaders / Splitters                   │
│        Chroma / Milvus / pgvector / Pinecone / Weaviate / Qdrant / FAISS / LanceDB  │
│        PyPDF / Unstructured / Docx / HTML / Markdown / CSV / Notion / Slack         │
│        RecursiveCharacterTextSplitter / TokenTextSplitter / SemanticChunker        │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                 LangSmith 可观测性（langchain-core callbacks）       │
│      Tracer / Evaluator / Prompt Hub / Dataset / Feedback           │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：纯 Python 库（无独立进程），用户进程内 import 即用；LCEL 是声明式的链式抽象；LangSmith 通过 HTTP 上报 trace。

## 七、嵌入模型适配（EMB）

langchain 通过 `Embeddings` 抽象类（`libs/langchain/langchain_classic/embeddings/base.py`）暴露 `embed_documents(texts)` / `embed_query(text)` 接口，下游 50+ 实现：

- **OpenAI 系列**：`OpenAIEmbeddings`（`text-embedding-3-small/large`，维度 1536 / 3072，max input 8191 token）、`AzureOpenAIEmbeddings`、`OpenAIEmbeddings` v3；
- **Anthropic**：无原生 embedding（langchain 推荐用 Voyage AI）；
- **HuggingFace**：`HuggingFaceEmbeddings`（`BAAI/bge-small-en-v1.5`、`bge-large`、`bge-m3` 多语、`all-MiniLM-L6-v2` 384 维、`intfloat/e5-large-v2` 1024 维）、`HuggingFaceInstructEmbeddings`；
- **Cohere**：`CohereEmbeddings`（embed-english-v3.0 / embed-multilingual-v3.0，1024 维，支持 input_type=search_query / search_document）；
- **本地 sentence-transformers**：`SentenceTransformerEmbeddings`（`all-mpnet-base-v2`、`multi-qa-mpnet-base-dot-v1`），自动选 GPU；
- **Ollama**：`OllamaEmbeddings`（`nomic-embed-text`、`mxbai-embed-large`、`snowflake-arctic-embed`）；
- **智源 BGE**：`langchain_community.embeddings.BgeEmbeddings`（中文首选，bge-large-zh-v1.5 1024 维）；
- **通义 DashScope**：`DashScopeEmbeddings`（text-embedding-v3 / v2，1024 / 1536 维）；
- **DeepSeek**：暂未官方 embedding（仅 LLM）；**常见用法**：DeepSeek LLM + bge / m3e 嵌入；
- **Jina**：`JinaEmbeddings`（jina-embeddings-v2-base 768 维，8k context）；
- **Voyage AI**：`VoyageEmbeddings`（voyage-3 1024 维，Anthropic 推荐）；
- **Google Vertex AI**：`VertexAIEmbeddings`（text-embedding-005 / multimodalembedding）；
- **维度选择**：384（MiniLM）/ 768（bge-base / jina-v2）/ 1024（bge-large / cohere / qwen-v3）/ 1536（OpenAI v3 small）/ 3072（OpenAI v3 large）；
- **切分策略**：
  - `RecursiveCharacterTextSplitter`（`document_transformers/text_splitter.py`，默认 chunk_size=1000，chunk_overlap=200，按 `\n\n` / `\n` / `. ` 递归切分）；
  - `CharacterTextSplitter` / `TokenTextSplitter`（按 token 切，tiktoken）；
  - `MarkdownTextSplitter` / `HTMLTextSplitter`（结构感知）；
  - `SemanticChunker`（基于嵌入相似度的语义切分，langchain-experimental）；
  - **推荐参数**：chunk_size 500-1500 字符、overlap 10-20%；长文档（>10MB）建议先按 section 切分；

## 八、文档解析器（LOAD）

langchain 通过 `DocumentLoader` / `BaseBlobLoader` 抽象（`document_loaders/base.py`）暴露 `load()` / `lazy_load()` 接口，下游 100+ 实现：

- **PDF**：
  - `PyPDFLoader`（`pypdf` 库，简单 PDF，纯文本）；
  - `PyMuPDFLoader` / `PDFMinerLoader`（`fitz`，支持图像抽取）；
  - `UnstructuredPDFLoader`（`unstructured[local-inference]`，支持表格 / 图像 / 公式抽取）；
  - `MathpixLoader`（公式 OCR）；
  - `AmazonTextractPDFLoader`（AWS 云 OCR）；
  - `PyPDFDirectoryLoader`（批量加载）；
- **HTML**：`UnstructuredHTMLLoader`、`BSHTMLLoader`（BeautifulSoup）、`AspestHTMLLoader`（Aspest）；
- **Markdown**：`UnstructuredMarkdownLoader`；
- **Docx**：`UnstructuredWordDocumentLoader`、`Docx2txtLoader`；
- **CSV / Excel**：`CSVLoader`、`UnstructuredCSVLoader`、`PandasDataFrameLoader`、`UnstructuredExcelLoader`；
- **数据库 dump**：`SQLDatabaseLoader`（SQLAlchemy）、`NotionDBLoader`、`AirtableLoader`；
- **Notion / Confluence / Slack / GitHub**：`NotionDBLoader`、`ConfluenceLoader`、`SlackChatLoader`、`GitHubIssuesLoader`；
- **表格 / 图片 / 公式抽取**：
  - `UnstructuredPDFLoader(strategy="hi_res")`：调用 YOLO/XGBoost 模型识别 layout；
  - `UnstructuredImageLoader`：图像中的文字 / 公式抽取；
  - `MathpixLoader`：公式转 LaTeX；
- **OCR（Tesseract / PaddleOCR）**：
  - `TesseractOCRLoader`（langchain-community，依赖 `pytesseract`）；
  - `RapidOCRLoader`（基于 PaddleOCR/RapidOCR，中文友好）；
  - `PaddleOCRLoader`（中文 OCR 首选）；
- **多模态**：`UnstructuredIO`（图像 / 音频 / 视频元数据抽取）、`ImageCaptioningLoader`（BLIP / LLaVA）；

## 九、检索器与重排器（RETRIEVE / RERANK）

langchain 通过 `BaseRetriever` 抽象（`schema/retriever.py`）暴露 `get_relevant_documents` / `aget_relevant_documents`，支持 30+ 检索器：

- **基础检索**：
  - `VectorStoreRetriever`（默认余弦 / 内积）；
  - `BM25Retriever`（基于 rank_bm25，无嵌入）；
  - `TFIDFRetriever`（基于 scikit-learn）；
  - `SVMRetriever` / `KNNRetriever`；
- **多查询 / HyDE**：
  - `MultiQueryRetriever`（让 LLM 生成多个查询以扩大召回）；
  - `HydeRetriever`（Hypothetical Document Embeddings，先用 LLM 生成假设答案再检索）；
  - `SelfQueryRetriever`（用 LLM 把自然语言查询拆成结构化 metadata filter）；
- **混合检索**：
  - `EnsembleRetriever`（RRF Reciprocal Rank Fusion，融合 BM25 + Vector，权重可调）；
  - `ChainOfThoughtRetriever`（CoT 风格的检索）；
- **重排（Rerank）**：
  - `CohereRerank`（`langchain_cohere`，Cohere Rerank 3 / 3.5，cross-encoder 模式）；
  - `JinaRerank`（jina-reranker-v2-base-multilingual，多语）；
  - `BgeRerank`（BAAI/bge-reranker-v2-m3，cross-encoder）；
  - `FlashrankRerank`（轻量 ONNX 本地 rerank）；
  - `RankLLMRerank`（langchain-community，RankLLM）；
  - **典型 workflow**：VectorStoreRetriever.topK=20 → CohereRerank.topN=4；
- **GraphRAG / 知识图谱**：
  - `GraphSAGERetriever`、`KGPassRetriever`、`LLMGraphRetriever`（langchain-experimental）；
  - `Neo4jVector`、`NebulaGraphRetriever`、`TigerGraphRetriever`；
  - 微软 GraphRAG（社区集成）；
- **上下文压缩**：
  - `ContextualCompressionRetriever`（`LLMChainExtractor` / `EmbeddingsFilter`）；
  - `LongContextReorder`（长上下文重排，避免 lost-in-the-middle）；
- **Self-RAG / CRAG**：langchain-experimental 提供 `SelfRAGRetriever`、`CRAG`（Corrective RAG）；

## 十、向量库后端（VECTOR STORE）

langchain 通过 `VectorStore` 抽象（`vectorstores/base.py`）暴露 `add_texts` / `similarity_search` / `similarity_search_with_score` / `from_texts` / `as_retriever` 接口，下游 50+ 后端：

- **Chroma**：`Chroma`（`langchain_chroma`，嵌入式 / 客户端-服务器模式）；
- **Milvus**：`Milvus`（`langchain_milvus`，单机 / 集群，支持 HNSW / IVF / DiskANN）；
- **pgvector**：`PGVector`（`langchain_postgres`，PostgreSQL + pgvector 扩展）；
- **Pinecone**：`Pinecone`（`langchain_pinecone`，serverless / pod-based）；
- **Weaviate**：`Weaviate`（`langchain_weaviate`，内置混合检索 / rerank）；
- **Qdrant**：`Qdrant`（`langchain_qdrant`，Rust 实现，支持 payload 过滤）；
- **LanceDB**：`LanceDB`（`langchain_lancedb`，嵌入式列存向量库）；
- **FAISS**：`FAISS`（`langchain_community.vectorstores.faiss`，Facebook C++ 仿真库）；
- **ElasticSearch**：`ElasticsearchStore`（`langchain_elasticsearch`，结合倒排 + 向量）；
- **OpenSearch**：`OpenSearchVectorSearch`；
- **Vespa**：`VespaStore`；
- **Deep Lake**：`DeepLake`（Activeloop，多模态数据集）；
- **DuckDB**：`DuckDB`（`duckdb`，列存 + 向量扩展）；
- **TiDB**：`TiDBVectorStore`；
- **Astra DB (DataStax)**：`AstraDB`（云原生 Cassandra 向量）；
- **Supabase**：`SupabaseVectorStore`（pgvector 包装）；
- **Annoy**：`Annoy`（langchain-community，Spotify 随机投影树）；
- **ScaNN**：`ScaNN`（Google C++ 库）；
- **HNSWLib**：`HNSWLib`（Yury Malkov header-only HNSW）；
- **MemoryVectorStore**（langchain-core，进程内）；**Default**。
- **选型建议**：
  - 原型 / 小规模（< 100k）：MemoryVectorStore / FAISS / Chroma；
  - 中规模（100k - 1M）：Qdrant / Weaviate / pgvector / LanceDB；
  - 大规模（> 1M）：Milvus / Pinecone / Elasticsearch / Vespa；
  - 已有 Postgres：pgvector；
  - 多模态：Deep Lake / LanceDB；

## 十一、查询/计算/接口层

- **Runnable 协议**（`langchain-core/runnables/`）：
  - `Runnable` 是所有组件的统一接口（`invoke` / `stream` / `batch` / `ainvoke` / `astream` / `abatch`）；
  - `RunnableSequence`（`prompt | llm | parser`）；
  - `RunnableParallel`（`{key: chain}` 并行）；
  - `RunnablePassthrough` / `RunnableAssign`；
  - `RunnableBranch`（条件分支）；
  - `RunnableWithFallbacks`（降级链）；
  - `RunnableConfig`（configurable fields, callbacks, tags, metadata）；
- **Agent**：
  - **ReAct**：`create_react_agent`（`libs/langgraph/`，`libs/langchain/langchain_classic/agents/react/`）；
  - **Function Calling / Tool Use**：`create_openai_functions_agent` / `create_tool_calling_agent`；
  - **Structured Chat**：`create_structured_chat_agent`（多输入工具）；
  - **Self-Ask**：`create_self_ask_with_search_agent`；
  - **Plan-and-Execute**：`PlanAndExecute`（langchain-experimental）；
  - **OpenAI Tools**：`create_openai_tools_agent`（v0.3+ 推荐）；
  - **LangGraph Agent**：复杂状态机首选（参见 langgraph wiki）；
- **流式响应（SSE / WebSocket）**：
  - LCEL `.stream()` / `.astream()`（同步 / 异步生成器）；
  - `FastAPI StreamingResponse` + `astream` → SSE；
  - `astream_events`（v0.2+，细粒度事件：on_chat_model_stream / on_retriever_end / on_tool_start）；
  - WebSocket：`websockets` + `astream_log`（token 级日志）；
- **记忆系统**：
  - 短期：`ConversationBufferMemory`（in-memory 全量）、`ConversationBufferWindowMemory`（窗口）、`ConversationSummaryMemory`（摘要压缩）；
  - 长期：`VectorStoreRetrieverMemory`（向量库）、`ConversationKGMemory`（Neo4j 知识图）；
  - 文件：`FileChatMessageHistory`（JSON 文件持久化）；
  - Redis / Postgres：`RedisChatMessageHistory`、`PostgresChatMessageHistory`；
- **评估与可观测性**：
  - **LangSmith**（`langsmith`，官方 SaaS）：trace + evaluate + hub；
  - **RAGAS**：`langchain-ai/ragas` 集成（context_relevancy / faithfulness / answer_relevancy）；
  - **TruLens**：`trulens` 集成；
  - **LangFuse**：`langfuse` 开源替代；
  - **Phoenix（Arize）**：`arize-phoenix` 实验性；
  - 回调机制：`CallbackHandler` / `BaseCallbackManager`；
- **生产部署**：
  - FastAPI + Uvicorn（最常见）；
  - Ray Serve（langchain 提供 `RayServeDeployer`）；
  - Docker + Kubernetes；
  - BentoML / Modal / Anyscale（Serverless）；
  - LangGraph Platform（官方托管）；

## 十二、生态与集成

- **客户端 SDK**：Python（主力）、TypeScript / JS（`@langchain/core` / `@langchain/openai` / `@langchain/community`，与 Python API 对齐）、LangChain4j（Java）、LangChain.rb（Ruby）；
- **集成矩阵**：700+ 集成（langchain-community 仓库），覆盖：
  - LLM 提供商：OpenAI、Anthropic、Google Vertex、Cohere、HuggingFace、Mistral、AWS Bedrock、Azure OpenAI、Ollama、DeepSeek、智源 GLM、通义 Qwen、字节豆包、月之暗面 Kimi；
  - 向量库：上文 § 十分 50+；
  - 文档加载：上文 § 八节 100+；
  - 工具：Requests、Wikipedia、Sleep、ArXiv、PubMed、JSON、Tavily Search、SerpAPI、Bing Search、DuckDuckGo；
- **LangGraph**（独立项目）：复杂 Agent 状态机；
- **LangSmith**（商业 SaaS）：trace + eval + hub + dataset；
- **LangChain Hub**（Prompt 模板仓库）；
- **OpenGPTs**（开源 LangSmith 替代，基于 LangGraph）；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `libs/langchain-core/langchain_core/runnables/base.py`：Runnable 协议核心；
  2. `libs/langchain-core/langchain_core/prompts/`：PromptTemplate / ChatPromptTemplate；
  3. `libs/langchain-core/langchain_core/messages/`：HumanMessage / AIMessage / SystemMessage / ToolMessage；
  4. `libs/langchain-core/langchain_core/vectorstores.py`：VectorStore 抽象；
  5. `libs/langchain-core/langchain_core/retrievers.py`：BaseRetriever 抽象；
  6. `libs/langchain-core/langchain_classic/chains/retrieval_qa.base.py`：RetrievalQA 链实现；
  7. `libs/langchain-core/langchain_classic/agents/initialize.py`：Agent 初始化；
  8. `libs/langchain-core/langchain_classic/agents/agent.py`：Agent Executor（ReAct 主循环）；
  9. `libs/langchain-core/langchain_classic/document_loaders/base.py`：DocumentLoader 抽象；
  10. `libs/langchain-core/langchain_classic/document_transformers/text_splitter.py`：TextSplitter；
- **LOC 与语言**：
  - `langchain-core` ~30k LoC；
  - `langchain`（classic + v1）~80k LoC；
  - `langchain-community` ~150k LoC（含 700+ 集成）；
  - 主语言：Python（99%）、少量 TypeScript（JS 端）；
- **构建系统**：uv（现代）/ Poetry（传统）/ setuptools（兼容）；pytest + mypy + black + ruff；
- **LangGraph**（独立 monorepo）：状态机、checkpoint、human-in-the-loop；建议作为 langchain Agent 的进阶路径学习；

## 十四、学习路径

- **入门（1-2 周）**：
  - 阅读官方 Quickstart（`libs/langchain/README.md`）；
  - 学习 Runnable 协议 + LCEL 语法（`libs/langchain_core/runnables/base.py`）；
  - 跑通第一个 RAG（VectorStoreRetriever + RetrievalQA + OpenAI）；
  - 了解 Embeddings + VectorStore + Splitter 三大组件；
- **进阶（2-4 周）**：
  - 深入学习 Agent（ReAct / Function Calling）；
  - 学习 LangGraph（状态机 + checkpoint + Human-in-the-loop）；
  - 实战 Multi-Doc RAG（MultiQueryRetriever + EnsembleRetriever + CohereRerank）；
  - 学习 LangSmith（tracing + evaluation）；
- **高级（1-3 月）**：
  - 阅读源码（Runnable / Agent Executor / RetrievalQA）；
  - 自定义 VectorStore / Retriever / Embeddings / Tool；
  - 生产部署（FastAPI + Streaming + K8s）；
  - 自建 RAG 评估体系（RAGAS / TruLens）；
- **推荐资源**：
  - 官方文档：https://python.langchain.com/docs/
  - LangChain Hub：https://smith.langchain.com/hub
  - LangSmith：https://docs.smith.langchain.com/
  - DeepLearning.AI 课程：LangChain for LLM Application Development
  - 博客：blog.langchain.dev
  - 论文 / 概念：ReAct、Self-RAG、HyDE、GraphRAG、CRAG；

## 十五、风险与陷阱

- **抽象泄漏**：langchain 的 Chain / Agent 抽象经常随版本变更（如 AgentExecutor v0.0.x → v0.3 接口不一致），升级需重写代码；
- **依赖膨胀**：`langchain-community` 体积庞大（150k LoC、700+ 集成），生产中建议按需 install `langchain-openai`、`langchain-chroma` 等独立 provider；
- **回调性能开销**：LangSmith tracing 在高并发下可能成为瓶颈（每个 callback 上报），生产中可关闭部分 callback；
- **Prompt Injection**：langchain 自带的 `PromptTemplate` 不防注入，外部输入需自行清洗 / 验证；
- **LCEL vs 旧 Chain**：v0.1+ 强烈推荐 LCEL（`prompt | llm | parser`），不要用旧 `LLMChain` / `SequentialChain`；
- **Embeddings 缓存缺失**：默认每次重新 embed，生产应加 cache（`CacheBackedEmbeddings`，`langchain-core/caches/`）；
- **VectorStore 选型不当**：MemoryVectorStore 用于生产是反模式（进程内重启即丢失），应选持久化后端；
- **RAG 评估指标缺失**：仅看 "retrieved doc relevance" 不够，必须看 faithfulness / answer_relevancy（RAGAS）；
- **Agent 死循环**：ReAct Agent 不设 max_iterations 会无限循环；Function Calling 限制 max_tool_calls；
- **Observability 黑盒**：默认无 tracing，调试 Agent 决策路径困难，必须接 LangSmith 或 LangFuse；
- **Pydantic v1/v2 不兼容**：langchain 0.3+ 强制 Pydantic v2，旧代码需迁移；
- **LangChain vs LlamaIndex 误区**：RAG 数据框架选 LlamaIndex（更深度的 indexing API），应用编排选 langchain；两者常组合使用；

## 十六、参考资料

- 官方仓库：https://github.com/langchain-ai/langchain
- LangChain 文档：https://python.langchain.com/docs/
- LangSmith：https://docs.smith.langchain.com/
- LangGraph：https://langchain-ai.github.io/langgraph/
- 论文：ReAct（Yao et al. 2022, https://arxiv.org/abs/2210.03629）、HyDE（Gao et al. 2022, https://arxiv.org/abs/2212.10496）、Self-RAG（Asai et al. 2023, https://arxiv.org/abs/2310.11511）
- 博客：https://blog.langchain.dev/
- LCEL 概念：https://python.langchain.com/docs/concepts/lcel/
- RAG 教学：https://docs.ragas.io/
- LangChain 中文社区：https://www.langchain.com.cn/