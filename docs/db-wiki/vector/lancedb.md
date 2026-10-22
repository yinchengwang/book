# LanceDB Wiki（嵌入式列式向量数据库）

> 参考版本：LanceDB 0.x（Rust + Python + Node.js + Java SDK，基于 Lance 列存格式）。**嵌入式列存 + 向量检索** 的新一代向量数据库，类似 DuckDB 的"嵌入式分析 + 向量"组合。本 wiki 重点覆盖向量类型特有章节（Lance 列存格式、IVF/HNSW 嵌入式实现、版本化数据、多模态）。

## 一、项目定位

LanceDB 是由 **LanceDB Inc.（创始人 Chang She，前 Pandas / Pandas-API / Lance 创始人）** 开源的 **嵌入式列式向量数据库**，基于 **Lance 列存格式**（Apache Arrow + 自研版本化），主打 **嵌入式 + 列存 + 向量检索 + 表格查询 + 跨语言（Python/JS/Java/Rust）** 体验，是 DuckDB 在 AI 时代的"向量"对应物。

- **痛点**：
  - Chroma / DuckDB 等嵌入式数据库不支持高性能向量 + 列存组合；
  - 大规模数据集单机向量检索需要类似 Parquet 的列存 + 压缩；
  - AI 应用需要 **版本化数据**（ML 数据集版本管理）；
  - 数据湖仓（Delta Lake / Iceberg）缺少原生向量检索；
  - 嵌入式场景下需要支持 **增量更新、二级索引、多模态**；
- **典型用户**：AI/ML 工程师、数据科学家、本地 AI 应用、嵌入式 RAG、LLM 微调数据、推荐系统召回；
- **核心口号**：**"Columnar + Vector + Serverless"** + **"Multimodal data lakehouse"** + **"Lance: Modern columnar data format"**。

## 二、发展历史与社区

- **起源**：2022 年由 Chang She（前 Pandas 维护者、DuckDB 早期 contributor）发起，最初是 **Lance 列存格式**项目（独立），后衍生 LanceDB；
- **里程碑**：
  - 2022：Lance 列存格式 0.1；
  - 2023：LanceDB 0.1（Python only）；
  - 2024：v0.4 引入 **Rust Core**，Node.js / Java SDK；
  - 2024-08：v0.6 引入 **磁盘索引（DiskANN-like）**、**全文检索**；
  - 2024-12：v0.8 引入 **多向量（ColBERT）**、**SQL 增强**；
  - 2025-04：v0.10 引入 **远程模式（Remote Server）**、**多租户**；
  - 2025-12：v1.0 GA 临近；
  - 2026：持续优化 **AI 原生数据湖仓**；
- **维护状态**：极活跃，月度版本，GitHub 4k+ star；
- **社区**：Discord、GitHub Discussions、Reddit r/MachineLearning。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**；
- **Lance 列存格式**：**Apache License 2.0**；
- **商业版本**：
  - **LanceDB Cloud**（全托管）；
  - **LanceDB Enterprise**（生产支持）；
- **生态关系**：
  - 与 **Apache Arrow** 紧耦合（底层格式）；
  - 与 **DuckDB** 互操作（嵌入式 OLAP）；
  - 与 **PyTorch / Pandas / Polars** 互转；
  - 与 **LangChain / LlamaIndex / Haystack** 是默认后端之一。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **多家 LLM 公司**（匿名）；
  - **Roblox**：游戏资产检索；
  - **Shopify**：商品 embedding 检索；
  - 国内：阿里、字节、网易等试点；
- **行业**：AI/ML、数据科学、RAG、推荐系统、内容平台、LLM 微调数据；
- **典型场景**：本地 RAG、LLM 训练数据版本管理、嵌入式向量检索、AI 数据湖仓、多模态数据管理。

## 五、一句话对比定位

> LanceDB 是 **"Rust + Python + Lance 列存 + 嵌入式 + 向量检索 + 表格查询 + 版本化 + 多模态"** 的嵌入式向量数据库；与 **Chroma（Python 嵌入式 + HNSW）**、**DuckDB（嵌入式 OLAP）**、**Qdrant（Rust 云原生）**、**Milvus（分布式强）**、**Pinecone（商业全托管）**、**FAISS（算法库）**、**USearch（嵌入式 C++）**、**Lance（列存格式）** 构成向量库阵营。LanceDB 的差异化是 **"嵌入式 + 列存 + 版本化 + AI 数据湖仓 + Apache Arrow 生态"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层（多语言 SDK）                       │
│  - Python: lancedb（推荐）                                          │
│  - Rust: lancedb（crate）                                           │
│  - Java / Scala: lancedb-java                                       │
│  - Node.js / TS: @lancedb/lancedb                                   │
│  - C: lancedb-c                                                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  API 层（嵌入式 API）                                │
│  python/src/lancedb/                                                │
│  ├─ connect(uri)（本地 / 远程模式）                                  │
│  ├─ create_table / open_table                                       │
│  ├─ Table.add() / search() / delete()                              │
│  ├─ VectorIndexBuilder                                              │
│  ├─ ScalarIndexBuilder                                              │
│  └─ Dataset API（版本化）                                           │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ Rust FFI
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Rust Core（lancedb crate）                           │
│  rust/lancedb/src/                                                   │
│  ├─ Table / Dataset（数据建模）                                     │
│  ├─ Index（向量 + 标量索引）                                        │
│  ├─ Query Planner                                                   │
│  ├─ Execution Engine（向量化执行）                                 │
│  └─ Optimizer / Statistics                                          │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Lance 列存格式（lance crate）                       │
│  rust/lance/src/                                                     │
│  ├─ Dataset（版本化数据集）                                         │
│  ├─ File Format（自定义 .lance）                                    │
│  ├─ Columnar IO（Arrow）                                            │
│  ├─ Index（IVF / HNSW / Scalar）                                    │
│  ├─ Encoder / Decoder（压缩 / 编码）                                │
│  ├─ Transaction / Manifest（版本管理）                              │
│  └─ Catalog（元数据）                                                │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  索引实现（lance-index crate）                        │
│  rust/lance-index/src/                                               │
│  ├─ vector::hnsw（HNSW 图）                                        │
│  ├─ vector::ivf（IVF 倒排）                                        │
│  ├─ vector::pq（PQ 量化）                                           │
│  ├─ vector::sq（Scalar Quantization）                              │
│  ├─ vector::flat（Flat 暴力）                                       │
│  └─ scalar::*（标量索引）                                           │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Apache Arrow / Parquet                             │
│  - 列存 Arrow RecordBatch                                          │
│  - 压缩（LZ4 / ZSTD / Snappy）                                     │
│  - 文件 I/O（Object Store 抽象）                                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **嵌入式优先（Embedded First）**：默认嵌入式（Python 进程内），可选远程模式（Serverless）；
- **Lance 列存格式**：自定义二进制格式 + Arrow + 版本化；
- **Rust Core + 多语言绑定**：PyO3 / JNI / NAPI 绑定；
- **索引与数据分离**：索引作为数据集的辅助结构；
- **AI 数据湖仓**：版本化数据集 + 向量 + 标量 + 全文；
- **多模态**：支持图片 / 视频 / 音频以 blob 形式存储 + 向量索引；
- **Serverless 模式**（v0.10+）：LanceDB Cloud 提供 HTTP API。

