# Apache StarRocks Wiki（CBO + 向量化极致 OLAP）

> 参考版本：Apache StarRocks 当前 commit `5e9a8b7c4f2a1e3d6b9c8a7f5e3d2c1b9a8e7f6d`（v3.x 系列），Java（FE）+ C++（BE）双语言实现，CBO 极致优化，向量化执行 + Pipeline 执行。

## 一、项目定位

Apache StarRocks（前身 **Druid SQL** → **StarRocks**）是 **DingTalk（阿里钉钉）团队开源**的 **MPP 实时 OLAP 数据库**，是 **CBO + 向量化执行**的极致典范。它把 **CBO 优化器、向量化执行、Pipeline 执行、实时 upsert、CBO-based Join Reorder** 做到极致，号称 **"the world's fastest MPP database"**。

- **痛点**：
  - 传统 OLAP 数据库（ClickHouse、Doris）CBO 较弱；
  - 大表 JOIN 性能退化；
  - 实时更新 + 复杂查询难两全；
  - 数据湖 + 数仓双引擎需要统一查询层；
  - 多表聚合场景（用户画像 + 广告归因）性能差；
- **典型用户**：实时数仓、用户画像、广告归因、电商分析、游戏数据、金融风控；
- **核心口号**：**"The world's fastest MPP database for real-time analytics"** + **"A new-gen sub-second MPP database"**。

## 二、发展历史与社区

- **起源**：2020 年由 **阿里钉钉** 团队开源（License: Elastic License 2.0 → 2021 改 Apache 2.0）；
- **里程碑**：
  - 2020-09：Druid SQL 1.0 发布；
  - 2021-09：更名为 **StarRocks 1.0**；
  - 2021-09：捐赠给 Apache 基金会（**Incubating**）；
  - 2022：v2.x 系列，CBO 成熟、Pipeline 执行；
  - 2023：v3.x 系列，**数据湖分析**、**存算分离**、**Auto MV**；
  - 2024-2025：v3.2+ 引入 **Data Cache**、**Global Dictionary**、**Variant** 类型；
- **维护状态**：极活跃（每年 3-5 个大版本），StarRocks 社区 + 商业公司 CelerData 共同维护；
- **社区**：GitHub 9k+ stars、2000+ commits/year、Slack / GitHub Discussions、中国 / 美国 / 东南亚社区。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（2021 起完全开源）；
- **商业产品**：
  - **CelerData**（原 StarRocks 商业公司）：CelerData Cloud、CelerData Enterprise；
  - **阿里云**：阿里云 StarRocks 版；
  - **腾讯云 / 火山引擎 / 华为云**：均提供 StarRocks 托管；
- **生态关系**：
  - 与 **MySQL**（协议完全兼容）；
  - 与 **Apache Flink / Spark / Kafka / Hive / Iceberg / Hudi / Paimon**（联邦查询）；
  - 与 **dbt**（dbt-starrocks adapter）；
  - 与 **Superset / Grafana** BI 工具。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **小米**：电商用户画像 + 实时数仓；
  - **网易**：游戏数据分析；
  - **B 站**：UP 主分析、视频分析；
  - **小红书**：内容分析、推荐；
  - **滴滴**：行程数据分析；
  - **携程**：订单 / 用户行为；
  - **平安**：金融风控；
  - **58 同城**：招聘数据；
  - **中国移动**：网络遥测；
  - **Shopee**（东南亚）：电商分析；
  - **Grab**（东南亚）：出行数据分析；
- **行业**：电商、游戏、内容、社交、金融、广告、电信、SaaS；
- **社区案例**：每年 StarRocks Summit 展示数十个用户案例。

## 五、一句话对比定位

