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

## 二十四、Segment v2 物理格式与稀疏索引

本节内容逐条对照 Apache Doris `master` 分支源码核对（核对基准：2026-10 的 commit `9e60fab`；该快照下 `docker/runtime/all-in-one/` 已出现 `4.1` 版本目录）。

> **路径迁移提示（重要）**：Doris 旧版本文档/博客中常见的 `be/src/olap/rowset/segment_v2/` 与 `be/src/vec/exec/`，在当前 master 中**已不存在**（`vscan_node.h`、`vhash_join_node.h`、`vaggregation_node.h`、`vectorized_reader.cpp`、`olap_page_cache.cpp` 均已检索不到）。它们分别迁移为 `be/src/storage/`（Segment / Rowset / Compaction / Index）与 `be/src/exec/`（执行算子）+ `be/src/core/`（Column / Type / Block）。下文统一使用**新路径**，涉及文件名时给出「旧名 → 新名」对照。注意：目录搬了，但 C++ 命名空间仍保留为 `doris::segment_v2`，读代码时不要被命名空间误导。

### 24.1 Segment 文件布局与页脚

一个 segment 落盘后是一组文件：

| 文件 | 后缀 | 内容 |
| --- | --- | --- |
| 数据文件 | `.dat` | 各列的 data page / index page / dictionary page 顺序拼接 |
| 二级索引文件 | `.idx` | 见 §26.2（`InvertedIndexDescriptor`） |

文件**尾部**是页脚 `SegmentFooterPB`，它是读一个 segment 的唯一入口。schema 定义在 `gensrc/proto/segment_v2.proto`（约 400 行，是理解 segment v2 的第一份必读文件），关键字段：

- `columns`（repeated `ColumnMetaPB`）——整张 tablet schema 的列元信息，每列带 `encoding` / `compression` / `is_nullable` / `dict_page` 指针 / `indexes`；
- `num_rows`——段总行数；
- `index_footprint` / `data_footprint` / `raw_data_footprint`——索引、编码后数据、原始数据的字节数，直接参与 compaction 决策；
- `compress_type`，**默认值 LZ4F**；
- `short_key_index_page`——前缀索引页；
- `primary_key_index_meta`——MoW 表的主键索引 + bloom filter + min/max key；
- `version`：`SEGMENT_FOOTER_VERSION_V2_BASELINE = 1`，`SEGMENT_FOOTER_VERSION_V3_EXT_COL_META = 2`（external catalog 表的列元信息）。

列元信息 `ColumnMetaPB` 里用于「跳过扫描」的三组字节计数是 `compressed_data_bytes` / `uncompressed_data_bytes` / `raw_data_bytes`；VARIANT 类型还会挂 `variant_statistics`（稀疏列与 doc_value 列的非空计数等）。

> **精确路径**：`be/src/storage/segment/segment.{h,cpp}`（段读写）、`be/src/storage/segment/column_meta_accessor.{h,cpp}`（页脚与列元信息的读取入口，取代了老版本的 `segment_footer.{h,cpp}`）。

### 24.2 Page 粒度、编码与压缩

Doris 的 data page **按编码后字节数切分**，不是按固定行数切分。默认值定义在 `be/src/storage/segment/options.h`：

```cpp
static constexpr size_t STORAGE_PAGE_SIZE_DEFAULT_VALUE = 65536;            // 列 data page 默认 64KB
static constexpr size_t STORAGE_DICT_PAGE_SIZE_DEFAULT_VALUE = 256 * 1024;  // 字典页默认 256KB
constexpr long ROW_STORE_PAGE_SIZE_DEFAULT_VALUE = 16384;                   // row-store（行存）page 16KB
```

> **纠偏**：线上文章常见的「page_size = 16KB」混淆了 row-store page 与列存 data page；列存场景应当用 64KB。

每个 page 尾部挂一个 `PageFooterPB`，`type` 取 `PageTypePB`：

| PageTypePB | 含义 | 附带 footer |
| --- | --- | --- |
| `DATA_PAGE` | 列数据页 | `DataPageFooterPB`：`first_ordinal` / `num_values` / `nullmap_size` |
| `INDEX_PAGE` | 列内部 BTree（primary key / ordinal）节点 | `IndexPageFooterPB`：`num_entries` / `type`(LEAF/INTERNAL) |
| `DICTIONARY_PAGE` | 字典编码的字典 | `DictPageFooterPB`：`encoding` |
| `SHORT_KEY_PAGE` | 前缀索引页 | `ShortKeyFooterPB`：`num_items` / `key_bytes` / `offset_bytes` / `num_rows_per_block` |
| `PRIMARY_KEY_INDEX_PAGE` | MoW 表主键索引页 | 同 `INDEX_PAGE` |

编码（`EncodingTypePB`）与压缩（`CompressionTypePB`）都是枚举，取值固定：

- **编码**：`PLAIN` / `PLAIN_ENCODING_V2`（varuint 长度前缀）/ `PLAIN_ENCODING_V3`（连续数据 + 连续长度 trailer）/ `PREFIX` / `RLE` / `DICT` / `BIT_SHUFFLE` / `FOR`（Frame-Of-Reference）；
- **压缩**：`NO_COMPRESSION` / `SNAPPY` / `LZ4` / `LZ4F` / `ZLIB` / `ZSTD` / `LZ4HC`。

