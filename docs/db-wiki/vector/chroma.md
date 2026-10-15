# Chroma Wiki（面向 LLM 的嵌入式向量数据库）

> 参考版本：Chroma 1.5.x（Python + Rust 双核，Rust 1.x 提供生产级性能），Python API + Rust 段实现，向量数据库领域的"嵌入式 SQLite"。本 wiki 重点覆盖向量类型特有章节（HNSW、SPANN、量化、嵌入式运行模型）。

## 一、项目定位

Chroma 是由 **Chroma Inc.**（原 Turri/Cassandra 系团队，CEO Jeff Huber）开源的 **面向 LLM 应用的嵌入式向量数据库**，以 **"AI 原生数据基础设施"** 为口号，提供 **Pythonic / TypeScript / Rust / Go 多语言客户端**、**进程内嵌 + 客户端-服务端两种运行模式**、**Collection-Embedding-Document 三层数据模型**，是 RAG / Agent 应用最常使用的向量数据库之一。

- **痛点**：
  - 传统向量库（Pinecone / Milvus / Weaviate）部署门槛高、需要专用集群，Python notebook 中难以内嵌使用；
  - 大部分向量库缺乏面向 LLM 的抽象（Document / Embedding / Metadata 关联），需要开发者手动桥接；
  - 中小规模 RAG（百万级以下）无需分布式，但需要简单易用的本地模式 + 升级到服务端模式的能力；
  - RAG 中常需混合检索（向量 + BM25 + 元数据过滤），需要开箱即用；
- **典型用户**：RAG 应用开发者、AI Agent 框架、LangChain / LlamaIndex 用户、初创公司 MVP、个人项目、Jupyter Notebook 教学；
- **核心口号**：**"the open-source data infrastructure for AI"** + **"the AI-native database"** + **"in-process, server, cloud"**。

## 二、发展历史与社区

- **起源**：2022 年由 Jeff Huber 与前 Cassandra 团队成员创建，最初定位为"AI 原生嵌入式数据库"，与 LangChain / LlamaIndex 同步发布生态；
- **里程碑**：
  - 2022-10：0.4 首发，开箱即用的 Python `pip install chromadb`；
  - 2023-02：0.4 GA，引入 **HNSW + SQLite** 嵌入式模式；
  - 2023-08：0.4.x 系列普及，成为 LangChain 默认向量库；
  - 2024-04：**Rust 段（Rust Segment）** 引入，承担 HNSW / SPANN / Blockfile 等核心数据结构；
  - 2024-09：0.5.x Rust 性能优化（Rust Segment 默认开启），Rust 1.x 重写分布式模式；
  - 2024-12：1.0 GA 临近，Rust 端 API 稳定，引入 **Distributed HNSW（基于 Raft + S3）**、**SPANN**（微软研究院灵感）；
  - 2025-08：1.5.x 系列，**SPANN**（Space PartitionNN）作为大规模索引、**HNSW** 作为默认索引、**量化 SPANN** 持续优化；
  - 2026：Chroma Cloud（商业托管）、Rust-native 全部默认、Python API 稳定；
- **维护状态**：极活跃，月度小版本，每周 commit 数十次；
- **社区**：Discord 7 万+ 用户、GitHub 14k+ star、官方文档站 docs.trychroma.com、Forum、Reddit r/LangChain。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（Python 客户端 + Rust 段 + 服务端）；
- **商业版本**：**Chroma Cloud**（全托管，含 Vector / Hybrid / Full-text Search；注册送 $5 试用）；
- **生态关系**：
  - 与 **LangChain / LlamaIndex / Haystack** 紧耦合，作为默认向量后端；
  - 与 **OpenAI / Cohere / HuggingFace / Sentence-Transformers** 通过 EmbeddingFunction 接口集成；
  - 与 **Rust AI 生态**（burn、tch-rs）紧耦合，Rust 段提供零成本抽象。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Mozilla**：AI 内部知识库；
  - **HubSpot**：CRM 数据 AI 化；
  - **Square / Block**：客服 AI；
  - **Replit**：项目内 AI 助手；
  - **Notion**：Notion AI 的部分后端；
  - **Roblox**：游戏内容检索；
  - 国内：阿里、字节、商汤、智谱等内部 RAG 试点；