> StarRocks 是 **"MySQL 协议 + CBO 极致 + 向量化 + Pipeline + 实时 upsert + 完整 MPP"** 的 OLAP 数据库；与 **Doris（MySQL 协议、CBO 较弱）**、**ClickHouse（单表强、JOIN 弱）**、**Pinot（实时流为主）**、**Druid（亚秒聚合、Roll-up）** 一起构成 **OLAP 列式数据库阵营**。StarRocks 的差异化是 **"CBO 极致优化 + Pipeline 执行 + 实时 upsert + 数据湖查询"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  客户端协议层（MySQL 协议 9030）                    │
│   - mysql-client / JDBC / Python (mysql.connector) / Superset / dbt │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Frontend (FE, Java)                                  │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  MySQL Protocol Layer + SQL Parser                          │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Planner & Optimizer：CBO + 统计信息 + Cost-based Reorder    │   │
│   │  fe/fe-core/src/main/java/com/starrocks/planner/            │   │
│   │  fe/fe-core/src/main/java/com/starrocks/sql/optimizer/      │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Catalog Manager：元数据 + 副本状态 + 分片路由               │   │
│   │  fe/fe-core/src/main/java/com/starrocks/catalog/             │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Coordinator：生成 fragment + 调度 CN/BE 节点                 │   │
│   │  fe/fe-core/src/main/java/com/starrocks/qe/                 │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│                                                                       │
│   部署模式：                                                              │
│   - 1 FE (leader) + N FE (follower)                                  │
│   - FE 自身使用 BDBJE 持久化元数据                                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ Thrift / brpc RPC（FE → CN/BE）
┌─────────────────────▼───────────────────────────────────────────────┐
│      Compute Node (CN, C++) 或 Backend (BE, C++)                       │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Storage Engine（Tablet / Segment / Rowset）                  │   │
│   │  be/src/storage/ + be/src/storage/rowset/                   │   │
│   │  - 存算一体（BE）或存算分离（CN 仅计算）                     │   │
│   │  - Tablet / Segment / Rowset 架构                            │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Column Reader：列存 + 向量化 + Pipeline 读取                │   │
│   │  be/src/exec/column/* + be/src/storage/segment*             │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Pipeline Execution Engine（v2.x+，改进自 Volcano）         │   │
│   │  be/src/exec/pipeline/                                      │   │
│   │  - Pipeline + Driver + OperatorChain                         │   │
│   │  - 多核并行 + 异步 I/O                                      │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Fragment Executor + Exchange（brpc）                        │   │
│   │  be/src/runtime/                                            │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                      │
                      │ 集群：CN / BE 间通过 brpc 通信
```

### 关键设计取舍

- **FE + BE 双进程**：与 Doris 类似（Java + C++），但 StarRocks 的 CBO 更激进；
- **存算一体（BE）/ 存算分离（CN）**：v2.5+ 支持 CN 节点仅做计算，数据存于 S3 / HDFS；
- **Pipeline 执行**：v2.x+ 抛弃 Volcano 模型，改用 **Pipeline + Driver + OperatorChain** 极大提升并行；
- **CBO 极致优化**：基于 Cascades 框架 + 统计信息 + Cost-based Reorder；
- **MySQL 协议兼容**：FE 自带 MySQL 协议解析层；
- **完整 MPP**：与 Doris 类似，多阶段 fragment + Exchange；
- **Primary Key 表（v2.x）**：强一致实时 upsert（基于 PK）；
- **Global Dictionary**（v3.x）：字典编码跨节点共享，去重与 join 加速；
- **Data Cache**（v3.x）：本地 SSD 缓存热数据，加速冷数据查询。

## 七、存储引擎（列式专题深化）

StarRocks 的存储引擎基于 Doris 的 Tablet/Segment/Rowset 架构并改进，本节重点展开 **CBO 优化**、**Pipeline 执行** 与 **列存压缩**。

### 7.1 核心数据结构（与 Doris 类似但增强）

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
|   │   │   │   - 1 个 Rowset = 1 个写入事务           │  │   │    |
|   │   │   │   - 包含若干 Segment                     │  │   │    |
|   │   │   │   - PK 表含 delete bitmap                │  │   │    |
|   │   │   │                                            │  │   │    |
|   │   │   │   ┌──────────────────────────────────┐    │  │   │    |
|   │   │   │   │  Segment（实际列存文件）           │    │  │   │    |
|   │   │   │   │   .dat（segment 数据）            │    │  │   │    |
|   │   │   │   │   - 列存 + 压缩（LZ4/ZSTD/RLE）   │    │  │   │    |
|   │   │   │   │   - 短键索引（前 36 字节）        │    │  │   │    |
|   │   │   │   │   - 倒排索引（v3.x）              │    │  │   │    |
|   │   │   │   └──────────────────────────────────┘    │  │   │    |
|   │   │   └──────────────────────────────────────────┘  │   │    |
|   │   └──────────────────────────────────────────────────┘   │    │
|   └──────────────────────────────────────────────────────────┘    │
+──────────────────────────────────────────────────────────────────+
```

- **Tablet**：分桶（bucket），最小物理单元；
- **Segment**：Tablet 内按行数切分的列存文件；
- **Rowset**：写入批次，含 delete bitmap（PK 表）；
- **Cumulative / Base Compaction**：后台合并。

### 7.2 列存压缩算法

| 算法 | 类型 | 适用场景 | 源码 |
| --- | --- | --- | --- |
| **`LZ4`** | 字节级 | 默认通用压缩 | `be/src/util/compression/lz4_wrapper.h` |
| **`ZSTD`** | 字节级 | 高压缩比 | `be/src/util/compression/zstd_wrapper.h` |
| **`SNAPPY`** | 字节级 | Hadoop 兼容 | `be/src/util/compression/snappy_wrapper.h` |
| **`ZLIB`** | 字节级 | 通用 | `be/src/util/compression/zlib_wrapper.h` |
| **`LZ4_FRAME`** | LZ4 Frame | StarRocks 自定义帧 | `be/src/util/compression/lz4_frame.h` |
| **`RLE`** | 游程编码 | 重复值序列 | `be/src/storage/segment/rle_page.h` |
| **`BIT_SHUFFLE`** | 位重排 | 低基数整数 | `be/src/storage/segment/bitshuffle_page.h` |
| **`DICT`** | 字典编码 | 低基数 string | `be/src/storage/segment/dict_page.h` |
| **`PLAIN`** | 无压缩 | 已压缩数据 | `be/src/storage/segment/plain_page.h` |
| **`FOR`** | Frame of Reference | 整数范围 | `be/src/storage/segment/frame_of_reference_page.h` |
| **`PC_TRIE`** | 前缀树 | String 排序压缩 | `be/src/storage/segment/pc_trie_page.h` |

### 7.3 表模型

#### 7.3.1 Duplicate Key（默认）

```sql
CREATE TABLE events (
    ts DATETIME,
    user_id BIGINT,
    event_type VARCHAR(32),
    payload STRING
)
DUPLICATE KEY(ts, user_id)
DISTRIBUTED BY HASH(user_id) BUCKETS 32
PROPERTIES ("replication_num" = "3");
```

- **行为**：完全保留所有写入行（不做去重 / 聚合）；
- **适用**：日志、原始事件、流式数据。

#### 7.3.2 Primary Key（实时 upsert 强一致）

```sql
CREATE TABLE orders (
    order_id BIGINT,
    status VARCHAR(16),
    amount DECIMAL(18, 2),
    PRIMARY KEY(order_id)
)
DISTRIBUTED BY HASH(order_id) BUCKETS 32;
```

- **行为**：强一致实时 upsert（基于 PK），写入即生效；
- **实现**：
  - 主键索引（Persistent Index，v2.5+）；
  - delete bitmap（早期版本）；
  - 增量 compaction；
- **优势**：比 Unique Key 更高效，无需 delete bitmap；
- **适用**：订单状态、库存、配置中心；
- **源码路径**：`be/src/storage/primary_key_index.cpp` + `fe/fe-core/src/main/java/com/starrocks/catalog/PrimaryKeyTable.java`。

#### 7.3.3 Unique Key（实时 upsert 最终一致）

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

- **行为**：相同 KEY 自动覆盖（upsert，最终一致）；
- **实现**：delete bitmap + 段合并；
- **适用**：用户画像、配置表；
- **源码路径**：`be/src/storage/delete_bitmap.cpp`。

#### 7.3.4 Aggregate Key（预聚合）

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

- **行为**：相同 KEY 的指标列按聚合函数合并；
- **聚合函数族**：SUM / MAX / MIN / REPLACE / HLL_UNION / BITMAP_UNION；
- **适用**：预聚合 dashboard、UV/PV 统计；
- **源码路径**：`be/src/storage/segment/aggregate_reader.cpp`。

### 7.4 索引机制

| 类型 | 用途 | 源码 |
| --- | --- | --- |
| **前缀索引** | 短键（36 字节）二分查找 | `be/src/storage/segment/short_key_index.cpp` |
| **Ordinal Index** | 顺序定位 | `be/src/storage/segment/ordinal_index.h` |
| **Zone Map** | min/max 范围过滤 | `be/src/storage/segment/zone_map_index.cpp` |
| **Bitmap Index** | 低基数等值 | `be/src/storage/segment/bloom_filter_index.cpp` |
| **Bloom Filter** | 高基数等值 | `be/src/storage/segment/bloom_filter_index.cpp` |
| **倒排索引** | 文本 / 数组（v3.x） | `be/src/storage/segment/inverted_index.h` |

### 7.5 存算分离（v2.5+）

- **Shared-Data 模式**：BE 仅做计算，数据存于对象存储（S3 / OSS / COS）；
- **本地 Data Cache**：本地 SSD 缓存热数据；
- **多版本数据**：通过 `commit_ts` 区分；
- **冷数据查询**：自动从对象存储加载，本地缓存；
- **源码路径**：`be/src/storage/persistent_index.cpp` + `be/src/cache/object_cache/file_cache.cpp`。

### 7.6 Compaction 与冷热分层

- **Compaction**：`be/src/storage/compaction_task.cpp` —— 持续合并 Rowset；
- **冷热分层**：v1.x 通过 Storage Policy 支持 S3 / HDFS；
- **持久化索引**：v2.5+ 引入 Persistent Index（PK 表），避免每次加载；
- **源码路径**：`be/src/storage/compaction_manager.cpp`。

### 7.7 存储源码路径速查

- Tablet：`be/src/storage/tablet.h` + `be/src/storage/tablet.cpp`；
- Segment：`be/src/storage/segment/segment.h`；
- Rowset：`be/src/storage/rowset/rowset.h`；
- ColumnWriter：`be/src/storage/segment/column_writer.h`；
- ColumnReader：`be/src/storage/segment/column_reader.h`；
- Page：`be/src/storage/segment/page_builder.h` + `page.h`；
- Compaction：`be/src/storage/compaction_task.cpp`；
- Persistent Index：`be/src/storage/persistent_index.cpp`；
- Storage Engine：`be/src/storage/storage_engine.h`。

## 八、索引机制（简化）

StarRocks 的索引体系主要靠 **短键索引 + Zone Map + Bloom Filter** 达成列存 + 实时 upsert 的极致性能。

### 8.1 短键索引（Short Key Index）

- **Doris-style 前缀索引**：StarRocks 自动取前 36 字节作为稀疏索引；
- **存储位置**：每个 Segment 文件页脚；
- **作用**：二分查找定位到具体 Page。

### 8.2 Zone Map（min/max）

- **每列每 Segment 的 min/max 值**：写入时自动记录；
- **作用**：范围过滤快速跳过整个 Segment。

### 8.3 Bloom Filter

- **高基数等值**：用户可手动为某列创建 Bloom Filter 索引；
- **代价**：写入开销 +10-20%，存储 +5%；
- **源码**：`be/src/storage/segment/bloom_filter_index.h`。

### 8.4 倒排索引（v3.x）

- **全文 / 数组**：v3.x 引入，支持 `LIKE` / `MATCH` / 数组 contains；
- **源码**：`be/src/storage/segment/inverted_index.h`。

## 九、查询 / 计算层

### 9.1 MySQL 协议 + SQL 方言

StarRocks **完全兼容 MySQL 协议**，SQL 方言与 MySQL 类似。

- **MySQL 协议**：默认 9030 端口（query）+ 9020 端口（HTTP）；
- **SQL 方言**：
  - `SELECT ... FROM ... WHERE ... GROUP BY ... ORDER BY ... LIMIT`；
  - **JOIN**：INNER / LEFT / RIGHT / FULL / CROSS / SEMI / ANTI；
  - **WITH**：CTE；
  - **WINDOW**：窗口函数；
  - **类型**：TINYINT / SMALLINT / INT / BIGINT / LARGEINT / FLOAT / DOUBLE / DECIMAL / DATE / DATETIME / VARCHAR / STRING / JSON / ARRAY / MAP / STRUCT / HLL / BITMAP；
- **StarRocks 特有**：
  - **CTE 递归**：`WITH RECURSIVE`；
  - **Bitmap / HLL 函数**：`bitmap_union`、`hll_union`、`bitmap_count`；
  - **物化视图**：异步 / 同步 / 增量刷新；
  - **Catalog 联邦查询**（v2.5+）：`CATALOG hive.db.table`；
  - **Pipeline Hint**：`/*+ SET_VAR(...) */`；

### 9.2 执行模型（CBO + Pipeline）

```
SQL → Parser → Logical Plan → Optimizer（Cascades CBO）
                                  │
                                  ▼ Cost-based Reorder
                          Physical Plan
                                  │
                                  ▼
                    Fragment Split (多阶段)
                                  │
                                  ▼
                    Pipeline Schedule (v2.x)
                                  │
                                  ▼
              Driver + OperatorChain (Pipeline 执行)
                                  │
                                  ▼
              异步 I/O + 多核并行 + 无锁调度
                                  │
                                  ▼
                              Exchange (brpc)
                                  │
                                  ▼
                            Final Result
```

### 9.3 CBO 优化器（Cascades 框架）

StarRocks 的 CBO 基于 **Cascades 框架**，是它与 Doris 的最大差异：

- **统计信息**：通过 `ANALYZE TABLE` 收集列级 / 表级统计；
- **Cost Model**：基于 IO / CPU / Network 代价；
- **Join Reorder**：基于代价的动态规划（DP）；
- **物化视图选择**：基于代价选择最优物化视图；
- **Predicate Pushdown**：列裁剪 + 谓词下推；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/sql/optimizer/Optimizer.java`。

