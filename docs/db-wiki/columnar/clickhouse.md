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

## 二十四、MergeTree part 生命周期与后台合并调度（算法展开）

> 本节与 §7.1 / §7.3 / §7.6 的分工：那几节只给出**结构清单**（有哪些文件、哪些引擎），本节补上**算法本身**。
> 源码路径以 ClickHouse **v24.12.x**（实测 tag `v24.12.1.1614-stable`）公开仓库为准；本仓库 `reference/columnar/clickhouse/` 子模块未 init，故一律**只给路径、不给行号**。

### 24.1 part 的身份：目录名就是它的元数据

part 是一个**不可变目录**，目录名编码了它的全部身份信息。v1 命名格式由 `src/Storages/MergeTree/MergeTreePartInfo.cpp` 的 `getPartNameV1()` 生成：

```
{partition_id}_{min_block}_{max_block}_{level}[_{mutation}]
例：202401_11_27_0          ← 分区 202401，块区间 [11,27]，层级 0（尚未被合并过）
    202401_11_28_1_3        ← 同一分区，合并到 level 1，mutation 序号 3
```

四个字段各有用途：

- `partition_id`：分区键表达式的求值结果（如 `toYYYYMM(ts)` → `202401`），决定 part 落在哪个目录；
- `min_block` / `max_block`：**块号（block number）区间**，表示 part 在全局单调递增插入序列上占的连续位置。合并后的 part 会消耗掉被合并掉的块号区间；
- `level`：合并树的层高，第 0 次合并产出 level 1，level 2 由两个 level 1 合并而来……**level 就是 LSM 合并树的深度**；
- `mutation`：mutation（`ALTER UPDATE/DELETE`）产物的版本序号；普通 INSERT part 不带这一段。

由此得到一条关键不变量：**同一分区内的 part 按 `min_block` 排序即为主键的排序**（因为 part 内的数据已按主键有序，而相邻 part 的主键区间要么相接、要么相交）。合并任务因此永远是**连续区间**的合并，而不是任意集合的合并——这是 MergeSelector 只在 `PartsRange` 上工作的根本原因。

### 24.2 写入：临时 part → 正式 part

一次 `INSERT` 的写入路径由 `src/Storages/MergeTree/MergeTreeDataWriter.cpp` 驱动：

```
INSERT block
   │
   ├─ 按分区键计算 partition_id，分配 block_number
   ├─ 构造 MergeTreePartInfo(part_id, block_number, block_number, level=0)
   ├─ part_dir = temp_prefix + part_name      ← 临时目录（tmp_path 前缀）
   ├─ new_data_part->is_temp = true
   ├─ 逐列写 *.bin / *.mrk2 / primary.cidx / skp_idx_*.idx / checksums.txt
   ├─ 写 partition.dat / count.txt 等元数据
   └─ 原子 rename：临时目录 → data/<partition_id>/<part_name>/
```

两条要点：

1. **临时 part 与正式 part 落同一套文件格式**，区别只有一个 `is_temp` 标志 + 目录前缀。这样合并逻辑（写新 part）可以完全复用写入逻辑；
2. **用 rename 而非就地改写做原子发布**。ClickHouse 的 part 一旦可见就永远不可变——这正是它不需要 WAL 的原因（详见 §7.5）。

后台合并写临时 part 时同理，入口在 `src/Storages/MergeTree/MergeTask.cpp` 与 `src/Storages/MergeTree/MergedBlockOutputStream.cpp`（后者负责把多个来源 part 的同名列按主键顺序归并成一列输出流）。

### 24.3 合并选择：两级判定 `allow()` 与 `score()`

调度入口是 `src/Storages/MergeTree/MergeTreeDataMergerMutator.cpp` 的 `selectPartsToMergeFromRanges()`。它按**固定级联顺序**依次尝试，选中即止：

```
① TTLDropSelector        整个 part 的 TTL 全部过期 → 直接删掉整块
② TTLDeleteMergeSelector 部分行 TTL 过期 → 合并时删除过期行
③ TTLRecompressSelector  TTL 触发重压缩
④ SimpleMergeSelector    常规的「按体积 + 年龄」选择
```

级联里前三级之所以优先于第四级，是因为 TTL 是**硬约束**（到期数据必须消失），而合并只是资源权衡。

落到 ④ 之后，真正的算法在 `src/Storages/MergeTree/MergeSelectors/SimpleMergeSelector.cpp`，由**两层判定**组成。

**第一层 `allow()`：这一段区间「值得」合并吗？**（决定 `end` 能不能继续往右扩）

