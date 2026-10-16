# qanything Wiki（网易出品的本地化 RAG 系统）

> 参考版本：QAnything 1.x / 2.x（2024-2026），由网易有道出品的开源本地化 RAG 系统。本 wiki 重点介绍 qanything 作为「网易本地化 RAG」的定位、RAG 四维度扩展、模块化架构、源码学习路径与典型陷阱。

## 一、项目定位

qanything（Question Answering based on Anything）是 **网易有道**（网易教育事业部）于 **2024 年 2 月开源**的 **「本地化 RAG 系统」**，全称 **「QAnything - Question and Answer based on Anything」**，定位是「**基于任意格式文档的本地化问答系统**」。qanything 强调**本地化部署 + 多文档格式支持 + 高质量 PDF 解析 + 中文友好**，是国内主流的中文 RAG 项目之一。

- **核心痛点**：
  - 现有 RAG 框架（langchain / llama-index）默认依赖云 API（OpenAI），数据外泄风险；
  - 中文 PDF 解析质量差（表格 / 公式 / 图像抽取）；
  - 多文档格式支持不完整（PDF / PPT / Excel / 图片 / 数据库）；
  - 缺少 **本地化 RAG 一体化方案**（PDF 解析 + 嵌入 + 检索 + 重排 + 生成）；
  - 缺少 **中文友好的 LLM 集成**（ChatGLM / Qwen / DeepSeek / 文心一言）；
  - 企业 / 政府 / 医疗 / 教育等数据敏感行业需要 **100% 本地部署**；
- **典型用户**：
  - **企业内部知识库**：金融 / 医疗 / 法律 / 政府 / 教育；
  - **个人 / 团队**：本地私有 RAG；
  - **中文 PDF 处理**：表格 / 公式 / 图像抽取；
  - **多文档格式**：PDF / Word / PPT / Excel / 图片 / 数据库；
  - **教育 / 学术**：本地 LLM 实验；
  - **政企 / 国产化**：支持国产 LLM + 国产数据库；

## 二、发展历史与社区

- **起源**（2024-02）：网易有道 AI 团队开源 qanything v1.0，最初为内部产品「QAGPT」；
- **里程碑**：
  - 2024-02：v1.0，FastAPI + PDF 解析 + LangChain + LLM；
  - 2024-04：v1.4，多格式文档支持（PDF / Word / PPT / Excel / 图片 / Markdown）；
  - 2024-06：v1.6，Milvus 集成、Elasticsearch 集成；
  - 2024-08：v1.8，BGE-Reranker 集成、重排优化；
  - 2024-10：v2.0，模块化架构（Connector / Kernel / Dependent Server）；
  - 2024-12：v2.2，多租户、用户管理；
  - 2025-03：v2.5，多 LLM 集成（ChatGLM / Qwen / DeepSeek / 文心）；
  - 2025-06：v2.8，性能优化、OCR / Rerank / LLM-Engine 独立 Server；
  - 2025-10：v3.0，UI 重构、Web 前端增强；
- **维护状态**：**活跃**（maintained by 网易有道 AI 团队），社区 50+ 贡献者，月均 50+ commits；
- **社区**：
  - GitHub 12k+ star、Apache 2.0 License；
  - 微信群 / QQ 群 / Discord / Gitee 镜像；
  - 商业产品 网易有道 LLM Platform / 智能客服；
- **关键贡献者**：网易有道 AI 团队、贡献者 50+；

## 三、许可与商业限制

- **核心代码**：**Apache 2.0 License**（`qanything_kernel/`、`front_end/`、`README.md`、`docker-compose-*`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **网易有道 LLM Platform**（闭源 SaaS）：商业 RAG / 客服 / 教育 AI；
  - **网易有道智选**（商业 LLM）；
- **生态关系**：
  - **上游依赖**：`fastapi`、`pydantic`、`langchain`、`llama-index`、`pymilvus`、`elasticsearch`、`transformers`、`sentence-transformers`、`bge-reranker`、`openai`、`huggingface-hub`、`uvicorn`、`Pillow`、`opencv-python`、`paddleocr`、`unstructured`、`pdfplumber`、`pymupdf`、`python-docx`、`python-pptx`、`openpyxl`；
  - **下游生态**：约 **3000+ 项目使用 qanything**，包括企业私有 RAG、教育 / 教学、政府 / 金融 / 医疗、国产化部署；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - 网易系公司内部使用；
  - 多家中文 RAG 团队；
  - 教育 / 教学机构；
  - 跨国企业中文部门；
