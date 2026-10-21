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

## 二十四、索引类型族与选型边界：HNSW / SPANN / 量化 SPANN / Brute Force（算法展开）

> 本节起至 §二十八的源码路径以本仓库 `reference/vector/chroma/` 子模块为准，实测版本 **`1.5.9-259-gdfe209a58`**（`git describe --tags`；HEAD commit `dfe209a58`），与本 wiki 头部声明的 Chroma 1.5.x 一致。若引用的是 0.4.x（纯 Python `chromadb.segment.impl.vector.local_hnsw.LocalHnswSegment`）时代，类名与参数默认值均已不同，**不可混写**。

### 24.1 HNSW 不是自研 Rust 图索引，而是 hnswlib 的薄封装

旧节 §8.2 把 `rust/index/src/hnsw.rs` 描述为「索引实现 / 图算法」，这在 1.5.x 上**不准确**：

```rust
// rust/index/src/hnsw.rs:80
pub struct HnswIndex {
    index: hnswlib::HnswIndex,          // ← 真正的图算法在 hnswlib（C++）里
    pub id: IndexUuid,
    pub distance_function: DistanceFunction,
}
```

`hnswlib` 是 `rust/index/Cargo.toml:37` 声明的外部依赖（Rust ↔ C++ FFI 绑定）。因此：

- **Chroma 自己实现的**：段（Segment）生命周期、索引版本管理、缓存、参数管道、并发控制；
- **Chroma 没有实现的**：HNSW 的 level 生成（指数衰减 `mult = 1/ln(M)`、每点 `floor(-ln(unif(0,1)) * mult)`）、邻居选择启发式（`SELECT_NEIGHBORS_HEURISTIC` 的"多样性剪枝"）、图收缩（`shrink_neighbor_list`）——**这些全部在 hnswlib 内，本仓库不可见**，本文不做展开。

Chroma 侧真正实现的是**参数管道**：

```text
Python 侧 HnswParams(metadata)            # chromadb/segment/impl/vector/hnsw_params.py:54
      │  hnsw:M / hnsw:construction_ef / hnsw:search_ef / hnsw:space
      ▼
Rust 侧 HnswIndexConfig                     # rust/index/src/hnsw.rs:22
      │  { max_elements, m, ef_construction, ef_search, random_seed, persist_path }
      ▼
hnswlib::HnswIndexInitConfig                # rust/index/src/hnsw.rs:156
      │  distance_function ← map_distance_function()   # rust/index/src/hnsw.rs:275
      ▼
hnswlib（C++）
```

`map_distance_function`（`rust/index/src/hnsw.rs:275`）只映射三种度量：`Cosine / Euclidean / InnerProduct`。

**容量策略**：初始 `max_elements` 只有 **100**（`rust/index/src/hnsw.rs:13`），源码注释解释得很直接——

```rust
// rust/index/src/hnsw.rs:8-13
// Setting it to a small value to prevent
// bloating of the index which directly impacts query latency by increasing
// the amount of bytes that need to be fetched from s3. The trade off is that
// there will be more data movement/allocations during compaction which is
// acceptable since compaction is a background operation.
pub const DEFAULT_MAX_ELEMENTS: usize = 100;
```

超出后按需扩容，且**不同路径策略不同**：

| 路径 | 扩容策略 | 位置 |
|------|----------|------|
| 嵌入式 Local HNSW | `next_power_of_two()`（2 的幂） | `rust/segment/src/local_hnsw.rs:800` |
| 分布式 HNSW | `index_capacity * 2` | `rust/segment/src/distributed_hnsw.rs:228` |
| SPANN 头节点索引 | `hnsw_capacity * 2` | `rust/index/src/spann/types.rs:1464` |

这是 HNSW 的一个隐蔽成本：**扩容需要整体重分配邻接表**，所以容量必须预留余量，否则运行期会产生多次全量复制。

### 24.2 索引参数的三个来源与实测默认值

参数在 Chroma 里有**三套并存的默认**，容易踩坑：

```text
① Python 段元数据路径（0.4 时代遗留）    chromadb/segment/impl/vector/hnsw_params.py
② Rust SPANN 内部配置                     rust/types/src/spann_configuration.rs
③ Rust HNSW Provider 配置                 rust/index/src/config.rs
```

实测默认值：

| 参数 | 默认值 | 位置 | 说明 |
|------|--------|------|------|
| `hnsw:space` | **`l2`** | `chromadb/segment/impl/vector/hnsw_params.py:56` | ⚠️ 不是 cosine，见 §25.5 / §28.3 |
| `hnsw:construction_ef` | 100 | `chromadb/segment/impl/vector/hnsw_params.py:57` | |
| `hnsw:search_ef` | 100 | `chromadb/segment/impl/vector/hnsw_params.py:58` | ⚠️ 旧节 §8.2 写 10 |
| `hnsw:M` | 16 | `chromadb/segment/impl/vector/hnsw_params.py:59` | |
| `hnsw:resize_factor` | 1.2 | `chromadb/segment/impl/vector/hnsw_params.py:63` | Python hnswlib 的预留系数 |
| SPANN `search_nprobe` | 64 | `rust/types/src/spann_configuration.rs:7` | 自适应开启时被覆盖 |
| SPANN `write_nprobe` | 32 | `rust/types/src/spann_configuration.rs:19` | |
| SPANN `split_threshold` | 50 | `rust/types/src/spann_configuration.rs:35` | posting list 超过就劈成 2 簇 |
| SPANN `merge_threshold` | 25 | `rust/types/src/spann_configuration.rs:51` | 低于就并入邻居 |
| SPANN `num_samples_kmeans` | 1000 | `rust/types/src/spann_configuration.rs:39` | 每次 split 的采样上限 |
| SPANN `m`（头节点图 M） | 64 | `rust/types/src/spann_configuration.rs:67` | 头节点图比全量 HNSW 更稠密 |
| SPANN `ef_construction` / `ef_search` | 200 / 200 | `rust/types/src/spann_configuration.rs:59,63` | |
| SPANN `reassign_neighbor_count` | 64 | `rust/types/src/spann_configuration.rs:47` | split 后连带重分配邻居的点数 |
| `permitted_parallelism` | 180 | `rust/index/src/config.rs:29` | 分段互斥锁的并发档位 |
| `pl_block_size` | 5 MiB | `rust/index/src/config.rs:54` | posting list 分块大小 |
| `adaptive_search_nprobe` | `true` | `rust/index/src/config.rs:58` | |

> ⚠️ 校验范围写在字段属性上，例如 `split_threshold` 被 `#[validate(range(min = 25, max = 200))]` 限定、`merge_threshold` 被 `range(min = 12, max = 100)` 限定（`rust/types/src/spann_configuration.rs:120,130`）——超出范围在配置反序列化时直接失败，而不是运行期才炸。

### 24.3 SPANN 的真实结构：簇心 HNSW + Blockfile posting list

旧节 §8.3 说 SPANN 是「把向量空间切分为 partition，先找最近 partition 再在 partition 内做 k-NN」，方向对，但**没说清 partition 是怎么被检索的**。1.5.x 的实现是一个**两级结构**：

```text
                    ┌──────────────────────────────────────┐
   查询向量 q ──────▶│  第 1 级：HNSW(簇心)                │
                    │  索引里只有「簇心」，没有原始向量     │
                    │  nprobe = determine_search_nprobe()  │
                    └───────────────┬──────────────────────┘
                                    │  center_ids: Vec<u32>
                                    ▼
                    ┌──────────────────────────────────────┐
                    │  第 2 级：Blockfile posting list     │
                    │  fetch_posting_list(head_id)         │
                    │  每个 posting list ≈ 50 个向量       │
                    └───────────────┬──────────────────────┘
                                    ▼
                    ┌──────────────────────────────────────┐
                    │  暴力精算 + BinaryHeap top-k          │
                    │  （SpanBfPlOperator）                │
                    └──────────────────────────────────────┘
```

关键类型（`rust/index/src/spann/types.rs:2721`）：

```rust
pub struct SpannIndexReader<'me> {
    pub posting_lists: BlockfileReader<'me, u32, SpannPostingList<'me>>,
    pub hnsw_index: HnswIndexRef,        // ← 只装簇心
    pub versions_map: BlockfileReader<'me, u32, u32>,
    pub dimensionality: usize,
    pub adaptive_search_nprobe: bool,
    pub params: InternalSpannConfiguration,
}
```

`nprobe` 不是固定值，而是一个**下限 + 自适应经验值取大**的函数（`rust/index/src/spann/types.rs:2884`）：

