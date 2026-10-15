# ClickHouse Wiki（OLAP 列存数据库事实标准）

> 参考版本：ClickHouse 当前 commit `a80f9ebcfd11ef96e5e8d0d1c6f6e1c9e5b2b18a`（v24.x 系列），C++ 实现，MergeTree 引擎家族，列式存储，完整 SQL 支持。

## 一、项目定位

ClickHouse 是 **Yandex** 开源的 **OLAP 列式数据库**，以**极致的单表查询性能**和**丰富的 MergeTree 引擎家族**著称，是 **AP（分析型）场景的事实标准之一**。它把"宽表 + 列存 + 向量化执行 + LSM-style 后台合并"做到极致，单机可处理 **每秒数亿行**的写入与 **PB 级**的数据。

- **痛点**：
  - 传统 OLAP（如 Teradata、Greenplum）成本高、扩展困难；
  - Hadoop/Hive 实时性差，秒级响应不可达；
  - 通用数据库（MySQL/PG）在大宽表聚合查询时退化严重；
  - 需要 **Clickstream** / **日志** / **物联网遥测** / **电商行为** / **广告曝光** 等典型 OLAP 场景；
  - 需要 **MySQL/PG 协议兼容**但底层为列存的混合负载；
- **典型用户**：数据分析团队、监控系统、广告平台、物联网平台、游戏数据分析、电商业务分析、BI 平台；
- **核心口号**：**"Fast as hell, OLAP onClick"** + **"True Column-Oriented DBMS"** + **"Real-Time Analytics"**。

## 二、发展历史与社区

- **起源**：2009 年由 **Yandex** 内部开发，用于 **Yandex.Metrica**（网站流量分析）；
- **里程碑**：
  - 2009：内部项目，最初叫 **"OlapServer"**；
  - 2011-2013：第一个公开版本，支持基础列存；
  - 2014-06：1.0 发布，开源（License: Apache 2.0）；
  - 2016：1.x 大版本稳定，引入 `MergeTree` / `ReplacingMergeTree` / `SummingMergeTree`；
  - 2017：加入 `Kafka` 表引擎、字典（dictionaries）、`JOIN` 改进；
  - 2018：18.x 系列，重写 `LowCardinality` 类型、加入 `Live View`（已废弃）；
  - 2019：19.x 系列，`final` 优化、`AggregatingMergeTree` 大幅改进；
  - 2020：20.x 系列，加入 **`MaterializedView`** 增强、`LIVE VIEW` 重写；
  - 2021：21.x 系列，加入 **`Executable` 表引擎**、**`Hive` 表引擎**；
  - 2022：22.x 系列，加入 **`WindowView`**、**`EXPLAIN PIPELINE`**；
  - 2023：23.x 系列，加入 **`KeeperMap`**、**`TimeSeries` 表引擎**（实验）、**`JSON` 类型**；
  - 2024-2025：24.x 系列，**`MergeTree` 索引增强**、**向量化执行更广泛**、**`AsyncInsert` 改进**；
- **维护状态**：极活跃（每年 4-6 个 stable 版本），Yandex → ClickHouse Inc.（美国子公司）持续维护；
- **社区**：GitHub 35k+ stars、5000+ commits/year、Slack 大型社区、ClickHouse Meetups 全球、CFP 演讲丰富。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）；
- **商业产品**：
  - **ClickHouse Cloud**（官方云托管，AWS/GCP/Aliyun）；
  - **Altinity**（ClickHouse 商业支持 + Altinity.Cloud）；
  - **ClickHouse, Inc.**（提供 Enterprise 特性如审计、Kerberos、RBAC 增强）；
- **生态关系**：
  - 与 **Kafka / RabbitMQ**（表引擎 `Kafka()` `RabbitMQ()`）紧密集成；
  - 与 **MySQL / PostgreSQL / MongoDB**（表引擎 `MySQL()` `PostgreSQL()` `MongoDB()`）可联邦查询；
  - 与 **Prometheus**（`remote_protocols` 接收 + Grafana 展示）；
  - 与 **dbt**（dbt-clickhouse 适配器）；
  - 与 **Superset / Metabase / Grafana** BI 工具。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **Yandex**：Yandex.Metrica（原始场景）；
  - **Cloudflare**：DNS 分析 + DDoS 检测；
  - **Uber**：Uber Analytics Platform（订单 / 行程分析）；
  - **Spotify**：音乐播放数据 / 用户行为；
  - **Twitter (X)**：广告曝光 / 推文分析；
  - **eBay**：商品 / 用户行为分析；
  - **GitHub**：内部监控 + 代码分析；
  - **Bloomberg**：金融数据分析；
  - **Cisco**：网络遥测；
  - **Disney+**：流媒体分析；
  - **腾讯 / 字节跳动 / 美团 / 京东**：日志分析、用户画像、广告归因；
  - **中国云厂商**：阿里云 / 腾讯云 / 华为云 均提供 ClickHouse 托管；
- **行业**：广告、电商、游戏、IoT、监控、金融、社交媒体、内容平台、电信；
- **社区案例**：每年 ClickHouse 官方博客发布数十个用户案例。

## 五、一句话对比定位

> ClickHouse 是 **"列存 + MergeTree 引擎家族 + 向量化执行 + 完整 SQL + 极致单表性能"** 的 OLAP 数据库；与 **Doris（FE/BE、MySQL 协议）**、**StarRocks（CBO + 向量化 + 实时更新）**、**Pinot（Star-Tree + 实时流）**、**Druid（Roll-up + 时序 + 亚秒查询）** 一起构成 **OLAP 列式数据库阵营**。ClickHouse 的差异化是 **"极致单表性能 + MergeTree 灵活引擎家族 + 完整 SQL + MySQL/PG/HTTP 多协议"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                  客户端协议层                                         │
│   - HTTP（默认 8123）/ MySQL（9004）/ PostgreSQL（9005）/ Native（9000）│
│   - clickhouse-client（CLI） / Grafana / Superset / dbt / JDBC       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ 协议解析
┌─────────────────────▼───────────────────────────────────────────────┐
│           Server（daemon）                                            │
│                                                                       │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │  Interpreters：SQL 解析 + AST + Query Analyzer               │  │
│   │  src/Interpreters/Interpreter*.h                            │  │
│   └──────────────────┬──────────────────────────────────────────┘  │
│                      │                                               │
│   ┌──────────────────▼──────────────────────────────────────────┐  │
│   │  Query Pipeline：向量化执行算子 + Pipeline 调度              │  │
│   │  src/Processors/ + src/QueryPipeline/                       │  │
│   └──────────────────┬──────────────────────────────────────────┘  │
│                      │                                               │
│   ┌──────────────────▼──────────────────────────────────────────┐  │
│   │  Storages：MergeTree / Log / Kafka / MySQL / Distributed     │  │
│   │  src/Storages/MergeTree/ + src/Storages/                     │  │
│   └──────────────────┬──────────────────────────────────────────┘  │
│                      │                                               │
│   ┌──────────────────▼──────────────────────────────────────────┐  │
│   │  Columns / DataTypes：列式内存表示 + 压缩编码                 │  │
│   │  src/Columns/ColumnVector.h + src/DataTypes/                 │  │
│   └──────────────────┬──────────────────────────────────────────┘  │
│                      │                                               │
│   ┌──────────────────▼──────────────────────────────────────────┐  │
│   │  Storages/Disk / Compression：LZ4/ZSTD/DoubleDelta/Gorilla  │  │
│   │  src/Compression/CompressionCodec*                          │  │
│   └──────────────────┬──────────────────────────────────────────┘  │
│                      │                                               │
│   ┌──────────────────▼──────────────────────────────────────────┐  │
│   │  Storages/MergeTree：parts / merges / mutations / indexes    │  │
│   │  src/Storages/MergeTree/MergeTreeData*.cpp                   │  │
│   └─────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                      │
                      │ 集群：ClickHouse Keeper (ZooKeeper 替代) + 副本 + 分片
