# Weaviate Wiki（Go 实现的模块化向量数据库）

> 参考版本：Weaviate 1.x（Go + HNSW + 模块化向量器生态），主打 **GraphQL API + 向量化模块 + 倒排 + 向量混合检索 + 多模态** 的语义搜索引擎。本 wiki 重点覆盖向量类型特有章节（HNSW + 倒排融合、SPFresh、模块化向量器）。

## 一、项目定位

Weaviate 是由 **SeMI Technologies**（荷兰创业公司，后改名 Weaviate 公司）开源的 **基于 Go 的模块化向量搜索引擎**，提供 **GraphQL/REST API、内置向量化模块（OpenAI / Cohere / HuggingFace 等）、倒排 + 向量混合检索、多模态（图/文/音）、分布式部署**，是当前生产环境最常用的开源语义搜索引擎之一。

- **痛点**：
  - 传统搜索（Elasticsearch）缺乏原生向量检索 + 自动向量化；
  - 向量库（Milvus / Qdrant）缺乏 GraphQL + 自然语言查询的语义层；
  - RAG 应用需要 **数据导入即向量化** 的零配置体验；
  - 多模态数据（图/文/音频）需要统一 schema 与混合检索；
  - 模块化需求：可插入不同的向量化器、reranker、生成器；
- **典型用户**：RAG 应用、企业语义搜索、AI Agent、知识图谱、多模态检索、客服 AI、电商搜索；
- **核心口号**：**"The AI-native database"** + **"Vector + GraphQL + Modules"** + **"Anything search, anything generation"**。

## 二、发展历史与社区

- **起源**：2018 年 Bob van Luijt、Etienne Dilocker 等在荷兰阿姆斯特丹创建，初为知识图谱增强的向量搜索；
- **里程碑**：
  - 2019-03：v0.1 开源，GraphQL + 向量索引；
  - 2019-09：v0.10 引入 **Modules**（OpenAI / GPT-3 / Cohere）；
  - 2020-06：v0.21 引入 **Generative Search**（RAG 风格）；
  - 2021-06：v1.0 GA，生产可用；
  - 2021-12：v1.10 引入 **Cross-references** + **Multi-tenancy**；
  - 2022-06：v1.14 引入 **Hybrid Search（BM25 + 向量）**；
  - 2023-04：v1.19 引入 **Named Vectors**（一个对象存多种向量）；
  - 2023-10：v1.22 引入 **Vector Compression（PQ）**；
  - 2024-04：v1.24 引入 **SPFresh**（vdb-bench 评估）；
  - 2024-09：v1.26 引入 **Replication** GA、**MMap** 完善；
  - 2025：v1.27+ 持续优化 **HNSW + SPFresh + RAG + 多模态**；
- **维护状态**：极活跃，Weaviate 公司 80+ 工程师，月度版本；
- **社区**：GitHub 12k+ star、Slack 7k+ 用户、Forum、Reddit、年度 Weaviate 大会。

## 三、许可与商业限制

- **核心代码**：**BSD-3-Clause License**（极宽松）；
- **商业版本**：
  - **Weaviate Cloud（WCD）**：全托管（AWS / GCP）；
  - **Weaviate Enterprise**：企业版（SLA、RBAC、Audit、Backup）；
- **生态关系**：
  - 与 **OpenAI / Cohere / Anthropic / HuggingFace / Voyage AI** 通过 **Modules** 集成；
  - 与 **LangChain / LlamaIndex / Haystack / DSPy** 默认后端；
  - 与 **AWS / GCP** 深度合作（Marketplace）；
  - 与 **Hugging Face** 集成 `text2vec-huggingface` 模块。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Salesforce**：CRM 语义搜索；
  - **Red Hat**：技术文档 RAG；
  - **Stack Overflow**：Stack Overflow for Teams 语义搜索；
  - **Morningstar**：金融研究；
  - 国内：阿里、字节、网易、小红书等试点；