```cpp
// 简化版，源文件 src/Storages/MergeTree/MergeSelectors/SimpleMergeSelector.cpp
bool allow(sum_size, max_size, min_age, range_size, partition_size, settings)
{
    // (1) 年龄硬闸门：全段都够老，无条件可合
    if (settings.min_age_to_force_merge && min_age >= settings.min_age_to_force_merge)
        return true;
    // (2) 最少 part 数闸门
    if (settings.min_parts_to_merge_at_once && range_size < settings.min_parts_to_merge_at_once)
        return false;

    // (3) 三个归一化因子，各自映射到 [0,1]
    size_normalized     = mapToUnit(log(1 + sum_size),
                          log(1+min_size_to_lower_base), log(1+max_size_to_lower_base));
    age_normalized      = mapToUnit(min_age,
                          lerp(min_age_at_min_size, min_age_at_max_size, size_normalized),
                          lerp(max_age_at_min_size, max_age_at_max_size, size_normalized));
    num_parts_normalized= mapToUnit(partition_size,
                          min_parts_to_lower_base, max_parts_to_lower_base);

    // (4) base 随「越老 / part 越多」动态下调，但下限是 2
    lowered_base = lerp(settings.base, 2.0, min(1.0, age_normalized + num_parts_normalized));

    // (5) 判定式：合并「对数底数」必须达到 lowered_base
    return (sum_size + range_size * settings.size_fixed_cost_to_add)
             / (max_size + settings.size_fixed_cost_to_add) >= lowered_base;
}
```

第 (5) 步的比值就是源码注释里说的 **base（合并树的对数底数 = 一次合并的「扇出」）**：

$$\text{base} = \frac{\sum |p_i| + n \cdot C}{\max |p_i| + C},\qquad C = \texttt{size\_fixed\_cost\_to\_add} = 5\,\text{MiB}$$

加性常数 `C`（固定合并成本）的意义是：**很小的 part 之间合并也算「有效」**，从而避免小 part 永远攒不起来导致 part 爆炸。实测默认值（`SimpleMergeSelector::Settings`，表级 `merge_selector_*` SETTINGS 可覆盖）：

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `merge_selector_base` | `5.0` | 写放大与 part 数的主旋钮 |
| `size_fixed_cost_to_add` | `5 MiB` | 固定成本 `C` |
| `min_size_to_lower_base` / `max_size_to_lower_base` | `1 MiB` / `100 GiB` | base 下调的体积区间（对数刻度） |
| `min_age_to_lower_base_at_min_size` / `max_age_to_lower_base_at_min_size` | `10 s` / `3600 s` | 小 part 时的年龄区间 |
| `min_age_to_lower_base_at_max_size` / `max_age_to_lower_base_at_max_size` | `10 s` / `30 d` | 大 part 时的年龄区间 |
| `min_parts_to_lower_base` / `max_parts_to_lower_base` | `10` / `50` | base 下调的 part 数区间 |
| `merge_selector_window_size` | `1000` | 只看最近 N 个 part（防止合并永远轮不到新 part） |
| `max_parts_to_merge_at_once` | `100` | 单次合并的 part 数上限（不影响 `OPTIMIZE FINAL`） |
| `min_parts_to_merge_at_once` | `0`（关闭） | 最少凑够几个才合 |

`base` 的**直觉**：连续合并 4 个体积 `5,3,1,1` 的 part，base = `10/5 = 2`，即"树的度是 2"。源码头注释把这件事说得很直白——**合并树完全平衡时写放大正比于 `log_c(数据量)`；退化成单链时正比于数据量本身**。base 就是控制这个形态的旋钮。

**第二层 `score()`：所有候选区间里哪一个最优？**

```cpp
// SimpleMergeSelector.cpp, file-local struct Estimator
static double score(count, sum_size, sum_size_fixed_cost)
{
    return (sum_size + sum_size_fixed_cost * count) / (count - 1.9);
}
```

分母的 `count - 1.9`（**不是** `count - 1`）是刻意的小常数偏置，等价于给"多合一点"一个软奖励，抑制 part 数。源码注释给出了完整的推导假设：*合并耗时 ∝ 区间总字节；查询代价 ∝ 瞬时 part 总数；把"整段时间内 part 数积分"最小化，得到的闭式就是 `sum_size / (count - 1)`*，再加上两项工程修正得到上式。

**区间扩展的搜索过程**（`selectWithinPartition`）：

```
for begin = [尾部 window_size 个 part 起点] .. parts_count:
    if !parts[begin].shall_participate_in_merges: continue
    sum_size, max_size, min_age ← parts[begin]
    for end = begin+2 .. parts_count:                 // 至少 2 个 part
        if end-begin > max_parts_to_merge_at_once: break
        if sum_size > max_total_size_to_merge: break
        累加 parts[end-1]，更新 sum_size / max_size / min_age
        if allow(...): estimator.consider(begin, end, ...)   // 记一个候选
# consider() 内部还做了两个「对齐」修正：
#   ① 若 begin 前一个 part 很大（size_prev_at_left > sum_size × 比例），按 log2 差距缩放 score；
#   ② 若区间尾部小 part 比例过高，从右端剔除（enable_heuristic_to_remove_small_parts_at_right，默认开）
# 最后取 min_score 对应的区间
```

**复杂度**：外层 `O(window_size)`、内层 `O(max_parts_to_merge_at_once)`，即每次选择 `O(W·N)` 次 `allow()` 判定，每次是常数次浮点运算——因为 W、N 都被配置上限钉死（1000 / 100），最坏也只有 10⁵ 级判定，可在毫秒内完成。真正昂贵的是**合并 I/O**，所以选择阶段干脆不用更聪明的搜索。

### 24.4 引擎家族在合并期的差异化算法