- **国内**（公开）：
  - **网易**：内部 RAG / 智能客服 / 教育 AI；
  - **多家金融 / 政府 / 教育机构**：私有 RAG；
  - **多家国产化厂商**：国产 LLM + 国产数据库；
  - **多家初创公司**：基于 qanything 二次开发；
- **典型场景**：
  - **企业内部知识库**：金融 / 医疗 / 法律 / 政府 / 教育；
  - **智能客服**：工单 / 邮件 / FAQ；
  - **教育 RAG**：教材 / 题库 / 文献库；
  - **中文 PDF 处理**：表格 / 公式 / 图像抽取；
  - **多文档格式**：PDF / Word / PPT / Excel / 图片；
  - **国产化部署**：支持国产 LLM + 国产数据库；
  - **个人 / 团队**：本地私有 RAG；

## 五、一句话对比定位

> qanything 是 **「网易有道出品 + 本地化 RAG + 中文友好 + 多文档格式 + 高质量 PDF 解析 + 模块化 + 国产 LLM + Milvus / Elasticsearch + Apache 2.0 + 12k star」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**txtai（一体化）**、**Verba（Weaviate 出品）**、**Cognita（TrueFoundry 出品）**、**R2R（SciPhi 出品）**、**Quivr（个人知识库）**、**私人 GPT（本地）** 构成 RAG 应用生态。qanything 的差异化是 **「网易出品 + 中文友好 + 高质量 PDF + 多文档格式 + 国产 LLM + 12k star + 国内首选」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（front_end/，Web UI）                    │
│        Web App / Mobile App / REST API / OpenAPI                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │ REST API
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Server 层（qanything_kernel/qanything_server/）    │
│   FastAPI Router / OpenAPI / Auth / Multi-Tenant / Files           │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Kernel 层（qanything_kernel/core/）                │
│   chains / retriever / tools（基于 LangChain / LlamaIndex）         │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────────┬───────────────┼────────────────┬──────────────────────┐
│              │               │                │                │
┌──────────────▼────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌────▼─────┐
│  Connector       │ │  Vector DB       │ │  Embedding        │ │ LLM      │
│  database/       │ │  Milvus          │ │  connector/        │ │ connector│
│  faiss/          │ │  Elasticsearch   │ │  embedding/        │ │ /        │
│  milvus/         │ │  FAISS           │ │  （sentence-trans │ │ llm/     │
│  mysql/          │ │  （connector/    │ │  formers、BGE）    │ │ /        │
│  embedding/      │ │   database/）    │ │                   │ │ base/    │
│  llm/            │ │                  │ │                   │ │ （OpenAI /│
│  rerank/         │ │                  │ │                   │ │ ChatGLM /│
│  （基于 LangChain │ │                  │ │                   │ │ Qwen /   │
│   / LlamaIndex）  │ │                  │ │                   │ │ DeepSeek│
└────────┬─────────┘ └─────────┬────────┘ └────┬──────────────┘ └────┬─────┘
         │                     │                │                    │
┌────────▼─────────────────────▼────────────────▼────────────────────▼─────┐
│                    Dependent Server（dependent_server/）                       │
│   ocr_server / pdf_parser_server / rerank_server / embedding_server            │
│   insert_files_serve / llm_server                                              │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Storage Layer                                            │
│  MySQL（文件元数据 / 用户信息）                                          │
│  Milvus / Elasticsearch（向量 + 全文检索）                             │
│  S3 / Local FS（文件存储）                                              │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：FastAPI 主进程 + Dependent Server（OCR / PDF Parser / Rerank / Embedding / LLM）+ Vector DB（Milvus / Elasticsearch）；模块化部署，支持分布式。

## 七、嵌入模型适配（EMB）

qanything 通过 `Embedding` 抽象（`qanything_kernel/connector/embedding/`）暴露接口：

- **BGE 系列**（默认，中文友好）：
  - `BAAI/bge-small-en-v1.5`（384 维）；
  - `BAAI/bge-large-en-v1.5`（1024 维，英文首选）；
  - `BAAI/bge-large-zh-v1.5`（1024 维，中文首选）；
  - `BAAI/bge-m3`（1024 维，多语 + 多功能检索）；
- **M3E 系列**（中文友好）：
  - `MokaAI/m3e-small`（中文，384 维）；
  - `MokaAI/m3e-base`（中文，768 维）；
  - `MokaAI/m3e-large`（中文，1024 维）；