- **行业**：RAG 应用、企业知识库、AI 客服、电商搜索、金融研究、医疗文档、代码搜索；
- **典型场景**：客户支持 RAG、文档问答、电商语义搜索、多模态检索、AI Agent 记忆、生物医学文献。

## 五、一句话对比定位

> Weaviate 是 **"Go + GraphQL/REST + HNSW + BM25 + 模块化向量器（Modules）+ 生成式搜索（Generative Search）+ Named Vectors + 多模态"** 的语义搜索引擎；与 **Milvus（Go + C++ + 分布式强）**、**Qdrant（Rust 云原生）**、**Chroma（Python 嵌入式）**、**LanceDB（嵌入式列存）**、**Vespa（Java/C++ 搜索平台）**、**Elasticsearch（搜索 + 向量）** 构成向量/语义搜索阵营。Weaviate 的差异化是 **"GraphQL API + 模块化向量生态 + Generative Search + 多模态开箱即用"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层                                     │
│  - Python: weaviate-python-client（官方）                            │
│  - JS / TS: weaviate-ts-client                                      │
│  - Java / Go：官方 SDK                                              │
│  - GraphQL 客户端（任何支持 GraphQL 的工具）                       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ GraphQL / REST / gRPC
┌─────────────────────▼───────────────────────────────────────────────┐
│                    API 接入层（GraphQL + REST）                        │
│  adapters/handlers/                                                  │
│  ├─ GraphQL schema（基于 gqlgen）                                  │
│  ├─ REST API（Get / Search / Batch）                               │
│  ├─ gRPC（v1.23+，批量高性能）                                     │
│  └─ Auth（API Key / OIDC / RBAC）                                  │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Usecase 层（业务用例）                               │
│  usecases/                                                           │
│  ├─ vectorizer（向量化调度）                                        │
│  ├─ modules（模块注册表）                                           │
│  ├─ traverser（图遍历）                                             │
│  ├─ classification（自动分类）                                     │
│  ├─ backup（备份恢复）                                              │
│  └─ schema（集合管理）                                              │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Repo 层（数据访问）                                  │
│  adapters/repos/db/                                                  │
│  ├─ repo（接口）                                                    │
│  ├─ IndexQueue（异步索引队列）                                      │
│  ├─ Compaction（段合并）                                            │
│  ├─ Batch（批量写入）                                              │
│  └─ Shards（分片管理）                                              │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐┐
│                  Vector Index 层                                     ││
│  adapters/repos/db/vector/                                          ││
│  ├─ hnsw（HNSW 实现）                                              ││
│  ├─ flat（Flat 暴力）                                               ││
│  ├─ compressionhelpers（PQ / BQ 辅助）                            ││
│  ├─ dynamic（动态索引：HNSW + Flat 自动切换）                      ││
│  ├─ hfresh（v1.24+ SPFresh 实现）                                  ││
│  ├─ multivector（ColBERT 多向量）                                  ││
│  ├─ geo（地理索引）                                                 ││
│  └─ cache（向量缓存）                                               ││
└─────────────────────┬───────────────────────────────────────────────┘┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  倒排 + 标量索引                                     │
│  adapters/repos/db/inverted/                                          │
│  ├─ BM25（倒排 + BM25 打分）                                       │
│  ├─ property length + BM25f block                                  │
│  ├─ Filters（标量过滤）                                            │
│  └─ Counters（聚合统计）                                           │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Object 存储                                         │
│  - BoltDB（本地嵌入式 KV）                                          │
│  - LSM（基于自定义实现）                                           │
│  - 文件系统（mmap）                                                │
│  - S3 / GCS / Azure（备份）                                        │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **模块化（Modules）**：所有外部集成（向量化、生成、Rerank）通过模块插件实现；
- **GraphQL API**：Schema-driven，开发者友好；
- **倒排 + 向量深度融合**：BM25 和 HNSW 在同一查询路径中通过 RRF / Weighted 融合；
- **多向量（Named Vectors）**：一个对象可存多向量（不同嵌入模型）；
- **HNSW 优化**：自研 HNSW 实现，针对 Filtered Search 优化；
- **SPFresh**（v1.24+）：Microsoft 提出的 incremental vector index；
- **Generative Search**：内置 RAG，结合向量化 + 生成模型；
- **BoltDB + mmap**：简化存储栈，单二进制部署。