同一个 part 区间，`MergeTree` / `Replacing` / `Summing` / `Aggregating` / `Collapsing` 的差别**只体现在合并用的 `IMergingAlgorithm` 上**，实现在 `src/Processors/Merges/Algorithms/`：

| 引擎 | 算法文件 | 同 ORDER BY key 的一组行如何被折叠 |
| --- | --- | --- |
| `MergeTree` | `SortingTransform`（不折叠） | 全部保留 |
| `ReplacingMergeTree(v)` | `ReplacingSortedAlgorithm.cpp` | 保留 `version` **最大**的那一行，其余丢弃 |
| `SummingMergeTree` | `SummingSortedAlgorithm.cpp` | 数值列逐列求和，key 列取一 |
| `AggregatingMergeTree` | `AggregatingSortedAlgorithm.cpp` | 把各行的聚合**状态**逐个喂进 `-Merge` 函数再输出 |
| `CollapsingMergeTree(sign)` | `CollapsingSortedAlgorithm.cpp` | `sign=+1` 与 `sign=-1` 成对抵消；余下的 `1` 保留（未读） |

`ReplacingSortedAlgorithm.h` 的类注释是权威表述：*"For each group of consecutive identical values of the primary key, keeps row with max `version` value."*

由此得到 §10/§21.1 里那条结论的算法解释：**去重是合并的副产品，不是写入时的保证**。写入路径上可以存在任意多份同 key 的行；只有当后台合并把它们收进同一个 part 时，`ReplacingSortedAlgorithm` 才会折叠。所以"不保证实时唯一"。

`AggregatingMergeTree` 的关键在于 `-State` 函数是**可序列化的中间态**（`src/AggregateFunctions/AggregateFunctionState.h`），合并 = 把这些中间态做结合律结合。这也是它能配 `MATERIALIZED VIEW` 做增量预聚合（§7.3.4）而 `SummingMergeTree` 不能表达 `uniq` 的原因。

### 24.5 `OPTIMIZE FINAL` 与写入节流

**`OPTIMIZE TABLE t FINAL`** 走的是 `MergeTreeDataMergerMutator.h` 的 `selectAllPartsToMergeWithinPartition(..., final, ...)`：它**直接把整个分区的所有 part 视作一个区间**交给合并，绕开 `SimpleMergeSelector` 的 `allow()` 判定。表级 SETTINGS `max_parts_to_merge_at_once = 100` 的描述里明写着 *"Doesn't affect OPTIMIZE FINAL query"*——正是这个意思。

- **幂等性**：对一个已完全合并（level 足够高、每个 key 只剩一行）的分区再跑 `FINAL`，要么无事可做（`optimize_throw_if_noop = 0` 时返回），要么只产生 level 相同的新 part；`optimize_skip_merged_partitions = 1` 会跳过"看起来已经合干净"的分区，避免无谓 I/O。
- **`final` 是读取侧的对偶**：`SELECT ... FROM t FINAL` 在读取时用同一个 `Replacing/Sorting` 算法做去重，省掉落盘合并，代价是查询期 CPU/内存放大（§21.2 的"`final` 滥用"陷阱）。
- `MergeTreeDataMergerMutator.cpp` 里还有一条硬约束：常规合并**不允许把单个 part 跟自己合并**（`if (parts_to_merge.size() == 1) throw LOGICAL_ERROR`），只有 `OPTIMIZE FINAL` / TTL 场景才放开。

**写入节流**（`src/Storages/MergeTree/MergeTreeSettings.cpp` 实测默认值）：

| SETTINGS | 默认 | 行为 |
| --- | --- | --- |
| `parts_to_delay_insert` | `1000` | 单分区活跃 part 数达到此值 → 对 INSERT **限速**（不是拒绝） |
| `max_delay_to_insert` | `1` | 单次 INSERT 最多被限速的秒数 |
| `parts_to_throw_insert` | `3000` | 超过此值 → 直接抛 `TOO_MANY_PARTS` |
| `max_parts_in_total` | `100000` | 全表所有分区的活跃 part 总数上限 |
| `max_avg_part_size_for_too_many_parts` | `1 GiB` | 平均 part 体积超过此值时，**前两条检查整体失效**（允许单表单分区 TB 级） |

> ⚠️ 常见误解：所谓 `Too many parts` 的阈值默认是 **1000 / 3000** 这一对，而不是 100。

**并发度与抢占**（24.12 起 `background_pool_size` 等已从 query SETTINGS 迁到 `config.xml`，见 `src/Core/ServerSettings.h`）：

```
merge 槽位数 ≈ background_pool_size(16) × background_merges_mutations_concurrency_ratio(2) = 32
```

抢占靠"空闲槽位水位"这一组阈值实现：空闲槽位少于 `number_of_free_entries_in_pool_to_lower_max_size_of_merge = 8` 时，自动调低单次合并的体积上限，让**小合并有机会插队**；少于 `number_of_free_entries_in_pool_to_execute_mutation = 20` 时干脆**暂缓 mutation**，把线程让给常规合并，避免 part 数失控。

### 24.6 复杂度小结

