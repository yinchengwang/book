# txtai Wiki（语义搜索与工作流执行引擎）

> 参考版本：txtai 8.x（2025-2026），由 NeuML 出品的开源语义搜索库。本 wiki 重点介绍 txtai 作为「一体化语义搜索 + LLM 工作流」的定位、RAG 四维度扩展、向量库 / Graph / Workflow 架构、源码学习路径与典型陷阱。

## 一、项目定位

txtai 是由 **David Meza**（Sr. Data Scientist，前 Verizon Media）于 **2020 年开源**的 **「语义搜索 + 工作流执行引擎」**。其核心定位是「**构建可组合的语义工作流，覆盖 Embeddings、向量搜索、SQL、图、LLM、Workflow**」，特点是**轻量级、一体化、生产级**。txtai 用 SQLite + Faiss 后端存储，提供 API（FastAPI）和 Python SDK，是 LLM 应用领域「All-in-One」的代表性项目。

- **核心痛点**：
  - 大多数语义搜索项目（RAG、向量库、Graph）需要拼凑多个库，运维门槛高；
  - Faiss / Pinecone / Milvus 等单独向量库缺少「文本 → 嵌入 → 索引 → 检索 → LLM 生成」一体化方案；
  - 跨语言、多模态、本地推理的部署复杂度高；
  - 工作流编排（如 NLP 流水线）缺少 Pythonic 抽象；
  - **GraphRAG** 缺少一体化框架；
  - 部署时需要同时管理 LLM API、向量库、检索器，缺少单一可执行；
- **典型用户**：
  - 中小规模语义搜索应用（万级 - 百万级）；
  - 一体化 RAG 部署（单进程即可运行）；
  - GraphRAG（图 + 向量融合）；
  - 多语种 / 多模态 RAG；
  - 教程 / 教学演示（txtai 教程详细）；
  - 生产级 API（FastAPI + Docker）；

## 二、发展历史与社区

- **起源**（2020）：David Meza 在 Verizon Media 工作期间，**作为 Python 库**，最初是 BERT + Faiss + SQLite 的组合；
- **里程碑**：
  - 2020-09：v1.0，Embeddings + ANN（Faiss）+ SQLite；
  - 2021-06：v2.0，引入 Pipeline 抽象；
  - 2022-03：v3.0，Workflow 引擎；
  - 2023-04：v5.0，引入 Graph（图存储 + GraphRAG）；
  - 2023-09：v6.0，引入 LLM Pipeline + Agent；
  - 2024-03：v7.0，Multilingual / Multimodal / Cloud 集成；
  - 2024-09：v8.0，Workflow 重构、Auto 索引、向量库可插拔；
  - 2025-09：v8.5+，优化 Cloud、本地 RAG agent、SentenceSplitter 重构；
- **维护状态**：**活跃**（maintained by NeuML），社区 30+ 贡献者，月均 30+ commits；
- **社区**：
  - GitHub 10k+ star、Apache 2.0 License；
  - Discord 1k+ 成员、Twitter @ neuroml；
  - 商业产品 NeuML Cloud（闭源 SaaS，托管 txtai pipelines + GPU）、作者 / 题库 / 镜像评测；
- **关键贡献者**：David Meza（创始人 / 主架构）、Vinh Nguyen（贡献者）、社区贡献者；

## 三、许可与商业限制

- **核心代码**：**Apache 2.0**（`src/python/txtai/`、`src/python/txtai/api/`），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **NeuML Cloud**（闭源 SaaS）：托管 txtai pipelines、GPU 加速、企业安全；
- **生态关系**：
  - **上游依赖**：`numpy`、`fastapi`、`uvicorn`、`pydantic`、`torch`（可选）、`transformers`（可选）、`sentence-transformers`（可选）、`openai`（可选）；
  - **下游生态**：约 **500+ 项目使用 txtai**，包括数据科学 / NLP 团队、教育 / 学术机构、NeuML Cloud 用户；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **NeuML Cloud**：托管用户；
  - **多家初创公司**：中型语义搜索；
  - **教育 / 学术机构**：教程项目；
  - **数据科学 / NLP 团队**：沉浸式语义搜索；