**压缩粒度就是 page 粒度**：每个 page 独立压缩、读取时按页解压。于是 page 不能太大（单页解压延迟尖峰），也不能太小（压缩率与索引开销劣化）——这正是 64KB 这个默认值的由来。

### 24.3 两级 Zone Map（稀疏索引）

Doris 的「稀疏索引」在代码里的实体是 **Zone Map**，结构定义在 `ZoneMapIndexPB`：

```protobuf
message ZoneMapIndexPB {
    optional ZoneMapPB segment_zone_map = 1;              // 段级：整列一个
    optional IndexedColumnMetaPB page_zone_maps = 2;      // 页级：每个 data page 一个
}
```

`ZoneMapPB` 除 `min` / `max` 外还带 `has_null` / `has_not_null` / `pass_all` / `has_nan` / `has_positive_inf` / `has_negative_inf` 六个布尔位——它们让「全为 NULL」「含 NaN」「全通过」这三种边界情形无需真正比较 min/max 就能判定。

**页级 zone map 的写入时机就是 page 边界**。`be/src/storage/index/zone_map/zone_map_index.h` 中 `ZoneMapIndexWriter::flush()` 的注释写得很直白：

> mark the end of one data page so that we can finalize the corresponding zone map

同一文件顶部的注释进一步明确了存储形态：

> Zone map index is represented by an IndexedColumn with ordinal index. The IndexedColumn stores serialized ZoneMapPB for each data page. It also create and store the segment-level zone map in the index meta so that reader can prune an entire segment without reading pages.

即：**页级 zone map 序列化成 `ZoneMapPB` 后写进一棵带 ordinal index 的 BTree（`IndexedColumnMetaPB`）**，以 page ordinal 为 key；查询时先在 BTree 上二分定位，再按需读页。

> **纠偏**：网上常见的「每 65536 行记一次 min/max」是错的。65536 是**字节数**（`STORAGE_PAGE_SIZE_DEFAULT_VALUE`），一个 64KB 的 page 能装多少行取决于列宽与压缩比——窄列一页可装几十万行，宽列可能只有几百行。

### 24.4 稀疏索引查找伪代码

```text
# 在 Tablet 上执行 range_filter(ts BETWEEN a AND b)
# 输入: tablet -> [rowset -> [segment -> SegmentFooterPB -> columns[ts].indexes[ZONE_MAP_INDEX]]

scan_tablet(tablet, pred):
    for rs in tablet.rowsets():                          # ① Rowset 级
        if not rs.still_alive(): continue
        segs = rs.segments()
        cands = [s for s in segs if segment_zone_map_pass(s.columns[ts].segment_zone_map, pred)]
        #        ↑ 常数时间：只解页脚，不读任何数据页
        if cands.empty(): continue                      #    整 rowset 剪枝

        for seg in cands:                               # ② Segment 级
            zm = seg.columns[ts].segment_zone_map
            if not zm.pass_all and not overlap(zm.min, zm.max, [a, b]):
                continue                                #    段级剪枝，仍不读数据页

            zms = seg.columns[ts].page_zone_maps        # ③ Page 级
            lo, hi = 0, page_count(zms) - 1
            while lo <= hi:                             #    在 IndexedColumn 的 ordinal BTree 上二分
                mid = (lo + hi) // 2
                pzm = lookup(zms, mid)                  #    O(log P) 次比较，不读数据页
                if pzm.max < a:      lo = mid + 1
                elif pzm.min > b:    hi = mid - 1
                else:
                    emit(pzm); lo = mid + 1; hi = mid - 1   # 命中区间，向两侧扩展
            # ④ 只对 emit 出来的页做解压 + 反序列化 + 精确比较
```

**复杂度**（P 为页数，N_cand 为命中行数，P_cand 为命中页数）：

| 阶段 | 时间 | I/O |
| --- | --- | --- |
| ① Rowset 剪枝 | `O(R · S)` 次 zone map 比较 | 只读 `rowset_meta` |
| ② Segment 剪枝 | `O(S)` | 只读页脚（通常已在 page cache） |
| ③ Page 定位 | `O(P · log P)` | 0（`page_zone_maps` 整棵常驻） |
| ④ 精确过滤 | `O(N_cand)` 解压 + 比较 | `O(P_cand)` 页读 |

对比全表扫的 `O(N)` 解压，稀疏索引把代价压到 `O(N_cand + P_cand)`，收益随 `N_cand / N` 的下降而放大——这也就是 `ORDER BY` 键列顺序（KEY 列前缀参与前缀索引）对范围查询影响巨大的根因。

### 24.5 Short Key Index（前缀索引的物理载体）

Doris 的前缀索引在 segment 内就是一个独立的 `SHORT_KEY_PAGE`。`be/src/storage/index/short_key_index.h` 给出了 page body 的字节布局：

```
ShortKeyPageBody := KeyContent^NumEntry , KeyOffset(vint)^NumEntry
NumEntry / KeyBytes / OffsetBytes  存于 ShortKeyFooterPB
```