| 环节 | 时间复杂度 | 说明 |
| --- | --- | --- |
| part 选择 `allow()` 判定 | `O(W·N)`，`W=1000`，`N≤100` | 上限被配置钉死 |
| 合并本身 | `O(总字节数)` | 顺序读写 + k 路归并 |
| 写放大（平衡树） | `≈ log_base(数据量)` 次全量重写 | base 默认 5 |
| 写放大（退化单链） | `O(数据量)` 次全量重写 | 官方注释明确指出这是要避免的形态 |

---

## 二十五、列式压缩编码算法：DoubleDelta / Gorilla / T64（算法展开）

> §7.2 已给出 codec **清单表**，本节补三个最值得手写的**位级编码算法**及其物理帧格式。
> 源码路径同样以 v24.12.x 为准（`reference/columnar/clickhouse/` 子模块未 init，只给路径不给行号）。

### 25.1 物理帧：每个压缩块 9 字节头

理解 codec 链的前提是先理解 `src/Compression/ICompressionCodec.cpp` 里 `compress()` / `decompress()` 写出的通用帧：

```
偏移  长度  内容
 0     1    method_byte            标识 codec（CompressionMethodByte 枚举）
 1     4    compressed_size        UInt32 小端，含头部长度
 5     4    uncompressed_size      UInt32 小端
 9     N    payload                codec 私有数据
 ↑──────── 固定 9 字节头
```

解压时先校验 `source[0] == getMethodByte()`，不匹配立即抛 `CANNOT_DECOMPRESS` 类错误——codec 是**自描述**的，列的 `*.bin` 不需要额外元数据就能解码。

`src/Compression/CompressionCodecMultiple.cpp` 在这之上再包一层，用于实现 `CODEC(T64, ZSTD)` 这样的链：

```
[codecs_count: UInt8][method_byte_1][method_byte_2] ... [method_byte_n][data]
   data 被 codec_1 压过一次 → codec_2 → ... → codec_n
```

即**列表里越靠前 = 越靠近原始数据 = 越"语义"；越靠后 = 越靠近磁盘 = 越"通用"**。

### 25.2 DoubleDelta：二阶差分的变长前缀码

实现在 `src/Compression/CompressionCodecDoubleDelta.cpp`（其文件头注释本身就是完整算法说明）。注意：源码作者特意吐槽了这个名字——**它不是"对 Delta 变换做两次"**，而是 Gorilla 论文里的 Gorilla 双 Delta 编码，扩展到 64 位。

**编码伪代码**：

```
输入：a[0..n-1]
输出：先写 n（4 字节小端），再写 a[0]（sizeof(T) 字节），再写 a[1]-a[0]，最后逐个写 dd

for i in 2 .. n-1:
    dd = a[i] - 2*a[i-1] + a[i-2]        # 等价于 (a[i]-a[i-1]) - (a[i-1]-a[i-2])
    if dd == 0:            write 1 bit  : 0
    elif -63   < dd <  64: write 2 bits : 10   ; 1 sign ;  6 bits |dd-1|
    elif -255  < dd < 256: write 3 bits : 110  ; 1 sign ;  8 bits |dd-1|
    elif -2047 < dd < 2048:write 4 bits : 1110 ; 1 sign ; 11 bits |dd-1|
    elif dd fits in int32: write 5 bits : 11110; 1 sign ; 31 bits |dd-1|
    else:                  write 5 bits : 11111; 1 sign ; 63 bits |dd-1|
```

几个设计细节值得拆开看：

- **`|dd - 1|` 而不是 `|dd|`**：变长前缀码里 `0` 已被单独占用（`0` 与 `10`/`110`/… 的前缀关系决定了后续码字的偏移），用 `|dd|-1` 编码可以把有效值域整体左移一位，为前缀腾出空间；
- **前缀长度单调递增**（1→2→3→4→5）：构成典型的**前缀可辨码**，解码器只需按位读前缀即可定位后续 payload，无需回溯；
- **步长恒定的序列退化成 1 bit/值**：源码给出的例子——`UInt8 [1,2,3,4,5,6,7,8,9,10]` 中后 8 个值的 `dd` 全是 0，编码结果是 `\x0a\x00\x00\x00\x01\x01\x00`（末尾那个 `0x00` 就是 8 个零比特）。

**复杂度**：时间 `O(n)`（每值一次减法 + 至多 1 次 64-bit 位写），空间 `O(1)`（流式 BitWriter）。最优情况常数极小——**恒定步长序列 1 bit/值，即 8:1 的定长压缩**；最差情况（dd 随机大）反而比原值大，此时需要链上第二级 codec（如 `ZSTD`）补救。

**适用场景**：**单调、步长近似恒定**的整数序列——时间戳（毫秒粒度、规则采样）、自增 ID、累计计数器。**不适合**随机 ID（`ReplacingMergeTree` 的 `event_id` 就别挂 DoubleDelta）。

### 25.3 Gorilla：XOR + 前导零窗口复用

实现在 `src/Compression/CompressionCodecGorilla.cpp`（源自 VLDB 2015 的 Gorilla 论文）。核心洞察：**浮点时序序列相邻值的变化很小，XOR 之后只有低位几个 bit 不同**。