```python
# rust/index/src/spann/types.rs:2889-2903
def determine_search_nprobe(self, collection_num_records_post_compaction, k):
    # Query at least 20x more points than k.
    min_nprobe      = ceil(k * 20 / split_threshold)
    optimal_nprobe  = (24 if n <= 500_000
                       else 32 if n <= 1_000_000
                       else 64) if adaptive_search_nprobe else params.search_nprobe
    return max(optimal_nprobe, min_nprobe)
```

其中 `k * 20 / split_threshold` 是**期望召回 20×k 个点所需的簇数**（每个簇平均 `split_threshold` 个点）。这条下界保证了两件事：

1. `k` 变大时 `nprobe` 自动变大（`k=100, split=50` → `nprobe ≥ 40`）；
2. 自适应档位（24 / 32 / 64）永远不会把它压到召回不足。

第二级是纯暴力：`rust/worker/src/execution/operators/spann_bf_pl.rs:59` 起手就是 `BinaryHeap::with_capacity(input.k)`。

**`fetch_posting_list` 还要做版本校验**（`rust/index/src/spann/types.rs:2964`）：逐条比对 posting list 里的 `doc_versions[i]` 与 versions map 里的当前版本，过期就跳过；同一个 `doc_offset_id` 重复出现也跳过。也就是说 **posting list 是可能包含已删除 / 已更新版本的**。

### 24.4 SPANN 的写路径：split / merge / 版本墓碑

SPANN 的 posting list 是自组织的，写入时按长度自适应分裂或保持：

```text
add(id, emb)
  │
  ├─ versions_map[id] += 1        # rust/index/src/spann/types.rs:759
  │                              # version == 0 表示已删除（rust/index/src/spann/types.rs:795）
  ├─ cosine ? normalize(emb) : emb # rust/index/src/spann/types.rs:1634-1638
  ▼
append(head_id, ...)
  │  持 posting_list_partitioned_mutex 分段锁（按 head_id 分片）
  ├─ 追加到 posting list
  ├─ 顺带清理：丢弃 version==0 或 version < 当前版本的条目  # rust/index/src/spann/types.rs:1240-1253
  ▼
if 存活条目数 <= split_threshold (50):
      写回，结束                                          # rust/index/src/spann/types.rs:1255
else:
      打乱 → k-means(k=2) → 拆成两个头节点
      两个新簇心都作为点插入头节点 HNSW                    # rust/index/src/spann/types.rs:1317-1331
      并对旧簇心的 64 个邻居做连带重分配                    # rust/index/src/spann/types.rs:1147-1168
```

合并在 GC 阶段对称进行（`garbage_collect_head`，`rust/index/src/spann/types.rs:1790`）：

```text
source_cluster_len = 清理过期条目后的长度
if source_cluster_len > merge_threshold (25):  写回，结束   # rust/index/src/spann/types.rs:1824
elif source_cluster_len == 0:                    从头节点 HNSW 删除  # rust/index/src/spann/types.rs:1843
else:  在 num_centers_to_merge_to (8) 个最近邻簇心里
       找第一个合并后长度 < split_threshold (50) 的目标        # rust/index/src/spann/types.rs:1890
```

**递归深度保护**：`MAX_REASSIGN_DEPTH = 4`（`rust/index/src/spann/types.rs:211`）。split 时若发现目标头节点已被并发删除，会递归追一个新头节点；超过 4 层就**让本次 compaction 失败**而不是静默丢点（`rust/index/src/spann/types.rs:1196-1206`）——源码注释说得很清楚：

```rust
// Returning Ok here would drop the point from the index, so fail
// the compaction instead and let the scheduler retry it.
```

split 用的 k-means 是一个**带 lambda 的两阶段变体**（`rust/index/src/spann/utils.rs:434`），不是标准 Lloyd：

- 中心初始化跑 3 轮（`NUM_ITERS_FOR_CENTER_INIT = 3`，`rust/index/src/spann/utils.rs:27`）；
- 主循环最多 100 轮（`NUM_ITERS_FOR_MAIN_LOOP = 100`，`rust/index/src/spann/utils.rs:28`），连续 5 轮无改进就停（`NUM_ITERS_NO_IMPROVEMENT = 5`，`rust/index/src/spann/utils.rs:29`），或中心位移 < `1e-3`（`rust/index/src/spann/utils.rs:468`）；
- 收敛后做最后一次指派，得到每簇的**真实代表点**（取簇内最近的那个点，而不是算术均值中心）——最终中心是 `cluster_nearest_point_idx` 指向的原始向量（`rust/index/src/spann/utils.rs:478-482`）。

这一步很关键：**簇心是一个真实存在的数据点**。它让 `posting list → 簇心` 的距离计算与 `HNSW(簇心) → 查询` 的距离计算走同一个度量，不需要额外的"聚类中心"语义。

### 24.5 Brute Force 的三处兜底边界

Chroma 有**三个互不相关**的"退化为暴力"路径，这是理解它延迟特征的关键：

| # | 触发条件 | 位置 | 复杂度 |
|---|----------|------|--------|
| ① | **HNSW 删除率 > 20% 且存活元素 < 100** | `rust/segment/src/local_hnsw.rs:320` | `O(n·d)`，`n < 100` |
| ② | **嵌入式持久模式**：未 compaction 的增量 batch 常驻暴力索引 | `chromadb/segment/impl/vector/local_persistent_hnsw.py:444` | 双路归并，`O((n_hnsw + n_bf)·d)` |
| ③ | **未 compaction 的 WAL 永远暴力扫** | `rust/worker/src/execution/operators/knn_log.rs:100` | `O(L·d)`，`L` = WAL 条数 |

**① 的判断式**（`rust/segment/src/local_hnsw.rs:315-320`）：

```rust
let delete_percentage = (len_with_deleted - actual_len) as f32 / len_with_deleted as f32;
// If the index is small and the delete percentage is high, its quite likely that the index is
// degraded, so we brute force the search
if delete_percentage > 0.2 && actual_len < 100 { /* 全量扫 + BinaryHeap top-k */ }
```

理由很实在：HNSW 的删除只是打墓碑（hnswlib 不做图收缩），墓碑一多，遍历时大量候选被跳过，**图比暴力还慢**。所以这里用一个"退化检测"阈值主动切暴力。

**② 的过查询 + 双路归并**（`chromadb/segment/impl/vector/local_persistent_hnsw.py:442-514`）：

```python
# Overquery by updated and deleted elements layered on the index because they may
# hide the real nearest neighbors in the hnsw index
hnsw_k = k + self._curr_batch.update_count + self._curr_batch.delete_count   # chromadb/segment/impl/vector/local_persistent_hnsw.py:444

bf_results   = self._brute_force_index.query(query)     # chromadb/segment/impl/vector/local_persistent_hnsw.py:463
hnsw_results = super().query_vectors(hnsw_query)       # chromadb/segment/impl/vector/local_persistent_hnsw.py:464  （HNSW 多路）
# 两条都已按距离升序，用双指针归并取前 k，且对 BF 已有 id 去重
while len(curr_results) < min(k, total_results):       # chromadb/segment/impl/vector/local_persistent_hnsw.py:486
    if bf_dist <= hnsw_dist: take bf; else: take hnsw (若 bf 未含该 id)
```

`hnsw_k = k + 新增数 + 删除数` 是关键：**增量层的脏数据会遮蔽真近邻**，所以必须按脏数据量放大 K 再归并。

**③ 的 WAL 暴力**（`rust/worker/src/execution/operators/knn_log.rs:100`）：`BinaryHeap::with_capacity(fetch)`，逐条 `SignedRoaringBitmap::contains` 判定是否命中过滤（`rust/worker/src/execution/operators/knn_log.rs:107`）——注意这里是**点查位图**，没有物化成 `Vec`。

复杂度对照：

| 路径 | 时间 | 空间 | 何时用 |
|------|------|------|--------|
| HNSW 图检索 | `O(ef · log N · d)` | `O(N·d + N·M)` | 常态 |
| SPANN 两级 | `O(nprobe·log C·d + nprobe·split·d)` | `O(C·d + N·d)`，`C`=簇心数 | 大规模 |
| 暴力（未 compaction WAL） | `O(L·d)` | `O(L·d)` | 写入到下一次 compaction 之间 |
| 暴力（删除退化） | `O(n·d)`，`n<100` | 同上 | 小索引高删除率 |

## 二十五、距离度量的工程约定：Cosine ≡ 归一化后的 IP、L2 不开方与 RaBitQ 无偏估计（算法展开）

