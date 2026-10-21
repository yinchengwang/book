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

## 二十四、LSM 存储引擎：Memtable 刷盘、层级段合并与 Bloom 过滤

Weaviate 没有外接 RocksDB / LMDB，而是自研了一套 LSM 存储引擎 `lsmkv`（`adapters/repos/db/lsmkv/`），同时承担对象存储、倒排索引与原始向量的持久化。本节按「写入 → 刷盘 → 合并 → 查找」四步拆解。

### 24.1 六种段策略

`adapters/repos/db/lsmkv/strategies.go:24` 定义了六种段策略，它们同时决定了两件事：段内 key 的比较语义，以及 compaction 时同 key 多值如何归约。

| 策略 | 常量 | 语义 | 典型用途 |
|------|------|------|---------|
| `replace` | `StrategyReplace` | 幂等 PUT，后写覆盖前写 | 对象存储（docID → JSON blob） |
| `setcollection` | `StrategySetCollection` | key → 值列表 | 倒排表的辅助数据 |
| `mapcollection` | `StrategyMapCollection` | key → map 的值列表 | BM25 倒排（term → docID + 频次） |
| `roaringset` | `StrategyRoaringSet` | key → RoaringBitmap | 过滤与聚合的计数 |
| `roaringsetrange` | `StrategyRoaringSetRange` | key → 带区间的 RoaringBitmap | 数值范围过滤 |
| `inverted` | `StrategyInverted` | 倒排变体，property length 一并落段 | `searchable` 倒排桶 |

`SegmentStrategyFromString`（`adapters/repos/db/lsmkv/strategies.go:41`）负责字符串到 `segmentindex.Strategy` 的映射，遇到未知策略直接 `panic` —— 因为策略与磁盘格式强绑定，静默降级会导致读错段。

### 24.2 Memtable 与 WAL：三阈值触发的 flush-and-switch

写入路径是「先落 Memtable 的 WAL（commit log），Memtable 满后整体刷成不可变的 segment」。触发条件是三个「或」（`adapters/repos/db/lsmkv/bucket.go:1977`）：

```
memtableTooLarge := b.active.Size()          >= b.memtableThreshold  // 默认 10 MiB
walTooLarge      := uint64(commitLogSize)   >= b.walThreshold
dirtyTooLong     := b.active.DirtyDuration() >= b.flushDirtyAfter
shouldSwitch := memtableTooLarge || walTooLarge || dirtyTooLong
```

默认阈值 10 MiB 定义在 `adapters/repos/db/lsmkv/bucket.go:339`。第三条 `dirtyTooLong` 容易被忽略但很重要：低 QPS 的冷分片即使远未写满，也会按时间刷盘，避免段长期滞留内存且无法被 compaction 回收。

刷盘严格分四步（`adapters/repos/db/lsmkv/memtable_flush.go:52`）：

```
1. commitlog.close()     // 关 WAL 缓冲 + fsync；失败即中止，绝不删 WAL
2. Size()==0 ?          // 空 memtable：删掉空 WAL 直接返回，否则下次启动会误重放
3. 写 <name>.l0.s<strategy>.db.tmp   // 新段恒为 level 0
4. fsync + 原子 rename   // .tmp → .db；成功后才删除对应 .wal
```

WAL 的记录格式是定长头 + 变长体（`adapters/repos/db/lsmkv/commitlogger.go:206`）：

```
| version == 1 (1byte)        |
| type (1byte)                |  // replace / collection / roaringset / roaringsetlist
| node length (4bytes)        |
| node (dynamic length)       |
| checksum (crc32 4bytes)     |
```

当前版本号 `CurrentCommitLogVersion = 1`（`adapters/repos/db/lsmkv/commitlogger.go:214`），保留 version 0 的旧格式仅为兼容。启动恢复见 `adapters/repos/db/lsmkv/bucket_recover_from_wal.go:34`：扫目录里的 `.wal`，0 字节的直接删，非空的按序重放回 memtable。该函数的注释明确 ctx 只在开头检查一次 —— 恢复一旦开始就让它跑完，崩溃循环里靠「每次启动多恢复一个 bucket」最终收敛。

Memtable 切换用双指针（`atomicallySwitchMemtable`，`adapters/repos/db/lsmkv/bucket.go:2290`）：先原子换出 `b.active` 到 `b.flushing`，新写全部落到新 active，旧 memtable 的写入者自然排空（`waitForZeroWriters`），再刷盘。这样读侧任何时刻都能看到至少一个完整 memtable。

