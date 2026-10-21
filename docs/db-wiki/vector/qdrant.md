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

## 二十四、Segment 生命周期与向量存储（源码级）

> 本节全部路径相对 `reference/vector/qdrant/`（Qdrant 1.19.0，commit `74f3e85b9`）。

### 24.1 Segment 的 on-disk 布局

Qdrant 的 Segment 是一个**自描述目录**：入口状态文件名是常量 `SEGMENT_STATE_FILE = "segment.json"`（`lib/segment/src/segment/mod.rs:41`），其内容是 `SegmentConfig`（`lib/segment/src/types.rs:1720`），字段为 `vector_data: HashMap<VectorNameBuf, VectorDataConfig>` + `sparse_vector_data` + `payload_storage_type`。加载时先反序列化 `segment.json`，再按其中的 `VectorDataConfig` 逐个打开向量存储与索引目录——因此**多向量（named vectors）可以在同一个 Segment 内并存且各自独立配置量化与存储类型**。

主要子目录与常量对应关系：

| 目录 / 文件 | 常量位置 | 说明 |
|-------------|---------|------|
| `segment.json` | `lib/segment/src/segment/mod.rs:41` | 段状态 + 各向量名配置 |
| `vectors/` | `lib/segment/src/vector_storage/dense/appendable_dense_vector_storage.rs:28` | 稠密向量 mmap 分片 |
| `deleted/` | `lib/segment/src/vector_storage/dense/appendable_dense_vector_storage.rs:29` | 删除位图 |
| `hnsw_config.json` | `lib/segment/src/index/hnsw_index/config.rs:9` | 固化后的建图参数（`m`/`m0`/`ef_construct`/`ef`/`full_scan_threshold`） |
| `graph.bin` | `lib/segment/src/index/hnsw_index/graph_layers.rs:57` | 分层图元信息 |
| `links.bin` | `lib/segment/src/index/hnsw_index/graph_layers.rs:59` | 图边（`Plain` 格式） |
| `links_compressed.bin` | `lib/segment/src/index/hnsw_index/graph_layers.rs:60` | 图边（`Compressed` 格式，varint 压缩） |
| `histogram_config.json` / `histogram_borders.bin` | `lib/segment/src/index/field_index/histogram.rs:19` | 数值索引的桶边界 |

### 24.2 appendable → immutable：由 optimizer 驱动

Segment 有两种形态：`Segment::is_appendable()` 直接返回内部的 `appendable_flag`（`lib/segment/src/segment/entry.rs:302`）。所有写入路径（`link_new_point` / `search_entry` / `drop_range`）都以 `debug_assert!(self.is_appendable())` 开头自检——即**非 appendable 段在编译期语义上禁止任何修改**。

驱动两态转换的是 collection 层的 optimizer，阈值集中在 **`shard` crate** 而非 `collection` crate：

| 常量 | 值 | 位置 |
|------|----|------|
| `DEFAULT_INDEXING_THRESHOLD_KB` | `10_000` | `lib/shard/src/optimizers/config.rs:17` |
| `DEFAULT_DELETED_THRESHOLD` | `0.2` | `lib/shard/src/optimizers/config.rs:18` |
| `DEFAULT_VACUUM_MIN_VECTOR_NUMBER` | `1000` | `lib/shard/src/optimizers/config.rs:19` |

用户侧配置字段是 `OptimizersConfig::indexing_threshold`，序列化别名仍是历史名 `indexing_threshold_kb`（`lib/collection/src/optimizers_builder.rs:87`，字段本体在 `lib/collection/src/optimizers_builder.rs:89`），文档字符串明确「**1kB ≈ 1 个 256 维向量**」，默认 10000 KB 来自实测（`lib/collection/src/optimizers_builder.rs:82`）。`collection` crate 通过 `use shard::optimizers::config::{...}`（`lib/collection/src/optimizers_builder.rs:17`）引用这些常量——**1.19 已把分片/优化器逻辑从 `collection` crate 抽到独立的 `shard` crate**。

伪代码（索引化触发）：

```
if vectors_size_bytes > indexing_threshold_kb * 1024:
    # appendable 段 → 建索引（可能同时建 HNSW + 量化）→ 转 immutable
    spawn(IndexingOptimizer)     # 内部用 max_indexing_threads 条 rayon 线程
elif deleted_ratio > deleted_threshold and points > vacuum_min_vector_number:
    spawn(VacuumOptimizer)       # 只重建 payload / id_tracker，不动向量图
```

### 24.3 `VectorStorageType`：六种存储形态

`VectorStorageType`（`lib/segment/src/types.rs:1999`）把「向量放哪、能不能追加」拆成两个正交维度：

| 变体 | 落盘 | 可追加 | 用途 |
|------|------|--------|------|
| `Memory` | 否 | 是 | 全内存，最快最吃内存 |
| `Mmap` | 单文件 mmap | 否 | 加载即 mmap，按 OS page cache 决定命中 |
| `ChunkedMmap` | 分片 mmap | 是 | 追加写 + 大段 mmap，生产默认 |
| `InRamChunkedMmap` | 分片 mmap + `mlock` | 是 | 强制常驻，替代已弃用的 `Memory` |
| `InRamMmap` | 单文件 mmap | 否 | 加载时整份预取进 RAM |
| `Empty` | 无文件 | — | 不可变段上新命名向量的占位壳 |

运行时载体是 `VectorStorageEnum`（`lib/segment/src/vector_storage/vector_storage_base.rs:607`），按「稠密/多向量/稀疏/空」×「volatile / memmap / io_uring / appendable-mmap / turbo」展开成约 25 个变体；Linux 上还有 `DenseUring`（`#[cfg(target_os = "linux")]`）走 io_uring 后端。

**`on_disk` 在 1.19 已被 `Memory` 三态取代**（`lib/segment/src/types.rs:1859`）：

```
enum Memory { Cold, Cached, Pinned }
```

- `Cold`：不预加载，首次查询慢，靠使用量逐步进 page cache —— 对应旧的 `on_disk = true`；
- `Cached`：启动预加载进 RAM cache，可被逐出 —— 对应旧的 `on_disk = false`；
- `Pinned`：加载后永不逐出，必须放得下 —— 对应旧的 `always_ram = true`。

迁移映射由 `Memory::from_on_disk` 完成：`on_disk=true → Cold`，`on_disk=false → Cached`（`lib/segment/src/types.rs:1877`）。`HnswConfig::on_disk` 本身已打上 `#[deprecated(since = "1.19.0", note = "Use `memory` instead")]`（`lib/segment/src/types.rs:838`），量化侧 `ScalarQuantizationConfig::always_ram` / `ProductQuantizationConfig::always_ram` / `BinaryQuantizationConfig::always_ram` 同批弃用（分别见 `lib/segment/src/types.rs:949` / `lib/segment/src/types.rs:993` / `lib/segment/src/types.rs:1049`）。**新字段 `memory` 优先级高于弃用字段**。

