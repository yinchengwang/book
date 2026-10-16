# verba Wiki（Weaviate 出品的 RAG Web 应用）

> 参考版本：Verba 3.x（2025-2026），由 Weaviate 出品的开源 RAG 聊天应用。本 wiki 重点介绍 verba 作为「Weaviate 官方 RAG UI」的定位、与 Weaviate 的紧密集成、RAG 四维度扩展、组件架构、源码学习路径与典型陷阱。

## 一、项目定位

verba 是由 **Weaviate**（向量数据库公司，创始人 Bob van Luijt）于 **2023 年 9 月开源**的 **「开源 RAG Web 应用」**，定位是「**基于 Weaviate 的端到端 RAG 聊天 UI**」。verba 提供完整的前端（Next.js）+ 后端（FastAPI），开箱即用支持文档解析、嵌入、检索、生成、可视化配置，是 Weaviate 生态的「官方 RAG 应用」。

- **核心痛点**：
  - 企业 / 个人想要快速搭建 RAG Web 应用，但前端 + 后端 + 向量库 + LLM 集成工作量大；
  - 需要可视化的 RAG 配置（嵌入模型选择 / 切分策略 / 检索参数）；
  - Weaviate 用户需要一个「端到端」的可视化 RAG 体验；
  - 现有 RAG 框架（langchain / llama-index）需要自行搭建 UI；
  - 生产级 RAG 需要 Streaming、Source 展示、Config UI、User feedback 等；
  - 缺乏「**Vector DB 自带的 RAG 应用**」（Weaviate 出品的「官方应用」）；
- **典型用户**：
  - **Weaviate 用户**：想要官方 RAG UI；
  - **企业 / 个人**：想要快速搭建 RAG Web 应用；
  - **RAG 入门开发者**：可视化学习 RAG 各组件；
  - **数据科学家**：可视化的 RAG 实验；
  - **客服 / 文档问答**：可视化生产 RAG；

## 二、发展历史与社区

- **起源**（2023-09）：Weaviate 在欧洲 Vector Search Day 公开 verba v1.0，定位「Weaviate 官方 RAG 应用」；
- **里程碑**：
  - 2023-09：v1.0，FastAPI + Weaviate + Next.js 基础架构；
  - 2023-12：v1.5，Reader 扩展（PDF / HTML / Docx / Notion / GitHub）；
  - 2024-03：v2.0，重写前端（Next.js App Router）、UpstageDocumentParse 集成、Streaming 优化；
  - 2024-06：v2.4，多 LLM 集成（OpenAI / Anthropic / Cohere / Google / Mistral / Groq / Upstage / Ollama）；
  - 2024-09：v2.5，Weaviate Embeddings、Upstage Solar Embeddings、Hybrid Retrieval；
  - 2024-12：v2.7，RAG Configurator（可视化配置）、Caching、Evaluation 集成；
  - 2025-06：v3.0，Plugin 系统、多模态（图像）、Custom Components；
- **维护状态**：**活跃**（maintained by Weaviate），社区 30+ 贡献者，月均 50+ commits；
- **社区**：
  - GitHub 6k+ star、MIT License；
  - Discord 1k+ 成员（Weaviate 官方 Discord）、Twitter @ weaviate_io；
  - 商业产品 Weaviate Cloud（闭源 SaaS，托管 Weaviate）；
- **关键贡献者**：Bob van Luijt（Weaviate CEO）、Mohamed Shahin（Verba 主架构）、Leonie Heuing（前端）、社区贡献者 30+；

## 三、许可与商业限制

- **核心代码**：**MIT License**（`goldenverba/`、`frontend/`、`README.md`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **Weaviate Cloud**（闭源 SaaS）：托管 Weaviate Serverless / Weaviate Enterprise Cloud；
  - **Weaviate Embedded**：开源（Apache 2.0）；
- **生态关系**：
  - **上游依赖**：`weaviate-client`、`fastapi`、`pydantic`、`openai`、`cohere`、`voyageai`、`huggingface-hub`、`sentence-transformers`、`ollama`、`unstructured`、`firecrawl-py`、`assemblyai`、`gitpython`、`next.js`、`react`、`typescript`；
  - **下游生态**：约 **1000+ 项目使用 verba**，包括 Weaviate 用户、教育 / 教学、企业 RAG Demo；