### 24.3 层级合并：findCompactionCandidates

Weaviate 用的是**分层（tiered / level-based）**合并，而非经典 LSM 的对数分层。源码注释直接用了 tiered 一词来对比（`adapters/repos/db/lsmkv/bucket_options.go:273`）。每个 segment 带一个 level，并写进文件名：`segmentExtraInfo` 生成 `.l%d.s%d`（`adapters/repos/db/lsmkv/segment_group_compaction.go:270`），形如 `...l2.sinverted.db`。

候选选取伪代码（对应 `adapters/repos/db/lsmkv/segment_group_compaction.go:72`）：

```
findCompactionCandidates():
    if shard is READONLY: return nil            # 上游 shard 只读时不合并
    for i := len(segments)-2 down to 0:         # 新的段优先（从右往左扫）
        l, r := segments[i], segments[i+1]
        if l.level < r.level: isUnordered = true; break
        if l.level == r.level and fitsSizeLimit(l, r):
            matching = (i, i+1); newLevel = l.level + 1
            if 存在同 level 的更旧段: newLevel = l.level   # 不制造比旧段更高的层
            break
        if l.level > r.level and compactLeftOverSegments and similarSizes(l, r):
            leftover = (i, i+1)
    return matching, else leftover, else 修复乱序段对
```

两条硬约束：只合并**相邻**段（以保留写入时序，让 newer 覆盖 older、删除得以生效）；新段的 level 不得高于旧段。

源码给的例子：s1(4) s2(3) s3(3) s4(2) s5(2) s6(2) s7(1) s8(0) → 先合 s4+s5 得 (3)，再 s2+s3 得 (4)，最后 s1+(s2s3) 得 (5)。

**乱序修复**是这段代码最难的部分。并发 compaction 会让段数组偶尔不按时间戳排列，某些段因此永远选不上。`findCompactionCandidates` 末尾有一段专门分支：一旦发现 `l.level < r.level`（左段 level 比右段低，违反「旧段 level ≥ 新段 level」的不变式），就把这对乱序段强行合并以恢复降序。

compaction 循环 `compactOrCleanup`（`adapters/repos/db/lsmkv/segment_group.go:1303`）里 compaction 优先于 cleanup（后者删除引用计数归零的旧段），但若距上次 cleanup 超过 12 小时、且上次 cleanup 早于上次 compaction，则强制先跑一次 cleanup，避免「只合并不清理」导致磁盘只涨不跌。

### 24.4 Bloom Filter

每个 disk segment 配一个 sidecar `.bloom` 文件（`adapters/repos/db/lsmkv/segment_bloom_filters.go:46`），从段索引的 `KeyCount` 现场构建，**误判率固定 0.001**（`adapters/repos/db/lsmkv/segment_bloom_filters.go:116`）：

```
bloom.NewWithEstimates(uint(s.index.KeyCount()), 0.001)
```

按最优参数 m = -n·ln(p)/(ln2)²、k = (m/n)·ln2 计算，n = 100 万、p = 0.001 时约 m ≈ 1.44 MB、k ≈ 10 —— 单段 1.4 MB 换取点查少一次磁盘 seek，很划算。

查表侧（`adapters/repos/db/lsmkv/segment_collection_strategy.go:30`）：

```
if s.useBloomFilter && !s.bloomFilter.Test(key) { return nil, lsmkv.NotFound }
```

判否直接短路，不进磁盘索引二分。反过来 Bloom 判「可能存在」时必须照常走索引 —— 0.1% 的误判率若不做区分处理，会被放大成大量无效 seek。

注意：Weaviate 的 Bloom **只服务点查**。BM25 的「term → docID」是批量扫描，天然是顺序读，不走 Bloom；只有「某 key 是否存在」这类点查才受益。

### 24.5 shard READONLY 时冻结 flush 与 compaction

`entities/storagestate/status.go:20` 定义了 shard 状态机：`READONLY` / `INDEXING` / `LOADING` / `READY`。切到 READONLY（如正在 drop 分片、或做 backup）时，两条路径同时被冻结：

- flush 判定直接返回 false 并打点告警（`adapters/repos/db/lsmkv/bucket.go:1989`，日志 `flush halted due to shard READONLY status`）；
- compaction 候选选取直接返回 nil（`adapters/repos/db/lsmkv/segment_group_compaction.go:77`，日志 `compaction halted due to shard READONLY status`）。