- **OpenAI**：`text-embedding-3-small/large`（1536/3072 维，可选）；
- **Cohere**：`embed-english-v3.0` / `embed-multilingual-v3.0`（1024 维，可选）；
- **本地 sentence-transformers**：`all-MiniLM-L6-v2`（384 维）、`intfloat/e5-large-v2`（1024 维）；
- **智源 BGE**：`BAAI/bge-large-zh-v1.5`（默认）；
- **通义 DashScope**：`text-embedding-v3`（1024 维，可选）；
- **DeepSeek**：`DeepSeekEmbedding`（社区）；
- **Custom Embedding**：用户自定义；
- **Dimension**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - **RecursiveCharacterTextSplitter**（基于 LangChain，默认 chunk_size=500，overlap=50）；
  - **ChineseTextSplitter**（中文友好，基于正则）；
  - **MarkdownTextSplitter**（Markdown 结构感知）；
  - **SentenceTextSplitter**（按句子切分）；
  - **TokenTextSplitter**（按 token 切，tiktoken）；
  - **推荐参数**：chunk_size 300-800 token、overlap 10-20%；

## 八、文档解析器（LOAD）

qanything 通过 `PDF Parser Server` + `OCR Server` + `Connector` 暴露接口，下游支持多种文档格式：

- **PDF**（核心，高质量解析）：
  - **PyPDF**（`pypdf`，简单 PDF，纯文本）；
  - **PDFPlumber**（`pdfplumber`，表格抽取）；
  - **PyMuPDF**（`pymupdf`，图像 / 表格抽取）；
  - **pdf_to_markdown**（`pdf_parser_server/pdf_to_markdown/`，自研，高质量 PDF 转 Markdown，OCR + 表格 + 公式）；
  - **Unstructured**（`unstructured[all-in-one]`，表格 / 图像 / 公式抽取）；
- **Word / Docx**：`python-docx`（`connector/`）；
- **PPT / PPTX**：`python-pptx`；
- **Excel**：`openpyxl` / `pandas`；
- **Markdown**：`markdown-it-py`；
- **HTML**：`BeautifulSoup` / `selectolax`；
- **TXT / CSV / JSON**：`connector/`；
- **图片**（OCR）：
  - **Tesseract**（`pytesseract`）；
  - **PaddleOCR**（中文 OCR 首选）；
  - **EasyOCR**（多语 OCR）；
  - **基于 LLM 的图像理解**（`unstructured` hi_res strategy）；
- **音频**：`Whisper`（社区）；
- **数据库 dump**：
  - **MySQL**（`connector/database/mysql/`）；
  - **PostgreSQL**（社区）；
  - **Elasticsearch**（`core/retriever/elasticsearchstore.py`）；
- **Web**：基于 `requests` / `trafilatura`（社区）；
- **支持格式**：PDF / Word / PPT / Excel / Markdown / HTML / TXT / CSV / JSON / 图片 / 数据库 dump；
- **多模态**：
  - 通过 pdf_to_markdown 高质量 PDF（OCR + 表格 + 公式）；
  - 通过 OCR Server 抽取图像中的文字 / 公式；
  - 通过 LLM 联合理解图像 + 文本；

## 九、检索器与重排器（RETRIEVE / RERANK）

qanything 通过 `core/retriever/` 暴露检索：

- **Vector Store Retriever**（`core/retriever/vectorstore.py`，默认）：
  - 基于 LangChain / LlamaIndex；
  - 支持 Milvus / Elasticsearch / FAISS；
- **Elasticsearch Retriever**（`core/retriever/elasticsearchstore.py`）：
  - BM25 + 向量混合检索；
- **Parent Retriever**（`core/retriever/parent_retriever.py`）：
  - 子块检索 + 父块上下文扩展；
- **Hybrid Retriever**：
  - 向量 + BM25 融合（RRF Reciprocal Rank Fusion）；
  - 默认 alpha=0.5；
- **Rerank**（`connector/rerank/`）：
  - **BGE Reranker**（`BAAI/bge-reranker-v2-m3`，默认，中文友好）；
  - **BGE Reranker v2**（多语）；
  - **Cohere Rerank**（可选）；
  - **CrossEncoder**（基于 `cross-encoder` 库，本地）；
  - **LLM-based Rerank**（基于 LLM 打分，可选）；
  - **独立 Rerank Server**（`dependent_server/rerank_server/`）；