## 五、业界主要使用方与案例

- **国际**（公开）：
  - **Weaviate 官方**：所有 Demo / 教程使用 verba；
  - **多家初创公司**：基于 verba 搭建 RAG 应用；
  - **教育 / 教学机构**：Weaviate 课程；
  - **企业 / 个人**：基于 verba 私有化部署；
- **国内**：
  - 多家 Weaviate 用户；
  - Weaviate 中文社区；
- **典型场景**：
  - **企业 RAG Demo**：快速搭建演示；
  - **客服 / 文档问答**：基于 verba 二次开发；
  - **RAG 学习 / 教学**：可视化学习 RAG；
  - **数据科学家实验**：可视化 RAG 实验；
  - **生产级 RAG 应用**：基于 verba 二次开发；
  - **Weaviate 用户**：官方 RAG 应用；

## 五、一句话对比定位

> verba 是 **「Weaviate 官方 RAG UI + Next.js + FastAPI + Reader 7+ + Embedder 8+ + Generator 6+ + MIT + RAG Configurator + Streaming + Visualization」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**Quivr（个人知识库）**、**私人 GPT（本地）**、**Cognita（TrueFoundry 出品）** 构成 RAG 应用生态。verba 的差异化是 **「Weaviate 官方 + 完整 Web App + Reader/Embedder/Generator 全栈 + 可视化配置 + 入门友好」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  前端层（frontend/，Next.js + React）                │
│   RAG Chat UI / Configurator / Document Manager / Streaming          │
│   ── Tailwind CSS / Radix UI / Tanstack Query ──                  │
└─────────────────────────────┬───────────────────────────────────────┘
                              │ REST + WebSocket
┌─────────────────────────────▼───────────────────────────────────────┐
│                  后端层（goldenverba/，FastAPI）                     │
│   Verba Manager（verba_manager.py）/ Components / Streaming          │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Reader（7+）   │ │  Embedder（8+）   │ │  Chunker（查询    │ │ Retriever  │
│  BasicReader    │ │  OpenAI / Cohere  │ │  chunking/）      │ │ WindowRetr │
│  HTMLReader     │ │  Google / Voyage  │ │  SentenceSplitter │ │ + Weaviate│
│  FirecrawlRead  │ │  HuggingFace     │ │  SemanticChunker  │ │  Hybrid   │
│  GitReader      │ │  Upstage / Ollama│ │  MarkdownChunker  │ │ (默认)    │
│  AssemblyAI     │ │  WeaviateEmbedd  │ │  HTMLChunker      │ │           │
│  UpstageDocParse│ │                 │ │  CodeChunker      │ │           │
│  UnstructuredAPI│ │                 │ │                   │ │           │
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────────────────┐
│                    Generation Layer（components/generation/）                          │
│    OpenAI / Anthropic / Cohere / Google / Mistral / Groq / Upstage / Ollama / 等等     │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Weaviate 向量库（默认）                              │
│   Weaviate Embedded / Weaviate Server / Weaviate Cloud              │
│   Hybrid Retrieval（BM25 + 向量融合）                                │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Weaviate 客户端（weaviate-client）                 │
│   Hybrid Search / Filters / Multi-tenancy                          │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：双层架构（前端 Next.js + 后端 FastAPI）；默认 Weaviate Embedded（进程内）；可选 Weaviate Server / Cloud。

## 七、嵌入模型适配（EMB）

verba 通过 `Embedder` 抽象（`goldenverba/components/embedding/`）暴露接口，下游 8+ 实现：