- **行业**：SaaS 客服、企业知识库、AI 助手、教育、法律合同检索、AI 编程助手；
- **典型场景**：RAG 应用（LangChain/LlamaIndex 默认）、AI Agent 工具、ChatBot 知识库、混合检索（向量 + BM25）、Embedding 缓存。

## 五、一句话对比定位

> Chroma 是 **"面向 LLM 的嵌入式向量数据库（Python + Rust 双核）+ Collection-Embedding-Document 三层抽象 + HNSW/SPANN 双索引 + 嵌入式 + 客户端/服务端 双模式 + 量化 + 混合检索"**；与 **Milvus（分布式强）**、**Qdrant（Rust 云原生）**、**Weaviate（Go 模块化）**、**LanceDB（嵌入式列存）**、**Pinecone（商业全托管）**、**FAISS（Meta 算法库）**、**USearch（C++ 嵌入式）** 构成向量库阵营。Chroma 的差异化是 **"AI 原生数据模型 + 嵌入式零部署 + LangChain 生态默认"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层                                   │
│  - Python: chromadb.Client / AsyncClient / AdminClient             │
│  - JavaScript/TS: chromadb npm（HTTP 客户端）                      │
│  - Rust: chroma-rust-sdk（Crate）                                   │
│  - Go: clients/go（HTTP 客户端）                                    │
│  chromadb/api/{client,async_client,fastapi}.py                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ HTTP/JSON (Server mode) or in-process call (Embedded mode)
┌─────────────────────▼───────────────────────────────────────────────┐
│                      API 服务层（FastAPI）                            │
│  chromadb/server/fastapi/                                            │
│  ├─ ServerAPI（/api/v1/collections, /api/v2/...）                  │
│  ├─ AuthMiddleware（Token / JWT / basic）                           │
│  ├─ Quota / Rate Limit                                              │
│  └─ Telemetry（OpenTelemetry）                                      │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  System / 配置注入层                                  │
│  chromadb/config.py：System 容器，DI 风格，组件化                  │
│  ├─ SegmentManager（管理各 Segment 生命周期）                       │
│  ├─ SysDB（系统元数据：collection、tenant、database）               │
│  ├─ QuotaEnforcer                                                  │
│  └─ OpExecutor / DistributedExecutor                              │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Segment 层（核心）                                  │
│  chromadb/segment/impl/（Python 包装）                              │
│  rust/segment/src/（Rust 实现，性能核心）                          │
│  ├─ LocalHnswSegment        内存 HNSW                              │
│  ├─ LocalHnswSegmentReader  磁盘 HNSW reader                      │
│  ├─ DistributedHnswSegment  分布式 HNSW（RAFT + S3）              │
│  ├─ SpannSegment            SPANN（Space Partition NN）           │
│  ├─ QuantizedSpannSegment   量化 SPANN                            │
│  ├─ SqliteMetadataSegment   SQLite 元数据                         │
│  ├─ BlockfileRecordSegment  Blockfile 记录                       │
│  └─ BlockfileMetadataSegment Blockfile 元数据                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  索引库（Rust crate）                                 │
│  rust/index/                                                        │
│  ├─ HnswIndex（HNSW 实现）                                         │
│  ├─ SpannIndex（SPANN 实现）                                       │
│  └─ Distance（COSINE / L2 / IP / Hamming）                        │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  存储后端                                             │
│  - SQLite（嵌入式元数据）                                           │
│  - Blockfile（自定义列存，Rust crate blockstore/）                 │
│  - S3 / GCS / Azure（云模式分布式存储）                              │
│  - 内存（纯嵌入模式）                                                │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **嵌入式优先（Embedded First）**：默认 `chromadb.PersistentClient(path="./chroma_db")` 把整个数据库嵌入到 Python 进程，无需启动服务；`chromadb.HttpClient(host="...")` 才走服务端；
- **Python + Rust 双核**：Python 负责 API / 配置 / 业务编排，Rust 负责高性能索引（HNSW / SPANN / Blockfile）；通过 PyO3 绑定暴露给 Python；
- **Segment 抽象**：所有数据按 SegmentType 划分（vector / metadata / record），每个 Collection 由 1 个 VectorSegment + 1-2 个 MetadataSegment 组成；
- **系统表 + 元数据分离**：SysDB 存 collection/tenant/database，Segment 存实际向量 / 文档 / 元数据；
- **DI 风格 System 容器**：所有组件通过 `System.instance()` 注册依赖，便于测试和扩展；
- **云原生分布式模式**：1.x 引入 Distributed HNSW（基于 Raft + S3），支持横向扩展。