```cpp
// src/Compression/CompressionCodecGorilla.cpp（化简）
if (xor == 0) { writer.writeBits(1, 0); }                       // 完全不变，1 bit
else {
    (lzb, tzb, data_bits) = getBinaryValueInfo(xor);            // 前导零/尾随零/有效位数
    if (lzb >= prev_lzb && tzb >= prev_tzb) {                   // 落在上一个窗口内
        writer.writeBits(2, 0b10);                              //   2 bit 前缀
        writer.writeBits(prev_data_bits, xor >> prev_tzb);      //   只补有效位
    } else {                                                    // 新窗口
        writer.writeBits(2, 0b11);                              //   2 bit 前缀
        writer.writeBits(LEADING_ZEROES_BIT_LENGTH, lzb);       //   lzb 本身
        writer.writeBits(DATA_BIT_LENGTH, data_bits);           //   有效位长度
        writer.writeBits(data_bits, xor >> tzb);                //   有效位内容
        prev_lzb = lzb; prev_tzb = tzb;
    }
}
```

**窗口复用的收益**：`lzb/tzb/data_bits` 这三个"元数据"只有在新窗口时才写（2+5+6 = 13 bit）；落在旧窗口内只需 2 bit。对缓慢变化的曲线，绝大部分相邻对都落在旧窗口里，摊薄成本后接近 2 bit/值。

**位宽参数**在 `getBitLengthOfLength()` 里，由数据类型宽度决定：

```cpp
static const UInt8 bit_lengths[] = {0, 4, 5, 0, 6, 0, 0, 0, 7};  // 按 data_bytes_size 索引
DATA_BIT_LENGTH          = bit_lengths[data_bytes_size];
LEADING_ZEROES_BIT_LENGTH = DATA_BIT_LENGTH - 1;
```

即 4 字节（`Float32`/`Int32`）→ `DATA=6, LZ=5`；8 字节（`Float64`）→ `DATA=7, LZ=6`。源码里的上界公式：

$$\text{max\_item\_bits} = 2 + \texttt{LEADING\_ZEROES\_BIT\_LENGTH} + \texttt{DATA\_BIT\_LENGTH} + 8 \cdot \text{bytes}$$

对 `Float32` = `2+5+6+32 = 45 bit`；`Float64` = `2+6+7+64 = 79 bit`。**这也是 Gorilla 不能单独兜底的最坏情形**——必须链一个通用压缩（ZSTD）才划算，与 §7.2 里 `Codec(Gorilla, ZSTD(1))` 的推荐用法一致。

**复杂度**：时间 `O(n)`，空间 `O(1)`（流式）。适用**单调/准单调的浮点序列**（CPU 占用率、内存水位、QPS 指标）。**不适合** NaN/Inf 混杂的序列（XOR 结果会失去窗口局部性，退化为每值 79 bit）。

### 25.4 T64：比特矩阵转置

实现在 `src/Compression/CompressionCodecT64.cpp`。思路完全不同：**不编码数值，只重排 bit 位置**。

对一个 64 值块（64 个 `UInt8` = 64 字节）构造一个 `64×8` 的 bit 矩阵，**行 = 值，列 = bit 位**，然后整体转置成 8 个 `UInt64`：

```cpp
// src/Compression/CompressionCodecT64.cpp
void transpose64x8(UInt64 * src_dst)
{
    const auto * src8 = reinterpret_cast<const UInt8 *>(src_dst);
    UInt64 dst[8] = {};
    for (UInt32 i = 0; i < 64; ++i) {          // i = 值的序号
        UInt64 value = src8[i];
        for (int b = 0; b < 8; ++b)            // b = 该值的第 b 个字节
            dst[b] |= ((value >> b) & 0x1) << i;
    }
    memcpy(src_dst, dst, 8 * sizeof(UInt64));
}
```

转置后，"64 个值各自的高位"被聚到同一个 `UInt64` 里，**全 0 的高位被直接裁掉**。所以源码注释说：*"if we have UInt8 with only 0 and 1 inside, 64×UInt8 will be compressed into 1×UInt64"*（512 bit → 64 bit，8:1）。

**裁剪靠块级 header**：每块先写 `min`（8 B）+ `max`（8 B）共 16 字节，然后

```cpp
num_bits = getValuableBitsNumber(min, max);   // 表示 [min,max] 所需的最少有效位数
// 之后只保留每字节的低 num_bits 位，高位丢弃
```

`min < 0 && max >= 0`（有符号跨零）时额外多存 1 bit 符号位。

宽类型（`sizeof(T) > 1`）走 `transposeBytes<T>()` 做**字节级转置**，与 `transpose64x8` 组合使用；源码还提供两个变体：`Byte`（默认，字节转置 + 末字节按 bit 转置）与 `Bit`（全 bit 转置，配 ZSTD 更好、配 LZ4 更差）。

**复杂度**：`transpose64x8` 是 64×8 = 512 次移位-or，**每处理 64 个值做一次**，即 `O(n·8)` 次定长操作（`8` = 每值的字节数，编译期常量）→ 视为 `O(n)`；空间 `O(1)`（块内 8 个 `UInt64`）。