### 25.1 三个度量的真实定义：Cosine 与 IP 是同一个式子

`rust/distance/src/types.rs:217` 定义了全部三种度量。关键在于**标量回退分支的真实计算**：

```rust
// rust/distance/src/types.rs:286-290  Euclidean
let mut sum = 0.0;
for i in 0..a.len() { sum += (a[i] - b[i]).powi(2); }
sum                                       // ← 没有 sqrt！返回的是平方欧氏距离

// rust/distance/src/types.rs:339-345  Cosine
// For cosine we just assume the vectors have been normalized, since that
// is what our indices expect.
let mut sum = 0.0;
for i in 0..a.len() { sum += a[i] * b[i]; }
1.0_f32 - sum                             // ← 就是 1 - 内积

// rust/distance/src/types.rs:394-398  InnerProduct
let mut sum = 0.0;
for i in 0..a.len() { sum += a[i] * b[i]; }
1.0_f32 - sum                             // ← 与 Cosine 逐字相同
```

三个结论：

1. **Cosine 与 InnerProduct 在代码里是同一个表达式** `1 - ⟨a,b⟩`。语义差别全靠"向量是否已归一化"承载——这就是 audit 维度里「余弦相似度需预归一化才能用 IP 加速」的**代码级证据**。真正的余弦 $\cos\theta = \frac{\langle a,b\rangle}{\|a\|\|b\|}$ 被省略了，因为源码注释直说索引期待的就是已归一化的向量。
2. **Euclidean 返回平方值**，不开方。对排序（top-k）而言 $\sqrt{\cdot}$ 单调，等价；但如果用户自己读 `distance` 字段做阈值判断，阈值语义是"距离的平方"。AVX 版本同理（`rust/distance/src/distance_avx.rs:307` 起的 `euclidean_distance`，累加 `sub*sub` 后直接 `hsum256_ps_avx` 求和返回，无 `sqrt`）。
3. 三个度量都被转成**"越小越相似"**的 distance 语义（`1 - sim`），所以下游所有 top-k 代码可以无脑用 min-heap / 小顶堆。

### 25.2 归一化的责任边界：谁在什么时候 normalize

归一化函数（`rust/distance/src/lib.rs:27`）：

```rust
pub fn normalize(vector: &[f32]) -> Vec<f32> {
    let norm = vector.iter().map(|x| x * x).sum::<f32>().sqrt();
    vector.iter().map(|x| x / (norm + 1e-32)).collect()   // ← 1e-32 防 0 除
}
```

调用点分布是**不对称**的，这点最容易踩：

| 阶段 | 位置 | 是否归一化 |
|------|------|-----------|
| SPANN 写入 | `rust/index/src/spann/types.rs:1634-1638` | `space == Cosine` 时归一化后存入 posting list |
| SPANN 量化写入 | `rust/index/src/spann/quantized_spann.rs:528` | 同上，**且先归一化再旋转**（旋转保范数） |
| 分布式 HNSW 写入 | `rust/segment/src/distributed_hnsw.rs:218` | ❌ 直接 `merged_embeddings_ref()` |
| HNSW 查询算子 | `rust/worker/src/execution/operators/knn_hnsw.rs:52-54` | ✅ `DistanceFunction::Cosine` 才 normalize |
| WAL 暴力查询（查询向量） | `rust/worker/src/execution/operators/knn_log.rs:93` | ✅ 同上 |
| WAL 暴力查询（库向量） | `rust/worker/src/execution/operators/knn_log.rs:116-117` | ✅ 同上 |
| 小索引暴力兜底（**库向量**） | `rust/segment/src/local_hnsw.rs:336-341` | ✅ 对**取出的库向量**再归一化 |

**⚠️ 关键陷阱**：写入路径（`rust/segment/src/distributed_hnsw.rs:218`）**不归一化**，查询路径（`rust/worker/src/execution/operators/knn_hnsw.rs:52`）**只归一化查询向量**。这意味着 cosine 语义下 HNSW 建图时用的是未归一化向量（长向量会主导邻接关系），而查询时用归一化查询向量——**图的质量与查询的度量不一致**。而嵌入式暴力兜底路径（`rust/segment/src/local_hnsw.rs:338`）却对库向量做了归一化，两条路径给出不同结果。这不是 bug（hnswlib 内部对 cosine 空间有约定），但确实是需要在生产中实测验证的行为差异。

### 25.3 SIMD 分派：编译期 cfg × 运行期 feature_detected

距离计算是 Chroma 唯一真正下探到汇编层的地方，做法是**两阶段分派**：

```text
编译期（rust/distance/src/lib.rs:7-24）
  #[cfg(all(target_feature="avx512f", "avx512dq", "avx512bw", "avx512vl", "fma"))]
      pub use distance_avx512::*;
  #[cfg(all(target_feature="avx", "fma"))]           pub use distance_avx::*;
  #[cfg(target_feature = "neon")]                    pub use distance_neon::*;
  #[cfg(all(target_feature="sse", not(target_feature="avx")))]
                                                      pub use distance_sse::*;

运行期（rust/distance/src/types.rs:240-285 等）
  if std::arch::is_x86_feature_detected!("avx512f") && ... { return avx512::cosine_distance(a,b); }
  ...
  // 全部不命中 → 落到标量 for 循环
```

注意这是**编译期选定实现集 + 运行期再挑一次**的双保险：编译期 `cfg` 保证不会把不存在的指令编进来，运行期 `is_*_feature_detected!` 处理同一二进制要在不同 CPU 上跑的情况。

AVX 实现是 **4 路展开 + FMA**（每轮处理 32 个 float）：

```rust
// rust/distance/src/distance_avx.rs:217 起
sum256_1 = _mm256_fmadd_ps(_mm256_loadu_ps(ptr1),      _mm256_loadu_ps(ptr2),      sum256_1);
sum256_2 = _mm256_fmadd_ps(_mm256_loadu_ps(ptr1.add(8)),  _mm256_loadu_ps(ptr2.add(8)),  sum256_2);
sum256_3 = _mm256_fmadd_ps(_mm256_loadu_ps(ptr1.add(16)), _mm256_loadu_ps(ptr2.add(16)), sum256_3);
sum256_4 = _mm256_fmadd_ps(_mm256_loadu_ps(ptr1.add(24)), _mm256_loadu_ps(ptr2.add(24)), sum256_4);
```

欧氏距离同理，但先做 `_mm256_sub_ps` 再 FMA 平方累加（`rust/distance/src/distance_avx.rs:330-332`）。两者尾部都会先 `hsum256_ps_avx` 归约四个累加器（`rust/distance/src/distance_avx.rs:250-253` 对 cosine、`:339-342` 对 euclidean），再补一个标量循环处理 `n % 32` 的余数，最后 `1.0 - result` / `result` 返回（`rust/distance/src/distance_avx.rs:255-258`、`:343-346`）。

`rust/index/Cargo.toml:39,43` 还同时依赖 `simsimd` 与 `faer` 两个 SIMD 库，但 `rust/distance/src/types.rs` 的分派链**只用自研的 4 个文件**，这两个库目前服务于其他路径。

### 25.4 量化下的距离估计：RaBitQ 无偏估计量

量化 SPANN 用的是 **RaBitQ（RaBit Quantization）** 的扩展版本，核心不是近似内积，而是**无偏估计量**（`rust/index/src/quantization/mod.rs:1`）。

记号（引自 `rust/index/src/quantization/mod.rs:9-17`）：

| 符号 | 含义 |
|------|------|
| `c` | 簇心 |
| `d` | 原始数据向量 |
| `r = d - c` | 数据残差 |
| `q` | 查询向量 |
| `r_q = q - c` | 查询残差 |
| `n = r / ‖r‖` | 归一化的数据残差 |
| `g` | 网格点（`n` 的方向近似） |

存储 4 个量（`rust/index/src/quantization/mod.rs:19-26`）：打包码 `bytes`（编码 `g`）、`correction = ⟨g, n⟩`、`norm = ‖r‖`、`radial = ⟨r, c⟩`。

查询时的估计式（`rust/index/src/quantization/mod.rs:32-34`）：

```text
⟨n, r_q⟩ ≈ ⟨g, r_q⟩ / ⟨g, n⟩
```

其中 `⟨g, r_q⟩` 在查询时由打包码算出，`⟨g, n⟩` 就是存下来的 `correction`。展开 `⟨d, q⟩`（`rust/index/src/quantization/mod.rs:124-129`）：

