# Lance Wiki（Rust 列式向量数据格式 — Lance file format）

> 参考版本：Lance 0.20 / 0.21 / 0.22（2024-2025），Rust 1.75+，与 Apache Arrow / DuckDB / Apache DataFusion 深度集成。本 wiki 重点覆盖 Lance 作为 AI 时代列式向量数据库文件格式的演进、版本管理、随机访问、与 Arrow / DataFusion 互转，以及与本项目的关联。

## 一、项目定位

Lance（Lance File Format）是由 **LanceDB 团队** 创建的 **「为 AI / 机器学习设计的列式数据格式」**，在 Apache Arrow 列式内存结构的基础上，提供：

- **百万级向量随机访问（point queries / KNN）**；
- **版本管理（time travel / zero-copy schema evolution / versioning）**；
- **高效的 update / delete / insert 操作**（不像 Parquet 那种「append-only」批量文件）；
- **统一的列存 + 行存 + 向量 + 文本 + 多模态存储**；
- **AI / RAG / DataFrame 原生集成**（DuckDB、Polars、DataFusion、HuggingFace Datasets）。

口号： **"Modern columnar data format for ML. Built for the AI era."**

- **核心痛点**：
  - **Parquet 不支持高效 update / delete**：每次 update 都必须 rewrite 整个 RowGroup；
  - **Parquet 不支持版本管理**：无 time travel、无 schema evolution；
  - **传统列存无法随机访问**：AI 场景下「亿级向量的 KNN + point lookup」需重新查询整张 Parquet；
  - **AI 数据集需要元数据 + 嵌入向量 + 文本 + 图像标签**：传统文件格式（Parquet / Avro / ORC）都不支持多模态；
  - **大数据训练 / RAG 需要 zero-copy 与 Arrow**：JSON / CSV / Parquet 都需 copy 到 Arrow，浪费 CPU；
  - **现有 Arrow IPC 不能 efficient random read**：Arrow IPC 是 stream 写入，无索引；
- **典型用户**：
  - **AI / RAG 应用**：嵌入向量存储 + retrieval-augmented generation；
  - **DuckDB / Polars / DataFusion 用户**：希望高效 update + 版本管理 + KNN 集成；
  - **DataFrame / 数据湖**：希望 AI 数据集可版本化、可版本回溯；
  - **企业 ML 平台**：Feature Store / DataFrame Lib / RAG 引擎；
  - **教学 / 个人项目**：HuggingFace Datasets AI 数据集；
  - **云原生 ML 服务**：LanceDB Cloud + Lance OSS + DataFusion + Polars + HuggingFace 集成；

## 二、发展历史与社区

- **起源**（2023）：
  - Lance 项目创始人 **Chang She**（前共情 / LanceDB 创始人，原 Dremio 工程 leader）；
  - 2023 Q1 出现社区讨论：「Parquet 是 1990 年代的格式，AI 时代需要新的列式格式」；
  - 2023 Q3 Lance 0.1 发布，基础 File format spec；
- **里程碑**：
  - 2023-08：0.1.0，基础 File format + 简单 update；
  - 2023-12：0.2.0，Full-Text Search、Index（IVF / HNSW）；
  - 2024-02：0.3.0，LanceDB Python 集成、DuckDB 集成（`duckdb-lance`）；
  - 2024-05：0.4.0，Versioning 1.0、Multiple Schema 版本；
  - 2024-07：0.5.0，**Lance 官宣 use Apache Arrow 作为内存模型**、RAG 增强；
  - 2024-09：0.8.0，多模态数据（图像 / 视频 / 音频 + parquet + text）；
  - 2024-12：0.10.0，DataFusion 集成 DataFusion-Lance、HuggingFace 官方集成；
  - 2025-Q1：0.18/0.20，**Lance file format spec 2.x**、`lance-rs` 1.0 + LanceDB Cloud；
  - 2025-Q3：0.22.x，**Index V2（IVF_PQ、HNSW、FT、BTree）** + Lance Database 升级；
- **维护状态**：**高速发展**，月 commit 200+（2024）；多家公司（Meta、HuggingFace、Disney）采用；
- **社区**：
  - GitHub 1.5k+ star，Discord 社区 5k+ 用户，`developer@lancedb.com`；
  - 每年 1-2 次 LanceDB 开发者峰会；
  - 合作社区：Apache DataFusion、HuggingFace、DuckDB Labs；
- **关键 contributor**：Chang She（创始 CEO）、毛鲁伟、毛建华（Meta）、Will Jones（Pathway）、Leandro Reis、Gundega Rozenberga；

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（`lance/`、`lance-core/`、`lance-io/`、`lance-datafusion/`、`lance-python`（PyO3 + Python wheels））；
- **与商业产品的关系**：
  - **LanceDB Cloud**：LanceDB 商业托管（serverless-managed），提供 SaaS API；
  - **LanceDB Enterprise**：企业版（私有部署 + 集中管控）；
  - **Meta / Disney / Apple**：基于 Lance 构建 AI 数据湖；
  - **HuggingFace**：`from_ lance` 在官方 Datasets 集成中；
- **生态关系**：
  - **上游依赖**：
    - **Apache Arrow Rust**：`arrow-array` / `arrow-schema` / `arrow-select`；
    - **tokio runtime**：异步 IO；
    - **object_store**：S3/GCS/Azure/OSS；
    - **Apache DataFusion**：SQL 引擎集成；
    - **faiss-rs / hnswlib-sys**（可选）：ANN 索引；
  - **下游生态**：
    - **LanceDB Python**：`lancedb`、`pylance`、`pyarrow` 互转；
    - **DuckDB**：`duckdb-lance` / `lance-duckdb` 集成；
    - **Apache DataFusion**：`datafusion-lance` SQL 查询 Lance 表；
    - **Polars**：`polars-lance` 文件 IO；
    - **HuggingFace**：`datasets` API 集成；
    - **LanceDB Cloud**：商业 SaaS；
    - **Milvus（实验）**：Lance 作为 Collection 持久化层；
    - **Chroma（实验）**：Lance 作为 Segment Storage；

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **LanceDB Cloud**：Lance 的官方商业 SaaS；
  - **Meta AI**：Meta 部分 AI 数据集使用 Lance；
  - **Disney**：Disney+ 内容搜索 + 推荐（基于 Lance + LanceDB）；
  - **HuggingFace**：HuggingFace 部分公共数据集转 Lance 格式；
  - **Apple**：Apple 内部分析平台部分模块；
  - **Pathway**：Pathway 流式 + Lance 集成；