- **国内**：
  - 多家中型 RAG 应用；
  - 学术机构教程；
- **典型场景**：
  - **一体化 RAG**：单进程即可运行完整 RAG；
  - **GraphRAG**：图 + 向量融合检索；
  - **多语种 RAG**：HuggingFace 多语嵌入；
  - **多模态 RAG**：图像 + 文本嵌入；
  - **本地语义搜索**：百万级向量，无需分布式；
  - **教程 / 教学**：txtai 教程详细且开源；
  - **生产 API**：FastAPI + Docker 部署；

## 五、一句话对比定位

> txtai 是 **「All-in-One 语义搜索 + GraphRAG 一体化 + Workflow 引擎 + Faiss + SQLite + FastAPI + Apache 2.0 + NeuML Cloud 商业矩阵 + 10k star + 教学友好」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**embedchain（极简封装）**、**私人 GPT（本地）**、**Quivr（个人知识库）** 构成 RAG 框架生态。txtai 的差异化是 **「All-in-One 单进程 + GraphRAG + Workflow + SQLite 内嵌 + 教学友好 + 轻量级」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（apps）                                   │
│        txtai API（FastAPI） / Python SDK / Web UI                    │
│        docker run neuml/txtai-api / txtai-api --port 8000           │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Workflow 引擎（txtai/workflow/）                    │
│   Workflow.__call__ / async workflow() / yaml workflow config       │
│   Node / Task / Local / Cloud / Pythonic DAG                        │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Pipeline 抽象（txtai/pipeline/）                    │
│   40+ 内置 Pipeline：Embeddings / LLM / RAG / Summary / Translate   │
│   Transcribe / Questions / Labels / Similarity / Reranker           │
│   factory(pipeline) => 自动发现                                       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Embeddings     │ │  Vector / ANN    │ │  Graph            │ │  Database  │
│  (txtai/embed  │ │  Faiss / HNSW    │ │  NetworkX / TuGraph│ │  SQLite   │
│  dings/)       │ │  IVF / ScaNN     │ │  图存储 + 检索     │ │  外部存储   │
│  HF / OpenAI    │ │  （txtai/vectors/）│ │  （txtai/graph/）   │ │  （txtai/data/）│
└─────────────────┘ └──────────────────┘ └───────────────────┘ └────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  LLM / RAG Pipeline（txtai/pipeline/llm/）          │
│   ragmatract  / question / extract / summary / generate / label     │
│   LLM (OpenAI / HuggingFace / Anthropic / Cohere / llama.cpp / GGUF) │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：纯 Python 库 + API Server（FastAPI）；Embeddings 默认进程内 Faiss + SQLite；可选 NeuML Cloud 远程 GPU。

## 七、嵌入模型适配（EMB）

txtai 通过 `Embeddings` 类（`src/python/txtai/embeddings/base.py`）暴露 `index()` / `search()` / `transform()` 接口，下游支持 30+ 模型：

- **HuggingFace**：
  - **sentence-transformers**：`Embeddings(path="sentence-transformers/all-MiniLM-L6-v2")`（384 维）；
  - **自研嵌入**：`Embeddings(path="BAAI/bge-small-en-v1.5")`、`bge-large-zh-v1.5`（1024 维，中文首选）；
  - **E5**：`intfloat/e5-large-v2`（1024 维）；
  - **M3E**：`MokaAI/m3e-large`（中文）；
  - **智源 BGE**：`BAAI/bge-large-zh-v1.5`；
  - **BGE-M3**：`BAAI/bge-m3`（多语，1024 维）；
  - **Jina**：`jinaai/jina-embeddings-v2-base-en`（768 维）；
  - **Instructor**：`hkunlp/instructor-xl`（指令嵌入）；
  - **ColBERT**：`colbert-ir/colbertv2.0`（晚交互式）；
