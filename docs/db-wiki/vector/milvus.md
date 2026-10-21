# Milvus Wiki（云原生分布式向量数据库）

> 参考版本：Milvus 2.x（Go 编排 + C++ Knowhere 索引内核 + Pulsar/S3 存储），面向 AI 大规模向量检索的云原生分布式数据库。本 wiki 重点覆盖向量类型特有章节（Knowhere 索引框架、IVF/HNSW/DiskANN、CGO 绑定、计算-存储分离）。

## 一、项目定位

Milvus 是由 **Zilliz（向量数据库公司）+ LF AI & Data 基金会**主导开源的 **云原生分布式向量数据库**，专为 **大规模向量相似度检索** 设计，支持 **十亿级向量、毫秒级延迟、水平扩展、计算-存储分离、多副本容灾**，是当前生产级向量检索领域的事实标准之一。

- **痛点**：
  - 传统向量库（FAISS / Annoy）仅是算法库，无持久化、分布式、容灾能力；
  - Pinecone 等商业全托管成本高、数据主权受限；
  - RAG / 推荐 / 多模态检索场景对 **十亿级向量、低延迟、混合检索（向量 + 标量）、多租户** 有刚性需求；
  - 需要支持 **GPU 加速、DiskANN（SSD 友好）、稀疏向量、稠密向量、混合检索** 等多种索引与查询模式；
- **典型用户**：RAG 应用、AI Agent 知识库、电商推荐（图片/视频 embedding）、多模态检索（文/图/音视频）、广告 CTR、生物制药分子检索、安全异常检测；
- **核心口号**：**"Vector database built for scalable similarity search"** + **"Made by the creators of Faiss"** + **"Unstructured data processing at scale"**。

## 二、发展历史与社区

- **起源**：2018 年 Zilliz 创始人 Charles Xie 启动，初代基于 FAISS；2019 年开源；
- **里程碑**：
  - 2019-10：v0.5.x 开源，单机 FAISS 包装；
  - 2020-06：v0.10.x 引入 **Attu** Web UI；
  - 2020-10：v1.0 GA（单机版），基于 FAISS + RocksDB；
  - 2021-04：v2.0 里程碑，**计算-存储分离架构重写**（Go 微服务 + C++ Knowhere + 消息队列 + 对象存储）；
  - 2021-09：v2.1 引入 **DiskANN**、**MMap 索引**；
  - 2022-06：v2.2 引入 **GPU 索引（CAGRA）**、**HNSW 优化**；
  - 2023-04：v2.3 引入 **Range Search**、**Search Iterator**、**Group By**；
  - 2023-08：v2.4 引入 **Sparse Vector（BM25 + SPLADE）**、**JSON 索引**、**Hybrid Search 2.0**；
  - 2024-07：v2.5 引入 **VDB（Vector Database）1.0**、**Streaming Node**、**水平扩容**；
  - 2025-06：v2.6 引入 **Lance 格式集成**、**新 Compaction**、**Snowflake 适配**；
  - 2026：持续向 **多模态统一存储（向量 + 全文 + JSON + 稀疏）** 演进；
- **维护状态**：极活跃，Zilliz 50+ 工程师全职投入，月度小版本、季度大版本；
- **社区**：GitHub 32k+ star、Slack 8k+ 用户、Forum、CSDN / 知乎高活跃、年度 **Milvus Community Day**。

## 三、许可与商业限制

- **Milvus 核心代码**：**Apache License 2.0**（Go 编排层 + 部分 C++ 代码）；
- **Knowhere**（C++ 索引内核）：**Apache License 2.0**（Milvus 自研，从 FAISS 派生）；
- **商业版本**：
  - **Zilliz Cloud**：全托管（AWS / GCP / Azure / 阿里云）；
  - **Milvus Enterprise**：支持 RBAC、审计、多区域复制、Backup；
- **生态关系**：
  - 与 **FAISS**（Meta）深度整合（Knowhere 内核分支自 FAISS）；
  - 与 **Towhee**（Zilliz 开源 AI 框架）打通；
  - 与 **LangChain / LlamaIndex / Haystack** 是默认向量后端之一；
  - 与 **Pulsar / Kafka**（消息队列）+ **S3 / MinIO / OSS / GCS**（对象存储）紧耦合。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **NVIDIA**：Jetson 设备 RAG；
  - **Shopee / Tokopedia**：电商推荐；
  - **Baidu**：ERNIE 知识库；
  - **Tencent**：微信 AI 内容检索；
  - **Shopify**：商品检索；
  - **Bosch**：工业 RAG；
  - **Roblox**：游戏资产检索；
  - 国内：阿里、字节、商汤、智谱、平安、好未来等；
- **行业**：电商推荐、AI RAG、内容平台、生物制药、网络安全、智能客服、视频理解；
- **典型场景**：十亿级图片向量检索（推荐）、RAG 知识库、跨模态文-图检索、生物分子相似度、欺诈检测 embedding 检索。

## 五、一句话对比定位

> Milvus 是 **"Go 编排 + C++ Knowhere 内核 + 计算-存储分离 + 多副本 + 10+ 种索引（IVF/HNSW/DiskANN/GPU CAGRA）+ 稀疏 + 稠密 + 标量过滤 + 混合检索"** 的云原生分布式向量数据库；与 **Pinecone（商业全托管）**、**Weaviate（Go 模块化）**、**Qdrant（Rust 云原生）**、**Chroma（Python 嵌入式）**、**LanceDB（嵌入式列存）**、**Vespa（Java/C++ 搜索平台）**、**FAISS（算法库）**、**USearch（嵌入式 C++）** 构成向量库阵营。Milvus 的差异化是 **"十年级生产验证 + 计算-存储分离 + 丰富索引 + 中文社区 + Pulsar/Kafka 流式"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层                                   │
│  - Python: pymilvus（推荐）                                          │
│  - Java / Go / Node.js / C++ / Rust（官方 SDK）                    │
│  - RESTful API（HTTP 端口 9091）                                     │
│  client/                                                            │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ gRPC (Port 19530) / HTTP
┌─────────────────────▼───────────────────────────────────────────────┐
│                      Proxy 接入层                                    │
│  internal/proxy/                                                     │
│  ├─ Proxy：请求路由、限流、鉴权、SDK 兼容                          │
│  ├─ Rate Limiter、Quota Enforcer                                    │
│  ├─ DDL/DML/DSL 解析（parser/）                                     │
│  └─ 数据校验、字段映射                                               │
└──────┬──────────────────┬───────────────────┬───────────────────────┘
       │ DDL              │ DML                │ DQL
┌──────▼───────┐  ┌───────▼────────┐  ┌──────▼───────────────┐
│ RootCoord   │  │  DataCoord     │  │  QueryCoordv2       │
│ - DDL       │  │  - 段管理      │  │  - Segment 调度      │
│ - TSO       │  │  - 索引构建    │  │  - 副本负载均衡      │
│ - 用户/角色 │  │  - 压缩调度    │  │  - Handoff          │
│  internal/  │  │  - Channel 分配│  │  - 资源调度          │
│  rootcoord/ │  │  internal/     │  │  internal/           │
└──────┬──────┘  │  datacoord/    │  │  querycoordv2/       │
       │         └────────┬───────┘  └──────────┬────────────┘
       │                  │                     │
       │         ┌────────▼───────────┐         │
       │         │  IndexCoord        │         │
       │         │  - 索引任务调度    │         │
       │         │  - 节点负载均衡    │         │
       │         │  internal/         │         │
       │         │  indexcoord/       │         │
       │         └────────┬───────────┘         │
       │                  │                     │
┌──────▼──────────────────▼─────────────────────▼───────────────────┐
│                      存储层（解耦）                                  │
│  - MetaStore（etcd / TiKV）：集群元数据、TSO                       │
│  - Object Storage（S3 / MinIO / OSS）：binlog、index、snapshot    │
│  - Message Queue（Pulsar / Kafka）：WAL、Channel 流                │
│    internal/tso/、internal/kv/、internal/storage/                  │
└──────┬─────────────────────────────────────────────────────────────┘
       │
┌──────▼──────────────────────────────────────────────────────────────┐
│                      节点层                                          │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐ │
│  │ QueryNodev2      │  │ DataNode         │  │ IndexNode        │ │
│  │ - 向量检索       │  │ - binlog 持久化  │  │ - 索引构建       │ │
│  │ - 段加载/缓存    │  │ - flush          │  │ - 索引优化       │ │
│  │ - 混合检索       │  │ - compaction     │  │ - 异步任务       │ │
│  │ internal/        │  │ internal/        │  │ internal/        │ │
│  │ querynodev2/     │  │ datanode/        │  │ indexnode/       │ │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘ │
│       │                       │                       │            │
│       └──────────┬────────────┴───────────┬───────────┘            │
│                  │ CGO 绑定               │                          │
│                  ▼                        ▼                          │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │          C++ 索引内核（Knowhere）                            │  │
│  │  internal/core/thirdparty/knowhere/                          │  │
│  │  ├─ IVF_FLAT / IVF_PQ / IVF_SQ8 / IVF_HNSW                 │  │
│  │  ├─ HNSW / HNSW_PQ / HNSW_SQ                                │  │
│  │  ├─ DiskANN / ScaNN                                         │  │
│  │  ├─ Flat / Brute Force                                       │  │
│  │  ├─ CAGRA（GPU HNSW）                                       │  │
│  │  ├─ BinIDF（倒排 + 向量）                                   │  │
│  │  └─ Sparse Inverted Index                                    │  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **计算-存储分离**：节点无状态，元数据存 etcd / TiKV，数据 binlog 存对象存储，WAL 走 Pulsar/Kafka；
- **多角色协调节点**：RootCoord / QueryCoord / DataCoord / IndexCoord 分工明确，每个都可独立扩缩容；
- **Streaming Node**（v2.5+）：流式摄入，降低写入延迟从秒级到毫秒级；
- **TSO（Timestamp Oracle）**：全局时钟服务，向量检索支持快照隔离；
- **CGO 绑定**：Go 编排通过 CGO 调用 C++ Knowhere 内核，性能无损；
- **Knowhere 抽离**：索引实现与节点逻辑解耦，可独立测试、升级；
- **WAL + binlog 双写**：保证崩溃恢复和 exactly-once 语义。