- **典型场景**：
  - **AI 数据湖**：亿级 embedding + image + tag + text 多模态存储；
  - **RAG 应用**：Lance + LanceDB + DataFusion 提供 SQL + 向量 KNN；
  - **向量数据库**：Lance Native 内置 vector index（IVF_PQ、HNSW）；
  - **DataFrame 与 SQL 互转**：DuckDB 查询 Lance 表，Polars 互转；
  - **Data Versioning**：DVC-like 时间机器追踪数据变换（`lance commit`、`lance undo`）；
  - **云原生 OLAP**：Lance 数据存 S3/GCS，按 Snapshot 版本访问；
  - **检索应用**：Lance + LanceDB + LangChain/LlamaIndex 取代 Pinecone / Weaviate；
  - **Edge 部署**：Lance 文件小（仅 ~10KB/百万行级别），适合边缘模型 cache；

## 五、一句话对比定位

> Lance 是 **「AI 时代列式数据格式 + Arrow 内存模型 + 内置 ANN 索引（IVF_PQ/HNSW/FT）+ 版本管理 + 高效 update/delete + 多模态 + DataFusion/DuckDB/Polars 集成 + Apache 2.0 + LanceDB 商业 SaaS」**；与 **Parquet（被动只读列存）**、**Iceberg（事务层表格式）**、**Delta Lake（事务层表格式）**、**Hudi（事务层表格式）**、**Arrow IPC（内存层协议）**、**DuckDB（嵌入式列存 DB）**、**LanceDB（嵌入式向量数据库）**、**FAISS（向量库）**、**Milvus（分布式向量库）**、**Avro / ORC** 构成大数据与 AI 数据湖存储生态。Lance 的差异化是 **「AI 原生设计 + Arrow 共享内存 + 版本管理 + 内置索引 + DataFrame 集成 + Apache 2.0」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          API 层                                      │
│  - Rust: lance crate (LanceDataset::open / scan / take / update)   │
│  - Python: pylance / lancedb (PyO3 绑定)                            │
│  - C/C++: 通过 Apache Arrow C ABI                                   │
│  - Java/Scala: 通过 Arrow Java + Flight SQL                         │
│  - CLI: lance CLI (commit, version, surgery, optimize)             │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          DataFrame / SQL 集成层                     │
│  - datafusion-lance: Apache DataFusion 表提供器                   │
│  - duckdb-lance: DuckDB 桥接                                        │
│  - polars-lance: Polars 桥接                                        │
│  - datasets-lance: HuggingFace Datasets 集成                       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          File Format 层（lance-core）               │
│  - Manifest (Dataset-level Metadata)                                │
│  - Fragment (Sub-Dataset, 由 N 个文件组成)                          │
│  - DataFile (Columnar File Format, 1+ Column Chunks)                │
│  - Column Chunk Index (BTree Index on Primary Key)                  │
│  - Page (Columnar Block, 由 Arrow 数组切片)                         │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          Index 层                                    │
│  - IVF_PQ Index (Vector KNN, Inverted File + Product Quantization) │
│  - HNSW Index (Hierarchical Navigable Small World)                  │
│  - BTree Index (Primary Key / Textual Search)                        │
│  - Full-Text Search (N-gram / Token List / Bitmap)                  │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          IO 层（lance-io）                          │
│  - object_store: S3/GCS/Azure/OSS/HDFS/Local 抽象                  │
│  - Async Read/Write (tokio-based)                                   │
│  - Caching: 内存 / SSD / MMR                                       │
│  - Cloud Optimized: 文件不连续 + 索引 + 可分块读取                │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          内存层（lance-core）                       │
│  - Apache Arrow RecordBatch（兼容 Arrow C ABI）                     │
│  - 零拷贝跨 Rust / Python / C 边界                                 │
│  - 自定义 DataType（Lance 自有扩展）                                │
└─────────────────────────────────────────────────────────────────────┘
```

**关键设计原则**：

1. **Arrow 内存模型**：Lance 的内存层完全基于 Arrow `RecordBatch`，与 Arrow IPC、DataFusion、DuckDB 零互转；
2. **Cloud Native**：文件按 `fragment` 切分（默认 1024-8192 MB/fragment），远程 partial read；
3. **Index Co-located**：BTree / IVF_PQ / HNSW / FT index 与 data file 平级，**为云上 partial read 服务**；
4. **Versioning Native**：每个 Dataset 有 `Manifest[N]`，通过 commit 追加 + 回滚；
5. **Upsert / Delete Friendly**：通过 hidden tombstone page + Materialized View 高效处理 update / delete；
6. **Multimodal Friendly**：schema 内 image/uri 列存为 String，实际数据放外部文件 + 引用；
7. **Time Travel**：通过 Manifest 版本号访问历史 snapshot；
8. **Open Format Spec**：完整 Lance File Format spec 完全公开，社区可在任意语言实现 reader / writer。

## 七、Lance 作为扩展的集成模式

Lance 作为数据文件格式，提供 **嵌入式库 + 服务端 + DataFrame 集成 + SQL 引擎集成** 多种集成模式。

### 7.1 嵌入式库（最常见）

```rust
use lance::dataset::{Dataset, WriteMode};
use lance::utils::LanceProjector;

// 1. 写 Lance 数据集
let writer = lance::Dataset::write(reader, "my_data.lance")
    .with_write_mode(WriteMode::OverwriteExisting)
    .execute()?;

// 2. 读取 Dataset
let dataset = Dataset::open("my_data.lance").await?;

// 3. 扫描（向量 KNN + 过滤）
let scan = dataset.scan()
    .column_names(["id", "vector", "name"])
    .filter("age > 18")
    .nearest("vector", &query_vec, 10)  // KNN
    .execute().await?;

// 4. 单 point lookup
let batch = dataset.take_rows(&[42u64], "id".parse()?).await?;
```

### 7.2 Python 集成

```python
import lancedb
import pyarrow as pa

# 1. 创建数据库
db = lancedb.connect("/tmp/lance_db")
table = db.create_table("docs", schema=pa.schema([
    ("id", pa.int64()),
    ("text", pa.utf8()),
    ("vector", pa.list_(pa.float32(), 1536))
]))

# 2. 写入数据
table.add(arrow_data)

# 3. KNN 查询
result = table.search(query_vector) \
    .where("age > 18") \
    .metric("cosine") \
    .limit(10) \
    .to_pandas()

# 4. 版本切换
table.checkout("commit_xyz")
```

### 7.3 DataFusion 集成

```rust
// 在 DataFusion SQL 中查询 Lance 表
use datafusion::prelude::*;
use datafusion_lance::LanceTableProvider;

let ctx = SessionContext::new();
ctx.register_table("lance_table",
    Arc::new(LanceTableProvider::new("path/to/dataset")))?;
let batches = ctx.sql("SELECT id, name FROM lance_table WHERE age > 18")
    .await?
    .collect()
    .await?;
