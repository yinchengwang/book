# embedchain Wiki（RAG 应用框架 / Mem0 长期记忆层）

> 参考版本：embedchain 0.1.x（2023）→ mem0 2.x（2024-2026）。原 embedchain 项目已演进为 mem0，定位从「极简 RAG 封装」扩展为「AI 长期记忆层（Memory Layer for Personalized AI）」。本 wiki 同时介绍 embedchain 历史定位与 mem0 当前能力，源码以 `mem0/` 目录结构为准，并交叉引用 mem0 官方仓库。

## 一、项目定位

embedchain 是由 **Taranjeet Chugh**（Founder @ Mem0ai）于 **2023 年 4 月开源**的 **「极简 RAG 应用框架」**，定位是「**用最少的代码构建生产级 RAG 应用**」，通过 `add()` / `query()` 两个 API 抽象数据接入、嵌入、检索、生成的完整流水线。2024 年项目演进为 **mem0**，定位升级为 **「AI 应用的长期记忆层（Memory Layer for Personalized AI）」**，支持短期 / 长期 / 用户级 / Agent 级记忆管理、个性化、自适应检索。

- **核心痛点**：
  - 现有 RAG 框架（langchain、llama-index）学习曲线陡，代码量大；
  - 新人想要快速搭建 RAG Demo，需要写大量配置；
  - LLM 应用缺长期记忆（用户偏好、对话历史、上下文关联）；
  - 多用户 / 多 Agent 场景下记忆隔离 / 共享困难；
  - 记忆检索需要结合语义搜索、图搜索、知识图谱；
  - 数据接入 / 切分 / 嵌入 / 检索 / 重排 / 生成「一个应用代码」极简封装；
- **典型用户**：
  - **RAG 入门开发者**：快速构建 RAG Demo；
  - **AI 应用开发者**：需要用户级 / Agent 级记忆管理；
  - **个性化 AI 助手**：用户偏好 / 历史对话记忆；
  - **Multi-Agent 协作**：Agent 间共享 / 隔离记忆；
  - **教育 / 教学**：embedchain 入门教程友好；

## 二、发展历史与社区

- **起源**（2023-04）：Taranjeet Chugh 开源 embedchain，定位「**5 行代码构建 RAG**」；
- **里程碑（embedchain 时代）**：
  - 2023-04：v0.1，简化封装 langchain 之上；
  - 2023-06：v0.7，扩展 60+ Loader（PDF / YouTube / Notion / Substack）；
  - 2023-09：v0.13，集成 8+ Vector DB（Chroma / FAISS / Pinecone / Weaviate）；
  - 2024-01：v0.15，API Server（FastAPI）；
- **里程碑（mem0 转型）**：
  - 2024-04：项目更名为 mem0（Memory + Zero），定位升级为「Long-term Memory Layer」；
  - 2024-06：v0.1，新增 Memory 类、个性化、用户隔离；
  - 2024-09：v1.0 GA，5000+ star；
  - 2024-12：v1.5+，集成 Graph Memory（图记忆）；
  - 2025-03：v2.0，OSS Edition 与 Cloud Edition 分离，Mem0 Cloud 商业版发布；
  - 2025-09：v2.2+，Multi-Memory、Agent Memory、Vector Store 全面化；
- **维护状态**：**活跃**（maintained by Mem0ai, Inc.），社区 80+ 贡献者，月均 100+ commits；
- **社区**：
  - GitHub 30k+ star（mem0）、6k+ star（embedchain 已 archived）；
  - Discord 5k+ 成员、Twitter 100k+ 关注；
  - 商业产品 Mem0 Cloud（闭源 SaaS，托管 Mem0 服务 + 多租户）；
- **关键贡献者**：Taranjeet Chugh（CEO / Founder）、Deshraj Yadav（CTO）、Karan Vaidya（Founding Engineer）、Mem0 团队 30+ 工程师；

## 三、许可与商业限制