## 七、存储引擎

### 7.1 Lance 列存格式

Lance 是 LanceDB 的核心创新：**自定义列存格式**（`.lance` 文件 + `.lance/_transactions/`）：

**结构**：
```
dataset.lance/
├── _versions/
│   ├── 1.version            # 版本 1（基线）
│   ├── 2.version            # 版本 2（增量）
│   └── ...
├── _manifests/
│   ├── 1.manifest            # 每个版本的元数据
│   └── 2.manifest
├── data/
│   ├── 0.lance              # 数据文件（columnar）
│   ├── 1.lance              # 增量数据文件
│   └── ...
├── _indices/
│   ├── vec_index_0.lance     # 向量索引
│   └── scalar_index_0.lance  # 标量索引
└── _deletions/
    ├── 1.arrow              # 软删除标记
    └── ...
```

**特点**：
- **版本化**：每次写入创建一个新版本，支持 **Time Travel**（读历史快照）；
- **追加写 + 合并（Compaction）**：增量写入，定期合并；
- **列存 Arrow**：基于 Apache Arrow RecordBatch；
- **压缩**：LZ4 / ZSTD / Snappy 可选；
- **删除标记（Soft Delete）**：避免立即物理删除；
- **MVCC**：基于版本号的乐观并发控制。

源码：`rust/lance/src/dataset/`、`rust/lance/src/io/`、`rust/lance/src/format/`。

### 7.2 Object Store 抽象

- **本地文件系统**（默认）；
- **S3 / GCS / Azure Blob**（v0.10+）；
- **HTTP**（远程模式）；
- 抽象接口：`object_store` crate。

### 7.3 元数据

- **Manifest**：每个版本一份，含 schema、字段元数据、数据文件清单、统计信息；
- **Transaction Log**：每次写入生成 transaction；
- **版本链**：通过 version 文件链接。

## 八、索引机制（**向量类型核心**）

### 8.1 向量索引算法全景

| 索引 | 文件位置 | 备注 |
|------|---------|------|
| **Flat** | `lance-index/src/vector/flat.rs` | 暴力 |
| **IVF** | `lance-index/src/vector/ivf.rs` | 倒排 |
| **HNSW** | `lance-index/src/vector/hnsw.rs` | 图 |
| **IVF + PQ** | `lance-index/src/vector/pq.rs` | 量化 |
| **IVF + SQ** | `lance-index/src/vector/sq.rs` | 标量量化 |

### 8.2 IVF（Inverted File）详解

LanceDB 的 IVF 实现位于 `rust/lance-index/src/vector/ivf.rs`：

- **结构**：K-means 聚类 + 倒排表；
- **参数**：`num_partitions`（K）、`num_bits`（量化位数）、`distance_type`（L2 / Cosine / Dot）；
- **训练**：从样本数据训练聚类中心；
- **查询**：先找最近 K 个聚类，再在候选内做精确 / 量化搜索；
- **优势**：训练成本低，召回可控；
- **适用**：百万-千万级向量。

### 8.3 HNSW 详解

LanceDB 的 HNSW 实现位于 `rust/lance-index/src/vector/hnsw.rs`：

- **自研**：基于经典 HNSW 算法；
- **存储**：构建后持久化到 `*.lance` 文件；
- **参数**：`m`（每节点连接数）、`ef_construction`、`ef`（搜索）；
- **支持距离**：L2、Cosine、Dot；
- **增量更新**：支持在线添加新向量；
- **适用**：十万-百万级向量。

### 8.4 量化方案

| 量化 | 文件 | 压缩比 | 备注 |
|------|------|--------|------|
| **Scalar Quantization (SQ)** | `lance-index/src/vector/sq.rs` | 1/4 | int8 |
| **Product Quantization (PQ)** | `lance-index/src/vector/pq.rs` | 1/8 ~ 1/32 | IVF_PQ 组合 |
| **RabitQ**（实验）| `lance-index/src/vector/` | 1/32 | 二值化 |

### 8.5 标量索引

LanceDB 同时支持标量索引（与向量索引协同）：

- **BTree**：有序范围查询；
- **Bitmap**：等值查询；
- **Inverted（全文）**：BM25 全文检索（v0.6+）；
- **Label List**：多标签查询；
- **Bloom Filter**：快速存在性检查；
- 源码：`rust/lance-index/src/scalar/`。

### 8.6 多向量（Multi-Vector）

- **场景**：ColBERT / ColPali 风格多向量嵌入；
- **API**：`vector_column` schema 中存多个向量字段；
- **距离**：MaxSim 或平均；
- 源码：`rust/lance/src/dataset/schema.rs`。

### 8.7 全文检索（v0.6+）

- **基于 Tantivy** 的倒排 + BM25；
- **API**：`table.search().text("query")`；
- **融合**：向量 + BM25 RRF；
- 源码：`rust/lance/src/index/text/`。

### 8.8 二级索引 + 向量融合

```python
import lancedb

db = lancedb.connect("./data")
table = db.open_table("docs")

# 标量过滤 + 向量检索
results = table.search([0.1, 0.2, ...]) \
    .where("category = 'book' AND price < 100") \
    .limit(10) \
    .to_pandas()
```

**过滤策略**：
- **Pre-filter**：先标量过滤得到 ID 集合，再在向量索引中检索；
- **Post-filter**：先向量检索，再标量过滤；
- **动态策略**：LanceDB 根据过滤选择性自动选择。

## 九、查询/计算/接口层

### 9.1 Python API（核心）

```python
import lancedb
import pyarrow as pa

# 1. 连接（嵌入式）
db = lancedb.connect("./data")

# 2. 创建表
data = pa.table({
    "id": [1, 2, 3],
    "vector": pa.array([[0.1, 0.2], [0.3, 0.4], [0.5, 0.6]], type=pa.list_(pa.float32(), 2)),
    "category": ["book", "movie", "music"],
})
table = db.create_table("docs", data)

# 3. 创建向量索引
table.create_index(
    metric="cosine",
    num_partitions=256,
    num_sub_vectors=96,
    vector_column_name="vector",
    index_type="IVF_PQ",
)

# 4. 查询
results = table.search([0.15, 0.25]).limit(5).to_pandas()

# 5. 版本化
table.checkout_version(1)        # Time Travel
```

### 9.2 Rust API

```rust
use lancedb::connect;
use arrow_array::RecordBatch;

let db = connect("./data").execute().await?;
let table = db.create_table("docs", batches).execute().await?;
table.create_index(&["vector"], Index::IvfPq(IvfPqParams { ... })).await?;
let results = table.search(&[0.1, 0.2]).limit(5).execute().await?;
```

### 9.3 多语言 SDK

| 语言 | 包名 |
|------|------|
| Python | `lancedb`（PyO3 绑定）|
| Rust | `lancedb`（crate，原生）|
| Java | `com.lancedb:lancedb`（JNI 绑定）|
| Node.js / TS | `@lancedb/lancedb`（NAPI 绑定）|
| C / C++ | `lancedb-c`（C FFI）|
| Go | `lancedb-go`（社区）|