## 七、存储引擎

### 7.1 三种存储后端

| 后端 | 适用场景 | 实现位置 |
|------|----------|----------|
| **Memory** | 测试、纯演示 | 内存 HNSW + 内存 dict |
| **SQLite + Persistent** | 本地嵌入式默认 | `chromadb/db/` + Rust `sqlite/` |
| **Blockfile + S3** | 生产 / 云 | Rust `blockstore/` + 对象存储 |

### 7.2 Blockfile（Rust 自研列存格式）

Chroma 的 Rust 段（Blockfile）是**自定义列存格式**，类似 Parquet 但更简单：

- 每个 Blockfile 是一个不可变 Block 列表，每 Block 含固定数量记录；
- 支持 **Sparse Index**（每 Block 摘要 min/max，加速过滤）；
- 支持 **Bloom Filter**（加速 key 查找）；
- 源码：`rust/blockstore/src/`, `rust/segment/src/blockfile_record.rs`, `rust/segment/src/blockfile_metadata.rs`；
- 写入是 **append-only**，定期 compaction。

### 7.3 SQLite 元数据段

- 用于存 **元数据字段（where 过滤）** 和 **collection schema**；
- 实现：`chromadb/segment/impl/sqlite.py` + Rust `rust/sqlite/`;
- 索引：在 metadata 字段上建 BTree 索引，支持 `where category="book"` 加速。

## 八、索引机制（**向量类型核心**）

### 8.1 向量索引算法全景

Chroma 当前默认 / 支持的索引算法：

| 索引 | 类型 | 适用规模 | 实现位置 |
|------|------|----------|----------|
| **HNSW** | 图索引（Graph） | 中小（百万-千万）| `rust/segment/src/local_hnsw.rs`, `rust/index/` |
| **SPANN** | 分区图（Space Partition + ANN）| 大（亿-百亿）| `rust/segment/src/spann_provider.rs` |
| **Quantized SPANN** | 量化 SPANN | 超大规模 | `rust/segment/src/quantized_spann.rs` |
| **Distributed HNSW** | 分布式 HNSW | 多机集群 | `rust/segment/src/distributed_hnsw.rs` |

### 8.2 HNSW 详解（Chroma 默认索引）

**HNSW（Hierarchical Navigable Small World）** 是 Malkov & Yashunin 2016 提出的图索引：

**分层结构**：

```
┌──────────────────────────────────────────────────────────────┐
│  Layer 2（最稀疏，5% 节点）                                  │
│  ●──────────────●──────────────●──────────────●             │
│  ↑ entry point                                                  │
├──────────────────────────────────────────────────────────────┤
│  Layer 1（中等密度，15% 节点）                                │
│  ●─────●──────●──────●──────●──────●──────●──────●         │
│  ●────●──────●──────●──────●──────●──────●                  │
├──────────────────────────────────────────────────────────────┤
│  Layer 0（最密集，100% 节点）                                │
│  ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●     │
│  全连接式近似 k-NN 图                                       │
└──────────────────────────────────────────────────────────────┘
```

**核心参数**：
- **`M`**：每个节点的最大连接数（默认 16，影响 recall 与内存）；
- **`efConstruction`**：构建时的候选队列大小（默认 100，影响构建速度与索引质量）；
- **`efSearch`**：查询时的候选队列大小（默认 10，影响 recall 与延迟）；
- **`maxNeighbors`, `maxLayer`**：内部参数。

**距离度量**：
- **COSINE**（默认，向量归一化后等价于内积）；
- **L2**（欧氏距离）；
- **IP**（内积，未归一化）。

**源码位置**：
- Rust 段接口：`rust/segment/src/local_hnsw.rs`（LocalHnswSegmentReader、apply_local_hnsw_segment）；
- 索引实现：`rust/index/src/hnsw.rs`（`HnswIndex` 结构、`HnswIndexConfig`）；
- 距离：`rust/distance/src/`。

**写入流程**（Chroma `add()`）：
1. 计算 Embedding（通过 EmbeddingFunction 或外部传入）；
2. 通过 SysDB 分配 SeqId；
3. 写入 Metadata Segment（SQLite / Blockfile）；
4. 写入 Vector Segment（内存 HNSW 增删 + 持久化队列）。