### 9.4 关键优化技术

- **CBO（Cost-Based Optimizer）**：v1.x+ 成熟，Cascades 框架；
- **Pipeline Execution**：v2.x+ 抛弃 Volcano，改用 Pipeline 模型；
- **Runtime Filter**：JOIN 时下推过滤器（min-max / bloom filter / in bitmap）；
- **Adaptive Executor**：自适应并发；
- **Predicate Pushdown**：在 Storage 层先过滤；
- **TopN Pushdown**：大 LIMIT 提前截断；
- **Dict Optimization**：低基数列查询走字典编码；
- **Materialized View**：自动改写查询到物化视图；
- **Global Dictionary**：v3.x+ 跨节点字典编码。

### 9.5 物化视图（Materialized View）

StarRocks 的物化视图是其差异化亮点：

- **同步 MV**：写入时实时更新（与上游表一致）；
- **异步 MV**：后台增量刷新（v2.4+）；
- **自动 MV**：v3.x+ 基于查询模式自动推荐；
- **MV 改写**：CBO 自动选择最优 MV；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/catalog/MaterializedView.java`。

### 9.6 源码路径

- SQL 解析：`fe/fe-core/src/main/java/com/starrocks/sql/parser/AstBuilder.java`；
- Planner：`fe/fe-core/src/main/java/com/starrocks/planner/PlanFragment.java`；
- Optimizer：`fe/fe-core/src/main/java/com/starrocks/sql/optimizer/Optimizer.java`；
- Catalog：`fe/fe-core/src/main/java/com/starrocks/catalog/Catalog.java`；
- Pipeline：`be/src/exec/pipeline/pipeline_driver.h` + `pipeline_builder.h`；
- ScanNode：`be/src/exec/olap_scan_node.h`；
- Join：`be/src/exec/join/join_hash_map.h`；
- Aggregation：`be/src/exec/aggregate/agg_hash_map.h`；
- Exchange：`be/src/runtime/data_stream_mgr.h`。

## 十、事务与一致性

StarRocks 提供有限的事务能力，**OLAP 场景** 不需要完整 ACID。

- **单 Tablet 写入原子性**：`INSERT` 内保证单 Tablet 原子；
- **`DELETE WHERE`**：等价 `UPDATE`（异步重写 Rowset）；
- **Stream Load 事务**：v2.x 引入 **2PC 事务**；
- **Primary Key 表**：强一致实时 upsert；
- **无跨节点 ACID 事务**（by design）；
- **Replication**：副本间通过 BDBJE（FE）+ Paxos-like 协议同步元数据；BE 间通过 `rowset_meta` 同步数据。

## 十一、复制与高可用

### 11.1 FE 高可用

- **BDBJE 持久化**：FE 元数据通过 Berkeley DB Java Edition 持久化；
- **Follower / Observer**：与 Doris 类似；
- **故障切换**：任意 Follower 可晋升 Master，毫秒级；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/persist/EditLog.java` + `fe/fe-core/src/main/java/com/starrocks/ha/BDBJEHA.java`。

### 11.2 BE 高可用

- **多副本**：每个 Tablet 默认 3 副本；
- **副本均衡**：FE 监控副本分布，自动调度迁移；
- **副本修复**：`ADMIN REPAIR TABLE` 触发副本同步；
- **副本类型**：默认 3 副本 + 写入多数副本确认；
- **源码路径**：`be/src/storage/storage_engine.h` + `fe/fe-core/src/main/java/com/starrocks/master/MasterImpl.java`。

### 11.3 数据可靠性

- **Tablet 副本**：默认 3 副本，分布在不同 BE；
- **存算分离**：v2.5+ 数据存于对象存储（S3 / OSS / COS）；
- **跨集群复制（CCR）**：v3.x 异地灾备；
- **快照**：`BACKUP` / `RESTORE`；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/backup/BackupHandler.java`。

## 十二、扩展性（MPP 专题深化）

StarRocks 是 **完整的 MPP 数据库**，是 CBO + Pipeline + 实时 upsert 的极致典范。

### 12.1 集群架构

```
┌──────────────────────────────────────────────────────────────────┐
│                客户端连接任意 FE 节点                              │
│                          │                                       │
│                          ▼                                       │
│             ┌────────────────────────────┐                       │
│             │  FE Leader (RW)            │                       │
│             │  + FE Follower             │                       │
│             │  + FE Observer             │                       │
│             └──────┬─────────────────────┘                       │
│                    │ Thrift / brpc RPC                           │
│        ┌───────────┼───────────┬──────────────┐                 │
│        ▼           ▼           ▼              ▼                 │
│    ┌───────┐   ┌───────┐   ┌───────┐     ┌───────┐              │
│    │ CN 1  │   │ CN 2  │   │ CN 3  │  ...│ CN N  │              │
│    │ 计算  │   │ 计算  │   │ 计算  │     │ 计算  │              │
│    │+数据  │   │+数据  │   │+数据  │     │+数据  │              │
│    └───────┘   └───────┘   └───────┘     └───────┘              │
│                                                                │
│    v2.5+ 可部署：CN（仅计算） + Object Storage（S3/OSS/COS）     │
└──────────────────────────────────────────────────────────────────┘
```

### 12.2 节点角色

| 角色 | 数量 | 职责 |
| --- | --- | --- |
| **FE Leader** | 1 | 读写 + 选主 |
| **FE Follower** | 1-N | 参与选举 + 读写扩展 |
| **FE Observer** | 0-N | 仅做扩展读（不参与选举） |
| **BE** | 3+ | 数据存储 + 计算 |
| **CN**（v2.5+） | 0+ | 仅做计算（存算分离） |
| **Broker** | 0+ | HDFS / S3 读写代理 |

### 12.3 数据分布策略

- **Hash 分桶**：`DISTRIBUTED BY HASH(user_id) BUCKETS N`；
- **Random 分桶**：v3.x+，随机分布；
- **Range 分区 + Hash 分桶**：推荐组合；
- **List 分区**：v2.x+；
- **自动分桶**：v3.x+ Auto Bucket；
- **Colocation Group**：v1.x+，把小表广播到所有 BE 节点；
- **Global Dictionary**：v3.x+ 跨节点字典编码；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/catalog/Partition.java`。

### 12.4 CBO 驱动的多阶段聚合

- **阶段 0 (Local)**：每个 BE/CN 节点扫描本地 Tablet + 局部聚合；
- **阶段 1 (Exchange + Merge)**：Coordinator 跨节点收集局部结果，合并为最终聚合；
- **阶段 2 (Final)**：可选最终聚合（`sum`、`count` 等）；
- **Broadcast Join**：小表广播到所有节点（Colocation 或 hint）；
- **Shuffle Join**：大表间按 Join Key 哈希 shuffle；
- **Runtime Filter**：基于 Bloom Filter / Min-Max 的下推优化；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/planner/PlanFragment.java` + `be/src/exec/join/join_hash_map.h`。

### 12.5 Pipeline 执行（v2.x+ 重大改进）

StarRocks v2.x 抛弃 Volcano 模型，改用 **Pipeline 执行模型**：

- **Pipeline**：多个连续算子组成的流水线；
- **OperatorChain**：算子链（多个连续 stage 合并为一个线程）；
- **Driver**：每个 Pipeline 一个 Driver，多核并行；
- **ScanOperator**：异步 I/O（`io_threads`）；
- **优势**：
  - **调度开销降低**：避免 Volcano 模型的频繁函数调用；
  - **多核利用更高效**：避免算子间的同步开销；
  - **异步 I/O**：CPU 与 IO 重叠；
- **源码路径**：`be/src/exec/pipeline/pipeline.h` + `pipeline_driver.h` + `pipeline_builder.h`。

### 12.6 资源隔离

- **资源组（Resource Group）**：
  - v2.x：`SPROPERTIES ("resource_group" = "group_name")`；
  - v3.x：Workload Group（CPU / 内存 / IO 限制）；
- **Workload Group（v3.x）**：
  - 通过 `CREATE WORKLOAD GROUP` 定义；
  - 软隔离（基于 CGroup）；
  - 支持 CPU 配额、内存限制、并发限制；
- **多租户**：通过 User + Resource Group 实现；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/resource/workloadgroup/WorkloadGroupMgr.java`。

### 12.7 动态扩缩容