### 9.4 远程模式（Serverless，v0.10+）

- **HTTP API** + **LanceDB Cloud**；
- **客户端 API 与本地一致**，自动切换本地 / 远程；
- **多租户**：基于 LanceDB Cloud 账号；
- 源码：`rust/lancedb/src/server/`。

### 9.5 DataFrame API（Polars 风格）

- v0.10+ 引入 Polars 风格 DataFrame API；
- SQL 兼容（`SELECT ... WHERE ... ORDER BY ... LIMIT ...`）；
- 矢量化执行（基于 Arrow）；
- 源码：`rust/lance/src/dataset/sql.rs`。

### 9.6 混合检索 + Rerank

- **向量 + BM25 RRF 融合**；
- **过滤融合**：`search().where().limit()`；
- **Rerank**：用户自行接入 Cohere / Cross-Encoder。

## 十、事务与一致性

- **MVCC（Multi-Version Concurrency Control）**：基于版本号；
- **乐观并发控制**：写入冲突检测；
- **Snapshot Isolation**：默认隔离级别；
- **Time Travel**：可读历史快照（基于版本号）；
- **崩溃恢复**：基于 transaction log + manifest 恢复。

## 十一、复制与高可用

- **本地嵌入式模式**：无 HA；
- **LanceDB Cloud**：内置多副本 + 跨区域；
- **自管**：通过 Object Store（S3 / GCS）多副本实现 HA。

## 十二、扩展性

- **垂直扩展**：增加内存 / CPU；
- **水平扩展**：通过 LanceDB Cloud 或 Object Store 路径；
- **冷热分层**：
  - **量化**：PQ / SQ 降低内存；
  - **Mmap**：索引 mmap 到磁盘；
  - **Compaction**：定期合并小文件为大文件。

## 十三、性能与基准

- **ann-benchmarks**：LanceDB 在 SIFT-1M 上排名中上；
- **嵌入式查询延迟**：百万级向量约 **1-10ms / query**；
- **IVF_PQ 召回率**：默认参数 90%+ R@10；
- **版本化开销**：写入延迟略高于无版本化数据库（多写一次 manifest）；
- **DataFrame 查询**：向量化执行，10x 加速 vs row-by-row。

## 十四、运维与可观测性

- **本地模式**：日志 + 简单 metrics；
- **LanceDB Cloud**：完整 Prometheus + Grafana；
- **Backup**：直接备份 `.lance` 目录；
- **Object Store 集成**：S3 / GCS / Azure Blob；
- **诊断工具**：`lance doctor` 命令。

## 十五、生态与集成

- **Apache Arrow**：底层格式；
- **DuckDB**：可读取 Lance 文件（`duckdb.read_lance()`）；
- **Polars / Pandas / PyTorch**：DataFrame 互转；
- **AI 框架**：LangChain、LlamaIndex、Haystack 默认后端；
- **数据湖仓**：与 Delta Lake / Iceberg 兼容路径探索中；
- **部署**：pip install lancedb / Cargo add lancedb。

## 十六、核心论文与算法原理

- **Lance 列存**：Lance 论文（基于 Arrow）；
- **HNSW**：Malkov & Yashunin, 2016；
- **IVF + PQ**：Jégou et al., 2011；
- **BM25**：经典排序；
- **Delta Lake / Iceberg**：时间旅行 + 版本化设计参考；
- **Apache Arrow**：列存内存格式；
- **MVCC**：经典数据库理论。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | Python API | `python/src/lancedb/` | 用户接口 |
| P0 | Rust Core | `rust/lancedb/src/` | 核心逻辑 |
| P0 | Lance Dataset | `rust/lance/src/dataset/` | 数据集管理 |
| P1 | IVF 索引 | `rust/lance-index/src/vector/ivf.rs` | 倒排 |
| P1 | HNSW 索引 | `rust/lance-index/src/vector/hnsw.rs` | 图索引 |
| P1 | PQ 量化 | `rust/lance-index/src/vector/pq.rs` | 量化 |
| P1 | Manifest | `rust/lance/src/format/manifest.rs` | 版本管理 |
| P2 | Columnar IO | `rust/lance/src/io/` | 文件读写 |
| P2 | Transaction | `rust/lance/src/dataset/transaction.rs` | 事务 |
| P2 | DataFrame | `rust/lance/src/dataset/sql.rs` | SQL 执行 |

## 十八、源码规模与开发语言

- **Rust**：约 70% LOC（Core + Lance + 索引）；
- **Python**：约 20% LOC（API + 测试 + 集成）；
- **TypeScript / JS**：约 5% LOC（Node.js SDK）；
- **Java**：约 3% LOC（Java SDK）；
- **构建系统**：Cargo + pyproject.toml + Maven；
- **CI**：GitHub Actions；
- **LOC 总量**：约 30 万行（含测试）。

## 十九、学习资源

- **官方文档**：https://lancedb.github.io/lancedb/
- **GitHub**：https://github.com/lancedb/lancedb
- **Lance 文档**：https://lancedb.github.io/lance/
- **Discord**：社区入口；
- **博客**：https://blog.lancedb.com/
- **YouTube**：LanceDB 频道；
- **CSDN / 知乎**：中文实践。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 / ARM64 | 原生 |
| macOS x86_64 / ARM64 | 原生 |
| Windows x86_64 | 原生 |
| Docker | 官方镜像 |
| LanceDB Cloud | 全托管 |

## 二十一、变体与衍生项目

- **LanceDB Cloud**：全托管；
- **Lance**：列存格式（独立项目）；
- **LanceDB Enterprise**：商业支持；
- **多语言 SDK**：Python / Rust / Java / Node.js / C。

## 二十二、风险与陷阱

1. **嵌入式模式无 HA**：单进程崩溃数据可恢复（基于 Object Store），但运行期单点；
2. **版本化写入开销**：每次写入生成新版本，写入延迟略高；
3. **小数据集 HNSW 过重**：小于 10k 向量建议用 Flat；
4. **索引构建时间长**：百万级向量 HNSW 构建需分钟级；
5. **量化召回损失**：PQ 召回率损失 2-5%；
6. **过滤选择性影响**：高选择性过滤会降低向量检索效率；
7. **多向量索引内存**：多个向量字段同时建索引，内存占用倍增；
8. **Lance 格式兼容性**：跨大版本格式可能不兼容；
9. **DataFrame API 实验性**：v0.10 引入，生产建议先验证；
10. **远程模式（Serverless）**：v0.10+ 引入，生态相对薄弱。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 已实现 HNSW，可参考 LanceDB 的 IVF_PQ 组合；
- **`engineering/rag/`** 中 RAG 系统可对接 LanceDB 嵌入式后端；
- **学习价值**：Lance 列存格式设计 + 版本化思想 + Arrow 集成；
- **MiniVecDB**：参考 Lance 的 Manifest + Transaction 设计版本化向量数据库；
- **`learning/notes/`** 可沉淀 Lance 列存格式设计经验。

## 二十四、Lance 文件格式版本、读写路径与 IO 调度

### 24.1 crate 边界：格式实现不在 lancedb 主仓库