### 24.4 ChunkedVectors：32MB 分片切分

`ChunkedMmap` 家族（`AppendableMmapDenseVectorStorage` 等）的切分粒度是编译期常量：

```
CHUNK_SIZE      = 32 MiB          （lib/segment/src/vector_storage/common.rs:27）
PAGE_SIZE_BYTES = 4096            （lib/segment/src/vector_storage/common.rs:16）
VECTOR_READ_BATCH_SIZE = 64       （lib/segment/src/vector_storage/common.rs:20）
```

测试构建下降到 512 KiB（`lib/segment/src/vector_storage/common.rs:23`）。分片大小在 `ChunkedVectors::create_config` 中按维度整除对齐（`lib/segment/src/vector_storage/chunked_vectors/lifecycle.rs:76`）：

```rust
chunk_size_bytes     = CHUNK_SIZE;                              // lib/segment/src/vector_storage/chunked_vectors/lifecycle.rs:76
vector_size_bytes    = dim * size_of::<T>();                    // lib/segment/src/vector_storage/chunked_vectors/lifecycle.rs:77
chunk_size_vectors   = chunk_size_bytes / vector_size_bytes;    // lib/segment/src/vector_storage/chunked_vectors/lifecycle.rs:78  整数除法，可能有余
corrected_bytes      = chunk_size_vectors * vector_size_bytes;  // lib/segment/src/vector_storage/chunked_vectors/lifecycle.rs:79  向下取整到整向量
```

**设计要点**：`corrected_chunk_size_bytes` 保证「一个 chunk 永远由整数个向量构成」，否则 mmap 的 slice 边界会与向量边界错位，SIMD 的 `for_each_payload_block` 就无法按块无拷贝读取。命中 `dim=0` 直接报 `service_error("The vector's dimension cannot be 0")`。

分片的价值：单文件 mmap 的问题是**写会触发整段文件的重映射**，`ChunkedMmap` 把「小段高频写 + 大段只读」分离，追加只落当前 tail chunk，已封口的历史 chunk 天然只读。批打分时按 `VECTOR_READ_BATCH_SIZE = 64` 个向量一批把 ID 拉成连续字节，避免逐点 `pread`。

### 24.5 HNSW 图的磁盘布局：level 0 不排序

`graph_links/mod.rs` 顶部的模块文档（`lib/segment/src/index/hnsw_index/graph_links/mod.rs:45`）用图示给出了 `links.bin` 的序列化布局，核心是两条不直观的规则：

```
links offset (level > 0) = level_offsets[level] + offsets[reindex[point_id]]
```

1. **level 0 的邻接表保持插入顺序，不排序**；level ≥ 1 的邻接表按 point_id 升序存储。原因是 level 0 的候选遍历是热路径，顺序写入对 cache 友好，而上层邻接表条目少、需要二分定位。
2. **reindex 表**：所有层共享一张 `point_id → 排序后下标` 的置换表。插入时 point_id 是递增分配的，所以 `reindex[i] = i - 已被删除的点数`，删除后再做一次线性压缩。这样 offset 表就是紧凑的连续数组，mmap 后可直接按下标随机访问。

图边格式有三种（`GraphLinksFormat`）：`Plain`（`links.bin`，定长 u32）、`Compressed`（`links_compressed.bin`，varint）、`CompressedWithVectors`（`links_comp_vec.bin`，额外内联原始+量化向量，用磁盘换随机读）。读取侧 `GraphLinksView` 是**零拷贝解析器**——它借用 owner（RAM 字节或 mmap 句柄）的字节，热路径上不做动态分发。

---

## 二十五、HNSW 检索与量化实现

### 25.1 HNSW 配置的默认值与「透传链」

`HnswConfig`（`lib/segment/src/types.rs:814`）的 `Default` 实现（`lib/segment/src/types.rs:1645`）给出四个关键默认值：

| 字段 | 默认 | 位置 |
|------|------|------|
| `m` | `16` | `lib/segment/src/types.rs:1648` |
| `ef_construct` | `DEFAULT_HNSW_EF_CONSTRUCT = 100` | `lib/segment/src/types.rs:1643` / `lib/segment/src/types.rs:1649` |
| `full_scan_threshold` | `DEFAULT_FULL_SCAN_THRESHOLD = 10_000` | `lib/segment/src/types.rs:2321` / `lib/segment/src/types.rs:1650` |
| `max_indexing_threads` | `0`（自动） | `lib/segment/src/types.rs:1651` |

**1.19 的一处语义变更必须注意**：`full_scan_threshold` 的 serde 别名是 `full_scan_threshold_kb`（`lib/segment/src/types.rs:826`），即**它的单位是 KB 而不再是「向量条数」**，注释写明「1Kb = 1 个 256 维向量」。建图时会被换算成向量条数并固化进 `hnsw_config.json`（`lib/segment/src/index/hnsw_index/hnsw/build.rs:96`）：

```rust
full_scan_threshold = hnsw_config.full_scan_threshold * 1024          // KB → 字节
                      / (available_bytes / total_vector_count)        // 字节 → 等效向量条数
```

`HnswGraphConfig::new`（`lib/segment/src/index/hnsw_index/config.rs:34`）在此基础上固化三个派生量（`lib/segment/src/index/hnsw_index/config.rs:47`）：

```
m0         = m * 2                      // level 0 出度翻倍（经典 HNSW 做法）
ef         = ef_construct               // 搜索束宽默认继承建图束宽
payload_m0 = payload_m.map(|v| v * 2)   // payload 专用图同上
```

注意 `ef` **只在建图时初始化一次**，运行期 `SearchParams::hnsw_ef` 未指定时就回落到 `self.config.ef`（`lib/segment/src/index/hnsw_index/hnsw/read_view/search.rs:39`），所以调 `ef` 必须走改配置 + 重建索引的路径，而非单次查询参数。

多租户隔离靠「**每个 Collection 独立一份 `HnswConfig` + 独立 Segment 目录**」实现：`HnswConfig` 序列化进 Collection 配置（`lib/segment/src/types.rs:814`），建图时按 `vector_name` 逐个取（`SegmentConfig::quantization_config`，`lib/segment/src/types.rs:1736`），因此不同 collection 可以用完全不同的 `m` / 量化组合，互不影响。

### 25.2 建图：level 采样、启发式剪枝、多线程

**level 采样**（`lib/segment/src/index/hnsw_index/graph_layers_builder.rs:388`）严格是 HNSW 论文的指数分布：

```
level_factor = 1 / ln(max(m, 2))            // graph_layers_builder.rs:320
sampled_level = round(-ln(U(0,1)) * level_factor)   // graph_layers_builder.rs:394
```

