# Apache Doris Wiki（MPP 实时 OLAP 数据库）

> 参考版本：Apache Doris 当前 commit `8e5b5f7f7e5b1b8c7e3d9f2a1b5c8e9d4a7c0b9a`（v2.x 系列），Java（FE）+ C++（BE）双语言实现，MySQL 协议，完整 MPP 架构。

## 一、项目定位

Apache Doris（原 **Palo**）是百度开源的 **MPP 实时 OLAP 数据库**，以 **MySQL 协议兼容**、**极简架构**（FE + BE）、**高并发实时写入 + 即席查询**著称，是国内最流行的 OLAP 数据库之一。它把"PB 级数据 + 亚秒查询 + MySQL 协议 + 实时 upsert"做到开箱即用，是 **SaaS / 用户行为 / 广告 / 报表**场景的事实标准之一。

- **痛点**：
  - ClickHouse 单表强但 JOIN / upsert 弱；
  - StarRocks 性能强但商业化重；
  - 传统 Hive / Spark SQL 延迟高（分钟级）；
  - 实时写入 + 即席查询难两全；
  - MySQL / PG 应用接入 BI 工具需要协议兼容；
- **典型用户**：数据分析、用户行为分析、广告实时归因、电商大屏、金融风控报表、SaaS 多租户报表；
- **核心口号**：**"MySQL-compatible, high-performance, real-time analytic database"** + **"A modern MPP database for real-time analytics"**。

## 二、发展历史与社区

- **起源**：2008 年由 **百度** 内部开发，初名 **Palo**；
- **里程碑**：
  - 2008-2017：百度内部使用，承载百度凤巢广告；
  - 2017-11：Palo 在 GitHub 公开（License: Apache 2.0）；
  - 2018：贡献给 Apache 基金会（**Apache Doris** / **Incubating**）；
  - 2019：v0.x 系列，核心架构（FE + BE）稳定；
  - 2020：v1.x 系列，引入向量化执行；
  - 2021：v1.2 引入 **TotalSort**、**冷热分层**；
  - 2022：v2.0 GA、**Multi-Catalog**（联邦查询）；
  - 2023：v2.1 引入 **Light Schema Change**、**Group Commit**；
  - 2024-2025：v2.2+ 引入 **Variant** 类型、**Auto Bucket**、**Data Lake 增强**；
- **维护状态**：极活跃（每年 2-4 个大版本），Apache Doris 社区 + 百度 SelectDB（商业公司）共同维护；
- **社区**：GitHub 30k+ stars、4000+ commits/year、Slack 6000+ 用户、中文社区极活跃（中国主导）。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）；
- **商业产品**：
  - **SelectDB**（原 Doris 中文社区商业公司）：商业版 SelectDB Cloud、SelectDB Enterprise；
  - **阿里云**：阿里云数据库 SelectDB 版、阿里云 Doris 托管；
  - **腾讯云 / 火山引擎 / 华为云**：均提供 Doris 托管；
- **生态关系**：
  - 与 **MySQL**（协议完全兼容）；
  - 与 **Flink**（doris-stream-loader / Flink Doris Connector）；
  - 与 **Kafka**（Routine Load）；
  - 与 **Spark / Hive / Iceberg / Hudi / Paimon**（联邦查询）；
  - 与 **dbt**（dbt-doris adapter）；
  - 与 **Superset / Grafana / Tableau** BI 工具。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **百度**：核心广告分析（原始场景）；
  - **京东**：订单分析、商品分析；
  - **美团**：商家分析、用户画像；
  - **腾讯**：广告归因、用户行为；
  - **字节跳动**：广告报表、内部数据分析；
  - **小米**：用户行为、电商报表；
  - **快手**：直播数据、广告曝光；
  - **网易**：游戏数据分析；
  - **B 站**：UP 主分析、视频分析；
  - **小红书**：用户画像、内容分析；
  - **平安**：金融风控报表；
  - **中国移动**：网络遥测；
- **行业**：广告、电商、游戏、内容、社交、金融、电信、教育；
- **社区案例**：每年 Apache Doris Summit 展示数十个用户案例。

## 五、一句话对比定位

> Apache Doris 是 **"MySQL 协议 + FE/BE 双层 + 向量化 + 实时 upsert + 完整 MPP"** 的 OLAP 数据库；与 **StarRocks（CBO 更强、向量化更激进）**、**ClickHouse（单表强、JOIN 弱）**、**Pinot（实时流为主）**、**Druid（亚秒聚合、Roll-up）** 一起构成 **OLAP 列式数据库阵营**。Doris 的差异化是 **"MySQL 协议兼容 + 极简架构（FE+BE）+ 完整 MPP + 实时 upsert"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                    客户端协议层（MySQL 协议 9030）                    │
│   - mysql-client / JDBC / Python (mysql.connector) / Superset / dbt │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Frontend (FE, Java)                                  │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  MySQL Protocol Layer + SQL Parser (libjava_sql_parser)      │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Planner & Optimizer：RBO + CBO + Cost-based Reorder         │   │
│   │  fe/fe-core/src/main/java/org/apache/doris/planner/         │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Catalog Manager：元数据 + 副本状态 + 分片路由               │   │
│   │  fe/fe-core/src/main/java/org/apache/doris/catalog/         │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Coordinator：生成 fragment plan + 调度 BE 节点              │   │
│   │  fe/fe-core/src/main/java/org/apache/doris/qe/              │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│                                                                       │
│   部署模式：                                                              │
│   - 1 FE（master） + N FE（follower/observer）                         │
│   - FE 自身使用 BDBJE 存储元数据                                       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ Thrift RPC（FE → BE fragment dispatch）
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Backend (BE, C++)                                     │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Storage Engine（Tablet / Segment / Rowset）                  │   │
│   │  be/src/olap/ + be/src/storage/                              │   │
│   │  - Tablet：每个分片一个 Tablet                                 │   │
│   │  - Segment：Tablet 内按 row 数切分                            │   │
│   │  - Rowset：写入批次（含 delete bitmap）                        │   │
│   │  - Columnar Writer + Reader                                  │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Column Reader：列存 + 向量化读取                              │   │
│   │  be/src/vec/columns/ + be/src/vec/exec/                     │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Vectorized Executor：向量化算子                              │   │
│   │  be/src/vec/exec/scan/ + be/src/vec/exec/join/              │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Fragment Executor + Exchange                                │   │
│   │  be/src/runtime/                                             │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│                                                                       │
│   部署模式：                                                              │
│   - N 个 BE 节点（无状态）+ 数据按 Tablet 分布                        │
│   - 每个 BE 可承载多个 Tablet                                           │
└─────────────────────────────────────────────────────────────────────┘
                      │
                      │ BE 间通过 brpc 通信（Exchange 数据流）