```

### 7.4 DuckDB 集成

```sql
-- 直接查询 Lance 表
SELECT * FROM lance_scan('my_data.lance') WHERE age > 18;
```

### 7.5 HuggingFace Datasets 集成

```python
from datasets import load_dataset

# 直接加载 Lance 格式的 HF 数据集
ds = load_dataset("poloclub/diffusiondb", format="lance", split="train")
```

### 7.6 服务端 / 远程访问

通过 `lance-remote`（实验）或 LanceDB Cloud，远程访问 Lance 数据集（HTTP / Flight）。

## 八、索引机制（核心）

Lance 的索引设计是核心，本节详细展开。

### 8.1 数据结构（File Format 视角）

```
Lance Dataset (commit N)
│
├── _versions/
│   └── <version>.manifest
│       - schema: Arrow Schema
│       - fragments: [FragmentMetadata]
│       - index: {field -> IndexMetadata}
│       - commit_date, commit_message
│
├── fragments/
│   └── <fragment_id>.lance
│       - DataFile
│         └─ ColumnChunk[i]
│             └─ Page[j] (Arrow array slice)
│       - Index (optional, IVF_PQ / HNSW / BTree / FT)
│       - Tombstones (delete / update log)
│
└── _indices/
    └── <index_uuid>.idx
        - IVF centroids / HNSW graph nodes
```

### 8.2 BTree Index

```python
table = db.create_table("docs", data)
# 创建 BTree 索引（对 id 列）
table.create_index("id", index_type="BTREE")

# 高效 point lookup
result = table.search().where("id = 42").to_pandas()
# 直接通过 fragment + page 索引定位
```

BTree Index 用于：
- 主键（primary key）高效 lookup；
- 范围查询（`WHERE id >= 100 AND id < 200`）；
- 字符串精确匹配（`WHERE url = '...'`）；
- 通过 fragment + data offset 直接定位数据，避免全表扫描。

### 8.3 IVF_PQ Index（向量 KNN）

```python
# 创建 IVF_PQ 索引
table.create_index(
    "vector",
    index_type="IVF_PQ",
    num_partitions=1024,  # IVF 中心数
    num_sub_vectors=32    # PQ 子量化器数
)

# KNN 查询
result = table.search(query_vector) \
    .metric("L2") \
    .nprobe(20) \         # 探测中心数
    .refine_factor(10) \  # 重排比例
    .limit(10) \
    .to_pandas()
```

**算法**：

```
1. IVF（Inverted File）：对 vector 列做 K-Means 聚类（`num_partitions` 个中心）；
2. PQ（Product Quantization）：每个向量切分为 `num_sub_vectors` 段，每段用 256-codebook 量化；
3. Query：先找最近 `nprobe` 个中心，再在中心内 PQ 距离估算 TopK；
4. Refine：用原始向量重排 refine_factor * limit 条候选。
```

**关键参数**：
- `num_partitions`：K-Means 聚类中心数（建议 sqrt(rows)）；
- `num_sub_vectors`：PQ 量化精度（32、48、64 常见）；
- `nprobe`：查询时探测的中心数（越大越精确越慢）；
- `refine_factor`：重排倍数（默认 2-10）。

### 8.4 HNSW Index（向量 KNN）

```python
table.create_index(
    "vector",
    index_type="HNSW",
    m=16,
    ef_construction=128
)
```

HNSW 算法（详见 `hnswlib.md` wiki）：
- 多层可导航小世界图；
- 每个节点保留 `m` 个邻居 + 上层连接稀疏；
- 构造复杂度 O(N log N)，查询 O(log N)；
- 比 IVF_PQ 更精确但占用更大内存。

### 8.5 全文索引（Full-Text Search）

```python
# 创建全文索引（token + position 列表）
table.create_index("text", index_type="FTS")

# BM25 全文搜索
result = table.search("query string").to_pandas()
```

- **Token 分词**：基于 tokenizer（默认英文）；
- **倒排索引**：每个 token → doc_id list；
- **位置索引**：支持 phrase query；
- **Bitmap** ：用于高基数 token 过滤。

### 8.6 自适应多索引（Multi-Index Combo）

```python
# HNSW + Scalar Index 同时使用（混合检索）
table.create_index("vector", index_type="HNSW")
table.create_index("category", index_type="BTREE")

# 混合检索：先向量粗排 + 后 scalar 过滤
result = table.search(qv) \
    .where("category = 'shoe'") \
    .limit(10) \
    .to_pandas()
```

混合索引关键：
- **HNSW 提供 KNN 候选**（快速 TopK with high recall）；
- **BTree 提供 scalar 过滤**（高 cardinality 字段）；
- Lance 自动协调二者取交集；

### 8.7 Manifest + 版本管理

```
Dataset
│
├── _versions/
│   ├── 1.manifest  ← initial
│   ├── 2.manifest  ← append data
│   ├── 3.manifest  ← update row 42
│   └── 4.manifest  ← delete rows [100-200]
│
├── _fragments/
│   ├── 00001.lance  ← original fragment
│   ├── 00002.lance  ← appended fragment
│   ├── 00003.lance  ← new fragment (with tombstones for row 42)
│   └── 00004.lance  ← tombstone-only fragment for rows [100-200]
```

**特性**：
- **Time Travel**：`table.checkout("3")` 访问版本 3 的状态；
- **Rolling Back**：`lance undo` / `table.checkout(current_version - 1)`；
- **Append**：写入时追加 `commit(N+1)` + 新 fragment；
- **Update**：标记原行 tombstone + 插入新行 + 新 commit；
- **Delete**：标记原行 tombstone + 新 commit；
- **Optimize**：定期重写为 less-fragmented 状态。

### 8.8 嵌入式 Block Cache + SSD 缓存

```python
# 配置块缓存
db = lancedb.connect(
    cache_size_limit_bytes = 8 * 1024**3,  # 8 GB memory cache
    cache_disk_path = "/tmp/lance_cache"  # SSD cache spill path
)
```

- 多级缓存：memory (`LRU`) → SSD (`LRU`) → S3；
- **零拷贝跨 cache 层**：通过 Arrow slice + 引用计数；
- **Lazy materialization**：仅查询实际访问的 column chunk。

## 九、查询 / 计算 / 接口层

### 9.1 SQL 集成

Lance 不内置 SQL 解析，但通过 DataFusion、DuckDB、Polars 集成提供 SQL：

```sql
-- DataFusion SQL 查询 Lance
SELECT id, name FROM lance_table WHERE age > 18 LIMIT 10;

-- DuckDB SQL 查询 Lance
SELECT * FROM lance_scan('my.lance') WHERE vector_ip(vec, qv) > 0.5;

-- Polars Rust
let df = LazyFrame::scan_lance("path.lance", ScanOptions::default())?
    .filter(col("age").gt(lit(18)))
    .select([col("name"), col("id")])
    .collect()?;