## 七、存储引擎

### 7.1 Binlog（行存持久化格式）

- **结构**：每个 Field（Column）一个 binlog 文件，追加写入，周期性 Sealed；
- **代码**：`internal/storage/binlog.go`, `internal/storagev2/`;
- **格式**：每个 binlog 含 MagicHeader、ColumnGroup、Payload（Arrow RecordBatch）；
- **StorageV2**：v2.5+ 引入的列存优化格式（基于 Lance 思路）。

### 7.2 对象存储 / 本地缓存

- **S3 / MinIO / OSS / GCS / Azure Blob**：冷数据；
- **本地磁盘**：`/var/lib/milvus/` 缓存热数据；
- **mmap**：索引加载支持 mmap，避免全量 load 到内存（`internal/storage/MemFileManagerImpl.h`）。

### 7.3 元数据存储（etcd / TiKV / MySQL）

- **etcd**：默认，存 Collection schema、Segment 元数据、Channel 分配；
- **TiKV**：分布式场景下的 etcd 替代（事务能力更强）；
- **MySQL/PostgreSQL**：可选元数据后端（企业版）；
- **代码**：`internal/kv/`, `internal/metastore/`。

### 7.4 消息队列（Pulsar / Kafka / RocketMQ）

- **WAL**：Write-Ahead Log，落盘前先追加到 MQ；
- **Channel**：Collection 维度的逻辑分区，多副本消费；
- **代码**：`internal/distributed/streamingcoord/`。

## 八、索引机制（**向量类型核心**）

### 8.1 向量索引算法全景

Milvus 通过 **Knowhere**（`internal/core/thirdparty/knowhere/`）支持多种向量索引：

| 索引 | 类型 | 数据精度 | 适用规模 | 距离度量 | 备注 |
|------|------|---------|----------|---------|------|
| **FLAT** | 暴力 | FP32 / FP16 / BF16 | 百万级以下 | L2 / IP / COSINE | 精确 |
| **IVF_FLAT** | 倒排 + 精确 | FP32 | 千万级 | L2 / IP / COSINE | 经典 IVF |
| **IVF_SQ8** | 倒排 + 标量量化 | SQ8 | 千万级 | L2 / IP | 内存减半 |
| **IVF_PQ** | 倒排 + 乘积量化 | PQ | 亿级 | L2 / IP / COSINE | 内存 1/30 |
| **HNSW** | 图索引 | FP32 / FP16 / BF16 | 千万级 | L2 / IP / COSINE | 高 recall |
| **HNSW_SQ** | 图 + 标量量化 | SQ8 | 千万级 | L2 / IP | 内存减半 |
| **HNSW_PQ** | 图 + 乘积量化 | PQ | 千万级 | L2 / IP / COSINE | 内存大幅降低 |
| **HNSW_PRQ** | 图 + 残差量化 | PRQ | 亿级 | L2 / IP | 2.x 引入 |
| **DiskANN** | SSD 图索引 | FP32 / SQ8 | 十亿级 | L2 / IP | 微软研究院 |
| **ScaNN** | 量化 + 分区 | PQ + 残差 | 亿级 | L2 / IP | Google ScaNN |
| **CAGRA** | GPU HNSW | FP32 / FP16 | 亿级 | L2 / IP / COSINE | NVIDIA RAPIDS |
| **Sparse_Inverted** | 倒排 | BM25 / SPLADE | 全规模 | 内积 | 稀疏向量 |
| **BinIDF** | 倒排 + 二值 | 二值 | 全规模 | Hamming | 二值向量 |

### 8.2 IVF（Inverted File）详解

**IVF** 是最经典的近似最近邻索引（基于 k-means 聚类）：

```
┌─────────────────────────────────────────────────┐
│  阶段 1：训练 k-means                            │
│  - 输入：N 个向量                                │
│  - 输出：K 个聚类中心（Centroids）                │
│  - 通常 K = 4096 ~ 65536                       │
└─────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────┐
│  阶段 2：建立倒排表                              │
│  - 每个向量分配到最近 Centroids（nprobe=1）      │
│  - 倒排表：Centroid_i → [Vector_1, Vector_5,...]│
└─────────────────────────────────────────────────┘
       ↓
┌─────────────────────────────────────────────────┐
│  阶段 3：查询                                    │
│  - 计算 query 到 K 个 Centroids 的距离          │
│  - 选 nprobe（默认 16）个最近 Centroids          │
│  - 在候选集合内做精确 k-NN                       │
└─────────────────────────────────────────────────┘
```

**参数**：
- **`nlist`**：聚类数（默认 16384）；
- **`nprobe`**：查询时探测的聚类数（默认 16，越大 recall 越高但越慢）；
- **`m`**：PQ 子量化器数；
- **`nbits`**：PQ 编码位数（默认 8）。

**源码**：`internal/core/thirdparty/knowhere/faiss/IndexIVF*.cpp`（Knowhere 分支自 FAISS）。

### 8.3 HNSW 详解（Milvus 高 recall 首选）

参见 Chroma Wiki 中的 HNSW 章节。Milvus 实现位于 `internal/core/thirdparty/knowhere/hnswlib/`（Knowhere 分支自 hnswlib）。

**Milvus 特有优化**：
- 支持 **FP16 / BF16** 存储（`IndexHNSW_C<bf16>` 模板特化）；
- 支持 **Filter-aware HNSW**：构建时知道标量过滤条件，跳过被过滤节点；
- **HNSW_PQ**：HNSW 图节点存储压缩后的 PQ 码，内存降低 4-10x；
- **HNSW_PRQ**：HNSW + Product Residual Quantization（v2.5+）；
- **HNSW 持久化**：mmap 加载，大幅降低启动时间。

### 8.4 DiskANN 详解（Milvus 杀手锏）

**DiskANN** 是微软研究院 2019 年提出的 **SSD 友好的图索引**：

- **核心思路**：将 Vamana 图（类似 NSW）持久化到 SSD，内存只保留 **PQ 码 + entry point + 小部分热节点**；
- **查询**：先查内存 PQ → 找到近似邻居 → 按需加载 SSD 节点 → refine；
- **优势**：
  - 单节点可承载 **十亿级向量**；
  - 内存占用仅 **1-3 bytes/dim**；
  - SSD 顺序读，IO 友好；
- **代价**：构建时间长、需要 SSD（NVMe 推荐）；
- **源码**：`internal/core/thirdparty/knowhere/diskann/`；
- **论文**：Subramanya et al., "DiskANN: Fast Accurate Billion-point Nearest Neighbor Search on a Single Node", 2019, NeurIPS。

### 8.5 CAGRA（GPU 索引）

- **NVIDIA RAPIDS cuVS** 提供的 GPU HNSW 实现；
- **优势**：相比 CPU HNSW **5-50x** 加速（取决于维度 / 规模）；
- **支持硬件**：Ampere / Hopper GPU（Compute Capability 8.0+）；
- **源码**：`internal/core/thirdparty/knowhere/cagra/`。

### 8.6 量化方案

| 方案 | 描述 | 压缩比 | 适用 |
|------|------|--------|------|
| **PQ（Product Quantization）** | 把向量切为 m 段，每段聚类 k=256，存 8-bit 码本 | 1/8 ~ 1/32 | IVF_PQ、HNSW_PQ |
| **SQ（Scalar Quantization）** | 整体量化到 8-bit 整数 | 1/4 | IVF_SQ8、HNSW_SQ |
| **PRQ（Product Residual Quantization）** | PQ + 残差编码 | 1/16 ~ 1/32 | HNSW_PRQ（v2.5+） |
| **RaBitQ**（实验性）| 二值化 + 残差，1-bit/维 | 1/32 | DiskANN |
| **二值化（Binary）** | Hamming 距离 | 1/32 | BinIDF |

源码：`internal/core/thirdparty/knowhere/quant/`。

### 8.7 混合索引：HNSW + 标量过滤

Milvus 通过 **Bitset** 实现过滤 + 向量检索的融合：

```
SQL: SELECT * FROM tbl 
     WHERE category='book' 
     ORDER BY vector_distance(emb, q_vec) 
     LIMIT 10

执行流程：
1. 标量索引（如 BitmapIndex、InvertedIndex）评估 WHERE，得到命中 row IDs
2. 生成 Bitset（length = segment_size）
3. HNSW / IVF 检索时携带 Bitset，跳过非命中节点
4. 合并 Top-K
```

**优化**：HNSW 图遍历过程中使用 **Pre-filter + Graph Reorder**（v2.4+），recall 接近无过滤。

源码：`internal/core/src/index/Utils.cpp`, `internal/core/src/segcore/segment_c.cpp`。

### 8.8 索引构建（IndexCoord + IndexNode）

```
Client: create_index(field, "HNSW", params)
   ↓
IndexCoord: 调度 IndexNode
   ↓
IndexNode: 加载 Segment → 调用 Knowhere::Build()
   ↓
Knowhere: 训练（k-means / PQ）+ 构建（HNSW / IVF）
   ↓
IndexNode: Serialize 到 S3
   ↓
QueryNode: 按需加载（mmap / 全量）
```

源码：`internal/indexcoord/`, `internal/indexnode/`, `internal/core/src/index/IndexFactory.cpp`。

## 九、查询/计算/接口层

### 9.1 REST / gRPC API

- **gRPC 端口**：19530（默认）；
- **HTTP 端口**：9091（健康检查、metrics）；
- **SDK 协议**：proto 定义在 `internal/proto/`，gRPC 生成 Go/Python/Java/Go SDK；
- **核心 endpoint**：
  - `CreateCollection`, `DropCollection`, `HasCollection`
  - `Insert`, `Upsert`, `Delete`
  - `Search`, `Query`, `HybridSearch`
  - `CreateIndex`, `DropIndex`, `LoadCollection`

### 9.2 SDK 生态