- **核心代码**：**Apache 2.0**（`mem0/`、`mem0-ts/`，原 `embedchain/` 目录已删除 / 仓库已 archived，所有代码迁移至 mem0），可商用、修改、私有部署；
- **与商业产品的关系**：
  - **Mem0 Cloud**（闭源 SaaS）：托管多租户 Mem0 服务、个性化、记忆分析、Memory Ops 工具；
  - **Mem0 Enterprise**：本地 / 私有部署版（与 AWS / Azure / GCP 集成）；
- **生态关系**：
  - **上游依赖**：`pydantic`、`openai`、`qdrant-client`、`sqlalchemy`、`httpx`、`posthog`、`pytz`、`protobuf`；
  - **下游生态**：约 **5000+ 项目使用 mem0**，包括 AI 助手、客服机器人、个性化推荐、教育 AI、Multi-Agent 协作；

## 四、业界主要使用方与案例

- **国际**（公开）：
  - **多家 AI 助手初创公司**：个性化助手；
  - **客服 AI 公司**：用户偏好记忆；
  - **教育 AI 公司**：学习进度记忆；
  - **Multi-Agent 框架**：Agent 间共享记忆（LangGraph / CrewAI / Autogen 集成）；
- **国内**：
  - 多家教育 / 客服 / 助手 AI 公司；
- **典型场景**：
  - **个性化 AI 助手**：用户偏好 / 历史对话记忆；
  - **客服 RAG**：用户历史工单 / 偏好；
  - **Multi-Agent 协作**：Agent 间共享 / 隔离记忆；
  - **教育 AI**：学习进度记忆；
  - **医疗 AI**：患者病史 / 偏好记忆；
  - **电商 AI**：用户偏好 / 购物历史记忆；
  - **Mem0 Cloud**：托管记忆服务（OSS 替代品）；

## 五、一句话对比定位

> mem0（embedchain 演进版）是 **「极简 RAG + AI 长期记忆层 + Multi-Memory + Graph Memory + Mem0 Cloud + 30k star + Apache 2.0 + 入门友好」**；与 **LangChain（编排）**、**LlamaIndex（数据）**、**Haystack（管道）**、**txtai（一体化）**、**私人 GPT（本地）**、**Quivr（个人知识库）**、**Letta（开源 MemGPT）** 构成 RAG / 记忆框架生态。mem0 的差异化是 **「Memory-First + 多租户 + 多类型 + 极简 API + Mem0 Cloud + 与 LangGraph 集成」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  用户应用层（apps）                                   │
│        FastAPI / Flask / Mem0 Cloud API / Python SDK                │
│        multi-modal memory.add / memory.search / memory.get_all       │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Memory 抽象（mem0/memory/）                         │
│   Memory.add / Memory.search / Memory.get_all / Memory.update       │
│   Memory.delete / Memory.history / Memory.delete_all                │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
┌──────────┬─────────────────┼─────────────────┬───────────────────────┐
│          │                 │                 │                       │
┌──────────▼──────┐ ┌─────────▼────────┐ ┌────▼──────────────┐ ┌─────▼──────┐
│  Memory Types   │ │  LLM Factory     │ │  Embedder Factory  │ │ Reranker  │
│  UserMemory     │ │  OpenAI / Anthrop│ │  OpenAI / HF       │ │ Cohere /  │
│  AgentMemory    │ │  Groq / Together │ │  Cohere / Voyage   │ │ BGE /     │
│  SessionMemory  │ │  Ollama / LMStudio│ │  HuggingFace       │ │ Jina /    │
│  GraphMemory    │ │  DeepSeek / vLLM │ │  VertexAI / Azure  │ │ LLMRerank │
└──────────┬──────┘ └─────────┬────────┘ └────┬──────────────┘ └─────┬──────┘
           │                  │                │                   │
