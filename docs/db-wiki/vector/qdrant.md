# Qdrant Wiki（Rust 实现的云原生向量数据库）

> 参考版本：Qdrant 1.x（Rust + Actix-web + RocksDB），面向生产环境的 **Rust 云原生向量数据库**，主打 **Payload 过滤 + HNSW + 量化 + 分布式**。本 wiki 重点覆盖向量类型特有章节（HNSW + Payload Filter、量化、Rust 实现细节）。

## 一、项目定位

Qdrant（读作 "quadrant"）是由 **Andrey Vasnetsov / Qdrant 公司**开源的 **Rust 实现的云原生向量数据库**，提供 **gRPC + REST API、Payload 过滤（标量/地理/JSON）、HNSW、量化（Scalar + Product + Binary）、分布式部署** 等能力，是当前生产环境最常用的开源向量数据库之一。

- **痛点**：
  - Python 嵌入式向量库（Chroma / LanceDB）无法支撑高并发生产；
  - 分布式向量数据库（Milvus）部署复杂、运维成本高；
  - 需要 **向量检索 + 标量过滤 / 地理过滤 / JSON 过滤** 的无缝融合；
  - 需要 **Rust 的内存安全 + 高性能** 优势；
  - 中等规模（百万-亿级）向量 + 丰富 Payload 的业务场景；
- **典型用户**：RAG 应用、AI 知识库、电商推荐、语义搜索、广告检索、生物制药、AI Agent、IoT 数据；
- **核心口号**：**"Vector Search with Filters"** + **"Production-grade vector database"** + **"Written in Rust"**。

## 二、发展历史与社区

- **起源**：2020 年由 Andrey Vasnetsov（前 Wasabi Hot Cloud Storage 工程师）发起，初为个人项目，后成立 Qdrant 公司；
- **里程碑**：
  - 2020-12：v0.1 开源，HNSW + 基础 API；
  - 2021-04：v0.5 引入 **Payload 索引**（integer / keyword / geo）；
  - 2021-09：v0.7 引入 **Quantization（Scalar + Product）**；
  - 2021-12：v0.10 引入 **Distributed Mode**（Raft）；
  - 2022-06：v1.0 GA，**生产可用**；
  - 2022-12：v1.2 引入 **Quantization on-disk**；
  - 2023-06：v1.4 引入 **GPU 实验性支持**；
  - 2024-02：v1.7 引入 **Sparse Vector**；
  - 2024-08：v1.10 引入 **Multivector**（ColBERT 风格）、**Binary Quantization（1-bit）**；
  - 2025-2026：持续强化 **Vector + BM25 全文 + 多模态**；
- **维护状态**：极活跃，月度小版本，季度大版本，GitHub 20k+ star；
- **社区**：Discord 5k+ 用户、Forum、Reddit r/rust 高活跃、年度 Qdrant 大会。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**；
- **商业版本**：
  - **Qdrant Cloud**（AWS / GCP / Azure / 阿里云多区域全托管）；
  - **Hybrid Cloud**（企业版，支持 K8s Operator）；
- **生态关系**：
  - 与 **Rust 生态**（tokio、actix-web）紧耦合；
  - 与 **LangChain / LlamaIndex / Haystack** 是默认向量后端之一；
  - 与 **Cohere / OpenAI / HuggingFace / FastEmbed** 通过客户端集成嵌入。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Microsoft**：Bing 视觉检索部分；
  - **Discord**：内容审核 embedding 检索；
  - **Bosch**：工业 RAG；
  - **Roblox**：游戏内容检索；
  - **JD.com**：电商搜索；
  - 国内：阿里、字节、网易、知乎、平安等；
- **行业**：电商推荐、AI RAG、AI Agent、内容平台、广告、生物制药、金融风控、语义搜索；
- **典型场景**：RAG 知识库、商品检索、跨模态文-图检索、欺诈检测 embedding、推荐系统召回。

## 五、一句话对比定位