**启发式剪枝**（HNSW 论文 Algorithm 4 `SELECT-NEIGHBORS-HEURISTIC`）在 `LinksContainer::fill_from_sorted_with_heuristic`（`lib/segment/src/index/hnsw_index/links_container.rs:47`）：

```rust
'outer: for candidate in candidates {          // candidates 按距离升序
    for &existing in &self.links {
        // 关键剪枝条件：candidate 与 existing 的距离 > candidate 与 query 的距离
        //   ⇒ candidate 被 existing「遮挡」，丢弃
        if score(candidate.idx, existing) > candidate.score {
            continue 'outer;
        }
    }
    self.links.push(candidate.idx);
    if self.links.len() >= level_m { break; }   // 收满 M 条即停
}
```

该启发式由编译期常量 `HNSW_USE_HEURISTIC = true` 全局开启（`lib/segment/src/index/hnsw_index/hnsw.rs:30`），由 `link_new_point_on_level` 分派到 `link_with_heuristic`（`lib/segment/src/index/hnsw_index/graph_layers_builder.rs:532`）或 `link_without_heuristic`（`lib/segment/src/index/hnsw_index/graph_layers_builder.rs:558`）。

**复杂度**：剪枝的期望时间由朴素 `O(ef_construct · M)` 降到 `O(ef_construct · log M)` 量级，代价是连通性下降——这是 recall 与建图速度的经典权衡点。

**多线程建图**（`lib/segment/src/index/hnsw_index/hnsw/build.rs:184`）：

```rust
num_entries = max(1, total_vector_count / full_scan_threshold * 10)  // lib/segment/src/index/hnsw_index/hnsw/build.rs:184
//                 ↑ 插入线程找到「已有邻居」时用的搜索束宽
let pool = rayon::ThreadPoolBuilder::new()
    .thread_name(|idx| format!("hnsw-build-{idx}"))
    .num_threads(permit.num_cpus as usize)              // lib/segment/src/index/hnsw_index/hnsw/build.rs:199
    .spawn_handler(|thread| { /* Linux 上 linux_low_thread_priority() */ ... });
```

线程命名带 `hnsw-build-{idx}` 前缀便于监控，Linux 上还会把建图线程降到低调度优先级，避免抢占在线查询流量。小数据量则退化为单线程：`SINGLE_THREADED_HNSW_BUILD_THRESHOLD = 32`（`lib/segment/src/index/hnsw_index/hnsw.rs:36`；测试构建为 256，`lib/segment/src/index/hnsw_index/hnsw.rs:38`），不超过该阈值时线程池只在单个 chunk 内批处理。

**ACORN 连通性估计**：`GraphLayersBuilder::subgraph_connectivity`（`lib/segment/src/index/hnsw_index/graph_layers_builder.rs:110`）在做带过滤建图时，会从当前子图的 entry point 出发做逐层 BFS，统计「给定预算内可达的点数比例」作为连通性指标，避免过滤把图打成孤岛。

### 25.3 图层搜索：四种 `search_on_level` 变体

`graph_layers.rs` 的模块文档（`lib/segment/src/index/hnsw_index/graph_layers.rs:1`）列出了四个变体，各有明确适用面：

| 变体 | 位置 | 束宽 | 使用场景 |
|------|------|------|---------|
| `search_on_level` | `lib/segment/src/index/hnsw_index/graph_layers.rs:108` | `ef` | 论文原始 HNSW，level 0 |
| `search_on_level_acorn` | `lib/segment/src/index/hnsw_index/graph_layers.rs:154` | `ef` | ACORN-1，level 0 带过滤 |
| `search_entry_on_level` | `lib/segment/src/index/hnsw_index/graph_layers.rs:279` | **1**（贪心） | level ≥ 1 的入口下降 |
| `search_on_level_with_vectors` | lib/segment/src/index/hnsw_index/graph_layers.rs:336 | `ef` | 内联向量的 `CompressedWithVectors` 图 |

标准版的核心循环（`lib/segment/src/index/hnsw_index/graph_layers.rs:108`）：

```
visited = VisitedList（位图，从 visited_pool 复用）
candidates = min-heap;  nearest = 固定长度优先队列(容量 = ef)
candidates.push(entry); nearest.push(entry)
while (c = candidates.pop_min()) is not None:
    if c.score < nearest.worst():            # 提前终止：最小候选都不如第 ef 好
        break
    for link in links(c.idx, level):        # ≤ M 个邻居
        if !visited.check(link): to_score.push(link)
    for s in scorer.score_points(to_score, M):
        candidates.push(s); nearest.push(s); visited.mark(s.idx)
```

**ACORN-1 的差异**（`lib/segment/src/index/hnsw_index/graph_layers.rs:154`）在于维护**两张 visited 集合**：

- `hop1_visited_list`：正常的一跳邻居，命中即跳过；
- `hop2_visited_list`：已被当作「被过滤掉的节点的二跳邻居」探索过，**不阻止再次作为一跳邻居处理**。

即 ACORN 不放弃被过滤节点，而是把它们标记为「待探索」，继续读取其邻居——代价是每个被过滤节点多付一次 `hop2_limit` 的打分，换来过滤后图的连通性不塌陷。`hop1_limit` 与 `hop2_limit` 是**每次探索**而非总量上限，注释明确说明「这是为了在邻居很多时避免过度打分」。

算法选择在 `GraphLayers::search` 里按 `SearchAlgorithm` 枚举（`lib/segment/src/index/hnsw_index/graph_layers.rs:81`）分派（`lib/segment/src/index/hnsw_index/graph_layers.rs:556`）：

```rust
let ef = max(ef, top);   // 束宽不能小于返回条数
match algorithm {
    Hnsw  => search_on_level(...),
    Acorn => search_on_level_acorn(...),
}
```

ACORN 的启用条件在 `search_with_graph`（`lib/segment/src/index/hnsw_index/hnsw/read_view/search.rs:60`）：必须 `acorn.enable == true` **且** `m0 != 0` **且**带 filter，且

```
selectivity = cardinality.exp / available_vector_count ≤ max_selectivity
```

默认 `max_selectivity = ACORN_MAX_SELECTIVITY_DEFAULT = 0.4`（`lib/segment/src/types.rs:622`），判定在 `lib/segment/src/index/hnsw_index/hnsw/read_view/search.rs:83`。语义是：**过滤越强（selectivity 越小）越值得开 ACORN**——因为强过滤下普通 HNSW 的贪心下降极易走进没有结果的死胡同。ACORN 目前**未实现**于内联向量图（`lib/segment/src/index/hnsw_index/hnsw/read_view/search.rs:92` 直接返回 `None`）。

### 25.4 查询编排器：四路分流

一次检索在 `HNSWIndexReadView::search`（`lib/segment/src/index/hnsw_index/hnsw/read_view/dispatch.rs:24`）里按下列顺序决策：