- **过滤**：
  - Vector DB where filter；
  - Metadata 过滤；
- **Multi-Query**：
  - 基于 LangChain MultiQueryRetriever；
- **HyDE**：
  - 基于 LangChain HyDE；
- **Self-RAG / CRAG**：
  - 社区集成；

## 十、向量库后端（VECTOR STORE）

qanything 通过 `connector/database/` 暴露抽象，下游多个后端：

- **Milvus**（`connector/database/milvus/`，默认）：
  - 单机 / 集群；
  - HNSW / IVF / DiskANN；
  - 中大规模文档；
- **Elasticsearch**（`core/retriever/elasticsearchstore.py`）：
  - 内置 BM25 + 向量；
  - 适合全文 + 向量混合；
- **FAISS**（`connector/database/faiss/`）：
  - Facebook C++ 仿真库；
  - 进程内 / 小规模；
- **MySQL**（`connector/database/mysql/`，作为元数据存储，非向量库）；
- **PostgreSQL + pgvector**（社区，可选）；
- **LanceDB**（社区，可选）；
- **Weaviate**（社区，可选）；
- **配置**：
  - `MILVUS_HOST` / `MILVUS_PORT`；
  - `ELASTICSEARCH_URL` / `ELASTICSEARCH_PASSWORD`；
  - `MYSQL_HOST` / `MYSQL_PORT` / `MYSQL_USER` / `MYSQL_PASSWORD` / `MYSQL_DB`；
- **Metadata 存储**：MySQL（默认）；
- **文件存储**：S3 / Local FS；
- **选型建议**：
  - 原型 / 小规模：FAISS；
  - 中规模（10k - 1M）：Milvus / Elasticsearch；
  - 大规模（> 1M）：Milvus Cluster；
  - 已有 ES：Elasticsearch；
  - 已有 MySQL + ES：Milvus + MySQL + ES（qanything 默认组合）；

## 十一、查询/计算/接口层

- **Server 层**（`qanything_kernel/qanything_server/`）：
  - **FastAPI Router**；
  - **OpenAPI**；
  - **Auth**（JWT）；
  - **Multi-Tenant**（多用户隔离）；
  - **File Management**（`server/files/`）；
  - **Ingest Management**（`server/ingest/`）；
  - **Chat Management**（`server/chat/`）；
- **Kernel 层**（`qanything_kernel/core/`）：
  - **Chains**（`core/chains/`，基于 LangChain / LlamaIndex）；
  - **Retriever**（`core/retriever/`，上文 § 九）；
  - **Tools**（`core/tools/`，Function Calling / Tools）；
- **Chains**（`core/chains/condense_q_chain.py`）：
  - **ConversationalRetrievalChain**（基于 LangChain，多轮对话 + 上下文压缩）；
  - **Custom Chains**（基于 LangChain）；
- **LLM 抽象**（`qanything_kernel/connector/llm/`）：
  - **OpenAI**（`connector/llm/base/openai.py`，gpt-4o / gpt-4-turbo，可选）；
  - **ChatGLM**（智谱 AI，`connector/llm/base/chatglm.py`，本地 / 云端）；
  - **Qwen**（通义千问，`connector/llm/base/qwen.py`，本地 / DashScope）；
  - **DeepSeek**（`connector/llm/base/deepseek.py`，本地 / 云端）；
  - **文心一言**（百度 ERNIE，社区）；
  - **Llama 3 / Mistral**（本地，可选）；
  - **Ollama**（本地，多 LLM）；
  - **llama.cpp**（本地 GGUF）；
  - **vLLM**（高性能推理，本地）；
  - **HuggingFace**（本地 Transformers）；
  - **Custom LLM**（用户自定义）；
- **Dependent Server**（`qanything_kernel/dependent_server/`）：
  - **OCR Server**（`ocr_server/`，基于 PaddleOCR/Tesseract，独立进程）；
  - **PDF Parser Server**（`pdf_parser_server/`，基于 pdf_to_markdown，独立进程）；
  - **Rerank Server**（`rerank_server/`，基于 BGE-Reranker，独立进程）；
  - **Embedding Server**（`embedding_server/`，基于 BGE / sentence-transformers，独立进程）；
  - **Insert Files Serve**（`insert_files_serve/`，文件摄入，独立进程）；
  - **LLM Server**（`llm_server/`，LLM 服务，独立进程）；