- **添加 BE**：`ALTER SYSTEM ADD BACKEND "host:port"` → 自动迁移 Tablet；
- **移除 BE**：`ALTER SYSTEM DECOMMISSION BACKEND "host:port"` → 主动迁移数据；
- **数据重分布**：Tablet 重新调度；
- **扩缩容粒度**：Tablet 级别；
- **自动均衡**：TabletScheduler 自动保持均衡；
- **存算分离**：v2.5+ 可独立扩缩 CN 与对象存储；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/master/TabletScheduler.java`。

## 十三、性能与基准

### 13.1 内部基准

- **SSB（Star Schema Benchmark）**：标准 benchmark；
- **TPC-H / TPC-DS**：标准 SQL benchmark；
- **ClickBench**：OLAP 典型场景；
- **NYC Taxi / GitHub Events**：真实数据集。

### 13.2 性能数字（典型）

- **单表扫描**：1 亿行 < 1 秒；
- **Join 查询**：3-5 表 JOIN < 2 秒（百亿级数据）；
- **写入吞吐**：单机 ~100k rows/s（Stream Load）；
- **实时 upsert**：Primary Key 表 100k+ rows/s；
- **压缩比**：5-10x（典型）；
- **Pipeline 加速**：相比 Volcano 模型 2-5 倍提升。

### 13.3 性能调优

- **数据建模**：Duplicate / Primary / Unique / Aggregate Key 选择；
- **统计信息**：`ANALYZE TABLE` 必须运行（否则 CBO 退化）；
- **CBO**：默认开启（基于 Cascades）；
- **Runtime Filter**：默认开启；
- **会话变量**：`SET enable_pipeline_engine = true`（v2.x+ 默认）；
- **物化视图**：合理设计可加速 10-100x；
- **Global Dictionary**：v3.x+ 加速去重与 join。

## 十四、运维与可观测性

### 14.1 监控

- **Prometheus 集成**：`FE HTTP 端口 8030/metrics`、`BE HTTP 端口 8040/metrics`；
- **Grafana 仪表盘**：官方提供；
- **审计日志**：`fe/log/audit.log`；
- **系统表**：`information_schema`、`__internal_schema`；
- **慢查询**：`SHOW QUERY STATISTICS`；
- **Tablet 状态**：`SHOW TABLET FROM tbl`；
- **副本分布**：`ADMIN SHOW REPLICA DISTRIBUTION FROM tbl`。

### 14.2 备份与恢复

- **Native 备份**：v2.x+ 原生支持 S3 / HDFS 全量 + 增量；
- **`BACKUP` + `RESTORE`**：`BACKUP SNAPSHOT db.snap TO repo ...`；
- **CCR（Cross-Cluster Replication）**：v3.x 异地灾备；
- **存算分离备份**：数据存于对象存储即可备份；
- **源码路径**：`fe/fe-core/src/main/java/com/starrocks/backup/BackupHandler.java`。

### 14.3 升级与滚动

- **大版本**：滚动重启（先 Follower 再 Leader）；
- **数据兼容性**：新版本读旧版本 segment 自动迁移；
- **回退**：保留旧二进制，重启即可读；
- **配置兼容**：`fe.conf` / `be.conf` 增量配置。

### 14.4 故障排查

- **`SHOW PROC "/cluster_health"`**：集群健康状态；
- **`SHOW PROC "/tablet_health"`**：Tablet 健康；
- **`SHOW PROC "/routine_load"`**：Routine Load 状态；
- **`SHOW BACKENDS`**：BE 节点状态；
- **`SHOW TABLETS FROM tbl`**：Tablet 状态；
- **常见问题**：Tablet 版本不一致、副本缺失、Compaction 卡死、BE OOM、Pipeline 卡死。

## 十五、生态与集成

### 15.1 数据接入

- **Stream Load**：HTTP POST 大批量导入（首选）；
- **Broker Load**：从 HDFS / S3 导入（批量）；
- **Routine Load**：从 Kafka 持续消费（流式）；
- **MySQL Load**：`INSERT INTO ... SELECT ... FROM mysql_db.tbl`（联邦）；
- **Spark Load**：从 Spark 写入；
- **Flink StarRocks Connector**：Flink Source / Sink；
- **dbt-starrocks**：dbt adapter；
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
- **StarRocks 官方 SDK**：Stream Loader（HTTPS 上传）。

## 十六、核心论文与算法原理

1. **Apache StarRocks 官方文档**：[docs.starrocks.io](https://docs.starrocks.io)；
2. **"StarRocks: A New-Generation Sub-Second MPP Database"**（VLDB 2023 demo）；
3. **"The Cascades Framework"**（Graefe, VLDB 1995）—— CBO 优化器框架；
4. **"The Volcano/Cascades Optimizer"** —— Volcano 与 Cascades 的关系；
5. **"Pipeline Execution for Query Evaluation"**（StarRocks 团队博客）—— Pipeline 模型；
6. **"Efficiently Compiling Efficient Query Plans for Modern Hardware"**（Neumann et al., VLDB 2011）—— 向量化与代码生成；
7. **"C-Store: A Column-oriented DBMS"**（Stonebraker et al., VLDB 2005）；
8. **"MonetDB/X100"**（Boncz et al., CIDR 2005）—— 向量化执行；
9. **"How Good is My LSM-tree?"** —— Compaction 调优；
10. **"Paxos Made Simple"**（Lamport, 2001）；
11. **"BDBJE 高可用"**（Oracle Berkeley DB 文档）；

## 十七、源码学习路径

### 17.1 必读模块清单（推荐顺序）

| 优先级 | 模块 | 路径 | 说明 |
| --- | --- | --- | --- |
| ★★★★★ | FE 入口 | `fe/fe-core/src/main/java/com/starrocks/qe/StmtExecutor.java` | SQL 执行入口 |
| ★★★★★ | FE Optimizer | `fe/fe-core/src/main/java/com/starrocks/sql/optimizer/Optimizer.java` | CBO |
| ★★★★ | FE Planner | `fe/fe-core/src/main/java/com/starrocks/planner/PlanFragment.java` | 生成 Plan |
| ★★★★ | FE Catalog | `fe/fe-core/src/main/java/com/starrocks/catalog/Catalog.java` | 元数据 |
| ★★★★★ | BE Pipeline | `be/src/exec/pipeline/pipeline.h` + `pipeline_driver.h` | Pipeline 执行 |
| ★★★★★ | BE Scan | `be/src/exec/olap_scan_node.h` | 扫描入口 |
| ★★★★ | BE Join | `be/src/exec/join/hash_join_node.h` | Hash Join |
| ★★★★ | BE Aggregation | `be/src/exec/aggregate/agg_hash_map.h` | 聚合 |
| ★★★★★ | BE Tablet | `be/src/storage/tablet.h` + `tablet.cpp` | 存储抽象 |
| ★★★★★ | BE Segment | `be/src/storage/segment/segment.h` | 列存文件 |
| ★★★★ | BE Compaction | `be/src/storage/compaction_task.cpp` | 后台合并 |
| ★★★★ | BE Persistent Index | `be/src/storage/persistent_index.cpp` | PK 表持久化索引 |
| ★★★ | FE HA | `fe/fe-core/src/main/java/com/starrocks/ha/BDBJEHA.java` | FE 高可用 |

### 17.2 源码规模与语言

- **总 LOC**：~150 万行（FE Java + BE C++）；
- **Java 占比**：~40%（FE + 工具）；
- **C++ 占比**：~55%（BE）；
- **辅助**：Python（test）、CMake（build）；
- **构建系统**：Maven（FE）+ CMake（BE）；
- **编译时长**：BE 冷启动 30-60 分钟，FE 几分钟。

## 十八、学习资源

- **官方文档**：[docs.starrocks.io](https://docs.starrocks.io)；
- **官方博客**：[starrocks.io/blog](https://www.starrocks.io/blog)；
- **GitHub**：[github.com/StarRocks/starrocks](https://github.com/StarRocks/starrocks)；
- **Slack**：[starrocks.slack.com](https://starrocks.slack.com)；
- **社区**：GitHub Discussions、中文用户群；
- **视频**：StarRocks Meetup、Bilibili StarRocks 教程；
- **认证**：StarRocks 官方认证（Certified StarRocks Engineer）；
- **CelerData 培训**：商业培训课程。

## 十九、可移植性与部署

- **操作系统**：Linux（主）、macOS（开发）；
- **容器化**：官方 Docker 镜像；
- **Helm Chart**：[StarRocks Helm Chart](https://github.com/StarRocks/starrocks-kubernetes-operator)；
- **Operator**：[starrocks-kubernetes-operator](https://github.com/StarRocks/starrocks-kubernetes-operator)；
- **离线部署**：tar 包 + 配置即可；
- **云部署**：CelerData Cloud / 阿里云 / 腾讯云 / AWS Marketplace。

## 二十、变体与衍生项目

- **Apache StarRocks**（开源，Apache 2.0）：核心；
- **CelerData**（商业）：StarRocks 商业公司；
- **CelerData Cloud**：托管云服务；
- **阿里云 StarRocks 版**：阿里云托管；
- **腾讯云 StarRocks**：腾讯云托管；
- **AWS Marketplace StarRocks**：AWS 部署镜像；
- **StarRocks 工具**：`starrocks-manager`、Stream Loader、CelerData Studio（GUI）；

## 二十一、风险与陷阱

### 21.1 协议与兼容性陷阱

- **MySQL 协议不完全支持**：事务、存储过程、用户变量行为差异；
- **数据类型差异**：StarRocks `LARGEINT`、`HLL`、`BITMAP` 不在 MySQL 标准；
- **`GROUP BY` 顺序敏感**：StarRocks 要求所有非聚合列都在 GROUP BY 中；
- **索引自动选择**：自动取前 36 字节，与用户预期不符；
- **Unique Key vs Primary Key**：版本差异行为不同；
- **AGGREGATE KEY 的 HLL/BITMAP**：重建时需用 `HLL_UNION`；
- **JDBC 兼容性**：MySQL JDBC 驱动可能不识别 StarRocks 特有类型；
- **流式事务（Stream Load 2PC）**：v2.x+ 支持；
- **多 Catalog**：catalog 联邦查询性能差异大；
- **小表 Broadcast Join**：colocation 配置复杂；
- **自动分桶（Auto Bucket）**：v3.x+ 实验；
- **物化视图改写**：CBO 可能未选择最优 MV；

### 21.2 性能陷阱

- **`SELECT *`**：拉取所有列，列存优势丧失；
- **`ORDER BY` 顺序不当**：短键索引失效；
- **高基数 JOIN Key**：shuffle 数据爆炸；
- **Broadcast Join 大表**：BE/CN 内存 OOM；
- **不写统计信息**：`ANALYZE TABLE` 未运行 → CBO 退化；
- **GROUP BY 内存爆炸**：超过 `query_mem_limit`；
- **Array / JSON 嵌套**：性能差；
- **大 LIMIT OFFSET**：全表扫描；
- **Stream Load 小包**：网络 / RPC 开销主导；
- **Routine Load 任务失败**：Kafka offset 管理；
- **多副本写入放大**：3 副本 3 倍网络；
- **频繁 Schema Change**：仅支持加列；
- **Tablet 数量过多**：单 Tablet < 100MB 时，调度开销大；
- **Compaction 卡死**：大 Rowset 合并慢；
- **UDF**：BE SDK C++，编写复杂；
- **跨节点 JOIN**：网络瓶颈（设计 colocation）；
- **Pipeline 不当**：v2.x+ Pipeline 调度配置不当；

### 21.3 运维陷阱

- **FE Leader 单点**：BDBJE 选主慢（秒级）；
- **BE 磁盘满**：Tablet 不可写；
- **副本缺失**：必须 `ADMIN REPAIR TABLE`；
- **Compaction 卡死**：Tablet 不可读；
- **Routine Load 任务堆积**：Kafka lag 增长；
- **冷热分层数据迁移失败**：S3 权限错误；
- **存算分离缓存穿透**：Data Cache 未命中导致 IO 风暴；
- **备份恢复**：Broker 依赖；
- **CCR 延迟**：异地复制可能秒级延迟；
- **大版本升级**：未滚动重启导致副本不同步；
- **集群节点配置漂移**：`fe.conf` / `be.conf` 散落各处；
- **监控告警缺失**：未监控 Tablet 健康 / replica lag；
- **TLS 配置错误**：FE / BE 通信失败；
- **Workload Group 配置错误**：CGroup 路径错误导致资源限制失效；
- **网络分区**：BE 心跳超时导致误判；
- **Auto Bucket 误用**：自动分桶不适用于所有场景；
- **冷启动延迟**：重启后首次查询慢（mark / page 缓存冷）；
- **Pipeline 卡死**：算子链过长导致调度失败；

### 21.4 安全陷阱

- **默认无 root 密码**：必须改；
- **明文密码**：配置文件中明文；
- **SQL 注入**：动态 SQL 拼接；
- **网络暴露**：FE 9030 / 9020 端口对外暴露；
- **RBAC 配置复杂**：User / Role / Resource Group 易错配；
- **审计日志**：默认开启可能影响性能；
- **Kerberos / LDAP**：企业版特性；

## 二十二、学习路径

### 22.1 入门（1 周）

1. 阅读 [docs.starrocks.io](https://docs.starrocks.io) Quick Start；
2. Docker 启动 `starrocks/allin1-ubuntu`；
3. 用 `mysql -h 127.0.0.1 -P 9030` 连接；
4. 创建第一张 Duplicate Key 表；
5. 用 Stream Load 导入数据；
6. 运行 `SELECT count() FROM table`；
7. 阅读 `be/src/storage/tablet.h` 的 100 行核心；
8. 阅读 [StarRocks Summit 视频](https://www.youtube.com/@StarRocksLabs)。

### 22.2 中级（2-4 周）

1. 部署 3 FE + 3 BE cluster；
2. 测试 Primary Key 实时 upsert；
3. 学习 Aggregate Key 预聚合；
4. 测试 JOIN 性能（多表 5 亿 JOIN）；
5. 配置 Workload Group 资源隔离；
6. 集成 Kafka Routine Load；
7. 集成 Flink StarRocks Connector；
8. 阅读 `fe/fe-core/src/main/java/com/starrocks/sql/optimizer/Optimizer.java`；
9. 阅读 `be/src/exec/pipeline/pipeline.h`；
10. 性能测试：`EXPLAIN` / `EXPLAIN ANALYZE`；
11. 设计第一个物化视图（异步 MV）；
12. 启用 Prometheus metrics + Grafana dashboard。

### 22.3 进阶（1-3 个月）

1. 部署多 BE 多副本生产集群；
2. 调优 `query_mem_limit` / `BUCKETS N` / 统计信息；
3. 实现自定义 Colocation Group；
4. 启用 CCR 跨集群复制；
5. 部署存算分离集群（CN + Object Storage）；
6. 自研 UDF（Java UDF）；
7. 阅读全部 `fe/fe-core/src/main/java/com/starrocks/sql/optimizer/`；
8. 阅读 `be/src/storage/segment/` 全部；
9. 阅读 `be/src/exec/join/hash_join_node.cpp` 的 join 实现；
10. 性能基准 vs Doris / ClickHouse；
11. 贡献 StarRocks 社区（PR 新算子、修复 bug）；
12. 深入 Pipeline 执行模型（`be/src/exec/pipeline/`）；

## 二十三、与本项目的关联

`engineering/` 中**没有 StarRocks 实现**，但有以下可借鉴模式：

- **CBO + Cascades 优化器** 与 `engineering/src/db/sql_engine.c` 的执行器可对比学习（`engineering/` 目前无 CBO）；
- **Pipeline 执行模型** 是 StarRocks 的核心创新，可借鉴到 `engineering/src/db/` 的执行器；
- **FE + BE 双进程架构** 与 `engineering/` 的 `db_server.c` + `bufmgr.c` 分层有相似之处；
- **Tablet + Segment 抽象** 可应用到 `engineering/src/db/storage_engine.c` 的多模态存储设计；
- **Primary Key 表实时 upsert** 与 `engineering/src/db/wal.c` 的 WAL + 版本链思想一致；
- **物化视图自动改写** 与 `engineering/src/db/` 未来物化视图模块可对比；
- **Workload Group 资源隔离** 可应用到 `engineering/src/db/` 的多租户隔离；
- **Compaction 机制** 与 `engineering/src/db/bufmgr.c` 的 checkpoint 设计思想一致；
- **Colocation Join** 与 `engineering/apps/` 的多副本数据库设计类似；
- **Multi-Catalog 联邦查询** 可应用到 `engineering/src/db/` 的多数据源查询；
- **MySQL 协议兼容** 与 `engineering/apps/db_driver/` 协议设计可对比学习；
- **Stream Load HTTP 上传** 与 `engineering/apps/web/` 的 REST API 设计类似；
- **测试模式**：StarRocks 的 `test/sql/` 数万条 SQL 测试用例，是高质量测试集范本。

## 二十四、稀疏索引与三层剪枝：算法细节

> 本节及 §25 / §26 的所有源码路径与常量均在 **StarRocks 3.5.21**（v3.x 末期 tag）实测核对。
> 3.x 与 2.5 在 BE 上已分化明显（如 runtime filter 从独立目录迁入 `be/src/storage/`），故本节只使用 3.x 实测成立的符号，不与 2.5 混写。

### 24.1 稀疏索引的真实形态：ShortKeyIndex

StarRocks 的稀疏索引不是「每页一行」也不是固定前缀截断，而是 **每 N 行（一个 block）一条变长短键**：

- **粒度**：`SegmentWriterOptions::num_rows_per_block = 1024`（生产构建）；`BE_TEST` 下为 `100`；
- **写入时机**：`SegmentWriter::flush` 中当 `_num_rows_written % num_rows_per_block == 0` 时 `add_item()` 写入一条短键；
- **序列化布局**（源码注释原文）：

```text
ShortKeyPageBody := KeyContent^NumEntry, KeyOffset(vint)^NumEntry
NumEntry, KeyBytes, OffsetBytes is stored in ShortKeyFooterPB
```

- **块号 → 行号换算**（`SegmentIterator` 内部公式）：
  `start_rowid = start_block_id * num_rows_per_block`，`end_rowid = (end_block_id + 1) * num_rows_per_block`。

短键的字节编码用一组**单字节 marker** 表达「相对大小」而非逐字段比较，这是它能变长且保序的关键：

| Marker | 含义 |
| --- | --- |
| `KEY_MINIMAL_MARKER` (0x00) | 字段最小值 |
| `KEY_NULL_FIRST_MARKER` (0x01) | NULL，且 NULL 排在值之前 |
| `KEY_NORMAL_MARKER` (0x02) | 普通值，内容紧随其后 |
| `KEY_NULL_LAST_MARKER` (0xFE) | NULL，且 NULL 排在值之后 |
| `KEY_MAXIMAL_MARKER` (0xFF) | 字段最大值 |

由此短键序列天然有序，可直接二分：

```text
// Segment::lower_bound / upper_bound → _sk_index_decoder 上的二分
function seekBlock(segment, predicate_key):
    it = segment.lower_bound(predicate_key)      // ShortKeyIndexDecoder::lower_bound
    while it != segment.end():
        block = *it
        # 该 block 的 [block*B, (block+1)*B) 行区间交给 Zone Map 再判
        if not zone_map_may_overlap(block, predicate_key):
            return NO_MATCH_BLOCK                 # 区间上界已越过，后续 block 更大，可提前终止
        if zone_map_definitely_inside(block, predicate_key):
            return MATCH_BLOCK
        emit candidate_range(block)
        it += 1
    return EMPTY