| 语言 | 包名 | 备注 |
|------|------|------|
| Python | `pymilvus` | 官方推荐，功能最全 |
| Java | `milvus-sdk-java` | 完整功能 |
| Go | `milvus-sdk-go` | 服务端自用 |
| Node.js / TS | `@zilliz/milvus2-sdk-node` | 完整功能 |
| C++ | `milvus-sdk-cpp` | 原生 |
| Rust | `milvus-sdk-rust` | 社区 |

### 9.3 查询语言

- **DSL（Milvus 2.x 自研）**：基于 JSON 的查询 DSL，类似 SQL 但简化；
- **表达式语法**：`col1 > 10 AND col2 == "book" AND JSON_CONTAINS(meta, "tag")`；
- **算子**：`==`, `!=`, `>`, `<`, `>=`, `<=`, `IN`, `LIKE`, `JSON_CONTAINS`, `ARRAY_CONTAINS`；
- **Search v.s. Query**：
  - **Search**：向量相似度检索（top-K）；
  - **Query**：标量过滤 + 标量投影；
  - **HybridSearch**：多路向量 + RRF 融合（v2.4+）。

### 9.4 多租户

- **Database**：命名空间隔离（`db_name`）；
- **Collection**：实际数据容器；
- **Partition**：Collection 内分区（v1 遗留，v2 弱化）；
- **Resource Group**：节点资源分组（v2.2+），隔离不同业务；
- **RBAC**：Database / Collection / Global 级别权限（企业版）。

### 9.5 混合检索

- **向量 + 标量**：见 §8.7；
- **向量 + BM25（全文）**：v2.4+ 支持 Sparse Vector，BM25 直接索引；
- **多路 RRF 融合**：

```python
result = client.hybrid_search(
    collection_name="docs",
    reqs=[
        AnnSearchRequest(vector=dense_emb, field="dense", param={"metric_type": "L2"}, limit=10),
        AnnSearchRequest(vector=sparse_emb, field="sparse", param={"metric_type": "IP"}, limit=10),
    ],
    ranker=RRFRanker(k=60),
    limit=10,
)
```

源码：`internal/core/src/query/SearchOnIndex.cpp`, `internal/proxy/task.go`。

### 9.6 Rerank（重排）

- **内置 Rerank**：RRFRanker、WeightRanker；
- **外部 Rerank**：用户自行用 Cross-Encoder / Cohere Rerank API 在客户端重排；
- **Function（v2.5+）**：内置 BM25、Splade、Embedding 函数，用户可在服务端调用。

## 十、事务与一致性

- **TSO（Timestamp Oracle）**：全局逻辑时钟，`internal/tso/`；
- **隔离级别**：默认 **Snapshot Isolation**（写入对读立即可见，读到写入前的快照）；
- **事务**：
  - DML 操作（Insert/Delete）原子；
  - 跨 Collection 事务有限支持；
- **WAL**：写入先入 Pulsar/Kafka，再异步消费持久化；
- **Exactly-Once**：基于 MQ offset + segment seq_id。

## 十一、复制与高可用

- **多副本（Replica）**：Segment 副本数可配（默认 1，生产建议 2-3）；
- **Handoff**：节点故障自动迁移 Segment；
- **Resource Group 隔离**：query 副本可分到不同 RG，HA + 负载均衡；
- **多区域**：Zilliz Cloud 支持跨区域复制（企业版）；
- **备份**：Snapshot 到对象存储，支持 PITR（Point-in-Time Recovery）。

## 十二、扩展性

- **水平扩展**：
  - 各 Coord 节点可独立扩缩容；
  - QueryNode / DataNode / IndexNode 独立添加；
  - Storage（对象存储）天然弹性；
- **垂直扩展**：QueryNode 可单节点多 GPU（CAGRA 加速）；
- **冷热分层**：v2.5+ 引入 Cold Tier（把冷数据卸载到对象存储 + DiskANN）。

## 十三、性能与基准

- **ann-benchmarks**：Milvus 在 glove-100、sift-128 上 IVF_PQ 接近 FAISS baseline；
- **HNSW recall**：默认参数下 95%+ @ R@10（百万级）；
- **DiskANN**：十亿级向量，单节点 NVMe SSD，**1-10ms / query**，内存 **< 100GB**；
- **CAGRA（GPU）**：相比 CPU HNSW **5-50x** 加速（4096 维、千万级向量）；
- **官方 benchmark**：https://milvus.io/docs/benchmark.md。

## 十四、运维与可观测性

- **Metrics**：Prometheus（`internal/http/` 暴露 `/metrics`）；
- **日志**：结构化日志（zap），支持 JSON / text；
- **Tracing**：OpenTelemetry（`internal/distributed/streamingcoord/`）；
- **健康检查**：`/health` endpoint；
- **备份恢复**：Snapshot 工具 + 官方 Milvus Backup；
- **可视化**：Attu（Web UI，独立项目）；
- **混沌测试**：内置混沌工程框架 `internal/distributed/`。

## 十五、生态与集成

- **AI 框架**：LangChain、LlamaIndex、Haystack、DSPy 默认后端；
- **ETL**：Towhee（Zilliz 开源）、Airbyte、Fivetran；
- **对象存储**：S3、MinIO、阿里云 OSS、腾讯云 COS、Azure Blob、GCS；
- **MQ**：Pulsar、Kafka、RocketMQ（社区版支持）；
- **元数据**：etcd、TiKV（推荐生产）；
- **监控**：Prometheus + Grafana 官方 Dashboard；
- **Operator**：Milvus Operator（K8s CRD）。

## 十六、核心论文与算法原理

- **HNSW**：Malkov & Yashunin, 2016, IEEE TPAMI；
- **IVF + PQ**：Jégou et al., 2011；
- **DiskANN**：Subramanya et al., 2019, NeurIPS；
- **ScaNN**：Guo et al., 2020, ICML；
- **CAGRA**：Ootomo et al., 2023, NVIDIA；
- **RRF**：Cormack et al., 2009；
- **RAFT 共识**：Ongaro & Ousterhout, 2014；
- **Snapshot Isolation**：Wu et al., 2017（FoundationDB）。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | Proxy 入口 | `internal/proxy/task.go` | 请求处理 |
| P0 | QueryNodev2 | `internal/querynodev2/` | 检索核心 |
| P0 | Knowhere IVF | `internal/core/thirdparty/knowhere/faiss/IndexIVF*` | 倒排 |
| P0 | Knowhere HNSW | `internal/core/thirdparty/knowhere/hnswlib/` | 图索引 |
| P1 | Knowhere DiskANN | `internal/core/thirdparty/knowhere/diskann/` | SSD 图 |
| P1 | Segcore | `internal/core/src/segcore/segment_c.cpp` | 段加载 |
| P1 | TSO | `internal/tso/` | 时间戳 |
| P2 | RootCoord | `internal/rootcoord/` | DDL |
| P2 | DataCoord | `internal/datacoord/` | 段管理 |
| P2 | IndexCoord | `internal/indexcoord/` | 索引调度 |
| P2 | StreamingCoord | `internal/distributed/streamingcoord/` | v2.5 流式 |

## 十八、源码规模与开发语言

- **Go**：约 65% LOC（编排、Coord、QueryNode、Proxy）；
- **C++**：约 30% LOC（Knowhere 内核、Segcore、Arrow 绑定）；
- **Python**：约 3% LOC（client、SDK 测试）；
- **Proto**：约 2% LOC（接口定义）；
- **构建系统**：Go modules + CMake（Knowhere + CGO）；
- **CI**：GitHub Actions + Jenkins（Milvus 内部）；
- **LOC 总量**：约 80 万行（含 Knowhere）。

## 十九、学习资源

- **官方文档**：https://milvus.io/docs
- **GitHub**：https://github.com/milvus-io/milvus
- **论坛**：https://discuss.milvus.io/
- **Zilliz University**：https://university.zilliz.com/
- **Slack / Discord**：社区入口；
- **博客**：https://milvus.io/blog；
- **书籍**：《Milvus 实战》（社区）；
- **CSDN / 知乎**：大量中文实践。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 | 原生 |
| Linux ARM64 | 官方支持（NVIDIA Jetson / Graviton）|
| macOS | 开发模式（不支持生产）|
| Windows | WSL2 推荐 |
| Docker / Compose | 官方 `docker-compose.yml` |
| Helm | 官方 Chart |
| Operator | K8s Operator |
| Zilliz Cloud | 全托管 |

## 二十一、变体与衍生项目

- **Zilliz Cloud**：商业全托管；
- **Milvus Enterprise**：商业版（RBAC、Audit、PITR）；
- **Milvus Lite**（v2.4+）：Python 嵌入式模式，类似 Chroma；
- **Milvus Lite WASM**：浏览器内运行；
- **社区分叉**：自托管定制版。

## 二十二、风险与陷阱

1. **部署复杂**：生产至少需要 9 个 Pod（4 Coord + 1 Standalone Coord + 1 Storage + 3 QueryNode 等），单机演示需用 Milvus Standalone；
2. **Knowhere 升级慢**：C++ 内核版本与 Go 编排版本强耦合，跨版本升级需阅读 UPGRADE.md；
3. **HNSW 大数据召回**：默认 `ef=64`，大数据（亿级）需要 `ef=200+`，延迟上升；
4. **IVF 训练成本**：nlist=65536 时训练需分钟-小时级，**必须先插入足量样本再 build index**；
5. **DiskANN SSD 依赖**：随机读性能决定延迟，建议 NVMe；
6. **CGO 调试困难**：Go 侧崩溃往往在 C++ 栈，需要配合 gdb + knowhere 调试符号；
7. **存储耦合**：MQ + 对象存储的可用性决定 SLA，Pulsar 集群运维复杂；
8. **v2.x 与 v1.x 数据格式不兼容**：升级需重新导入；
9. **多租户隔离不彻底**：Database 隔离是软隔离，资源竞争仍可能。

## 二十三、与本项目的关联

- **`engineering/include/index/`** 中已有部分 HNSW 实现，可参考 Knowhere 抽象设计统一索引接口；
- **`learning/notes/`** 可沉淀 Knowhere C++ 实现细节；
- **`engineering/rag/`** 可作为向量后端（Milvus Lite / Standalone）；
- **MiniVecDB**：参考 Milvus 的 Segment 抽象简化实现向量段管理。