```
1. exact = params.exact
   → 直接 plain scan，并强制关闭量化 { ignore: true, rescore: false }   (dispatch.rs:43)

2. is_hnsw_disabled = (config.m == 0 && config.payload_m == 0)
   → plain scan（图里没有任何边）                                          (dispatch.rs:38)

3. 无 filter：available_vector_count < config.full_scan_threshold
   → plain scan（段内删除率高时，图遍历不如直接扫）                        (dispatch.rs:65)

4. 有 filter：
   a. cardinality.max <  full_scan_threshold → plain scan               (dispatch.rs:121)
   b. cardinality.min >  full_scan_threshold → HNSW graph               (dispatch.rs:133)
   c. 落在 [min, max] 之间 → 采样估计后再定                              (dispatch.rs:149)
```

第 4c 步的采样估计（`lib/segment/src/index/sample_estimation.rs:22`）用 **Agresti–Coull 置信区间**（`lib/segment/src/index/sample_estimation.rs:8`，`z = 2`）判定，最多样本 `MAX_ESTIMATED_POINTS = 1000`（`lib/segment/src/index/sample_estimation.rs:5`）：

```rust
// 每采一个点就更新一次区间，一旦区间不再跨越 threshold 立即退出
if exp - interval > threshold { return true; }   // 确定超阈值 → 走图
if exp + interval < threshold { return false; }  // 确定低于阈值 → 走 plain
// 样本用完仍不确定 → 用点估计兜底：return exp > threshold
```

基数估计本身来自 payload 索引的 histogram，再用 `adjust_to_available_vectors`（`lib/segment/src/index/query_estimator.rs:26`）按「删除点与过滤条件不相关」的假设修正：

```
deleted      = available_points - available_vectors
min' = max(0, estimation.min - deleted)
max' = min(estimation.max, available_vectors, available_points)
exp' = round(estimation.exp * min(1, available_vectors / available_points))
```

非索引的 `PlainVectorIndex` 用同构的判据（`lib/segment/src/index/plain_vector_index/read_view/search.rs:27`）：

```
vector_size_bytes = available_bytes / available_vector_count            // lib/segment/src/index/plain_vector_index/read_view/search.rs:38
threshold_bytes   = search_optimized_threshold_kb * 1024                // lib/segment/src/index/plain_vector_index/read_view/search.rs:40
有 filter: scan_size = vector_size_bytes * cardinality.max              // lib/segment/src/index/plain_vector_index/read_view/search.rs:46
           → scan_size <= threshold_bytes ? plain : 走 hnsw
无 filter: total    = vector_size_bytes * available_vector_count       // lib/segment/src/index/plain_vector_index/read_view/search.rs:49
           → total <= threshold_bytes ? plain scan : 拒绝（indexed_only 时）
```

其中 `search_optimized_threshold_kb = max(indexing_threshold_kb, hnsw.full_scan_threshold_kb)`（`lib/collection/src/collection_manager/segments_searcher.rs:186`），`BYTES_IN_KB = 1024`（`lib/segment/src/common/mod.rs:241`）。

**求值顺序优化**：`OptimizedFilter`（`lib/segment/src/index/query_optimization/optimized_filter.rs:9`）把 `should` / `min_should` / `must` / `must_not` 拆成四个 `Vec<ConditionCheckerEnum>`，并按**选择性由弱到强**短路求值（`lib/segment/src/index/query_optimization/optimized_filter.rs:47`）：`should` 任一命中即继续 → `min_should` 计数够数 → `must` 全中 → `must_not` 全不中，任一环节失败立即 `return Ok(false)`。1.19 把 `min_should` 从 `Option<ShouldConditions>` 改成 `Vec + min_should_count`，让多个条件共享一个计数阈值。

### 25.5 距离度量的预处理差异

`Distance` 枚举只有四变体（`lib/segment/src/types.rs:313`）：`Cosine` / `Euclid` / `Dot` / `Manhattan`。`Metric` trait 把「预处理」和「打分」分开（`lib/segment/src/spaces/metric.rs:8`），**只有 Cosine 有非平凡预处理**：

| Metric | `preprocess` | `similarity` |
|--------|--------------|--------------|
| `CosineMetric` | L2 归一化 `x / ||x||` | 点积 |
| `EuclidMetric` | 原样返回（`lib/segment/src/spaces/simple.rs:69`） | `-Σ(a-b)²`（`lib/segment/src/spaces/simple.rs:214`） |
| `DotProductMetric` | 原样返回 | `Σa·b`（`lib/segment/src/spaces/simple.rs:237`） |
| `ManhattanMetric` | 原样返回 | `-Σ|a-b|`（`lib/segment/src/spaces/simple.rs:221`） |

```rust
fn cosine_preprocess(vector: DenseVector) -> DenseVector {   // simple.rs:228
    let mut length = vector.iter().map(|x| x * x).sum();
    if is_length_zero_or_normalized(length) { return vector; }   // 已归一化/零向量直接短路
    length = length.sqrt();
    vector.iter().map(|x| x / length).collect()
}
```

**为什么 Cosine 必须先归一化**：`Cosine(q,d) = (q·d)/(‖q‖·‖d‖)`，分母依赖具体向量，无法在图遍历中只算点积。而归一化后 `‖q‖=‖d‖=1`，有

```
cos(q,d) = q·d = (1/2)(‖q‖² + ‖d‖² − ‖q−d‖²) = 1 − ½‖q−d‖²
```

于是 Cosine 可**复用 Euclid 的平方距离内核**，只需常数变换。`is_length_zero_or_normalized` 的短路很重要：它让「重复预处理同一向量」幂等（模块内 `test_cosine_stable_preprocessing` 用 DIM=1500、100 次重复归一化验证）。

**SIMD 分派**按维度与指令集逐级降级：AVX+FMA 且 `dim ≥ MIN_DIM_SIZE_AVX = 32` → AVX；SSE 且 `dim ≥ MIN_DIM_SIZE_SIMD = 16` → SSE；aarch64 NEON 同阈值；否则落到标量实现（`lib/segment/src/spaces/simple.rs:15` / `lib/segment/src/spaces/simple.rs:22`）。`MetricPostProcessing` 负责把内部「越大越近」的分数转成用户可见值：`EuclidMetric::postprocess = |score|^0.5`（开方还原真距离），`ManhattanMetric::postprocess = |score|`。

### 25.6 标量量化 int8：min-max 公式与分数还原

`ScalarQuantizationConfig`（`lib/segment/src/types.rs:937`）目前只有 `ScalarType::Int8` 一个变体（无 int4，**wiki §8.4 表格中的 int4 行在 1.19 中不存在**），另可选 `quantile ∈ [0.5, 1.0]` 做分位数截断以抑制离群值。

训练（`EncodedVectorsU8::encode`，`lib/quantization/src/encoded_vectors_u8.rs:193`）：

```
alpha  = (max - min) / 127            // lib/quantization/src/encoded_vectors_u8.rs:524
offset = min                          // lib/quantization/src/encoded_vectors_u8.rs:525
```