```

### 关键设计取舍

- **列存 + 向量化执行**：内核大量使用 **SIMD**（SSE/AVX/AVX-512）与 **分支预测优化**，每个查询算子一次处理一批（block，~8192 行）；
- **LSM-style 写入**：每次 `INSERT` 产生一个 part（data part），后台 merge 合并小 part 为大 part；
- **多协议入口**：默认 TCP（Native 9000），兼容 MySQL/PG 协议使得 BI 工具零成本接入；
- **可插拔存储引擎**：每个表可以选不同的引擎（MergeTree 家族 + Log + Kafka + Distributed + External + Materialized View）；
- **无全局事务**：ClickHouse 故意不支持跨节点事务（AP 优先），单节点插入通过 ZooKeeper/Keeper 保证幂等（`insert_keeper_fault_injection` 测试）。

## 七、存储引擎（列式专题深化）

ClickHouse 的存储引擎是其核心竞争力，本节重点展开 **列存压缩** 与 **MergeTree 引擎家族**。

### 7.1 核心数据结构

```
+──────────────────────────────────────────────────────────────────+
|                       MergeTree Table                              |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Parts 目录（按 partition 划分）                            │    |
|   │   data/2024-01-01_2024-01-02_0_0_0/                      │    |
|   │   ├── primary.cidx        (主键索引，稀疏)                  │    |
|   │   ├── *.bin               (列存数据，LZ4/ZSTD/DoubleDelta)  │    |
|   │   ├── *.mrk2              (mark 文件，记录 bin 偏移)        │    |
|   │   ├── minmax_*.idx        (分区 min/max，跳过整个分区)     │    |
|   │   ├── skp_idx_*.idx       (二级跳跃索引，如 minmax/set/bloom)│   |
|   │   ├── partition.dat       (分区元数据)                       │    |
|   │   └── checksums.txt       (文件校验)                        │    |
|   └──────────────────────────────────────────────────────────┘    |
|                                                                    |
|   后台 Merge Selector + MergeTask 持续合并小 part → 大 part       |
+──────────────────────────────────────────────────────────────────+
```

- **Data Part**：每次 `INSERT` 产生一个不可变 part（Immutable Part），存储于 `data/<partition>/<part_name>/`；
- **Mark 文件**：每个列文件配套一个 `.mrk2` 文件，记录 `block` 在 `.bin` 中的偏移（O(1) 跳转）；
- **Primary Index**：稀疏主键索引（默认每 8192 行一个 entry），**不保证唯一性**（允许重复）；
- **Part Merge**：后台 `MergeTreeBackgroundExecutor` 持续合并小 part 为大 part（`merge_max_block_size` 控制）。

### 7.2 列存压缩算法全家福

ClickHouse 内置 **20+ 种压缩算法**，可叠加使用（pipeline chain）。

| 算法 | 类型 | 适用场景 | 源码 |
| --- | --- | --- | --- |
| **`NONE`** | 无压缩 | 已压缩数据（外部 codec） | `src/Compression/CompressionCodecNone.h` |
| **`LZ4`** | 字节级 | 默认通用压缩，快 | `src/Compression/CompressionCodecLZ4.h` |
| **`LZ4HC`** | 字节级 | 高压缩比（HC = High Compression） | `src/Compression/CompressionCodecLZ4HC.h` |
| **`ZSTD`** | 字节级 | 高压缩比，慢一些 | `src/Compression/CompressionCodecZSTD.h` |
| **`ZSTD_QAT`** | 字节级 | Intel QAT 硬件加速 | `src/Compression/CompressionCodecZSTDQAT.h` |
| **`Delta`** | 增量编码 | 单调递增整数（时间戳 / counter） | `src/Compression/CompressionCodecDelta.h` |
| **`DoubleDelta`** | 二阶增量 | 稳定步长整数（auto-increment ID） | `src/Compression/CompressionCodecDoubleDelta.h` |
| **`Gorilla`** | XOR + 前导零 | 浮点时序数据 | `src/Compression/CompressionCodecGorilla.h` |
| **`T64`** | 比特打包 | 低基数 64 位类型 | `src/Compression/CompressionCodecT64.h` |
| **`Dictionary`** | 字典编码 | 低基数 string（≤100k 唯一值） | `src/Compression/CompressionCodecDictionary.h` |
| **`RLE`** | 游程编码 | 重复值序列（status=200） | `src/Compression/CompressionCodecRLE.h` |
| **`FPC`** | 浮点压缩 | 浮点数组 | `src/Compression/CompressionCodecFPC.h` |
| **`AES_*`** | 对称加密 | 静态加密 | `src/Compression/CompressionCodecEncrypted.h` |
| **`Multiple`** | 组合 | 链式压缩（如 `Delta+ZSTD`） | `src/Compression/CompressionCodecMultiple.h` |

**典型组合示例**：

```sql
-- 时间戳 + DoubleDelta + ZSTD（金融时序）
Codec(DoubleDelta, ZSTD(3))

-- 低基数 string + Dictionary + LZ4
Codec(Dictionary, LZ4HC(9))