## 二十四、Segment 模型、封段策略与逻辑删除

> §8.8 只讲了「索引构建」的流程，本节下沉到 **数据在段里怎么躺、什么时候封、删除怎么生效、查询怎么定可见范围** 这四件事。

### 24.1 三态 Segment：C API 契约

segcore 用一个 C enum 固定了 Segment 的形态，文件顶部注释明确写了 **Growing / Sealed 的枚举值不可改**（Go 侧通过 CGO 共享同一份定义）：

`internal/core/src/common/type_c.h:27`
```c
enum SegmentType {
    Invalid = 0,
    Growing = 1,
    Sealed = 2,
    Indexing = 3,
};
```

- **Growing（可变段）**：驻留在写入节点内存里，承载 `InsertRecord`（行存，可变长）；可被反复 append、可被原地删除、可以被 seal 多次（每次 seal 后开一个新的）。
- **Sealed（冻结段）**：不可变，只接受增量 delta log（`StreamPush`）；存储上按 Field 切 chunk（默认 `chunk_rows = 32 * 1024`，见 `internal/core/src/segcore/SegcoreConfig.h:288`），并持有列存版 `SealedIndexingRecord`（带 per-chunk 偏移表）。
- **Indexing**：仅索引构建期间的过渡态，不属于对外契约。

**最关键的一点**：Growing 段的向量检索走的是 **interim index（临时索引）**，不是用户指定的索引。Milvus 会把用户的 `HNSW` / `IVF_PQ` 映射成一个「能持续增量 append 的近似索引」：

`internal/core/src/segcore/IndexConfigGenerator.cpp:43`
```cpp
if (origin_index_type_ == knowhere::IndexEnum::INDEX_SPARSE_WAND) {
    index_type_ = knowhere::IndexEnum::INDEX_SPARSE_WAND_CC;
} else if (is_sparse_) {
    index_type_ = knowhere::IndexEnum::INDEX_SPARSE_INVERTED_INDEX_CC;
} else {
    index_type_ = config.get_dense_vector_intermin_index_type();   // 默认 IVFFLAT_CC / SCANN DVR
}
```

interim index 的默认参数（`internal/core/src/segcore/SegcoreConfig.h:288` 起）：

| 参数 | 默认值 | 作用 |
|---|---|---|
| `chunk_rows` | `32 * 1024` | 每个 chunk 的行数 |
| `nlist` | `100` | interim 索引的聚类数（远小于 sealed 索引） |
| `nprobe` | `4` | interim 索引的探测聚类数 |
| `sub_dim` | `2` | PQ 子空间维数下限 |
| `refine_ratio` | `3.0` | 量化召回后的 refine 放大倍数 |
| `build_ratio` | `0.1` | 索引满载比例，达到即触发正式索引构建 |

**为什么 interim 的 `nlist` 只有 100**：Growing 段通常只有几十万行，聚类数过大时每簇样本不足、倒排链过长反而拖慢检索；且 interim 索引会被反复重建，训练开销敏感。sealed 索引的 `nlist` 才会按数据量放大（十万~百万行时常用 4096~65536）。

### 24.2 段容量标定：maxRowNum 由 schema 反推

段不是按「行数」封的，而是按 **字节数**。`maxRowNum` 在分配时段就由 schema 推出：

`internal/datacoord/segment_allocation_policy.go:51`
```go
threshold := Params.DataCoordCfg.SegmentMaxSize.GetAsFloat() * 1024 * 1024
return int(threshold / float64(sizePerRecord)), nil
```

即

```
maxRowNum = ⌊ segmentMaxSize / sizePerRecord(schema) ⌋
segmentMaxSize 默认 1024 MB（`configs/milvus.yaml:757`）
```

`sizePerRecord` 由 `typeutil.EstimateSizePerRecord(schema)` 估算，取决于向量维度、变长字段平均长度、字段数量。所以一个 1024 维 float32 向量段能装的行数（约 25 万）远少于一个只含 int64 主键的段（约 1300 万）。

### 24.3 四条封段策略 + 抖动

DataCoord 的封段判定是四条独立策略的 OR，任意一条命中即 seal：

`internal/datacoord/segment_manager.go:215`
```go
func defaultSegmentSealPolicy() []SegmentSealPolicy {
	return []SegmentSealPolicy{
		sealL1SegmentByBinlogFileNumber(Params.DataCoordCfg.SegmentMaxBinlogFileNumber.GetAsInt()),
		sealL1SegmentByLifetime(),
		sealL1SegmentByCapacity(Params.DataCoordCfg.SegmentSealProportion.GetAsFloat()),
		sealL1SegmentByIdleTime(600s, 16MB, 1024MB),
	}
}
```

1. **binlog 文件数**（默认 32）：只统计**第一个 Field** 的 binlog 数，避免被字段数放大 —— 那段循环里有个显式 `break`（`internal/datacoord/segment_allocation_policy.go:151`）。
2. **生命周期**：`now − startPosition.ts ≥ SegmentMaxLifetime`（默认 86400s），防止某个段被无限 append。
3. **容量（含抖动）** —— 最微妙的一条：

`internal/datacoord/segment_allocation_policy.go:123`
```go
jitter := paramtable.Get().DataCoordCfg.SegmentSealProportionJitter.GetAsFloat()
ratio := (1 - jitter*rand.Float64())
return float64(segment.GetNumOfRows()) >= sizeFactor*float64(segment.GetMaxRowNum())*ratio
```

伪代码：
```
ratio      ← 1 − jitter · U(0,1)          # U 为 [0,1) 均匀分布
shouldSeal ← numOfRows ≥ sealProportion · maxRowNum · ratio
```

默认 `sealProportion = 0.12`（`configs/milvus.yaml:759`）、`jitter = 0.1`（`configs/milvus.yaml:760`），所以实际阈值在 **10.8% ~ 12%** 之间随机浮动。**抖动的作用不是"凑数"，而是打散 seal 时刻** —— 若没有抖动，同 schema 的所有 Collection 会在同一行数被 seal，全集群形成写放大尖峰。

也正因阈值只有 12%，DataCoord 在段还剩 88% 空间时就允许继续往里分配新行（`internal/datacoord/segment_allocation_policy.go:86`），以保证「一定能 seal 掉」：

```go
maxRowsWithSealProportion := int64(float64(segment.GetMaxRowNum()) * ...SegmentSealProportion.GetAsFloat())
free := maxRowsWithSealProportion - segment.GetNumOfRows() - allocSize
if free < 0 { continue }    // 剩余量连 12% 都不到 → 不再分配，等 seal
```

4. **空闲时间**：`time.Since(lastWrittenTime) > 600s` 且段大小 > `minSizeFromIdleToSealed`（16 MB）。注释说明了动机（`internal/datacoord/segment_allocation_policy.go:162`）：用户持续小批量写入但忘了 seal，段迟迟达不到 12% 阈值，**主动 seal 可以触发 QueryCluster 的 handoff**，把查询压力从写入节点挪走；而不能简单调低 expiry 时间，否则会误伤「频繁小批写入」的场景。

### 24.4 Channel 级封段：两条

`internal/datacoord/segment_manager.go:224`
```go
func defaultChannelSealPolicy(meta *meta) []channelSealPolicy {
	return []channelSealPolicy{
		sealByTotalGrowingSegmentsSize(),
		sealByBlockingL0(meta),
	}
}
```

- `sealByTotalGrowingSegmentsSize`：一个 channel 上所有 Growing 段总大小超过阈值时，**挑最大的那个** seal（`lo.MaxBy`，见 `internal/datacoord/segment_allocation_policy.go:215`）。
- `sealByBlockingL0`：L0 delta log 堆积会阻塞 L0 compaction，因此把与最早 L0 段时间范围重叠的 Growing 段提前 seal。实现方式是按 `startPosition` 排序后做「时间区间扫描」，找出所有落在 `[minStartTs, ∞)` 的候选（`internal/datacoord/segment_allocation_policy.go:244`），阈值默认 `blockingL0SizeInMB: 64` / `blockingL0EntryNum: 5000000`（`configs/milvus.yaml:790`）。

### 24.5 TimestampIndex：快照隔离在 segcore 的落点

可见性不是「过滤每一行」，而是 **chunk 级时间戳栅栏**。

构建时对每个 chunk 取 min/max，并**断言 chunk 之间时间戳不交叉**（`internal/core/src/segcore/TimestampIndex.cpp:30`）：
```cpp
TimestampIndex::build_with(const Timestamp* timestamps, int64_t size) {
    ...
    for (int slice_id = 0; slice_id < num_slice; ++slice_id) {
        auto [min_v, max_v] = std::minmax_element(timestamps + offset, timestamps + offset + length);
        Assert(last_max_v <= *min_v);          // 段内行按时间戳全局有序
        offset += length;
        timestamp_barriers.push_back(*min_v);
        last_max_v = *max_v;
    }
    timestamp_barriers.push_back(last_max_v);
    Assert(std::is_sorted(timestamp_barriers.begin(), timestamp_barriers.end()));
```

查询期二分定位到**最后一个 `max_ts ≤ query_ts` 的 chunk**，直接返回区间：

`internal/core/src/segcore/TimestampIndex.cpp:63`
```cpp
std::pair<int64_t, int64_t>
TimestampIndex::get_active_range(Timestamp query_timestamp) const {
    if (query_timestamp >= max_timestamp_) return {size_, size_};   // 最常见：整段可见
    if (query_timestamp < min_timestamp_) return {0, 0};
    auto iter = std::upper_bound(timestamp_barriers_.begin(), timestamp_barriers_.end(), query_timestamp);
    int block_id = (iter - timestamp_barriers_.begin()) - 1;
    return {start_locs_[block_id], start_locs_[block_id + 1]};
}
```

复杂度：查询 **O(log C)**（C = chunk 数），空间 **O(C)**。对 Growing 段，`GenerateFakeSlices`（`internal/core/src/segcore/TimestampIndex.cpp:81`）在每次追加后根据实际 chunk 长度动态重切，保证每个 slice 至少 `min_slice_length` 行 —— 避免尾部切出过密的碎片 chunk 把栅栏撑爆。