`ShortKeyIndexBuilder(segment_id, num_rows_per_block)` 每积累 `num_rows_per_block` 行就把当前 key 追加进去；查询时取前 K 个 key 二分定位到 block，再在 block 内逐行比较。因此 `num_rows_per_block`（由 `ShortKeyFooterPB.num_rows_per_block` 记录）是**前缀索引粒度与内存占用的直接旋钮**：调小 → 二分更快、索引更大；调大 → 反之。源码注释同时给出了该 builder 的使用边界：它**只接受已排序的 binary key**，且目前只用于 BetaRowset。

### 24.6 本节源码路径

| 内容 | 路径（当前 master） | 旧路径（旧版本文档中常见） |
| --- | --- | --- |
| segment v2 protobuf 定义 | `gensrc/proto/segment_v2.proto` | 同名，路径未变 |
| Page / 编码默认值 | `be/src/storage/segment/options.h` | `be/src/olap/rowset/segment_v2/column_meta.h` |
| Segment 读写 | `be/src/storage/segment/segment.{h,cpp}` | `be/src/olap/rowset/segment_v2/segment.{h,cpp}` |
| 页脚 / 列元信息读取 | `be/src/storage/segment/column_meta_accessor.{h,cpp}` | `be/src/olap/rowset/segment_v2/segment_footer.{h,cpp}` |
| 列页读取 | `be/src/storage/segment/column_reader.{h,cpp}` | 同名文件，目录已迁 |
| Zone Map 索引 | `be/src/storage/index/zone_map/zone_map_index.{h,cpp}` | `be/src/olap/rowset/segment_v2/zone_map_index.{h,cpp}` |
| 前缀索引 | `be/src/storage/index/short_key_index.{h,cpp}` | 同名文件，目录已迁 |
| Page Cache | `be/src/storage/cache/page_cache.{h,cpp}` | `be/src/olap/rowset/segment_v2/olap_page_cache.{h,cpp}` |

## 二十五、Compaction 调度：累计合并、全量合并与垂直合并

Tablet 的写入是「小批量高频」的，直接堆积会同时踩三个坑：读放大（查询要合并几十个 rowset）、写放大（版本链过长）、元数据膨胀。Doris 用三条互补的 compaction 流水线解决，调度逻辑全部落在 `be/src/storage/compaction/` 与 `be/src/storage/rowset/`。

### 25.1 三类 compaction 的分工

| 类型 | 实现 | 合并对象 | 目的 |
| --- | --- | --- | --- |
| **Cumulative Compaction**（累计合并） | `cumulative_compaction.{h,cpp}` + `cumulative_compaction_policy.{h,cpp}` | cumulative 层内相邻小 rowset | 消化高频小写入，控制读放大 |
| **Base Compaction**（全量合并） | `base_compaction.{h,cpp}` | base rowset + 全部 cumulative rowset | 回收删除标记、压缩存储、版本归并 |
| **Segment Compaction**（段合并） | `be/src/storage/rowset/segcompaction.{h,cpp}` | **同一个 rowset 内**的多个小 segment | 消除导入/批量写产生的小文件 |

三者是**流水线**关系：cumulative 把小 rowset 攒大 → 攒到阈值后交给 base；写入期则由 segcompaction 在 rowset 落盘前把碎段并掉。`compaction/` 下另有两个特化策略（时序表 `cumulative_compaction_time_series_policy`、binlog 表 `cumulative_compaction_binlog_policy`）和两个辅助任务（`full_compaction`、`cold_data_compaction`），由 `CumulativeCompactionPolicyFactory` 按表类型挑选。

### 25.2 Cumulative Compaction 的打分函数

Doris 不用「rowset 数量 > 阈值」这种粗规则，而是给每个 tablet 算一个 **compaction score**（`SizeBasedCumulativeCompactionPolicy::calc_cumulative_compaction_score`）。rowset 的单点分数来自 `RowsetMeta::get_compaction_score()`（`be/src/storage/rowset/rowset_meta.h`）：

```cpp
// 源码（删去 DCHECK/CHECK 与临时变量，逻辑未改）
uint32_t get_compaction_score() const {
    if (!is_segments_overlapping()) return 1;   // 段互不重叠（Append / Duplicate 模型）：恒为 1
    return num_segments();                       // 段重叠（Unique 模型）：等于段数
}
```

打分流程：

```text
calc_cumulative_compaction_score(tablet):
    point  = tablet.cumulative_layer_point()      # cumulative 层起点版本
    score  = 0;  total = 0
    for rs in tablet.all_rs_metas():
        if rs.end_version < point or not rs.is_local(): continue   # base 层不参与
        total += rs.total_disk_size
        score += rs.get_compaction_score()

    promotion = clamp(base.total_disk_size * compaction_promotion_ratio,   # 0.05
                       compaction_promotion_min_size_mbytes,                # 128 MB
                       compaction_promotion_size_mbytes)                   # 1024 MB
    if total >= promotion: return score           # ① 攒够了，全部参与

    # ② 没攒够：按 size 从大到小「砍掉」大文件，只留小文件参与合并（抑制抖动）
    for rs in sort(rowset_to_compact, by size desc):
        if level(rs.size) <= level(total - rs.size): return score
        total -= rs.size; score -= rs.get_compaction_score()
    return score
```