**查询流程**（Chroma `query()`）：
1. 计算 query 向量；
2. 过滤 metadata（`where` 子句在 Metadata Segment 命中 SeqId 集合）；
3. 在 HNSW 中以候选 SeqId 集合为约束（可选 `filtered_search`）做近似 k-NN；
4. 合并结果（distance + metadata + document）。

### 8.3 SPANN（Space Partition NN）

SPANN 是微软研究院提出的 **基于空间分区的 ANN 索引**（论文 SPANN: Highly-efficient Billion-scale Approximate Nearest Neighbor Search），Chroma 在 1.x 引入：

- **思路**：把向量空间切分为多个 partition（聚类中心），posting list 维护每个 partition 的向量列表；
- **查询**：先找最近 partition，再在 partition 内做精确 / 近似 k-NN；
- **优势**：适合**超大规模**（亿级以上）、**磁盘友好**（posting list 可外存）；
- **代价**：召回率对聚类质量敏感，需调参；
- **源码**：`rust/segment/src/spann_provider.rs`, `rust/index/src/spann/`。

### 8.4 Quantized SPANN

- 在 SPANN 基础上对 posting list 的向量做 **PQ（Product Quantization）** 压缩；
- 大幅降低内存与磁盘占用；
- 源码：`rust/segment/src/quantized_spann.rs`（需 `usearch` feature 开启）。

### 8.5 分布式 HNSW

- **架构**：多个 Worker 节点各自维护部分 HNSW shard + 协调节点（Cordinator）；
- **协调**：基于 **Raft** 一致性（Rust `memberlist/` crate）；
- **持久化**：向量与图结构存于 **S3 / GCS**（Rust `wal3/` + `s3heap/`）；
- **路由**：通过 SysDB 维护 sharding 元数据；
- **源码**：`rust/segment/src/distributed_hnsw.rs`, `rust/worker/`, `rust/sysdb/`, `rust/memberlist/`。

### 8.6 混合索引：HNSW + 过滤

- **pre-filter**：先在元数据 segment 过滤（SQLite where）得到 SeqId 集合，再在 HNSW 上做 constrained search；
- **post-filter**：先 HNSW 检索 top-K，再在 metadata 上过滤（K 要放大）；
- **Graph pre-filter**：参考 ACORN 思路在 HNSW 图遍历过程中跳过被过滤的节点。

## 九、查询/计算/接口层

### 9.1 Python API（核心）

```python
import chromadb

# 嵌入式模式（最常用）
client = chromadb.PersistentClient(path="./chroma_db")
collection = client.get_or_create_collection("my_docs", metadata={"hnsw:space": "cosine"})

# 添加
collection.add(
    documents=["Chroma is a vector database", "Chroma is AI-native"],
    metadatas=[{"source": "web"}, {"source": "doc"}],
    ids=["doc1", "doc2"],
)

# 查询
results = collection.query(
    query_texts=["What is Chroma?"],
    n_results=2,
    where={"source": "web"},
)
```

- 完整 API：`chromadb/api/client.py`, `chromadb/api/types.py`；
- 异步 API：`chromadb/api/async_client.py`；
- Admin API：`chromadb/api/admin.py`（创建 database / tenant）。

### 9.2 HTTP REST API（Server 模式）

- 实现：`chromadb/server/fastapi/`；
- 主要 endpoint：`/api/v1/collections`, `/api/v1/collections/{id}/add`, `/api/v1/collections/{id}/query`；
- 认证：Token（`Authorization: Bearer chroma-...`）、JWT、Basic；
- 二进制协议：Protocol Buffers（`idl/`）+ JSON。

### 9.3 多语言 SDK

| 语言 | 包名 | 实现 |
|------|------|------|
| Python | `chromadb` | 原生 |
| TypeScript / JS | `chromadb` (npm) | HTTP |
| Rust | `chroma` (crate) | 原生（Crate） |
| Go | `clients/go` | HTTP |
| Ruby / Java | 社区第三方 | HTTP |

### 9.4 多租户（Multi-tenancy）

- **三层模型**：`Tenant`（顶层）→ `Database`（命名空间）→ `Collection`（实际数据）；
- 默认租户：`default_tenant`，默认数据库：`default_database`；
- 隔离：每个 Tenant 独立 SQLite + Blockfile 目录；
- 配额：QuotaEnforcer（`chromadb/quota/`）限制资源使用。