这是保证「备份 / 删除期间磁盘视图静止」的关键：否则 flush 可能在 backup 列举文件的过程中新建段，导致备份集内部不一致。

## 二十五、HNSW 提交日志压缩器：raw → sorted → snapshot

HNSW 图本身不写 mmap 二进制索引文件，而是全靠提交日志重放。这是 Weaviate 与 hnswlib / usearch 等第三方实现最大的架构差异之一 —— **自研**（`adapters/repos/db/vector/hnsw/`），不依赖外部 HNSW 库。

### 25.1 提交日志：16 种操作类型的追加流

每个向量索引一份提交日志，落在 `hnsw.commitlog.d/<unix时间戳>`，写入缓冲 32 KiB（`adapters/repos/db/vector/hnsw/commit_logger.go:35`）。

记录类型共 16 种（`adapters/repos/db/vector/hnsw/compact/types.go:18`）：

```
AddNode, SetEntryPointMaxLevel, AddLinkAtLevel, ReplaceLinksAtLevel,
AddTombstone, RemoveTombstone, ClearLinks, DeleteNode,
ResetIndex, ClearLinksAtLevel, AddLinksAtLevel,
AddPQ, AddSQ, AddMuvera, AddRQ, AddBRQ
```

图拓扑变更（AddLink / ReplaceLinks / ClearLinks）与 tombstone 混在同一条日志里，因此**删除是延迟生效的**：`AddTombstone` 只打标记，真正的摘除要等下一次 compaction。

单文件上限 `defaultCommitLogSize = 500 MB`，轮转阈值取其 1/5 = 100 MB（`adapters/repos/db/vector/hnsw/commit_logger.go:74`）。轮转逻辑 `switchCommitLogs`（`adapters/repos/db/vector/hnsw/commit_logger.go:503`）的步骤顺序值得逐行读注释，值得照抄的工程细节是：

```
1. flush + fsync 旧文件，但**不关 fd**
2. open 新文件（O_WRONLY|O_APPEND|O_CREATE）
3. 成功后才把 l.currentFile / currentWriter 指向新 fd
4. 最后才关旧 fd（关失败只告警，不影响轮转语义）
```

这样任何一步 ENOSPC / EIO 失败，都不会让 `currentFile` 指向死 fd —— 下一次 AddNode 仍写向健康 fd，轮转留到下一个维护周期重试。

### 25.2 四种文件形态

压缩器把提交日志目录看成四种文件（`adapters/repos/db/vector/hnsw/compact/file_info.go:17`）：

| 形态 | 后缀 | 含义 |
|------|------|------|
| `FileTypeRaw` | 无 | 刚轮转出来的原始追加流 |
| `FileTypeCondensed` | `.condensed` | v1 时代的 legacy 合并文件 |
| `FileTypeSorted` | `.sorted` | 已按 nodeID 排好序 |
| `FileTypeSnapshot` | `.snapshot` | 全量状态检查点 |

snapshot 是**绝对状态**（每个 nodeID 的各层连接表 + tombstone 标志），sorted 文件是**增量流**。两者在归并时按时间戳决定优先级。

### 25.3 写放大权衡：decideAction

`RunCycle`（`adapters/repos/db/vector/hnsw/compact/compactor.go:151`）每轮做六件事：清理孤儿 .tmp → 扫描目录得 `DirectoryState` → 解决重叠 → 把 raw/condensed 转成 sorted → `decideAction` → 执行所选动作。

决策函数（`adapters/repos/db/vector/hnsw/compact/compactor.go:380`）就是写放大与文件数之间的帕累托：

```
sortedRatio := sortedSize / totalSize

if snapshot == nil:
    if sortedCount > MaxFilesPerMerge: return MergeSorted    # 先归并，否则一次快照写放大不可控
    if sortedCount > 0:                return CreateSnapshot
    return None

if sortedRatio > SnapshotThreshold:                          # 默认 0.20
    if sortedCount > MaxFilesPerMerge: return MergeSorted
    return CreateSnapshot
if sortedCount > 1: return MergeSorted                       # 增量占比低，只归并降文件数
return None
```

默认值在 `adapters/repos/db/vector/hnsw/compact/compactor.go:82`：`MaxFilesPerMerge = 5`、`SnapshotThreshold = 0.20`。