> Qdrant 是 **"Rust 云原生 + HNSW + Payload 过滤 + 多种量化（Scalar/Product/Binary）+ gRPC/REST + 分布式 + 多向量 + 稀疏向量"** 的生产级向量数据库；与 **Milvus（Go + C++ + 分布式强）**、**Weaviate（Go + 模块化）**、**Chroma（Python 嵌入式）**、**LanceDB（嵌入式列存）**、**Vespa（Java/C++ 搜索平台）**、**Pinecone（商业全托管）**、**FAISS（算法库）**、**USearch（嵌入式 C++）** 构成向量库阵营。Qdrant 的差异化是 **"Rust 高性能 + 单二进制部署 + Payload 过滤极致优化 + 分布式友好"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层                                     │
│  - Python: qdrant-client（官方推荐）                                 │
│  - Rust / Go / Java / TypeScript / C# / Elixir                      │
│  - REST API + gRPC API                                              │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ gRPC (Port 6333-6334) / REST (Port 6333)
┌─────────────────────▼───────────────────────────────────────────────┐
│                    API 服务层（Actix-web）                             │
│  src/actix_api/                                                      │
│  ├─ REST handlers（tonic / actix-web）                             │
│  ├─ gRPC handlers（tonic）                                          │
│  ├─ OpenAPI 文档                                                    │
│  └─ Auth（API Key / JWT / RBAC）                                   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Collection / Shard 层                                │
│  lib/collection/src/                                                │
│  ├─ CollectionManager                                          │
│  ├─ ShardReplicaSet / Shard / ShardProxy                     │
│  ├─ UpdateQueue（写入路径）                                        │
│  ├─ Searcher / Query Planner                                   │
│  └─ Consensus（Distributed 模式：Raft via raft-rs）             │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Segment 层                                          │
│  lib/segment/src/                                                    │
│  ├─ Segment（向量 + Payload 段）                                   │
│  ├─ Vector Storage（HNSW + 量化存储）                              │
│  ├─ Payload Storage（基于 RocksDB-like MDB）                      │
│  ├─ Payload Index（integer / keyword / float / geo / text）       │
│  ├─ IdTracker / Deleted Points                                    │
│  └─ Quantizer（Scalar / Product / Binary）                        │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  索引底层（Rust crates）                              │
│  lib/segment/src/index/                                              │
│  ├─ hnsw_index（HNSW 图索引）                                      │
│  ├─ plain_payload_index（标量 Payload）                            │
│  └─ struct_payload_index（JSON 路径）                             │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  存储后端                                             │
│  - RocksDB（Segment 元数据 / Payload 索引）                          │
│  - 自研 MDB（in-memory 嵌入式 KV）                                  │
│  - 文件系统（向量数据 / 索引二进制）                                │
│  - S3 / GCS / Azure（云模式，备份）                                 │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **Rust 单语言**：全栈 Rust（API + 索引 + 存储），无 CGO；
- **单二进制部署**：一个二进制同时担任 API + 存储 + 索引；
- **Payload 一等公民**：向量 + 任意 JSON Payload 同时索引和过滤；
- **WAL**：每个 Segment 自带 WAL（`lib/wal/`），崩溃恢复；
- **量化默认开启**：Scalar Quantization（int8）默认开启，内存降低 4x；
- **HNSW + Payload 过滤**：核心场景，过滤 + 向量检索深度融合；
- **分布式（可选）**：通过 Raft + 分片实现，非默认。

## 七、存储引擎

### 7.1 向量存储

- **Segment 内向量存储**：
  - 内存模式：raw vector (float32)；
  - 量化模式：int8 / PQ-encoded / binary；
- 文件格式：`storage/vector_data.rs` 自研二进制格式；
- **mmap**：向量可 mmap 加载，避免全量内存占用。

### 7.2 RocksDB-like Payload 存储

- Qdrant 自研 `MDB`（Memory-mapped Database）替代 RocksDB：
  - 源码：`lib/segment/src/payload_storage/`；
  - LSM-Tree 风格，KV 接口；
  - 用于存 **Payload 索引** + **文档 / ID 映射**；
- 也可选 RocksDB（向后兼容）。

### 7.3 WAL（Write-Ahead Log）

- 每个 Segment 自带 WAL（`lib/wal/`）；
- 写入先 WAL，后异步合并到 Segment；
- 崩溃恢复：从 WAL 重放未提交记录；
- **Trififo**：`lib/trififo/` 三层 FIFO 缓存，连接 WAL 与 Segment。

### 7.4 元数据存储