其中 `level(s)` 是 `SizeBasedCumulativeCompactionPolicy::_level_size`，即**向下取 2 的幂次**（带两级截断）：

```cpp
// 源码（bit_width(x) 即 sizeof(x)*8-1-__builtin_clzl(x)，逻辑未改）
int64_t _level_size(int64_t size) {
    if (size < 1024) return 0;                                  // <1KB 视为同一档
    int64_t max_level = 1 << (bit_width(_promotion_size / 2));  // 上界与 promotion_size 挂钩
    if (size >= max_level) return max_level;
    return 1 << (bit_width(size) - 1);                         // = 2^floor(log2(size))
}
```

这个 `2^floor(log2 s)` 分档是关键：score 只对「跨越了一个数量级」的大文件敏感，**同一数量级内的微小差异不刷分**，从而避免 score 忽高忽低导致的反复调度。选输入集时 `pick_input_rowsets` 还会用 `DEFER` 从尾部裁掉超 `max_compaction_score` 的 rowset，并保证**至少两个 rowset**——单个非重叠 rowset 无法自我合并。

### 25.3 Cumulative 层的晋升条件

合并完成后是否推进 `cumulative_layer_point`（把这个 rowset 交给 base 层），由 `update_cumulative_point` 判定，满足其一即可：

1. 输出 rowset 的 `total_disk_size >= tablet.cumulative_promotion_size()`；
2. 参与合并的 rowset 中存在 delete 版本 → 直接晋升；
3. **MoW 表专属**：输出 rowset 的 `end_version - start_version > compaction_promotion_version_count`（默认 1000）也强制晋升。源码注释写明了原因：delete bitmap 会随版本数膨胀，进而让 tablet meta 过大、`save_meta` 变慢。

### 25.4 Base Compaction 的三个触发条件

`be/src/storage/compaction/base_compaction.cpp` 中 `_input_rowsets` 按 end version 排序后，`_input_rowsets[0]` 即 base。三条满足其一即触发：

```text
_need_base_compaction(tablet):
    # 前置：score 上限截断——MoW 表更宽松
    limit = (MoW ? mow_base_compaction_max_compaction_score   # 200
                 : base_compaction_max_compaction_score)      # 20
    accumulate get_compaction_score() until score > limit; 截断 _input_rowsets

    # ① 累计 rowset 数超阈值
    if len(_input_rowsets) > base_compaction_min_rowset_num:      # 5
        return TRUE

    # ② 增量数据相对 base 的比例超阈值
    ratio = sum(data_disk_size of _input_rowsets[1:]) / _input_rowsets[0].data_disk_size
    if ratio > base_compaction_min_data_ratio:                    # 0.3
        return TRUE

    # ③ 距上次 base compaction 的时间间隔超阈值（兜底，防小表永远不合）
    if now() - _input_rowsets[0].creation_time
         > base_compaction_interval_seconds_since_last_operation: # 86400s
        return TRUE
    return FALSE
```

另有两处硬前置：候选 rowset 中若带 delete predicate 则直接拒绝（`BE_NO_SUITABLE_VERSION`），以及 `[0-1]` 为空段时拒绝（该场景无需 base compaction）。

### 25.5 垂直合并（Vertical Compaction）

传统 compaction 是**行式**的：读出整行、再写整行，宽表（几百列）时单线程的解码/编码成为瓶颈。Doris 的垂直合并把列切成若干 **column group**，每组独立成一个 segment 并行写，最后再把多份 segment 合回一个 rowset。核心模板在 `be/src/storage/rowset/vertical_beta_rowset_writer.h`：

```cpp
// 源码节选（删去 override 标记与 private 分区，逻辑未改）
template <class T> requires std::is_base_of_v<BaseBetaRowsetWriter, T>
class VerticalBetaRowsetWriter : public T {
    Status add_columns(const Block* block, const std::vector<uint32_t>& col_ids,
                       bool is_key, uint32_t max_rows_per_segment, bool has_cluster_key);
    Status flush_columns(bool is_key);       // 关掉当前列组 writer，切下一组
    Status final_flush();                    // 收尾
    Status build(RowsetSharedPtr& rowset);   // 多段合并成最终 rowset
    std::vector<std::unique_ptr<VerticalSegmentWriter>> _segment_writers;
};
```

列组大小由 `vertical_compaction_num_columns_per_group` 控制（默认 **5** 列/组），单段目标大小由 `vertical_compaction_max_segment_size` 控制（默认 **1 GiB**）。收益模型很直白：

```
T_row  = D + W                       # 行式：解码全列 + 编码全列
T_vert = max_g (D_g + W_g)           # 列组并行，组间无依赖
```

当写放大 W 占主导且列组数足够并行时，`T_vert ≈ T_row / G`（G 为列组数）。代价是同一行被拆进 G 个文件，读路径需要 G 路读 + 行拼接，因此只在「写重读轻」的导入/合并场景启用（`enable_vertical_compaction` 默认 true）。

### 25.6 调度主循环伪代码