直觉推导：sorted 只占总量 20% 时，做一次全量 snapshot 要重写整个图，写放大 W = totalSize / sortedSize ≥ 5 倍且不可摊薄，此时只把 5 个 sorted 归并成 1 个即可；反过来 sorted 占比已达 80% 时，5 个 sorted 归并成 1 个只能省下有限收益，而 snapshot 能把「1 个 snapshot + 不断增长的小增量」的长期成本压平。两者的交叉点就落在 0.2 附近。

这与 RocksDB 的 tiered（按层整体合并）和 leveled（每层多 SST + overlap）属于不同问题域：HNSW 提交日志只面对「单文件顺序追加 + 按 nodeID 全序可归并」这一种形态，因此只需两态决策，无需分层比率参数。

### 25.4 N 路归并与原子落盘

`createSnapshot`（`adapters/repos/db/vector/hnsw/compact/compactor.go:558`）：

```
1. 收集输入：既有 snapshot + 全部 sorted 文件
2. 按 StartTS 升序排序（时间戳即优先级）
3. 每个文件建一个 Iterator：snapshot 用 SnapshotIterator，sorted 用 WALCommitReader
4. 交给 NWayMerger 做 k 路归并
5. 输出文件名由 BuildMergedFilename(startTS, endTS, Snapshot) 决定
6. 经 SafeFileWriter 写入，Commit() = 原子 rename；defer Abort() 保证失败不留半成品
7. 删除所有源文件
```

第 7 步里的 `if f.Path == outPath { continue }`（`adapters/repos/db/vector/hnsw/compact/compactor.go:659`）是一条血泪教训，注释就写在前面：从 1.37.x 升级到 compactv2 时，某个 `.condensed` 文件可能与既有 snapshot 共享时间戳，`BuildMergedFilename(start=ts, end=ts, Snapshot)` 会产出与输入 snapshot **完全相同的路径**，`Commit()` 原子覆盖了它；若随后无脑删除源文件，就把刚写好的快照自己删掉，下次启动图就没了。

`RunCycle` 的中断语义同样写得很清楚：`shouldAbort` 只在文件之间、合并节点之间轮询，正在进行的单文件转换或合并会跑完（半途放弃一个 SafeFileWriter 的 Commit 会留下残缺输出）。由于「不删源文件直到替代物完全写好」这条不变式，中断在磁盘上零退化，下一轮从断点继续。

`NWayMerger`（`adapters/repos/db/vector/hnsw/compact/nway_merger.go:162`）把所有 iterator 压成小顶堆，堆顶是 nodeID 最小的那个；取出该 nodeID 在所有 iterator 中的全部 commit，交给 `commitMerger` 归约（后写的链接覆盖先写的，最后一条 commit 胜出），产出 `NodeCommits`。

## 二十六、动态索引切换与向量量化：threshold、ef 自适应、PQ/BQ 编码与 rescoring

### 26.1 dynamic 索引：flat → HNSW 的一次性升级

`dynamic` 索引（`adapters/repos/db/vector/dynamic/index.go:146`）不是「每次查询动态选」，而是**一次性结构升级**：初始用 flat（暴力扫描），对象数超过 `threshold` 后，把 flat 里的全部向量拷进新建的 HNSW 图，然后删掉 flat 桶。

```
dynamic.ShouldUpgrade()  (adapters/repos/db/vector/dynamic/index.go:576):
    if !status.IsUpgraded(): return true, threshold     // 尚未升级 → 问「要不要建图」
    return inner.ShouldUpgrade()

inner hnsw.ShouldUpgrade()  (adapters/repos/db/vector/hnsw/index.go:915):
    if !cachePrefilled:        return false, 0          # 缓存没预热完不升级
    if sqConfig.Enabled:       return true, sqConfig.TrainingLimit
    if rqConfig.Enabled:       return true, 1
    return pqConfig.Enabled, pqConfig.TrainingLimit
```

注意升级**之后** `ShouldUpgrade` 的语义变了：不再问「要不要建图」，而是问「要不要开始量化压缩」，返回值第二项是量化训练的样本上限。

`doUpgrade`（`adapters/repos/db/vector/dynamic/index.go:654`）的并发控制值得看：全程持 `RLock` 构造新索引，`copyToVectorIndex`（`adapters/repos/db/vector/dynamic/index.go:788`）用**短命 cursor** 逐条读 flat 桶灌进新图 —— 用短命 cursor 而非长事务，避免阻塞同一 LSM store 上的 flush。构造完成后才 `Lock()` 换指针、写 bbolt 标记位、关旧桶删目录。整个过程查询无需停机。