- **OpenAI**：`Embeddings(path="openai/text-embedding-3-small")`（1536 维）；
- **Cohere**：`Embeddings(path="cohere/embed-english-v3.0")`（1024 维）；
- **本地 ONNX**：`Embeddings(path="onnx/model.onnx")`；
- **CLIP**：`Embeddings(path="openai/clip-vit-base-patch32")`（图像 + 文本，512 维）；
- **CLIP 多模态**：`CLIP()` 单独类；
- **Ollama**：`Embeddings(path="ollama/nomic-embed-text")`；
- **llama.cpp / GGUF**：`Embeddings(path="llama.cpp/nomic-embed-text-v1.5-GGUF")`（本地 CPU 推理）；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - `Embeddings(content="txtai.database.factory.SentenceSplitter")`（按 sentence / word / passage）；
  - 自定义 `Tokenizer` 抽象类；
  - 章节 / 段落感知；
  - **推荐参数**：chunk_size 256-512 token、overlap 10%；

## 八、文档解析器（LOAD）

txtai 通过 `Embeddings.index()` 接口或自定义 `Stream` 暴露数据加载，下游支持 50+ 数据源：

- **本地文件**：
  - CSV / JSON / JSONL / TSV / Parquet；
  - TXT（纯文本）；
  - PDF（基于 `pypdf` / `pdfminer`）；
  - HTML（基于 `trafilatura` / `selectolax`）；
  - Docx（基于 `python-docx`）；
  - Markdown（基于 `markdown-it-py`）；
- **数据库 dump**：
  - SQLite（`txtai.database.factory.SQLite`）；
  - DuckDB；
  - Elasticsearch（`Elasticsearch`）；
  - PostgreSQL / MySQL；
- **自定义 Stream**：
  - `Stream` 抽象（`src/python/txtai/database/base.py`）；
  - 子类化实现 `read()` / `stream()`；
  - 常见流：S3、Confluence、Notion、Slack、GitHub、HuggingFace Datasets、arXiv；
- **流式读取**：流式处理大数据集，无需全部加载到内存；

## 十、检索器与重排器（RETRIEVE / RERANK）

txtai 通过 `Embeddings.search()` 接口暴露检索，下游支持多种检索模式：

- **向量检索**（默认）：
  - `Embeddings.search(query, limit)` → 余弦相似度；
  - 支持 `weights`（多字段权重）；
  - 支持 `filter`（metadata 过滤）；
- **稀疏检索**：
  - `Embeddings.search(query, limit, weights={"text": 0.7, "title": 0.3})`；
  - 自定义 tokenizer 权重；
- **图检索**：
  - `Graph.search(query)` → 图遍历（Cypher 风格）；
  - **GraphRAG**：通过 `Graph` 类定义图 + `Embeddings` 类定义向量 → `RAGPipeline` 融合；
- **Rerank**：
  - **CrossEncoder**：`Reranker(path="colbert-ir/colbertv2.0")`；
  - **Bi-Encoder**：`Embeddings(path=...)`（默认）；
  - **LateEncoder**（晚交互式）：`LateEncoder(path="colbert-ir/colbertv2.0")`；
  - **LLM-based Rerank**：通过 `LLM.prompt()` + prompt template；
- **混合检索**：
  - `RAGPipeline(rerank="crossencoder")`：先用 bi-encoder 检索，再用 cross-encoder 重排；
  - `RAGPipeline(autoscale=True)`：自动缩放 topK；
  - 自定义 `Reranker` 抽象；
- **问题生成**：
  - `Questions()`（`src/python/txtai/pipeline/text/questions.py`）：基于 LLM 生成问题的替代查询；
  - 多查询融合；