LanceDB 仓库的 `rust/` 目录下**只有 `lancedb` 一个 crate**（另有 `license_header.txt`）。Lance 列式格式本身的编解码位于**独立的 `lance` 系列 crate**，通过 Cargo 依赖引入（`rust/lancedb/Cargo.toml:37` 起）：

| crate | 职责 | 从 lancedb 侧可见的入口 | 引用出处 |
|--------|------|------------------------|---------|
| `lance-encoding` | 格式版本号 + 解码器插件 | `lance_encoding::version::LanceFileVersion`、`lance_encoding::decoder::{DecoderPlugins, FilterExpression}` | `rust/lancedb/src/connection.rs:37`、`rust/lancedb/src/dataloader/permutation/shuffle.rs:11` |
| `lance-file` | `.lance` 文件读写 | `lance_file::reader::{FileReader, FileReaderOptions}`、`lance_file::writer::{FileWriter, FileWriterOptions}` | `rust/lancedb/src/dataloader/permutation/shuffle.rs:13`、`rust/lancedb/src/dataloader/permutation/shuffle.rs:14` |
| `lance-io` | 对象存储 + 扫描调度 | `lance_io::scheduler::{ScanScheduler, SchedulerConfig}`、`lance_io::utils::CachedFileSize` | `rust/lancedb/src/dataloader/permutation/shuffle.rs:18`、`rust/lancedb/src/dataloader/permutation/shuffle.rs:19` |
| `lance-index` | 标量 / 倒排索引 | `lance_index::scalar::InvertedIndexParams` | `rust/lancedb/src/index/scalar.rs:64` |
| `lance` | Dataset / Manifest / 一致性 | `lance::dataset::{DeleteBuilder, Version}`、`lance::io::ObjectStore` | `rust/lancedb/src/table.rs:19`、`rust/lancedb/src/dataloader/permutation/shuffle.rs:9` |

> ⚠️ **可验证性边界**：本节只写能从 lancedb 仓库交叉验证的结论。`Fragment` / `FragmentMetadata` / `DeletionPredicate` / V2 编码的 null bitmap 与 offset 布局 / 页索引内部 offset+size 缓冲结构等类型在本仓库**没有定义点**，故一律不展开——宁可留白，不可硬圆。

### 24.2 LanceFileVersion：别名 vs 具体小版本

`LanceFileVersion`（从 `lance_encoding::version` 重导出，`rust/lancedb/src/connection.rs:37`）有两类取值：

- **别名**：`Legacy`、`Stable`、`Next` —— 不指向具体小版本，在**存储时刻**才解析；
- **具体小版本**：`V2_0`、`V2_2`、`V2_3` …… 随编码器能力逐个新增。

**关键坑：别名不能直接比较。** 建表测试用 `Legacy` / `Stable` 两个别名建表（`rust/lancedb/src/connection/create_table.rs:400`），断言时先从 manifest 的 `data_storage_format.lance_file_version()` 取回（`rust/lancedb/src/connection/create_table.rs:430`），再对两侧都 `resolve()` 后比较（`rust/lancedb/src/connection/create_table.rs:433`）。配置侧则从字符串解析（`rust/lancedb/src/database/namespace.rs:217`）。

### 24.3 写入参数：`data_storage_version` 的「只升不降」规则

控制文件格式的写参是 `WriteParams::data_storage_version: Option<LanceFileVersion>`，`None` 表示走默认 `Stable`。Blob 列对格式版本有硬要求，由 `ensure_blob_storage_version` 兜底（`rust/lancedb/src/blob.rs:110`）：

```text
fn ensure_blob_storage_version(schema, params: &mut WriteParams):
    if not has_blob_columns(schema):
        return                                   # 无 blob 列则完全不干预
    resolved = params.data_storage_version
                 .unwrap_or(LanceFileVersion::Stable)   # rust/lancedb/src/blob.rs:117
                 .resolve()                       # 别名在此刻定值
    if resolved < LanceFileVersion::V2_2:        # rust/lancedb/src/blob.rs:119   只升不降
        params.data_storage_version = Some(LanceFileVersion::V2_2)   # rust/lancedb/src/blob.rs:120
```

三条语义：

1. **只升不降**——用户显式指定更高的 `V2_3` 不会被降到 `V2_2`；显式指定更低的 `V2_0` 会被抬到 `V2_2`。即「显式选旧格式」在有 blob 列时不成立。
2. **幂等、O(列数)**——纯元数据判断，不触碰数据面。
3. **读取侧配套校验**——读到 legacy blob v1 列（扩展名 `lance-encoding:blob`）调用 blob API 直接报错并给迁移提示（`rust/lancedb/src/blob.rs:126`），判定入口是 `field.is_blob_v2()`（`rust/lancedb/src/blob.rs:132`）。

由此可推断一条工程结论：**格式版本是 per-column 强绑定的**。写侧的「抬版本」只保证**新写入**满足 v2 约束，历史 v1 列仍需显式迁移；读路径必须**按列判定**，不能按文件一刀切。

### 24.4 写入路径：`FileWriter` 与 `max_rows_per_file` 的双向代价

`lance_file::writer::FileWriter::try_new(object_writer, schema, FileWriterOptions)` 是写 `.lance` 文件的入口（`rust/lancedb/src/dataloader/permutation/shuffle.rs:156`）。

文件切分阈值 `max_rows_per_file` 是一个**内存的双向旋钮**——源码注释把两端代价都写明了：

| 取值 | 内存代价 | 句柄 / I/O 代价 | 引用 |
|------|---------|----------------|------|
| 过大 | 每文件缓冲 `max_rows_per_file` 行数据（`rust/lancedb/src/dataloader/permutation/shuffle.rs:35`） | 文件数少、顺序 I/O 好 | `rust/lancedb/src/dataloader/permutation/shuffle.rs:35` |
| 过小 | 每行开销摊薄到小缓冲 | 需同时持有 `total_num_rows / max_rows_per_file` 个 writer，连同列写缓冲一起膨胀 | `rust/lancedb/src/dataloader/permutation/shuffle.rs:38` |

默认 `max_rows_per_file = 1024 * 1024`（`rust/lancedb/src/dataloader/permutation/shuffle.rs:56`），文件数为 `ceil(num_rows / max_rows_per_file)`（`rust/lancedb/src/dataloader/permutation/shuffle.rs:129`）。数据打散场景直接开到 10Mi 行（`rust/lancedb/src/dataloader/permutation/builder.rs:276`）。

`clump_size` 做的是另一组交换：**在连续行块内打散**，随机性下降但读放大改善（`rust/lancedb/src/dataloader/permutation/shuffle.rs:48`）。

### 24.5 读取路径：ScanScheduler → FileScheduler → FileReader → 分页读

读路径是四层结构，每层职责单一（以数据打散的全量扫描为例，`rust/lancedb/src/dataloader/permutation/shuffle.rs:205` 起）：