编码是**非对称 min-max 缩放**（不是「围绕 0 对称」）：

```rust
fn encode_value(&self, value: f32) -> u8 {          // encoded_vectors_u8.rs:95
    let i = (value - self.offset) / self.alpha;
    i.clamp(0.0, 127.0).round() as u8
}
```

编码后的相似度用整数点积/L1 计算（`score_point_simple` / `_neon` / `_sse` / `_avx` 四档，见 `lib/quantization/src/encoded_vectors_u8.rs:338` / `lib/quantization/src/encoded_vectors_u8.rs:379` / `lib/quantization/src/encoded_vectors_u8.rs:425` / `lib/quantization/src/encoded_vectors_u8.rs:471`），最后由 `postprocess_score` 还原（`lib/quantization/src/encoded_vectors_u8.rs:100`）：

```
score_real = multiplier * score_int + query_offset + vector_offset
```

`multiplier` 按距离类型分支（`lib/quantization/src/encoded_vectors_u8.rs:211`）：

| 距离 | 展开式 | multiplier | 位置 |
|------|--------|-----------|------|
| `Dot` / `Cosine` | `(αx−o)(αy−o) = α²xy − αo(x+y) + o²` | `α²` | `lib/quantization/src/encoded_vectors_u8.rs:213` |
| `L1` | `|(αx−o) − (αy−o)| = α·|x−y|` | `α` | `lib/quantization/src/encoded_vectors_u8.rs:216` |
| `L2` | `(αx−o − αy+o)² = α²(x−y)²` | `−2α²`（差分平方展开的交叉项系数） | `lib/quantization/src/encoded_vectors_u8.rs:219` |

额外的 `o²` 偏移由 `get_shift()` 统一处理（`lib/quantization/src/encoded_vectors_u8.rs:118`）：`Dot`/`Cosine` 下 `shift = dim * offset²`，而 `L1`/`L2` 恒为 `0`——注释解释 L2 从平方差计算，公共 offset 在平方前就抵消了，若再加 `offset²` 会让分数依赖「整体平移所有向量与查询向量」。Euclid/Manhattan 还需 `invert = true` 把「越小越近」翻成「越大越近」（`lib/segment/src/vector_storage/quantized/quantized_vectors.rs:232`）。

### 25.7 PQ：对称码本 + LUT 查表

`ProductQuantizationConfig`（`lib/segment/src/types.rs:987`）只暴露一个 `compression: CompressionRatio`。压缩比到「每字节装几个码」的映射在 `get_bucket_size`（`lib/segment/src/vector_storage/quantized/quantized_vectors.rs:236`）：

```
X4 → 1    X8 → 2    X16 → 4    X32 → 8    X64 → 16     // lib/segment/src/vector_storage/quantized/quantized_vectors.rs:240..lib/segment/src/vector_storage/quantized/quantized_vectors.rs:244
```

每向量字节数 = `ceil(dim / chunk_size)`（`lib/quantization/src/encoded_vectors_pq.rs:710`），即 `bucket_size` 个 8-bit 码共享 `bucket_size` 个维度，故 `compression ≈ 4 × bucket_size`（4×dim 字节 ÷ dim/bucket_size 字节），与枚举语义自洽。码本固定 256 个质心（`CENTROIDS_COUNT = 256`，`lib/quantization/src/encoded_vectors_pq.rs:31`），k-means 采样上限 `KMEANS_SAMPLE_SIZE = 10_000`、最大 100 轮、收敛阈值 `1e-5`（`lib/quantization/src/encoded_vectors_pq.rs:28`、`lib/quantization/src/encoded_vectors_pq.rs:29`、`lib/quantization/src/encoded_vectors_pq.rs:30`）。

**编码用欧氏距离，与配置的距离类型无关**（`lib/quantization/src/encoded_vectors_pq.rs:301`）：

```rust
for range in vector_division {                 // 每个子空间
    // 源码注释：by product quantization algorithm use euclid metric for any similarity function
    let distance = subvector.iter().zip(centroid_data)
                             .map(|(a,b)| (a-b).powi(2)).sum();
    if distance < min_distance { min_centroid_index = i; }
    encoded_vector.push(min_centroid_index as u8);   // 一个子空间 → 一个 u8 码
}
```

**Qdrant 用的是对称 PQ**——query 与 database 共享同一套码本，编码端与查询端都用「子向量 → 最近质心编号」。查询时不做在线 k-means，而是预先把 query 对每个子空间的每个质心的距离算成一张 **LUT（查找表）**（`EncodedVectorsPQ::encode_query`，`lib/quantization/src/encoded_vectors_pq.rs:519`）：

```rust
lut = Vec[f32];                                    // 长度 = 子空间数 × 256
for range in &self.metadata.vector_division {
    for i in 0..256 {
        let distance = vector_parameters.distance_type.distance(&query[range], &centroids[i][range]);
        lut.push(if invert { -distance } else { distance });   // 用**配置的真实距离类型**
    }
}
```

打分退化为一次查表求和（`score_point_simple`，`lib/quantization/src/encoded_vectors_pq.rs:478`）：

```
score(d) = Σ_{j=0}^{m-1} lut[j * 256 + code_j(d)]
```

LUT 布局是**子空间主序**（`lut += centroids_count` 逐子空间步进），因此 SSE/AVX 实现可以一次取 4 个子空间的 4 个表项做水平加法（`lib/quantization/src/encoded_vectors_pq.rs:411` 的 `_sse`、`lib/quantization/src/encoded_vectors_pq.rs:446` 的 `_neon`），这是 LUT 必须按子空间分组而非按码值分组的原因。

**复杂度**：PQ 打分从 `O(dim)` 降到 `O(m)`（`m = dim / chunk_size`），代价是只保留粗粒度信息；`encode_query` 的 LUT 构造开销 `O(m × 256)` 被每次查询一次摊销，可忽略。

**与标量量化的组合取舍**：Qdrant 支持 `scalar_quantization` 与 `product_quantization` 二选一（`QuantizationConfig` 枚举，`lib/segment/src/types.rs:1123`，变体为 `Scalar`/`Product`/`Binary`/`Turbo`），**不支持叠加**。取舍很直接——标量化误差可还原为线性变换（见 25.6 的 multiplier），PQ 的查表和只能给出**子空间近似**，对需要精确排序的场景（如 rerank 前置召回）精度损失更大。实践上「Scalar int8 打底 + 少量 oversampling + 原始向量 rescoring」是默认路线。

### 25.8 二值量化：符号位 / z-score 2bit / xor-popcnt

`BinaryQuantizationConfig`（`lib/segment/src/types.rs:1045`）新增了 `encoding: OneBit | TwoBits | OneAndHalfBits` 与 `query_encoding`，后者实现**非对称量化**——查询向量可以用不同位宽编码以换取精度（代价是性能）。