```rust
// rust/index/src/quantization/mod.rs:124-129
let r_dot_r_q = norm * g_dot_r_q / correction;      // ⟨r, r_q⟩ ≈ ‖r‖·⟨g,r_q⟩/⟨g,n⟩
let d_dot_q   = c_dot_q + radial + r_dot_r_q;      // ⟨d, q⟩ = ⟨c,q⟩ + ⟨r,c⟩ + ⟨r,r_q⟩
let d_norm_sq = c_norm * c_norm + 2.0 * radial + norm * norm;   // ‖d‖²
```

再按度量转成 distance（`rust/index/src/quantization/mod.rs:131-135`）：

```rust
match distance_fn {
    Cosine       => 1.0 - d_dot_q / (d_norm_sq.sqrt() * q_norm).max(f32::EPSILON),
    Euclidean    => d_norm_sq + q_norm * q_norm - 2.0 * d_dot_q,
    InnerProduct => 1.0 - d_dot_q,
}
```

数据向量之间的估计（`rabitq_distance_code`，`rust/index/src/quantization/mod.rs:145`）源码直接引用了论文定理：

```rust
// rust/index/src/quantization/mod.rs:161-166
// Theorem 3.2:
//     ⟨o, q⟩ = E[⟨o¯, q⟩ / ⟨o¯, o⟩]
//     Where the error is bounded by O(1/√D) with high probability.
//     Namely, as D → ∞, the error approaches 0.
```

**误差界 $O(1/\sqrt{D})$ 随维度 $D$ 增大而收敛**——这正是量化后仍能保持高召回的原因。

代价是每条记录的头部开销：`Code<1>` 头 **16 字节**（含 `signed_sum`），`Code<4>` 头 **12 字节**（`rust/index/src/quantization/mod.rs:77-80`）。所以对低维向量（如 64 维 float32 = 256 字节）而言，12-16 字节的头部占了 5-6% 的空间。

### 25.5 度量选择的工程决策表

| space | 是否需调用方预归一化 | 与其他度量的关系 | 图构建特点 | 主要陷阱 |
|-------|--------------------|----------------|----------|---------|
| `l2`（默认） | 否 | 与 `ip` 在单位球面上等价 | 距离度量唯一满足三角不等式的，可用度量空间索引剪枝 | 返回的是**平方值**（`rust/distance/src/types.rs:288`），阈值语义要乘回去 |
| `cosine` | ✅ Chroma 内部在查询侧归一化 | **代码上与 `ip` 逐字相同**（`rust/distance/src/types.rs:345` vs `rust/distance/src/types.rs:398`） | 未归一化写入时建图退化（见 §25.2） | 跨索引/跨版本搬运会丢归一化前提 |
| `ip` | ❌ 不归一化才有意义 | 见上 | 长向量天然占优 | 与 cosine 混用会得到静默错误的结果 |

> ⚠️ **重要更正**：本 wiki §8.2 写「**COSINE（默认）**」，§22 陷阱 3 也基于这个前提。1.5.x 实测默认值是 **`l2`**——Python 段 `hnsw:space` 默认 `"l2"`（`chromadb/segment/impl/vector/hnsw_params.py:56`），`EmbeddingFunction.default_space()` 默认 `"l2"`（`chromadb/api/types.py:904-906`），SPANN 侧 `default_space_spann()` 也是 `Space::L2`（`rust/types/src/spann_configuration.rs:75-77`）。三处一致。旧节原文按红线不改，但**调参时必须以 `l2` 为起点**。

## 二十六、过滤的执行位置：Filter → Knn(HNSW ∥ WAL) → Merge → Project 与 RoaringBitmap 有符号位图（算法展开）

### 26.1 1.5.x 是纯 pre-filter

源码里画了完整的查询流水线（`rust/worker/src/execution/orchestration/knn_filter.rs:161-189`，ASCII 图在源注释中）：

```text
    ┌────────────────────┐
    │  FetchLogOperator  │   从 WAL 拉未 compaction 的记录
    └─────────┬──────────┘
              ▼
    ┌────────────────────┐
    │   FilterOperator   │   ★ 过滤在这里发生，先于任何向量检索
    └─────────┬──────────┘
              ▼
              ┌───────────────┐
              │               │
        ┌─────┴──────┐        │   两条 KNN 支路并行：
        │            │        │
        ▼            ▼        │
   KnnLogOperator  KnnHnswOperator
  （暴力扫 WAL）   （HNSW 图检索）
        └─────┬──────┘        │
              ▼               │
     KnnMergeOperator ─────────┘   双指针归并
              ▼
     KnnProjectionOperator            回填 metadata / document
```

**要点**：过滤发生在第 2 步，HNSW 拿到的是一个"允许/禁止集合"，然后才做图检索。这与旧节 §8.6 描述的 pre-filter 一致，但旧节没有指出**1.5.x 已经不再提供 post-filter 路径**——`Filter` 的输出只有两种位图，没有"先 ANN 再过滤"的算子。

### 26.2 SignedRoaringBitmap：Include / Exclude 双态表示

这是 Chroma 1.5.x 过滤层的核心数据结构（`rust/types/src/signed_rbm.rs:30`）：

```rust
pub enum SignedRoaringBitmap {
    Include(RoaringBitmap),   // 「是这些 id」
    Exclude(RoaringBitmap),   // 「不是这些 id」
}
```

`full()` = `Exclude(空)`，`empty()` = `Include(空)`（`rust/types/src/signed_rbm.rs:36-42`），`flip()` 互换两者（`rust/types/src/signed_rbm.rs:52`）。

**为什么要有 Exclude 态**：同一个 `where` 条件，取反后候选集大小可能天差地别：

| 查询 | 语义 | 稀疏表示 | 存储量级 |
|------|------|---------|---------|
| `{"year": {"$gt": 1990}}` | 命中绝大多数 | `Include(大部分 id)` | **巨大** |
| `{"year": {"$lt": 1990}}` | 命中极少数 | `Exclude(大部分 id)` | 同样巨大——但换 `Exclude(少数 id)` | **很小** |

RoaringBitmap 对连续段用 `Array` 容器、稀疏段用 `Bitmap` 容器，压缩比很高。用 `Include/Exclude` 择小存储，是把"过滤谓词的补集往往更稀疏"这一经验直接编码进类型里。

**⚠️ 代价**：压缩优势在 HNSW 边界被丢掉——`rust/worker/src/execution/operators/knn_hnsw.rs:39-49` 把位图**完全物化**成 `Vec<usize>`：

```rust
let (allowed, disallowed) = match &input.compact_offset_ids {
    SignedRoaringBitmap::Include(rbm) if rbm.is_empty() => return Ok(Default::default()),
    SignedRoaringBitmap::Include(rbm) => (rbm.iter().map(|o| o as usize).collect(), Vec::new()),
    SignedRoaringBitmap::Exclude(rbm) => (Vec::new(), rbm.iter().map(|o| o as usize).collect()),
};
```

这是因为 `hnswlib` 的 `query` 接口签名只接受 `&[usize]`（`rust/index/src/hnsw.rs:190-200`）。所以**高选择性的 pre-filter 会在这里产生一次 O(候选数) 的物化分配**。反过来，WAL 侧的暴力路径不需要物化——它直接 `rbm.contains(offset_id)` 点查（`rust/worker/src/execution/operators/knn_log.rs:107`）。

### 26.3 双来源求值：WAL 与 compacted segment 分开过滤再取交

`Filter` 同时挂两个元数据提供者（`rust/worker/src/execution/operators/filter.rs:244`）：

```rust
pub(crate) enum MetadataProvider<'me> {
    CompactData(
        &'me MetadataSegmentReaderShard<'me>,
        &'me Option<RecordSegmentReaderShard<'me>>,
        &'me RecordSegmentReaderOptions,
        usize, // bruteforce_candidate_limit
    ),
    Log(&'me MetadataLogReader<'me>),
}
```

`MetadataLogReader` 是一个**为过滤临时搭的内存倒排索引**，布局刻意与 metadata segment 对齐（`rust/worker/src/execution/operators/filter.rs:103-114`）：

```rust
pub(crate) struct MetadataLogReader<'me> {
    // This maps metadata keys to `BTreeMap`s, which further map values to offset ids
    // This mimics the layout in the metadata segment
    compact_metadata: HashMap<String, BTreeMap<MetadataValue, RoaringBitmap>>,
    document: HashMap<u32, &'me str>,
    updated_offset_ids: RoaringBitmap,        // 日志中被改动过的 id
    user_id_to_offset_id: HashMap<&'me str, u32>,
}
```