```text
ObjectStore
   │  SchedulerConfig::max_bandwidth(&object_store)          # rust/lancedb/src/dataloader/permutation/shuffle.rs:215  按目标带宽限流
   ▼
ScanScheduler::new(Arc<ObjectStore>, scheduler_config)       # rust/lancedb/src/dataloader/permutation/shuffle.rs:216  全局 IO 调度器
   │  .open_file(&path, &CachedFileSize::unknown())          # rust/lancedb/src/dataloader/permutation/shuffle.rs:232  单文件调度句柄
   ▼
FileScheduler                                                   # 按带宽切分该文件的读请求
   ▼
FileReader::try_open(scheduler,        # rust/lancedb/src/dataloader/permutation/shuffle.rs:235
                     metadata_decoder,  # 形参 2
                     DecoderPlugins,   # rust/lancedb/src/dataloader/permutation/shuffle.rs:238
                     LanceCache,       # 形参 4
                     FileReaderOptions)# rust/lancedb/src/dataloader/permutation/shuffle.rs:240
   │  .read_stream(ReadBatchParams::RangeFull,   # rust/lancedb/src/dataloader/permutation/shuffle.rs:246  读取范围
   │              num_rows, batch_size,          # 总行数、单批行数
   │              FilterExpression::no_filter())  # rust/lancedb/src/dataloader/permutation/shuffle.rs:249  下推谓词
   ▼
RecordBatch 迭代器
```

四个要点：

1. **限流发生在调度器层而非 reader 层**。`SchedulerConfig::max_bandwidth`（`rust/lancedb/src/dataloader/permutation/shuffle.rs:215`）让并发扫描共享一份带宽预算，避免 N 个查询各自把带宽打满。
2. **文件大小允许未知**。`CachedFileSize::unknown()`（类型见 `rust/lancedb/src/dataloader/permutation/shuffle.rs:19`，实参见 `rust/lancedb/src/dataloader/permutation/shuffle.rs:233`）说明调度器可以在探测出真实大小之前先排请求——对 S3 这类无 `stat` 语义的存储是必需的。
3. **解码器是可插拔的，谓词下推到编码层**。`Arc::<DecoderPlugins>::default()`（`rust/lancedb/src/dataloader/permutation/shuffle.rs:238`）是解码器注册表，`lance_encoding::decoder::FilterExpression`（`rust/lancedb/src/dataloader/permutation/shuffle.rs:11`）是下推谓词。关键在于谓词在**编码页内部**求值，而不是「先整列解码、再过滤」——这正是 Lance 免解码过滤的实现入口。
4. **分页而非整文件加载**。`read_stream` 的四个参数分别是：读取范围（此处取 `RangeFull` 全范围变体，类型是范围参数枚举）、总行数、单批行数、过滤谓词（`rust/lancedb/src/dataloader/permutation/shuffle.rs:246`、`:249`）。范围与批量大小都是参数，因此**同一套 API 覆盖全范围流式读与区间读**，读取始终以批为单位推进，无需把整个文件物化进内存。

## 二十五、数据集版本链、软删除与存储生命周期

### 25.1 一切变更都是加性的（append-only）

LanceDB 版本模型最核心的一句写在源码注释里：**「Every change in LanceDb is additive」**——数据被移除时不是原地删，而是生成一个**不含该数据的新版本**，旧版本原地保留（`rust/lancedb/src/table/optimize.rs:51`）。注释同时点明这样做的两个理由：保证一致性与并发，以及支撑 time travel / undo。

三个直接后果：

1. **读路径天然无锁**：任何读只认一个版本号，版本内的文件清单不可变；
2. **Time Travel 免费获得**：能 checkout 旧版本就等价于能 undo；
3. **磁盘只增不减**：必须靠显式 prune 回收（见 25.5）。

### 25.2 版本 API 家族

| API | 语义 | 引用 |
|-----|------|------|
| `version()` | 取当前版本号 | `rust/lancedb/src/table.rs:1633` |
| `checkout(version)` | 切到指定版本，之后所有读走该版本 | `rust/lancedb/src/table.rs:676` |
| `checkout_tag(tag)` | 切到标签指向的版本 | `rust/lancedb/src/table.rs:679` |
| `checkout_latest()` | 回到最新版本，兼作 checkout 的撤销 | `rust/lancedb/src/table.rs:1683` |
| `restore()` | 用当前 checkout 的版本**覆盖最新版本**（真 undo） | `rust/lancedb/src/table.rs:1691` |
| `list_versions()` | 列出全部未 prune 的版本 | `rust/lancedb/src/table.rs:685` |
| `checkout_branch(name)` | 分支隔离 | `rust/lancedb/src/table.rs:693` |
| tag 的 `create` / `update` | 维护标签到版本的映射 | `rust/lancedb/src/table.rs:296`、`rust/lancedb/src/table.rs:302` |

`checkout` 的三条约束（源码注释 `rust/lancedb/src/table.rs:1646` 起）：

- **只影响本表句柄**，其他 table 实例不受影响（`rust/lancedb/src/table.rs:1649`：变成一个 "view" / "detached head"）；
- **checkout 后禁用 read_consistency_interval**（`rust/lancedb/src/table.rs:1646`：快照已固定，无需再探测新版本）；
- **处于 checkout 状态时任何写操作会失败**（`rust/lancedb/src/table.rs:1653`）。源码里体现为写路径统一先过 `ensure_mutable()` 闸门（`rust/lancedb/src/table/delete.rs:31`）。

分支还支持「(branch, version) 二维坐标」定位（`rust/lancedb/src/table.rs:696`）：默认实现是 `checkout_branch` 后再 `checkout`，实现方可以覆写为**一次 manifest 读**同时解析两个坐标（`rust/lancedb/src/table.rs:699`）。

### 25.3 行标识：offset 与 row id 都不稳定

这是最容易被误用的一点，源码用两段独立注释讲清楚：

| 标识 | 稳定性 | 关键说明 | 引用 |
|------|--------|---------|------|
| dataset offset | **不稳定**，0 基、相对当前版本 | 前面的行被删后同一逻辑行的 offset 会变；优点是有效取值集合可预先枚举为 `[0, len(table))`，适合采样 | `rust/lancedb/src/table.rs:1441`、`rust/lancedb/src/table.rs:1444` |
| row id | **比 offset 稳定**，但仍随 compaction / update 变化 | 官方有让 row id 稳定的在途 issue（lancedb#1120） | `rust/lancedb/src/table.rs:1465`、`rust/lancedb/src/table.rs:1468` |

两者都**不保证返回顺序**：需要保序时必须把 offset / row id 显式作为输出列带回来再自行对齐（`rust/lancedb/src/table.rs:1448`、`rust/lancedb/src/table.rs:1475`）。

RRF 混合融合正是靠 row id 作为两条召回流的连接键（`rust/lancedb/src/rerankers/rrf.rs:14`）。

### 25.4 删除：软删除的两条实现路径

删除统一从 `execute_delete` 入口分发（`rust/lancedb/src/table/delete.rs:27`），先过 `ensure_mutable()` 闸门（`rust/lancedb/src/table/delete.rs:31`），再按谓词形态二分：