```

### 关键设计取舍

- **极简两进程**：只有 FE（Java，处理 SQL/元数据）和 BE（C++，处理存储/执行），没有 Master/Slave/Worker 区分；
- **MySQL 协议兼容**：FE 自带 MySQL 协议解析层，所有 BI 工具零成本接入；
- **完整 MPP**：FE 生成 fragment plan，分发到多个 BE 节点并行执行 + Exchange 数据交换；
- **向量化执行**：BE 端所有算子向量化（一次处理一批，~4096 行），CBO + 代价模型；
- **实时 upsert**：Unique Key 表通过 delete bitmap + 段合并实现（v1.x+ 引入）；
- **Tablet 分布**：每个表分桶（Bucket）+ Hash 分布，按 tablet 调度到 BE；
- **BDBJE 持久化**：FE 元数据通过 Berkeley DB Java Edition 持久化（写入文件 + 复制）；

## 七、存储引擎（列式专题深化）

Apache Doris 的存储引擎是其核心竞争力，本节重点展开 **Tablet/Segment/Rowset 架构** 与 **列存压缩**。

### 7.1 核心数据结构（Tablet / Segment / Rowset 三层架构）

```
+──────────────────────────────────────────────────────────────────+
|                       Table（逻辑表）                              |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Partition（分区，可选 RANGE / LIST）                       │    |
|   │   按 PARTITION BY RANGE(date) 划分                        │    |
|   │   ┌──────────────────────────────────────────────────┐   │    |
|   │   │  Tablet（分桶，Hash 分桶）                         │   │    |
|   │   │   DISTRIBUTED BY HASH(user_id) BUCKETS 32         │   │    |
|   │   │                                                    │   │    |
|   │   │   ┌──────────────────────────────────────────┐  │   │    |
|   │   │   │  Rowset（写入批次）                        │  │   │    |
|   │   │   │   - 由若干 Segment 组成                   │  │   │    |
|   │   │   │   - 含 version + delete bitmap            │  │   │    |
|   │   │   │   - 可持久化为 RowsetMeta                 │  │   │    |
|   │   │   │                                            │  │   │    |
|   │   │   │   ┌──────────────────────────────────┐    │  │   │    |
|   │   │   │   │  Segment（实际列存文件）           │    │  │   │    |
|   │   │   │   │   .seg（segment 主文件）           │    │  │   │    |
|   │   │   │   │   - ColumnWriter 写入              │    │  │   │    |
|   │   │   │   │   - 包含页脚（column metadata）    │    │  │   │    |
|   │   │   │   │   - 按列组织 + 压缩                │    │  │   │    |
|   │   │   │   │                                    │    │  │   │    |
|   │   │   │   │   ┌──────────────────────────┐    │    │  │   │    |
|   │   │   │   │   │  Column Page（列页）     │    │    │  │   │    |
|   │   │   │   │   │   page_size=16KB         │    │    │  │   │    |
|   │   │   │   │   │   压缩：LZ4 / ZSTD       │    │    │  │   │    |
|   │   │   │   │   │   编码：RLE/BitShuffle   │    │    │  │   │    |
|   │   │   │   │   └──────────────────────────┘    │    │  │   │    |
|   │   │   │   └──────────────────────────────────┘    │  │   │    |
|   │   │   └──────────────────────────────────────────┘  │   │    |
|   │   └──────────────────────────────────────────────────┘   │    |
|   └──────────────────────────────────────────────────────────┘    |
|                                                                    |
+──────────────────────────────────────────────────────────────────+
```

- **Tablet**：分桶（bucket），是 Doris 的最小物理单元，对应一个或多个 Segment 文件；
- **Segment**：Tablet 内按行数切分的列存文件（默认 100MB），持久化到磁盘；
- **Rowset**：写入批次（含 delete bitmap），记录该批次对应的 segment + 范围 + 版本；
- **后台 Compaction**：BE 后台 `CompactionDaemon` 持续合并 Rowset（解决小写放大的关键）。

### 7.2 列存压缩算法

Doris 借用 ClickHouse 的压缩思路，同时实现了自己的 Page 压缩。

| 算法 | 类型 | 适用场景 | 源码 |
| --- | --- | --- | --- |
| **`LZ4`** | 字节级 | 默认通用压缩，快 | `be/src/util/lz4_wrapper.h` |
| **`ZSTD`** | 字节级 | 高压缩比，慢一些 | `be/src/util/zstd.cpp` |
| **`LZ4F`** | LZ4 Frame | Doris 自定义 LZ4 帧格式 | `be/src/util/lz4_wrapper.h` |
| **`SNAPPY`** | 字节级 | Hadoop 生态常见 | `be/src/util/snappy_wrapper.h` |
| **`RLE`** | 游程编码 | 重复值序列 | `be/src/olap/rowset/segment_v2/rle_page.h` |
| **`BIT_SHUFFLE`** | 位重排 | 低基数整数 | `be/src/olap/rowset/segment_v2/bitshuffle_page.h` |
| **`DICT`** | 字典编码 | 低基数 string | `be/src/olap/rowset/segment_v2/dict_page.h` |
| **`PLAINTEXT`** | 无编码 | 已压缩数据 | `be/src/olap/rowset/segment_v2/plain_page.h` |
| **`FrameOfReference`** | 帧编码 | 整数范围小 | `be/src/olap/rowset/segment_v2/frame_of_reference_page.h` |

**典型 Page 编码组合**：RLE + BitShuffle + LZ4F 是 Doris 的"杀手锏"。

### 7.3 表模型

Doris 有四种表模型，影响存储和查询行为。

#### 7.3.1 Duplicate Key（默认）

```sql
CREATE TABLE events (
    ts DATETIME,
    user_id BIGINT,
    event_type VARCHAR(32),
    payload TEXT
)
DUPLICATE KEY(ts, user_id)
DISTRIBUTED BY HASH(user_id) BUCKETS 32
PROPERTIES ("replication_num" = "3");
```

- **行为**：完全保留所有写入行（不做去重 / 聚合）；
- **适用**：日志、原始事件、流式数据；
- **存储**：直接追加列存 Page，无聚合。

#### 7.3.2 Unique Key（实时 upsert）

```sql
CREATE TABLE user_profile (
    user_id BIGINT,
    name VARCHAR(64),
    age INT,
    last_update DATETIME
)
UNIQUE KEY(user_id)
DISTRIBUTED BY HASH(user_id) BUCKETS 32;
```

- **行为**：相同 KEY 自动覆盖（upsert）；
- **实现**：
  - **merge-on-write**：写入时合并（v2.0+ 默认，实时读一致）；
  - **merge-on-read**：读时合并（v1.x 默认，写入快但读放大）；
  - **delete bitmap**：标记被删除的行，避免读放大；
- **适用**：用户画像、配置表、订单状态；
- **源码路径**：
  - `be/src/olap/rowset/segment_v2/column_writer.h`；
  - `fe/fe-core/src/main/java/org/apache/doris/alter/SchemaChangeHandler.java`。

#### 7.3.3 Aggregate Key（预聚合）

```sql
CREATE TABLE page_view_agg (
    date DATE,
    user_id BIGINT,
    city VARCHAR(64),
    pv BIGINT SUM DEFAULT "0",
    uv BIGINT HLL_UNION HLL_UNION_DEFAULT
)
AGGREGATE KEY(date, user_id, city)
DISTRIBUTED BY HASH(user_id) BUCKETS 32;
```

- **行为**：相同 KEY 的指标列按聚合函数合并（`SUM` / `MAX` / `MIN` / `REPLACE` / `HLL_UNION` / `BITMAP_UNION`）；
- **聚合函数族**：
  - **SUM / MAX / MIN / REPLACE**：基础聚合；
  - **HLL_UNION**：HyperLogLog 去重计数；
  - **BITMAP_UNION**：Bitmap 精确去重计数；
  - **QUANTILE_UNION**：近似分位数；
- **适用**：预聚合 dashboard、UV/PV 统计；
- **源码路径**：`be/src/olap/rowset/segment_v2/aggregate_reader.cpp`。

#### 7.3.4 Primary Key（v2.0+，实时 upsert 强一致）

```sql
CREATE TABLE orders_pk (
    order_id BIGINT,
    status VARCHAR(16),
    amount DECIMAL(18, 2),
    PRIMARY KEY(order_id)
)
DISTRIBUTED BY HASH(order_id) BUCKETS 32;
```

- **行为**：强一致实时 upsert（基于 PK），写入即生效；
- **优势**：比 Unique Key merge-on-write 更高效，无需 delete bitmap；
- **适用**：订单状态、库存、配置中心；
- **源码路径**：`be/src/olap/primary_key_index.cpp` + `fe/fe-core/src/main/java/org/apache/doris/catalog/PrimaryKeyTable.java`。

### 7.4 索引机制

| 类型 | 用途 | 源码 |
| --- | --- | --- |
| **前缀索引** | Doris 自动取前 36 字节作为稀疏索引 | `be/src/olap/rowset/segment_v2/column_reader.cpp` |
| **Bloom Filter** | 等值过滤（v1.x+） | `be/src/olap/rowset/segment_v2/bloom_filter_index_reader.cpp` |
| **Zone Map** | 范围过滤（min/max） | `be/src/olap/rowset/segment_v2/zone_map_index.cpp` |
| **Ordinal Index** | 顺序定位 | `be/src/olap/rowset/segment_v2/ordinal_page_index.cpp` |
| **Bitmap Index** | 低基数等值（v2.x） | `be/src/olap/rowset/segment_v2/bloom_filter_index.h` |
| **倒排索引** | 文本 / 数组（v2.x） | `be/src/olap/rowset/segment_v2/inverted_index_reader.h` |
| **NGram BloomFilter** | 文本 N-gram（v2.x） | `be/src/olap/rowset/segment_v2/ngram_bloom_filter.h` |

### 7.5 Compaction 与冷热分层

- **Compaction**：`be/src/olap/compaction.cpp` —— 持续合并小 Rowset 为大 Rowset；
  - **Cumulative Compaction**：小合并（增量）；
  - **Base Compaction**：大合并（触发条件：Rowset 数 > 阈值）；
- **冷热分层**：v1.2+ 通过 Storage Policy 支持 S3 / HDFS 冷存储；
- **Tiered Storage**：v2.x+ 多级存储（Hot/Warm/Cold）；
- **源码路径**：`be/src/olap/storage_engine.h` + `be/src/olap/compaction_task_executor.cpp`。

### 7.6 存储源码路径速查

- Tablet：`be/src/olap/tablet.h` + `be/src/olap/tablet.cpp`；
- Segment：`be/src/olap/rowset/segment_v2/segment.h`；
- Rowset：`be/src/olap/rowset/rowset.h` + `rowset_meta.h`；
- ColumnWriter：`be/src/olap/rowset/segment_v2/column_writer.h`；
- ColumnReader：`be/src/olap/rowset/segment_v2/column_reader.h`；
- Page：`be/src/olap/rowset/segment_v2/page_builder.h` + `page.h`；
- Compaction：`be/src/olap/compaction.cpp` + `be/src/olap/cumulative_compaction.cpp`；
- Storage Engine：`be/src/olap/storage_engine.h`；
- PrimaryKey Index：`be/src/olap/primary_key_index.cpp`。

## 八、索引机制（简化）

Doris 的索引体系主要靠 **前缀索引 + Bloom Filter + Zone Map** 达成列存 + 实时 upsert 的极致性能。本节简述核心索引。

### 8.1 前缀索引

- **Doris 自动取前 36 字节**：建表时不需要指定索引列，Doris 自动取 `ORDER BY / DUPLICATE KEY / UNIQUE KEY / AGGREGATE KEY / PRIMARY KEY` 的前 36 字节作为前缀索引；
- **存储位置**：每个 Segment 文件页脚；
- **作用**：二分查找定位到具体 Page；
- **优化**：把 `WHERE` 过滤频次高的列放在 `KEY` 中靠前位置。

### 8.2 Zone Map（min/max）

- **每列每 Segment 的 min/max 值**：写入时自动记录；
- **作用**：范围过滤快速跳过整个 Segment（如 `WHERE ts < '2024-01-01'`）；
- **源码**：`be/src/olap/rowset/segment_v2/zone_map_index.cpp`。

### 8.3 Bloom Filter

- **高基数等值**：用户可手动为某列创建 Bloom Filter 索引；
- **代价**：写入开销 +10-20%，存储 +5%；
- **源码**：`be/src/olap/rowset/segment_v2/bloom_filter_index_writer.h`。

### 8.4 倒排索引（v2.x）

- **全文 / 数组**：v2.x 引入，支持 `LIKE` / `MATCH` / 数组 contains；
- **源码**：`be/src/olap/rowset/segment_v2/inverted_index_writer.h`。

## 九、查询 / 计算层

### 9.1 MySQL 协议 + SQL 方言

Doris **完全兼容 MySQL 协议**，SQL 方言与 MySQL / PG 类似。

- **MySQL 协议**：默认 9030 端口（query）+ 9020 端口（HTTP）；
- **SQL 方言**：
  - `SELECT ... FROM ... WHERE ... GROUP BY ... ORDER BY ... LIMIT`；
  - **JOIN**：INNER / LEFT / RIGHT / FULL / CROSS / SEMI / ANTI；
  - **WITH**：CTE；
  - **WINDOW**：窗口函数（`ROW_NUMBER` / `RANK` / `LAG` / `LEAD` / `SUM OVER`）；
  - **类型**：TINYINT / SMALLINT / INT / BIGINT / LARGEINT / FLOAT / DOUBLE / DECIMAL / DATE / DATETIME / VARCHAR / STRING / JSON / VARIANT / ARRAY / MAP / STRUCT / HLL / BITMAP / QUANTILE_STATE；
- **Doris 特有**：
  - `HLL_UNION` / `HLL_RAW` / `BITMAP_UNION` / `TO_BITMAP`；
  - `APPROX_COUNT_DISTINCT`；
  - `SHOW BACKENDS` / `SHOW TABLETS` / `SHOW PROC`；
  - `ADMIN SET REPLICA STATUS` / `ADMIN REPAIR TABLE`；
  - 多 Catalog 联邦查询（`CATALOG hive.db.table`）。

### 9.2 执行模型

```
MySQL 协议 → FE SQL Parser → Logical Plan → Optimizer（CBO）→ Physical Plan
                                                                        │
                                                          Fragment dispatch (Thrift)
                                                                        │
                                                            ┌───────────┴───────────┐
                                                            ▼                       ▼
                                                    Scan Fragment (BE)        Exchange
                                                            │
                                                    Local Execution (BE, 向量化)
                                                            │
                                                    Exchange back to Coordinator
                                                            │
                                                    Final Aggregation / Sort
                                                            │
                                                    Result back to Client