```

### 9.2 DataFrame API

```python
import lancedb
db = lancedb.connect("/tmp/lance_db")
table = db.open_table("docs")

# 1. 简单查询
result = table.to_pandas()

# 2. 过滤
result = table.search().where("age > 18").to_pandas()

# 3. 向量搜索
result = table.search(query_vector, vector_column_name="vector", k=10).to_pandas()

# 4. 混合搜索
result = table.search(query_vector) \
    .where("category = 'shoe'") \
    .nprobe(20) \
    .limit(10) \
    .to_pandas()

# 5. 向量化 += append + delete
table.add(new_data)
table.delete("id > 100")  # 删除 id > 100
```

### 9.3 自定义 UDF / Transform

```python
import pyarrow as pa
from lancedb.udf import register_udf

@register_udf("sentence_embedding")
def sentence_embedding(text_batch: pa.Array) -> pa.Array:
    # 调用本地模型
    return [model.encode(text.as_py()) for text in text_batch]
```

### 9.4 Lance CLI

```bash
# Inspect 目录结构
lance dataset inspect /path/to/lance_db

# 列出索引
lance dataset indices /path/to/lance_db/docs

# 创建索引
lance dataset create-index /path/to/lance_db/docs vector ivf_pq

# Optimize 合并 fragment
lance dataset optimize /path/to/lance_db/docs

# 提交版本
lance dataset commit /path/to/lance_db -m "added 1000 rows"

# 回滚
lance dataset undo /path/to/lance_db

# 列出所有版本
lance dataset history /path/to/lance_db