**适用场景**：**低基数整数**（状态码、地区码、类别 ID 的数值化）。**不适合**均匀分布的 64 位大整数（转置后无冗余可裁）。注意它与 §7.2 表里的 `Dictionary` codec 目标重叠但机制不同：`Dictionary` 显式建码本，T64 只是"按 bit 位置聚合再裁剪"，无需码本。

### 25.5 选型速查与复杂度总表

| codec | 变换 | 最坏位宽 | 典型位宽 | 时间 | 空间 | 最佳场景 |
| --- | --- | --- | --- | --- | --- | --- |
| `DoubleDelta` | 二阶差分 + 前缀码 | 1+5+1+63 = 70 bit | **1 bit**（恒定步长） | `O(n)` | `O(1)` | 单调等步长整数（时间戳、自增 ID） |
| `Gorilla` | XOR + 零位窗口复用 | `2+LZ+DATA+8B`（F32: 45 / F64: 79） | ~2–4 bit | `O(n)` | `O(1)` | 浮点时序指标 |
| `T64` | bit 矩阵转置 + 裁剪 | `n`（无冗余时） | `n·log2(基数)` | `O(n)` | `O(1)` | 低基数整数码 |
| `Delta` | 一阶差分（按 `delta_bytes_size` 分组） | `n + 2B` | ~8 bit | `O(n)` | `O(1)` | 较单调但步长抖动的整数 |
| `LZ4` / `ZSTD` | 通用字节级 | `n + 9B` | 视数据 | `O(n)` | `O(n)` | 链末兜底 |

三点选型经验（都与 §7.2 的示例互相对应）：

1. **codec 是有向链，不是一个集合**。`Codec(DoubleDelta, ZSTD(3))` 表示"先语义变换，再通用压缩"；反过来写会让通用压缩先去吃掉二阶差分里已消失的字节模式，效果打折。
2. **`isCompression() == false` 的 codec（`Delta` / `DoubleDelta` / `Gorilla` / `T64`）不能作为链尾**——它们自身不做熵编码，必须后面跟一个真压缩器，否则最坏情况比不压还大。判断依据是 `ICompressionCodec` 里的 `isCompression()` 虚函数。
3. **压缩率与 CPU 的取舍点**：`ZSTD` 的 level 越高越慢，`LZ4` 反而适合高吞吐写入；`CODEC` 可以按列分别设置——这正是列存相对行存的核心优势，**每列独立选变换**。

---

## 二十六、稀疏主键索引与跳数索引：granule 级查找算法（算法展开）

> §8.1 / §7.4 只描述了"稀疏索引 + 二级索引"的现象，本节给出**查找伪代码**与**索引失效判定**。
> 源码路径以 v24.12.x 为准（`reference/columnar/clickhouse/` 子模块未 init，只给路径不给行号）。

### 26.1 granule：索引的最小不可分割单位

每个 part 的主键索引不是树，而是**一张有序表**：第 `i` 行 = 第 `i` 个 granule 的**首行主键值**。granule 边界由 `index_granularity`（默认 **8192 行**，表级 SETTINGS，`src/Storages/MergeTree/MergeTreeSettings.cpp`）决定。

```
part 目录/
  primary.cidx     ← 主索引：每 granule 一条记录 = 该 granule 首行的主键值
  <col>.bin        ← 列数据
  <col>.mrk2       ← mark 文件：granule → .bin 内 byte offset 的映射（O(1) 跳转）
  skp_idx_*.idx    ← 跳数索引（见 §27.3）
```

配套的 `index_granularity_bytes`（默认 **10 MiB**，设为 0 关闭）提供**自适应粒度**：当压缩率随数据增长而上升时，改按字节而非按行切 granule。实现见 `src/Storages/MergeTree/MergeTreeIndexGranularityAdaptive.cpp`——它不再存"固定行数"，而是存**每 mark 的累计行数** `marks_rows_partial_sums`：

```cpp
// src/Storages/MergeTree/MergeTreeIndexGranularityAdaptive.cpp
size_t getMarkRows(mark_index)  const { return marks_rows_partial_sums[mark_index] - marks_rows_partial_sums[mark_index - 1]; }
size_t getTotalRows()           const { return marks_rows_partial_sums.back(); }
bool   hasFinalMark()           const { return getLastMarkRows() == 0; }   // 末尾 mark 行数为 0 = 哨兵
```

另有 `src/Storages/MergeTree/MergeTreeIndexGranularityConstant.cpp` 实现固定行数粒度。两者统一收敛到接口 `IMergeTreeIndexGranularity`（`getMarkRows` / `getMarksCount` / `getTotalRows` / `hasFinalMark`），上层查找算法对此**完全无感知**。

**两条边界约定**（贯穿所有查找逻辑）：

- 索引表的**最后一条记录代表"右端 +∞"**（`hasFinalMark()` 为真时），因为最后一个 granule 没有"下一个首行值"；
- 主键含 `NULL` 时，稀疏索引里 `NULL` 被映射为 `+∞`（排序取 `NULLS LAST`），保证 granule 区间 `[left, right)` 的单调性不被 NULL 破坏。

### 26.2 主索引查找：二分 vs 栈式排除搜索

真正的查找在 `src/Storages/MergeTree/MergeTreeDataSelectExecutor.cpp` 的 `markRangesFromPKRange()`。它有**两条完全不同的路径**，这是最容易被误解的地方——**ClickHouse 的稀疏索引查找并不总是二分查找**：