- **OpenAI**：`OpenAIEmbedder`（`OpenAIEmbedder.py`，text-embedding-3-small/large 1536/3072 维）、`AzureOpenAIEmbedder`；
- **Cohere**：`CohereEmbedder`（`CohereEmbedder.py`，embed-english-v3.0 / embed-multilingual-v3.0 1024 维）；
- **Google**：`GoogleEmbedder`（Vertex AI text-embedding-005 / Gemini embedding）；
- **Voyage AI**：`VoyageAIEmbedder`（`VoyageAIEmbedder.py`，voyage-3 1024 维）；
- **HuggingFace / sentence-transformers**：`SentenceTransformersEmbedder`（`SentenceTransformersEmbedder.py`，`all-MiniLM-L6-v2` 384 维、`bge-large-zh-v1.5` 1024 维）；
- **Ollama**：`OllamaEmbedder`（`OllamaEmbedder.py`，`nomic-embed-text` / `mxbai-embed-large`）；
- **Upstage**：`UpstageEmbedder`（Upstage Solar Embeddings）；
- **Weaviate Embeddings**：`WeaviateEmbedder`（`WeaviateEmbedder.py`，Weaviate 官方嵌入服务）；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - **SentenceSplitter**（默认，`chunking/SentenceChunker.py`，按句子）；
  - **SemanticChunker**（`chunking/SemanticChunker.py`，基于嵌入相似度的语义切分）；
  - **HTMLChunker**（`chunking/HTMLChunker.py`，HTML 结构感知）；
  - **MarkdownChunker**（`chunking/MarkdownChunker.py`，Markdown 结构感知）；
  - **CodeChunker**（`chunking/CodeChunker.py`，代码 AST 切分 Python / JS / Java）；
  - **TokenChunker**（按 token 切，tiktoken）；
  - **推荐参数**：chunk_size 500-1500 token、overlap 10-20%；

## 八、文档解析器（LOAD）

verba 通过 `Reader` 抽象（`goldenverba/components/reader/`）暴露接口，下游 7+ 实现：

- **BasicReader**：`BasicReader.py`（基础文本 / TXT / Markdown）；
- **HTMLReader**：`HTMLReader.py`（基于 BeautifulSoup / trafilatura）；
- **FirecrawlReader**：`FirecrawlReader.py`（Firecrawl API，高精度爬取 + 解析）；
- **GitReader**：`GitReader.py`（基于 gitpython，从 GitHub 仓库读取）；
- **AssemblyAIAPI**：`AssemblyAIAPI.py`（音频转录，基于 AssemblyAI API）；
- **UnstructuredAPI**：`UnstructuredAPI.py`（Unstructured.io API，表格 / 图像 / 公式抽取）；
- **UpstageDocumentParse**：`UpstageDocumentParse.py`（Upstage Document AI API，高精度 OCR）；
- **上传类型**：PDF / Docx / HTML / Markdown / TXT / JSON / CSV / Notion（自定义）；
- **多模态**：
  - 通过 Unstructured API 的 `hi_res` strategy 抽取图像 / 表格 / 公式；
  - 通过 UpstageDocumentParse 抽取 PDF 中的图像；

## 九、检索器与重排器（RETRIEVE / RERANK）

verba 通过 `Retriever` 抽象（`goldenverba/components/retriever/`）暴露接口：

- **WindowRetriever**（`WindowRetriever.py`，默认）：
  - 基于 Weaviate **Hybrid Retrieval**（BM25 + 向量融合）；
  - **窗口扩展**：检索段落 → 扩展到上下文章节；
  - `window_size` 参数可调（默认 1）；
- **自定义 Retriever**：继承 `BaseRetriever`；
- **重排（Rerank）**：
  - 通过 Weaviate 自带 **reranker-transformers** 模块；
  - **Cohere Rerank**（Weaviate 内置）；
  - 通过 custom rerank server 集成；
- **过滤（Filters）**：
  - Weaviate where filter；
  - `metadata` filtering；
- **Graph RAG**：
  - 通过 Weaviate **References** / **Cross-references**；
  - 跨文档引用（document → section → chunk）；

## 十、向量库后端（VECTOR STORE）

verba 主要绑定 **Weaviate**（默认 + 强耦合）：

- **Weaviate Embedded**（默认，`weaviate-embedded-python` 包）：
  - 进程内 Weaviate；
  - 零配置启动；
  - 适合原型 / Demo；
- **Weaviate Server**（Docker）：
  - 本地 Weaviate Server（`docker run semitechnologies/weaviate`）；
  - 适合生产环境；
- **Weaviate Cloud**（商业 SaaS）：
  - Serverless / Enterprise Cloud；
  - 适合企业生产环境；
- **配置**（`docker-compose.yml` / `components/managers.py`）：
  - `WEAVIATE_URL`：Weaviate Server URL；
  - `WEAVIATE_API_KEY`：API key；