# 治理文件
lance dataset surgery /path/to/lance_db/
```

### 9.5 向量化计算

Lance 完全基于 Arrow，因此 SIMD 加速：
- L2 / Cosine / IP 距离：AVX2 / AVX512 加速；
- Filter / Take / Project：Arrow `select` + `take` API；
- 聚合：Arrow `aggregate` API；
- KNN：IVF_PQ + HNSW 走 SIMD 距离计算。

### 9.6 客户端 SDK

| 语言 | 库 | 状态 |
|------|----|------|
| **Rust** | `lance`、`lance-iox` | 官方核心 |
| **Python** | `lancedb`、`pylance` | 官方（PyO3） |
| **C/C++** | 通过 Arrow C ABI | 通过 DataFusion / DuckDB |
| **Java/Scala** | 通过 Arrow + Flight | 实验 |
| **Go** | 通过 Arrow Go | 实验 |
| **JS/TS** | 通过 Arrow JS | 实验 |

## 十、事务与一致性

- **Lance 不提供分布式事务**（单实例一致性）：
  - 同一 Dataset 内的写入：**Atomic**（commit 后可读）；
  - 多 Dataset 跨写：**不保证原子**，需业务层协调；
  - **MVCC 风格**：Manifest 版本号 + Fragment Tombstone，**读不阻塞写**；
  - **Snapshot 隔离**：每个 commit 是一个 snapshot；
  - **Time Travel**：访问过去版本；
- **分布式事务**：
  - LanceDB Cloud 提供 Multi-table transaction；
  - 与 Iceberg / Delta Lake 整合时，事务由上游协调；
  - Lance OSS 仍单实例事务；

## 十一、复制与高可用

- **Lance 自身无复制机制**：
  - 数据存于 S3/GCS/Local，靠 Object Storage 的多版本 + 跨 region 复制；
  - Manifest 版本号天然支持多副本 + 恢复；
- **冷热分层**：Lance 数据可在 hot S3 + cold S3 Glacier 间迁移；
- **PITR（Point-in-Time Recovery）**：保留 N 个老 Manifest 版本 → 时间点恢复；
- **LanceDB Cloud**：自动多 region 复制 + cross-region failover；

## 十二、扩展性

| 维度 | 实现 |
|------|------|
| **水平扩展** | 通过 LanceDB Cloud（多 shard）；OSS 不支持，但可手工 sharding by ID range |
| **垂直扩展** | 通过 Arrow SIMD + Memory cache + SSD cache |
| **动态扩缩容** | LanceDB Cloud K8s HPA；OSS 不支持 |
| **冷热分层** | 通过 S3 Lifecycle Policy |
| **多租户** | 一个数据库多个 table，独立 Manifest |

## 十三、性能与基准

- **向量 KNN**：
  - IVF_PQ（num_partitions=1024）：1 亿 768 维，p95 KNN latency 约 20-50ms；
  - HNSW：1 亿 768 维，p95 KNN latency 约 5-15ms（IVF_PQ 显著快召回率相近时）；
- **Point Lookup**：
  - 1000 万行 + BTree Index：单行 lookup < 1ms；
  - 1 亿行 + BTree Index：单行 lookup 1-3ms；
- **Scan**：
  - 全列 scan：~500 MB/s（SSD、SIMD 加速下）；
- **Update / Delete**：
  - 1000 万行中 update 10000 条：< 2 秒；
  - vs Parquet 完全重写：> 5 分钟；
- **版本管理开销**：每次 commit ~1-10ms（仅写 Manifest）；
- **第三方基准**：ann-benchmarks、HNSW Benchmarks、HuggingFace Datasets 加载速度基准，Lance 较 Parquet 加载快 2-10×；
- **vs Parquet**：
  - 读：性能相当 / Lance 部分场景快（索引 + 切片）；
  - 写：Lance 支持 fast update / delete（Parquet 不支持）；
  - 体积：相当（uncompressed）/Lance 大 10-20%（含索引）；

## 十四、运维与可观测性

- **监控**：
  - **LanceDB Cloud** 提供 Prometheus + OpenTelemetry 集成；
  - **Lance OSS**：通过 `lance::Instrument` 暴露 trace；
- **慢查询日志**：
  - Lance `tracing` crate 集成 OpenTelemetry；
- **备份恢复**：
  - 通过 Manifest 版本号实现 PITR；
  - S3 version control + Lifecycle 配合；
- **Tracing**：
  - Rust `tracing` crate；
  - HuggingFace `datasets` 集成后端；
  - LanceDB Cloud 通过 OpenTelemetry tracing；
- **Profile 工具**：
  - `cargo flamegraph`、`perf`、Dtrace、Pyroscope；
  - `py-spy` for Python call stack；
- **资源使用**：
  - 内存 cache：`cache_size_limit_bytes`；
  - SSD cache：`cache_disk_path`；
  - Object Storage IO：optimize 后可减少；

## 十五、生态与集成

- **官方库**：
  - `lance`（Rust core）；
  - `lance-core`（File format schema）；
  - `lance-io`（异步 IO + Object Store）；
  - `lance-index`（IVF_PQ、HNSW、BTree、FT）；
  - `lance-datafusion`（DataFusion 集成）；
  - `lance-encoding`（column encoding decoder）；
- **官方 Python 包**：
  - `pylance`（Rust 绑定 + Python 接口）；
  - `lancedb`（LanceDB SDK + Python wrapper）；
- **Data Frame / SQL 集成**：
  - `datafusion-lance`：DataFusion 表提供；
  - `duckdb-lance`：DuckDB 桥接；
  - `polars-lance`：Polars 桥接；
- **AI / ML 框架**：
  - HuggingFace Datasets（官方支持）；
  - LangChain（`langchain-lancedb`）；
  - LlamaIndex（`llama-index-vector-stores-lancedb`）；
  - Haystack（实验）；
- **文件格式交换**：
  - Parquet ↔ Lance（`lance-convert`）；
  - CSV ↔ Lance（Arrow CSV）；
  - JSON ↔ Lance（Arrow JSON）；
  - ORC / Avro：需自定义；
- **客户端 SDK**：
  - Python、Rust；
  - C/C++（通过 Arrow C ABI）；
  - JavaScript（实验）；
- **BI 工具**：通过 DataFusion 或 DuckDB 暴露 ODBC / JDBC；
- **容器化**：
  - `lancedb/lancedb` Docker；
  - `apache/datafusion` + Lance image；
- **Kubernetes Operator**：
  - LanceDB Cloud 提供 Operator；
  - OSS 可通过 `datafusion-on-k8s` + Lance image；
- **Helm Chart**：
  - `lancedb-cloud` Helm chart（商业）；
- **消息队列**：
  - 通过 Kafka Connect + Lance Sink 集成（实验）；

## 十六、核心论文与算法原理

- **Lance File Format Specification v2**（2024）：Lance 团队官方 spec；
- **IVF_PQ 量化**：
  - **Jegou et al. (2011). Product Quantization for Nearest Neighbor Search**（IEEE TPAMI）；
- **HNSW**：
  - **Malkov & Yashunin (2018). Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs**；
- **Versioned File Format**：
  - **Apache Iceberg 论文**：版本管理 + 隐藏 partition；
  - **Delta Lake 论文**：基于 _delta_log 文件；
- **Full-Text Search**：
  - **BM25 / TF-IDF**：Robertson (2009);
  - **Bitmap 索引**：Wu (2014). "Faster Indexes in Lucene";
- **Modern File Format**：
  - **Apache Parquet Spec**：持久化层基准；
  - **Apache Arrow 内存层 spec**；
- **Embeddings / ANN 基准**：
  - ann-benchmarks、HNSWlib benchmarks；

## 十七、源码学习路径

### 17.1 入门（必须看）

1. **`lance/src/dataset.rs`**：`Dataset::open` / `write` / `scan` / `take` 主入口，约 1500 行；
2. **`lance-core/src/schema.rs`**：Lance Schema 与 Arrow Schema 互转；
3. **`lance-core/src/data/fragment.rs`**：Fragment 是 Lance 写入的基本单位；
4. **`lance-io/src/object_store.rs`**：S3 / GCS 抽象；
5. **`lance/src/io/commit.rs`**：Manifest 提交逻辑。

### 17.2 进阶（重要模块）

6. **`lance-core/src/format/manifest.rs`**：Manifest 数据结构；
7. **`lance-core/src/datatypes/field.rs`**：自定义 Field 类型；
8. **`lance/src/index/`**：`IVF_PQ` / `HNSW` / `BTree` / `FT` 索引实现；
9. **`lance-index/src/vector/`** + `pq.rs` / `ivf.rs` / `hnsw.rs` / `flat.rs`；
10. **`lance/src/io/writer.rs`** + `reader.rs`：写入 / 读取主路径；
11. **`lance/src/dataset/transaction.rs`**：commit / rollback / append / update / delete；
12. **`lance-datafusion/src/`**：DataFusion Table Provider。

### 17.3 高阶（可选）

13. **`lance-encoding/src/`**：Column encoding / decoding（Dictionary / BSS / MiniBlock）；
14. **`lance/src/io/exec/`**：Lance-level Operator pipeline（Scan / Filter / Take / Project / KNN）；
15. **`lance/src/dataset/optimize.rs`**：Optimize Compaction；
16. **`lance/src/index/vector/index_stage.rs`**：Index rebuild pipeline；
17. **`python/src/`**：PyO3 绑定（约 2000 行 Rust + 2000 行 Python）；
18. **`pylance/src/dataset.rs` / `table.rs`** / `indices.rs`：Python wrapper；
19. **`docs/format.rst`**：Lance format spec 草稿（约 500 行 + 阅读 IO spec）；
20. **`lance/src/io/cache.rs`**：Memory + SSD multi-level cache。

## 十八、源码规模与开发语言

- **Rust**：约 **12 万行**（不含依赖）：
  - `lance/`：约 4 万行（核心 orchestration）；
  - `lance-core/`：约 3 万行（File format schema + encoding）；
  - `lance-io/`：约 1.5 万行（异步 IO + Object Store）；
  - `lance-index/`：约 3 万行（IVF_PQ + HNSW + BTree + FT）；
  - `lance-datafusion/` + `lance-encoding/` 等：约 1.5 万行；
- **Python 绑定**：`pylance` + `lancedb` + `pyarrow` 互转，约 **1.5 万行**（含 Python + PyO3 Rust）；
- **C++**：极少（仅 benchmark）；
- **C 绑定**：通过 Arrow C ABI；
- **Cargo workspaces**：单仓库多 crate（约 12+）；
- **测试代码**：约 25% 代码量（`lance/src/test/`、`pylance/python/tests/`）；
- **文档**：约 15% 代码量（含 format spec）；

## 十九、学习资源

- **官方主页**：
  - https://lancedb.com/ ：LanceDB 官网；
  - https://lancedb.github.io/lance/ ：Lance 项目主页（GitHub Pages）；
- **官方文档**：
  - https://lancedb.github.io/lance/ ：用户指南；
  - https://lancedb.github.io/lance/format.html ：File Format Spec；
- **官方 GitHub**：
  - https://github.com/lancedb/lance ：Lance 主仓；
  - https://github.com/lancedb/lancedb ：LanceDB Python 集成；
- **Lance Format Spec**：
  - https://lancedb.github.io/lance/format.html ：Full Spec；
- **官方博客**：
  - https://blog.lancedb.com/ ：Lance 团队博客；
- **演讲与会议**：
  - **DEBS 2024 / VLDB 2024**：Lance 团队演讲；
  - **ApacheCon 2024**：Lance + DataFusion 演讲；
- **第三方教程**：
  - **DataCamp**: "Modern AI Data Format with Lance"；
  - **Towards Data Science**: "Lance: AI-native Columnar Data Format"；
  - 中文 CSDN / 知乎专栏（搜索「Lance 列式格式」）；
- **Hacker News / Reddit**：
  - r/MachineLearning、`/r/dataengineering`；
  - Hacker News `lance` tag；
- **书籍**：
  - 暂无专门 Lance 书籍；
  - 可参考 "Data Pipelines Pocket Reference"（James Densmore）；
- **示例代码**：
  - `lance/examples/` / `lance/python/examples/`：约 50+ 示例；

## 二十、可移植性与部署

- **支持的平台**：
  - Linux（x86_64, ARM64）；
  - macOS（x86_64, arm64）；
  - Windows（x86_64）；
  - Rust ≥ 1.75；
  - Python ≥ 3.9；
- **Object Store**：
  - S3（含 AWS S3、MinIO、Ceph、S3-compatible）；
  - GCS；
  - Azure Blob Storage；
  - Cloudflare R2；
  - 阿里云 OSS、腾讯云 COS、华为云 OBS；
  - 本地文件系统；
  - HDFS（实验）；
- **容器化**：
  - `lancedb/lancedb-server` Docker；
  - `lance/lance` Docker（运行时镜像）；
  - Apache DataFusion + Lance image（社区）；
- **Helm Chart**：
  - LanceDB Cloud Helm（商业）；
- **K8s Operator**：
  - LanceDB Cloud Operator；
  - **Apache Operator SDK**：DataFusion Operator + Lance；
- **嵌入式部署**：
  - 静态库 / 动态库：约 30-80MB；
  - Python wheel：约 50-80MB；
  - 最常见：嵌入式 ML 推理服务（CPU）；
- **部署形态**：
  - 嵌入式 Python / Rust；
  - LanceDB Cloud (serverless-managed)；
  - LanceDB Enterprise（私有部署）；
  - HuggingFace Datasets API；
- **Edge / Lambda / WASM**：
  - 通过 PyO3 → Python wheel → 在 Lambda 中运行（受限 250MB binary）；
  - Lance WASM 实验（Rust → wasm32）；

## 二十一、变体与衍生项目

| 项目 | 关系 |
|------|------|
| **LanceDB** | 嵌入式 + 服务器版本，基于 Lance |
| **LanceDB Cloud** | Lance 的 SaaS 商业版 |
| **LanceDB Enterprise** | 企业版私有部署 |
| **HuggingFace Datasets (Lance 后端)** | HuggingFace 官方支持 |
| **LangChain LanceDB** | LangChain 集成 |
| **LlamaIndex LanceDB** | LlamaIndex 集成 |
| **Apache DataFusion + Lance** | DataFusion 表提供 |
| **DuckDB + Lance** | DuckDB Bridge |
| **Polars + Lance** | Polars 桥接 |
| **Milvus（实验）** | Lance 作为 Collection Storage |
| **Chroma（实验）** | Lance 作为 Segment Storage |

## 二十二、风险与陷阱

### 22.1 数据一致性

1. **Commit 失败**：网络中断 / 进程崩溃可能导致 commit inconsistency，**Lance 0.18+ 提供 atomicity via external system (S3 conditional writes)**；
2. **Manifest 版本污染**：S3 上 NFS-style 锁失效时，多客户端同时写入可能丢失 commit，**建议单写多读**；
3. **外部 Object Store 强一致性**：OSS、GCS 默认提供 strong consistency，但**部分 S3-compatible (Ceph) 仍需测试**；
4. **Tombstone 膨胀**：大量 update / delete 会让 fragment 充满 tombstone，**需定期 `optimize` 合并**；

### 22.2 性能陷阱

1. **IVF_PQ num_partitions 不匹配**：太小精度差，太大 build 慢 + 内存大；**经验 sqrt(rows)**；
2. **HNSW m 过大**：m=64 / ef_construction=200 → 内存极大；**建议 m=16 / ef=64**；
3. **Full-Text Index 不感知中文**：默认 tokenizer 只处理英文 + 空格，**中文需 jieba 或自定义**；
4. **大数据 + 频繁 update**：碎片率高，**导致 query 慢**；需频繁 optimize；
5. **Vector 字段用 Float32 占用大**：1536 维 = 6KB / 行，**10 亿行 = 6TB 原始 vector**，**应用 halfvec 或 PQ 量化**；
6. **多 Index 并存代价**：每个 index 占空间 + IO 加速有限；**只在 hot field 上建索引**；
7. **Take + Filter 反序问题**：先 take 再 filter vs 先 filter 再 take 有不同性能，**Lance 的 optimizer 还在完善**；
8. **Manifest 频繁 commit**：高频 commit 导致 manifest 文件数膨胀，**可压缩为 `_versions/0001-0010.manifest_aggregated`**；
9. **Parquet 文件中有误字段引用**：Lance 严格 schema，**不支持 Parquet 兼容 schema evolution，需要显式 mapping**；

### 22.3 集成 / SQL 陷阱

1. **DataFusion **Lance** 集成**：当前仅支持简单 select / filter / KNN，**不支持 insert / update**（通过 lance 自行 commit）；
2. **DuckDB bridge **LanceScan**：Experimental，**某些 Arrow type 不支持 (Union, Map)**；
3. **HuggingFace Datasets 编码**：HF Datasets 用 Lance 时**可能丢失 metadata**，建议显式转 Arrow first；
4. **Polars 与 Lance 类型映射**：Decimal / DateTime64 可能失精；**建议转 string**；
5. **Vector 索引传 null 列**：embedding 列中存在 null 时 index build 会出错；**建议先 filter null**；
6. **Lance remote**（cloud）：实验状态，**易超时**；

### 22.4 运维 / 安全

1. **无 RBAC**：Lance OSS 无内置用户权限；**建议外部用 ACL 限定 S3 path**；
2. **PII 风险**：`column metadata` 中可能含列来源（CSV 头），**可被攻击者扫描**；
3. **S3 / OSS IAM 越权**：Lance 调 S3 时需要 IAM 角色，**建议 on-behalf-of IAM**；
4. **Cache 文件泄漏**：Memory / SSD cache 可能在磁盘中存 sensitive data，**建议 SSD 加密**；
5. **PyO3 GIL 与多线程**：pylance 在多线程下需用 GIL 释放 API，否则**严重拖慢**；
6. **Long-running Index Build**：HNSW 大数据集 build 可能 OOM；**建议 partial build**；
7. **no compliance tooling**：Lance 不内置 GDPR / PCI / HIPAA，**业务层需自行设计**；
8. **GC 压力**：pylance 大表查询时，C++ 端通过 PyO3 多次大内存释放，**Python GC 压力大**；

### 22.5 兼容性

1. **Lance 格式 spec 仍在演进**：v1 → v2 改动某些字段，**跨大版本读旧数据需 upgrade**；
2. **Manifest 跨大版本不兼容**：0.10 → 0.20 间 manifest schema 改了**，建议升级到 0.22+**；
3. **Index 重建**：升级 Lance 后**首次 build index 时 rebuild**；
4. **Arrow 类型演进**：Arrow Decimal128 / Decimal256 在不同 Lance 版本下表现不同，**显式 cast**；
5. **Python wheel 平台差异**：Lance wheel 仅 Python 3.9-3.12，**旧版本 Python 3.8 不兼容**；
6. **wasm 限制**：Lance WASM（实验）不支持 HNSW，**仅 IVF_PQ**；
7. **GPU 索引暂不支持**：Lance 暂不内置 GPU 索引，**推荐使用 cuVS / FAISS GPU**；

## 二十三、与本项目的关联

| engineering 模块 | 与 Lance 关联 |
|------------------|----------------|
| `engineering/src/db/storage/` | 持久化层可参考 Lance 的 fragment + manifest 设计 |
| `engineering/src/db/columnar/` | 列存引擎可参考 Lance 的 column chunk + page 编码 |
| `engineering/src/db/vector/` | 向量存储可参考 Lance 的 IVF_PQ + HNSW + 二级索引 |
| `engineering/src/db/index/` | B-Tree + 全文索引可参考 Lance 的 BTree + FT index |
| `engineering/include/db/versioning.h` | 版本管理可参考 Lance 的 manifest + commit chain |
| `engineering/src/db/executor/` | 执行器可以参考 Lance 的 scan pipeline (Scan → Filter → Take → KNN) |

具体落地路径：

1. **第一步**：Cargo workspace 增加 `lance`、`lance-io`、`arrow-rs` 依赖；
2. **第二步**：将 `engineering/src/db/storage/` 中 columnar 存储改为 Lance 的 fragment 抽象；
3. **第三步**：将 `engineering/src/db/vector/` 中向量索引替换为 Lance IVF_PQ + HNSW；
4. **第四步**：增加 `engineering/include/db/versioning.h`，实现 Manifest + commit chain（参考 Lance 的 `_versions/`）；
5. **第五步**：在 `engineering/sdk/python/` 增加 `pylance` 绑定，让用户通过 Python `lancedb` 访问本数据库；
6. **第六步**：在 `engineering/apps/db_server/` 提供 HTTP / Flight Server，远程访问 Lance 数据集（参考 `lance-server`）；

## 二十四、PQ 距离估算的两类算法、Fragment 合并与墓碑的读取语义

> 本节源码路径：`rust/lance-index/src/ivf/pq.rs`、`rust/lance-index/src/ivf/ivf.rs`、`rust/lance/src/dataset/fragment.rs`、`rust/lance/src/dataset/reader.rs`（canonical，模块未 init，行号需校对）。§8.3 给出 IVF_PQ 的**四步流程**与参数含义，§8.7 给出 manifest/版本的文件布局与特性清单。但**三个决定实际性能与正确性的机制仍未展开**：PQ 的距离估算究竟用「非对称距离」还是「对称距离」、两者精度差多少、以及 fragment 合并与墓碑（tombstone）在读取路径上如何被正确处理。
>
> **模式说明**：本篇走模式 B。路径为 Lancedb `lancedb/lance` 主干的稳定目录布局，**具体行号需在子模块 init 后校对**。符号名（`ProductQuantizer`、`centroids`、`Fragment`、`deletion_file`、`Refine`）沿用上游与 FAISS 系列的命名。

### 24.1 PQ 的两类距离：对称距离 SDC vs 非对称距离 ADC

§8.3 说「PQ 距离估算 TopK」和「Refine 用原始向量重排」，但**没说估算用什么距离**。这是 PQ 里精度与速度的核心取舍，有两种算法：

```
# 定义：原向量 x（dim 维），切成 M 段 x = [x_1, ..., x_M]
#       codebook：每个 m 有一个 256 项码本 C_m（256 个代表向量）
#       编码：code(x) = [id_1, ..., id_M]，每个 id ∈ [0,256)，共 M 字节