```cpp
bool key_condition_exact_range = !key_condition_useful
                              || key_condition.matchesExactContinuousRange();
if (!key_condition_exact_range || !part_offset_condition_exact_range)
    → 路径 A：generic exclusion search（排除法搜索）
else
    → 路径 B：二分查找（binary search）
```

**路径 A：排除法搜索（通用，栈式分裂）**

```cpp
// 简化版，源文件 src/Storages/MergeTree/MergeTreeDataSelectExecutor.cpp
std::vector<MarkRange> stack = { {0, marks_count - (has_final_mark ? 1 : 0)} };
while (!stack.empty()) {
    MarkRange r = stack.back(); stack.pop_back();

    // ① 区间级剪枝：把该区间的 [left, right] 喂给 KeyCondition
    auto verdict = check_in_range(r);
    if (!verdict.can_be_true) continue;                  // 整段可跳过，丢弃

    if (r.end == r.begin + 1) {                          // ② 已细到单个 mark
        if (res.empty() || r.begin - res.back().end > min_marks_for_seek)
            res.push_back(r);                            //    距离远 → 开新段
        else
            res.back().end = r.end;                      //    距离近 → 并入上一段（避免 seek 反而更慢）
        continue;
    }

    // ③ 按 coarse granularity 分裂子段压栈（右到左）
    size_t step = (r.end - r.begin - 1) / merge_tree_coarse_index_granularity + 1;
    for (size_t end = r.end; end > r.begin + step; end -= step)
        stack.emplace_back(end - step, end);
    stack.emplace_back(r.begin, end);
}
```

`check_in_range()` 的判定式就是稀疏索引的全部威力所在——**一个 granule 区间 `[begin, end)` 覆盖的主键值域恰好是 `[index[begin], index[end])`**：

```cpp
// markRangesFromPKRange 内的 check_key_condition lambda（化简）
if (range.end == marks_count) {          // 右端是哨兵 mark → 值域右开边界 = +∞
    left  = field(index, range.begin);
    right = POSITIVE_INFINITY;
} else {
    left  = field(index, range.begin);    // 区间左端首行主键
    right = field(index, range.end);      // 下一 granule 首行主键 = 本 granule 的上界（不含）
}
return key_condition.checkInRange(used_key_size, index_left, index_right, key_types, mask);
```

于是 `WHERE pk BETWEEN a AND b` 的处理变成：把 `[a,b]` 与每个 granule 的 `[left,right)` 求交，无交则整段丢弃。`IS NULL` / `!=` / `IN` 等非连续谓词走同一套机制（`checkInRange` 返回 `BoolMask`，三态 `can_be_true / can_be_false / uncertain`）。

`merge_tree_coarse_index_granularity`（默认 **8**，server SETTINGS）控制每层分裂成几块：设成 1 就退化成纯二分。**为什么默认是 8 而不是 1**——稀疏索引项在内存里是 `Column` 形态，`checkInRange` 会做列比较，批量比较 8 个 mark 比逐个比较更划算（SIMD/缓存友好）。

**路径 B：二分查找（仅当谓词是连续区间）**

```cpp
// src/Storages/MergeTree/MergeTreeDataSelectExecutor.cpp（化简）
searched_left = 0; searched_right = last_mark;
while (searched_left + 1 < searched_right) {              // 找左边界
    middle = (searched_left + searched_right) / 2;
    if (check_in_range(MarkRange(0, middle)).can_be_true) searched_right = middle;
    else                                                    searched_left = middle;
}
result_range.begin = searched_left;
// … 对称地再跑一轮找右边界
```

**复杂度对比**：

| 路径 | 触发条件 | 判定次数 | 说明 |
| --- | --- | --- | --- |
| B 二分 | 谓词 = 连续键区间（`matchesExactContinuousRange()`） | `O(log N)` | 两次二分，`N` = mark 数 |
| A 排除法 | 通用（含 `IS NULL`、低选择性 `IN`、`!=`） | 期望 `O(C·log_C N)`，最坏 `O(N)` | `C = merge_tree_coarse_index_granularity = 8`；最坏发生在几乎所有 mark 都命中时 |

"最坏 `O(N)`" 不是缺陷而是正确性保证：稀疏索引**只能保证不误删**，命中率高时该读还得读。真正决定性能的不是这一步（`N` 相对总行数极小，通常 `行数/8192`），而是随后 `O(命中行数)` 的解码。

**`_part_offset` 伪列**也走同一条路径：它的"索引值"就是 mark 对应的起始行号与 part 名，`check_part_offset_condition()` 用 `getMarkStartingRow(range.begin)` / `getMarkStartingRow(range.end) - 1` 作为左右界——这也是 `_part` / `_part_offset` 过滤能下推到存储层的原因。

### 26.3 跳数索引：注入、查询、失效

跳数索引与主索引是**同一套接口**下的两种实例：主键索引是 `MergeTreeIndexGranularityConstant/Adaptive` 的一种特殊化，而跳数索引是 `IMergeTreeIndex`（`src/Storages/MergeTree/MergeTreeIndices.h`）+ `IMergeTreeIndexAggregator`（写入时逐 block 累积）+ `IMergeTreeIndexCondition`（查询时判定）。