- Collection schema / shard 元数据 / cluster topology：
  - 单机模式：JSON 配置文件 + 本地 RocksDB-like；
  - 分布式模式：Raft 共识（raft-rs）。

## 八、索引机制（**向量类型核心**）

### 8.1 索引体系全景

Qdrant 提供两大类索引：**向量索引** 与 **Payload 索引**，二者通过 **Graph pre-filter** 深度融合。

| 索引类型 | 文件位置 | 备注 |
|---------|---------|------|
| **HNSW** | `lib/segment/src/index/hnsw_index/` | 向量图索引 |
| **PlainPayload** | `lib/segment/src/index/plain_payload_index/` | 标量 Payload |
| **StructPayload** | `lib/segment/src/index/struct_payload_index/` | JSON 路径 Payload |
| **Mmap HNSW** | `lib/segment/src/index/hnsw_index/` | 磁盘 HNSW |

### 8.2 HNSW 详解

**Qdrant 的 HNSW 实现位置**：

```
lib/segment/src/index/hnsw_index/
├── hnsw.rs                  # 入口
├── hnsw_builder.rs          # 构建器
├── hnsw_iterator.rs         # 迭代式构建
├── graph_layers.rs          # 分层图
├── graph_link.rs            # 图边（紧凑存储）
├── graph_link_allocator.rs  # 内存分配器
├── searches.rs              # 搜索逻辑
├── point_ids.rs             # ID 集合
└── ...
```

**参数**：

| 参数 | 默认 | 含义 |
|------|------|------|
| `m` | 16 | 每节点最大连接数 |
| `ef_construct` | 100 | 构建候选队列 |
| `ef` | 128 | 搜索候选队列 |
| `full_scan_threshold` | 10000 | < 此值用暴力 |
| `max_indexing_threads` | 0 | 自动 |

**距离度量**：

- **Cosine**（默认）；
- **Euclid**（L2）；
- **Dot**（内积）；
- **Manhattan**（L1）。

**距离计算**：`lib/spaces/` 内含 SIMD 优化（SSE / AVX / AVX-512）。

### 8.3 HNSW + Payload 过滤（Qdrant 杀手锏）

**问题**：HNSW 检索时如何跳过被 Payload 过滤掉的节点？

**Qdrant 的解决方案（多策略）**：

```
┌─────────────────────────────────────────────────────────────┐
│  策略 1：Payload-first Filter（默认）                        │
│  1. 在 Payload Index 中评估 filter                           │
│  2. 得到 ID 集合（白名单 / 黑名单）                          │
│  3. HNSW 搜索时只访问白名单内的节点                          │
│  4. 性能：过滤率高时 recall 接近无过滤                       │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  策略 2：HNSW-iterative + Re-check                          │
│  1. HNSW 检索时按图遍历返回候选                             │
│  2. 候选数 ≥ ef * filter_factor（默认 2-5）                 │
│  3. 在 Payload 中 re-check 过滤                             │
│  4. 性能：过滤率低时高效                                     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  策略 3：Graph pre-filter（Qdrant 自研）                    │
│  1. 在 HNSW 图遍历中实时跳过一个过滤节点                    │
│  2. 类似 ACORN 思路                                         │
│  3. 实现：graph_layers.rs 中 visited HashSet 跳过 filter   │
│  4. 性能：recall 接近无过滤，延迟略增                       │
└─────────────────────────────────────────────────────────────┘
```

**自动策略选择**（`lib/segment/src/index/hnsw_index/`）：
- **Cardinality-based**：根据过滤结果集大小自动选最佳策略；
- **Sampling-based**：用采样估算最优参数。

### 8.4 量化方案

| 量化 | 压缩比 | 精度 | 适用 |
|-------|--------|------|------|
| **Scalar (int8)** | 1/4 | 99%+ | 默认 |
| **Scalar (int4)** | 1/8 | 95%+ | 大数据 |
| **Product Quantization (PQ)** | 1/16 ~ 1/32 | 90%+ | 千万-亿级 |
| **Binary Quantization (1-bit)** | 1/32 | 80%+ | 超大规模（实验） |
| **TurboQuant**（v1.16+）| 1/32 | 95%+ | 新方案 |
| **Reranker** | 不压缩 | 100% | 精排 |