完整求值伪代码（对齐 `rust/worker/src/execution/operators/filter.rs:790-876`）：

```python
def filter(query_ids, where_clause, logs, metadata_seg, record_seg, bruteforce_limit):
    materialized = materialize_logs(record_seg, logs)
    log_reader = MetadataLogReader.create(materialized, record_seg)

    # ── 短路：无任何过滤条件 ────────────────────────────
    if query_ids is None and where_clause is None:
        return Output(
            log_offset_ids     = full(),                       # WAL 全要
            compact_offset_ids = full() - log_reader.updated,  # segment 全要，扣掉 WAL 改过的
        )

    log_provider     = MetadataProvider.Log(log_reader)
    compact_provider = MetadataProvider.CompactData(
        metadata_seg, record_seg, plan, bruteforce_limit)

    # ── ① query_ids（按用户 id 过滤）────────────────────
    user_log, user_compact = (
        (Include(log_reader.search_user_ids(query_ids)),
         Include(record_seg.get_offset_id_for_user_id(id) for id in query_ids))
        if query_ids else (full(), full())
    )

    # ── ② where_clause 在 WAL 上求值 ────────────────────
    log_offset_ids = (where_clause.eval(log_provider) & user_log) if where_clause else user_log

    # ── ③ where_clause 在 compacted segment 上求值 ───────
    #    恒定扣掉所有出现在日志里的 id：日志覆盖 segment（旧值被新值遮蔽）
    compact_offset_ids = (
        (where_clause.eval(compact_provider) & user_compact
         - log_reader.updated_offset_ids)
        if where_clause
        else (user_compact - log_reader.updated_offset_ids)
    )
    return Output(log_offset_ids, compact_offset_ids)
```

两个设计要点：

1. **短路路径不是免费的**（`rust/worker/src/execution/operators/filter.rs:790`）：即使没有过滤条件，`compact_offset_ids` 也必须减去 `updated_offset_ids`，否则同一文档会从两个来源各出一份。
2. **日志恒定压过 segment**（`rust/worker/src/execution/operators/filter.rs:867`、`rust/worker/src/execution/operators/filter.rs:870`）：WAL 里出现过的 id 一律从 compacted 结果中剔除。这让"同一 id 在两处存在"的状态永远解析为"以 WAL 为准"，与 compaction 的合并语义一致。

And/Or 的组合在树上直接做位运算（`rust/worker/src/execution/operators/filter.rs:700-707`）：

```rust
BooleanOperator::And => child_evaluations.fold(SignedRoaringBitmap::full(),    BitAnd::bitand),
BooleanOperator::Or  => child_evaluations.fold(SignedRoaringBitmap::empty(),   BitOr::bitor),
```

### 26.4 `$contains` 的三阶段与 50000 兜底

`$contains`（文档子串匹配）走一条特殊路径，因为 Blockfile 上的全文索引只能给**候选集**，给不了确定答案（`rust/worker/src/execution/operators/filter.rs:255`）：

```text
Stage 1  trigram 索引        ──→ 候选集（可能有大量假阳性）
Stage 2  token bitmap 索引   ──→ 候选集（分词后倒排，更精确）
Stage 3  暴力逐条 doc.contains(query) 校验  ← 关键
```

三阶段的设计是"索引给候选 + 原文校验保精确"。校验有**预算上限**（`bruteforce_candidate_limit`，默认 **50 000**，见 `rust/worker/src/config.rs:406-408`）：

```rust
// rust/worker/src/execution/operators/filter.rs:308-321
// Stage 3: brute-force verification against actual documents.
// Budget: verify up to `bruteforce_candidate_limit` candidates.
// Brute-force throughput varies with document length (~1M
// docs/sec for short docs, ~300K for long). Candidates beyond
// the budget are included unverified (include-all) to preserve
// recall at the cost of precision.
// TODO: cursor-based pagination could continue brute-force
// verification across multiple query rounds, improving
// precision for large candidate sets without exceeding the
// per-round latency budget.
```

超预算的候选**直接无条件放行**（`rust/worker/src/execution/operators/filter.rs:322-350`）：

```rust
let limit = *bruteforce_limit;
let to_verify:  Vec<u32>         = candidates.iter().take(limit).collect();   // 预算内 → 逐条校验
let unverified: RoaringBitmap   = candidates.iter().skip(limit).collect();   // 预算外 → 全部放行
// ... 并发拉取文档 → doc.contains(query) 过滤 ...
result |= unverified;   // ← include-all 保召回
```

这是向量库在过滤层的典型兜底权衡：**宁可多返回（假阳性），不可漏返回（假阴性）**。因为下游还有 ANN 排序，用户通常只取 top-k，多出来的候选未必进入结果。

注释里的吞吐数据（短文档 ~100 万/秒，长文档 ~30 万/秒）也解释了为什么阈值是 5 万：单次查询多花 50-170 ms 的 CPU 就到顶了。源码留了 TODO：将来用 cursor 分页跨多轮校验，把精度换回来。

### 26.5 读隔离级别 ReadLevel：把「一致 vs 延迟」做成 API 旋钮

过滤能不能看到刚写的 WAL 条目，取决于读级别（`rust/types/src/execution/plan.rs:553-564`）：

```rust
pub enum ReadLevel {
    /// Read from both the index and the write-ahead log (default).
    /// Provides full consistency with all committed writes visible.
    #[default]
    IndexAndWal,
    /// Read only from the index, skipping the write-ahead log.
    /// Provides eventual consistency - recent uncommitted writes may not be visible.
    IndexOnly,
    /// Read from the index and up to a server-configured number of write-ahead
    /// log entries. Provides a consistent prefix of the WAL with bounded query
    /// latency: recently committed writes beyond the limit may not be visible.
    IndexAndBoundedWal,
}
```

```python
def read_logs(read_level, bounded_wal_limit):
    match read_level:
        case IndexOnly:          limit = 0                                    # 跳过 WAL
        case IndexAndWal:        limit = None          # 读满 compaction 边界 # rust/worker/src/execution/orchestration/count.rs:173
        case IndexAndBoundedWal: limit = bounded_wal_limit   # 默认 250 条    # rust/worker/src/execution/orchestration/count.rs:182-193
```

`bounded_wal_limit` 默认 **250**（`rust/worker/src/config.rs:402-404`）。

对照表：

| 级别 | 读 WAL | 延迟 | 可见性 | 适用 |
|------|-------|------|--------|------|
| `IndexAndWal`（默认） | 全量到 compaction 边界 | 高（随 backlog 线性增长） | 所有已提交写入 | 写后读、事务性需求 |
| `IndexAndBoundedWal` | ≤ 250 条 | 有界 | 一致前缀 | 延迟敏感的读多场景 |
| `IndexOnly` | 否 | 最低 | 落后于 compaction | 可容忍陈旧的批处理 |

> ⚠️ **重要更正**：本 wiki §10 写「一致性级别：默认**最终一致**（向量写入异步持久化）」。1.5.x 的默认是 `IndexAndWal`，**所有已提交写入立即可见**（`rust/types/src/execution/plan.rs:554-556` 注释明写 "full consistency with all committed writes visible"）。最终一致是**可选**（`IndexOnly`），不是默认。旧节原文按红线不改。

## 二十七、Segment 生命周期与 WAL：Fragment / Manifest / Snapshot 与 fork-open-flush 合并（算法展开）

### 27.1 Segment 类型与身份：URN 化的类型 + 文件路径分离

Segment 在 1.5.x 是**元数据里的一行记录**，类型用 URN 表达（`rust/types/src/segment.rs:130-159`）：

```rust
pub enum SegmentType {
    BlockfileMetadata, BlockfileRecord,
    HnswDistributed, HnswLocalMemory, HnswLocalPersisted,
    Sqlite,
    Spann, QuantizedSpann,
}
// → "urn:chroma:segment/vector/hnsw-distributed"
//   "urn:chroma:segment/vector/spann"
//   "urn:chroma:segment/metadata/blockfile" ...
```

注意 **`HnswLocalMemory` / `HnswLocalPersisted` / `HnswDistributed` 是三个并列变体**——旧节 §6 的架构图把它们画成三种不同的 Segment 类，方向对，但 1.5.x 更进一步：分布式 HNSW 不再是独立实现，而是把「HNSW 索引文件」当成一种 segment payload 存到对象存储，类型区分的是**文件的持久化形态**而非算法。

Segment 记录本体（`rust/types/src/segment.rs:181-188`）：