- **Hybrid Retrieval**（默认）：
  - `alpha` 参数：`alpha=0` 纯 BM25、`alpha=1` 纯向量、`alpha=0.5` 混合；
  - **RRF（Reciprocal Rank Fusion）**：Weaviate 1.7+ 支持；
- **Schema 管理**：
  - 自动创建 Collection（`Verba`）；
  - 自动添加 properties（`text`、`chunk_id`、`document_id`、`metadata`）；
  - 自动添加 cross-references（document → section → chunk）；
- **多模态**：
  - 通过 Weaviate `multi2vec5` module（CLIP）支持图像；
- **备选向量库**：
  - verba 强耦合 Weaviate，但可通过自定义 `Embedder` / `Reader` 改造支持其他向量库；
  - 社区有支持 Qdrant / Pinecone 的 fork（实验性）；

## 十一、查询/计算/接口层

- **Verba Manager**（`goldenverba/verba_manager.py`）：
  - 单例管理所有 Components；
  - `verba_manager.add_document(...)` → Reader → Chunker → Embedder → Weaviate；
  - `verba_manager.retrieve(query, config)` → Weaviate Hybrid + generate text；
  - `verba_manager.stream(query, config)` → 流式响应；
  - `verba_manager.config_manager()` → 配置管理；
- **Components 注册**（`components/__init__.py`）：
  - 动态注册 Reader / Chunker / Embedder / Generator / Retriever；
  - 通过 `@register_component` 装饰器；
  - Plugin 机制（v3+）；
- **Generator**（`components/generation/`）：
  - **OpenAI**：`OpenAIGenerator.py`（gpt-4o / gpt-4-turbo / gpt-3.5-turbo）；
  - **Anthropic**：`AnthropicGenerator.py`（claude-3-5-sonnet / claude-3-opus）；
  - **Cohere**：`CohereGenerator.py`（command-r-plus）；
  - **Google**：`GoogleGenerator.py`（gemini-pro / gemini-1.5-pro）；
  - **Mistral**：`MistralGenerator.py`（mistral-large）；
  - **Groq**：`GroqGenerator.py`（llama-3.1-70b-versatile）；
  - **Upstage**：`UpstageGenerator.py`（Solar）；
  - **Ollama**：`OllamaGenerator.py`（本地 LLM）；
  - **自定义 Generator**：继承 `BaseGenerator`；
  - **流式响应**：所有 Generator 支持 `stream=True`；
- **前端**（`frontend/`）：
  - **RAG Chat UI**：对话界面，streaming + source 显示；
  - **RAG Configurator**（v2.7+）：可视化配置 Reader / Chunker / Embedder / Generator / Retriever；
  - **Document Manager**：上传 / 列表 / 删除文档；
  - **Streaming**：WebSocket + Server-Sent Events；
- **Config 管理**（`components/managers.py`）：
  - **VerbaConfig**：全局配置；
  - **RAGConfig**：单个检索请求配置；
  - **UI Configurator**：前端可视化编辑；
  - **持久化**：JSON 文件 / 数据库；
- **记忆系统**：
  - 默认无持久化记忆（每次会话独立）；
  - 可通过自定义 ConversationBuffer 扩展；
- **评估与可观测性**：
  - **Custom Evaluation**：通过自定义 Evaluator；
  - **Phoenix（Arize）** 集成；
  - **Langfuse** 集成；
  - **Weaviate Query Insights**（自带的查询分析）；
- **生产部署**：
  - **Docker Compose**（`docker-compose.yml`，FastAPI + Next.js + Weaviate Embedded）；
  - **Kubernetes**（Helm chart / 自定义）；
  - **Weaviate Cloud** + verba 自行部署；
  - **Vercel**（前端）；

## 十二、生态与集成

- **客户端**：浏览器（前端 Next.js）、Python SDK（FastAPI）；
- **集成矩阵**：
  - **Reader**：Basic / HTML / Firecrawl / Git / AssemblyAI / Unstructured / Upstage；
  - **Embedder**：OpenAI / Cohere / Google / Voyage / HuggingFace / Upstage / Ollama / Weaviate；
  - **Chunker**：Sentence / Semantic / HTML / Markdown / Code / Token；
  - **Generator**：OpenAI / Anthropic / Cohere / Google / Mistral / Groq / Upstage / Ollama；
  - **Retriever**：WindowRetriever（基于 Weaviate Hybrid）+ 自定义；
  - **Weaviate 模块**：reranker-transformers / multi2vec5（CLIP）/ generative-*；