```

**复杂度**：定位起点需 `O(log K)`（K = 短键条数 ≈ N/1024）；候选 block 数为 `O(log(N/B))` 量级（Zone Map 的 min/max 对有序键区间是紧致的），每个候选 block 的判定 `O(1)`。相比全量扫描 `O(N)`，剪枝收益随基数上升而增大。

### 24.2 三层剪枝：Rowset → Segment → Page

StarRocks 的扫描并非「打开文件再逐页过滤」，而是 **三棵索引逐层收敛**（3.x 已把谓词解析下沉到 `be/src/storage/predicate_tree/`）：

```text
function scanTablet(tablet, version, conjuncts):
    # L1 Rowset 级：key range + 段级统计
    for rs in version.rowsets:
        if not rowset_key_range_may_hit(rs, conjuncts):      # sort key min/max
            continue
        # L2 Segment 级：Zone Map（每列每段 min/max）
        for seg in rs.segments:
            zm = seg.zone_maps()
            if any(conjunct, zone_map_prune(conjunct, zm)):   # 可判定为必不命中
                continue
            # L3 Page 级：Ordinal/Page Index + Page Cache
            page_ranges = seg.page_index_prune(conjunct)      # 直接跳到页号
            for pr in page_ranges:
                for blk in pr.blocks():
                    handle = StoragePageCache.lookup(CacheKey(seg, col, blk))
                    yield decode_page(handle)                  # RAII 句柄，析构时归还
