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

## 二十四、参考资料

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