```text
# BE 后台 compaction 调度（建立在 be/src/storage/compaction/compaction.h 的 CompactionMixin 之上）
loop every scheduling_tick():
    for tablet in storage_engine.tablets():
        for task_type in [CUMULATIVE, BASE]:
            if not permit_limiter.acquire(task_type): continue      # 全局并发闸门
            if task_type == CUMULATIVE:
                score = policy.calc_cumulative_compaction_score(tablet)
                if score <= 0: continue
                policy.pick_input_rowsets(tablet, candidates,
                                          max_score, min_score,
                                          &inputs, &last_delete_ver, &out_score)
                # pick 内部再按 level_size 对折 + DEFER 从尾部裁掉超 score 的 rowset，
                # 且保证至少 2 个 rowset（单个非重叠 rowset 无法自我合并）
            else:
                inputs = base_compaction._filter_input_rowset(tablet)
                if inputs empty or not _need_base_compaction(tablet): continue
            submit_to_thread_pool(task_type, tablet, inputs)        # max_cumu / max_base
```

### 25.7 相关配置项（默认值已逐条核对 `be/src/common/config.cpp`）

| 配置项 | 默认值 | 作用 |
| --- | --- | --- |
| `enable_compaction_checksum` | `false` | 合并时校验 checksum |
| `disable_auto_compaction` | `false` | 关掉自动 compaction（仅手工触发） |
| `compaction_promotion_ratio` | `0.05` | 晋升阈值 = base 大小 × 该比例 |
| `compaction_promotion_min_size_mbytes` | `128` | 晋升阈值下限 |
| `compaction_promotion_size_mbytes` | `1024` | 晋升阈值上限 |
| `compaction_promotion_version_count` | `1000` | MoW 表按版本数强制晋升 |
| `compaction_min_size_mbytes` | `64` | 参与合并的最小 rowset |
| `base_compaction_min_rowset_num` | `5` | base 触发条件① |
| `base_compaction_min_data_ratio` | `0.3` | base 触发条件② |
| `base_compaction_interval_seconds_since_last_operation` | `86400` | base 触发条件③ |
| `base_compaction_max_compaction_score` | `20` | 单次 base 合并 score 上限 |
| `mow_base_compaction_max_compaction_score` | `200` | MoW 表的放宽上限 |
| `enable_vertical_compaction` | `true` | 垂直合并总开关 |
| `vertical_compaction_num_columns_per_group` | `5` | 列组的列数 |
| `vertical_compaction_max_segment_size` | `1073741824`（1 GiB） | 单段目标大小 |
| `max_cumu_compaction_threads` | `-1`（自动） | 累计合并线程数 |
| `max_base_compaction_threads` | `4` | 全量合并线程数 |
| `enable_segcompaction` | `true` | 段合并开关 |
| `segcompaction_batch_size` | `10` | 单批最多合并的 segment 数 |
| `segcompaction_candidate_max_rows` | `1048576` | 候选 segment 行数上限 |
| `segcompaction_candidate_max_bytes` | `104857600` | 候选 segment 字节上限 |
| `segcompaction_task_max_rows` | `1572864` | 单任务行数上限 |
| `segcompaction_task_max_bytes` | `157286400` | 单任务字节上限 |
| `segcompaction_num_threads` | `5` | 段合并线程池大小 |

> **纠偏**：老版本文档里的 `max_compaction_num`、`base_compaction_max_concurrency` 在当前代码中**已不存在**，对应的并发控制已改为 `max_cumu_compaction_threads` / `max_base_compaction_threads` 两个线程池参数。

### 25.8 本节源码路径

| 内容 | 路径（当前 master） |
| --- | --- |
| compaction 抽象基类与通用工具 | `be/src/storage/compaction/compaction.{h,cpp}` |
| 累计合并 + 策略 | `be/src/storage/compaction/cumulative_compaction.{h,cpp}`、`cumulative_compaction_policy.{h,cpp}` |
| 时序表 / binlog 表策略 | `be/src/storage/compaction/cumulative_compaction_time_series_policy.{h,cpp}`、`cumulative_compaction_binlog_policy.{h,cpp}` |
| 全量合并 | `be/src/storage/compaction/base_compaction.{h,cpp}` |
| 段合并 | `be/src/storage/rowset/segcompaction.{h,cpp}` |
| 垂直合并 | `be/src/storage/rowset/vertical_beta_rowset_writer.{h,cpp}`、`be/src/storage/segment/vertical_segment_writer.{h,cpp}` |
| Rowset 元信息 | `be/src/storage/rowset/rowset_meta.{h,cpp}` |
| 并发闸门 | `be/src/storage/compaction/compaction_permit_limiter.{h,cpp}` |
| 配置项声明 / 默认值 | `be/src/common/config.h` / `be/src/common/config.cpp` |

## 二十六、索引族实现、物化视图改写与 CBO

§7.4 / §8 已经把索引类型「是什么」列成了一张表，本节不重复，只讲**怎么实现、怎么查、怎么被优化器用上**。

### 26.1 二级索引的元数据骨架

段内所有二级索引共用同一套元数据结构 `ColumnIndexMetaPB`，由 `type` 区分：