┌──────────▼──────────────────▼────────────────▼───────────────────▼───────────────────┐
│                    Vector Stores / Graph Stores / Relational Stores                    │
│    Qdrant / Chroma / PGVector / Pinecone / Weaviate / Milvus / Azure / LanceDB / FAISS │
│    Memgraph / Neo4j / NetworkX / Kuzu                                                  │
│    SQLite / Postgres / MySQL (历史元数据)                                                │
└─────────────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────────────┐
│                  Proxy / Storage Abstraction（mem0/proxy/）          │
│   MemoryStore（基底）+ MemoryClient（轻量）+ OpenAI 兼容 API           │
└─────────────────────────────────────────────────────────────────────┘
```

进程模型：纯 Python 库；可选 FastAPI Server（Mem0 OSS）；Mem0 Cloud 远程 API。

## 七、嵌入模型适配（EMB）

mem0 通过 `EmbedderFactory`（`mem0/configs/embeddings/`）暴露抽象，下游 30+ 实现：

- **OpenAI**：`OpenAIEmbedder`（text-embedding-3-small/large 1536/3072 维）、`AzureOpenAIEmbedder`；
- **HuggingFace**：`HuggingFaceEmbedder`（`BAAI/bge-large-zh-v1.5`、`intfloat/e5-large-v2`、`all-MiniLM-L6-v2`）；
- **Cohere**：`CohereEmbedder`（embed-english-v3.0 / embed-multilingual-v3.0 1024 维）；
- **Voyage AI**：`VoyageEmbedder`（voyage-3 1024 维）；
- **Google Vertex AI**：`VertexAIEmbedder`（textembedding-gecko / text-embedding-005）；
- **AWS Bedrock**：`BedrockEmbedder`；
- **Together AI**：`TogetherEmbedder`；
- **Ollama**：`OllamaEmbedder`（`nomic-embed-text`）；
- **LMStudio**：`LMStudioEmbedder`；
- **vLLM**：`VLLMEmbedder`；
- **DeepSeek**：`DeepSeekEmbedder`（社区）；
- **智源 BGE**：`BgeEmbedder`；
- **通义 DashScope**：`DashScopeEmbedder`；
- **自定义**：`CustomEmbedder`（继承 BaseEmbedder）；
- **维度选择**：384 / 768 / 1024 / 1536 / 3072；
- **切分策略**：
  - `Chunker`（`mem0/memory/chunking.py`，基于 langchain text splitter）；
  - 默认 `chunk_size=500`，`overlap=50`；
  - 句子级 / 段落级 / token 级；
  - 自定义 Chunker：`CustomChunker(separator="\n\n")`；

## 八、文档解析器（LOAD）

mem0 通过 `Message` 抽象 + 自定义 Loader 暴露数据接入，下游支持多种输入：

- **文本输入**：
  - `Memory.add(messages, user_id=...)`：对话消息；
  - `Memory.add(text=..., metadata=...)`：纯文本；
  - **Message 结构**：role（user / assistant / system）+ content；
- **文件输入**（自定义 Loader）：
  - **PDF / Docx / CSV / JSON / HTML**：继承 `BaseLoader`；
  - 通过 `load_data()` 接口；
- **Notion / Confluence / Slack / GitHub**（社区）：
  - `NotionLoader` / `ConfluenceLoader` / `SlackLoader` / `GitHubLoader`；
  - 通过 `webhook` / `cron` 同步；
- **音频 / 图像**（社区）：
  - **Whisper** 转录；
  - **CLIP** 图像嵌入；
- **数据库 dump**：
  - 自定义 ETL → Message 列表；
- **API 输入**：
  - `Memory.from_messages(messages)`；
  - `Memory.add(messages=[{"role": "user", "content": "..."}])`；
- **Graph Memory Loader**：
  - 从对话中抽取（user, relation, target）三元组；
  - 写入 Graph DB（Memgraph / Neo4j）；

## 九、检索器与重排器（RETRIEVE / RERANK）

mem0 通过 `Memory.search()` 接口暴露检索，下游支持多类型检索：

- **向量检索**（默认）：
  - `Memory.search(query, user_id, limit=10)` → 余弦相似度；
  - 支持 `filter`（user_id / agent_id / metadata）；
- **图检索（GraphRAG）**：
  - `Memory.search(query, search_mode="graph")`；
  - **Graph Memory**：基于 Memgraph / Neo4j；
  - **三元组抽取**：LLM 从对话中抽取（user, relation, target）；
  - **图遍历**：从 query → 相关节点 → 邻居节点；
- **混合检索**：
  - `Memory.search(query, search_mode="hybrid")`：向量 + 图融合；
  - `Reranker`：在向量结果基础上 rerank；
- **重排（Rerank）**：
  - **Cohere**：`CohereReranker`（`mem0/configs/rerankers/cohere.py`，Cohere Rerank 3 / 3.5）；
  - **BGE**：`BgeReranker`（`mem0/configs/rerankers/bge.py`，BAAI/bge-reranker-v2-m3）；
  - **LLM-based**：`LLMReranker`（用 LLM 打分）；
  - **HuggingFace**：`HuggingFaceReranker`（cross-encoder）；
- **个性化检索**：
  - `Memory.search(query, user_id="alice")`：用户隔离；
  - `Memory.search(query, agent_id="bob")`：Agent 隔离；
  - `Memory.search(query, run_id="session-1")`：会话隔离；
- **记忆更新 / 冲突解决**：
  - **ADD / UPDATE / DELETE / NOOP**：LLM 决策记忆操作；
  - **Prompt 模板**：`mem0/configs/prompts.py`；
  - **冲突解决**：基于 LLM 判定记忆是否需要更新；

## 十、向量库后端（VECTOR STORE）

mem0 通过 `VectorStoreFactory`（`mem0/configs/vector_stores/`）暴露抽象，下游 20+ 后端：

- **Qdrant**：`QdrantStore`（默认）；
- **Chroma**：`ChromaStore`；
- **pgvector**：`PGVectorStore`（PostgreSQL + pgvector）；
- **Pinecone**：`PineconeStore`；
- **Weaviate**：`WeaviateStore`；
- **Milvus**：`MilvusStore`；
- **LanceDB**：`LanceDBStore`；
- **FAISS**：`FaissStore`（进程内）；
- **Azure Search**：`AzureAISearchStore`；
- **Supabase**：`SupabaseStore`；
- **Elasticsearch**：`ElasticsearchStore`；
- **OpenSearch**：`OpenSearchStore`；
- **MongoDB Atlas**：`MongoDBAtlasStore`；
- **Cassandra**：`CassandraStore`；
- **Redis**：`RedisStore`；
- **Vespa**：`VespaStore`；
- **Upstash**：`UpstashVectorStore`（serverless）；
- **Baai**：`BaaiStore`（BAAI 商业向量库）；
- **MemoryGraph**：`MemgraphStore`（图数据库）；
- **Neo4j**：`Neo4jStore`（图数据库）；
- **NetworkX**：`NetworkXStore`（进程内图）；
- **Kuzu**：`KuzuStore`（嵌入式图）；
- **Memory**：`MemoryStore`（进程内 / 测试用）；
- **Vector Store + Graph Store 二选一**：
  - 简单场景：只用 Vector Store；
  - GraphRAG：Vector Store + Graph Store；
- **选型建议**：
  - 原型 / 小规模：FAISS / Chroma / MemoryGraph（NetworkX）；
  - 中规模（10k - 1M）：Qdrant / Weaviate / pgvector；
  - 大规模（> 1M）：Milvus / Pinecone / Elasticsearch；
  - GraphRAG：Memgraph / Neo4j（生产）；NetworkX / Kuzu（原型）；

## 十一、查询/计算/接口层

- **Memory API**（`mem0/memory/main.py`）：
  - `Memory.add(messages, user_id=..., agent_id=..., run_id=..., metadata=...)`；
  - `Memory.search(query, user_id=..., agent_id=..., run_id=..., limit=10, filters=...)`；
  - `Memory.get_all(user_id=..., agent_id=...)` → 列出所有记忆；
  - `Memory.update(memory_id, data)`；
  - `Memory.delete(memory_id)`；
  - `Memory.delete_all(user_id=...)`；
  - `Memory.history(memory_id)` → 变更历史；
  - `Memory.reset()` → 清空所有记忆；
- **Memory Modes**：
  - **OSS**（本地 / 私有部署）；
  - **Cloud**（Mem0 Cloud 托管）；
  - **Enterprise**（本地 / 私有 / 企业级）；
- **LLM 抽象**（`mem0/configs/llms/`）：
  - **OpenAI**（gpt-4o / gpt-4-turbo / gpt-3.5-turbo）；
  - **Anthropic**（claude-3-5-sonnet / claude-3-opus / claude-3-haiku）；
  - **Groq**（llama-3.1-70b-versatile / mixtral-8x7b）；
  - **Together**（Llama / Mixtral / Qwen）；
  - **Ollama**（本地 LLM）；
  - **LMStudio**（本地 LLM）；
  - **vLLM**（高性能推理）；
  - **DeepSeek**（DeepSeek-V3 / DeepSeek-R1）；
  - **Anthropic Vertex**（Claude on GCP）；
  - **Azure OpenAI**（Azure 上的 OpenAI）；
  - **AWS Bedrock**（AWS Bedrock 上的 Claude / Titan / Llama）；
  - **Google Gemini**（gemini-pro / gemini-1.5-pro）；
  - **xAI**（Grok）；
  - **MiniMax**（MiniMax M3 / 自研模型）；
- **Graph Memory**（`mem0/memory/graph_memory.py`）：
  - 从对话中抽取三元组（subject, predicate, object）；
  - 写入 Graph Store（Memgraph / Neo4j）；
  - GraphRAG：基于图遍历的检索；
- **流式响应**：
  - **LLM 流式**：与 OpenAI 兼容（streaming callback）；
  - **Memory 流式**：实时添加记忆，无需等待；
- **记忆可视化**：
  - **Memory Dashboard**（Mem0 Cloud）；
  - **Memory Timeline**：记忆增删改历史；
  - **Memory Search Test**：在线检索测试；
- **评估与可观测性**：
  - **Posthog 集成**（mem0 依赖 posthog 收集匿名统计）；
  - **OpenTelemetry**（部分支持）；
  - **Langfuse 集成**；
  - **Phoenix 集成**；
  - **Custom Evaluator**：用户自定义评估；
- **生产部署**：
  - **OSS**：Docker / Kubernetes / Python；
  - **Cloud**：Mem0 Cloud（官方托管）；
  - **Enterprise**：本地 / 私有 / 多租户；
- **CLI**：
  - `mem0` CLI 命令；
  - `mem0-api` FastAPI 服务（`mem0/client/main.py`）；

## 十二、生态与集成

- **客户端 SDK**：Python（主力）、TypeScript（`mem0-ts/`）、REST API；
- **集成矩阵**：
  - **LLM 提供商**：上文 § 十一节 15+；
  - **向量库**：上文 § 十节 20+；
  - **图存储**：Memgraph、Neo4j、NetworkX、Kuzu；
  - **框架集成**：
    - **LangGraph**（官方 example）；
    - **CrewAI**（官方 example）；
    - **Autogen**（官方 example）；
    - **LlamaIndex**（官方 example）；
    - **Haystack**（社区）；
    - **Phidata**（官方 example）；
  - **前端**：
    - **Next.js + Vercel AI SDK**；
    - **Streamlit**；
    - **Gradio**；
- **Mem0 Cloud**：闭源 SaaS，提供 Memory API、Dashboard、Webhook、分析；
- **Mem0 Enterprise**：本地 / 私有部署（VPC / On-Premises）；
- **Webhooks**：Mem0 → 外部系统（消息 / 邮件 / Slack）；
- **Dashboard**：Memory 可视化、监控、分析、计费；

## 十三、源码学习路径

- **必读模块**（按学习顺序）：
  1. `mem0/memory/main.py`：Memory 主类（add / search / get_all / delete）；
  2. `mem0/memory/utils.py`：记忆处理工具（prompt、LLM 调用）；
  3. `mem0/configs/base.py`：配置基底；
  4. `mem0/configs/llms/base.py`：LLM 抽象；
  5. `mem0/configs/llms/openai.py`：OpenAI 实现；
  6. `mem0/configs/embeddings/base.py`：Embedder 抽象；
  7. `mem0/configs/vector_stores/base.py`：Vector Store 抽象；
  8. `mem0/configs/vector_stores/qdrant.py`：Qdrant 实现（默认）；
  9. `mem0/configs/rerankers/base.py`：Reranker 抽象；
  10. `mem0/configs/prompts.py`：Prompt 模板；
  11. `mem0/memory/graph_memory.py`：Graph Memory；
  12. `mem0/client/main.py`：FastAPI server；
  13. `mem0/proxy/`：存储代理；
- **LOC 与语言**：
  - `mem0/` ~10k LoC；
  - `mem0-ts/` ~5k LoC；
  - 主语言：Python（核心）、TypeScript（前端）；
- **构建系统**：Hatchling / Poetry；pytest + mypy + black + ruff；

## 十四、学习路径

- **入门（1 周）**：
  - 阅读官方 Quickstart（`mem0/README.md`）；
  - 跑通 `Memory.add(messages)` + `Memory.search(query)`；
  - 了解 Memory Modes（OSS / Cloud / Enterprise）；
  - 了解多用户隔离（user_id / agent_id / run_id）；
- **进阶（2-3 周）**：
  - 深入学习 Vector Store 后端（Qdrant / pgvector / Pinecone）；
  - 深入学习 LLM 后端（OpenAI / Anthropic / Ollama）；
  - 学习 Graph Memory（Memgraph / Neo4j）；
  - 学习 Rerank（Cohere / BGE）；
  - 与 LangGraph / CrewAI / Autogen 集成；
- **高级（1-2 月）**：
  - 阅读源码（Memory 主类 + Vector Store + LLM Factory）；
  - 自定义 Vector Store / LLM / Embedder / Reranker；
  - Mem0 Cloud 集成（生产部署）；
  - 自建 Memory 评估体系；
  - Multi-Memory 模式（Vector + Graph + RDBMS）；
- **推荐资源**：
  - 官方文档：https://docs.mem0.ai/
  - GitHub：https://github.com/mem0ai/mem0
  - 论文：Mem0（Chugh et al., 2024, arxiv.org/abs/2504.19413）、MemoryBank（Zhong et al. 2024, arxiv.org/abs/2305.10250）、A-Mem（Wang et al. 2025, arxiv.org/abs/2502.12110）
  - 博客：https://mem0.ai/blog
  - Mem0 Cloud：https://app.mem0.ai/

## 十五、风险与陷阱

- **项目演进不稳定**：embedchain → mem0 重大重构，外部教程 / 博客过时；建议参考官方最新文档；
- **API 频繁变更**：mem0 2.x 仍在快速迭代，v0.1 → v2.x 部分 API 变更；
- **多租户数据隔离**：默认 `user_id` / `agent_id` / `run_id` 隔离，生产环境需自行实现 row-level security；
- **LLM 调用频率**：每次 `Memory.add` 都调用 LLM（抽取事实 + 决策 ADD/UPDATE/DELETE），生产高并发下成本高；
- **Vector Store 性能**：默认 Qdrant 单节点，> 1M 记忆需升级 Qdrant Cluster / Milvus；
- **Graph Memory 性能**：Memgraph / Neo4j 是核心依赖，部署复杂；
- **记忆冲突解决不完善**：LLM 决策 ADD/UPDATE/DELETE 不可靠，可能产生不一致；
- **Mem0 Cloud 锁定**：使用 Cloud API 后切换到 OSS 困难（数据导出 / 迁移）；

## 十六、参考资料

- 官方仓库：https://github.com/mem0ai/mem0
- embedchain 历史仓库：https://github.com/embedchain/embedchain（archived）
- 官方文档：https://docs.mem0.ai/
- Mem0 Cloud：https://app.mem0.ai/
- 论文：Mem0（Chugh et al., 2024, arxiv.org/abs/2504.19413）、MemoryBank（Zhong et al. 2024, arxiv.org/abs/2305.10250）、A-Mem（Wang et al. 2025, arxiv.org/abs/2502.12110）
- 博客：https://mem0.ai/blog
- 集成示例：https://docs.mem0.ai/integrations

## 十七、与本项目关联

mem0（embedchain 演进版）作为「AI 长期记忆层 + 多 Memory 类型 + Graph Memory + Mem0 Cloud」框架，与本项目（engineering/ 轨道 C/C++ 自研存储与向量索引）在「多 Memory 抽象 + 向量/图后端 + LLM 抽取与冲突解决」三个维度有直接参考价值：

- **多类型 Memory 抽象**：mem0 通过 `UserMemory` / `AgentMemory` / `SessionMemory` / `GraphMemory`（`mem0/memory/`）实现记忆分层（用户级 / Agent 级 / 会话级 / 图记忆），与本项目多模态存储抽象（KV / Vector / Timeseries / Document / Spatial / Graph / Yang 七种数据模型，参见 `engineering/include/db/mm_storage.h` 与 `engineering/src/db/storage/mm/mm_storage.c`）在「多数据模型统一接口」设计理念上高度一致；可类比 `MemoryType.USER_MEMORY` 与 `STORAGE_TYPE_VECTOR` 的抽象层次。
- **向量后端 + Graph 后端双重路线**：mem0 同时支持向量检索（Qdrant / Chroma / PGVector / Pinecone / Weaviate / Milvus / LanceDB）与图检索（Memgraph / Neo4j / NetworkX / Kuzu），形成「向量召回 + 图关联」的 Web 双星架构。本项目则在 `engineering/src/db/index/vector_index/` 自研 HNSW + PQ/SQ 量化实现（`hnsw_pq.c`、`hnsw_sq.c`），并在 `engineering/src/db/storage/graph/` 提供图存储能力，可对照 mem0 的「向量 + 图」复合检索设计。
- **LLM 抽取与冲突解决**：mem0 每次 `Memory.add` 调用 LLM 做事实抽取 + ADD/UPDATE/DELETE 决策；本项目则在 `engineering/src/algo-prod/quantization/`（PQ / LvQ / `adaptive_quantization.c`）与 `engineering/src/algo-prod/dict/`（结巴分词 `dict_cut.c` / `dict_hmm.c`）自研底层组件，可对照 mem0 的「LLM-driven memory update」思路，在「规则驱动 + 模型驱动」权衡上提供参考。
- **Vector DB 选型决策**：mem0 支持的向量库（Qdrant / Chroma / PGVector / Pinecone / Weaviate / Milvus / LanceDB / Azure）在 `docs/db-wiki/vector/` 已有工程视角梳理（`docs/db-wiki/vector/chroma.md`、`docs/db-wiki/vector/qdrant.md`、`docs/db-wiki/vector/milvus.md`、`docs/db-wiki/vector/lancedb.md`），可作为 mem0 Vector Store 后端选型的对照资料。
- **GUC 配置 + Mem0 cloud**：mem0 通过环境变量 + Cloud API 管理用户 / Agent 配置；本项目实现 PostgreSQL 风格的 GUC 配置系统（`engineering/src/db/core/guc.c`、`engineering/include/db/core/guc.h`），设计理念类似但更底层。
- **多模态记忆与时序**：mem0 通过 TimescaleDB（Timeseries Memory）支持时序记忆；本项目则在 `engineering/src/db/storage/ts/` 提供原生时序存储能力，与 mem0 选型思路一致，但完全自研底层实现。
- **学习建议**：阅读 mem0 源码重点关注「多 Memory 类型 API 设计」「LLM-driven memory update 算法」，对照本项目 `engineering/include/db/mm_storage.h` 的统一存储接口与 `engineering/src/db/storage/` 的模块化实现，加深对「数据模型抽象 + 长期记忆」系统设计的理解。