## 十、向量库后端（VECTOR STORE）

txtai 通过 `Vectors` 类（`src/python/txtai/vectors/base.py`）暴露后端抽象：

- **Faiss**（默认）：
  - `Flat()`（精确）；
  - `IVF()`（倒排文件索引）；
  - `HNSW()`（Hierarchical Navigable Small World）；
  - `IVFPQ()`（倒排 + 乘积量化）；
  - `IVFScalarQuantizer()`；
  - `BinaryIVF()`（二值）；
- **ScaNN**：`ScaNN()`（Google C++）；
- **NMSLIB**：`NMS()`（基于 NMSLIB C++，HNSW / SW-graph）；
- **零扩展**：`hnswlib` Python 绑定（`Hnswlib()`，`txtai/vectors/hnswlib.py`）；
- **Annoy**：`Annoy()`（Spotify 随机投影树）；
- **NumPy**：`NumPy()`（精确，小量/教学）；
- **PyNNDescent**：`NNDescent()`（基于 NN-descent）；
- **PGVector**（PostgreSQL）：`PGVector()`；
- **Qdrant**（社区集成）：`Qdrant()`；
- **自定义 Vectors**：子类化 `Vectors` 基类；
- **存储后端**：
  - **SQLite**（默认，本地文件）；
  - **DuckDB**（列存）；
  - **外部数据库**：`txtai.config/Embeddings.config` 中设置；
- **选型建议**：
  - 原型 / 小规模（< 100k）：Faiss Flat / NumPy；
  - 中规模（100k - 1M）：Faiss IVF / HNSW / PGVector；
  - 大规模（> 1M）：Faiss IVFPQ / ScaNN / NMSLIB；
  - GraphRAG：txtai Graph 一体化；

## 十一、查询/计算/接口层

- **Pipeline 抽象**（`src/python/txtai/pipeline/base.py`）：
  - `Pipeline.__call__(inputs)`；
  - `factory(pipeline)` → 自动发现内置 Pipeline；
  - 40+ 内置 Pipeline；
  - 自定义 Pipeline：继承 `Pipeline`；
- **内置 Pipeline**（40+）：
  - `Embeddings()`（上文 § 七）；
  - `RAG()`（RAG Pipeline）；
  - `LLM()`（LLM 抽象）；
  - `Summary()`（文本摘要）；
  - `Translation()`（翻译）；
  - `Transcription()`（音频转录，Whisper）；
  - `Textractor`（文本类 Pipeline 集合）；
  - `Questions()`（问题生成）；
  - `Labels()`（零样本 / 少样本分类）；
  - `Similarity()`（句子相似度）；
  - `Reranker()`（重排）；
  - `CrossEncoder()`（交叉编码器）；
  - `LateEncoder()`（晚交互式）；
  - `ImageCaption()`（图像生成文本）；
  - `Objects()`（目标检测）；
  - `Entity()`（实体识别）；
- **LLM Pipeline**（`txtai/pipeline/llm/`）：
  - **LLM** 类：统一 LLM 抽象；
  - **支持**：
    - **HuggingFace**（本地）：`LLM(path="meta-llama/Llama-3-8B-Instruct")`（`transformers` / `vllm` / `llama.cpp` / `mlx`）；
    - **OpenAI**：`LLM(path="openai/gpt-4o")`；
    - **Anthropic**：`LLM(path="anthropic/claude-3-5-sonnet")`；
    - **Cohere**：`LLM(path="cohere/command-r-plus")`；
    - **Google Gemini**：`LLM(path="gemini-pro")`；
    - **Mistral**：`LLM(path="mistral/mistral-large")`；
    - **Groq**：`LLM(path="groq/llama-3-1-70b-versatile")`；
    - **Ollama**：`LLM(path="ollama/llama3")`；
    - **llama.cpp**：`LLM(path="llama.cpp/Llama-3-8B-Instruct-GGUF")`（本地 CPU）；
  - **流式响应**：`LLM(stream=True)`；
  - **Function Calling**：`LLM(tools=[...])`；
  - **prompt template**：自定义 prompt → response；