**实现位置**：
- `lib/quantization/src/`：
  - `scalar.rs`：标量量化；
  - `product.rs`：PQ 量化；
  - `binary.rs`：1-bit 二值化；
  - `turboquant/`：Google TurboQuant 集成；
- `lib/segment/src/quantizations/`：Segment 集成层。

**量化 + HNSW 工作流**：
1. 训练阶段：从原始向量训练量化码本；
2. 写入阶段：原始向量量化后存入 Segment，原始向量可丢弃（可配置保留）；
3. 查询阶段：用量化向量做 HNSW 检索 → top-K → 用原始向量（若有）或量化向量 re-rank。

### 8.5 多向量（Multivector，v1.10+）

- **场景**：ColBERT / ColPali 等 late-interaction 模型；
- **API**：`vector: { colbert: [vec1, vec2, ...] }`；
- **索引**：HNSW 索引存每组多向量的中心，查询时做 **MaxSim** 打分；
- **实现**：`lib/segment/src/index/hnsw_index/`。

### 8.6 稀疏向量（Sparse Vector，v1.7+）

- **场景**：SPLADE / BM25 sparse embedding；
- **API**：`vector: { sparse: { indices: [...], values: [...] } }`；
- **索引**：基于倒排 + 评分（类似 SPLADE 论文）；
- **实现**：`lib/sparse/src/`，`lib/segment/src/index/sparse_index/`。

### 8.7 Payload 索引（非向量但常配合）

| Payload 字段类型 | 索引类型 | 文件 |
|----------------|---------|------|
| **integer** | Bitmap + Value | `plain_payload_index/` |
| **keyword** | Bitmap + Value | `plain_payload_index/` |
| **float** | Value Range | `plain_payload_index/` |
| **geo** | Bounding Box / H3 | `geo_index/` |
| **text** | 倒排 + Posting List | `lib/text_index/` |
| **bool** | Bitmap | `plain_payload_index/` |
| **datetime** | Range + Bitmap | `plain_payload_index/` |
| **JSON path** | 嵌套索引 | `struct_payload_index/` |

**API 示例**：

```python
client.search(
    collection_name="docs",
    query_vector=[0.1, 0.2, ...],
    query_filter=Filter(
        must=[
            FieldCondition(key="category", match=MatchValue(value="book")),
            FieldCondition(key="price", range=Range(lte=100.0)),
        ]
    ),
    limit=10,
)
```

### 8.8 全文检索（BM25 + 向量融合）

- **BM25 索引**：`lib/bm25/`（基于 Tantivy / 自研）；
- **混合检索**：向量 + BM25 RRF 融合；
- **API**：

```python
client.search(
    collection_name="docs",
    query_text="What is Chroma?",
    search_params={"hybrid": {"fusion": "rrf", "vector_weight": 0.7}}
)
```

## 九、查询/计算/接口层

### 9.1 REST API（端口 6333）

```
GET    /collections
POST   /collections/{name}/points/search
POST   /collections/{name}/points/scroll
POST   /collections/{name}/points/upsert
POST   /collections/{name}/points/delete
PUT    /collections/{name}/index
POST   /collections/{name}/points/recommend
POST   /collections/{name}/points/discover
```

OpenAPI 文档自动生成。

### 9.2 gRPC API（端口 6334）

- 高性能场景；
- 完整定义：`lib/api/src/grpc/`；
- proto 生成。

### 9.3 SDK 生态

| 语言 | 包名 | 备注 |
|------|------|------|
| Python | `qdrant-client` | 官方推荐 |
| Rust | `qdrant-rust` | 官方 |
| Go | `qdrant-go` | 官方 |
| Java | `qdrant-java` | 官方 |
| TypeScript / JS | `@qdrant/qdrant-js` | 官方 |
| C# | `Qdrant.Client` | 官方 |
| Elixir | `qdrant-ex` | 社区 |
| Ruby / PHP / Scala | 社区 | HTTP |

### 9.4 多租户

- **Collection 隔离**：每个 Collection 独立索引；
- **Metadata 命名空间**：Collection metadata 用于租户标记；
- **RBAC**（企业版）：Collection 级别权限；
- **Shard**：通过分片隔离资源。

### 9.5 混合检索 + Reranker