```

### 9.3 关键优化技术

- **CBO（Cost-Based Optimizer）**：v0.15+ 成熟，基于统计信息（`ANALYZE TABLE`）+ 代价模型选择 join order / 算子；
- **RBO（Rule-Based Optimizer）**：列裁剪、谓词下推、子查询改写；
- **向量化执行**：BE 端所有算子按 `Block`（~4096 行）批量处理；
- **Runtime Filter**：JOIN 时下推过滤器（min-max / bloom filter / in bitmap）；
- **Partition Pruning**：跳过整个分区（`PARTITION BY RANGE(ts)`）；
- **Bucket Pruning**：跳过无关 bucket；
- **Predicate Pushdown**：在 Storage 层先过滤；
- **TopN Pushdown**：大 LIMIT 提前截断；
- **Adaptive Executor**：自适应并发（v2.x）；
- **Pipeline 执行**：v2.x 引入，改进 Volcano 模型（参考 Velox/Impala）。

### 9.4 源码路径

- SQL 解析：`fe/fe-core/src/main/java/org/apache/doris/qe/StmtAnalyzer.java`；
- Planner：`fe/fe-core/src/main/java/org/apache/doris/planner/Planner.java`；
- Optimizer：`fe/fe-core/src/main/java/org/apache/doris/nereids/optimizer/`（v2.x Nereids 优化器）；
- Fragment：`fe/fe-core/src/main/java/org/apache/doris/planner/PlanFragment.java`；
- ScanNode：`be/src/vec/exec/scan/vscan_node.h`；
- Join：`be/src/vec/exec/join/vhash_join_node.h`；
- Aggregation：`be/src/vec/exec/aggregation/vaggregation_node.h`；
- Exchange：`be/src/runtime/data_stream_mgr.h`。

## 十、事务与一致性

Doris 提供有限的事务能力，**OLAP 场景** 不需要完整 ACID。

- **单 Tablet 写入原子性**：`INSERT` 内保证单 Tablet 原子（多 Tablet 不保证）；
- **`DELETE WHERE`**：等价 `UPDATE`（异步重写 Rowset）；
- **Stream Load 事务**：v2.x 引入 **2PC 事务**（`begin_transaction` + `commit_transaction`）；
- **Multi-Table 事务**：v2.x 实验性支持；
- **无跨节点 ACID 事务**（by design）；
- **Replication**：副本间通过 BDBJE（FE）+ Paxos-like 协议同步元数据；BE 间通过 `rowset_meta` 同步数据。

## 十一、复制与高可用

### 11.1 FE 高可用

- **BDBJE 持久化**：FE 元数据通过 Berkeley DB Java Edition 持久化；
- **Follower / Observer**：
  - **Follower**：参与选举（类 Paxos）；
  - **Observer**：不参与选举，仅做读写扩展；
- **故障切换**：任意 Follower 可晋升 Master，毫秒级；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/persist/EditLog.java` + `fe/fe-core/src/main/java/org/apache/doris/ha/BDBJEHA.java`。