**1-bit** 编码就是取符号位（`encode_one_bit_vector`，`lib/quantization/src/encoded_vectors_binary.rs:558`）：

```rust
for (i, &v) in vector.iter().enumerate() {
    if v > 0.0 { encoded_vector[i / bits_count] |= one << (i % bits_count); }
}
```

前提是向量已被归一化到 `[-1, 1]`（Cosine 的 preprocess 恰好保证了这一点）。

**2-bit** 编码用 z-score 把维度分成三档（`lib/quantization/src/encoded_vectors_binary.rs:662`）：

```
v_z = (value - mean) / sd
SIGMAS = 2/3
v_z <= -2/3   → (false, false)   // 负且小
-2/3 < v_z < 2/3 → (true, false)  // 中性
v_z >= 2/3   → (true, true)     // 正且大
```

打分退化为汉明距离：`xor_popcnt` 计算 `q ⊕ d` 的置位数，归一化后由 `zeros_count = dim - xor_product` 还原相似度（`lib/quantization/src/encoded_vectors_binary.rs:797`）：

| 距离 + invert | score |
|---------------|-------|
| `Dot`/`Cosine`, invert=false | `xor_product - zeros_count` |
| `Dot`/`Cosine`, invert=true | `zeros_count - xor_product` |
| `L1`/`L2`, invert=true | `zeros_count - xor_product` |
| `L1`/`L2`, invert=false | `xor_product - zeros_count` |

注意 `invert` 语义翻转了直觉：余弦相似度下「不同位数越少越相似」，但 `invert=false` 时反而是 `xor_product - zeros_count = 2·xor_product − dim`（位数多=分高）——这是因为 Cosine 的 `DistanceType` 映射里 Binary 量化固定用 `DistanceType::Dot`（`lib/segment/src/vector_storage/quantized/quantized_vectors.rs:225`），而二值向量在 `[-1,1]` 上「同号为正相关」，异号才对齐负相关，故 xor 位数越多分越高。

`xor_popcnt` 按平台与位宽分层：x86 上 AVX-512 → AVX → SSE（128/64/32 位宽各一套），aarch64 上 NEON，最后落到标量 `(b1 ^ b2).count_ones()`（`lib/quantization/src/encoded_vectors_binary.rs:208`）。另有 `xor_popcnt_scalar` 处理「查询用 k 位、库内存 dim 位」的非对称情形（`lib/quantization/src/encoded_vectors_binary.rs:213`）。

### 25.9 oversampling + rescoring：量化检索的补偿

量化必然损失召回，Qdrant 用两段式补偿，参数封装在 `QuantizationSearchParams`（`lib/segment/src/types.rs:573`）：

| 字段 | 默认 | 语义 |
|------|------|------|
| `ignore` | `false` | 完全不用量化向量检索 |
| `rescore` | `None`（自动） | 是否用原始向量对 top-k 重打分 |
| `oversampling` | `None`（不超采样） | 预选倍数，须 ≥ 1.0 |

**判定量化是否生效**（`is_quantized_search`，`lib/segment/src/index/vector_index_search_common.rs:15`）：

```
quantized_search = quantized_storage.is_some() && !params.quantization.ignore && !params.exact
```

即 **`exact: true` 天然关闭量化**——这正是 25.4 第 1 步那条「强制 `{ignore: true, rescore: false}`」的由来。

**超采样条数**（`get_oversampled_top`，`lib/segment/src/index/vector_index_search_common.rs:27`）：

```rust
match oversampling_value {
    Some(o) if quantization_enabled && o > 1.0 => (o * top) as usize,
    _ => top,
}
```

例如 `oversampling = 2.4, limit = 100` → 量化索引召回 240 条。**注意短路条件 `o > 1.0`**：设为 1.0 与不设等价，验证层会拒绝任何提前返回。

**重打分**（`postprocess_search_result`，`lib/segment/src/index/vector_index_search_common.rs:48`）：

```rust
let rescore = quantization_enabled && params.quantization.rescore.unwrap_or(default_rescoring);
if rescore {
    // 用原始向量存储重新打分（原始向量若在磁盘上会多一轮随机读）
    let mut scorer = FilteredScorer::new(query, vector_storage, None, None, deleted_bits, hw);
    search_result = scorer.score_points(&mut result_ids, 0).collect();
    search_result.sort_unstable(); search_result.reverse();
}
search_result.truncate(top);
```

因此端到端流程是：

```
量化索引 (int8/PQ/BQ)  ──召回──▶  ceil(oversampling × top)  条
        │
        ├─ oversampling = 1.0：直接 truncate(top)，最快但召回最低
        └─ oversampling > 1.0 且 rescore：原始向量重打分 ──sort──▶  top 条
```

---

## 二十六、Payload 索引、全文检索与分片一致性

### 26.1 `PayloadIndexType`：十一种索引形态

`PayloadIndexType`（`lib/segment/src/index/payload_config.rs:145`）列出 11 个变体：`IntIndex` / `DatetimeIndex` / `IntMapIndex` / `KeywordIndex` / `FloatIndex` / `GeoIndex` / `FullTextIndex` / `BoolIndex` / `UuidIndex` / `UuidMapIndex` / `NullIndex`。**wiki §8.7 表格遗漏了 `UuidIndex`、`UuidMapIndex`、`NullIndex` 三项**。

每个索引用 `FullPayloadIndexType` 携带两个正交维度：

- **`IndexMutability`**（`lib/segment/src/index/payload_config.rs:174`）：`Mutable`（支持增删改）/ `Immutable`（只支持删）。appendable 段只能挂 `Mutable`，这是「不可变段可整段 mmap、appendable 段需常驻内存」的根本原因。
- **`StorageType`**（`lib/segment/src/index/payload_config.rs:183`）：`Gridstore`（heap 结构）/ `Mmap { is_on_disk }`（落盘 + 可选 mmap）。

底层目录按类型分：`numeric_index/`（int/float/datetime）、`map_index/`（keyword/uuid/int-map）、`bool_index/`、`null_index/`、`geo_index/`、`full_text_index/`，均在 `lib/segment/src/index/field_index/` 下。

### 26.2 数值索引的 Histogram 分桶

数值索引的基数估计靠 `Histogram<T>`（`lib/segment/src/index/field_index/histogram.rs:29`），核心字段是 `borders: BTreeMap<Point<T>, Counts>`，其中 `Counts { left, right }`（`lib/segment/src/index/field_index/histogram.rs:22`）记录分界点两侧各有多少条记录。

```rust
const MIN_BUCKET_SIZE: usize = 10;                     // histogram.rs:18
pub struct Histogram<T> {                               // lib/segment/src/index/field_index/histogram.rs:29
    max_bucket_size: usize, precision: f64, total_count: usize,
    borders: BTreeMap<Point<T>, Counts>,
}
```