```

三层剪枝的复杂度递降关系：`O(rowsets) → O(segments) → O(pages)`。StarRocks 3.x 用 `RuntimeFilterPredicates`（按 `driver_sequence` 分组）把 **runtime filter 注入到 L2/L3 之间的列块读取路径**上，因此下推谓词与运行时过滤共用同一套 `ColumnPredicate` 抽象。

### 24.3 Page Cache

`StoragePageCache` 是 StarRocks 3.x 的列块缓存（3.x 从独立子目录收敛到了 `be/src/storage/page_cache.h`）：

- `lookup(const CacheKey& key, PageCacheHandle* handle)` —— 查缓存并交出 RAII 句柄，`PageCacheHandle` 析构时自动释放条目（引用计数式）；
- `memory_usage()` / `set_capacity()` / `get_capacity()` / `adjust_capacity(int64_t delta, size_t min_capacity)` —— 容量在线调整；
- `get_lookup_count()` —— 命中率观测。

缓存粒度是 **解码后的页数据**，因此命中后无需重复解压，是「读放大」的关键抑制点。

### 24.4 Sorted Store：Primary Key 表的内存有序化

Primary Key 表的实时 upsert 并不靠 delete bitmap（3.x 早已用 `DelVector` 取代，见 §25.4），而是靠 **写入侧的内存有序化（Sorted Store）**：

- `MemTable::_sort(bool is_final, bool by_sort_key)` / `_append_to_sorted_chunk(Chunk* src, Chunk* dest, bool is_final)` —— 写入的 Chunk 先按 sort key 排序再追加到有序缓冲区，`is_final` 决定是否已到可 flush 状态；
- `PrimaryIndex::upsert(rssid, rowid_start, pks, DeletesMap* deletes)` —— 同一主键再写时，把**被覆盖的旧位置**（`rssid`, `rowid`）收集进 `DeletesMap`；
- `PrimaryIndex::erase(pks, DeletesMap*)` —— 删除同样落到 `DeletesMap`；
- `MemTable::check_supported_column_partial_update()` —— partial update 的前置合法性检查；
- `PrimaryIndex::commit(PersistentIndexMetaPB*)` —— 刷盘后持久化索引。

即：**新行追加进有序区，旧行位置被登记为待删**，读写分离。写放大为 O(1) 次定位 + 一次有序缓冲追加，摊还复杂度 O(log N)（主键索引查找）。

### 24.5 本节路径校正（3.5.21 实测）

§7.2 / §7.4 / §8 给出的若干路径在 3.5.21 中**不成立**，此处只做补充说明（不改动前文）：

| §7/§8 中的写法 | 3.5.21 实际路径 |
| --- | --- |
| `be/src/storage/segment/rle_page.h` 等 6 个压缩页 | `be/src/storage/rowset/rle_page.h`、`plain_page.h`、`bitshuffle_page.h`、`dict_page.h`、`binary_dict_page.h`、`binary_prefix_page.h`、`frame_of_reference_page.h` |
| `be/src/storage/segment/short_key_index.cpp` | `be/src/storage/short_key_index.cpp` |
| `be/src/storage/segment/segment.h` / `column_reader.h` | `be/src/storage/rowset/segment.h` / `column_reader.h` |
| `be/src/storage/segment/zone_map_index.cpp` | `be/src/storage/rowset/zone_map_index.cpp` |
| `be/src/storage/segment/bloom_filter_index.cpp` | `be/src/storage/rowset/bloom_filter_index_reader.cpp` + `bloom_filter_index_writer.cpp` |
| `be/src/storage/segment/inverted_index.h` | 不在该路径；3.x 倒排索引在 `be/src/storage/index/` 与 `be/src/formats/` 一侧 |
| `be/src/storage/column/column_data.cpp` | 3.x 无 `be/src/storage/column/`；列容器在 `be/src/column/`（`column.h`、`binary_column.h`、`fixed_length_column.h`） |
| 「前 36 字节短键」 | 3.5.21 未见固定 36 字节截断常量，实为变长编码 + `TabletSchema::num_short_key_columns()` |
| Bloom Filter 默认 FPP | `be/src/storage/olap_define.h`：`BLOOM_FILTER_DEFAULT_FPP = 0.05` |

## 二十五、Compaction 与 partial update 的合并语义

§7.6 只用四条要点概述了 Compaction。本节补上算法层面：**生命周期、输出切分、partial update 的 read-modify-write、删除位图的写放大**。

### 25.1 Compaction 的四阶段生命周期

`Compaction`（`be/src/storage/compaction.h`）是所有 compaction 的基类，源码注释给出了固定四步：

```text
1. pick rowsets satisfied to compact     # 选输入
2. do compaction                         # 归并
3. modify rowsets                        # 版本推进（modify_rowsets_without_lock）
4. gc unused rowsets                     # 回收
```

各阶段对应虚函数：`pick_rowsets_to_compact()` / `do_compaction()` / `compact()` / `ReaderType compaction_type()`。子类包括 `CumulativeCompaction`（存量+增量累积合并）与 `SizeTieredCompactionPolicy`（按大小分层，偏向新数据）。

`CompactionTask::run()` 内部有一条**行数守恒断言**，是判断 compaction 是否正确的重要不变量：

```text
DCHECK(_task_info.input_rows_num ==
       _output_rowset->num_rows() + stats.merged_rows + stats.filtered_rows)