- **RAG Pipeline**：
  - `RAG(rag="txtai.pipeline.rag.RAG")`；
  - 配置：`RAGPipeline(embeddings, graph, rerank, llm)`；
  - 支持 GraphRAG / VectorRAG / HybridRAG；
- **Workflow 引擎**（`src/python/txtai/workflow/`）：
  - `Workflow.__call__(elements)` → 多步并行 / 串行；
  - YAML 配置 / Python 编程；
  - Local / Cloud 节点（远端 LLM / 远端 GPU）；
  - DAG 拓扑；
- **Graph**（`src/python/txtai/graph/`）：
  - `Graph.create(elements)` → 创建图；
  - `Graph.search(query)` → 遍历图；
  - **后端**：
    - **NetworkX**（默认）：进程内；
    - **TuGraph**（`TuGraph()`）：企业级分布式图存储；
  - **GraphRAG**：通过 `Embeddings` + `Graph` → `RAGPipeline`；
- **Agent**（v6.0+）：
  - `Agent(tools=[...], llm=LLM(...))`；
  - **ReAct** 风格；
  - **Function Calling**；
  - **Tool**：自定义工具函数（`@tool` 装饰器）；
- **聊天 / 记忆**：
  - `LLM.prompt(messages=[...])` → 多轮对话；
  - 自定义 Memory（in-memory / SQLite / Redis）；
- **评估与可观测性**：
  - **ragas**（自定义集）；
  - **DeepEval**（社区集）；
  - **Phoenix**（社区集）；
  - **OpenTelemetry**（部分支持）；
- **生产部署**：
  - FastAPI + Uvicorn（`txtai-api` 命令）；
  - Docker：`docker run neuml/txtai-api`；
  - **NeuML Cloud**（官方托管）；
  - Kubernetes；

## 十二、生态与集成

- **客户端 SDK**：Python（主力）、HTTP API（自动生成 OpenAPI）、JavaScript / TypeScript（社区）；
- **集成矩阵**：
  - **LLM 提供商**：OpenAI、Anthropic、Cohere、Google Gemini、Groovy、Mistral、Ollama、llama.cpp、HuggingFace（本地）；
  - **向量库**：Faiss（默认）、ScaNN、NMSLIB、hnswlib、Annoy、NumPy、PyNNDescent、PGVector；
  - **图存储**：NetworkX、TuGraph；
  - **数据库**：SQLite（默认）、DuckDB、PostgreSQL、Elasticsearch；
  - **音频 / 图像**：Whisper、CLIP、YOLO；
  - **数据源**：CSV / JSON / Parquet / HuggingFace Datasets / S3 / Confluence / Notion；
- **NeuML Cloud**（商业 SaaS）：托管 txtai pipelines；
- **可视化工具**：txtai-graph（社区）；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `src/python/txtai/embeddings/base.py`：Embeddings 主类（Faiss + SQLite + index/search/upsert）；
  2. `src/python/txtai/vectors/base.py`：Vectors 抽象；
  3. `src/python/txtai/database/pipeline.py`：数据库操作；
  4. `src/python/txtai/pipeline/base.py`：Pipeline 抽象；
  5. `src/python/txtai/pipeline/llm/llm.py`：LLM Pipeline；
  6. `src/python/txtai/pipeline/llm/rag.py`：RAG Pipeline；
  7. `src/python/txtai/graph/base.py`：Graph 主类；
  8. `src/python/txtai/workflow/base.py`：Workflow 引擎；
  9. `src/python/txtai/embeddings/index/`：索引逻辑；
  10. `src/python/txtai/api/routers/`：FastAPI 路由；
- **LOC 与语言**：
  - `src/python/txtai/` 约 25k LoC；
  - 主语言：Python（99%）、少量 TypeScript（JS 端）；