中途失败时 `cleanupAbortedUpgrade` 会显式删掉提交日志目录，注释说明原因：下次 `hnsw.New()` 会重放该目录下的**全部**文件，残留的半成品会污染重建出的图。

### 26.2 ef 自适应

`ef` 默认为 -1，含义是「让 Weaviate 挑」（`entities/vectorindex/hnsw/config.go:29`）。挑选逻辑（`adapters/repos/db/vector/hnsw/search.go:46`）：

```
searchTimeEF(k):
    ef := atomic.LoadInt64(&h.ef)
    if ef < 1: return autoEfFromK(k)      # -1 → 自动
    if ef < k: ef = k                     # ef 必须 ≥ k，否则结果会被提前截断
    return ef

autoEfFromK(k)  (adapters/repos/db/vector/hnsw/search.go:62):
    ef := k * efFactor                    # 默认 8
    ef = clamp(ef, efMin, efMax)          # 默认 [100, 500]
    if k > ef: ef = k
    return ef
```

默认值 `dynamicEfMin=100` / `dynamicEfMax=500` / `dynamicEfFactor=8`（`entities/vectorindex/hnsw/config.go:30`）。代入：k=10 → ef=80 → 抬到下限 100；k=100 → ef=800 → 压到上限 500。全程原子读，改配置不需要给每次搜索加锁。

### 26.3 filtered search 的 flat 短路

带过滤条件的 HNSW 查询有个退化问题：过滤器把候选集缩到很小，图遍历要绕很大一圈才凑够 ef 个合法节点，此时暴力扫描更划算。`SearchByVector`（`adapters/repos/db/vector/hnsw/search.go:80`）做了这个短路：

```
if allowList != nil && !h.forbidFlat && allowList.Len() < flatSearchCutoff:
    return h.flatSearch(ctx, vector, k, searchTimeEF(k), allowList)
return h.knnSearchByVector(ctx, vector, k, searchTimeEF(k), allowList)
```

`flatSearchCutoff` 默认 **40000**（`entities/vectorindex/hnsw/config.go:34`）。这是 ACORN 式过滤检索（默认 `filterStrategy = acorn`，`entities/vectorindex/hnsw/config.go:39`）之外的第二道保险：过滤极窄时直接退 flat，宽过滤时才走图。

### 26.4 PQ：把距离计算从 O(D) 降到 O(M)

把 D 维向量切成 M 段（每段 D/M 维），每段用 K = 256 个质心量化，编码成 M 个字节。配置结构在 `entities/vectorindex/hnsw/pq_config.go:44`（`enabled` / `bitCompression` / `segments` / `centroids` / `trainingLimit` / `encoder`），默认值见同文件 `entities/vectorindex/hnsw/pq_config.go:28`（`centroids=256`、`trainingLimit=100000`、`encoder.type=kmeans`）。

关键是 ADC（Asymmetric Distance Computation）：查询向量保持 float32，用查表把距离拆成 M 个可预计算的子距离。

预计算（`adapters/repos/db/vector/compressionhelpers/product_quantization.go:74`）：

```
for i in 0..M-1:                    # 子空间
    for c in 0..K-1:                # 质心
        lut[i][c] = dist(query[i*D/M : (i+1)*D/M], centroid[i][c])
```

代价是 M·K 次距离计算。源码注释给的例子：D=1536、每段 4 维即 M=384、K=256 时是 98304 次，「仍然很快，且能让 LookUp 热路径上完全没有分支」。

查询退化成查表求和（`adapters/repos/db/vector/compressionhelpers/product_quantization.go:87`）：

```
distance(encoded_y) = Σ_{i=0}^{M-1} lut[i][ y.code[i] ]
```

代码把 i 手工 8 次展开并注明「M1 MacBook 上的甜点，换平台可能有差异但肯定不会更差」；另有两处 `panic` 分支专门给编译器做 Bounds-Check Elimination 提示 —— 它们在逻辑上不可能触发，但能让编译器删掉热循环里的边界检查。

单次距离计算的复杂度与存储对比：

| 方案 | 距离复杂度 | 额外查询侧预处理 | 每向量存储 |
|------|-----------|----------------|-----------|
| float32 全量 | O(D) | 无 | 4D 字节 |
| PQ（ADC） | O(M) | O(M·K) | M 字节（D=1536/M=384 时 32×） |
| BQ | ⌈D/64⌉ 次 popcount | 无 | ⌈D/64⌉×8 字节 |

### 26.5 tile 编码器：不聚类，直接按分布分位