## 七、存储引擎

### 7.1 Object Store（对象存储）

- **BoltDB**：基于 mmap 的 KV 存储（`github.com/boltdb/bolt` 的 fork / 改进版）；
- 每个 Shard 一个 BoltDB 文件，存对象 + 向量 + Payload；
- **LSM 替代**（v1.20+）：部分场景用 LSM 替代 BoltDB。

### 7.2 索引文件

- **HNSW 图**：单独文件 mmap；
- **BM25 倒排**：单独文件；
- **PQ 码本**：单独文件；
- **SPFresh**：v1.24+ 的 incremental index。

### 7.3 元数据

- **Schema**：集合 schema 存 BoltDB；
- **多租户**：单实例多 Database；
- **Sharding**：基于 Tenant 路由。

## 八、索引机制（**向量类型核心**）

### 8.1 向量索引算法全景

| 索引 | 文件 | 特点 |
|------|------|------|
| **HNSW** | `adapters/repos/db/vector/hnsw/` | 默认 |
| **Flat** | `adapters/repos/db/vector/flat/` | 暴力 |
| **Dynamic** | `adapters/repos/db/vector/dynamic/` | HNSW + Flat 自动切换 |
| **SPFresh** | `adapters/repos/db/vector/hfresh/` | v1.24+ 增量索引 |
| **Multi-Vector** | `adapters/repos/db/vector/multivector/` | ColBERT |

### 8.2 HNSW 详解（Weaviate 实现）

Weaviate 有自己的 HNSW 实现（非用第三方），位于 `adapters/repos/db/vector/hnsw/`：

```
hnsw/
├── commitlog/           # 增量写入日志
├── compact/             # 段合并
├── compression_tests/
├── datasets/
├── distancer/           # 距离计算
├── visited/             # 访问追踪（用于过滤）
├── config.go            # 配置
├── hnsw.go              # 主入口
├── insert.go            # 插入
├── search.go            # 检索
├── commit_logger.go     # 提交日志
├── cache.go             # 缓存
└── ...
```

**参数**：
- `maxConnections`：每节点连接数（默认 64）；
- `efConstruction`：构建候选数（默认 128）；
- `ef`：搜索候选数（默认 100）；
- `vectorCacheMaxObjects`：向量缓存；
- `pq`: PQ 量化压缩（v1.22+）。

**Weaviate HNSW 特点**：
- **Filtered Search**：HNSW 图遍历过程中维护"过滤集合"，跳过被过滤节点（参考 ACORN 思路）；
- **CommitLog**：写入先入 CommitLog，后异步合并到 HNSW 图；
- **Dynamic Index**：小数据用 Flat，大数据用 HNSW，自动切换；
- **PQ Compression**：v1.22+ 支持 PQ 量化，内存降低 4-16x；
- **Async Indexing**：写入与索引构建异步进行。

### 8.3 倒排 + 向量混合检索（Weaviate 杀手锏）

Weaviate 的核心是 **BM25 倒排 + HNSW 向量 + 标量过滤** 的深度融合：

```
查询流程（Hybrid Search）：
1. 解析查询（GraphQL / Weaviate Query Language）
2. 并行执行两条路径：
   a. BM25 倒排（按 term 召回）
   b. HNSW 向量检索（按相似度召回）
3. 标量过滤（filter）在两条路径上都生效
4. 融合两条结果：
   - RRF（Reciprocal Rank Fusion）：默认
   - Weighted：自定义权重
5. 返回 Top-K
```