### 9.5 混合检索（Hybrid Search）

- **向量检索**：HNSW / SPANN 默认；
- **BM25 全文检索**：内置（Rust `bm25_chroma` crate）；
- **融合**：通过 `Collection.query()` 多模返回 + 客户端 RRF 融合；
- **Rerank**：支持外部 Cross-Encoder（用户自行接入）。

### 9.6 Embedding Function 接口

- **抽象**：`chromadb/api/types.py::EmbeddingFunction`；
- **内置**：Default Embedding Function（基于 `all-MiniLM-L6-v2`，小模型）、ONNX Embedding Function；
- **外部**：OpenAI / Cohere / HuggingFace / Sentence-Transformers 适配器；
- **自定义**：用户实现 `__call__(self, input: Documents) -> Embeddings`。

## 十、事务与一致性

- **嵌入式模式**：单进程，SQLite 事务保证元数据一致性，HNSW 内存操作无显式事务；
- **服务端模式**：基于 **Raft** 的元数据一致性（Rust `memberlist/` + `sysdb/`），向量写入走 **WAL（`wal3/`）**；
- **一致性级别**：默认最终一致（向量写入异步持久化到 S3 / 磁盘）；
- **崩溃恢复**：通过 `wal3` 重放未刷盘日志。

## 十一、复制与高可用

- **嵌入式模式**：无 HA，依赖文件系统；
- **服务端模式**：
  - **元数据**：Raft 共识（3/5 节点）；
  - **向量数据**：存 S3 / GCS / Azure，多副本由对象存储保证；
  - **Worker 节点**：无状态，故障后协调节点重新分配；
- **Chroma Cloud**：全托管，多区域复制。

## 十二、扩展性

- **垂直扩展**：HNSW 受限于内存（M 16 约每向量 50-100 bytes × 维度）；
- **水平扩展**：
  - **服务端模式**：增加 Worker 节点（向量分片）；
  - **嵌入式模式**：升级到服务端模式无需迁移 API；
- **冷热分层**：暂未官方支持，社区方案是把热 HNSW 留在内存、冷向量存 Blockfile。

## 十三、性能与基准

- **HNSW 查询延迟**：百万级向量约 **1-10ms / query**（Python 调用 + 1k embedding）；
- **Rust HNSW vs Python HNSW**：**5-10x** 加速（hnswlib-py 同期）；
- **ann-benchmarks**：Chroma HNSW 在 glove-100、gist-960 上接近 FAISS baseline；
- **SPANN**：亿级向量查询延迟 **10-50ms**，内存占用仅 **1-2 bytes/dim**（量化）；
- **嵌入式模式**：纯本地操作，吞吐受限于磁盘 I/O。

## 十四、运维与可观测性

- **监控**：Prometheus exporter（Rust `metering/` + `metering-macros/`）；
- **日志**：结构化日志（Rust `log/` + `log-service/`）；
- **Tracing**：OpenTelemetry（`rust/tracing/`）；
- **备份**：嵌入式模式直接拷贝 SQLite + Blockfile；服务端模式用 S3 快照；
- **CLI**：`chroma run --path ...`, `chroma db migrate`。

## 十五、生态与集成

- **LangChain / LlamaIndex**：默认向量后端；
- **Haystack**：Chroma Document Store；
- **OpenAI / Cohere / HuggingFace**：Embedding Function 适配；
- **Docker / Helm**：`deployments/docker/`, `k8s/`；
- **CLI**：`rust/cli/`（chroma 命令）。

## 十六、核心论文与算法原理