- **RRF**：Reciprocal Rank Fusion；
- **Cross-encoder**：用户自行接入；
- **内置 Rerank**：通过 `score` 字段 + 客户端二次排序。

### 9.6 高级查询

- **Recommend API**：基于正负样本的向量检索；
- **Discover API**：从 query 向量到 target 向量的偏移；
- **Context API**：携带历史 context 调整检索；
- **Group By**：按 Payload 字段分组检索；
- **Sample**：随机采样；
- **Search Iterators**：流式分页。

## 十、事务与一致性

- **写入**：先 WAL + 内存 Buffer，异步刷盘；
- **读一致性**：默认 **最终一致**；
- **快照隔离**：每个 Segment 自带版本号；
- **多副本（Distributed）**：Raft 共识保证线性一致读；
- **崩溃恢复**：WAL 重放 + Segment 合并。

## 十一、复制与高可用

- **单机模式**：无 HA；
- **分布式模式**（v1.0+）：
  - **Raft 共识**（raft-rs crate）；
  - **Shard 分片**：Collection 自动分片到多个节点；
  - **Replica Factor**：每分片 1-N 副本；
  - **Consensus**：写入走 Raft Leader，读取可走 Follower；
- **Qdrant Cloud**：跨区域 HA + 自动备份。

## 十二、扩展性

- **垂直扩展**：增加节点内存；
- **水平扩展**：
  - **Shard 数量**动态调整（`collection.update_collection(replication_factor=2)`）；
  - **Shard Transfer**：在线迁移；
- **冷热分层**：
  - **Quantization**：默认开启 Scalar Quantization，热数据 int8；
  - **Mmap**：大索引可 mmap 到磁盘；
  - **On-disk 量化**：PQ 量化后存磁盘，仅查询时解码。

## 十三、性能与基准

- **ann-benchmarks**：Qdrant 在 SIFT-1M / GloVe-100 上排名中上；
- **HNSW 延迟**：百万级向量约 **1-5ms / query**（Rust + AVX-512）；
- **量化收益**：Scalar Quantization 内存降低 4x，召回率损失 < 1%；
- **PQ 收益**：内存降低 16x，召回率损失 2-5%；
- **Binary Quantization**：内存降低 32x，召回率损失 5-15%；
- **GPU 实验**：单卡 A100 可加速 HNSW 5-10x（v1.4+）。

## 十四、运维与可观测性

- **Prometheus**：内置 metrics endpoint `/metrics`；
- **Tracing**：OpenTelemetry；
- **结构化日志**（tracing crate）；
- **慢查询日志**：`--enable-slow-queries`；
- **Dashboard**：Grafana 官方 Dashboard；
- **备份恢复**：
  - 单机模式：文件系统快照；
  - 分布式模式：Snapshot API + S3；
- **集群迁移**：在线 shard transfer。

## 十五、生态与集成

- **AI 框架**：LangChain、LlamaIndex、Haystack 默认后端；
- **Embedding**：FastEmbed（Qdrant 出品）、Cohere、OpenAI、HuggingFace；
- **可视化**：Qdrant Web UI；
- **Operator**：Qdrant Operator（K8s）；
- **Docker / Helm**：官方支持；
- **RAG 框架**：DSPy、PromptTools；
- **迁移工具**：vec2qdrant（从其他向量库迁移）。

## 十六、核心论文与算法原理

- **HNSW**：Malkov & Yashunin, 2016；
- **PQ**：Jégou et al., 2011；
- **Scalar Quantization**：基础理论；
- **Binary Quantization**：仅基于符号的编码；
- **TurboQuant**：Google 2024 加速量化论文；
- **ACORN**（Graph pre-filter）：2024；
- **ColBERT / late interaction**：Khattab & Zaharia, 2020；
- **BM25**：经典 TF-IDF 排序；
- **RRF**：Cormack et al., 2009；
- **H3 Geo Index**：Uber 2018；
- **RAFT**：Ongaro & Ousterhout, 2014。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | API 入口 | `src/actix_api/` | REST/gRPC 入口 |
| P0 | HNSW 索引 | `lib/segment/src/index/hnsw_index/hnsw.rs` | 图索引核心 |
| P0 | Payload 索引 | `lib/segment/src/index/plain_payload_index/` | 标量 Payload |
| P1 | 量化 | `lib/quantization/src/` | Scalar / PQ / Binary |
| P1 | Segment | `lib/segment/src/segment.rs` | 段管理 |
| P1 | Collection | `lib/collection/src/collection/` | 集合管理 |
| P1 | Shard | `lib/collection/src/shards/` | 分片 |
| P2 | 分布式 Raft | `lib/collection/src/shards/local_shard.rs` | raft-rs |
| P2 | WAL | `lib/wal/` | 写前日志 |
| P2 | 多向量 | `lib/segment/src/index/hnsw_index/` | ColBERT |
| P2 | 稀疏向量 | `lib/sparse/src/` | SPLADE |