- **Tools**（`core/tools/`）：
  - **Function Calling**；
  - **Built-in Tools**：Search、Web、Coding、Math、Database；
  - **Custom Tools**：用户自定义；
- **流式响应**：
  - FastAPI StreamingResponse + SSE；
  - async generator；
- **记忆系统**：
  - **InMemory**（默认，进程内）；
  - **Persistent**（MySQL / Redis / 文件）；
- **评估与可观测性**：
  - **Custom Evaluator**：用户自定义评估；
  - **Langfuse 集成**（社区）；
  - **Phoenix 集成**（社区）；
  - **OpenTelemetry 集成**（部分支持）；
  - **MySQL Audit Log**：摄入 / 检索 / 反馈日志；
- **生产部署**：
  - **Docker Compose**（`docker-compose-linux.yaml`、`docker-compose-mac.yaml`、`docker-compose-win.yaml`）；
  - **Kubernetes**（社区）；
  - **网易有道 LLM Platform**（商业托管）；

## 十二、生态与集成

- **客户端**：Web App（`front_end/`，Vue/React/Python 任意）、Python SDK（FastAPI）、REST API、OpenAPI；
- **集成矩阵**：
  - **LLM 提供商**：OpenAI（可选）、ChatGLM（默认推荐）、Qwen（默认推荐）、DeepSeek（默认推荐）、文心一言（社区）、Llama 3、Mistral、Ollama、llama.cpp、vLLM、HuggingFace；
  - **向量库**：Milvus（默认）、Elasticsearch（默认）、FAISS（可选）、LanceDB / pgvector / Pinecone / Weaviate（社区）；
  - **Parser**：PyPDF、PDFPlumber、PyMuPDF、pdf_to_markdown（自研）、Unstructured、python-docx、python-pptx、openpyxl；
  - **OCR**：PaddleOCR（中文首选）、Tesseract、EasyOCR；
  - **Rerank**：BGE Reranker（默认推荐）、Cohere Rerank（可选）、CrossEncoder；
  - **存储**：MySQL（默认）、S3 / Local FS（文件）、Milvus / Elasticsearch（向量 + 全文）；
  - **Cache**：Redis（社区）；
  - **任务队列**：Celery（社区）；
  - **Auth**：JWT；
- **网易有道 LLM Platform**：商业 SaaS；
- **国产化适配**：支持国产 LLM（ChatGLM / Qwen / DeepSeek / 文心）+ 国产数据库（MySQL / 国产向量库）；
- **Docker 多平台**：Linux / Mac / Windows 三个 docker-compose 配置文件；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `qanything_kernel/qanything_server/`：Server 入口；
  2. `qanything_kernel/core/`：核心逻辑；
  3. `qanything_kernel/core/chains/condense_q_chain.py`：Condense Question Chain；
  4. `qanything_kernel/core/retriever/vectorstore.py`：Vector Store Retriever；
  5. `qanything_kernel/core/retriever/elasticsearchstore.py`：Elasticsearch Retriever；
  6. `qanything_kernel/core/retriever/parent_retriever.py`：Parent Retriever；
  7. `qanything_kernel/connector/database/milvus/`：Milvus Connector；
  8. `qanything_kernel/connector/database/elasticsearch/`：Elasticsearch Connector（社区）；
  9. `qanything_kernel/connector/database/faiss/`：FAISS Connector；
  10. `qanything_kernel/connector/database/mysql/`：MySQL Connector；
  11. `qanything_kernel/connector/embedding/`：Embedding Connector；
  12. `qanything_kernel/connector/llm/`：LLM Connector；
  13. `qanything_kernel/connector/llm/base/`：LLM Base；
  14. `qanything_kernel/connector/rerank/`：Rerank Connector；
  15. `qanything_kernel/dependent_server/embedding_server/`：Embedding Server；
  16. `qanything_kernel/dependent_server/rerank_server/`：Rerank Server；
  17. `qanything_kernel/dependent_server/ocr_server/`：OCR Server；
  18. `qanything_kernel/dependent_server/pdf_parser_server/`：PDF Parser Server；
  19. `qanything_kernel/configs/`：配置文件；
  20. `qanything_kernel/utils/`：工具函数；
  21. `qanything_kernel/dependent_server/insert_files_serve/`：文件摄入 Server；