```rust
pub struct Segment {
    pub id: SegmentUuid,
    pub r#type: SegmentType,
    pub scope: SegmentScope,                     // 分片范围
    pub collection: CollectionUuid,
    pub metadata: Option<Metadata>,
    pub file_path: HashMap<String, Vec<String>>, // ← 文件路径单独一层
}
```

`file_path` 是 `HashMap<String, Vec<String>>`（`rust/types/src/segment.rs:187`）——**key 是 segment 类型，value 是该类型下的文件列表**。这让"同一个 segment 类型在分片场景下有多个文件"成为可能，也方便 `empty_segment()`（`rust/types/src/segment.rs:193`）在重建时清空路径而保留身份。

### 27.2 wal3 的三层对象模型：Fragment / Manifest / Snapshot

Chroma 的 WAL（`rust/wal3/`）不是简单的追加文件，而是一个**三层结构**：

```text
Fragment   不可变的连续追加块（底层 blob，可含乱码）
   ▲ 被引用
Manifest   可变的清单：指向一批 Fragment；每次写入都被覆盖
   ▲ 被快照
Snapshot   不可变、内容寻址（路径即 setsum）：压缩掉一大段 Manifest 里的指针
```

设计意图写在 `rust/wal3/src/manifest.rs:1-6`：

```rust
//! Manifests, and their immutable cousin, snapshots, are the metadata that describe a log.
//! A manifest transitively names every file in object storage that is part of the log.
//! Snapshots are content-addressable and immutable, while manifests get overwritten.  For that
//! reason, manifests embed the ETag for conditional writes while snapshots do not.
```

**关键差异**：Manifest 可覆盖 → 用 **ETag 条件写**保证并发安全（乐观并发）；Snapshot 内容寻址 → 天然幂等，不需要条件写。

**LogPosition**（`rust/wal3/src/lib.rs:379`）是一个严格递增、无空洞的 u64 时间戳：

```rust
pub struct LogPosition {
    /// The offset field of a LogPosition is a strictly increasing timestamp.  It has no gaps and
    /// spans [0, u64::MAX).
    offset: u64,
}
impl LogPosition {
    pub const MAX: LogPosition = LogPosition { offset: u64::MAX };
    pub const MIN: LogPosition = LogPosition { offset: u64::MIN };
    pub fn contains_offset(start: LogPosition, end: LogPosition, offset: u64) -> bool {
        start.offset <= offset && offset < end.offset
    }
}
```

**Fragment**（`rust/wal3/src/lib.rs:920`）是一段连续写入，带 setsum（内容校验和）：

```rust
/// A Fragment is an immutable piece of the log containing adjacent writes.
pub struct Fragment {
    pub path: String,
    pub seq_no: FragmentIdentifier,   // SeqNo(u64) 或 Uuid
    pub start: LogPosition,
    pub limit: LogPosition,
    pub num_bytes: u64,
    pub setsum: Setsum,
}
pub fn possibly_contains_position(&self, position: LogPosition) -> bool {
    LogPosition::contains_offset(self.start, self.limit, position.offset)
}
```

**Manifest**（`rust/wal3/src/manifest.rs:251`）持有快照指针 + fragment 列表，外加 `acc_bytes`（累计字节）与 `collected`（GC 已收集的 setsum）。

### 27.3 写入 → 快照 → 回收

**快照生成**（`generate_snapshot`，`rust/wal3/src/manifest.rs:292`）是自底向上的滚降：

```python
def generate_snapshot(manifest, opts, writer):
    # ① 尝试滚降已有快照指针
    depth = max(s.depth for s in manifest.snapshots) or 0
    while depth > 0:
        picked = 从后往前收集 depth 相同层的快照，直到 picked >= opts.snapshot_rollover_threshold
        if len(picked) >= opts.snapshot_rollover_threshold:
            # 若第 0 个快照与 picked[0] 首尾相接且层级更低，一并吸收
            return Snapshot(depth=depth+1, snapshots=picked, setsum=Σsetsum)
        depth -= 1

    # ② 快照指针够不到阈值时，改从 fragment 层建
    if len(manifest.fragments) >= opts.fragment_rollover_threshold:
        picked = []
        for frag in manifest.fragments:
            # ⚠️ 永远保留 seq_no 最大的那个 fragment
            if frag.seq_no == max_seq_no:      # 见下方注释
                continue
            picked.append(frag)
            if len(picked) >= opts.fragment_rollover_threshold: break
        if len(picked) >= opts.fragment_rollover_threshold:
            return Snapshot(depth=1, fragments=picked, setsum=Σsetsum)
    return None
```

「永远保留最后一个 fragment」的理由写在 `rust/wal3/src/manifest.rs:343-351`：

```rust
// NOTE(rescrv):  When taking a snapshot, it's important that we keep around
// one fragment so that the max seq no is always calculable.
// Otherwise, a low-traffic log could be compacted into a state where all of
// its fragments have been compacted and therefore the implicit fragment seq no
// for each fragment is zero.  This wedges the manifest manager.
```

阈值是**按 JSON 序列化字节数倒推**的（`rust/wal3/src/lib.rs:524-536`）：

```rust
fn default_snapshot_rollover_threshold() -> usize { (1 << 18) / SnapshotPointer::JSON_SIZE_ESTIMATE }  // 256 KiB / 256B = 1024
fn default_fragment_rollover_threshold() -> usize { (1 << 19) / Fragment::JSON_SIZE_ESTIMATE }          // 512 KiB / 256B = 2048
```

即 **Manifest 文件大小恒定在 ~768 KiB**，与日志总量无关。

**应用快照**（`rust/wal3/src/manifest.rs:382`）先 `scrub()` 自检、逐个比对 fragment 是否一致，再 `split_off` 掉已被快照覆盖的前缀，并压入一个 `SnapshotPointer`。`apply_snapshot` **首尾各调一次 `scrub()`**（`rust/wal3/src/manifest.rs:383` 和 `rust/wal3/src/manifest.rs:417`），保证前后都不变。

**三阶段 GC**（`rust/wal3/src/lib.rs:1056-1080`）把「计算 → 更新元数据 → 删数据」拆开，是分布式对象存储上安全 GC 的标准做法：

```python
def garbage_collect(...):
    garbage = phase1_compute_garbage()      # 纯计算：不写任何东西
    new_manifest = phase2_update_manifest(garbage)   # 只改 Manifest（ETag 条件写）
    phase3_delete_garbage(garbage)          # 才真删对象；失败可重放，因为 manifest 已不含它们
```

对应 Rust 接口：`garbage_collect_phase1_compute_garbage`（`rust/wal3/src/lib.rs:1056`）、`garbage_collect_phase2_update_manifest`（`rust/wal3/src/lib.rs:1063`）、`garbage_collect_phase3_delete_garbage`（`rust/wal3/src/lib.rs:1073`）。

**顺序不变式**：任何写入位置 `p` 存在一个 fragment `F` 使 `F.possibly_contains_position(p)`；`Manifest::round_to_boundary`（`rust/wal3/src/manifest.rs:510`）把任意位置向下取整到最近的 fragment 起点——这正是子模块 HEAD 那个 commit `[ENH](worker): Widen async fn windows to the furthest fitting compaction boundary (#7621)` 在用的原语。

### 27.4 HNSW 索引的 fork / open / create / flush：COW 式合并

HNSW 索引的生命周期由 `HnswIndexProvider` 管理（`rust/index/src/hnsw_provider.rs:52`）。四个动词对应四种场景：

| 方法 | 何时用 | 语义 | 行号 |
|------|-------|------|------|
| `create` | 段里还没有任何索引文件 | 全新建一个 id 分配 UUID | `rust/index/src/hnsw_provider.rs:381` |
| `open` | 查询命中一个已持久化的 id | 从 S3 拉 4 个文件 → 反序列化 | `rust/index/src/hnsw_provider.rs:296` |
| `fork` | compaction 要在已有索引上继续写 | **克隆数据 + 分配新 UUID** | `rust/index/src/hnsw_provider.rs:168` |
| `flush` | compaction 结束 | 把内存索引序列化回 4 个文件 | `rust/index/src/hnsw_provider.rs:427` |

`fork` 是最关键的（`rust/index/src/hnsw_provider.rs:168`）：

```text
fork(source_id, ...)
  │
  ├─ new_id = Uuid::new_v4()                    # rust/index/src/hnsw_provider.rs:177 副本是新身份
  ├─ fetch_hnsw_segment(source_id)             # rust/index/src/hnsw_provider.rs:186 从 S3 并发拉 FILES 的 4 个 buffer
  ├─ get(new_id, cache_key) 二次检查           # rust/index/src/hnsw_provider.rs:196 等待期间别人可能已加载完
  ├─ spawn_blocking(|| HnswIndex::load_from_hnsw_data(...))   # rust/index/src/hnsw_provider.rs:211 阻塞加载丢到独立线程池
  ├─ cache.insert(cache_key, index)            # rust/index/src/hnsw_provider.rs:235
  └─ return HnswIndexRef
```