— 对称距离 SDC（Symmetric Distance Computation）：
    query 与 database 【都】用同一个 codebook 编码，再用码字距离：
        SDC(q, code(x)) = Σ_m || q̂_m - c_m[id_m] ||²
    ★ query 也被量化 → 引入两次量化误差（query 侧 + database 侧）

— 非对称距离 ADC（Asymmetric Distance Computation）：
    query 保持【原始精度】，只有 database 被量化：
        ADC(q, code(x)) = Σ_m || q_m - c_m[id_m] ||²
    ★ 误差只有 database 侧一次

分解（关键恒等式，让 ADC 的查找表加速成为可能）：
    ||q_m - c_m[id_m]||² = ||q_m||² - 2·⟨q_m, c_m[id_m]⟩ + ||c_m[id_m]||²
    累加后：
        ADC(q,x) = Σ_m||q_m||²  -  2·Σ_m ⟨q_m, c_m[id_m]⟩  +  Σ_m ||c_m[id_m]||²
                  └── 常数 ──┘      └──── 可预计算 LUT ────┘     └── 常数 ──┘
```

**「非对称」的含义**是 query 侧用更高精度（原始向量）、database 侧用量化——所以它「不对称」且**更准**。ADC 是工业界默认选择，理由是分解后可做**预计算查找表**：

```
建立查找表（每个 query 只做一次，O(M × 256)）：
    LUT[m][i] = || q_m - c_m[i] ||²          # m ∈ [0,M)，i ∈ [0,256)