### 11.2 BE 高可用

- **多副本**：每个 Tablet 默认 3 副本，写入需多数副本 ACK；
- **副本均衡**：FE 监控副本分布，自动调度迁移；
- **副本修复**：`ADMIN REPAIR TABLE` 触发副本同步；
- **副本类型**：默认 3 副本 + 写入多数副本确认（quorum）；
- **源码路径**：`be/src/olap/storage_engine.h` + `fe/fe-core/src/main/java/org/apache/doris/master/MasterImpl.java`。

### 11.3 数据可靠性

- **Tablet 副本**：默认 3 副本，分布在不同 BE；
- **冷热分层**：S3 / HDFS 多副本；
- **跨集群复制（CCR）**：v2.x 引入，支持异地灾备；
- **快照**：`BACKUP` / `RESTORE`（v2.x 全量 + 增量）；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/backup/BackupHandler.java`。

## 十二、扩展性（MPP 专题深化）

Doris 是 **完整的 MPP 数据库**，与 ClickHouse 的分布式查询（22.x+）相比更成熟。

### 12.1 集群架构

```
┌──────────────────────────────────────────────────────────────────┐
│                客户端连接任意 FE 节点                              │
│                          │                                       │
│                          ▼                                       │
│             ┌────────────────────────────┐                       │
│             │  FE Master (RW)            │                       │
│             │  + FE Follower             │                       │
│             │  + FE Observer             │                       │
│             └──────┬─────────────────────┘                       │
│                    │ Thrift RPC (fragment dispatch)               │
│        ┌───────────┼───────────┬──────────────┐                 │
│        ▼           ▼           ▼              ▼                 │
│    ┌───────┐   ┌───────┐   ┌───────┐     ┌───────┐              │
│    │ BE 1  │   │ BE 2  │   │ BE 3  │  ...│ BE N  │              │
│    │Tablet │   │Tablet │   │Tablet │     │Tablet │              │
│    │  1-8  │   │  9-16 │   │ 17-24 │     │       │              │
│    └───────┘   └───────┘   └───────┘     └───────┘              │
│        │           │           │              │                 │
│        └───────────┴───────────┴──────────────┘                 │
│                    │ brpc (Exchange)                              │
│            ┌───────▼────────────────────┐                       │
│            │  Coordinator Fragment     │                       │
│            └────────────────────────────┘                       │
│                                                                 │
└──────────────────────────────────────────────────────────────────┘
```

### 12.2 节点角色

| 角色 | 数量 | 职责 |
| --- | --- | --- |
| **FE Master** | 1 | 读写 + 选主 |
| **FE Follower** | 1-N | 参与选举 + 读写扩展 |
| **FE Observer** | 0-N | 仅做扩展读（不参与选举） |
| **BE** | 3+ | 数据存储 + 执行 |
| **Broker** | 0+ | HDFS / S3 读写代理（v1.x） |

### 12.3 数据分布策略

- **Hash 分桶**（推荐）：`DISTRIBUTED BY HASH(user_id) BUCKETS N`，数据均匀分布；
- **Random 分桶**：v2.x+，随机分布（无需 hash key）；
- **Range 分区 + Hash 分桶**：`PARTITION BY RANGE(ts) DISTRIBUTED BY HASH(user_id)`；
- **List 分区**：v2.x+，按枚举值分区（如地区）；
- **自动分桶**（v2.x）：Auto Bucket 根据数据量自动调整 bucket 数；
- **Colocation Group**：v1.x+，把小表广播到所有 BE 节点避免 shuffle；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/catalog/Partition.java` + `DistributionInfo.java`。