`Histogram::new`（`lib/segment/src/index/field_index/histogram.rs:44`）断言 `0 < precision < 1`：分桶时若新桶样本数超过 `max_bucket_size × precision` 就再切一刀，从而在「桶数可控」与「桶内分布不过度倾斜」之间取平衡。落盘时拆成 `histogram_config.json`（配置）+ `histogram_borders.bin`（桶边界数组，`lib/segment/src/index/field_index/histogram.rs:20`），加载路径支持后台预取（`Histogram::preopen`，`lib/segment/src/index/field_index/histogram.rs:56`）。

范围查询（`$lt` / `$gt`）先用 `BTreeMap` 定位到桶边界，再在命中的桶内做精确比较——**桶边界即索引，查询复杂度 `O(log B + Σ桶大小)`**，B 为桶数。这也是 25.4 中 `cardinality.min` / `cardinality.max` 能作为 `full_scan_threshold` 判据的来源。

### 26.3 关键词索引与 front-coded 前缀索引

`MapIndexKey`（`lib/segment/src/index/field_index/map_index/key.rs:14`）是 map 索引键的抽象 trait，关键在于**哪些键类型支持前缀匹配**：

```rust
const SUPPORTS_PREFIX_INDEX: bool = false;   // 默认不支持（key.rs:21）
fn prefix_index_bytes(&self) -> Option<&[u8]> { None }   // key.rs:29
fn starts_with(&self, _prefix: &Self) -> bool { false } // key.rs:36
```

`str` 的实现（`lib/segment/src/index/field_index/map_index/key.rs:62`）把三者全部打开：`SUPPORTS_PREFIX_INDEX = true`、`prefix_index_bytes() = self.as_bytes()`、`starts_with = str::starts_with`。`i32` / `u64` 等类型保持关闭——整数没有前缀语义。

**前缀索引的磁盘格式是 front-coding（前置压缩）**（`lib/segment/src/index/field_index/map_index/prefix_index/format.rs`）：

```
MAGIC            = b"QdrPrfx\0"       // lib/segment/src/index/field_index/map_index/prefix_index/format.rs:12
VERSION          = 1                 // lib/segment/src/index/field_index/map_index/prefix_index/format.rs:13
BLOCK_SIZE_TARGET = 4096             // lib/segment/src/index/field_index/map_index/prefix_index/format.rs:16  每个 key block 目标字节数
```

每条 key 记录只存 `shared_prefix_len`（与**前一个 key**的最长公共前缀长度）+ `suffix_len` + 后缀字节；块内第一条 key 的 `shared_prefix_len == 0`（`lib/segment/src/index/field_index/map_index/prefix_index/format.rs:46`）。块索引区则每块存 `first_key`（完整）、块字节大小、key 数、postings 总数（`lib/segment/src/index/field_index/map_index/prefix_index/format.rs:31`）。

```
理论空间节省：若相邻 key 平均公共前缀为 p 字节（p ≈ log_256(N) ≈ 3~4）
  原始:        key_len 字节/条
  front-coded: (2 + key_len − p) 字节/条
  p = 4, key_len = 20 时节省约 30%
```

前缀查找先二分块索引定位起始块，再在块内线性扫描比较 `shared_prefix_len`（块内 key 已按字节序有序，`prefix_index_bytes` 的字节序必须与 `Owned` 的 `Ord` 一致——`key.rs:26` 的文档注释明确了这个不变量）。落盘文件名 `prefix_index.bin`（`lib/segment/src/index/field_index/map_index/prefix_index/mod.rs:95`）。

通配（前后缀）匹配走普通有序 map 的范围扫描 `get_range(prefix..)`——前缀索引只加速**前缀**一种形态。

### 26.4 Bool / Null 索引：目录即结构

bool 索引直接用两个子目录表达集合：`trues` 与 `falses`（`lib/segment/src/index/field_index/bool_index/mutable_bool_index/mod.rs:12` 与 `lib/segment/src/index/field_index/bool_index/mutable_bool_index/mod.rs:13`），每个目录内是 RoaringBitmap。null 索引同理：`has_values` 与 `is_null`（`lib/segment/src/index/field_index/null_index/mutable_null_index/mod.rs:12` 与 `lib/segment/src/index/field_index/null_index/mutable_null_index/mod.rs:13`）。

这是全文索引也用的同一套原语——`PostingList`（`lib/segment/src/index/field_index/full_text_index/inverted_index/posting_list.rs:5`）内部就是 `RoaringBitmap`：

```rust
pub struct PostingList { list: RoaringBitmap }
```

RoaringBitmap 把整数集合按「低 16 位稠密数组 / 高位分块 run-length」混合存储：小基数基数上几乎无开销，大基数上压缩比可达数十倍，且支持 O(log n) 的 contains 与快速并/交/差。这解释了 bool 索引「Bitmap + Value」的表格描述——**Qdrant 是纯 RoaringBitmap，没有独立 value 表**（value→point 的反向映射由 `on_disk_point_to_values.rs` / `immutable_point_to_values.rs` 承担）。

### 26.5 倒排全文索引：分词器 + posting list

`TokenizerType`（`lib/segment/src/data_types/index.rs:250`）四个变体：`Prefix` / `Whitespace` / `Word`（默认）/ `Multilingual`。分派在 `Tokenizer::tokenize`（`lib/segment/src/index/field_index/full_text_index/tokenizers/mod.rs:223`）：

| Tokenizer | 切分规则 | 位置 |
|-----------|---------|------|
| `Whitespace` | `text.split_whitespace()` | `tokenizers/mod.rs:23` |
| `Word` | `split(\|c: !char::is_alphanumeric(c))` | `tokenizers/mod.rs:41` |
| `Prefix` | 词内滑窗 n-gram（`min_token_len..=max_token_len`） | `tokenizers/mod.rs:57` |
| `Multilingual` | unicode 分词（CJK 拆字等） | `tokenizers/multilingual.rs` |

`Prefix` 分词器的关键设计（`lib/segment/src/index/field_index/full_text_index/tokenizers/mod.rs:57`）是**索引侧展开 n-gram、查询侧只取最大 n-gram**：

```
文档 "theory" → 索引 ["th","the","theo","theor","theory"]
查询 "hel"    → 只查 ["hel"]
```

即文档侧承担 O(n²) 的索引膨胀，查询侧保持 O(1)。且 `tokenize_query` **刻意不过滤 stopwords**（`tokenizers/mod.rs:96` 的文档注释举例：`stopwords = ["the"]`，文档含 "theory"，查询 "the" 仍应命中 "theor*" 前缀），因为增量输入场景下词还没输完。

多值数组字段用哨兵 `ARRAY_BOUNDARY_SENTINEL = "\x00"`（`lib/segment/src/index/field_index/full_text_index/inverted_index/mod.rs:30`）在倒排里标记数组边界。