**一致性级别 → guarantee timestamp 的换算**（Proxy 侧，`internal/proxy/util.go:1410`）：
```go
func parseGuaranteeTsFromConsistency(ts, tMax typeutil.Timestamp, consistency commonpb.ConsistencyLevel) typeutil.Timestamp {
	switch consistency {
	case commonpb.ConsistencyLevel_Strong:
		ts = tMax
	case commonpb.ConsistencyLevel_Bounded:
		ratio := Params.CommonCfg.GracefulTime.GetAsDuration(time.Millisecond)
		ts = tsoutil.AddPhysicalDurationOnTs(tMax, -ratio)
	case commonpb.ConsistencyLevel_Eventually:
		ts = 1
	}
	return ts
}
```
- **Strong**：`ts = tMax`（各 channel 的最大 MVCC 时间戳），配合等待所有 channel 追平 → 强一致，读延迟最高。
- **Session**：**不覆写 `ts`**，沿用客户端上次读到的 `guarantee_ts`（`internal/proxy/util.go:1423` 的 `parseGuaranteeTs` 只处理 Strong / Bounded 两个分支，Session 走原值透传）。
- **Bounded**：`ts = tMax − gracefulTime`（**物理时间**，用 `tsoutil.AddPhysicalDurationOnTs` 从逻辑时间戳里扣），容忍最多一个 `gracefulTime` 的陈旧 → 免去等待，延迟大幅下降。
- **Eventually**：`ts = 1`，不做任何等待（注意是**逻辑时间戳 1** 而非 0，保证不会退化成「无时间戳」的裸扫描路径）。

### 24.6 逻辑删除：无锁 SkipList 墓碑 + 增量快照

删除不物理抹数据，而是写一条 `(delete_ts, row_id)` 墓碑进 **无锁 SkipList**：

`internal/core/src/segcore/DeletedRecord.h:48`
```cpp
// a lock-free list for multi-thread insert && read
using SortedDeleteList =
    folly::ConcurrentSkipList<std::pair<Timestamp, Offset>, Comparator>;
```

Comparator 先比 `delete_ts` 再比 `row_id`（`internal/core/src/segcore/DeletedRecord.h:40`），保证同一批内删除的顺序稳定 —— 这是下面快照游标能单调推进的前提。

写入语义（`internal/core/src/segcore/DeletedRecord.h:153`）：
```
for i, pk in enumerate(pks):
    row_id, delete_ts ← search_pk_func(pk, timestamps[i])
    if deleted_mask_.size() > row_id and deleted_mask_[row_id]: return   # 已删，幂等
    if insert_ts ≠ 0 and delete_ts ≤ insert_ts: return                 # 删除早于插入 ⇒ 无效删除
    skip_list.insert((delete_ts, row_id))
    if sealed: deleted_mask_.set(row_id)                               # 长度固定
    else:      deleted_mask_.resize(insert_record_->row_count())      # Growing 随行数增长
             ; deleted_mask_.set(row_id)
```

那条 `delete_ts ≤ insert_ts` 的检查（`internal/core/src/segcore/DeletedRecord.h:168`）是分布式场景的必备防线：**乱序到达的 delete 可能早于对应的 insert**，若不丢弃，这条 delete 会把一条「还没写入」的行永久标记为已删。

查询期把墓碑 OR 进过滤 bitset —— 这正是 §8.7 里「HNSW/IVF 检索时携带 Bitset 跳过节点」的数据来源。`DeletedRecord::Query` 有两条路径（`internal/core/src/segcore/DeletedRecord.h:228`）：

```
Query(bitset, insert_barrier, query_ts):
    snapshot ← atomic_load(latest_snapshot_)
    if snapshot ≠ nil and query_ts ≥ snapshot.max_ts:              # 快路径
        bitset |= snapshot.bitset                                  # O(N/64)，完全不碰 SkipList
        return
    # 慢路径：定位「最后一个 dump 点 ≤ query_ts」的快照，跳过之前全部迭代
    loc ← max{ i : snapshots_[i].ts ≤ query_ts }
    bitset |= snapshots_[loc].bitmap
    it   ← skip_list.lower_bound(snap_next_pos_[loc])
    while it ≠ end and it.ts ≤ query_ts:                            # 只扫未 dump 的增量尾部
        if it.row_id < insert_barrier: bitset.set(it.row_id)
        it++
```

复杂度对比：无快照时慢路径是 **O(D·log D)**（D = 墓碑数）；有快照时降到 **O(B + D′)**，其中 B = 增量尾部条数。

**快照 dump**：`DumpSnapshot`（`internal/core/src/segcore/DeletedRecord.h:281`）每积累 `DELETE_DUMP_BATCH_SIZE = 10000` 条（`internal/core/src/common/Consts.h:91`）就把 bitmap 物化一份快照，并记下快照时 SkipList 的游标位置 `snap_next_pos_`：
```
while (total − dumped) > 10000:
    bitmap ← 已 dump 的历史快照 OR 上本次 10000 条墓碑
    snapshots_.push((dump_ts, bitmap.clone()))
    snap_next_pos_.push(*it)                    # 游标，单调递增
    dumped_entry_count_ += 10000
```
快照触发点在 `StreamPush` 里（`internal/core/src/segcore/DeletedRecord.h:128`）：只有当**整批删除的时间戳都 ≥ max_load_timestamp_** 时才 dump —— 半新半旧的一批做不出正确的单调游标。

**内存计费粒度**：只有 Sealed 段把墓碑计入 caching layer 的加载资源，且按 **64 KB 粒度** charge / refund，避免每次 push 都去惊动全局资源管理器：

`internal/core/src/segcore/DeletedRecord.h:189`
```cpp
constexpr int64_t MIN_DELTA_SIZE = 64 * 1024;
auto new_estimated_size = size();
if (std::abs(new_estimated_size - estimated_memory_size_) > MIN_DELTA_SIZE) { ... }
```

Growing 段的墓碑内存不单独计费，已包含在 `SegmentGrowingImpl::EstimateSegmentResourceUsage()` 里（注释见 `internal/core/src/segcore/DeletedRecord.h:204`）—— 因为 Growing 段被当作一个整体单位统计，不按字段拆分。

### 24.7 Growing 段的 interim index 何时转正

Growing 段一直用 interim index，逐批插入时判断是否已到转正阈值：

`internal/core/src/segcore/FieldIndexing.cpp:145`
```cpp
if (type == DataType::VECTOR_FLOAT &&
    valid_count >= indexing_ptr->get_build_threshold()) {
    indexing_ptr->AppendSegmentIndexDense(...);
}
```

阈值公式（`internal/core/src/segcore/IndexConfigGenerator.cpp:94`）：
```
buildThreshold = max(⌊ maxIndexRowCount · buildRatio ⌋, nlist · 39)
```
其中 `buildRatio = 0.1`，`nlist = 100` ⇒ 下界 **3900** 行。**为什么是 39 × nlist**？k-means 训练需要每个聚类摊到足够多的训练样本才收敛得住 —— nlist=100 时对应 3900 个训练样本，是 O(nlist) 的经验常数（远低于 faiss 常用的 39×nlist×iters 上界取法）。Sealed 段从 binlog 重建索引时用同一判据（`internal/core/src/segcore/ChunkedSegmentSealedImpl.cpp:6751`）。

`maxIndexRowCount` 则由 QueryNode 从 schema 反推（`internal/querynodev2/segments/index_meta.go:49`）：
```
maxIndexRowCount = ⌊ segmentMaxSize · sealProportion / sizePerRecord ⌋
```

## 二十五、索引参数体系与 AUTOINDEX 自动选型

### 25.0 先纠正一个前提：Knowhere 已不在 Milvus 仓库内

`internal/core/thirdparty/knowhere/` 目录下**只有 2 个文件**（`CMakeLists.txt` 和 `knowhere.pc.in`），内核由 CMake 在构建期从独立仓库拉取：

`internal/core/thirdparty/knowhere/CMakeLists.txt:17`
```cmake
set( KNOWHERE_VERSION d7cfd888 )
set( GIT_REPOSITORY  "https://github.com/zilliztech/knowhere.git")
```

这意味着 §8.2~§8.6 描述的 **HNSW 构建、IVF 训练、PQ 码本训练、Vamana 图构建这些算法本体都在 `zilliztech/knowhere`**，Milvus 仓库里读得到的是**参数如何被组装、默认值是什么、索引能力如何被注册**。本节讲的正是后者 —— 这也是「学习 Milvus 如何用索引」与「学习索引算法本身」的分界线。

### 25.1 索引能力位图：Go 侧定义、C++ 侧供数

Milvus 用一个 uint64 位图描述每个向量索引的能力，位定义在 Go 侧，**数据通过 CGO 从 C++ 查询**：

`internal/util/vecindexmgr/vector_index_mgr.go:49`
```go
NOTrainFlag uint64 = 1 << 16  // 无需训练索引结构（FLAT 类），召回率 100%
KNNFlag    uint64 = 1 << 17  // 默认 KNN，即精确检索
GpuFlag    uint64 = 1 << 18  // 需要 GPU 设备
MmapFlag   uint64 = 1 << 19  // 主内存可用 mmap 管理，显著提升容量
MvFlag     uint64 = 1 << 20  // 支持物化视图加速过滤检索
DiskFlag   uint64 = 1 << 21  // 需要磁盘（DiskANN）
```

装载过程（`internal/util/vecindexmgr/vector_index_mgr.go:118`）：
```go
func (mgr *vecIndexMgrImpl) init() {
	size := int(C.GetIndexListSize())
	vecIndexList := make([]unsafe.Pointer, size)
	vecIndexFeatures := make([]uint64, size)
	C.GetIndexFeatures(...)                       // CGO 调 Knowhere
	for i := 0; i < size; i++ {
		key := C.GoString((*C.char)(vecIndexList[i]))
		mgr.features[key] = vecIndexFeatures[i]
	}
}
```

**这是 Knowhere 抽离后的核心解耦点**（呼应 §6 的「Knowhere 抽离」）：新增一种索引只需在 Knowhere 侧注册，Go 编排层零改动即可感知其能力。反过来说，看 Milvus 代码判断「某索引支不支持 GPU/mmap」是**查不到的** —— 必须去 Knowhere 看 `IndexFactory` 的注册表。