- **LOC 与语言**：
  - `qanything_kernel/` ~50k LoC；
  - `front_end/` ~10k LoC；
  - `third_party/`（依赖子模块）；
  - 主语言：**Python**（95%）、TypeScript / Vue / React（前端 5%）；
- **构建系统**：
  - **Docker Compose**（`docker-compose-*.yaml`）；
  - **Python pip**（`requirements.txt`）；
  - **Pytest**（`test/`）；
  - **Vite / Webpack**（前端）；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Tutorial（`README.md`、`README_zh.md`、`FAQ_zh.md`）；
  - 跑通 `docker-compose up`（Linux / Mac / Windows）；
  - 上传 PDF / Word / PPT，提问；
  - 了解前端 UI；
- **进阶（2-3 周）**：
  - 深入学习 Connector（Embedding / LLM / Rerank）；
  - 实战 Custom Connector；
  - 研究 Dependent Server（Embedding / Rerank / OCR / PDF Parser）；
  - 学习 Milvus / Elasticsearch 集成；
- **高级（1-2 月）**：
  - 阅读源码（Server + Kernel + Connector + Dependent Server）；
  - 自定义 LLM / Embedding / Rerank；
  - 多租户 / RBAC 实现；
  - 生产部署（Kubernetes + Docker）；
  - 国产化部署（国产 LLM + 国产数据库）；
  - 性能优化（GPU 加速 + 量化）；
- **推荐资源**：
  - 官方文档：https://github.com/netease-youdao/QAnything
  - GitHub：https://github.com/netease-youdao/QAnything
  - Gitee 镜像：https://gitee.com/netease-youdao/QAnything
  - 论文：RAG、BGE-Reranker、PaddleOCR、Milvus、Elasticsearch；
  - 博客：网易有道 AI 博客；
  - 微信群 / QQ 群 / Discord；
  - 中文教程：CSDN / 知乎 / Bilibili；

## 十五、风险与陷阱

- **架构相对早期**：v2.x 模块化重构，v3.x UI 重构，API 频繁变更；
- **多进程部署复杂**：Dependent Server（OCR / PDF Parser / Rerank / Embedding / LLM）多进程，部署复杂；
- **本地 LLM 性能瓶颈**：本地 LLM 推理速度慢，需要 GPU；
- **Milvus 依赖**：Milvus 是核心向量库，迁移到其他向量库需重写 Connector；
- **Elasticsearch 依赖**：Elasticsearch 是核心混合检索引擎，部署需较大内存；
- **PDF 解析性能**：高质量 PDF 解析（pdf_to_markdown）速度较慢，需 GPU 加速；
- **MySQL Schema 复杂**：文件元数据 / 用户 / 权限多表设计，迁移复杂；
- **国产 LLM 切换**：仅 OpenAI / ChatGLM / Qwen / DeepSeek 等少数 LLM 适配；
- **RAG 评估缺失**：默认无 RAG 评估，需自定义集成；
- **Observability 黑盒**：默认无 tracing，需自接 OpenTelemetry / Langfuse；
- **社区生态中等**：相比 langchain / llama-index，社区贡献较少；
- **国产数据库支持有限**：向量库主要依赖 Milvus / Elasticsearch，国产向量库适配较少；
- **多语言支持有限**：默认中文友好，多语种（如英文）支持较弱；
- **License 注意**：Apache 2.0 + 部分第三方依赖（`unstructured`、`PaddleOCR`、`Milvus`）许可协议兼容问题；
- **Docker Compose 多平台差异**：Linux / Mac / Windows 三个 compose 文件，配置不同；

## 十六、参考资料

- 官方仓库：https://github.com/netease-youdao/QAnything
- Gitee 镜像：https://gitee.com/netease-youdao/QAnything
- 官方文档：https://github.com/netease-youdao/QAnything/blob/main/README_zh.md
- FAQ：https://github.com/netease-youdao/QAnything/blob/main/FAQ_zh.md
- 网易有道：https://www.youdao.com/
- BGE：https://github.com/FlagOpen/FlagEmbedding
- PaddleOCR：https://github.com/PaddlePaddle/PaddleOCR
- Milvus：https://milvus.io/
- Elasticsearch：https://www.elastic.co/
- 论文：RAG、BGE-Reranker、PaddleOCR、Milvus、Elasticsearch、ChatGLM、Qwen、DeepSeek
- 博客：网易有道 AI 博客；
- 微信群 / QQ 群 / Discord；
- 中文教程：CSDN / 知乎 / Bilibili / YouTube 中文频道