| ColumnIndexTypePB | 含义 | 用途 |
| --- | --- | --- |
| `ZONE_MAP_INDEX` | min/max | 范围剪枝（§24.3） |
| `ORDINAL_INDEX` | page ordinal → 物理 offset | 定位物理页 |
| `BLOOM_FILTER_INDEX` | Bloom Filter | 等值剪枝 |
| `NESTED_OFFSETS_INDEX` | block 级 offsets | 嵌套类型（ARRAY / STRUCT）下钻 |
| `BITMAP_INDEX` | Roaring Bitmap | 源码已标注 **deprecated** |

Bloom Filter 的参数写在 `BloomFilterIndexPB`：哈希策略 `HashStrategyPB` ∈ {`HASH_MURMUR3_X64_64`, `CITY_HASH_64`, `XX_HASH_64`}，算法 `BloomFilterAlgorithmPB` ∈ {`BLOCK_BLOOM_FILTER`, `CLASSIC_BLOOM_FILTER`, `NGRAM_BLOOM_FILTER`}——最后一个即中文全文检索用的 N-gram Bloom Filter，其分词由倒排索引的 token 抽取器负责（`be/src/storage/itoken_extractor.{h,cpp}`）。

Bloom Filter 的判据是标准形式：

```text
f(key) ∈ [0, k)            # k 个哈希函数
bits[f(key) % m] = 1       # k 个 bit 全为 1 → "可能存在"，需回表精确过滤
P_fp ≈ (1 - e^(-kn/m))^k   # 最优 k = (m/n)·ln2
```

Doris 侧的实现位于 `be/src/storage/index/bloom_filter/`，与 zone map / ordinal index 同属 `IndexFileReader` / `IndexFileWriter` 抽象之下（`be/src/storage/index/index_file_{reader,writer}.{h,cpp}`）。

### 26.2 倒排索引：两套引擎、三种存储格式

这是 Doris 4.x 变化最大的索引子系统。`be/src/storage/index/inverted/` 下并列着两套引擎：

- **CLucene 引擎**（`inverted/clucene/`）：基于 Apache Lucene C++ 的分词与倒排表，`inverted_index_writer.cpp` / `inverted_index_searcher.cpp` / `inverted_index_compaction.cpp`；
- **SNII 引擎**（`inverted/snii/`）：自研 BKD 树路线（`snii_bkd_index_writer.cpp` / `snii_bkd_query.cpp`），面向数值/高精度列的等值与范围查询。

存储格式由 protobuf 枚举 `InvertedIndexStorageFormatPB` 决定，`IndexFileWriter` 据此选实现：

| 格式 | 选择条件 | 文件布局 |
| --- | --- | --- |
| `V1` | `_storage_format == V1` | 每列一个索引文件 |
| `V2` | `!= V1 && != SNII` | 每 segment 一个索引文件（收纳全部索引） |
| `SNII` | `== SNII` | 走 SNII 引擎的独立写入路径 |

文件名的精确规则在 `be/src/storage/index/inverted/common/inverted_index_desc.cpp`：

```cpp
// 本地存储路径前缀：
//   {storage_dir}/data/{shard_id}/{tablet_id}/{schema_hash}/{rowset_id}_{seg_id}
// 存算分离（云端）路径前缀：
//   data/{tablet_id}/{rowset_id}_{seg_id}

// V1：{rowset_id}_{seg_id}_{index_id}[@{suffix}].idx   —— index_id 即列的 unique_id，一个列一个文件
// 源码（仅折行，逻辑未改；index_suffix = ".idx"）：
std::string InvertedIndexDescriptor::get_index_file_name_v1(const std::string& rowset_id,
                                                            int64_t seg_id, int64_t index_id,
                                                            std::string_view index_path_suffix) {
    std::string suffix =
            index_path_suffix.empty() ? "" : std::string {"@"} + index_path_suffix.data();
    return fmt::format("{}_{}_{}{}{}", rowset_id, seg_id, index_id, suffix, index_suffix);
}

// V2：{rowset_id}_{seg_id}.idx                         —— 一个 segment 只有一个 .idx
std::string InvertedIndexDescriptor::get_index_file_name_v2(const std::string& rowset_id,
                                                            int64_t seg_id) {
    return fmt::format("{}_{}{}", rowset_id, seg_id, index_suffix);
}
```

注意 segment 数据文件后缀是 `.dat`（`segment_suffix`），索引文件后缀是 `.idx`（`index_suffix`）；另有若干构建期临时文件 `null_bitmap` / `bkd` / `bkd_meta` / `bkd_index`。

**V1 → V2 的意义**：V1 下 compaction 每合并一个 segment 就要重写 N 个索引文件；V2 收敛成 1 个文件，元数据项与打开的搜索器数都降到 `O(1)` 量级。另外 `be/src/storage/segment/segment_index_file_cache_loader.h` 定义了 `SegmentIndexFileCacheLoadReason` ∈ {`LOAD`, `CUMULATIVE_COMPACTION`, `BASE_COMPACTION`, `SCHEMA_CHANGE`}，让索引文件区段（`SegmentIndexFileCacheRange`）能被显式预热进 file cache——与 §24.4「页脚常驻、页数据按需」是同一套缓存分层思路。