查询单个 code(x)：
    dist = Σ_m LUT[m][ code(x)[m] ]          # M 字节查表 + M 次加法
    ★ 【没有乘法、没有向量减法】

对比不查表的朴素做法：Σ_m ||q_m - c_m[id_m]||² = 每段一次减法+平方+求和
```

| 维度 | SDC | ADC |
|------|-----|-----|
| 精度 | 较低（query 侧也被量化） | **较高（只量化 database）** |
| 查表加速 | 可以（两边都是码字） | 可以（且常数项可预折叠到外层） |
| 每次查询建 LUT 成本 | O(M×256) | O(M×256) |
| LUT 内存 | — | O(M × 256) float32（M=32 时约 32 KB） |

> **复杂度与工程含义**：把 M 字节 code 的距离估算压到 **M 次查表 + M 次加法**（M=32 时约 64 次操作），相比直接算 M 段 L2 距离（32 × dim 次乘加，dim=128 时约 4096 次）**快约 60 倍**。这就是 PQ 能把向量检索从「毫秒」降到「微秒」的原因。
>
> LUT 大小为 `M × 256 × 4` 字节（M=64 时 64 KB），常驻 L1/L2 缓存；配合 code 的连续内存布局（32~64 字节正好一个缓存行），局部性良好。**这也是 PQ 天然适合批量查询的原因**——一次建 LUT 可扫多个 code。
>
> **refine 的必要性由此而来**：PQ 估算（哪怕是 ADC）仍有误差，所以 §8.3 的 `refine_factor` 要把前 `refine_factor × limit` 条候选**用原始向量重排**。经验值 2~10。PQ 的精度参数 `num_sub_vectors`（M）决定「每段维数 = dim / M」——M 越大每段越短、码本精度越高，但 code 变长、LUT 变大。

### 24.2 Fragment 合并与墓碑：读取路径的可见性判定

§8.7 列出 Update/Delete 都是「标记原行 tombstone + 新 commit」，但**读取时如何判定某一行是否可见**没有说明。这是 fragment 化存储的核心读路径。

```
读取一个全局 row_id：
  1. row_id 是【全局连续】的（fragment 内 offset + fragment 的起始 row_id）
  2. 定位 row_id 落在哪个 fragment（fragment 按 row_id 区间有序，可二分）
     ★ 若多个 fragment 覆盖同一 row_id（append 后又更新 → 产生重复 row_id），
       必须按【版本序】从新到旧遍历
  3. 对每个候选 fragment，检查 deletion file：
       若 row_id ∈ 该 fragment 的删除集合 → 该版本不可见 → 继续找更早的 fragment
  4. 第一个未被标记删除的 fragment 中的该行 → 返回