```text
execute_delete(table, predicate):
    table.dataset.ensure_mutable()?                      # rust/lancedb/src/table/delete.rs:31  checkout 态拒写
    match predicate:
        Predicate::String(s) =>                           # SQL 字符串谓词
            dataset = table.dataset.get().await?
            r = dataset.delete(s).execute().await?        # rust/lancedb/src/table/delete.rs:41  走 lance 内建路径
            table.dataset.update(dataset)
            return { num_deleted_rows: r.num_deleted_rows,
                     version: dataset.version() }
        Predicate::Expr(expr) =>                           # Arrow 表达式谓词
            dataset = table.dataset.get().await?           # rust/lancedb/src/table/delete.rs:45
            r = DeleteBuilder::from_expr(dataset, expr).execute().await?   # rust/lancedb/src/table/delete.rs:46
            table.dataset.update(r.new_dataset)
            return { num_deleted_rows: r.num_deleted_rows,
                     version: r.new_dataset.version() }
```

**为什么要两条路径**：字符串谓词要过 SQL parser；`Expr` 谓词免解析直接下推到扫描层。返回结构统一为 `DeleteResult { num_deleted_rows, version }`（`rust/lancedb/src/table/delete.rs:21`），其中 `version` 是**提交版本号**；取值为 `0` 时表示兼容旧服务端未返回版本号的情况（`rust/lancedb/src/table/delete.rs:17`）。

软删除的代价正是 25.1 的第 3 条：删除本身不释放任何磁盘空间。

### 25.5 `optimize`：三个动作对应三种资源泄漏

`optimize` 类比 PostgreSQL 的 `VACUUM`，三个动作各自堵一种泄漏（`rust/lancedb/src/table.rs:1501`）：

| 动作 | 堵的泄漏 | 机制 | 引用 |
|------|---------|------|------|
| **Compaction** | 只增不减导致的小文件碎片 | 合并小文件为大文件 | `rust/lancedb/src/table/optimize.rs:36` |
| **Prune** | 历史版本占用的磁盘 | 删除超过保留期的旧版本 | `rust/lancedb/src/table/optimize.rs:49` |
| **OptimizeIndices** | 未入索引的新数据 | 把 unindexed rows **增量并入**既有索引，不重训 | `rust/lancedb/src/table/optimize.rs:76` |

**Compaction** 的动机值得强调：LanceDB **按只读文件系统设计**以获得性能与并发安全，每次写都落新文件，小文件同时伤读和写（`rust/lancedb/src/table/optimize.rs:36`）。因此这类变更操作跑得越频繁，compact 越要跑得勤。

**Prune** 的安全阀：默认**不删 7 天内的版本**，因为它们可能属于在途事务；可显式覆盖，但覆盖后若仍有被 tag 的旧版本会直接报错（`rust/lancedb/src/table/optimize.rs:65`、`rust/lancedb/src/table/optimize.rs:71`）。

**OptimizeIndices** 揭示了一个重要查询行为：新数据**不进索引也能被搜到**，因为检索会**并行扫描已索引数据与未索引数据**（`rust/lancedb/src/table/optimize.rs:78`）。这既是延迟兜底，也是 26.7 中 `fast_search` 弱一致快路径的合法性来源。

**触发经验值**（源码 `rust/lancedb/src/table.rs:1508`）：累积新增/修改 **10 万行**以上，或执行 **20 次**以上数据变更操作，就该跑一次 optimize。

### 25.6 读一致性：`interval` 是延迟与一致性的显式权衡

`read_consistency_interval: Option<Duration>`（`rust/lancedb/src/connection.rs:657`）三档语义（注释 `rust/lancedb/src/connection.rs:647`）：

| 取值 | 语义 | 代价 |
|------|------|------|
| `None`（默认） | **完全不做一致性检查** | 跨进程的最新写入可能读不到；性能最好 |
| `Some(0)` | 强一致：每次读都检查其他进程的更新 | 每读一次一次探测 |
| `Some(d > 0)` | 最终一致：距上次检查超过 d 才探测 | 存在长度为 d 的延迟窗口 |

**只作用于读**——写操作始终强一致（`rust/lancedb/src/connection.rs:655`）。表句柄侧用 `DatasetConsistencyWrapper` 包装数据集来落实这个间隔（`rust/lancedb/src/table.rs:2212`）。

### 25.7 远端模式：分级重试 + 三类失败计数

远端（Serverless）把一致性问题的另一半交给 HTTP 层。`RetryConfig`（`rust/lancedb/src/remote/client.rs:222`）把失败**按成因拆成三个独立计数器**，而不是一个总重试数：

| 计数器 | 默认 | 覆盖的失败类型 |
|--------|------|---------------|
| `retries` | 3 | 一般请求失败 |
| `connect_retries` | 3 | 连接建立失败 |
| `read_retries` | 3 | 响应体读取 / 解码失败 |

退避是**指数 + 抖动**（`rust/lancedb/src/remote/client.rs:249`）：

```text
sleep_i = backoff_factor * 2^i + U(0, backoff_jitter)
默认 backoff_factor = 0.25，backoff_jitter = 0.25
→ 0.25s, 0.5s, 1.0s, ...
```

重试的 HTTP 状态码默认为 409 / 429 / 500 / 502 / 503 / 504（`rust/lancedb/src/remote/client.rs:278`）。

**一条重要的正确性约束**：**写操作遇到 5xx 绝不重试**，因为可能造成重复写入（`rust/lancedb/src/remote/client.rs:274`）。分类逻辑在 `increment_from_error`——按 `reqwest::Error` 的 `is_connect()` / `is_body()` / `is_decode()` 判定归入哪类计数（`rust/lancedb/src/remote/retry.rs:70`、`rust/lancedb/src/remote/retry.rs:78`）。

**异步建索引的等待器**同样是有界轮询：默认 1 秒一跳、总上限 2 小时（`rust/lancedb/src/index/waiter.rs:11`、`rust/lancedb/src/index/waiter.rs:12`），收敛条件是「所有目标索引的 `num_unindexed_rows == 0`」（`rust/lancedb/src/index/waiter.rs:47`）。源码注释坦承该判定在持续写入下**可能永远不稳定**，计划未来换成 status / job 模型（`rust/lancedb/src/index/waiter.rs:48`）。

### 25.8 IO 层：主备双写的镜像对象存储

`MirroringObjectStore`（`rust/lancedb/src/io/object_store.rs:48`）把写请求**先落次存储、再落主存储**，读只走主存储（`rust/lancedb/src/io/object_store.rs:92`）。设计目标是「主存储持久但慢、次存储快但不持久」，用延迟换吞吐。

关键细节：**manifest 文件不参与镜像**，只写主存储（`rust/lancedb/src/io/object_store.rs:55`）。判定函数极其简单——文件名等于 `_latest.manifest` 即为「primary only」（`rust/lancedb/src/io/object_store.rs:43`），写路径在 `put_opts` 入口分叉（`rust/lancedb/src/io/object_store.rs:64`）。

> 这条规则是版本化提交原子性的关键：**数据文件可以慢一拍同步到主存储，但 manifest（提交点）必须先安全落到主存储**，否则会出现「数据丢了而 manifest 宣称它存在」的悬挂引用。包装入口是 `WrappingObjectStore` 包装器（`rust/lancedb/src/io/object_store.rs:178`）。

## 二十六、索引体系实现：IVF-PQ 参数、标量/全文索引与混合检索

### 26.1 权威索引类型枚举

`IndexType`（`rust/lancedb/src/index.rs:292`）是索引类型的**唯一权威定义**，13 个变体分三组：