-- Float 指标 + Gorilla + ZSTD
Codec(Gorilla, ZSTD(1))
```

### 7.3 MergeTree 引擎家族

ClickHouse 的 **存储引擎即表引擎**，每个表根据使用场景选择不同引擎。**MergeTree 家族**是 OLAP 主力。

#### 7.3.1 MergeTree（基础）

```sql
CREATE TABLE events (
    ts DateTime,
    user_id UInt64,
    event_type LowCardinality(String),
    payload String
)
ENGINE = MergeTree()
PARTITION BY toYYYYMM(ts)
ORDER BY (ts, user_id)
SETTINGS index_granularity = 8192;
```

- **核心特性**：
  - 不可变 parts + 后台合并（LSM-like）；
  - 主键（ORDER BY）建立稀疏索引（每 8192 行一个 mark）；
  - 分区裁剪（`minmax` 索引跳过整个 partition）；
  - 支持二级跳跃索引（minmax / set / bloom_filter / tokenbf / ngrambf）；
  - 支持 TTL（`TTL ts + INTERVAL 30 DAY`）；
  - 支持 **mutation**（`ALTER DELETE` / `UPDATE`）但非实时（异步重写 part）；
- **源码路径**：
  - `src/Storages/MergeTree/MergeTreeData.h`（核心抽象）；
  - `src/Storages/MergeTree/MergeTreeDataPart.h`（单个 part 表示）；
  - `src/Storages/MergeTree/MergeTreeDataPartCompact.cpp`（紧凑格式）；
  - `src/Storages/MergeTree/MergeTreeDataPartWide.cpp`（宽格式，默认）；
  - `src/Storages/MergeTree/MergedBlockOutputStream.cpp`（写入路径）；
  - `src/Storages/MergeTree/IMergeTreeDataPart.cpp`。

#### 7.3.2 ReplacingMergeTree（去重）

```sql
ENGINE = ReplacingMergeTree(version)
PARTITION BY toYYYYMM(ts)
ORDER BY (user_id, event_id);
```

- **核心特性**：
  - 合并时保留 `version` 最大的行（或 `ORDER BY` 第一个）；
  - 实现"最终一致性"去重，**不保证实时唯一**（合并前可能有重复）；
  - 典型用法：`version` 为时间戳 / 雪花 ID；
  - `final` 关键字强制合并时去重（但仍依赖 merge 进度）；
- **源码路径**：
  - `src/Storages/MergeTree/MergeTreeDataWriter.cpp`（写入路径）；
  - `src/Storages/MergeTree/ReplacingMergeTreeSelector.cpp`（合并选择）。

#### 7.3.3 SummingMergeTree（聚合）

```sql
ENGINE = SummingMergeTree()
PARTITION BY toYYYYMM(ts)
ORDER BY (user_id);
```

- **核心特性**：
  - 合并时自动 `SUM` 同 ORDER BY key 的数值列；
  - 典型用法：**物化计数器**（每个用户的访问次数、点击数）；
  - 仅在合并时聚合，**实时查询仍可能有部分聚合**；
- **源码路径**：
  - `src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp`（合并逻辑）；
  - `src/Storages/StorageSummingMergeTree.cpp`。

#### 7.3.4 AggregatingMergeTree（高级聚合）

```sql
ENGINE = AggregatingMergeTree()
PARTITION BY toYYYYMM(ts)
ORDER BY (user_id);

-- 配合 -State / -Merge 聚合函数
CREATE MATERIALIZED VIEW mv_user_stats
ENGINE = AggregatingMergeTree()
ORDER BY (user_id)
AS SELECT
    user_id,
    sumState(revenue) AS revenue_sum,
    uniqState(user_id) AS user_count