倒排表的两种实现并存于 `lib/posting_list/src/`（独立的 `posting_list` crate）：`PostingList`（RoaringBitmap，无位置信息）与带 `Positions(Vec<u32>)` / `TokenPosition`（`lib/segment/src/index/field_index/full_text_index/inverted_index/positions.rs:8` / `lib/segment/src/index/field_index/full_text_index/inverted_index/positions.rs:54`）的增强版，支持短语查询。

### 26.6 BM25：作为稀疏嵌入器而非独立索引

`lib/bm25/` crate **不依赖 Tantivy**（`Cargo.toml` 依赖仅 `murmur3_32` 与可选 `serde`），它是**纯计算**：把 BM25 的 TF 权重算成一个稀疏向量，交给稀疏向量索引去检索。**wiki §8.8 中「基于 Tantivy / 自研」的表述在 1.19 中不准确**。

超参与默认值（`lib/bm25/src/lib.rs:29`、`lib/bm25/src/lib.rs:30`、`lib/bm25/src/lib.rs:31`）：

```
DEFAULT_K1        = 1.2      // TF 饱和
DEFAULT_B         = 0.75     // 长度归一化强度
DEFAULT_AVG_DOC_LEN = 256.0  // 语料平均文档长度
```

TF 权重（`lib/bm25/src/lib.rs:156`）：

```rust
tf = n * (k1 + 1.0)                                            // lib/bm25/src/lib.rs:156
tf /= k1 * (1 - b + b * doc_len / avg_doc_len) + n             // lib/bm25/src/lib.rs:157
```

查询侧每个 unique token 权重固定 `1.0`（`embed_query`，`lib/bm25/src/lib.rs:118`），IDF 项被下推到打分时的稀疏向量 scorer——**公式退化为 SPLADE 风格的 `Σ_idf(t) · tf(t)`**。token → `u32` 的映射是 `(murmur3_32(token) as i32).unsigned_abs()`（`lib/bm25/src/lib.rs:166`），与既有 BM25 稀疏向量 wire 兼容。

稀疏索引侧有三种形态（`SparseIndexType`，`lib/segment/src/index/sparse_index/sparse_index_config.rs:18`）：`MutableRam`（appendable 段）/ `ImmutableRam` / `Mmap`，并可用 `Modifier::{None, Idf}`（`lib/segment/src/data_types/modifier.rs:13`）在查询时对稀疏向量权重做 IDF 改写；IDF 语料范围可选全局或按 filter 限定（`IdfParams`，`lib/segment/src/types.rs:684`）。

### 26.7 分片路由：一致性哈希环 + shard key

`CollectionParams`（`lib/collection/src/config.rs:93`）里四个字段决定分片拓扑：

| 字段 | 类型 | 默认 | 位置 |
|------|------|------|------|
| `shard_number` | `NonZeroU32` | `1` | `lib/collection/src/config.rs:101` |
| `sharding_method` | `Option<ShardingMethod>` | `Auto` | `lib/collection/src/config.rs:85` |
| `replication_factor` | `NonZeroU32` | `1` | `lib/collection/src/config.rs:312` |
| `write_consistency_factor` | `NonZeroU32` | `1` | `lib/collection/src/config.rs:316` |

`ShardingMethod` 只有 `Auto`（点按 point id 散列到全部分片）与 `Custom`（按 `shard_key` 散列，同 key 必落同一分片）。`ShardKey` 是 `Keyword(EcoString)` 或 `Number(u64)` 二选一（`lib/segment/src/types.rs:6309`）。

路由用**一致性哈希环**（`HashRingRouter`，`lib/collection/src/hash_ring.rs:18`），关键是两个细节：

1. **虚拟节点**：`HASH_RING_SHARD_SCALE = 100`（`lib/collection/src/hash_ring.rs:15`），每个分片在环上放 100 个虚拟点 `StableHashed((shard, idx))`（`lib/collection/src/hash_ring.rs:239`）。scale 越高分布越均匀、查找越慢——`HashRing::fair(scale)` 的文档明说这个权衡（`lib/collection/src/hash_ring.rs:215`）。另有 `HashRing::raw()` 每分片仅 1 点。
2. **哈希必须是稳定哈希**：环用 `StableHashBuilder` 而非 `RandomState`，否则**同一节点在重启后算出的映射不同**，路由不可复现——分布式系统的硬性要求。

在线扩缩容（resharding）时 router 切到 `Resharding { old, new }` 双环态（`lib/collection/src/hash_ring.rs:20`），读写环分别 commit——这是实现「不停机迁移」的机制。

### 26.8 写一致性：`write_consistency_factor` 的 quorum

`write_consistency_factor` 定义「**多少副本执行成功即认为写入成功**」（`lib/collection/src/config.rs:112` 的文档：提高它会更强地抵御不一致，但副本不足时会失败；对吞吐无影响）。判定在 `ReplicaSet::update` 尾部（`lib/collection/src/shards/replica_set/update.rs:489`）：

```rust
let write_consistency_factor = config.params.write_consistency_factor.get() as usize;  // lib/collection/src/shards/replica_set/update.rs:481
let minimal_success_count = write_consistency_factor.min(replica_count);               // lib/collection/src/shards/replica_set/update.rs:489
// 然后对 (successes, failures) 做 partition_result，
// successes.len() >= minimal_success_count 即视为成功
```

**`min(replica_count)` 是安全阀**：`write_consistency_factor = 3` 但集群只有 2 个副本时，阈值降为 2——**永远不会因为 quorum 不可达而写入失败**。这与 Raft 的多数派语义不同：Qdrant 的一致性判定发生在**副本集合层面**（fan-out 到 N 个 replica set 并计数），单个 replica set 内部才各自走 Raft。

读取侧没有 `read_consistency_factor` 字段——**wiki §十一与 §十提到的「读一致性级别」在 1.19 的 Collection 配置中并不存在**。实际读语义由两个参数控制（`lib/collection/src/config.rs:120`）：

- `read_fan_out_factor`：同时向几个副本发读请求；
- `read_fan_out_delay_ms`：本地副本变慢后等多久再 fan out（默认 0 = 不延迟 fan out）。

即读是「best-effort + 读多个副本合并」，不是 quorum 读。`ReplicaSet` 用 `is_local`（`lib/collection/src/shards/replica_set/mod.rs:411`）标记本机副本，读请求先打本地再决定是否扩散。

WAL 侧：`SerdeWal<R>`（`lib/shard/src/wal.rs:20`）提供 `write` / `ack(until_index)` / `flush` / `flush_async` / `drop_from(from_index)` / `set_extended_retention` / `set_normal_retention`（`lib/shard/src/wal.rs:219`–`lib/shard/src/wal.rs:305`）——`ack` 是「内存段已持久化到 Segment，可以丢弃对应 WAL 区间」的游标；扩展保留期用于支持点级快照。

---

## 二十七、参考资料

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