### 12.4 多阶段聚合

Doris 的 MPP 执行分为 **多阶段 fragment**：

- **阶段 0 (Local)**：每个 BE 节点扫描本地 Tablet 数据 + 局部聚合；
- **阶段 1 (Exchange + Merge)**：Coordinator 跨 BE 收集局部结果，合并为最终聚合；
- **阶段 2 (Final)**：可选最终聚合（`sum`、`count` 等）；
- **Broadcast Join**：小表广播到所有 BE（Colocation 或 hint）；
- **Shuffle Join**：大表间按 Join Key 哈希 shuffle；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/planner/PlanFragment.java` + `be/src/vec/exec/join/vhash_join_node.h`。

### 12.5 资源隔离

- **资源组（Resource Group）**：
  - v0.x：`SPROPERTIES ("resource_group" = "group_name")`；
  - v2.x：Workload Group + Resource Tag（CPU / 内存 / IO 限制）；
- **Workload Group（v2.x）**：
  - 通过 `CREATE WORKLOAD GROUP` 定义；
  - 软隔离（基于 CGroup）；
  - 支持 CPU 配额、内存限制、并发限制；
- **多租户**：通过 User + Resource Group 实现；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/resource/workloadgroup/WorkloadGroupMgr.java`。

### 12.6 动态扩缩容