FROM events
GROUP BY user_id;
```

- **核心特性**：
  - 存储 **聚合状态**（`-State` 函数）而非原始数据；
  - 合并时自动 `-Merge`；
  - 典型用法：**预聚合**、**用户画像**、**实时 dashboard**；
  - 配合 `AggregatingMergeTree` 的 `MATERIALIZED VIEW` 形成"实时数仓"；
- **源码路径**：
  - `src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp`（合并时触发 -Merge）；
  - `src/AggregateFunctions/AggregateFunctionState.h`（-State 函数序列化）。

#### 7.3.5 其他家族成员

| 引擎 | 用途 |
| --- | --- |
| **`CollapsingMergeTree`** | 基于 sign 列折叠（已读 = +1，未读 = -1） |
| **`VersionedCollapsingMergeTree`** | 同上 + version 列 |
| **`GraphiteMergeTree`** | Graphite 时序数据库专用（Path / Value / Time） |
| **`TimeSeries`** | 23.x+ 实验，专为时序设计 |
| **`Buffer`** | 内存 buffer，定期 flush 到目标表 |
| **`Distributed`** | 集群分片（本地表代理） |
| **`MaterializedView`** | 实时 ETL |
| **`Live View`** | 实时查询（已废弃） |
| **`WindowView`** | 时间窗口聚合 |

### 7.4 二级跳跃索引（Skipping Index）

ClickHouse 主索引是稀疏的，**二级跳跃索引**用于补充：

| 类型 | 用途 | 源码 |
| --- | --- | --- |
| **`minmax`** | 范围过滤 | `src/Storages/MergeTree/MergeTreeIndexMinMax.cpp` |
| **`set`** | 等值枚举 | `src/Storages/MergeTree/MergeTreeIndexSet.cpp` |
| **`bloom_filter`** | 高基数等值 | `src/Storages/MergeTree/MergeTreeIndexBloomFilter.cpp` |
| **`tokenbf`** | 文本 token | `src/Storages/MergeTree/MergeTreeIndexBloomFilterText.cpp` |
| **`ngrambf`** | 文本 N-gram | `src/Storages/MergeTree/MergeTreeIndexBloomFilterNgram.cpp` |
| **`hypothesis`** | 概率统计 | `src/Storages/MergeTree/MergeTreeIndexHypothesis.cpp` |

### 7.5 存储与刷盘

- **WAL**：ClickHouse **没有传统意义的 WAL**（LSM 风格下 data part 即"日志"）；
- **Atomic Insert**：通过 Keeper 协调（`insert_keeper_fault_injection` 测试）保证幂等；
- **Mutation**：异步重写（`ALTER UPDATE` / `DELETE` → mutation task → background merge）；
- **Replica**：通过 `ZooKeeper` / **`ClickHouse Keeper`**（自带 ZK 替代品）同步日志（`replication_queue` + `replica_log`）；
- **冷热分层**：24.x+ 通过 `Storage Policy` + `Volume` + `Disk` 支持 S3/HDFS 冷存储（`MergeTree` cold volume）。

### 7.6 存储源码路径速查

- 主表抽象：`src/Storages/MergeTree/MergeTreeData.h:1`；
- Part 写入：`src/Storages/MergeTree/MergedBlockOutputStream.cpp:1`；
- 后台合并：`src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp:1`；
- Compaction 调度：`src/Storages/MergeTree/MergeTreeBackgroundExecutor.cpp`；
- Mutation：`src/Storages/MergeTree/MutateTask.cpp`；
- Replication：`src/Storages/StorageReplicatedMergeTree.cpp`（**核心，~6000 行**）；
- Keeper 集成：`src/Storages/MergeTree/Coordination/*` + `src/Coordination/KeeperServer.cpp`。

## 八、索引机制（简化）

ClickHouse 的索引体系不是其核心，**主要靠 MergeTree 引擎 + 列存 + 向量化执行**达成极致性能。本节仅简述核心索引。

### 8.1 主索引（Primary Index）

- **稀疏索引**：默认每 8192 行一个 mark（`index_granularity`）；
- **存储位置**：`<part_dir>/primary.cidx`；
- **作用**：通过二分查找定位 mark，配合 `.mrk2` 跳转到 `.bin` 文件 offset；
- **不保证唯一性**：允许 ORDER BY 重复（与 B+Tree 不同）。

### 8.2 跳跃索引（Skipping Index）

详见 7.4 节，是主索引之外的二级过滤。

### 8.3 数据跳过索引

- **分区裁剪**：`PARTITION BY toYYYYMM(ts)` 配合 `WHERE ts >= '2024-01-01'` 可跳过整月分区（通过 `minmax_*.idx`）；
- **Projections**：23.x+ 实验，投影存储（类似 materialized view 但在 part 内）。

### 8.4 源码路径

- 主索引构造：`src/Storages/MergeTree/IMergeTreeDataPart.cpp`（`loadPrimaryIndex` 方法）；
- 跳跃索引：`src/Storages/MergeTree/MergeTreeIndex*`（20+ 文件）；
- 索引合并：`src/Storages/MergeTree/MergedBlockOutputStream.cpp` 的 `writeSkipIndices` 方法；
- Projections：`src/Storages/MergeTree/MergeTreeProjection*`（24.x 新增）。

## 九、查询 / 计算层

### 9.1 SQL 方言

ClickHouse SQL **高度兼容标准 SQL**，同时扩展了大量 OLAP 函数。

- **SELECT**：标准 `SELECT ... FROM ... WHERE ... GROUP BY ... ORDER BY ... LIMIT`；
- **JOIN**：支持 `INNER / LEFT / RIGHT / FULL / CROSS / SEMI / ANTI`；
- **WITH**：CTE（Common Table Expression）；
- **WINDOW**：窗口函数（`ROW_NUMBER / RANK / LAG / LEAD / SUM OVER`）；
- **子查询**：标量 / IN / EXISTS / correlated；
- **Lambda**：`() -> expr`（高阶函数）；
- **类型**：UInt8/16/32/64/128/256、Int8/16/32/64/128/256、Float32/64、Decimal、String、FixedString、UUID、Date、Date32、DateTime、DateTime64、Enum、Array、Tuple、Map、Nullable、LowCardinality、JSON（24.x）。

### 9.2 执行模型

```
SQL → Parser → AST → Analyzer → QueryPlan → Pipeline → Processors → MergeTree Read
       │         │       │          │            │           │
       │         │       │          │            │           └─ 拉取 part 块
       │         │       │          │            └─ Expression/Aggregating/Joining
       │         │       │          └─ 优化：filter pushdown、prewhere、projection
       │         │       └─ 类型检查 + 权限检查
       │         └─ 重写 + 优化（distributed subquery）
       └─ 解析 DDL/DML
```

### 9.3 关键优化技术

- **Prewhere**：读取列前先过滤（按主索引顺序读取最小列集）；
- **Projection Pushdown**：只读必要列；
- **Filter Pushdown**：在 Storage 层先过滤；
- **Aggregate Combination**：本地 + 全局二阶段聚合（`max_threads`、`max_bytes_before_external_group_by`）；
- **Runtime Filter**：JOIN 时下推过滤器；
- **Partition Pruning**：跳过整个分区；
- **Sampling**：概率近似查询（`SAMPLE 0.1`）；
- **Read Tasks**：并行 part 读取（`max_parallel_replicas`、`parallel_replicas_count`）。

### 9.4 客户端协议

- **Native TCP（9000）**：原生高性能二进制协议；
- **HTTP（8123）**：默认 Web 协议，`SELECT ... FORMAT JSON` 直接 REST；
- **MySQL 协议（9004）**：`clickhouse-server --mysql_port=9004`；
- **PostgreSQL 协议（9005）**：`--postgresql_port=9005`；
- **gRPC**：21.x+ 实验，参考 `src/Server/GRPCServer.cpp`；
- **Prometheus Remote**：接收 `RemoteWrite` 协议；
- **Kafka Table Engine**：`CREATE TABLE kafka_queue (...) ENGINE = Kafka(...)`；
- **JDBC / ODBC**：Java / BI 工具集成。

### 9.5 源码路径

- SQL 解析：`src/Parsers/Parser*.cpp`（200+ 文件）；
- Analyzer：`src/Interpreters/ExpressionAnalyzer.cpp` + `src/Analyzer/`（23.x+ 新 analyzer）；
- Pipeline：`src/QueryPipeline/QueryPipelineBuilder.cpp` + `src/Processors/`;
- 算子：`src/Processors/Transforms/*Transforms.cpp`；
- 执行：`src/Processors/Executors/ExecutionThreadContext.cpp`；
- 聚合：`src/AggregateFunctions/AggregateFunction*.h`（100+ 文件）；
- JOIN：`src/Interpreters/JoinSwitcher.h` + `src/Processors/Transforms/JoinTransform.cpp`。

## 十、事务与一致性

ClickHouse **故意放弃强事务**——OLAP 场景不需要。提供了几类一致性语义：

- **单节点插入原子性**：单 `INSERT` 内保证原子（多 block）；
- **跨节点插入**：通过 Keeper 协调，最终一致；
- **`OPTIMIZE FINAL`**：强制合并，可能阻塞生产；
- **`SELECT FROM ... FINAL`**：读取时合并，但有性能代价；
- **`ASOF JOIN`**：时序场景的近似 JOIN（不要求严格相等）；
- **无 ACID 跨节点事务**（by design）；
- **Replication**：副本间通过 Keeper 的 `replication_queue` + `replica_log` 同步（异步 + 最终一致）。

## 十一、复制与高可用

### 11.1 ReplicatedMergeTree

- **副本数**：通过 Keeper 的 znode `/replicas/{replica_name}` 协调；
- **副本协议**：每个 part 写入后通知 Keeper，其它副本拉取（leaderless）；
- **故障切换**：任意副本可写（与 Kafka 类似）；
- **数据完整性**：`checksums.txt` 校验 part；
- **源码路径**：
  - `src/Storages/StorageReplicatedMergeTree.cpp`（**核心，~6000 行**）；
  - `src/Storages/MergeTree/ReplicatedMergeTreeQueue.cpp`；
  - `src/Storages/MergeTree/ReplicatedMergeTreeLogEntry.cpp`；
  - `src/Coordination/KeeperServer.cpp`（Keeper 实现，类 etcd/raft）。

### 11.2 ClickHouse Keeper（ZK 替代）

- **背景**：早期依赖 ZooKeeper，但 ZK 维护复杂且性能不足；
- **Keeper**：21.x+ 引入，自带 Raft 实现（`src/Coordination/KeeperServer.cpp`）；
- **优势**：与 ClickHouse 版本同步发布，运维简单；
- **源码路径**：`src/Coordination/KeeperServer.cpp` + `src/Coordination/KeeperStateMachine.cpp`。

### 11.3 多集群与异地容灾

- **异地副本**：通过异步复制实现跨机房（同 cluster 不同 shard）；
- **异地灾备**：典型方案是异地异步副本 + 监控告警；
- **异地多活**：支持但需业务层处理冲突（无内置 CRDT）。

## 十二、扩展性（MPP 专题深化）

ClickHouse 的扩展性是其与传统 MPP 的差异点：**真正的分布式查询是 22.x 才稳定**（`DistributedQueryPipeline`）。

### 12.1 集群架构

```
┌──────────────────────────────────────────────────────────────────┐
│              客户端连接任意 ClickHouse 节点                       │
│                         │                                         │
│                         ▼                                         │
│            ┌────────────────────────────┐                        │
│            │  Coordinator (initiator)   │                        │
│            │  生成 Distributed Query    │                        │
│            │  Plan：parallel_replicas    │                        │
│            └──────┬─────────────────────┘                        │
│                   │ 分发 query plan                              │
│       ┌───────────┼───────────┐                                 │
│       ▼           ▼           ▼                                 │
│   Shard 1     Shard 2     Shard 3                                │
│   (分片)      (分片)       (分片)                                │
│   local       local        local                                  │
│   Replica×3   Replica×3    Replica×3                              │
│       │           │           │                                 │
│       └───────────┴───────────┘                                 │
│                   │ exchange results                              │
│            ┌──────▼─────────────────────┐                        │
│            │  Coordinator merge results│                        │
│            └────────────────────────────┘                        │
│                                                                 │
└──────────────────────────────────────────────────────────────────┘
```

### 12.2 节点角色

ClickHouse **没有强节点角色**，任意节点可作为 coordinator 或 worker：

- **Coordinator（initiator）**：接收客户端请求、生成 plan、合并结果；
- **Worker**：执行本地查询并返回部分结果；
- **Keeper 节点**：运行 ClickHouse Keeper（独立进程，独立集群）；
- **无主控**：任意节点可读可写（最终一致）。

### 12.3 分布式查询实现

- **Distributed 表引擎**：`CREATE TABLE ... ENGINE = Distributed(cluster, db, table, sharding_key)`；
- **sharding_key**：Hash / rand / 自定义函数，决定数据落到哪个 shard；
- **parallel_replicas**：单 shard 内跨副本并行（24.x 成熟）；
- **Distributed Query Pipeline**：22.x+ 重写，生成跨节点的 Exchange 算子；
- **数据分布策略**：
  - **Hash 分布**：`rand()` / `cityHash64(user_id) % num_shards`；
  - **Range 分布**：自定义函数（少见）；
  - **List 分布**：按用户 ID 段（少见）；
- **多阶段聚合**：
  - 阶段 1（local）：每个 shard 在本地预聚合；
  - 阶段 2（global）：coordinator 合并所有部分聚合结果；
- **shuffle**：通过 `remote()` 函数或 `clusterAllReplicas()` 实现；
- **源码路径**：
  - `src/Interpreters/Cluster.cpp`；
  - `src/Storages/StorageDistributed.cpp`；
  - `src/Processors/Executors/PipelineExecutor.cpp`；
  - `src/Planner/`（24.x 新计划器）。

### 12.4 资源隔离

- **资源层级**：
  - **Server**（`max_server_memory_usage`）：全局；
  - **Query**（`max_memory_usage`）：单查询；
  - **User**（`max_memory_usage_for_user`）：单用户；
  - **Quota**（`max_execution_time` / `max_insert_block_size`）；
- **Workload Scheduling**（24.x）：引入 **Workload Entity** 资源组（参考 `src/Interpreters/WorkloadEntityStorage.cpp`）；
- **CPU 调度**：暂未原生支持，基于 OS CFS；
- **多租户**：通过 RBAC + User Quotas 实现。

### 12.5 动态扩缩容

- **扩 shard**：添加新节点 → `SYSTEM RELOAD CONFIG` → 业务层修改 sharding_key 公式；
- **数据重分布**：通过 `clickhouse-copier` 或 `clickhouse-backup` + 手工迁移；
- **缩 shard**：移除节点 → 修改 `remote_servers.xml` → 业务层继续工作（剩余节点承担）。

### 12.6 冷热分层（Storage Policy）

```xml
<storage_configuration>
    <disks>
        <hot_disk><path>/data/hot/</path></disk>
        <cold_disk><path>/data/cold/</path><type>s3</type></cold_disk>
    </disks>
    <policies>
        <hot_to_cold>
            <volumes>
                <hot><disk>hot_disk</disk></hot>
                <cold><disk>cold_disk</disk></cold>
            </volumes>
            <move_factor>0.2</move_factor>
        </hot_to_cold>
    </policies>
</storage_configuration>
```

- **TTL + Move**：通过 `ALTER TABLE ... MODIFY TTL ts + INTERVAL 30 DAY MOVE TO DISK 'cold_disk'`；
- **S3 / HDFS**：`type=s3` 支持 AWS S3、MinIO、Ceph、阿里云 OSS。

## 十三、性能与基准

### 13.1 内部基准

- **ClickBench**（官方）：100 个查询，覆盖典型 OLAP 场景；
- **Star Schema Benchmark (SSB)**：标准 benchmark；
- **TPC-H / TPC-DS**：标准 SQL benchmark；
- **NYC Taxi / GitHub Events / HITS / OnTime**：真实数据集 benchmark。

### 13.2 性能数字（典型）

- **单表扫描**：1 亿行 1 列（UInt64）< 1 秒；
- **聚合**：`SELECT count(), avg(x) FROM 10亿行` < 2 秒；
- **写入**：`INSERT ... VALUES` 单机 ~100k rows/s，`AsyncInsert` 可达 1M rows/s；
- **压缩比**：典型 5-10x（具体看 codec）；
- **冷启动**：10s+（启动时加载元数据 + 检查 parts）。

### 13.3 性能调优

- **配置**：`max_threads`、`max_memory_usage`、`max_parallel_replicas`；
- **表结构**：`ORDER BY` 列顺序最关键（按查询过滤列排）；
- **Codec**：见 7.2 节；
- **Skip Index**：选择列加 `minmax` / `set` / `bloom_filter`；
- **Projections**：大表考虑（24.x）；
- **并行**：开启 `parallel_replicas`，合理设置并发。

## 十四、运维与可观测性

### 14.1 监控

- **Prometheus 集成**：`prometheus_port` + `metrics` 表；
- **Grafana 仪表盘**：官方提供 [clickhouse-grafana](https://github.com/Altinity/clickhouse-grafana)；
- **内置指标**：`system.metrics`、`system.events`、`system.asynchronous_metrics`；
- **Query Log**：`system.query_log`（含执行时间 / 资源 / 错误）；
- **Part Log**：`system.part_log`（合并 / 插入 / mutation）；
- **Text Log**：`system.text_log`（日志聚合表）。

### 14.2 备份与恢复

- **`clickhouse-backup`**（Altinity 开源）：全量 + 增量备份；
- **`BACKUP`**（22.x+ 原生）：`BACKUP DATABASE db TO S3(...)`；
- **`RESTORE`**：对应还原；
- **`freeze` + `cp`**：早期手工快照（`ALTER TABLE ... FREEZE`）；
- **复制方案**：ReplicatedMergeTree + Keeper 自带副本。

### 14.3 升级与滚动

- **大版本**：滚动重启（每个节点 `system stop merges` + 重启 + `system start merges`）；
- **数据兼容性**：新版本读旧版本 part 自动迁移；
- **回退**：保留旧二进制，重启即可读；
- **配置兼容**：`config.d/` 增量配置避免修改全局。

### 14.4 故障排查

- **`system.parts`**：所有 part 状态（`active / outdated / broken`）；
- **`system.replication_queue`**：副本同步延迟；
- **`system.mutations`**：mutation 进度；
- **`system.zookeeper_log`**：Keeper 操作日志；
- **常见问题**：`Too many parts`（合并慢）、`Disk full`、`ZooKeeper session expired`、`Mutation stuck`。

## 十五、生态与集成

### 15.1 客户端

- **C++ Native**：`clickhouse-cpp`（官方）；
- **Python**：`clickhouse-driver`、`clickhouse-sqlalchemy`、`clickhouse-connect`；
- **Go**：`clickhouse-go`（官方）；
- **Java**：`clickhouse-jdbc`（官方）；
- **Node.js**：`@clickhouse/client`（官方）；
- **PHP**：`smi2/phpclickhouse`、`tinderbox/clickhouse-php-client`；
- **Rust**：`clickhouse-rs`；
- **C#**：`Octonica.ClickHouseClient`。

### 15.2 数据集成

- **Kafka**：`Kafka` 表引擎（流式消费）；
- **MySQL**：表引擎 `MySQL(...)` + `MaterializedPostgreSQL`；
- **PostgreSQL**：`MaterializedPostgreSQL` 表引擎；
- **MongoDB**：`MongoDB` 表引擎；
- **S3**：`s3()` 表函数；
- **HDFS**：`hdfs()` 表函数；
- **JDBC**：`jdbc()` 表函数（通过 ODBC bridge）；
- **URL**：`url()` 表函数。

### 15.3 BI 与可视化

- **Grafana**：官方 datasource plugin；
- **Superset**：`clickhouse-sqlalchemy`；
- **Metabase**：官方 driver；
- **Tableau**：通过 JDBC / ODBC；
- **Power BI**：通过 ODBC；
- **DataGrip / DBeaver**：JDBC 集成；
- **Mode / Redash**：HTTP 接口集成。

### 15.4 ETL 与数据建模

- **dbt**：dbt-clickhouse；
- **Airbyte**：`destination-clickhouse` connector；
- **Fivetran / Stitch**：商业 connector；
- **Kafka Connect**：clickhouse-kafka-connect；
- **Spark Connector**：ClickHouse Spark Connector；
- **Apache Beam**：ClickHouse IO。

## 十六、核心论文与算法原理

1. **ClickHouse 官方文档**：[clickhouse.com/docs](https://clickhouse.com/docs) —— 最佳入门；
2. **"The Design and Implementation of ClickHouse"**（公开 PPT）—— Alexey Milovidov 自述架构；
3. **LSM-Tree 原始论文**："The Log-Structured Merge-Tree (LSM-Tree)"（O'Neil et al., 1996）；
4. **"Database Compression"**（Daniel Abadi et al., VLDB 2010）—— 列存压缩原理；
5. **"C-Store: A Column-oriented DBMS"**（Stonebraker et al., VLDB 2005）—— 列存数据库鼻祖；
6. **"MonetDB/X100: Hyper-Pipelining Query Execution"**（Boncz et al., CIDR 2005）—— 向量化执行鼻祖；
7. **"How To Build a Columnar Database Engine"**（Markus Dreseler 等）—— 列存引擎设计范式；
8. **"ClickHouse Keeper: Raft-based Coordination"** —— 官方博客。

## 十七、源码学习路径

### 17.1 必读模块清单（推荐顺序）

| 优先级 | 模块 | 路径 | 说明 |
| --- | --- | --- | --- |
| ★★★★★ | 整体入口 | `src/Server.cpp` | 启动 + 协议监听 |
| ★★★★★ | 协议层 | `src/Server/TCPHandler.cpp`、`HTTPHandler.cpp` | 客户端交互 |
| ★★★★★ | 解析 | `src/Parsers/ParserSelectQuery.cpp` | SQL → AST |
| ★★★★ | Storage | `src/Storages/StorageMergeTree.cpp`、`StorageReplicatedMergeTree.cpp` | 表抽象 |
| ★★★★★ | 合并 | `src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp` | 后台合并核心 |
| ★★★★ | 压缩 | `src/Compression/CompressionCodec*` | 各种 codec |
| ★★★★ | 列存 | `src/Columns/ColumnVector.h`、`ColumnCompressed.h` | 列表示 |
| ★★★★ | 执行 | `src/Processors/Executors/PipelineExecutor.cpp` | Pipeline 调度 |
| ★★★ | 聚合 | `src/AggregateFunctions/AggregateFunctionCount.h` | -State/-Merge |
| ★★★ | 副本 | `src/Storages/MergeTree/ReplicatedMergeTreeQueue.cpp` | 副本协调 |
| ★★★ | Keeper | `src/Coordination/KeeperServer.cpp` | Raft 实现 |

### 17.2 源码规模与语言

- **总 LOC**：~150 万行 C++（核心库）；
- **C++ 占比**：~85%；
- **辅助**：Python（test）、CMake（build）；
- **构建系统**：CMake + Ninja（自研 `CMake` 配置 + 一键 build 脚本）；
- **编译时长**：冷启动 30-60 分钟（多线程）。

## 十八、学习资源

- **官方文档**：[clickhouse.com/docs](https://clickhouse.com/docs)；
- **官方博客**：[clickhouse.com/blog](https://clickhouse.com/blog)（含架构解析 + 用户案例）；
- **GitHub**：[github.com/ClickHouse/ClickHouse](https://github.com/ClickHouse/ClickHouse)；
- **视频**：[ClickHouse 官方 YouTube](https://www.youtube.com/c/ClickHouseDB)、历年 Percona / Data + AI Summit 演讲；
- **书籍**："ClickHouse Cookbook"（早期内部资料）；
- **会议**：ClickHouse 每年举办 **ClickHouse Meetup**（全球）+ Percona Live；
- **认证**：ClickHouse, Inc. 暂未官方认证，Altinity 提供商业培训。

## 十九、可移植性与部署

- **操作系统**：Linux（主）、macOS（开发）、Windows（WSL / Docker）、AArch64 / ARM；
- **容器化**：官方 Docker 镜像（`clickhouse/clickhouse-server`）；
- **Helm Chart**：[Altinity Helm Chart](https://github.com/Altinity/clickhouse-operator)、官方 altinity/clickhouse-operator；
- **Operator**：[Altinity Operator for Kubernetes](https://github.com/Altinity/clickhouse-operator)；
- **Terraform**：第三方模块可用；
- **离线部署**：tar 包 + 配置即可。

## 二十、变体与衍生项目

- **ClickHouse Cloud**（官方云）：托管服务，支持 AWS / GCP / 阿里云；
- **Altinity.Cloud**：商业支持 + 托管；
- **ClickHouse for embedded**：实验（`chdb`）—— 类似 DuckDB 的进程内执行；
- **chDB**：Python 嵌入式 ClickHouse（24.x）；
- **C++ Native SDK**：`clickhouse-cpp`（独立项目，Apache 2.0）；
- **HouseOps / sqlhouse / clickhouse-flamegraph**：第三方工具；
- **国内云**：阿里云 ClickHouse、腾讯云 CDW-ClickHouse、火山引擎 ClickHouse。

## 二十一、风险与陷阱

### 21.1 协议与兼容性陷阱

- **MySQL/PG 协议仅部分支持**：DDL、事务、复杂 JOIN 行为不一致；
- **`ReplacingMergeTree` 不保证实时唯一**：必须配 `final` 或下游去重；
- **`SummingMergeTree` 行为反直觉**：合并时才聚合，实时查询有重复；
- **`ALTER DELETE/UPDATE` 是 mutation**：异步重写 part，可能阻塞生产；
- **Materialized View 的目标表修改**：会断链，需 `SYSTEM RESTART REPLICA`；
- **TTL 删除与冷热分层**：可能导致查询延迟突增；
- **类型歧义**：`Int64` / `UInt64` 转换需 `CAST`，否则溢出；
- **DateTime 精度**：`DateTime64` 时区行为差异；
- **MySQL 客户端连接**：MySQL 8.0 默认 caching_sha2_password 需配置；
- **JDBC 客户端版本**：老版本不支持新功能（如 `JSON` 类型）；
- **keeper 兼容性**：升级 Keeper 版本需重启整个集群；
- **ReplicatedMergeTree 与 Non-replicated 互操作**：禁止跨表类型 JOIN。

### 21.2 性能陷阱

- **`SELECT *`**：拉取所有列，列存优势丧失；
- **`ORDER BY` 选择不当**：稀疏索引失效 → 全 part 扫描；
- **高基数 `ORDER BY`**：mark 跳过的局部性差；
- **未建跳跃索引**：高基数列等值过滤慢；
- **`final` 滥用**：每次查询都合并，性能崩塌；
- **JOIN 大表**：未启用分布式 JOIN 时 OOM；
- **GROUP BY 内存爆炸**：超过 `max_memory_usage` 报错；
- **Array 嵌套**：深嵌套（Tuple(Array(Tuple))) 性能差；
- **LowCardinality 误用**：高基数列反而更慢；
- **`INSERT` 单行**：频繁小写 → part 爆炸 → 合并瓶颈；
- **mutation 大批量**：`ALTER DELETE WHERE` 触发大量 part 重写；
- **Distributed 表 `count()`**：跨 shard 时需要 `sumMerge`；
- **高频 mutation**：导致 part 反复重写；
- **AsyncInsert 异常**：网络抖动时丢数据风险（24.x 改进）；
- **Materialized View 阻塞**：目标表卡顿时上游 INSERT 也阻塞。

### 21.3 运维陷阱

- **`Too many parts`**（默认 100）：写入过快，合并跟不上；
- **磁盘满**：part 不可写，导致 INSERT 失败；
- **ZooKeeper/Keeper 不可用**：副本间无法协调 → 集群分裂；
- **Mutation stuck**：`system.mutations` 卡死导致数据积压；
- **冷热分层数据迁移失败**：S3 权限错误；
- **备份工具限制**：`clickhouse-backup` 不支持所有 Storage Policy；
- **大版本升级**：未滚动重启导致副本不同步；
- **集群节点配置漂移**：`config.d/` 散落各处；
- **监控告警缺失**：未监控 part 数 / merge 进度 / replica lag；
- **TLS 配置错误**：客户端连接失败；
- **RBAC 配置复杂**：用户 / 角色 / 行级权限易错配；
- **CPU 资源调度**：CGroup 配置不当导致节点间干扰；
- **Workload Entity（24.x）**：与 Quota 冲突时优先 Workload；
- **冷启动延迟**：重启后首次查询慢（mark 缓存冷）；
- **跨集群复制（CC）**：基于 `ReplicatedMergeTree + 异步` 的实现，需手动控制延迟。

### 21.4 安全陷阱

- **默认无密码**：`default` 用户无密码（必须改）；
- **明文密码**：配置文件明文（建议 `password_encryption` + 密钥）；
- **SQL 注入风险**：动态查询拼字符串；
- **网络暴露**：HTTP / MySQL / PG 端口对外暴露；
- **Kerberos / LDAP 配置**：企业版特性。

## 二十二、学习路径

### 22.1 入门（1 周）

1. 阅读 [clickhouse.com/docs](https://clickhouse.com/docs) Quick Start；
2. Docker 启动 `clickhouse/clickhouse-server`；
3. 用 `clickhouse-client` 创建第一张 MergeTree 表；
4. 插入 1 千万行测试数据；
5. 运行 `SELECT count() FROM table`、`SELECT avg(x) FROM table`；
6. 阅读 `src/Storages/StorageMergeTree.cpp` 的 200 行核心；
7. 阅读 [ClickHouse 官方 YouTube 入门视频](https://www.youtube.com/c/ClickHouseDB)。

### 22.2 中级（2-4 周）

1. 部署 3 节点 cluster（1 节点配置 shard × 2 replica）；
2. 测试 ReplicatedMergeTree 副本同步；
3. 启用 ClickHouse Keeper 替代 ZK；
4. 学习所有 MergeTree 引擎（Replacing / Summing / Aggregating / Collapsing）；
5. 使用 AggregatingMergeTree + MATERIALIZED VIEW 构建实时 dashboard；
6. 配置 TTL + Storage Policy 冷热分层；
7. 阅读 `src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp`；
8. 阅读 `src/Storages/StorageReplicatedMergeTree.cpp`；
9. 性能测试：`EXPLAIN PIPELINE` + `EXPLAIN ESTIMATE`；
10. 集成 Kafka：`CREATE TABLE kafka_queue ... ENGINE = Kafka(...)`；
11. 启用 Prometheus metrics + Grafana dashboard。

### 22.3 进阶（1-3 个月）

1. 部署多 shard 多副本生产集群；
2. 调优 `max_threads` / `max_memory_usage` / `index_granularity`；
3. 实现跨集群复制（CC）；
4. 启用 Workload Entity 资源隔离；
5. 自研 UDF：`src/Functions/` + 注册；
6. 二次开发：新增压缩算法或 MergeTree 引擎变种；
7. 阅读全部 `src/Storages/MergeTree/`；
8. 阅读 `src/Coordination/KeeperServer.cpp` 的 Raft 实现；
9. 性能基准 vs StarRocks / Doris / DuckDB；
10. 贡献 ClickHouse 社区（PR 新算子、修复 bug）；

## 二十三、与本项目的关联

`engineering/` 中**没有 ClickHouse 实现**，但有以下可借鉴模式：

- **列存压缩思路**可应用到 `engineering/src/db/` 的 `rel.c`（如对时序数据列做 Gorilla 编码）；
- **`MergeTree` 的 LSM-style 后台合并** 与 `engineering/src/db/wal.c` 的 checkpoint 设计思想一致；
- **跳跃索引（Skip Index）** 设计可参考 `engineering/src/db/btreeam.h`（`BTree AM` 中可加 minmax 索引）；
- **稀疏主索引**思路可借鉴到 `engineering/src/db/index/` 模块；
- **Codec 链式压缩** 与 `engineering/src/db/storage_engine.h` 的多模态存储类似；
- **`AggregatingMergeTree` 预聚合**思想可应用到 `engineering/src/db/` 的物化视图模块（如果将来实现）；
- **`clickhouse-cpp` Native 协议** 与 `engineering/apps/db_driver/` 可对比学习；
- **测试模式**：ClickHouse 的 `tests/queries/` 数万条 SQL 测试用例，是高质量测试集范本；
- **异步 Mutation 模式**：`src/Storages/MergeTree/MutateTask.cpp` 的后台任务调度可借鉴到 `engineering/src/db/` 的异步重写；
- **Workload Entity 资源隔离**：可应用到 `engineering/src/db/` 的多租户隔离设计。

## 二十四、参考资料

1. **ClickHouse 官方仓库**：[github.com/ClickHouse/ClickHouse](https://github.com/ClickHouse/ClickHouse)；
2. **官方文档**：[clickhouse.com/docs](https://clickhouse.com/docs)；
3. **官方博客**：[clickhouse.com/blog](https://clickhouse.com/blog)；
4. **ClickHouse Meetups**：[clickhouse.com/events](https://clickhouse.com/events)；
5. **Altinity Operator for Kubernetes**：[github.com/Altinity/clickhouse-operator](https://github.com/Altinity/clickhouse-operator)；
6. **clickhouse-backup**：[github.com/Altinity/clickhouse-backup](https://github.com/Altinity/clickhouse-backup)；
7. **clickhouse-cpp**：[github.com/ClickHouse/clickhouse-cpp](https://github.com/ClickHouse/clickhouse-cpp)；
8. **clickhouse-go**：[github.com/ClickHouse/clickhouse-go](https://github.com/ClickHouse/clickhouse-go)；
9. **clickhouse-jdbc**：[github.com/ClickHouse/clickhouse-jdbc](https://github.com/ClickHouse/clickhouse-jdbc)；
10. **clickhouse-sqlalchemy**：[github.com/xzkostyan/clickhouse-sqlalchemy](https://github.com/xzkostyan/clickhouse-sqlalchemy)；
11. **dbt-clickhouse**：[github.com/ClickHouse/dbt-clickhouse](https://github.com/ClickHouse/dbt-clickhouse)；
12. **ClickHouse for Prometheus**：[github.com/ClickHouse/clickhouse_exporter](https://github.com/ClickHouse/clickhouse_exporter)；
13. **ClickHouse Keeper 协议**：参考 etcd Raft 论文；
14. **LSM-Tree 原始论文**："The Log-Structured Merge-Tree (LSM-Tree)"（O'Neil et al., 1996）；
15. **C-Store 论文**："C-Store: A Column-oriented DBMS"（Stonebraker et al., VLDB 2005）；
16. **MonetDB/X100 论文**："MonetDB/X100: Hyper-Pipelining Query Execution"（Boncz et al., CIDR 2005）；
17. **Database Compression 论文**："Database Compression"（Daniel Abadi et al., VLDB 2010）；
18. **ClickHouse vs StarRocks vs Doris 基准**：DB-Engines、Percona Live 演讲；
19. **MergeTree 系列官方文档**：[clickhouse.com/docs/engines/table-engines/mergetree-family](https://clickhouse.com/docs/engines/table-engines/mergetree-family/mergetree)；
20. **Codec 系列官方文档**：[clickhouse.com/docs/sql-reference/statements/create/codecs](https://clickhouse.com/docs/sql-reference/statements/create/codecs)；
21. **Replication 文档**：[clickhouse.com/docs/engines/table-engines/mergetree-family/replication](https://clickhouse.com/docs/engines/table-engines/mergetree-family/replication)；
22. **Distributed 文档**：[clickhouse.com/docs/engines/table-engines/special/distributed](https://clickhouse.com/docs/engines/table-engines/special/distributed)；
23. **Aggregator 实战**：[clickhouse.com/blog/aggregating_state](https://clickhouse.com/blog/aggregating-state-in-clickhouse)；
24. **Materialized View 实战**：[clickhouse.com/blog/materialized-view-the-better-way](https://clickhouse.com/blog/materialized-views-the-best-way-to-improve-clickhouse-performance)；
25. **Cloudflare 案例**：[clickhouse.com/blog/cloudflare](https://clickhouse.com/blog/)（Cloudflare 在 ClickHouse 上的实践）。