- **Plugin 系统**（v3+）：
  - 第三方 Component 扩展；
- **Weaviate Cloud**：商业 SaaS；
- **文档 & 教程**：https://docs.weaviate.io/verba；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `goldenverba/verba_manager.py`：Verba Manager 主类；
  2. `goldenverba/components/managers.py`：Component 管理；
  3. `goldenverba/components/reader/BasicReader.py`：基础 Reader；
  4. `goldenverba/components/reader/UnstructuredAPI.py`：Unstructured Reader；
  5. `goldenverba/components/embedding/OpenAIEmbedder.py`：OpenAI 嵌入；
  6. `goldenverba/components/chunking/SentenceChunker.py`：句子切分；
  7. `goldenverba/components/chunking/SemanticChunker.py`：语义切分；
  8. `goldenverba/components/retriever/WindowRetriever.py`：Hybrid 检索；
  9. `goldenverba/components/generation/OpenAIGenerator.py`：OpenAI 生成；
  10. `goldenverba/server/api.py`：FastAPI 路由；
  11. `frontend/app/`：Next.js 前端；
- **LOC 与语言**：
  - `goldenverba/` ~10k LoC；
  - `frontend/` ~15k LoC；
  - 主语言：Python（后端 70%）、TypeScript（前端 30%）；
- **构建系统**：
  - Python：Poetry / setuptools；
  - Frontend：npm / Next.js；
  - pytest（后端）+ Vitest / Playwright（前端）；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（`README.md`、`PYTHON_TUTORIAL.md`、`FRONTEND.md`、`TECHNICAL.md`）；
  - 跑通 `docker-compose up`；
  - 上传 PDF / Markdown，提问；
  - 了解 RAG Configurator；
- **进阶（2-3 周）**：
  - 深入学习 Verba Manager；
  - 实战 Custom Reader / Embedder / Generator；
  - 学习 Weaviate Hybrid Retrieval；
  - 学习 Streaming 机制；
- **高级（1-2 月）**：
  - 阅读源码（Verba Manager + WindowRetriever）；
  - 自定义 Plugin；
  - 与 LangChain / LlamaIndex 集成；
  - 生产部署（Weaviate Cloud + Kubernetes）；
  - RAG 评估（Custom Evaluator）；
- **推荐资源**：
  - 官方文档：https://docs.weaviate.io/verba
  - Tutorial：https://github.com/weaviate/verba/blob/main/PYTHON_TUTORIAL.md
  - Frontend 文档：https://github.com/weaviate/verba/blob/main/FRONTEND.md
  - 技术文档：https://github.com/weaviate/verba/blob/main/TECHNICAL.md
  - Weaviate 文档：https://docs.weaviate.io/
  - 论文：Hybrid Search、BM25、ColBERT、HyDE、Self-RAG；

## 十五、风险与陷阱

- **强耦合 Weaviate**：verba 与 Weaviate 紧耦合，迁移到其他向量库需要重写 Reader / Retriever；
- **Weaviate Embedded 性能限制**：默认 Weaviate Embedded 适合原型 / Demo，生产环境需 Weaviate Server / Cloud；
- **RAG Configurator 复杂度**：可视化配置对新手友好，但对高级用户（自定义逻辑）灵活性不足；
- **Streaming 不完全**：某些 Generator（Upstage / Google）的 Streaming 实现可能有 bug；
- **Weaviate Cloud 锁定**：使用 Weaviate Cloud 后切换到本地 Weaviate Server 需迁移 schema；
- **Custom Plugin 文档不全**：Plugin 系统（v3+）文档较少，第三方贡献少；
- **多租户隔离缺失**：默认无 user-level 隔离，生产环境需自行实现；
- **评估指标缺失**：默认无 RAG 评估，需自定义集成；
- **Observability 黑盒**：默认无 tracing，需自接 OpenTelemetry / Langfuse / Phoenix；
- **依赖版本冲突**：Weaviate 客户端 + FastAPI + Next.js 版本升级时常出现依赖冲突；
- **Large Document 性能**：大文档（>1000 页）索引时间较长；