## 十八、源码规模与开发语言

- **Rust**：约 99% LOC（核心 + API + 索引 + 存储）；
- **Python**：约 1% LOC（client SDK 测试）；
- **TypeScript**：约 1% LOC（Web UI）；
- **构建系统**：Cargo workspace（多个 crate）；
- **CI**：GitHub Actions；
- **LOC 总量**：约 50 万行（含测试、benchmark）。

## 十九、学习资源

- **官方文档**：https://qdrant.tech/documentation/
- **GitHub**：https://github.com/qdrant/qdrant
- **Discord**：https://discord.gg/qdrant
- **博客**：https://qdrant.tech/blog/
- **教程**：官方 Quickstart、LangChain 集成教程；
- **HN / Reddit**：r/rust、r/MachineLearning；
- **YouTube**：Qdrant 官方频道。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 | 原生 |
| Linux ARM64 | 原生 |
| macOS x86_64 / ARM64 | 原生 |
| Windows x86_64 | 原生 |
| Docker | 官方镜像 |
| Helm / K8s | 官方 Chart + Operator |
| Qdrant Cloud | 全托管 |

## 二十一、变体与衍生项目

- **Qdrant Cloud**：官方全托管；
- **Qdrant Hybrid Cloud**：企业版（K8s 自管）；
- **FastEmbed**：Qdrant 出品的轻量嵌入 Python 库；
- **qdrant-js / qdrant-rust**：官方 SDK；
- **第三方绑定**：qdrant-elixir、qdrant-scala。

## 二十二、风险与陷阱

1. **Rust 单二进制不等于单点**：单 Qdrant 进程不支持 HA，生产必须分布式或 Qdrant Cloud；
2. **HNSW 内存占用**：百万级 float32 向量约 4-10GB 内存，建议开启 Scalar Quantization；
3. **量化召回损失**：PQ 量化召回率损失 2-5%，需要补偿（提高 ef / nprobe）；
4. **Payload 字段数量过多**：影响索引大小和构建时间；
5. **Geo 索引**：H3 精度选择影响索引大小；
6. **Mmap 误用**：大 Segment mmap 会触发 OOM Killer；
7. **分布式模式复杂度**：Raft 配置错误会导致脑裂；
8. **WAL 恢复慢**：大 Segment WAL 崩溃恢复可能数分钟；
9. **混合检索排序**：RRF / 加权融合需根据业务调参；
10. **schema 修改**：Collection schema 重建索引代价高。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 已实现部分 HNSW，可参考 Qdrant 的 HNSW + Payload Filter 设计；
- **`engineering/rag/`** 中向量后端可对接 Qdrant Standalone；
- **MiniVecDB**：参考 Qdrant 的 Payload Index + Vector Index 双层架构简化实现；
- **学习价值**：Rust 实现的高性能 ANN 是参考范例；
- **学习路径**：Qdrant 的 HNSW 实现是 Rust 学习 ANN 的优秀教材。

## 二十四、参考资料

- [Qdrant GitHub](https://github.com/qdrant/qdrant)
- [Qdrant 官方文档](https://qdrant.tech/documentation/)
- [Qdrant Cloud](https://cloud.qdrant.io/)
- [Qdrant 大学](https://qdrant.tech/learn/)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [TurboQuant 论文](https://arxiv.org/abs/2406.04636)
- [ACORN filtered HNSW](https://arxiv.org/abs/2403.04871)
- [ColBERT 论文](https://arxiv.org/abs/2004.12832)
- [raft-rs](https://github.com/tikv/raft-rs)
- [Rust SIMD 距离](https://github.com/qdrant/spaces)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)