**实现位置**：
- 倒排：`adapters/repos/db/inverted/`；
- HNSW：`adapters/repos/db/vector/hnsw/`；
- 融合：`usecases/traverser/hybrid_search.go`。

**API 示例**：

```python
client.query.get("Article", ["title", "content"]).with_hybrid(
    query="What is the capital of France?",
    vector=[0.1, 0.2, ...],   # 可选，自动向量化
    alpha=0.5,                # 0=纯向量，1=纯 BM25
).with_limit(10).do()
```

### 8.4 PQ（Product Quantization）压缩

Weaviate v1.22+ 引入 PQ：

- **压缩比**：1/8 ~ 1/32；
- **训练**：从样本数据训练码本；
- **存储**：HNSW 图节点存储 PQ 码而非原始向量；
- **查询**：粗检索 PQ + 精排（可选原始向量 re-rank）；
- **实现**：`adapters/repos/db/vector/compressionhelpers/`。

### 8.5 SPFresh（v1.24+）

**SPFresh** 是 Microsoft 2023 提出的 **incremental vector index**：

- **思路**：与 SPANN 类似，空间分区 + posting list；
- **优势**：支持在线更新（vs HNSW 重建代价大）；
- **代码**：`adapters/repos/db/vector/hfresh/`；
- **论文**：Sun et al., "SPFresh: Incremental In-Place Update for Billion-Scale Approximate Nearest Neighbor Search", 2023。

### 8.6 Named Vectors（多向量）

- **场景**：一个对象存多向量（如 OpenAI + Cohere 两种嵌入）；
- **API**：
  ```python
  collection = client.collections.create(
      name="Article",
      vectorizer_config=[
          Configure.NamedVectors.text2vec_openai(name="title_vec"),
          Configure.NamedVectors.text2vec_cohere(name="content_vec"),
      ],
  )
  ```
- **查询**：`hybrid(search="...", targets=["title_vec", "content_vec"])`。

### 8.7 模块化向量器（Modules）

**内置模块**（`usecases/modules/`）：

| 模块 | 用途 |
|------|------|
| `text2vec-openai` | OpenAI Embedding |
| `text2vec-cohere` | Cohere Embedding |
| `text2vec-huggingface` | HuggingFace 模型 |
| `text2vec-ollama` | Ollama 本地模型 |
| `text2vec-voyageai` | Voyage AI |
| `generative-openai` | OpenAI GPT 生成（RAG）|
| `generative-cohere` | Cohere 生成 |
| `reranker-cohere` | Cohere Rerank |
| `qna-openai` | OpenAI Q&A |
| `img2vec-neural` | 图片 Embedding（ResNet50）|
| `multi2vec-clip` | CLIP 多模态 |
| `sum-transformers` | 摘要 |

### 8.8 Generative Search（RAG 内置）

```python
response = collection.generate.hybrid(
    query="What is the capital of France?",
    limit=10,
    grouped_task="Summarize these documents in 1 sentence.",
)
```

实现：`usecases/modules/generative-*`。

## 九、查询/计算/接口层

### 9.1 GraphQL API

- **Schema-driven**：集合定义即 GraphQL schema；
- **端点**：`POST /v1/graphql`；
- **生成工具**：`github.com/99designs/gqlgen`；
- **能力**：`Get`, `Explore`, `Aggregate`, `Hybrid`, `NearObject`, `NearText`, `NearVector`, `Ask`（RAG）。

### 9.2 REST API

```
GET    /v1/schema
POST   /v1/objects
POST   /v1/batch/objects
POST   /v1/objects/{id}/references
POST   /v1/graphql
DELETE /v1/objects/{id}
```

### 9.3 gRPC API（v1.23+）

- 高性能场景；
- `Batch`, `Search`, `BatchObjects` 等。

### 9.4 SDK 生态

| 语言 | 包名 | 备注 |
|------|------|------|
| Python | `weaviate-python-client` | 官方 |
| TypeScript | `weaviate-ts-client` | 官方 |
| Java | `weaviate-java-client` | 官方 |
| Go | `weaviate-go-client` | 官方 |
| C# | `weaviate-csharp-client` | 官方 |