```

即：`输入行数 = 输出行数 + 被主键合并掉的行数 + 被谓词过滤掉的行数`。Primary Key 表的 `merged_rows > 0` 正是 upsert 语义在 compaction 阶段的体现 —— 同一 sort key 的多行只保留一最新行。

### 25.2 输出 Rowset 的切分

Compaction 不把全部输入压成一个文件，而是按行数上限滚动切分多个输出 Rowset。设单次 compaction 输入总行数为 `N_in`，单个 Rowset 的行数上限为 `L`（由 FE 下发的 `output_rowset_size` / compaction 参数决定），则：

```text
输出 Rowset 数 ≈ ceil(N_in / L)
写放大 = (N_in + 冗余 I/O) / N_out
```

对 Primary Key 表，`N_out < N_in`（合并掉重复主键）；对 Duplicate 表 `N_out == N_in`。写放大主要来自 **读 N_in 写 N_out + 删除旧 Rowset 文件**，因此 `L` 取值是 I/O 效率与 compaction 触发频率的直接权衡。

### 25.3 Partial update 的 read-modify-write

Primary Key 表的 partial update（只更新部分列）走 **「定位 → 读旧行 → 补齐未更新列 → 整行追加」** 的四步：

```text
function partialUpdate(tablet, rowset_id, key_columns, value_columns, new_values, update_mask):
    # 1. 在 PrimaryIndex 中按主键定位
    rows = PrimaryIndex.get(key_columns)                    # -> (rssid, rowid)
    if rows is EMPTY:
        # 主键不存在 -> 退化为整行 INSERT
        append_full_row(key_columns + value_columns, new_values)
        return INSERTED

    old = concat_all_segments.read_rows(rows, all_columns)  # 2. 读旧行全列

    # 3. 按 mask 合并：仅更新 value_columns 中置位的列
    merged = old
    for col in value_columns where update_mask[col]:
        merged[col] = new_values[col]

    # 4. 整行追加到有序 MemTable；旧位置由 upsert 登记为待删
    DeletesMap deletes
    memtable.insert(merged)
    PrimaryIndex.upsert(rows.rssid, rows.rowid, key_columns, &deletes)
    for (rssid, rowid) in deletes:
        DelVector[rssid].add(rowid, version)                # 旧行在新版本中被遮蔽
    return UPDATED
```

**复杂度**：单行 partial update 为 `O(log N_k)`（主键索引查找，`N_k` 为该 Tablet 主键数）+ `O(|all_columns|)`（读旧行宽度）。这是 partial update 相比 delete+insert 方案省掉一次排序归并的原因 —— 但代价是 **必须读取全部列**，所以宽表上应把常变列放在更新列里、常量列排除。

`PrimaryIndex::try_replace(...)` / `replace(...)` 是配套的 **原地替换** 路径：若旧行仍位于 `min_movable_rowset` 之后的可变 Rowset 中，可原地覆盖而无需读改写。

### 25.4 DelVector：版本化的 Roaring Bitmap 及其写放大

3.x 中标记删除的载体是 `DelVector`（`be/src/storage/del_vector.h`），源码注释给出的文件布局是：

```text
|<format version (currently 0x01)> 1 byte| serialized roaring bitmap |
```

- 每个 DelVector 绑定一个版本（`EditVersion` 的 major version）；
- `add_dels_as_new_version(dels, version, ...)` 的签名是「一批删除 + 一个新版本」，表明它是 **按版本整段重写** 而非增量 patch；
- `load(version, data, length)` / `init(version, data, length)` 按版本懒加载，多版本可共存（读时取当前 version 对应的那一份）。

**写放大推导**（按 roaring bitmap 的序列化语义，非源码常量）：若单 Rowset 行数为 `n`，每次产生删除都要重写整个 bitmap，开销为 `O(n / 64)` 个 64 位字；而 Roaring 的 run-length 编码在删除点稀疏时能大幅压缩上界。代价是删除点密集（如批量 DELETE 命中大半行）时，退化为接近 `O(n)` 的重写 —— 这正是 StarRocks 建议 **删除走 compaction 而非高频小批量 DELETE** 的原因。

### 25.5 本节路径校正（3.5.21 实测）

| §7 / §7.6 / §7.7 中的写法 | 3.5.21 实际路径 |
| --- | --- |
| `be/src/storage/compaction/` 子目录 | **不存在**。3.x compaction 文件平铺在 `be/src/storage/`：`compaction.h`、`compaction_task.cpp`、`compaction_context.h`、`compaction_manager.cpp`、`compaction_utils.cpp`、`base_compaction.cpp`、`cumulative_compaction.cpp`、`vertical_compaction_task.cpp`、`size_tiered_compaction_policy.cpp` |
| `be/src/storage/compaction_task.cpp` | 路径正确，但该文件只是任务调度层；归并逻辑在 `Compaction::do_compaction()` |
| `be/src/storage/compaction_manager.cpp` | 路径正确 |
| `be/src/storage/delete_bitmap.cpp`（§7.3.3 Unique Key） | 3.x 不存在。标记删除用 `be/src/storage/del_vector.cpp`；Unique Key 表在 3.x 已基本由 Primary Key 表取代 |
| `be/src/storage/primary_key_index.cpp`（§7.3.2） | 实际为 `be/src/storage/primary_index.cpp`（`PersistentIndex` 另在 `persistent_index.cpp`） |
| `be/src/storage/segment/aggregate_reader.cpp`（§7.3.4） | 不在该路径；3.x 聚合读在 `be/src/storage/aggregate_iterator.cpp` |
| `be/src/storage/page_cache/page_cache.h`（brief 写法） | 3.5.21 为 `be/src/storage/page_cache.h` |

## 二十六、CBO：等价类谓词下推与 Join 选序

§9.3 / §9.4 用要点罗列了 CBO 与 runtime filter，但未展开算法。StarRocks 的 EQBO（Equivalence-Based Query Optimizer）在 Join 选序上有一个**可逐行对照源码**的实现：`JoinReorderDP`（精确子集 DP）、`JoinReorderGreedy`（贪心 bushy DPsub）、`JoinReorderLeftDeep`（左深树），由 `JoinReorderFactory` 按会话变量门控后在三者中取最小代价。

### 26.1 等价类（Equivalence Class）驱动的谓词下推

EQBO 的基础结构是两个容器：

- `ColumnRefSet`（`be/.../sql/optimizer/base/ColumnRefSet.java`）—— 用 `BitSet` 表示「一批列」，Join 组的 atoms 直接用它做集合运算；
- `EquivalenceClasses`（`.../base/EquivalenceClasses.java`）—— 列等价类容器，核心 API 为 `addEquivalence(left, right)`、`getEquivalenceClass(column)`、`getEquivalenceClasses()`。

它解决的问题：若 `t1.a = t2.b`，则 `t1.a > 10` 与 `t2.b > 10` 语义等价，可以互相转写。于是 **任何一个等价类成员上的谓词都可以下推到所有成员所在的关系上**：

```text
function deriveEquivalencePredicates(relations, equivalence_classes):
    out = []
    for ec in equivalence_classes.getEquivalenceClasses():
        preds = []                              # 挂在这个等价类上的谓词
        for col in ec:
            for rel in relations_of(col):
                preds += predicates_on(rel, col)   # 收集该列上的谓词
        # 谓词下推：把 ec 上收集到的谓词下推到 relations 的交/并
        for rel in relations_of(ec):
            for p in preds:
                out += push_down(p, rel, ec)       # p 中的列按 ec 改写为 rel 的列
    return out
```

这一步把「谓词绑在某一列上」变成「谓词绑在等价类上」，从而让 Join 选序在枚举连接顺序时可以自由地左右交换等价列 —— 这是 StarRocks Join Reorder 能够工作的前提。

### 26.2 三条 Join 选序策略的选路

`JoinReorderFactory.createJoinReorderAdaptive()` 的门控逻辑（可直读）：

```text
algorithms = [ JoinReorderLeftDeep ]                       # 永远参与

if cbo_enable_dp_join_reorder  and atoms.size <= cbo_max_reorder_node_use_dp:
    algorithms += [ JoinReorderDP ]

if cbo_enable_greedy_join_reorder and atoms.size <= cbo_max_reorder_node_use_greedy:
    algorithms += [ JoinReorderGreedy ]

# CBO 对每个候选生成物理计划并比代价，取最小
return argmin_cost(algorithms.map(plan))
```

即 **DP 精确解与 Greedy 近似解同台竞技，由代价模型裁决**。DP 是 `O(3^n)` 的精确解，只在小表数时开启；表数一多就退化到贪心。

### 26.3 DP 子集枚举：递推、记忆化与剪枝

`JoinReorderDP.getBestExpr(BitSet joinKeys)` 的递推式，就是教科书式的 **子集划分 DP**：

```text
memo = {}                                       # BitSet -> GroupInfo

function best(S):                              # S = 参与连接的基表集合
    if |S| == 1: return singleton_group(S)      # 基表：代价为扫描代价
    if S in memo: return memo[S]

    best_expr = None
    for A in proper_nonempty_subsets(S):       # 注意：A 与 S\A 都会被各枚举一次
        L = best(A)
        if best_expr and L.cost > best_expr.cost: continue    # 剪枝 1（下半边已经超预算）
        R = best(S \ A)
        if best_expr and R.cost > best_expr.cost: continue    # 剪枝 2（对称剪枝）
        e = build_join_expr(L, R)
        if e is None: continue                  # 无法构造合法 JOIN（如外连接阻断）
        e.expr.deriveLogicalPropertyItself()
        calculateStatistics(e.expr)            # 依等价类传播的统计信息算中间结果行数
        computeCost(e)
        best_expr = min_by_cost(best_expr, e)
    memo[S] = best_expr
    return best_expr