| 组 | 变体 | 引用 |
|----|------|------|
| 向量（7） | `IvfFlat` | `rust/lancedb/src/index.rs:295` |
| | `IvfSq` | `rust/lancedb/src/index.rs:297` |
| | `IvfPq` | `rust/lancedb/src/index.rs:299` |
| | `IvfRq` | `rust/lancedb/src/index.rs:301` |
| | `IvfHnswPq` | `rust/lancedb/src/index.rs:303` |
| | `IvfHnswSq` | `rust/lancedb/src/index.rs:305` |
| | `IvfHnswFlat` | `rust/lancedb/src/index.rs:307` |
| 标量（4） | `BTree` | `rust/lancedb/src/index.rs:310` |
| | `Bitmap` | `rust/lancedb/src/index.rs:312` |
| | `LabelList` | `rust/lancedb/src/index.rs:314` |
| | `Fm`（FM-Index） | `rust/lancedb/src/index.rs:316` |
| 全文（1） | `FTS` | `rust/lancedb/src/index.rs:319` |
| 兜底 | `Unknown` | `rust/lancedb/src/index.rs:321` |

`Unknown` 是个显式的**前向兼容兜底**：本版本 LanceDB 不认识的索引类型会落到这里而不是解析失败，配合 `#[serde(alias = ...)]`（如 `rust/lancedb/src/index.rs:294`）保证旧客户端读新 manifest 不崩。

> **勘误（与 §8 的差异，以本枚举为准）**：枚举中**没有 Bloom Filter 变体**；另有 §8.5 未列出的 `Fm`（FM-Index）。另 §8.7 称全文检索「基于 Tantivy」——本仓库 `Cargo.lock` / `Cargo.toml` / `rust/` 全部 `.rs` 中 `tantivy` 命中数为 **0**，FTS 实际由 `lance_index::scalar::InvertedIndexParams` 实现（重导出见 `rust/lancedb/src/index/scalar.rs:64`）。

### 26.2 IVF 参数的三个公式与双向代价

四个 IVF 参数共享同一套语义（宏 `impl_ivf_params_setter!`，`rust/lancedb/src/index/vector.rs:61`）：

| 参数 | 默认 | 公式 / 权衡 |
|------|------|------------|
| `num_partitions`（K） | **√N**（N = 行数） | 默认取行数的平方根（`rust/lancedb/src/index/vector.rs:66`）。**太大**→选分区阶段慢；**太小**→区内搜索阶段慢（`rust/lancedb/src/index/vector.rs:68`） |
| `sample_rate` | 256 | 训练向量数 = `sample_rate * num_partitions`（`rust/lancedb/src/index/vector.rs:83`、`:88`）。这是**双参数耦合**：调 K 会等比放大训练集 |
| `max_iterations` | 50 | k-means 最大迭代轮数；源码明确指出多数情况下 k-means 会提前收敛，此参数**通常不起作用**，只在明显不收敛时才需调大（`rust/lancedb/src/index/vector.rs:104`） |
| `target_partition_size` | 无 | 直接指定分区目标大小，是 `num_partitions` 的替代口径，在「查询性能 vs 精度」之间取舍（`rust/lancedb/src/index/vector.rs:114`） |

**一条必须遵守的约束**：训练索引用的度量类型**必须与搜索时一致**，否则结果不准（`rust/lancedb/src/index/vector.rs:52`、`rust/lancedb/src/index/vector.rs:53`）。

复杂度：训练 `O(sample_rate · K · iter · N_train · d)`，查询 `O(K · d + |cand| · d)`。

### 26.3 PQ 子空间数：SIMD 友好优先

`suggested_num_sub_vectors`（`rust/lancedb/src/index/vector.rs:306`）不是随便取的：

```text
suggested_num_sub_vectors(dim):
    if dim % 16 == 0:  return dim / 16      # rust/lancedb/src/index/vector.rs:309
    if dim % 8  == 0:  return dim / 8       # rust/lancedb/src/index/vector.rs:312
    warn!("dimension not divisible by 8 or 16, may degrade PQ")  # rust/lancedb/src/index/vector.rs:314
    return 1                                # rust/lancedb/src/index/vector.rs:317
```

**理由是硬件的**：每个子向量 8 或 16 字节可让距离计算吃到高效 SIMD 指令（`rust/lancedb/src/index/vector.rs:131`）。维度不能被 8 整除时退化为 1 个子向量——源码注释直言「这不是好选择，很可能性能很差」（`rust/lancedb/src/index/vector.rs:134`）。

**IVF-PQ 的完整参数集**（`IvfPqIndexBuilder`，`rust/lancedb/src/index/vector.rs:267`）= IVF 组 + PQ 组：

| 组 | 字段 | 默认 |
|----|------|------|
| IVF | `distance_type` / `num_partitions` / `sample_rate` / `max_iterations` / `target_partition_size` | `L2` / `None` / 256 / 50 / `None`（`rust/lancedb/src/index/vector.rs:289`、`:292`、`:293`、`:294`） |
| PQ | `num_sub_vectors` / `num_bits` | `None`（交由上面的公式推导）/ `None`（`rust/lancedb/src/index/vector.rs:290`、`:291`） |

IVF-HNSW 家族额外叠加 `m`（默认 20，`rust/lancedb/src/index/vector.rs:151`、`rust/lancedb/src/index/vector.rs:411`）与 `ef_construction`（默认 300，`rust/lancedb/src/index/vector.rs:161`、`rust/lancedb/src/index/vector.rs:412`）。

压缩比速查：`IVF_HNSW_SQ` 把每维压成 8-bit 整数，float32 向量上即 **4× 压缩**（`rust/lancedb/src/index/vector.rs:433`）。

### 26.4 标量索引选型

| 类型 | 适用条件 | 关键数字 / 机制 | 引用 |
|------|---------|-----------------|------|
| `BTree` | 高基数、查询高选择性 | block size 固定 4096；10 亿行表头约需 `sizeof(Scalar) × 256KiB` 内存，定位到正确 row id 通常需读 `sizeof(Scalar) × 4096` 字节 | `rust/lancedb/src/index/scalar.rs:17`、`rust/lancedb/src/index/scalar.rs:21`、`rust/lancedb/src/index/scalar.rs:22` |
| `Bitmap` | 低基数（**< 1000 个唯一值**） | 每个可能值存一张 row id 位图 | `rust/lancedb/src/index/scalar.rs:39` |
| `LabelList` | `List<T>` 列 + `array_contains_all` / `array_contains_any` | 底层就是 Bitmap 索引 | `rust/lancedb/src/index/scalar.rs:47` |
| `Fm`（FM-Index） | 字符串 / 二进制列的**子串**搜索 | Ferragina–Manzini 自索引，匹配原始字节的**任意子串**，与倒排 FTS 匹配分词词正好相反 | `rust/lancedb/src/index/scalar.rs:57`、`rust/lancedb/src/index/scalar.rs:58` |
| `FTS` | 分词后的词级检索 | 即 `lance_index::scalar::InvertedIndexParams` 的重导出 | `rust/lancedb/src/index/scalar.rs:64` |

`Fm` 与 `FTS` 的区别是选型关键：**子串包含用 `Fm`，词级检索用 `FTS`**——后者需要分词，CJK 等无空格语言的分词会显著改变召回。