- **HNSW**：Malkov & Yashunin, "Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs", 2016, IEEE TPAMI；
- **SPANN**：Chen et al., "SPANN: Highly-efficient Billion-scale Approximate Nearest Neighbor Search", 2021, NeurIPS；
- **PQ（Product Quantization）**：Jégou et al., 2011；
- **k-means 聚类**：SPANN 的基础；
- **ACORN**（filtered HNSW）：2024；
- **RAFT 共识**：Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm", 2014。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | API 入口 | `chromadb/__init__.py`, `chromadb/api/client.py` | 用户接口 |
| P0 | System 容器 | `chromadb/config.py` | DI 风格组件管理 |
| P0 | Local HNSW | `rust/segment/src/local_hnsw.rs` | HNSW 段实现 |
| P0 | HNSW 索引 | `rust/index/src/hnsw.rs` | 图算法 |
| P1 | SPANN | `rust/segment/src/spann_provider.rs` | 大规模索引 |
| P1 | Blockfile | `rust/segment/src/blockfile_record.rs` | 自研列存 |
| P1 | SysDB | `rust/sysdb/src/` | 元数据 |
| P2 | Distributed HNSW | `rust/segment/src/distributed_hnsw.rs` | 分布式 |
| P2 | FastAPI 服务 | `chromadb/server/fastapi/` | HTTP 服务 |
| P2 | WAL | `rust/wal3/` | 写前日志 |

## 十八、源码规模与开发语言

- **Python**：约 50% LOC（API / 配置 / 服务）；
- **Rust**：约 45% LOC（核心索引 / 存储 / 分布式）；
- **TypeScript / JS**：约 3%（前端、客户端）；
- **构建系统**：`Cargo.toml` + `pyproject.toml` + Docker；
- **CI**：GitHub Actions（`Dockerfile.windows`, `Tiltfile`）。

## 十九、学习资源

- **官方文档**：https://docs.trychroma.com/
- **GitHub**：https://github.com/chroma-core/chroma
- **Discord**：https://discord.gg/MMeYNTmh3x
- **AGENTS.md / CLAUDE.md**：项目根目录提供开发指南；
- **博客**：https://blog.trychroma.com/
- **HN / Reddit**：r/LangChain 大量 Chroma 实战经验。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 | 原生 |
| macOS x86_64 / ARM64 | 原生（Rust 编译）|
| Windows x86_64 | 原生（`Dockerfile.windows`）|
| ARM Linux | Rust 交叉编译 |
| Docker | `Dockerfile` 多阶段构建 |
| Helm / K8s | `k8s/` Chart |
| Chroma Cloud | 全托管 |

## 二十一、变体与衍生项目

- **Chroma Cloud**：官方全托管服务；
- **Rust 客户端独立**：可单独使用 `chroma` crate（无需服务端）；
- **OpenInference**：与 OpenTelemetry AI tracing 集成；
- **社区分叉**：少量自托管分支。

## 二十二、风险与陷阱

1. **嵌入式模式的内存爆炸**：HNSW 默认 `M=16`，百万向量 × 768 维 ≈ 10-20GB 内存；建议开启量化；
2. **持久化时机**：`PersistentClient` 写入会触发 `persist()`，但崩溃时仍有数据丢失窗口；关键数据建议走服务端模式；
3. **HNSW efSearch 调优**：默认 `ef=10` 在小数据集 OK，大数据集（百万+）需要 `ef=50-100` 提升 recall，但延迟也会上升；
4. **Python GIL**：默认 HNSW 在 Python 中持有 GIL，但 Rust 段（1.x 默认）已释放 GIL；
5. **多进程并发**：嵌入式模式不支持多进程写同一 collection，必须升级到服务端；
6. **SPANN 训练数据敏感**：聚类质量依赖训练数据分布，强烈建议先 `add()` 足量样本再 query；
7. **版本升级**：1.x 的 Rust 段与 0.4.x Python 段数据格式不完全兼容，迁移前需阅读 MIGRATIONS。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 已实现部分向量索引（基于 Faiss），可参考 Chroma 的 HNSW Segment 抽象设计；
- **`learning/notes/`** 可沉淀 Chroma 的 Rust crate 设计与 PyO3 绑定经验；
- **本项目暂无分布式向量检索**，Chroma 的 Raft + S3 模式可作为未来扩展参考。

## 二十四、参考资料

- [Chroma GitHub](https://github.com/chroma-core/chroma)
- [Chroma 官方文档](https://docs.trychroma.com/)
- [Chroma Cloud](https://www.trychroma.com/)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [SPANN 论文](https://proceedings.neurips.cc/paper/2021/hash/483cc4d2ec1d8bd19c5dd3b8a2e08e3c-Abstract.html)
- [PyO3 绑定文档](https://pyo3.rs/)
- [RAFT 论文](https://raft.github.io/raft.pdf)
- [Jégou PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [ACORN filtered HNSW](https://arxiv.org/abs/2403.04871)