```

**递推式**：`cost*(S) = min over A ⊂ S, A ≠ ∅ of [ cost*(A) + cost*(S\A) + joinCost(A, S\A) ]`。

**复杂度**：状态数为 `2^n`（每个 BitSet 一个 memo 项，内存 `O(2^n)`）；每个状态 `S` 枚举 `2^|S| - 2` 个划分，总工作量

```text
Σ_{k=0}^{n} C(n,k) · 2^k = (1 + 2)^n = 3^n
```

即 **`O(3^n)` 时间 / `O(2^n)` 空间**。两个剪枝把实际开销显著压低：只要子问题最优代价已超过当前最优，就不再展开（代价单调可加，子树代价不可能再降）。此外 `generatePartitions` 枚举的是**完整幂集**而非「含最小元素的那一半」，所以每个无序划分被访问两次 —— 这是这份实现里一个可观测的常数因子开销，剪枝恰好也顺带缓解了它。

### 26.4 Greedy：按层切分的 bushy 搜索

`JoinReorderGreedy` 不做精确递推，而是把解空间按 **join 层级（join level）** 组织，逐层扩展：

```text
# 左深 + bushy 混合：curJoinLevel = x + y，且 x >= y > 1
for rightLevel in 2 .. curJoinLevel / 2:
    searchJoinOrders(curJoinLevel - rightLevel, rightLevel, isSearchBushy = true)

function searchJoinOrders(leftLevel, rightLevel, isSearchBushy):
    for L in best_groups_of_level(leftLevel, curLevel):
        for R in (isSearchBushy ? best_groups_of_level(rightLevel) : all_groups_of_level(rightLevel)):
            if L.atoms ∩ R.atoms ≠ ∅: continue      # 同一基表不能被连两次
            e = build_join_expr(L, R)
            e.expr.deriveLogicalPropertyItself()
            calculateStatistics(e.expr)
            computeCost(e)
            record(curLevel, L.atoms ∪ R.atoms, e)   # 同一 atoms 集合下保留最优
```

三处值得注意的设计：

1. **`curJoinLevel >= 4` 才搜 bushy**（源码注释：3 层及以下的 join tree 本身就不可能是 bushy）；
2. **首表不用贪心** —— `getBestGroupList` 在 `curLevel.level == 1` 时原样返回全部候选，注释写明「否则容易陷入局部最优」；
3. **顶层保留 TopK，非顶层只留 1 个最优** —— `addExprToGroup` 对 `atoms.cardinality() == atomSize` 的全集把表达式塞进优先队列 `topKExpr`，其余组只维护 `lowestExprCost`。这让搜索在中间层极度收敛，只在最终层放宽以保留多样性。

### 26.5 Runtime Filter 的生成与注入

Runtime filter 由 **build 侧生成、probe 侧消费、存储层执行** 三段构成：

1. **生成**：`JoinBuildJoiners` 持有 `pipeline::RuntimeMembershipFilters _build_runtime_filters`（即 `std::vector<RuntimeMembershipFilterPtr>`，别名指向 `RuntimeFilterBuildDescriptor`），在 `create_runtime_filters(RuntimeState*)` 中把 build 侧的 min/max、字典、bloom 信息封装下发；`_build_runtime_filters_from_planner` 标志区分「由 FE 下推」与「BE 自建」。
2. **传输**：`_build_runtime_filters` 经 brpc 从 build 端广播给各 probe Driver。
3. **执行**：`be/src/storage/runtime_filter_predicate.h` 中 `RuntimeFilterPredicates` 按 `driver_sequence` 收集一组 `RuntimeFilterPredicate`，在列块解码后、Filter 算子之前求值；`DictColumnRuntimeFilterPredicate` 是字典列的特化实现。

因为注入点在**列块解码之后**，runtime filter 省下的是「解码后的数据参与后续 Filter / Join Probe 的开销」，而不是 I/O 本身 —— I/O 的削减靠的是 §24 的 Zone Map 与短键索引。

### 26.6 本节路径校正（3.5.21 实测）

| brief / §9.3 中的写法 | 3.5.21 实际 |
| --- | --- |
| `StarOptJoinOrderOptimizer.java`（brief） | **StarRocks 无此类名**（那是 Doris 的）。StarRocks 是 `.../sql/optimizer/rule/join/` 下的 `JoinReorderDP` / `JoinReorderGreedy` / `JoinReorderLeftDeep` / `JoinOrder` / `MultiJoinNode` |
| `.../sql/optimizer/join/`（brief） | 该子目录不存在，实际为 `.../sql/optimizer/rule/join/` |
| `.../sql/optimizer/utils/EquivalenceUtils.java`（brief） | 不存在；等价类在 `.../sql/optimizer/base/EquivalenceClasses.java` 与 `base/ColumnRefSet.java` |
| `JoinGraph` / `EqrexNode`（brief） | 均未在 3.5.21 源码中出现，已剔除 |
| `be/src/runtime_filter/`（brief） | **3.5.21 无此目录**。运行时过滤在 `be/src/storage/runtime_filter_predicate.h` 与 `be/src/exec/pipeline/runtime_filter_types.h` |
| `be/src/vectorized/`（brief） | **3.5.21 无此目录**。等价位置：`be/src/exec/aggregator.cpp`、`be/src/exec/join_hash_map.cpp`、`be/src/exec/sorting/`、`be/src/column/` |
| `be/src/exec/join/join_hash_table.cpp`（brief） | 3.x 为平铺的 `be/src/exec/join_hash_map.cpp` + `hash_joiner.cpp` + `hash_join_node.cpp`（无 `join/` 子目录） |
| `be/src/storage/rowset/segment*.cpp`（brief） | 路径**正确** —— 3.5.21 中 segment/column reader/writer 均在 `be/src/storage/rowset/` |
| `be/src/storage/column/column_data.cpp`（brief） | 3.5.21 无 `be/src/storage/column/`；列容器在 `be/src/column/` |
| `be/src/storage/temporary_variable_page.cpp`（brief） | 3.5.21 不存在；Sorted Store 由 `be/src/storage/memtable.cpp` 的 `MemTable::_sort` 承担 |
| `be/src/storage/page_cache/page_cache.h`（brief） | 3.5.21 为 `be/src/storage/page_cache.h` |
| `.../optimizer/statistics/`、`.../catalog/`（brief） | 路径**正确** |
| 代价模型（§9.3） | `.../sql/optimizer/cost/CostModel.java`、`HashJoinCostModel.java` |

## 二十七、参考资料

1. **Apache StarRocks 官方仓库**：[github.com/StarRocks/starrocks](https://github.com/StarRocks/starrocks)；
2. **官方文档**：[docs.starrocks.io](https://docs.starrocks.io)；
3. **官方博客**：[starrocks.io/blog](https://www.starrocks.io/blog)；
4. **StarRocks Summit**：[starrocks.io/summit](https://www.starrocks.io/summit)；
5. **CelerData**：[celerdata.com](https://celerdata.com)；
6. **StarRocks Kubernetes Operator**：[github.com/StarRocks/starrocks-kubernetes-operator](https://github.com/StarRocks/starrocks-kubernetes-operator)；
7. **Flink StarRocks Connector**：[github.com/StarRocks/starrocks-connector-for-apache-flink](https://github.com/StarRocks/starrocks-connector-for-apache-flink)；
8. **Spark StarRocks Connector**：[github.com/StarRocks/starrocks-spark-connector](https://github.com/StarRocks/starrocks-spark-connector)；
9. **dbt-starrocks**：[github.com/StarRocks/dbt-starrocks](https://github.com/StarRocks/dbt-starrocks)；
10. **StarRocks 与 Doris 对比**：starrocks.io/blog；
11. **StarRocks 与 ClickHouse 对比**：starrocks.io/blog；
12. **Star Schema Benchmark (SSB)**：[github.com/electrum/ssb-dbgen](https://github.com/electrum/ssb-dbgen)；
13. **TPC-H 文档**：[tpc.org/tpc_h](http://tpc.org/tpc_h)；
14. **Cascades 框架**："The Cascades Framework"（Graefe, VLDB 1995）；
15. **Volcano 模型**："Volcano: An Extensible and Parallel Query Evaluation System"（Graefe, IEEE 1994）；
16. **MonetDB/X100 论文**："MonetDB/X100: Hyper-Pipelining Query Execution"（Boncz et al., CIDR 2005）；
17. **C-Store 论文**："C-Store: A Column-oriented DBMS"（Stonebraker et al., VLDB 2005）；
18. **Apache StarRocks 论文**（VLDB 2023 demo）；
19. **Pipeline Execution 博客**：starrocks.io/blog/pipeline-execution；
20. **Primary Key 表设计**：starrocks.io/blog/primary-key；
21. **CBO 优化器设计**：starrocks.io/blog/cbo-optimizer；
22. **存算分离白皮书**：starrocks.io/blog/shared-data；
23. **StarRocks 部署最佳实践**：docs.starrocks.io/benchmark/best-practice；
24. **StarRocks 性能调优指南**：docs.starrocks.io/admin-manual/optimization-guide；
25. **StarRocks 数据建模最佳实践**：docs.starrocks.io/data-modeling。