### 9.5 多租户

- **多租户**：单实例多 Database；
- **Tenant isolation**：每个 Tenant 独立索引 + 资源隔离；
- **RBAC**：Database / Collection 级别（企业版）。

### 9.6 混合检索 + Rerank

- **RRF**：默认融合；
- **Weighted**：用户自定义向量 / BM25 权重；
- **Rerank**：通过 `reranker-cohere` 模块集成。

## 十、事务与一致性

- **写入**：BoltDB 事务 + CommitLog；
- **读一致性**：默认 **最终一致**；
- **Schema 修改**：可能触发后台重建索引；
- **多副本**：v1.26 GA，Raft 共识；
- **崩溃恢复**：从 CommitLog 重放。

## 十一、复制与高可用

- **v1.26+ Replication GA**：
  - 基于 Raft；
  - Shard 副本自动同步；
  - Leader / Follower 模式；
- **Cloud**：跨区域 HA；
- **Backup**：S3 / GCS 备份恢复。

## 十二、扩展性

- **垂直扩展**：增加节点内存；
- **水平扩展**：
  - **Sharding**：基于 Tenant 自动分片；
  - **Replication**：副本数可配；
- **冷热分层**：PQ 量化 + Mmap 索引；
- **Dynamic Index**：大数据自动 HNSW，小数据 Flat。

## 十三、性能与基准

- **ann-benchmarks**：Weaviate HNSW 在 SIFT-1M 上排名中上；
- **HNSW 延迟**：百万级向量约 **1-10ms / query**；
- **PQ 收益**：内存降低 4-16x，召回率损失 1-5%；
- **Hybrid Search**：相比纯向量，BM25 融合可提升 recall 5-15%；
- **Generative Search**：取决于外部 LLM 延迟。

## 十四、运维与可观测性

- **Prometheus**：内置 `/metrics`；
- **OpenTelemetry**：tracing 支持；
- **结构化日志**（logrus）；
- **慢查询日志**：查询时间 > 阈值；
- **Backup**：内置 backup 文件 + S3 集成；
- **Operator**：Weaviate Operator（K8s）；
- **可视化**：Weaviate Cloud Console、weaviate-cli。

## 十五、生态与集成

- **AI 框架**：LangChain、LlamaIndex、Haystack、DSPy；
- **向量器模块**：OpenAI、Cohere、HuggingFace、Voyage AI、Ollama、Jina；
- **生成模块**：OpenAI、Cohere、Anthropic（社区）；
- **Rerank**：Cohere、Voyage AI；
- **可视化**：Weaviate Console、weaviate-cli；
- **部署**：Docker Compose、Helm、Operator；
- **MCP**：Model Context Protocol 集成。

## 十六、核心论文与算法原理

- **HNSW**：Malkov & Yashunin, 2016；
- **BM25**：经典 TF-IDF 排序；
- **PQ**：Jégou et al., 2011；
- **SPFresh**：Sun et al., 2023；
- **SPANN**：Chen et al., 2021；
- **ACORN**（filtered HNSW）：2024；
- **CLIP**：Radford et al., 2021；
- **ColBERT**：Khattab & Zaharia, 2020；
- **RRF**：Cormack et al., 2009；
- **RAFT**：Ongaro & Ousterhout, 2014。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | GraphQL | `adapters/handlers/graphql/` | API 入口 |
| P0 | HNSW | `adapters/repos/db/vector/hnsw/hnsw.go` | 图索引核心 |
| P0 | 倒排 | `adapters/repos/db/inverted/` | BM25 |
| P1 | Hybrid Search | `usecases/traverser/hybrid_search.go` | 融合 |
| P1 | PQ 压缩 | `adapters/repos/db/vector/compressionhelpers/` | 量化 |
| P1 | Modules | `usecases/modules/` | 模块化 |
| P1 | SPFresh | `adapters/repos/db/vector/hfresh/` | 增量索引 |
| P2 | Repo | `adapters/repos/db/` | 仓库层 |
| P2 | Replication | `cluster/replication/` | 分布式 |
| P2 | Usecase | `usecases/` | 业务用例 |