# 删除集合的存储形式（对应 §8.1 的 fragment 格式）：
  - bitmap：每bit 对应一个 row_id → 判定 O(1) 一次位测试
  ★ 这正是选 bitmap 而非 (deleted_ids list) 的原因：
    list 的判定是 O(log n) 查找，且内存随删除数增长
```

**墓碑累积的成本**：fragment 被大量行更新/删除后，其 deletion bitmap 会变长，而**fragment 数据文件本身大小不变**（墓碑在独立的 deletion file 里）。这导致「磁盘占用」与「有效数据量」逐渐背离，直到 `optimize` 重写。§8.7 把 optimize 描述为「重写为 less-fragmented 状态」，其算法实质是：

```
optimize（compaction）：
    输入：当前 version 的 fragment 集合（共 N 个，含大量 tombstone）
    1. 扫描所有 fragment，【丢弃】已删除行，重复row_id 【只保留最新版本】
    2. 按目标大小重新切分为少量 fragment（消除碎片）
    3. 写入新 fragment，产生新 manifest commit（N+1）
    ★ 旧 fragment 的物理文件【不立即删除】—— 仍被历史 version 引用
```

> **复杂度**：读取单行的可见性判定为 **O(覆盖该 row_id 的 fragment 数)**，典型为 O(1)（二分 + 一次位测试）；墓碑空间成本为 O(fragment 行数 / 8) 字节。optimize 本身是 O(总行数) 的全量重写，代价高，因此**触发时机通常按「墓碑占比超阈值」而非固定周期**。
>
> **关键设计权衡**：optimize 不删除旧 fragment 是**为 time travel 付出的必要代价**——若立即删除，`checkout("3")` 就读不到历史版本。这是「多版本存储」与「GC 及时性」的经典取舍，多数实现选择保留，并由后台 GC 按「无任何 version 引用」的条件回收。

### 24.3 IVF 的分区扫描与 nprobe 的召回率量化

§8.3 已给出 K-Means 步骤与 `num_partitions ≈ sqrt(rows)` 建议。这里补充 **nprobe 的召回率量化关系**（与 pgvector 的 IVFFlat 同源，见 `pgvector.md` §二十四）：

```
召回率是两个独立近似的【乘积】：
    recall ≈ P(覆盖) × P(排序精度)
            └── IVF ┘   └── PQ ┘

    P(覆盖)：真实最近邻所属分区被 nprobe 扫到的概率
        - 分布均匀时 ≈ nprobe / num_partitions
        - 分布倾斜（热门分区）时，热门分区被扫到的概率 > 均匀假设 → 覆盖更好

    P(排序精度)：该候选在 PQ 估算下仍排进 topK 的概率（见 §24.1 的 ADC 误差）
```

因此调参有明确顺序：

```
提高 recall 的优先级：
  1. 先调 nprobe（提升覆盖）—— 若最近邻所在分区根本没被扫到，
     后面 PQ 精度再高也无用（乘积的第一项为0）
  2. 再调 num_sub_vectors M（提升精度）—— 缩小每段维数，量化误差下降

总成本模型：
    cost ≈ nprobe × (N / num_partitions) × M    （次查表）
    ↑           ↑            ↑
 线性于nprobe  线性于分区大小线性于 code 长度

这解释了 Lance 默认 num_partitions 随数据量快速增长（远大于 pgvector 的默认 1000）：
    在大数据集上，【增大分区数】比【增大 nprobe】更划算——
    因为 nprobe 的增长直接线性乘在扫描量上，而分区数增长把每分区扫描量压下来
```

> **经验起点**：dim=768时 `num_sub_vectors` 取 48~96（每段 8~16 维），`nprobe` 取 10~20，`refine_factor` 取 2~10。若召回率仍不够，**先加 num_sub_vectors（精度成本低），再加 nprobe（延迟成本高）**。

---

## 二十五、参考资料

1. Lance 项目主页：https://lancedb.com/
2. Lance GitHub：https://github.com/lancedb/lance
3. Lance File Format Spec：https://lancedb.github.io/lance/format.html
4. Lance 文档：https://lancedb.github.io/lance/
5. LanceDB Python SDK：https://github.com/lancedb/lancedb
6. LanceDB Cloud：https://lancedb.com/cloud/
7. DataFusion + Lance 集成：https://github.com/lancedb/lance/tree/main/lance-datafusion
8. DuckDB + Lance bridge：https://github.com/lancedb/lance/tree/main/lance-duckdb
9. HuggingFace Datasets Lance：https://huggingface.co/docs/datasets/
10. Apache Arrow 项目：https://arrow.apache.org/
11. Apache Parquet 项目：https://parquet.apache.org/
12. Apache DataFusion 项目：https://datafusion.apache.org/
13. AI 数据湖文章：https://lancedb.com/blog/category/ai-data-lake/
14. Lance 论文与版本管理论文集：https://blog.lancedb.com/
15. Jegou et al. (2011) Product Quantization for Nearest Neighbor Search (IEEE TPAMI)
16. Malkov & Yashunin (2018) Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs (IEEE TPAMI)
17. Lance Format Spec 草案：https://github.com/lancedb/lance/tree/main/docs/format
18. Hacker News Lance tag：https://news.ycombinator.com/from?site=lancedb.com
19. PyLance：https://pypi.org/project/pylance/
20. LanceDB Discord：https://discord.com/invite/lancedb