**注册的类型**（`src/Storages/MergeTree/MergeTreeIndices.cpp` 的 `MergeTreeIndexFactory::MergeTreeIndexFactory()`）：

```
minmax | set | bloom_filter | tokenbf_v1 | ngrambf_v1 | hypothesis | vector_similarity
```

每个索引有自己的粒度（DDL 里 `INDEX idx ... GRANULARITY n`），粒度 n 表示**每 n 个主索引 granule**汇总一条索引项。

**写入侧聚合器**（`IMergeTreeIndexAggregator::update(block, pos, limit)`）每读满一个 block 就产出一个 `IMergeTreeIndexGranule`，`limit` 保证索引自身不会失控膨胀：

| 类型 | 写入侧聚合 | 查询侧判定 | 典型压缩效果 |
| --- | --- | --- | --- |
| `minmax` | 维护 `[min, max]` 闭区间 | `[min,max] ∩ [lo,hi] = ∅` → 整段丢弃 | 列与排序键相关性高时极强；与排序键负相关时**几乎无效** |
| `set(N)` | 收集去重值集合，超 N 即**标记为过载** | 谓词值与集合无交 → 丢弃；**过载则必不丢** | 低基数枚举列 |
| `bloom_filter` | 若干 hash 函数 + 位图 | 谓词值不在位图 → 丢弃（有假阳性） | 高基数等值；`false_positive` 参数控制位图大小 |
| `tokenbf_v1` / `ngrambf_v1` | 文本按 token / N-gram 灌入位图 | 同上，但作用在分词/字符组上 | `LIKE '%xx%'`、`hasToken()` |
| `hypothesis` | 在线统计，判是否存在反例 | 若存在反例则**放弃剪枝** | 无压缩收益，换取不误删 |

**`set` 的过载保护是本节最值得记住的细节**（`src/Storages/MergeTree/MergeTreeIndexSet.cpp`）：

```cpp
// 写入侧：MergeTreeIndexAggregatorSet::update
if (max_rows && size() > max_rows) { /* 停止收集，标记过载 */ }

// 查询侧：MergeTreeIndexConditionSet::mayBeTrueOnGranule
size_t size = granule.size();
if (size == 0 || (max_rows != 0 && size > max_rows))
    return true;                    // ← 空 granule 或过载：一律「可能命中」，绝不剪枝
if (!condition.checkInHyperrectangle(granule.set_hyperrectangle, ...).can_be_true)
    return false;                   // 只有确定无交才允许剪枝
```

也就是说，**`set(N)` 的 N 是"剪枝能力"与"索引体积"之间的开关**：N 越大索引越小、但过载越频繁、剪枝越弱。**没有 N（0）表示不限，唯一值全存**。这是"索引不能用来做唯一性约束"的技术原因。

`set` 的数据结构是 `set_hyperrectangle`（多列等值谓词的**超矩形**），所以 `set` 索引天然支持 `WHERE a = 1 AND b = 'x'` 的联合等值判定。

### 26.4 三条索引的失效判定与复杂度

| 判定 | 主键索引 | `minmax` | `set` | `bloom_filter` |
| --- | --- | --- | --- | --- |
| 判定输入 | granule `[left,right)` 键值域 | `[min,max]` 区间 | 值集合 / 超矩形 | 谓词值 hash 后的位图位 |
| 剪枝条件 | `checkInRange` 三态 | 区间无交 | 集合无交且未过载 | 位未命中 |
| 单次判定代价 | `O(k)`，`k` = 参与谓词的主键列数 | `O(1)` | `O(集合大小)` 或 `O(k)`（超矩形） | `O(1)` × hash 函数个数 |
| 最坏情况 | 命中率高时全部 mark 都要读 | 列与排序键无关时全不剪 | 过载时全不剪 | 假阳性率高时近似不剪 |

**通用失效判据**（工程经验，与 §21.2 的性能陷阱呼应）：

1. **排序键前缀单调性是主索引的前提**——`ORDER BY` 之后追加的列只有在**全局单调**时才能被稀疏索引有效利用，否则稀疏索引退化为"每 granule 一个常量"，剪枝率归零；
2. **跳数索引对与排序键相关性弱的列无效**——`minmax` 在乱序列上等价于无用；
3. **跳数索引对 `IS NULL` 无效**（值域下界恒为 NULL），这是文档反复强调"高基数列用 bloom_filter 而低基数列用 set"的原因；
4. **跳数索引不参与 `FINAL` 的去重判定**，也**不参与 mutation 的谓词下推**，它只是纯粹的读优化旁路。

> **可借鉴到本项目**：`engineering/src/db/btreeam.h` 的 BTree AM 是**聚簇**索引（每个键都存一个 entry），与 MergeTree 的稀疏索引是互补关系——若在 BTree AM 上叠加一层"每 N 页记一次页级 `[min,max]`"的 minmax 摘要，即可零成本获得 §27.3 里 `minmax` 的剪枝能力；其数据结构与 `MergeTreeIndexGranularityConstant` 完全同构。

## 二十七、参考资料

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