PQ 默认用 kmeans 编码器，但 Weaviate 还提供 `tile` 编码器（`entities/vectorindex/hnsw/pq_config.go:22` 定义 `PQEncoderTypeTile = "tile"`）。它**不做聚类**，而是假设每个子空间的分量近似服从正态或对数正态，直接把累积分布 [0,1] 均分成 2^bits 个桶。

在线累计矩（`adapters/repos/db/vector/compressionhelpers/tile_encoder.go:165`）：

```
Add(x):
    x' := distribution.Transform(float64(x[segment]))   # normal: x'; log-normal: ln(max(x,0))
    s1 += x';  s2 += x'^2;  n += 1
    mean = s1 / n
    var  = (s2 + n·mean² - 2·mean·s1) / n
    std  = sqrt(var)
```

编码就是取 CDF 的分位桶号（`adapters/repos/db/vector/compressionhelpers/tile_encoder.go:177`）：

```
Encode(x):
    cdf := distribution.CDF(float64(x[segment]))
    b   := floor(cdf · bins)          # bins = 2^bits
    return byte(b)
```

反量化时桶 b 的重建值取该桶中点对应的分位数：b=0 取 `Quantile(1/bins)`，b=bins 取 `Quantile((bins-1)/bins)`，其余取 `Quantile((b/bins + (b+1)/bins)/2)`。质心按需惰性计算，并用原子标志位缓存。

复杂度对比：kmeans 的 `Fit` 是 O(n·K·D/M·M) = O(n·K·D)；tile 只需一趟 O(n·D/M) 累计。代价是精度依赖分布假设 —— 若 embedding 各维不是近似正态（例如已经过 softmax 的概率分布），tile 的重建误差会明显大于 kmeans。这个取舍由 `pq_config.go` 的 `distribution` 字段暴露（`log-normal` / `normal`，默认 `log-normal`）。

### 26.6 BQ：1 bit/维 + rescoring 精排

二元量化（`adapters/repos/db/vector/compressionhelpers/binary_quantization.go:28`）只看符号：

```
Encode(vec):
    blocks := ceil(D/64)
    for b in 0..blocks-1:
        bits := 0
        for bit in 0..63:  if vec[i] < 0 { bits |= 1 << bit };  i++
        code[b] = bits
```

压缩比恒为 32×（`adapters/repos/db/vector/compressionhelpers/binary_quantization.go:63`）：float32 的 4D 字节 vs ⌈D/64⌉×8 字节，**与维度无关**。

压缩向量之间的距离退化为 popcount 异或和（`adapters/repos/db/vector/hnsw/distancer/hamming.go:63`）：

```
d(x, y) = Σ_{b} popcount( x[b] XOR y[b] )
```

单条距离从 D 次浮点比较降到 ⌈D/64⌉ 次 popcount，128 维时只要 2 条指令。

代价是排序严重失真 —— 符号位丢掉了幅值，量化距离只能用来**粗筛**。补救是 rescoring 阶段（`adapters/repos/db/vector/hnsw/search.go:1257`）：

```
rescore(res, k):
    1. 若 SQ/RQ 启用且 RescoreLimit ≥ k，先把候选裁到 RescoreLimit 个
    2. 用原向量（float32）逐个重算精确距离，重填小顶堆，只保留 k 个
    3. 并发度 = clamp(ctx 携带的并发预算, rescoreConcurrency = 2×GOMAXPROCS, len(ids))
```

所以 BQ 的实际召回率取决于两件事：① 粗筛取多少候选（`rescoreLimit`）；② 原始向量是否还在。SQ 默认 `DefaultSQRescoreLimit = 20`（`entities/vectorindex/hnsw/sq_config.go:19`），RQ 默认 20、BRQ 默认 512（`entities/vectorindex/hnsw/rq_config.go:24`）。

一个值得注意的开关：`shouldRescore`（`adapters/repos/db/vector/hnsw/search.go:187`）在 `sqConfig.RescoreLimit == 0` 或 `rqConfig.RescoreLimit == 0` 时返回 false —— 把 rescoreLimit 设成 0 就是显式关闭精排，直接用量化距离出结果（最快，但召回率掉得最多）。flat 索引侧同理，`entities/vectorindex/flat/config.go:26` 的 `DefaultCompressionRescore = -1` 表示「交给 Weaviate 挑」，实际取值在 `adapters/repos/db/vector/flat/index.go:183`。

## 二十七、参考资料

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