## 十八、源码规模与开发语言

- **Go**：约 95% LOC（核心）；
- **Python**：约 3% LOC（client SDK 测试）；
- **TypeScript / JS**：约 1% LOC（Web Console）；
- **Proto**：约 1% LOC（gRPC）；
- **构建系统**：Go modules；
- **CI**：GitHub Actions；
- **LOC 总量**：约 60 万行（含测试）。

## 十九、学习资源

- **官方文档**：https://weaviate.io/developers/weaviate
- **GitHub**：https://github.com/weaviate/weaviate
- **博客**：https://weaviate.io/blog
- **Academy**：https://weaviate.io/academy
- **Slack / Forum**：https://forum.weaviate.io/
- **YouTube**：Weaviate 频道；
- **书籍**：《Weaviate 实战》（社区）。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 / ARM64 | 原生 |
| macOS x86_64 / ARM64 | 原生 |
| Windows x86_64 | 原生 |
| Docker | 官方镜像 |
| Helm / K8s | 官方 Chart + Operator |
| Weaviate Cloud | 全托管 |

## 二十一、变体与衍生项目

- **Weaviate Cloud（WCD）**：官方全托管；
- **Weaviate Enterprise**：商业版；
- **Embedded Weaviate**：Python 嵌入式模式（实验）；
- **Weaviate MCP**：Model Context Protocol 集成；
- **第三方模块**：社区贡献的向量化器、生成器。

## 二十二、风险与陷阱

1. **模块依赖外部 API**：默认 Modules 依赖 OpenAI / Cohere 等，**必须配置 API Key**；
2. **GraphQL 学习曲线**：对不熟悉 GraphQL 的开发者有学习成本；
3. **HNSW 内存**：默认每节点连接数 64（高于其他库），内存占用约 1.5-2x；
4. **多向量索引膨胀**：Named Vectors 同时建多套索引，内存/磁盘占用倍增；
5. **BoltDB 限制**：单 Shard 大小有上限，超大集合需要分片；
6. **Generative Search 成本**：每次查询都调用外部 LLM，**注意成本与延迟**；
7. **SPFresh 实验性**：v1.24 引入，生产建议先用 HNSW；
8. **多租户隔离**：租户数量过多会影响性能；
9. **Replication 配置**：Raft 配置错误导致脑裂；
10. **Backup 恢复慢**：大集合 backup 恢复可能数小时。

## 二十三、与本项目的关联

- **`engineering/rag/`** 中 RAG 系统可对接 Weaviate 作为向量后端；
- **`engineering/src/index/`** 已实现 HNSW，可参考 Weaviate 的倒排 + 向量融合设计；
- **`learning/notes/`** 可沉淀 Weaviate 模块化设计经验；
- **学习价值**：GraphQL + 模块化的设计思路；
- **MiniVecDB**：参考 Weaviate 的 HNSW + 倒排双层架构简化实现。

## 二十四、参考资料

- [Weaviate GitHub](https://github.com/weaviate/weaviate)
- [Weaviate 官方文档](https://weaviate.io/developers/weaviate)
- [Weaviate Cloud](https://console.weaviate.cloud/)
- [Weaviate Academy](https://weaviate.io/academy)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [SPFresh 论文](https://arxiv.org/abs/2311.18879)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [BM25 文献](https://en.wikipedia.org/wiki/Okapi_BM25)
- [RRF 论文](https://dl.acm.org/doi/10.1145/1645953.1646036)
- [CLIP 论文](https://arxiv.org/abs/2103.00020)
- [ColBERT 论文](https://arxiv.org/abs/2004.12832)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)