**为什么用 `fork` 而不是 `open` 后就地改**：`open` 返回的 `HnswIndexRef` 可能正在被并发查询使用；compaction 若直接往里加向量，会与查询竞争。`fork` 出一个新 UUID 后，**旧索引保持不变直到 compaction 成功**，语义上就是 copy-on-write。失败就丢弃副本，零副作用。

`create` 需要锁，因为索引初始化和文件操作不是原子的（`rust/index/src/hnsw_provider.rs:394-398`）：

```rust
// We take a lock here to synchronize concurrent creates of the same index.
// Otherwise, we could end up with a corrupted index since the filesystem
// operations are not guaranteed to be atomic.
// The lock is a partitioned mutex to allow for higher concurrency across collections.
let _guard = self.write_mutex.lock(&id).await;
```

`commit()` 是个**空实现**（`rust/index/src/hnsw_provider.rs:421-425`）：

```rust
pub fn commit(&self) -> Result<(), Box<dyn ChromaError>> {
    // We are using direct HNSW and we don't need to commit since we're going off the in-memory index.
    // Each preceding write will have already made changes to the in-memory index.
    Ok(())
}
```

因为内存索引本身就是真相来源——**直接 HNSW（`use_direct_hnsw = true`）模式下没有中间缓冲，落盘粒度就是 `flush` 那一次**。

落盘格式是 hnswlib 的 4 个文件（`rust/index/src/hnsw_provider.rs:29-34`）：

```rust
pub const FILES: [&str; 4] = ["header.bin", "data_level0.bin", "length.bin", "link_lists.bin"];
```

其中 `data_level0.bin` 是第 0 层全部向量（最大），`link_lists.bin` 是所有层的邻接表。存储 key 形如 `{prefix}/hnsw/{index_uuid}/{file}`（`rust/index/src/hnsw_provider.rs:160-166`）。

**compaction 调度的选谁策略**（`rust/worker/src/compactor/scheduler_policy.rs:33-47`）是 **LAS（longest since last compaction）**：

```rust
impl SchedulerPolicy for LasCompactionTimeSchedulerPolicy {
    fn determine(&self, collections: Vec<CollectionRecord>, number_jobs: i32) -> Vec<CompactionJob> {
        collections.sort_by_key(|a| a.last_compaction_time);   // 最早压的排前面
        // 取前 min(number_jobs, len) 个出 job
    }
}
```

这是为了**公平性**：谁最久没被压就压谁，防止持续写入的 collection 饿死其他 collection。

### 27.5 缓存与锁策略

**缓存 key 是 collection 而非 index**（`rust/index/src/hnsw_provider.rs:36-50`）：

```rust
type CacheKey = CollectionUuid;
// The key of the cache is the collection id and the value is
// the HNSW index for that collection. This restricts the cache to
// contain atmost one index per collection. Ideally, we would like
// this index to be the latest index for that collection but rn it
// is not guaranteed. For e.g. one case could be:
// 1. get index version v1
// 2. get index version v2 (> v1)
// 3. get index version v1 (can happen due to an inflight query
//    that started before compaction of v2 occured) -- this will
//    evict v2 even though it is more recent and will be used again in future.
```

源码注释诚实承认了一个竞态：**一个在 compaction 前发起的慢查询可能把旧版本重新塞回缓存，挤掉更新的版本**。`get()` 会比对 `hnsw_index.id == *index_id`，不匹配就返回 `None` 走重载路径（`rust/index/src/hnsw_provider.rs:145-158`），所以只是性能损失不会返回错数据。

**锁结构**是三层：

```text
Arc<dyn Cache<CollectionUuid, HnswIndexRef>>        # 跨 collection 的 LRU
  └── HnswIndexRef { inner: Arc<RwLock<DistributedHnswInner>> }   # rust/index/src/hnsw_provider.rs:67
        └── AysncPartitionedMutex<IndexUuid>          # 跨索引的分段异步互斥
```

单索引内的读写用 `parking_lot::RwLock`，且**扩容用了 upgradable_read**（`rust/segment/src/distributed_hnsw.rs:220-231`）：

```rust
let mut index = self.index.inner.upgradable_read();       // 多读单升级
let index_len     = index.hnsw_index.len_with_deleted();
let index_capacity = index.hnsw_index.capacity();
if index_len + 1 > index_capacity {
    index.with_upgraded(|index| {
        // Bump allocation by 2x
        index.hnsw_index.resize(index_capacity * 2)      // 独占写，只在需要时拿
    })?;
}
match index.hnsw_index.add(record.get_offset_id() as usize, embedding) { ... }
```

这是标准的 **read-mostly + 罕见升级** 模式：绝大多数 add 不扩容，只持读锁；扩容时才短暂独占。批量写入则直接用 `rayon` 并行（`rust/segment/src/local_hnsw.rs:808-809` 的 `hnsw_batch.into_par_iter()`）。

**分段异步互斥**（`rust/index/src/hnsw_provider.rs:138`）：

```rust
write_mutex: AysncPartitionedMutex::with_parallelism(permitted_parallelism as usize, ())
```

`permitted_parallelism` 默认 **180**（`rust/index/src/config.rs:29-31`），注释解释了为什么是 `parallelism²` 个分区：

```rust
// This is the number of collections that can be loaded in parallel
// without contending with each other.
// Internally the number of partitions of the partitioned mutex
// that is used to synchronize concurrent loads is set to
// permitted_parallelism * permitted_parallelism. This is
// inspired by the birthday paradox.
```

**分区数 ≈ 并发度²** 是在用**生日悖论**做负载分散：若哈希均匀，`n` 个并发请求落到 `p` 个分区的碰撞概率是 `O(n²/p)`，取 `p = n²` 使碰撞概率 `O(1)`。

**缓存权重按 MB 计**（`rust/index/src/hnsw_provider.rs:110-126`）：

```rust
impl chroma_cache::Weighted for HnswIndexRef {
    fn weight(&self) -> usize {
        let bytes = index.hnsw_index.len() * size_of::<f32>() * index.hnsw_index.dimensionality();
        let as_mb = bytes / 1024 / 1024;
        if as_mb == 0 { 1 } else { as_mb }      // 至少占 1 个单位，防止 0 权重条目永不被淘汰
    }
}
```

**嵌入式模式另有 FD 管理**（`rust/segment/src/local_segment_manager.rs`）：LRU 池容量 65536（`rust/segment/src/local_segment_manager.rs:21`），淘汰时通过**事件监听器关闭文件描述符**（`rust/segment/src/local_segment_manager.rs:58-63`）：

```rust
let handle = tokio::spawn(async move {
    while let Some((_, index)) = rx.recv().await {
        index.close().await;       // Close the FD here.
    }
});
```

嵌入式模式下 HNSW 是 mmap 式常驻的，FD 泄漏会直接耗尽进程句柄，所以淘汰必须绑一个异步 close。

## 二十八、Collection Schema 与 Embedding Function：把模型编排纳入数据模型（算法展开）

### 28.1 Schema 的两级结构：defaults + keys

1.5.x 把「collection 怎么建」从 metadata 提升成了一等 schema（`rust/types/src/collection_schema.rs:233-244`）：

```rust
/// This represents the server-side schema structure used for index management
pub struct Schema {
    /// Default index configurations for each value type
    pub defaults: ValueTypes,
    /// Key-specific index overrides
    pub keys: HashMap<String, ValueTypes>,
    /// Customer-managed encryption key for collection data
    pub cmek: Option<Cmek>,
}
```

- `defaults`：按**值类型**（`float_list` / `sparse_vector` / `string` …）给默认索引配置；
- `keys`：按**具体 key** 覆盖，特殊 key `#embedding` 就是 embedding 向量（访问器 `embedding_vector_index_mut`，`rust/types/src/collection_schema.rs:355`）。

一次更新要**同时改两处**（`rust/types/src/collection_schema.rs:329-343`）：

```rust
if let Some(ref ef) = config.embedding_function {
    self.defaults_vector_index_mut()?  .config.embedding_function = Some(ef.clone());
    self.embedding_vector_index_mut()?.config.embedding_function = Some(ef.clone());
}
```

**更新能力是不对称的**（`rust/types/src/collection_schema.rs:280-285`）：