### 25.2 AUTOINDEX 默认参数：CPU 与 GPU 两套真值

`autoIndex.enable` 默认 **false**（开源版需显式开启，`pkg/util/paramtable/autoindex_param.go:84`）。开启后按向量数据类型查表：

| 向量类型 | 参数键 | 默认值 | 出处 |
|---|---|---|---|
| 稠密 float | `autoIndex.params.build` | 见下（按 CUDA 分叉） | `pkg/util/paramtable/autoindex_param_nocuda.go:26` / `pkg/util/paramtable/autoindex_param_cuda.go:26` |
| int8 | `autoIndex.params.int8.build` | `{"M": 18, "efConstruction": 240, "index_type": "HNSW", "metric_type": "COSINE"}` | `pkg/util/paramtable/autoindex_param.go:120` |
| 二值 | `autoIndex.params.binary.build` | `{"nlist": 1024, "index_type": "BIN_IVF_FLAT", "metric_type": "HAMMING"}` | `pkg/util/paramtable/autoindex_param.go:129` |
| 稀疏 | `autoIndex.params.sparse.build` | `{"index_type": "SPARSE_INVERTED_INDEX", "metric_type": "IP"}` | `pkg/util/paramtable/autoindex_param.go:111` |
| 大 topk | `autoIndex.params.largeTopK.build` | `{"nlist": 128, "index_type": "IVF_SQ8", "metric_type": "COSINE"}` | `pkg/util/paramtable/autoindex_param.go:155` |

**HNSW 的 M / efConstruction 实际默认值 = 18 / 240**（不是论文与社区常用的 16 / 200）。稠密 float 的默认值按是否 CUDA 编译**分叉为两套**：

`pkg/util/paramtable/autoindex_param_nocuda.go:26`（`//go:build !cuda`）
```go
DefaultValue: `{"M": 18 ,"efConstruction": 240,"index_type": "HNSW_SQ", "sq_type": "SQ4U", "refine": true, "refine_type": "FP16", "metric_type": "COSINE"}`,
```
`pkg/util/paramtable/autoindex_param_cuda.go:26`（`//go:build cuda`）
```go
DefaultValue: `{"intermediate_graph_degree":64, "graph_degree": 32, "index_type": "GPU_CAGRA", "metric_type": "COSINE"}`,
```

两套默认值的设计意图完全不同：
- **CPU 走 HNSW_SQ + SQ4U 标量量化 + FP16 refine**：图上节点存 SQ4（4 bit/维，1/8 压缩）；召回时用 FP16 原始值重算 refine 补回精度。**召回由「图遍历 + 量化距离」两层构成**，refine 是必需的补救步骤。
- **CUDA 走 GPU_CAGRA**，且 `intermediate_graph_degree = 64 = 2 × graph_degree = 32`。这就是 CAGRA 的核心设计：**构建期用宽图保质量，查询期用窄图提速度**。中间图边数翻倍，构建期 O(M·N) 的边插入成本只多约一倍，但能让查询期的窄图覆盖到更远的邻居，从而在相同召回下把查询期的 `M` 减半。

注意 §8.1 索引表里的 `HNSW` / `HNSW_SQ` / `HNSW_PQ` 在 AUTOINDEX 场景下**不是并列选项** —— Milvus 默认选的是 `HNSW_SQ`（量化图），不是全精度 `HNSW`。

### 25.3 refine_type 必须与向量数据类型对齐

`internal/util/indexparamcheck/index_params_validation.go:141`
```go
func AdjustAutoIndexParamsByDataType(config map[string]string, dataType schemapb.DataType) map[string]string {
	refineTypeLower := strings.ToLower(refineType)
	switch dataType {
	case schemapb.DataType_Float16Vector:
		if refineTypeLower == "bf16" || refineTypeLower == "fp32" {
			requiredRefineType = "FP16"
		}
	case schemapb.DataType_BFloat16Vector:
		if refineTypeLower == "fp16" || refineTypeLower == "fp32" {
			requiredRefineType = "BF16"
		}
	}
	...
}
```

规则：**FP16 数据不能配 BF16/FP32 refine，反之亦然**；其他值（如 `sq8`）不修改。原因是 refine 阶段要拿原始向量算精确距离，**量化位宽与存储位宽必须一致**，否则转换有损、refine 反而污染结果。同样的对齐逻辑在 C++ 侧还有一份（`internal/core/src/segcore/IndexConfigGenerator.cpp:112` 的 `GetBuildBaseParams`：BFLOAT16 + FP16_QUANT 会改写成 BF16_QUANT，反之亦然）。

### 25.4 标量索引族与 AUTOINDEX 映射表

C++ 侧枚举了 10 种标量索引（`internal/core/src/index/ScalarIndex.h:39`）：
```cpp
enum class ScalarIndexType {
    NONE = 0, BITMAP, STLSORT, MARISA, INVERTED, HYBRID, JSONSTATS, RTREE, NGRAM, FMINDEX,
};
```

Go 侧的**默认**标量索引类型是 **INVERTED**（`internal/proxy/util.go:83`）：
```go
DefaultArithmeticIndexType = indexparamcheck.IndexINVERTED
DefaultStringIndexType     = indexparamcheck.IndexINVERTED
```

开启 `scalarAutoIndex.enable` 后，**按标量类型自动选型**（`pkg/util/paramtable/autoindex_param.go:220`）：
```json
{"int": "HYBRID", "varchar": "HYBRID", "bool": "BITMAP",
 "float": "HYBRID", "json": "HYBRID", "geometry": "RTREE", "timestamptz": "STL_SORT"}
```

| 类型 | 数据结构 | 适合 | 查询复杂度 | 代码 |
|---|---|---|---|---|
| **INVERTED** | Tantivy（Rust）倒排 | 高基数 varchar、数值等值/范围 | 建 O(N)，查 O(log D + P) | `internal/core/src/index/InvertedIndexTantivy.cpp` |
| **BITMAP** | 每个 distinct 值一个位图 | 低基数（bool、枚举） | O(N/64) 或 O(card·N/64) | `internal/core/src/index/BitmapIndex.cpp` |
| **HYBRID** | BITMAP + INVERTED 二选一 | int / varchar / float | 同上，按基数自动切 | `internal/core/src/index/HybridScalarIndex.cpp` |
| **STL_SORT** | 全量排序 + 二分 | 时间戳等天然有序列 | 建 O(N log N)，查 O(log N + K) | `internal/core/src/index/ScalarIndexSort.cpp` |
| **NGRAM** | 字符 n-gram 倒排 | 子串 / 模糊匹配 | 建 O(N·n)，查 O(P) | `internal/core/src/index/NgramInvertedIndex.cpp` |
| **RTREE** | R-Tree | geometry 空间查询 | 建 O(N log N)，查 O(log N + K) | `internal/core/src/index/RTreeIndex.cpp` |
| **MARISA** | Trie 前缀树 | 前缀匹配（遗留） | O(len) | `internal/core/src/index/StringIndexMarisa.cpp` |
| **SKIP** | min/max 跳跃 | 范围快速排除 | O(1) 排除 | `internal/core/src/index/SkipIndex.cpp` |
| **FMINDEX** | FM-index（完美哈希 + 位向量） | 超大基数等值 | O(log σ) | `internal/core/src/index/FMIndex.cpp` |
| **JSONSTATS** | JSON path 统计 | JSON 内嵌套路径 | — | `internal/core/src/index/json_stats/` |

**HYBRID 的基数切换阈值**：默认 `DefaultBitmapCardinalityLimit = 100`（`pkg/util/paramtable/autoindex_param.go:77`），上限 1000（`internal/util/indexparamcheck/constraints.go:52`）。校验在 `internal/util/indexparamcheck/hybrid_index_checker.go:31`：
```go
if !CheckIntByRange(params, common.BitmapCardinalityLimitKey, 1, MaxBitmapCardinalityLimit) { ... }
```
即 distinct 值 ≤ 100 → 走 BITMAP（更快、更省）；> 100 → 走 INVERTED。这条规则是**数据驱动的**：同样的字段，数据集里 distinct 值从 3 个涨到 1 万个，最优索引会自己切换。

**只有 BITMAP 支持 offset cache**（`internal/util/indexparamcheck/index_type.go:58`）：
```go
func IsOffsetCacheSupported(indexType IndexType) bool { return indexType == IndexBitmap }
```
因为 bitmap 天然能把「命中 offset 集合」直接转成行号数组，无需回表查主键。

**mmap 支持的标量索引**（`internal/util/indexparamcheck/index_type.go:66`）：`INVERTED / BITMAP / HYBRID / TRIE / NGRAM / FMINDEX`。**`STL_SORT` 不支持 mmap** —— 二分查找要求值域常量驻内存，无法只把数据留在页缓存里靠缺页换入（那会让每次二分都吃 IO）。

### 25.5 稀疏索引：Milvus 的「BM25」是两段式的

这是最容易误解的一点：**Milvus 的 BM25 Function 里没有任何 k1 / b 参数**。

- **BM25 Function（Go 侧，`internal/util/function/bm25_function.go`）**：把 VARCHAR 文本经 analyzer 分词，转成**稀疏向量**（term → 权重）写进 SparseFloatVector 字段。它只做「文本 → 稀疏向量」的编码，**不含 BM25 打分公式**。
- **SPARSE_INVERTED_INDEX（C++ Knowhere 侧）**：真正的 BM25 打分倒排在这里，k1 / b / idf 全在这一层。