> **边界说明**：社区文档中常见的「global inverted index / local inverted index」是一组**存储布局术语**，在当前源码中并不对应独立的类或配置项，本文不据其展开；可确证的是上述 `InvertedIndexStorageFormatPB` 三种格式与 `.idx` 命名规则。

### 26.3 物化视图：同步 rollup vs 异步 MV

Doris 的物化视图分两类，**一致性模型完全不同**：

| | **同步物化视图（rollup）** | **异步物化视图（AMV）** |
| --- | --- | --- |
| 作用域 | **单表内**，只能基于同一张基表的部分列 + 聚合 | **跨表、跨库**，可基于 JOIN 之后的结果 |
| 维护时机 | 导入时同步更新，**强一致** | 独立调度刷新，**最终一致** |
| 查询 | 优化器按需改写到 rollup | 优化器做模式匹配后改写 |
| 存储 | 同 tablet 内，多一份列存数据 | 独立的 MV 表，走自己的生命周期 |

异步 MV 的查询改写由 Nereids 的 exploration 规则族完成，全部落在 `fe/fe-core/src/main/java/org/apache/doris/nereids/rules/exploration/mv/`。该目录按「MV 侧算子形状 × 查询侧算子形状」做**叉乘枚举**，仅聚合与 JOIN 两类就有三十余条规则，例如：

```text
MaterializedViewAggregateRule                  # 纯聚合匹配
MaterializedViewFilterAggregateRule            # 过滤下推后聚合
MaterializedViewProjectAggregateRule           # 投影裁剪后聚合
MaterializedViewLimitAggregateRule             # LIMIT/TOPN + 聚合
MaterializedViewFilterJoinRule                 # 过滤 + JOIN
MaterializedViewProjectJoinRule                # 投影 + JOIN
MaterializedViewOnlyScanRule                   # 仅扫描列裁剪
MaterializedViewWindowRule                     # 窗口函数
MaterializedViewAggregateOnNoneAggregateRule   # 基表无聚合 → MV 有聚合
```

配套的匹配上下文是 `MaterializationContext` / `AsyncMaterializationContext`，其中 `HyperGraphComparator` 负责判断「MV 的 JOIN 超图」与「查询的 JOIN 超图」是否同构——这正是异步 MV 能支撑 TPC-DS 这类多表 JOIN 查询的关键，也是它与早期「仅单表 rollup 匹配」的根本分界。

**改写判定的简化伪代码**：

```text
try_rewrite(query_plan, mv_list):
    for mv in mv_list:
        # ① 同构判定：JOIN 超图 + 列对应关系
        cmp = HyperGraphComparator.compare(query_plan.join_graph, mv.join_graph)
        if not cmp.isomorphic: continue

        # ② 列匹配：把查询列映射到 MV 列
        slot_map = EquivalenceClass.map_slots(query_plan, mv, cmp.correspondence)
        if slot_map is None: continue

        # ③ 谓词匹配：查不上的过滤条件必须能全部下推到 MV 侧
        residual = MaterializedViewPredicateCollector.collect_residual(query_plan, mv, slot_map)
        if not residual.is_self_contained: continue     # 残留谓词仍引用查表 → 放弃改写

        # ④ 收益判定：改写后不能比原计划更贵
        if cost(rewrite_with_mv) < cost(query_plan): yield rewrite_with_mv
```

第 ④ 步依赖下一节的 cost model。同步 rollup 的元信息由 `fe/fe-core/src/main/java/org/apache/doris/alter/MaterializedViewHandler.java` 管理。

### 26.4 CBO：Nereids 的 Memo / Cost / Stats 三件套

> **路径纠偏**：Doris 4.x 中 **`nereids/optimizer/` 目录已不存在**，优化器主循环迁到了 `fe/fe-core/src/main/java/org/apache/doris/nereids/jobs/executor/Optimizer.java`，规则与数据结构平铺在 `nereids/` 的各子包下。

| 环节 | 包 / 类 | 职责 |
| --- | --- | --- |
| 优化主循环 | `nereids/jobs/executor/Optimizer.java` | 依次跑 rewrite → 列裁剪 → join reorder → implementation |
| Memo / Cascades | `nereids/memo/` | 逻辑计划的 Group，join 枚举在 group 树上展开 |
| 代价模型 | `nereids/cost/CostModel.java`、`CostCalculator.java`、`CostWeight.java` | 给每个物理算子算行数 + IO + CPU + 内存，加权成可比标量 |
| 统计推导 | `nereids/stats/StatsCalculator.java`、`FilterEstimation.java`、`JoinEstimation.java` | 由基表统计信息沿算子树推导中间结果行数 |
| 基数估计工具 | `nereids/stats/StatsMathUtil.java` | 均匀分布假设 + 最小/最大基数钳制（clamp） |
| 统计信息采集 | `statistics/analysis/HistogramTask.java`、`AnalysisJob.java` | `ANALYZE TABLE` 的执行体，产出直方图 |
| 历史优化（HBO） | `nereids/stats/HboPlanInfoProvider.java`、`statistics/hbo/` | 用历史 plan 反馈真实基数，反哺下一次选型 |
| JOIN 重排规则 | `nereids/rules/exploration/join/` | `JoinCommute`、`InnerJoinLeftAssociateProject`、`OuterJoinAssocProject`、`SemiJoinSemiJoinTransposeProject` 等交换律/结合律变换 |
| 谓词下推规则 | `nereids/rules/rewrite/PushDownFilterThrough{Aggregation,Generate,Join,PartitionTopN,Project,Repeat,SetOperation,Sort,Window}.java` | 覆盖 9 种算子形态的下推 |