- **添加 BE**：`ALTER SYSTEM ADD BACKEND "host:port"` → 自动迁移 Tablet；
- **移除 BE**：`ALTER SYSTEM DECOMMISSION BACKEND "host:port"` → 主动迁移数据；
- **数据重分布**：Tablet 重新调度（`REBALANCE DISK`）；
- **扩缩容粒度**：Tablet 级别（细粒度）；
- **自动均衡**：TabletScheduler 自动保持均衡；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/master/TabletScheduler.java`。

## 十三、性能与基准

### 13.1 内部基准

- **SSB（Star Schema Benchmark）**：标准 benchmark；
- **TPC-H / TPC-DS**：标准 SQL benchmark；
- **ClickBench**：OLAP 典型场景；
- **NYC Taxi / GitHub Events**：真实数据集。

### 13.2 性能数字（典型）

- **单表扫描**：1 亿行 < 1 秒；
- **Join 查询**：3-5 表 JOIN < 3 秒（百亿级数据）；
- **写入吞吐**：单机 ~100k rows/s（Broker Load）/ ~1M rows/s（Stream Load）；
- **实时 upsert**：Unique Key 表 merge-on-write 模式 100k rows/s；
- **压缩比**：5-10x（典型）。

### 13.3 性能调优

- **数据建模**：Duplicate / Unique / Aggregate Key 选择；
- **分桶**：`BUCKETS N` 取 `BE 数 × 4-8`；
- **统计信息**：`ANALYZE TABLE` 收集列统计；
- **CBO**：v0.15+ 推荐开启（默认开启）；
- **Runtime Filter**：默认开启；
- **会话变量**：`SET enable_vectorized_engine = true`（默认开启）；

## 十四、运维与可观测性

### 14.1 监控

- **Prometheus 集成**：`FE HTTP 端口 8030/metrics`、`BE HTTP 端口 8040/metrics`；
- **Grafana 仪表盘**：Doris 官方提供；
- **审计日志**：`fe/log/audit.log`（所有 SQL 记录）；
- **系统表**：`information_schema`、`__internal_schema`；
- **慢查询**：`SHOW QUERY STATISTICS`（v2.x）；
- **Tablet 状态**：`SHOW TABLET FROM tbl`；
- **副本分布**：`ADMIN SHOW REPLICA DISTRIBUTION FROM tbl`。

### 14.2 备份与恢复

- **Broker 备份**：v1.x 通过 HDFS 备份（`BACKUP SNAPSHOT`）；
- **Native 备份**：v2.x 原生支持 S3 / HDFS 全量 + 增量；
- **`BACKUP` + `RESTORE`**：`BACKUP SNAPSHOT db.snap TO repo ...`；
- **CCR（Cross-Cluster Replication）**：v2.x 异地灾备；
- **源码路径**：`fe/fe-core/src/main/java/org/apache/doris/backup/BackupHandler.java`。

### 14.3 升级与滚动

- **大版本**：滚动重启（先 Follower 再 Master）；
- **数据兼容性**：新版本读旧版本 segment 自动迁移；
- **回退**：保留旧二进制，重启即可读；
- **配置兼容**：`fe.conf` / `be.conf` 增量配置。

### 14.4 故障排查

- **`SHOW PROC "/cluster_health"`**：集群健康状态；
- **`SHOW PROC "/tablet_health"`**：Tablet 健康；
- **`SHOW PROC "/routine_load"`**：Routine Load 状态；
- **`SHOW BACKENDS`**：BE 节点状态；
- **`SHOW TABLETS FROM tbl`**：Tablet 状态；
- **常见问题**：Tablet 版本不一致、副本缺失、Compaction 卡死、BE OOM。

## 十五、生态与集成

### 15.1 数据接入

- **Stream Load**：HTTP POST 大批量导入（首选）；
- **Broker Load**：从 HDFS / S3 导入（批量）；
- **Routine Load**：从 Kafka 持续消费（流式）；
- **MySQL Load**：`INSERT INTO ... SELECT ... FROM mysql_db.tbl`（联邦）；
- **Spark Load**：从 Spark 写入（v1.x）；
- **Flink Doris Connector**：Flink Source / Sink；
- **dbt-doris**：dbt adapter；
- **DataX**：阿里开源 ETL 工具。

### 15.2 数据导出

- **`SELECT INTO OUTFILE`**：导出到 HDFS / S3；
- **MySQL 客户端导出**：`mysqldump`；
- **Spark / Hive**：联邦查询。

### 15.3 BI 与可视化

- **Apache Superset**：官方支持；
- **Grafana**：MySQL 数据源；
- **Tableau**：MySQL / JDBC；
- **Power BI**：MySQL；
- **FineBI / Smartbi / QuickBI**：国产 BI 工具均支持 MySQL。

### 15.4 客户端

- **MySQL CLI**：`mysql -h -P 9030`；
- **JDBC**：标准 MySQL JDBC driver；
- **Python**：`mysql.connector` / `pymysql`；
- **Go**：`go-sql-driver/mysql`；
- **Java**：标准 JDBC；
- **Doris 官方 SDK**：DorisStreamLoader（HTTPS 上传）。

## 十六、核心论文与算法原理

1. **Apache Doris 官方文档**：[doris.apache.org](https://doris.apache.org/docs)；
2. **"Apache Doris: A Modern MPP Database for Real-Time Analytics"**（VLDB 2024 demo）；
3. **"The C-Store Project"**（Stonebraker et al., VLDB 2005）—— 列存原理；
4. **"MonetDB/X100"**（Boncz et al., CIDR 2005）—— 向量化执行；
5. **"Volcano: An Extensible and Parallel Query Evaluation System"**（Graefe, IEEE 1994）—— Volcano 模型；
6. **"Efficiently Compiling Efficient Query Plans for Modern Hardware"**（Neumann et al., VLDB 2011）—— 向量化与代码生成；
7. **"How Good is My LSM-tree? Compaction Analysis"**—— Compaction 调优；
8. **"BDBJE 高可用"**（Oracle Berkeley DB 文档）；
9. **"Paxos Made Simple"**（Lamport, 2001）—— 一致性协议；
10. **"CBO 优化器"**（参考 Cascades framework，SQL Server）。

## 十七、源码学习路径

### 17.1 必读模块清单（推荐顺序）

| 优先级 | 模块 | 路径 | 说明 |
| --- | --- | --- | --- |
| ★★★★★ | FE 入口 | `fe/fe-core/src/main/java/org/apache/doris/qe/StmtExecutor.java` | SQL 执行入口 |
| ★★★★★ | FE Planner | `fe/fe-core/src/main/java/org/apache/doris/planner/Planner.java` | 生成 Plan |
| ★★★★ | FE Optimizer | `fe/fe-core/src/main/java/org/apache/doris/nereids/optimizer/` | Nereids CBO |
| ★★★★ | FE Catalog | `fe/fe-core/src/main/java/org/apache/doris/catalog/Catalog.java` | 元数据 |
| ★★★★★ | BE 入口 | `be/src/runtime/exec_env.h` | BE 进程 |
| ★★★★★ | ScanNode | `be/src/vec/exec/scan/vscan_node.h` | 扫描入口 |
| ★★★★ | Join | `be/src/vec/exec/join/vhash_join_node.h` | Hash Join |
| ★★★★ | Aggregation | `be/src/vec/exec/aggregation/vaggregation_node.h` | 聚合 |
| ★★★★★ | Tablet | `be/src/olap/tablet.h` + `tablet.cpp` | 存储抽象 |
| ★★★★★ | Segment | `be/src/olap/rowset/segment_v2/segment.h` | 列存文件 |
| ★★★★ | Compaction | `be/src/olap/compaction.cpp` | 后台合并 |
| ★★★ | HA | `fe/fe-core/src/main/java/org/apache/doris/ha/BDBJEHA.java` | FE 高可用 |

### 17.2 源码规模与语言

- **总 LOC**：~150 万行（FE Java + BE C++）；
- **Java 占比**：~40%（FE + 工具）；
- **C++ 占比**：~55%（BE）；
- **辅助**：Python（test）、CMake（build）；
- **构建系统**：Maven（FE）+ CMake（BE）；
- **编译时长**：BE 冷启动 30-60 分钟，FE 几分钟。

## 十八、学习资源

- **官方文档**：[doris.apache.org/docs](https://doris.apache.org/docs)；
- **官方博客**：[doris.apache.org/blog](https://doris.apache.org/blog)；
- **GitHub**：[github.com/apache/doris](https://github.com/apache/doris)；
- **Slack**：[apache-doris.slack.com](https://apache-doris.slack.com)；
- **中文社区**：Doris 中文用户群、Apache Doris Summit；
- **视频**：Doris Meetup、Bilibili Doris 教程；
- **认证**：Apache Doris 官方认证（Certified Apache Doris Engineer）。

## 十九、可移植性与部署

- **操作系统**：Linux（主）、macOS（开发）；
- **容器化**：官方 Docker 镜像；
- **Helm Chart**：[Doris Operator for Kubernetes](https://github.com/selectdb/doris-operator)；
- **Operator**：[doris-operator](https://github.com/selectdb/doris-operator)；
- **离线部署**：tar 包 + 配置即可；
- **云部署**：SelectDB Cloud / 阿里云 / 腾讯云 / AWS Marketplace。

## 二十、变体与衍生项目

- **Apache Doris**（开源，Apache 2.0）：核心；
- **SelectDB**（商业）：原 Doris 中文社区商业公司；
- **SelectDB Cloud**：托管云服务；
- **阿里云 SelectDB 版**：阿里云托管；
- **腾讯云 Doris**：腾讯云托管；
- **AWS Marketplace Doris**：AWS 部署镜像；
- **Doris 工具**：`doris-manager`、`doris-streamloader`、SelectDB Studio（GUI）。

## 二十一、风险与陷阱

### 21.1 协议与兼容性陷阱

- **MySQL 协议不完全支持**：事务、存储过程、用户变量行为差异；
- **数据类型差异**：Doris `LARGEINT`、`HLL`、`BITMAP` 不在 MySQL 标准；
- **`GROUP BY` 顺序敏感**：Doris 要求所有非聚合列都在 GROUP BY 中（与 MySQL 8.0+ 一致）；
- **索引自动选择**：Doris 自动取前 36 字节，与用户预期不符；
- **Unique Key merge-on-write vs merge-on-read**：版本差异行为不同；
- **AGGREGATE KEY 的 HLL/BITMAP**：重建时需用 `HLL_UNION`；
- **Broker 兼容**：v1.x 依赖 Broker，v2.x 已去除；
- **JDBC 兼容性**：MySQL JDBC 驱动可能不识别 Doris 特有类型；
- **流式事务（Stream Load 2PC）**：仅 v2.x+ 支持；
- **多 Catalog**：catalog 联邦查询性能差异大；
- **小表 Broadcast Join**：colocation 配置复杂；
- **自动分桶（Auto Bucket）**：v2.x+ 实验；

### 21.2 性能陷阱

- **`SELECT *`**：拉取所有列，列存优势丧失；
- **`ORDER BY` 顺序不当**：前缀索引失效；
- **高基数 JOIN Key**：shuffle 数据爆炸；
- **Broadcast Join 大表**：BE 内存 OOM；
- **不写统计信息**：`ANALYZE TABLE` 未运行 → CBO 退化；
- **GROUP BY 内存爆炸**：超过 `exec_mem_limit`；
- **Array / JSON 嵌套**：性能差（推荐预先 flatten）；
- **大 LIMIT OFFSET**：v1.x 全表扫描；
- **Stream Load 小包**：网络 / RPC 开销主导；
- **Routine Load 任务失败**：Kafka offset 管理；
- **多副本写入放大**：3 副本 3 倍网络；
- **频繁 Schema Change**：Light Schema Change 仅支持加列；
- **Tablet 数量过多**：单 Tablet < 100MB 时，调度开销大；
- **Compaction 卡死**：大 Rowset 合并慢；
- **UDF**：Be SDK C++，编写复杂；
- **跨 BE JOIN**：网络瓶颈（设计 colocation）；

### 21.3 运维陷阱

- **FE Master 单点**：BDBJE 选主慢（秒级）；
- **BE 磁盘满**：Tablet 不可写；
- **副本缺失**：必须 `ADMIN REPAIR TABLE`；
- **Compaction 卡死**：Tablet 不可读；
- **Routine Load 任务堆积**：Kafka lag 增长；
- **冷热分层数据迁移失败**：S3 权限错误；
- **备份恢复**：Broker 依赖（v1.x）；
- **CCR 延迟**：异地复制可能秒级延迟；
- **大版本升级**：未滚动重启导致副本不同步；
- **集群节点配置漂移**：`fe.conf` / `be.conf` 散落各处；
- **监控告警缺失**：未监控 Tablet 健康 / replica lag；
- **TLS 配置错误**：FE / BE 通信失败；
- **Workload Group 配置错误**：CGroup 路径错误导致资源限制失效；
- **网络分区**：BE 心跳超时导致误判；
- **Auto Bucket 误用**：自动分桶不适用于所有场景；
- **冷启动延迟**：重启后首次查询慢（mark / page 缓存冷）。

### 21.4 安全陷阱

- **默认无 root 密码**：必须改；
- **明文密码**：配置文件中明文；
- **SQL 注入**：动态 SQL 拼接；
- **网络暴露**：FE 9030 / 9020 端口对外暴露；
- **RBAC 配置复杂**：User / Role / Resource Group 易错配；
- **审计日志**：默认开启可能影响性能；
- **Kerberos / LDAP**：企业版特性。

## 二十二、学习路径

### 22.1 入门（1 周）

1. 阅读 [doris.apache.org/docs](https://doris.apache.org/docs) Quick Start；
2. Docker 启动 `apache/doris`；
3. 用 `mysql -h 127.0.0.1 -P 9030` 连接；
4. 创建第一张 Duplicate Key 表；
5. 用 Stream Load 导入数据；
6. 运行 `SELECT count() FROM table`；
7. 阅读 `be/src/olap/tablet.h` 的 100 行核心；
8. 阅读 [Apache Doris Summit 视频](https://www.youtube.com/@apachedoris)。

### 22.2 中级（2-4 周）

1. 部署 3 FE + 3 BE cluster；
2. 测试 Unique Key merge-on-write 实时 upsert；
3. 学习 Aggregate Key 预聚合；
4. 测试 JOIN 性能；
5. 配置 Workload Group 资源隔离；
6. 集成 Kafka Routine Load；
7. 集成 Flink Doris Connector；
8. 阅读 `fe/fe-core/src/main/java/org/apache/doris/qe/StmtExecutor.java`；
9. 阅读 `be/src/vec/exec/scan/vscan_node.h`；
10. 性能测试：`EXPLAIN` / `EXPLAIN ANALYZE`；
11. 启用 Prometheus metrics + Grafana dashboard。

### 22.3 进阶（1-3 个月）

1. 部署多 BE 多副本生产集群；
2. 调优 `exec_mem_limit` / `BUCKETS N` / 统计信息；
3. 实现自定义 Colocation Group；
4. 启用 CCR 跨集群复制；
5. 自研 UDAF（Java UDF）；
6. 阅读全部 `fe/fe-core/src/main/java/org/apache/doris/nereids/`；
7. 阅读 `be/src/olap/rowset/segment_v2/` 全部；
8. 阅读 `be/src/vec/exec/join/vhash_join_node.cpp` 的 join 实现；
9. 性能基准 vs StarRocks / ClickHouse；
10. 贡献 Apache Doris 社区（PR 新算子、修复 bug）；

## 二十三、与本项目的关联

`engineering/` 中**没有 Doris 实现**，但有以下可借鉴模式：

- **FE + BE 双进程架构** 与 `engineering/` 的 `db_server.c` + `bufmgr.c` 分层有相似之处；
- **Tablet + Segment 抽象** 可应用到 `engineering/src/db/storage_engine.c` 的多模态存储设计；
- **CBO + 向量化执行** 与 `engineering/src/db/sql_engine.c` 的执行器可对比学习；
- **Unique Key merge-on-write 实时 upsert** 与 `engineering/src/db/wal.c` 的 WAL + 版本链思想一致；
- **Workload Group 资源隔离** 可应用到 `engineering/src/db/` 的多租户隔离；
- **Compaction 机制** 与 `engineering/src/db/bufmgr.c` 的 checkpoint 设计思想一致；
- **Colocation Join** 与 `engineering/apps/` 的多副本数据库设计类似；
- **Multi-Catalog 联邦查询** 可应用到 `engineering/src/db/` 的多数据源查询；
- **MySQL 协议兼容** 与 `engineering/apps/db_driver/` 协议设计可对比学习；
- **测试模式**：Doris 的 `regression-test/` 数万条 SQL 测试用例，是高质量测试集范本；
- **Routine Load 流式导入** 可应用到 `engineering/apps/` 实时数据接入；
- **Stream Load HTTP 上传** 与 `engineering/apps/web/` 的 REST API 设计类似。

## 二十四、参考资料

1. **Apache Doris 官方仓库**：[github.com/apache/doris](https://github.com/apache/doris)；
2. **官方文档**：[doris.apache.org/docs](https://doris.apache.org/docs)；
3. **官方博客**：[doris.apache.org/blog](https://doris.apache.org/blog)；
4. **Doris Summit**：[doris.apache.org/summit](https://doris.apache.org/summit)；
5. **SelectDB**：[selectdb.com](https://selectdb.com)；
6. **Doris Operator**：[github.com/selectdb/doris-operator](https://github.com/selectdb/doris-operator)；
7. **Flink Doris Connector**：[github.com/apache/doris-flink-connector](https://github.com/apache/doris-flink-connector)；
8. **Spark Doris Connector**：[github.com/apache/doris-spark-connector](https://github.com/apache/doris-spark-connector)；
9. **dbt-doris**：[github.com/selectdb/dbt-doris](https://github.com/selectdb/dbt-doris)；
10. **Apache Doris 与 ClickHouse 对比**：doris.apache.org/blog；
11. **Apache Doris 与 StarRocks 对比**：doris.apache.org/blog；
12. **Star Schema Benchmark (SSB)**：[github.com/electrum/ssb-dbgen](https://github.com/electrum/ssb-dbgen)；
13. **TPC-H 文档**：[tpc.org/tpc_h](http://tpc.org/tpc_h)；
14. **Berkeley DB Java Edition (BDBJE)**：[docs.oracle.com/cd/E17277_02/html/berkeleydb/je](https://docs.oracle.com/cd/E17277_02/html/berkeleydb/je/index.html)；
15. **Volcano 模型**："Volcano: An Extensible and Parallel Query Evaluation System"（Graefe, IEEE 1994）；
16. **MonetDB/X100 论文**："MonetDB/X100: Hyper-Pipelining Query Execution"（Boncz et al., CIDR 2005）；
17. **C-Store 论文**："C-Store: A Column-oriented DBMS"（Stonebraker et al., VLDB 2005）；
18. **Apache Doris 论文**（VLDB 2024 demo）；
19. **SelectDB Cloud 文档**：[selectdb.com/docs](https://selectdb.com/docs)；
20. **Doris 中文社区文档**：[mp.weixin.qq.com/s/ApacheDoris](https://mp.weixin.qq.com/s/ApacheDoris)；
21. **Doris 设计模式**："Apache Doris: Modern MPP Database for Real-Time Analytics"（VLDB 2024 demo）；
22. **Doris 部署最佳实践**：doris.apache.org/docs/benchmark/best-practice；
23. **Doris 性能调优指南**：doris.apache.org/docs/admin-manual/optimization-guide；
24. **Doris 数据建模最佳实践**：doris.apache.org/docs/data-table/best-practice；
25. **Doris 多租户实践**：doris.apache.org/docs/admin-manual/multi-tenant。