可选的打分算法（`internal/util/indexparamcheck/constraints.go:82`）：
```go
SparseInvertedIndexAlgos = []string{"TAAT_NAIVE", "DAAT_WAND", "DAAT_MAXSCORE",
                                   "BLOCK_MAX_MAXSCORE", "BLOCK_MAX_WAND", "SINDI"}
```
- **TAAT（Term-at-a-time）**：逐 term 累加各 posting list 的贡献。`TAAT_NAIVE` 是最朴素实现 —— 每个 term 把整条倒排链扫一遍，复杂度 O(Σ|posting(t)|)。查询 term 少、单个 term 高频时最划算。
- **DAAT（Document-at-a-time）**：逐候选文档打分。`DAAT_MAXSCORE` 在打分前先用 term 的分数上界做「当前候选不可能进 topK」的剪枝。
- **WAND**：**全局上界 + pivot**。对查询 term 按上界降序排序，累加上界找到第一个「累加上界 ≥ 当前 topK 阈值」的文档作 pivot，其前所有文档只需与 pivot 比较而无需精算；随后由 pivot 动态决定每条 posting 链推进多少位。复杂度从「扫全部 posting」降到「只算可能进 topK 的文档」。
- **Block-Max-WAND**：把倒排按块（block）切分，缓存**每个块内每个 term 的最大分**。先用块级上界跳过大块，再在候选块内做 WAND pivot。剪枝更紧（块内上界比全局上界小得多），代价是额外的块级元数据。`BLOCK_MAX_MAXSCORE` 是同族的打分版本。

Block-Max 剪枝的基本形态：
```
# 查询 q = {t1..tm}，每 term 带 (blockMaxScore[t][b], postings[t])
while block b 还有候选:
    ub(b) ← Σ_t blockMaxScore[t][b]          # 块内所有 term 最大贡献之和
    if ub(b) < 当前 topK 第 K 名:
        skip 整个块                          # 上界剪枝，安全
    else:
        for 候选文档 d in b:
            score(d) ← Σ_{t ∈ q ∩ d} w_t · idf_t · tf(t,d)   # 精算
            push topK
```

`SINDI` 是 Milvus 自研的稀疏索引变体（支持更多维度组合与持久化布局）。

`drop_ratio_build`（`internal/util/indexparamcheck/constraints.go:46`）是稀疏索引专有参数：构建时随机丢弃一部分低权重维度以压缩体积。但**不允许用于 Growing 段的 interim index**（`internal/core/src/segcore/IndexConfigGenerator.cpp:82`）—— 丢弃是随机的，重复丢弃会让同一段数据的稀疏维度集合与已入库的 posting 不一致，破坏增量 append 的正确性。

## 二十六、Compaction 体系：L0/L1 分层、触发判定与合并装箱

### 26.1 Segment 分层：L0 / L1 / L2

`pkg/proto/data_coord.proto:24`
```proto
enum SegmentLevel {
  Legacy = 0; // zero value for legacy logic
  L0 = 1;     // L0 segment, contains delta data for current channel
  L1 = 2;     // L1 segment, normal segment, with no extra compaction attribute
  L2 = 3;     // L2 segment, segment with extra data distribution info
}
```
- **L0**：承载当前 channel 的**增量 delta log**（删除 + update），是 L1 compaction 的输入；由 L0 compaction 归并回 L1。
- **L1**：普通段（Legacy compaction 的产物），无额外属性。
- **L2**：带**数据分布信息**的段，供 clustering compaction 使用。

L0 compaction 把 Collection 分为「活跃 / 空闲」两条路（`internal/datacoord/compaction_policy_l0.go:87`）：
```go
if idleCollsSet.Contain(collID) {
    idleL0Views = append(idleL0Views, labelViews...)
} else {
    activeL0Views = append(activeL0Views, labelViews...)
}
events[TriggerTypeLevelZeroViewChange] = activeL0Views
events[TriggerTypeLevelZeroViewIDLE]    = idleL0Views
```
活跃 Collection 每 10s 触发一轮（`levelzero.triggerInterval: 10`，`configs/milvus.yaml:828`），空闲 Collection 走低频路径；强制触发阈值 `forceTrigger.minSize: 8MB` / `maxSize: 64MB`（`configs/milvus.yaml:830`）。候选筛选要求段 **healthy + flushed + 不在 compacting + 不在 importing + level == L0**（`internal/datacoord/compaction_policy_l0.go:117`）。

### 26.2 单段合并（Single Compaction）的四个判据

`ShouldDoSingleCompaction`（`internal/datacoord/compaction_trigger.go:739`）按顺序**短路判定**，任一命中就触发「这个段自己和自己合」：

```
ShouldDoSingleCompaction(segment, compactTime):
    # ① 严格容差路径：最早时间戳已过 expireTime
    if ShouldCompactExpiry(EffectiveTimestamp(stats.timestampFrom, commitTs), compactTime):
        return true

    # ② 过期比例/体积路径（20% 分桶粒度，故意低估）
    ratio ← SingleCompactionRatioThreshold                      # 0.2
    if commitTs > 0:
        expiredFraction ← (commitTs < expireTime) ? 1.0 : 0.0
    else:
        qualifying ← count{ q ∈ stats.timestampQuantiles : 0 < q < expireTime }
        expiredFraction ← (qualifying ≥ 2) ? (qualifying − 1)/len(quantiles) : 0.0
    expiredApproxSize ← expiredFraction · stats.insertBinlogSize
    if expiredFraction ≥ ratio
       or expiredApproxSize > SingleCompactionExpiredLogMaxSize (10 MB):
        return true

    # ③ 墓碑堆积
    if hasTooManyDeletions(segment): return true

    # ④ 索引版本升级 / TTL 字段到期
    if ShouldRebuildSegmentIndex(segment): return true
    if ShouldCompactExpiryWithTTLField(compactTime, segment): return true
    return false
```

② 里 `(qualifying − 1)` 这个**减一**是有意为之（注释见 `internal/datacoord/compaction_trigger.go:770`）：`Q[i] < expireTime` 只能保证 `percentiles[i]` 比例的行已过期，在「各行大小相等」的假设下乘以 `insertBinlogSize` 会**高估**过期字节数 —— 当过期 binlog 比段内平均值更小时误差更大。于是主动**下调一个 20% 桶**来防止过度触发；精确边界由 ① 的严格路径兜底。**这是「宁可漏触发、不可误触发」的典型设计**：误触发一次 compaction 的代价（读全段 + 写新段 + 索引重建）远高于漏触发的代价。

③ `hasTooManyDeletions`（`internal/datacoord/compaction_trigger.go:643`）三个阈值：
```
deltaLogCount        > 200   → 触发（IO 次数累积）
deletedRows / rows   ≥ 0.2   → 触发（墓碑过密）
deltaLogSize         > 16 MB → 触发
```
其中 `deletedRows / numOfRows` 这条专门针对 **int64 主键** —— 代码注释写得很直白：`int64 PK tends to accumulates deleted row counts`。因为主键是 int64 时 segment 的实际行数会随删除收缩，墓碑数只增不减，比例会单调上升；而变长主键（varchar）在删除时会合并 tombstone 成 delete range，墓碑占比增长慢得多。

### 26.3 多段合并（Mix Compaction）的装箱算法

`generatePlans`（`internal/datacoord/compaction_trigger.go:447`）把候选段分三档，然后**贪心装箱**：

```
输入：segments、expectedSize = ⌊ SegmentMaxSize × SegmentCompactableProportion ⌋

# 1) 三档分类
for seg in segments:
    if signal.isForce or ShouldDoSingleCompaction(seg):  prioritized ← seg   # 高优先
    elif isSmallSegment(seg, expectedSize):               small      ← seg   # 小段
    else:                                                unplanned   ← seg

# 2) 阈值判定
isSmallSegment(seg, E)     : seg.size <  0.50 · E      # smallProportion
isCompactableSegment(t, E) : t         >  0.85 · E      # compactableProportion
                              （若 compactable < small 则抬到 small）

# 3) 装箱
toUpdate ← packer(prioritized);  toMerge ← packer(small)
satisfiedSize ← 0.85 · E ;  maxLeftSize ← E − satisfiedSize

while (pack, left) = toMerge.pack(E, maxLeftSize, minSegs=3, maxSegs=4096) ≠ ∅:
    buckets ← buckets ∪ {pack}            # 阶段一：只合并「攒得够一整桶」的小段
while (pack, _) = toUpdate.packWith(E, MaxInt64, 0, 4096, toMerge) ≠ ∅:
    buckets ← buckets ∪ {pack}            # 阶段二：高优先段不限剩余体积，全部装箱
for s in toUpdate.candidates:
    buckets ← buckets ∪ {{s}}             # 残留高优先段单独成桶
while (pack, _) = toMerge.pack(E, MaxInt64, 3, 4096) ≠ ∅:
    buckets ← buckets ∪ {pack}            # 阶段三（legacy）：把小段全塞进桶
squeezeSmallSegmentsToBuckets(toMerge.candidates, buckets, E)   # 阶段四：挤剩余量
```

三个阶段对应三种不同意图，**顺序不能换**：
- **阶段一**只接受「合并后能达到 0.85·E」的组合（`maxLeftSize = E − 0.85E = 0.15E`），保证产出的段足够饱满，不浪费一次合并机会。
- **阶段二**对高优先段**放开剩余体积上限**（`math.MaxInt64`）—— 这些段已经因为墓碑/过期被判定「必须处理」，不能因为桶已满就跳过。
- **阶段三、四**是兜底：把剩下的小段强行塞进已有的桶（squeeze），宁可让某桶略微超标也要把小段消化掉，否则小段永远达不到阶段一门槛，形成**饥饿死锁**。

复杂度：`pack` 是首适应降序装箱（段按大小预排序后单趟扫描），**O(N log N)**；段数上限 `maxSegs = 4096`（代码注释明确「simple compaction 里已无关」，保留为兜底保护）。

`minSegs = 3`（`dataCoord.compaction.min.segment`，`pkg/util/paramtable/component_param.go:6073`）：**至少 3 个段才值得发起一次合并**。2 段合并的收益（减少 1 个段）抵不上写放大成本。

**扩容上限**：`isExpandableSmallSegment`（`internal/datacoord/compaction_trigger.go:639`）允许合并后的段膨胀到 `1.25 × expectedSize`：
```go
return segment.getSegmentSize() < int64(float64(expectedSize)*(Params.DataCoordCfg.SegmentExpansionRate.GetAsFloat()-1))
```
`expansionRate = 1.25`（`configs/milvus.yaml:779`），yaml 注释说得很清楚（`configs/milvus.yaml:778`）：`During compaction, the size of segment # of rows is able to exceed segment max # of rows by (expansionRate-1) * 100%` —— 因为合并后的段要同时装下所有输入段的 binlog **和** 它们各自的 delta log。