```rust
// HNSW updates are not allowed
if config.hnsw.is_some() {
    return Err(SchemaError::InvalidConfigurationUpdate {
        message: "HNSW configuration updates are not supported".to_string(),
    });
}
```

只有 SPANN 的 `search_nprobe` / `ef_search` 和 embedding_function 配置允许热更新（`rust/types/src/collection_schema.rs:301-306`、`rust/types/src/collection_schema.rs:329-343`）；HNSW 参数改不了。原因是 HNSW 图结构已经物化在索引文件里，改 `M` 意味着全量重建，而 SPANN 的 nprobe 只是查询期参数。

### 28.2 EmbeddingFunction 是六方法协议

Python 侧 `EmbeddingFunction` 是一个 **Protocol**（`chromadb/api/types.py:831`），必须实现的方法：

| 方法 | 签名 | 基类默认 | 作用 |
|------|------|---------|------|
| `__call__(input)` | `input: D → Embeddings` | 必须实现 | 实际推理 |
| `name()` | `→ str` | `NotImplemented` + 警告 | **身份标识**，写入 schema |
| `build_from_config(config)` | `→ EmbeddingFunction` | `NotImplemented` + 警告 | 反序列化重建 |
| `get_config()` | `→ Dict` | `NotImplemented` + 警告 | 序列化 |
| `default_space()` | `→ Space` | **`"l2"`** | 决定索引度量 |
| `supported_spaces()` | `→ List[Space]` | **`["cosine","l2","ip"]`** | 合法性白名单 |
| `validate_config(config)` | `→ None` | 空实现 | 参数校验 |
| `validate_config_update(old, new)` | `→ None` | 空实现 | 升级校验 |
| `is_legacy()` | `→ bool` | 见下 | 兼容判定 |

`is_legacy()`（`chromadb/api/types.py:948-954`）的判定很直接——**三个必需方法任一未实现即为 legacy**：

```python
def is_legacy(self) -> bool:
    if (self.name() is NotImplemented
        or self.get_config() is NotImplemented
        or self.build_from_config(self.get_config()) is NotImplemented):
        return True
```

Rust 侧对应三态枚举（`rust/types/src/collection_configuration.rs:35-42`）：

```rust
pub enum EmbeddingFunctionConfiguration {
    #[serde(rename = "legacy")]  Legacy,                    // 旧客户端，服务端不知道是哪个模型
    #[serde(rename = "known")]   Known(EmbeddingFunctionNewConfiguration),
    #[serde(rename = "unknown")] Unknown,                    // 客户端知道、服务端不认识的扩展
}
```

`is_default()`（`rust/types/src/collection_configuration.rs:45-51`）判定"是否就是内置的 default EF"：`Legacy → false`，`Unknown → true`，`Known(c) → c.name == "default"`。

这三种态的实际后果是：**`Legacy` 的 collection 服务端无法保证 query 时的 embedding 与写入时同源**——这是向量库最隐蔽的正确性风险，用户换模型顺序会静默污染索引。

### 28.3 建 collection 时的 space 消解顺序

`chromadb/api/collection_configuration.py:370-441` 完整实现了优先级消解：

```python
ef = config.get("embedding_function")
if ef.is_legacy():
    ef_config = {"type": "legacy"}                      # ← 短路，不碰 space
else:
    # ① 若既没给 hnsw_config 也没给 spann_config，且 metadata 里没有 hnsw:space
    #    → 用 EF 的 default_space 造一个 hnsw_config
    if hnsw_config is None and spann_config is None:
        if metadata is None or metadata.get("hnsw:space") is None:
            hnsw_config = CreateHNSWConfiguration(space=ef.default_space())   # chromadb/api/collection_configuration.py:384

    # ② 已有 config 但没给 space → 从 EF 补
    if hnsw_config is not None and hnsw_config.get("space") is None:
        hnsw_config["space"] = ef.default_space()        # chromadb/api/collection_configuration.py:388
    if spann_config is not None and spann_config.get("space") is None:
        spann_config["space"] = ef.default_space()       # chromadb/api/collection_configuration.py:390

    # ③ 校验 space 是否在 EF 支持列表里 —— 只是 warn，不拒绝
    if hnsw_config.get("space") not in ef.supported_spaces():
        warnings.warn(f"space {...} is not supported by {ef.name()}")          # chromadb/api/collection_configuration.py:396

    ef_config = {"name": ef.name(), "type": "known", "config": ef.get_config()}  # chromadb/api/collection_configuration.py:422-426
    register_embedding_function(type(ef))                 # chromadb/api/collection_configuration.py:427  ← 注册到进程级表
```

**优先级链**（`chromadb/api/collection_configuration.py:444-449` 的 `populate_create_hnsw_defaults` 是其简化版）：

```text
显式 config["space"]  >  metadata["hnsw:space"]  >  EF.default_space()  >  "l2"
```

⚠️ 注意 ③ 步只是 **`warnings.warn`，不抛异常**（`chromadb/api/collection_configuration.py:394-406`）。也就是说 `OpenAIEmbeddingFunction` 配 `space="ip"` 是允许的——只是警告。反过来，**默认路径下你用哪个 EF 就默认拿到哪种度量**：`SentenceTransformerEmbeddingFunction` 不覆写 `default_space()` 就拿 `"l2"`，而很多句向量模型是 L2 归一化后再算余弦的（此时 `cosine` 与 `l2` 等价），所以默认值"碰巧"是对的——但这是巧合，不是有意设计。

**内置 EF 数量**：`chromadb/utils/embedding_functions/` 下有 **32 个** `.py` 模块（含 `__init__.py` / `config_validation.py` / `utils.py` / `schemas/`），覆盖 OpenAI / Cohere / HuggingFace / SentenceTransformer / Jina / Mistral / Voyage / Bedrock / Google / Ollama / instructor / open_clip / nomic / morph 等，以及 **BM25 / SPLADE 稀疏 EF**（`bm25_embedding_function.py`、`chroma_cloud_splade_embedding_function.py`）。

### 28.4 双轨参数路径：config 优先，metadata 兼容

0.4 时代的 `hnsw:` 前缀 metadata **仍然完整可用**（`chromadb/segment/impl/vector/hnsw_params.py:10-23`）：

```python
param_validators: Dict[str, Validator] = {
    "hnsw:space":           lambda p: bool(re.match(r"^(l2|cosine|ip)$", str(p))),
    "hnsw:construction_ef": lambda p: isinstance(p, int),
    "hnsw:search_ef":       lambda p: isinstance(p, int),
    "hnsw:M":               lambda p: isinstance(p, int),
    "hnsw:num_threads":     lambda p: isinstance(p, int),
    "hnsw:resize_factor":   lambda p: isinstance(p, (int, float)),
}
persistent_param_validators: Dict[str, Validator] = {
    "hnsw:batch_size":     lambda p: isinstance(p, int) and p > 2,
    "hnsw:sync_threshold": lambda p: isinstance(p, int) and p > 2,
}
```

校验失败即抛 `ValueError`（`chromadb/segment/impl/vector/hnsw_params.py:40-43`），**未知 key 也会报错**——这意味着往 metadata 里塞 `hnsw:anything_new` 会直接失败。

两轨的差异：

| 维度 | metadata 轨（`hnsw:`） | schema 轨（`CreateHNSWConfiguration`） |
|------|----------------------|-----------------------------------|
| 载体 | collection 的 metadata dict | SysDB 的 `schema` JSON |
| 可改 | 建库后不能改 | SPANN 的 nprobe / ef_search 可改 |
| HNSW 参数 | 全支持 | ❌ 不支持（`rust/types/src/collection_schema.rs:281`） |
| 与 EF 的联动 | 无 | `space` 由 `EF.default_space()` 兜底 |

**迁移建议**：新代码走 schema 轨；老 collection 的 `hnsw:` metadata 不要删（删了会用默认值重建索引，召回会掉）。

## 二十九、参考资料

- [Chroma GitHub](https://github.com/chroma-core/chroma)
- [Chroma 官方文档](https://docs.trychroma.com/)
- [Chroma Cloud](https://www.trychroma.com/)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [SPANN 论文](https://proceedings.neurips.cc/paper/2021/hash/483cc4d2ec1d8bd19c5dd3b8a2e08e3c-Abstract.html)
- [PyO3 绑定文档](https://pyo3.rs/)
- [RAFT 论文](https://raft.github.io/raft.pdf)
- [Jégou PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [ACORN filtered HNSW](https://arxiv.org/abs/2403.04871)