## 十六、参考资料

- 官方仓库：https://github.com/weaviate/verba
- 官方文档：https://docs.weaviate.io/verba
- Weaviate 文档：https://docs.weaviate.io/
- Weaviate Cloud：https://console.weaviate.cloud/
- 论文：BM25（Robertson et al. 1995）、Weaviate Hybrid Search（https://docs.weaviate.io/developers/weaviate/search/hybrid）、ColBERT（Khattab & Zaharia 2020, https://arxiv.org/abs/2004.12832）、HyDE（Gao et al. 2022, https://arxiv.org/abs/2212.10496）
- 博客：https://weaviate.io/blog
- Tutorial：https://github.com/weaviate/verba/blob/main/PYTHON_TUTORIAL.md

## 十七、与本项目关联

Verba 作为「Weaviate 出品 + 黄金 RAG + Hybrid Search + Reader/Generator 模块化」RAG 前端框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「Weaviate 集成 + 模块化 Reader/Generator + 黄金 RAG 检索管线」三个维度具有直接参考价值：

- **Weaviate 向量库后端**：Verba 默认绑定 Weaviate（`goldenverba/components/vector_store/`），通过 Weaviate 内置 Hybrid Search（BM25 + 向量 RRF 融合）实现高质量召回。可在 Weaviate 集成细节上对照本项目 wiki（`docs/db-wiki/vector/`），尤其是与 Weaviate 协同的混合检索设计。
- **模块化组件设计**：Verba 通过 `goldenverba/components/` 把 Reader / Chunker / Embedder / Generator / Vector Store / Reranker 拆为可插拔模块。本项目则在 `engineering/include/db/` 提供 Catalog / GUC / VectorIndex / mm_storage 模块化抽象，与 Verba 的「模块可替换」设计理念一致。
- **黄金 RAG 检索管线**：Verba 通过 Weaviate Hybrid Search + Reader 模块化 Rerank 实现「高质量 RAG」参考实现；本项目则在 `engineering/src/db/index/vector_index/` 自研 HNSW + PQ/SQ 量化（`hnsw_pq.c`、`hnsw_sq.c`、IVF `inverted_lists.h`），可在「自研 vs Weaviate Hybrid」决策上提供参考。
- **多 Reader 适配**：Verba 通过 Reader 抽象适配 Weaviate Reader / 文件读取 / Web Reader；本项目多模态 RAG 应用层（`engineering/apps/multimodal_rag/`）通过自定义 parser 支持 PDF / 图像 / 视频摄取，可在「多 Reader 适配模式」上做工程化对照。
- **GUC + Weaviate Cloud 配置**：Verba 通过 Weaviate Cloud + 环境变量管理配置；本项目实现 PostgreSQL 风格的 GUC 配置（`engineering/src/db/core/guc.c` 与 `engineering/include/db/core/guc.h`），可在「运行时配置统一管理」理念上提供底层参考。
- **量化与字典组件**：Verba 不自研量化与分词器；本项目则在 `engineering/src/algo-prod/quantization/`（PQ / LvQ / `adaptive_quantization.c`）与 `engineering/src/algo-prod/dict/`（结巴分词 `dict_cut.c` / `dict_hmm.c`）自研底层组件，可对照 Verba 通过 Weaviate 远程服务的设计思路。
- **冷热分层与多模态存储**：Verba 通过 Weaviate 单一向量库存储文档；本项目则实现多模态存储统一抽象（`engineering/include/db/mm_storage.h`、`engineering/src/db/storage/mm/mm_storage.c`），把 KV / Vector / Timeseries / Document / Spatial / Graph / Yang 七种数据模型统一管理，比 Verba 的「单一向量库」更通用。
- **学习建议**：阅读 Verba 源码重点关注「Weaviate Hybrid Search 集成」「Reader/Generator 模块化设计」，与本项目 `engineering/apps/multimodal_rag/` 应用层 + `engineering/include/db/` 接口层做对比，加深对「生产级 RAG 前端」的工程理解。