代价模型最终比较的是加权标量：

```text
Cost(P) = w_row·|rows(P)| + w_io·|io_bytes(P)| + w_cpu·|cpu_ns(P)| + w_mem·|peak_mem(P)|
P*      = argmin over { P | P 是 Memo group 内任一物理实现 }
```

**Join 算法选择**就落在这个 argmin 里。BE 侧的落地件为：`hashjoin_build_sink.{h,cpp}`（build 端，构造 hash table）、`hashjoin_probe_operator.{h,cpp}`（probe 端）、`be/src/exec/operator/join/*_join_impl.cpp`（inner / left_outer / full_outer / left_semi / left_anti / null_aware_* 六种连接类型的 probe 逻辑）、`be/src/exec/common/hash_table/join_hash_table.h`（hash 表本体）。Broadcast 与 Shuffle 的差别，本质就是 build 端的数据要不要过 `be/src/exec/exchange/`（`vdata_stream_sender.cpp` / `vdata_stream_recvr.cpp`）——不过网即 broadcast，过网即 shuffle；「哪个表当 build 端」由 cost model 依据统计信息决定。

### 26.5 运行时过滤器：把 join 的选择性反馈给存储层

统计信息再准也有误差，Doris 因此在执行期做二次修正：**runtime filter** 在 build 端扫描时实时收集对 probe 端的过滤条件，经 `be/src/exec/runtime_filter/` 广播回各 scan 节点，直接转化为 segment 级别的跳过：

| 类 | 作用 |
| --- | --- |
| `runtime_filter_producer.cpp` | build 端生产 filter（min/max、IN、bitmap） |
| `runtime_filter_consumer.cpp` | probe 端消费并合并 |
| `runtime_filter_mgr.cpp` | filter 的注册、分发与生命周期 |
| `runtime_filter_merger.h` | 多路 filter 合并（取交） |
| `runtime_filter_bucket_pruner.cpp` | 用 filter 剪 bucket |
| `runtime_filter_partition_pruner.cpp` | 用 filter 剪分区 |

于是 `SELECT ... FROM big JOIN small ON big.k = small.k` 的执行代价从「读全量 big」降到「读 big 中 k 落在 small 值域内的部分」——与 §24.4 的稀疏索引剪枝是同一条链路上的两级优化。

### 26.6 本节源码路径

| 内容 | 路径（当前 master） |
| --- | --- |
| 索引通用读写 | `be/src/storage/index/index_file_{reader,writer}.{h,cpp}`、`be/src/storage/index/index_common.h` |
| Bloom Filter | `be/src/storage/index/bloom_filter/` |
| 倒排索引（入口） | `be/src/storage/index/inverted/` |
| 倒排 CLucene 引擎 | `be/src/storage/index/inverted/clucene/{inverted_index_writer,inverted_index_searcher,inverted_index_compaction}.cpp` |
| 倒排 SNII 引擎 | `be/src/storage/index/inverted/snii/`（含 `bkd/` 子目录） |
| 倒排索引文件命名 | `be/src/storage/index/inverted/common/inverted_index_desc.{h,cpp}` |
| 倒排存储格式 | `be/src/storage/index/index_storage_format_v{1,2}.{h,cpp}`、`gensrc/proto/snii.proto` |
| 倒排索引缓存 | `be/src/storage/index/inverted/common/inverted_index_cache.{h,cpp}`、`be/src/storage/segment/segment_index_file_cache_loader.{h,cpp}` |
| 前缀索引 | `be/src/storage/index/short_key_index.{h,cpp}` |
| FE 索引元数据 | `fe/fe-core/src/main/java/org/apache/doris/catalog/TabletInvertedIndex.java`、`catalog/LocalTabletInvertedIndex.java` |
| FE 物化视图 | `fe/fe-core/src/main/java/org/apache/doris/alter/MaterializedViewHandler.java` |
| FE MV 改写规则 | `fe/fe-core/src/main/java/org/apache/doris/nereids/rules/exploration/mv/` |
| FE 优化器主循环 | `fe/fe-core/src/main/java/org/apache/doris/nereids/jobs/executor/Optimizer.java` |
| FE 代价模型 | `fe/fe-core/src/main/java/org/apache/doris/nereids/cost/` |
| FE 统计信息 | `fe/fe-core/src/main/java/org/apache/doris/nereids/stats/`、`org/apache/doris/statistics/analysis/` |
| BE Join 算子 | `be/src/exec/operator/hashjoin_{build_sink,probe_operator}.{h,cpp}`、`be/src/exec/operator/join/` |
| BE Scan 算子 | `be/src/exec/scan/olap_scanner.{h,cpp}` |
| BE Runtime Filter | `be/src/exec/runtime_filter/` |

## 二十七、参考资料

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