- **构建系统**：Hatch；pytest + mypy + black + ruff；
- **CLI 命令**：
  - `txtai`（CLI）；
  - `txtai-api`（FastAPI server）；
  - `txtai-workflow`（Workflow runner）；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Quickstart（`src/python/txtai/README.md`）；
  - 跑通 Embeddings + search；
  - 跑通 LLM Pipeline；
  - 跑通 RAG Pipeline；
- **进阶（2-3 周）**：
  - 深入学习 Embeddings（Faiss 后端、HNSW、IVFPQ）；
  - 学习 Graph（图存储 + GraphRAG）；
  - 学习 Workflow（多步编排）；
  - 学习 Agent（Tools + LLM）；
- **高级（1-2 月）**：
  - 阅读源码（Embeddings + RAG Pipeline + Workflow）；
  - 自定义 Vector 后端 / Pipeline / Workflow；
  - NeuML Cloud 集成；
  - 多语种 / 多模态 RAG；
  - 生产部署（FastAPI + Docker + Kubernetes）；
- **推荐资源**：
  - 官方文档：https://neuml.github.io/txtai/
  - Tutorial：https://neuml.github.io/txtai/getting-started/
  - GitHub：https://github.com/neuml/txtai
  - NeuML Cloud：https://cloud.neuml.com/
  - 论文 / 概念：RAG、HNSW、Faiss、ColBERT、DPR、GraphRAG；

## 十五、风险与陷阱

- **一体化架构**：所有功能都在一个库中，单包安装，升级时有库依赖（`pip install txtai[all]`），生产中建议按需 install；
- **Faiss 后端复杂度**：Faiss 的 IVF / HNSW 参数调优需要专业知识（`nlist`、`nprobe`、`M`、`efConstruction`）；
- **SQLite 后端限制**：单文件 SQLite 限制（< 281 TB 没问题），但写入并发限制（默认单写多读，需 WAL 模式）；
- **Graph 性能**：NetworkX 进程内实现，大图（> 100k 节点）需 TuGraph 分布式后端；
- **LLM API 切换**：LLM 抽象虽然统一，但各 provider API 差异（Function Calling、流式、Prompt 模板），切换需适配；
- **HuggingFace 模型部署**：本地 LLM 需要 GPU + 大内存（Llama 3 8B 需要 ~16GB VRAM），生产部署复杂；
- **Workflow 调试**：YAML 配置的工作流调试困难（哪个节点报错？）；
- **ragas 集成缺失**：默认无 RAG 评估，需自定义集成；
- **Observability 黑盒**：默认无 tracing，需自接 OpenTelemetry 或 NeuML Cloud；
- **neuML Cloud 闭源**：商业项目需购买 NeuML Cloud 或自建 GPU 集群；
- **GraphRAG 概念复杂**：Cypher 风格的图查询学习曲线陡；
- **Python 3.10+ 要求**：txtai 8.x 要求 Python 3.10+，旧环境无法升级；
- **零迁移风险**：txtai 早期版本（v6.x → v8.x）部分接口变更，升级需读 migration doc；

## 十六、参考资料

- 官方仓库：https://github.com/neuml/txtai
- 官方文档：https://neuml.github.io/txtai/
- NeuML Cloud：https://cloud.neuml.com/
- 论文：Faiss（Johnson et al. 2017, https://arxiv.org/abs/1702.08734）、HNSW（Malkov & Yashunin 2018, https://arxiv.org/abs/1603.09320）、ColBERT（Khattab & Zaharia 2020, https://arxiv.org/abs/2004.12832）、DPR（Karpukhin et al. 2020, https://arxiv.org/abs/2004.04906）
- 博客：https://neuml.github.io/txtai/blog/
- Tutorial：https://neuml.github.io/txtai/getting-started/
- 中文社区：https://github.com/neuml/txtai/discussions