### 26.4 策略族：7 种 compaction 并存

DataCoord 里 7 类 compaction 各占一个 Go 文件，按 intent 分派：

| 策略文件 | 用途 | 关键配置 |
|---|---|---|
| `compaction_policy_l0.go` | L0 → L1 合并 | `levelzero.triggerInterval: 10s` |
| `compaction_policy_single.go` | 单段内 delta + 墓碑合并 | `single.ratio.threshold: 0.2` |
| `compaction_task_mix.go` | 多段合并（装箱执行侧） | `mix.triggerInterval: 60s`（`configs/milvus.yaml:826`） |
| `compaction_policy_clustering.go` | 按数据分布重分布 | `clustering.enable: true` |
| `compaction_policy_forcemerge.go` | 强制合并 | — |
| `compaction_policy_storage_version.go` | 存储格式升级 | `compaction.storageVersion.enabled: true` |
| `compaction_policy_bump_schema_version.go` | schema 版本原地升级 | `bumpSchemaVersion.enabled: false` |

clustering compaction 的参数体现了「**受内存约束的 k-means**」设计（`configs/milvus.yaml:847` 起）：
```yaml
clustering:
  maxTrainSizeRatio: 0.8    # KMeans 训练集最大占数据量的比例，超出则下采样
  maxCentroidsNum: 10240    # 聚类中心数上限
  minCentroidsNum: 16       # 下限
  minClusterSizeRatio: 0.01 # 最小簇 / 平均簇大小
  maxClusterSizeRatio: 10   # 最大簇 / 平均簇大小
  maxClusterSize: 5g        # 单簇最大字节数
```
即：给定数据量 D，
```
nCentroids ← clamp(⌈D / preferSegmentSize · preferSegmentSizeRatio⌉, 16, 10240)
trainSize  ← min(D, 0.8·D)          # 超限则下采样
```
**为什么要下采样**：把 L2 段的全部向量读进内存做 k-means，亿级规模下就是 OOM。`maxTrainSizeRatio: 0.8` 是拿召回率换内存 —— 用 80% 的样本训练出的聚类中心，与用 100% 样本训练的聚类中心，在分段这个粗粒度任务上几乎没有区别。

### 26.5 Compaction 期间查询是否可见

**结论：compaction 不阻塞查询。** 输入段在新段提交前保持可读（QueryNode 上继续按老段检索），提交时通过 `segmentID → 新 segmentID` 的重定向映射把结果行号重映射到新段。

Compaction 结果携带 `base_manifest` 做**乐观并发 CAS**（`pkg/proto/data_coord.proto:864`）：
> base_manifest is the manifest the in-place schema-bump result was built on (the plan-pinned base). DataCoord adopts the result only if it still equals the segment's current manifest (optimistic-concurrency CAS)

即：若 compaction 执行期间段又被别的流程改过，结果**作废重来** —— 避免用陈旧快照覆盖新数据。

另外 compaction **会被未加载完的保护快照阻塞（fail-closed）**（`internal/datacoord/compaction_trigger.go:557`）：
```go
if signal.collectionID > 0 && t.meta.isCollectionCompactionBlocked(signal.collectionID) {
    mlog.Info(context.TODO(), "skip compaction candidates for collection due to unloaded protected snapshot RefIndex", ...)
}
```
宁可整个 Collection 不做 compaction，也不破坏用户的一致性快照。

## 二十七、混合检索的融合算子：RRF / Weighted RRF / 加权打分

§9.5 给的是 Python SDK 侧调用示例。**服务端真正的融合**发生在两处：
1. QueryNode 多路召回后交 Proxy 融合（经典路径）；
2. **Function chain**（v2.6+）：`internal/util/function/chain/operator_merge.go` 里的 `MergeOp`，把「BM25 函数 + 向量函数」的输出当成 DataFrame 做**算子级**融合 —— 这是当前推荐路径。

两种融合策略（`internal/util/function/chain/operator_merge.go:44`）：
```go
MergeStrategyRRF      MergeStrategy = "rrf"
MergeStrategyWeighted MergeStrategy = "weighted"
```

### 27.1 RRF 的精确公式与实现

`internal/util/function/chain/operator_merge.go:681`
```go
// Weighted RRF score: pathWeight / (k + rank), rank is 1-based.
// pathWeight defaults to 1 to preserve classic RRF scores.
rrfScore := float32(pathWeight / (op.rrfK + float64(rowIdx+1)))

if existingScore, exists := candidateScores[key]; exists {
    candidateScores[key] = existingScore + rrfScore
} else {
    candidateScores[key] = rrfScore
    candidateLocs[key] = idLocation{inputIdx: inputIdx, rowIdx: rowIdx}
}
```

数学形式：

```
score(d) = Σ_{i ∈ paths}  w_i / (k + rank_i(d))
```

- `rank_i(d)` 是 **1-based**：第 1 名 rank = 1（代码里是 `rowIdx+1`，因为 `rowIdx` 从 0 起）。
- `k` 是平滑常数，SDK 默认 `RRFRanker(k=60)`，参数键名就是字符串 `"k"`（`internal/proxy/task.go:144`：`RRFParamsKey = "k"`）。
- `w_i` 是路径权重，**未显式设置时默认 1.0**（`pathWeight := 1.0; if op.weightsSet { pathWeight = op.weights[inputIdx] }`），从而退化为教科书 RRF。

**为什么用 rank 而不是分数**：各路召回的分数尺度完全不同 —— BM25 是非负对数似然（无上界），dense cosine ∈ [−1,1]，L2 是距离（越小越好，方向还相反）。量纲不可比时直接加权求和毫无意义。**rank 是序数统计量，尺度无关** —— 这是 RRF 的全部价值所在。

**k=60 的来源与含义**（CORMACK et al. 2009 的经验值）：
```
rank = 1   → 1/61 ≈ 0.01639
rank = 10  → 1/70 ≈ 0.01429
rank = 100 → 1/160 = 0.00625
```
第 1 名与第 100 名的分差比约 **2.6 : 1**。k 越大 → 高名次与低名次的分数差距越小 → 越「民主」，偏向被多路同时召回的中游文档；k 越小 → 越「精英」，强烈偏好各路 Top-1。SDK 层暴露这个参数就是为了让用户调这个「民主度」。

### 27.2 三种策略的取舍

```
# RRF（rank 融合）
score(d) = Σ_i w_i / (k + rank_i(d))
  优点：无需归一化，尺度无关，实现最简
  缺点：完全丢弃原始距离 —— 「第 1 名但距离差 10 倍」与「第 1 名且几乎完全命中」
        在 RRF 下得分完全相同

# Weighted（分数融合）
score(d) = Σ_i w_i · normalize(score_i(d))
  优点：保留距离信息，可精细调控 dense/sparse 权重比
  缺点：必须归一化；Min-Max 归一化受离群点影响极大（一个极端距离把
        整个区间拉扁），Z-score 则对样本量敏感

# Weighted RRF（推荐）
score(d) = Σ_i w_i / (k + rank_i(d))
  折中：保留 rank 的尺度无关性，同时用 w_i 表达路径重要性
```

### 27.3 一个真实的排序反转算例

设 `k = 60`，两路召回，dense 权重 1.0、sparse 权重 0.5：

| 文档 | dense rank | sparse rank | 无权重 RRF (1,1) | Weighted RRF (1, 0.5) |
|---|---|---|---|---|
| A | 1 | 50 | 1/61 + 1/110 = **0.02552** | 1/61 + 0.5/110 = **0.02093** |
| B | 3 | 2 | 1/63 + 1/62 = **0.03201** | 1/63 + 0.5/62 = **0.02393** |
| C | 30 | 1 | 1/90 + 1/61 = **0.02750** | 1/90 + 0.5/61 = **0.01930** |

- **无权重时排序 B > C > A** —— C 靠 dense 第 30 名 + sparse 第 1 名胜出。这正是「两路都认可的高质量文档」应得的位置。
- **给 sparse 降到 0.5 权重后排序变为 B > A > C** —— C 的 sparse 第 1 名贡献从 1/61 ≈ 0.0164 腰斩到 0.0082，而 dense 第 30 名只值 1/90 ≈ 0.0111，撑不住名次，掉到 A 之后。

这个反转正是「给稀疏路降权」的**预期效果**：当 dense 是主力召回、sparse 只做补充召回时，C 这种「sparse 极强但 dense 极弱」的文档很可能是关键词精确匹配但语义不相关的噪声（关键词攻击 / 拼写变体），降权可以有效压制它。

### 27.4 融合与一致性是正交的

混合检索结果的**最终可见性**仍由 guarantee timestamp 决定（§24.5），与融合算法**正交**：融合只在「各自可见的行集合」上重排。因此 **RRF 不会让一条 `delete_ts > guarantee_ts` 的已删文档复活** —— 因为墓碑过滤发生在 `DeletedRecord::Query`（§24.6），早于融合算子执行。这也是 §24.6 那条 `delete_ts ≤ insert_ts` 检查必须放在写入路径而非查询路径的原因：只有写入期拦掉「删除早于插入」的病态记录，才能保证查询期单纯按 `delete_ts ≤ guarantee_ts` 过滤就是正确的。

## 二十八、参考资料

- [Milvus GitHub](https://github.com/milvus-io/milvus)
- [Milvus 官方文档](https://milvus.io/docs)
- [Zilliz Cloud](https://zilliz.com/cloud)
- [Knowhere GitHub](https://github.com/zilliztech/knowhere)
- [DiskANN 论文](https://papers.nips.cc/paper/2019/hash/0898c07cd8de3f8f97b64dc93d3599b8-Abstract.html)
- [ScaNN 论文](https://arxiv.org/abs/1908.10396)
- [CAGRA 论文](https://arxiv.org/abs/2308.04376)
- [Milvus Operator](https://github.com/zilliztech/milvus-operator)
- [Attu Web UI](https://github.com/zilliztech/attu)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)