### 26.5 FTS 的分词参数与默认值

Python 侧的 `tokenize` 函数签名（`python/src/table.rs:514`）直接给出了全部分词参数的默认值：

| 参数 | 默认 | 作用 |
|------|------|------|
| `base_tokenizer` | `"simple"` | 基础分词器 |
| `language` | `"English"` | 停用词 / 词干提取的语言 |
| `max_token_length` | `40` | 超长 token 截断 |
| `lower_case` | `true` | 小写化 |
| `stem` | `true` | 词干提取 |
| `remove_stop_words` | `true` | 去停用词 |
| `ascii_folding` | `true` | 变音符号折叠 |
| `ngram_min_length` | `3` | n-gram 下界 |
| `ngram_max_length` | `3` | n-gram 上界 |
| `prefix_only` | `false` | 仅前缀匹配 |

（`python/src/index.rs:43` 侧的建索引路径与之一一对应，见 `python/src/index.rs:60`、`python/src/table.rs:524`；位置信息由 `with_position` 单独控制，见 `python/src/index.rs:54`。）

**两条工程结论**：

1. **语言支持是编译期 feature，不是运行时配置**。`lance-index` 依赖显式开启 `tokenizer-jieba` 与 `tokenizer-lindera` 两个 feature（`rust/lancedb/Cargo.toml:43`）——处理中文必须依赖这些 feature 已编入。
2. **不支持的语言在配置期就报错**，不是静默降级：`base_tokenizer().language(&lang)` 返回 `Err` 时直接抛 `ValueError`（`python/src/table.rs:547`、`python/src/index.rs:50`）。

### 26.6 混合检索：并发双路 + 归一化 + RRF

`execute_hybrid`（`rust/lancedb/src/query.rs:1332`）的执行流程：

```text
① 克隆查询并强制 with_row_id                  # rust/lancedb/src/query.rs:1339-1341（RRF 需要行 id 做连接键）
② 向量侧清掉 full_text_search                 # rust/lancedb/src/query.rs:1345
③ try_join! 并发跑 FTS 与向量两路             # rust/lancedb/src/query.rs:1346  ← 并发，不是串行
④ rank 归一化（可选 NormalizeMethod::Rank）  # rust/lancedb/src/query.rs:1365
⑤ min-max normalize_scores                   # rust/lancedb/src/query.rs:1369
⑥ reranker（默认 RRFReranker）               # rust/lancedb/src/query.rs:1372-1376
⑦ 按 limit 截断                              # rust/lancedb/src/query.rs:1394
```

**第 ③ 步的并发性是关键工程点**：两路用 `try_join!` 并发执行，任一路失败整体失败。

**第 ⑤ 步的 min-max 归一化**（`rust/lancedb/src/query/hybrid.rs:150`）有一个防除零细节：当 `max - min < 1e-4` 时**改用 `max` 作分母**，而不是直接除以 0（注释 `rust/lancedb/src/query/hybrid.rs:149` 明确说这等价于 numpy 的 `np.isclose`）。

**RRF 公式**（`rust/lancedb/src/rerankers/rrf.rs:87`）：

```text
RRF(d) = Σ_over_lists  1 / (rank_list(d) + k)

默认 k = 60                                  # rust/lancedb/src/rerankers/rrf.rs:41
k = 60 接近最优，但取值不敏感                # rust/lancedb/src/rerankers/rrf.rs:31
来源：Cormack et al., SIGIR 2009
```

实现上以 row id 为键在 `BTreeMap<u64, f32>` 上累加两路贡献（`rust/lancedb/src/rerankers/rrf.rs:85`、`rust/lancedb/src/rerankers/rrf.rs:14`），最后按分数降序 `sort_to_indices`（`rust/lancedb/src/rerankers/rrf.rs:117`）。

**复杂度**：`O(N_fts + N_vec + U log U)`，其中 N 为两路结果长度、U 为并集行 id 数。RRF 只看**排名**不看原始分数——这正是它能融合「BM25 分数」和「向量距离」这种量纲完全不同的召回源的原因。

### 26.7 过滤与召回的三个旋钮

| 旋钮 | 默认 | 语义 | 引用 |
|------|------|------|------|
| `prefilter` | `true` | `true` = 先过滤再向量检索；`false`（`postfilter()`）= 先向量检索再过滤 | `rust/lancedb/src/query.rs:860`、`rust/lancedb/src/query.rs:572` |
| `refine_factor` | 无 | 重排阶段放大倍数 | `rust/lancedb/src/query.rs:1048` |
| `fast_search` | `false` | 只扫已索引数据，弱一致快路径 | `rust/lancedb/src/query.rs:481` |

**`postfilter` 的机制与代价**（`rust/lancedb/src/query.rs:489`）：过滤被挪到 **refine 阶段**执行，只在小结果集上跑谓词，延迟更低；代价是**可能返回少于 `limit` 甚至 0 行**——如果最近的邻居都不满足过滤条件（`rust/lancedb/src/query.rs:499`）。

**`refine_factor` 的两阶段重排**（`rust/lancedb/src/query.rs:1273`）：

```text
① ANN 检索 top (limit × refine_factor)     例：limit=10, factor=3 → 取 30 条
② 取回这 30 条的完整未压缩值
③ 按真实距离重排，保留前 10 条
```

注意 `refine_factor = 1` 与**完全不调用**并不等价：即使取 1 倍，LanceDB 仍需取回完整未压缩值以便重排，因此一定影响延迟（`rust/lancedb/src/query.rs:1281`）。HNSW 的 `ef` 另有默认 `1.5 × limit`（`rust/lancedb/src/query.rs:1045`）。

**`fast_search` 的合法性前提**来自 25.5：新数据本就通过「并行扫已索引 + 未索引」被搜到，所以只扫索引只是**放弃这部分弱一致保证换延迟**；要让新数据进入快路径，需先跑 `optimize` 把数据并入索引（`rust/lancedb/src/query.rs:483`）。

**组合决策表**：

| 场景 | 建议配置 |
|------|---------|
| 过滤选择性高（如 `category = 'book'`）且列上有标量索引 | `prefilter = true`（默认）+ 该列建 `BTree` / `Bitmap` |
| 谓词过复杂或列上无索引、过滤不筛选 | `postfilter()` + 调大 `refine_factor` |
| 只查已索引数据、能接受弱一致 | `fast_search()` + 定期 `optimize` |
| 需要高召回且延迟可换 | `refine_factor(3)` |
| 向量 + 关键词 | 默认 RRF（k=60），必要时自定义 reranker 实现 `Reranker` trait |

## 二十七、参考资料

- [LanceDB GitHub](https://github.com/lancedb/lancedb)
- [LanceDB 官方文档](https://lancedb.github.io/lancedb/)
- [Lance 列存格式](https://lancedb.github.io/lance/)
- [LanceDB Cloud](https://cloud.lancedb.com/)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [Apache Arrow](https://arrow.apache.org/)
- [Delta Lake 论文](https://www.vldb.org/pvldb/vol13/p3411-armbrust.pdf)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)
- [DuckDB Lance 集成](https://duckdb.org/docs/api/lance)