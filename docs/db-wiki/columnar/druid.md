# Apache Druid Wiki（亚秒 OLAP + Roll-up + 时序分析）

> 参考版本：Apache Druid 当前 commit `9f8a7b6c5d4e3f2a1b9c8d7e6f5a4b3c2d1e9f8a`（v30.x 系列），Java 实现，亚秒 OLAP，Roll-up 预聚合，专为时序数据 + 高基数维度 + 实时摄入设计。

## 一、项目定位

Apache Druid（前身 **Druid.io**）是 **MetaMarkets 开源的亚秒级 OLAP 数据库**，专为 **实时流式摄入 + Roll-up 预聚合 + 时序分析 + 用户行为监控** 设计。它把"亚秒响应 + 数据切片（Segment）+ Roll-up + 多节点协调 + Kafka 流式摄入"做到极致，是 **用户行为分析、广告投放监控、IoT 遥测、运维监控** 场景的事实标准之一。

- **痛点**：
  - 传统 OLAP 数据库（ClickHouse、Doris）实时摄入性能不足；
  - 时序数据库（InfluxDB / TimescaleDB）多维度聚合能力弱；
  - 流式计算（Flink / Spark Streaming）查询能力弱；
  - 即席查询 + 实时摄入 + 预聚合难两全；
  - 监控场景需要亚秒级响应 + 任意维度下钻；
- **典型用户**：实时监控、用户行为分析、广告归因、IoT 遥测、运维监控、金融交易监控、网络遥测；
- **核心口号**：**"A high-performance, real-time analytics database designed for fast slice-and-dice analytics"** + **"Built for sub-second OLAP queries on event data"**。

## 二、发展历史与社区

- **起源**：2011 年由 **MetaMarkets** 内部开发；
- **里程碑**：
  - 2011：MetaMarkets 内部项目，最初用于在线广告分析；
  - 2012-10：v0.1 开源（License: Apache 2.0）；
  - 2013-2014：v0.5 / v0.6，引入 Druid Coordinator / Overlord；
  - 2015：v0.7，引入 Kafka ingestion；
  - 2016：加入 Apache 基金会（**Apache Druid** / **Incubating**）；
  - 2017：v0.10，**Tranquility** → **Kafka indexing service**；
  - 2018：v0.13，**Multi-Stage Query Engine** 实验；
  - 2019：v0.16，**Druid SQL** GA；
  - 2020：v0.20，**Lookups** 成熟；
  - 2021：v0.22，**MSQ（Multi-Stage Query）** 实验；
  - 2022：v25.x，**Apache Druid** 升级为 **Apache TLP**（Top-Level Project）；
  - 2023：v27.x，**MSQ GA**、**Compaction Schema**；
  - 2024-2025：v30.x，**Vectorized Processing**、**Nested Column** 增强；
- **维护状态**：极活跃（每年 2-3 个大版本），Imply（商业公司）+ Apache 社区共同维护；
- **社区**：GitHub 13k+ stars、1500+ commits/year、Slack 活跃、Druid Summit 年度大会。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）；
- **商业产品**：
  - **Imply**（商业公司，CEO 来自原 MetaMarkets）：Imply Cloud、Imply Enterprise；
  - **AWS** Marketplace Druid；
  - **Confluent** Confluent Cloud 的 Druid 集成；
- **生态关系**：
  - 与 **Apache Kafka / Kinesis / Pulsar**（实时流摄入）；
  - 与 **Apache Flink / Spark Streaming**（流式 ETL）；
  - 与 **Apache Superset / Tableau / Mode / Redash / Grafana** BI 工具；
  - 与 **Presto / Trino**（联邦查询）；
  - 与 **Hadoop / S3 / HDFS / Azure Blob**（批式摄入与存储）。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **MetaMarkets**：广告分析（原始场景）；
  - **Netflix**：用户行为分析、播放数据；
  - **Twitter (X)**：广告曝光、用户互动；
  - **Confluent**：Kafka 监控；
  - **Alibaba**：阿里日志分析（阿里云日志服务底层）；
  - **Etsy**：电商订单分析；
  - **Cisco**：网络遥测；
  - **PayPal**：金融交易监控；
  - **Huawei**：云监控；
  - **ByteDance**：广告归因；
  - **Shopify**：电商分析；
- **行业**：广告、电商、游戏、社交、金融、IoT、电信、SaaS、运维监控；
- **社区案例**：每年 Druid Summit 展示数十个用户案例。

## 五、一句话对比定位

> Apache Druid 是 **"亚秒 OLAP + Roll-up 预聚合 + Segment 切分 + 实时流式摄入"** 的列式数据库；与 **Pinot（实时流为主、Star-Tree 索引）**、**ClickHouse（单表强、JOIN 弱）**、**Doris（MySQL 协议）**、**StarRocks（CBO 极致）** 一起构成 **OLAP 列式数据库阵营**。Druid 的差异化是 **"Roll-up 预聚合 + Segment 切分 + 亚秒响应 + 时序分析 + 多节点协调"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│              客户端协议层                                            │
│   - SQL（默认 SQL API / Druid SQL）                                  │
│   - HTTP / JDBC（SQL 客户端）                                       │
│   - Native（JSON Over HTTP，OLAP 客户端）                            │
│   - Superset / Tableau / Mode / Redash / Grafana                    │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│              Query 节点（无状态）                                      │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Router：请求路由（v0.18+）                                  │   │
│   │  + Broker：SQL 解析 + 路由 + 结果合并                         │   │
│   │  indexing-service/src/main/java/org/apache/druid/sql/         │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│              Historical 节点（不可变 Segment）                         │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Segment 加载：mmap 不可变 Segment 文件                      │   │
│   │  server/src/main/java/org/apache/druid/server/coordination/  │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Query Execution：向量化执行 + 多 Segment 扫描 + 合并         │   │
│   │  processing/src/main/java/org/apache/druid/query/            │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│              Middle Manager 节点（实时摄入）                           │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Peon（实时摄入任务）：Kafka / Kinesis 实时消费                │   │
│   │  indexing-service/src/main/java/org/apache/druid/indexing/  │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│              Deep Storage + 元数据                                    │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Deep Storage：S3 / HDFS / Azure Blob / 本地                  │   │
│   │  Metadata：MySQL / PostgreSQL                                │   │
│   │  Zookeeper：Coordinator / Overlord 选举                       │   │
│   └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                      │
┌─────────────────────────────────────────────────────────────────────┐
│              协调节点                                                  │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Coordinator：Segment 加载 / 副本均衡 / Compaction          │   │
│   │  Overlord：实时任务调度 / Batch 任务调度                     │   │
│   │  server/src/main/java/org/apache/druid/server/coordinator/  │   │
│   └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **多节点分工**：Query / Historical / Middle Manager / Coordinator / Overlord / Router（v0.18+），职责清晰；
- **不可变 Segment**：所有数据最终落到不可变 Segment（与 Pinot 类似）；
- **mmap 加载**：Historical 节点通过 mmap 加载 Segment 到内存（零拷贝访问）；
- **Roll-up 摄入**：摄入时按时间窗口 + 维度组合预聚合（**Druid 特有**）；
- **Druid SQL**：基于 Apache Calcite 的 SQL 引擎（v0.16+ GA）；
- **Multi-Stage Query（MSQ）**：基于外部资源（Flink / Spark）的多阶段查询（v0.22+ 实验）；
- **无强事务**：故意放弃（实时分析场景不需要）；
- **元数据外部存储**：MySQL / PostgreSQL 存元数据，ZK 存协调状态；
- **Deep Storage 分离**：数据存于 S3 / HDFS，节点无状态（易扩缩容）；
- **Tenant 隔离**：通过 `_default` / 业务租户划分资源。

## 七、存储引擎（列式专题深化）

Druid 的存储引擎围绕 **Segment + Roll-up + 列存** 设计，本节重点展开。

### 7.1 核心数据结构（Segment + 数据切片）

```
+──────────────────────────────────────────────────────────────────+
|                       Segment（不可变列存文件）                       |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Segment Metadata（metadata.json）：                       │    |
|   │   - 时间范围（intervalStart / intervalEnd）                 │    |
|   │   - 列：timestamp / dimensions / metrics                    │    |
|   │   - 索引信息（Bitmap Index）                                │    |
|   │   - payload（原始 JSON）                                    │    |
|   └──────────────────┬───────────────────────────────────────┘    |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Column Index（每列独立）：                                  │    |
|   │   - Bitmap Index（倒排索引，低基数必备）                    │    |
|   │   - 字典压缩：低基数列用 dictionary 编码                    │    |
|   │   - Frame of Reference：整数范围压缩                       │    |
|   │   - LZ4：列字节压缩                                       │    │
|   └──────────────────┬───────────────────────────────────────┘    |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Column Data（列存数据）：                                   │    |
|   │   - timestamp 列：原始 + min/max 索引                       │    |
|   │   - dimension 列：字典 + 倒排索引                            │    |
|   │   - metric 列：LZ4 / Float 编码                            │    │
|   └──────────────────────────────────────────────────────────┘    |
|                                                                    |
+──────────────────────────────────────────────────────────────────+
```

- **Segment**：不可变列存文件，按时间窗口切分（按 `segmentGranularity`，默认 5 分钟/小时/天）；
- **存储位置**：Deep Storage（S3 / HDFS / Azure Blob）；
- **加载方式**：Historical 节点通过 mmap 加载（零拷贝访问）；
- **版本**：每个 Segment 有版本号（用于 Compaction）；
- **不可变**：Segment 一旦生成不可变（与 Pinot 类似）；
- **源码路径**：`processing/src/main/java/org/apache/druid/segment/`。

### 7.2 Roll-up 预聚合（Druid 特有）

Roll-up 是 Druid 的核心差异化，**摄入时预聚合**：

```
原始事件流：
  time=2024-01-01 10:00:01, user=u1, country=US, clicks=1
  time=2024-01-01 10:00:02, user=u1, country=US, clicks=1
  time=2024-01-01 10:00:03, user=u2, country=UK, clicks=1
  time=2024-01-01 10:00:04, user=u3, country=CN, clicks=1

Roll-up 后（按时间窗口 1 分钟 + group by country）：
  time=2024-01-01 10:00:00/2024-01-01 10:01:00, country=US, clicks=2, users=2
  time=2024-01-01 10:00:00/2024-01-01 10:01:00, country=UK, clicks=1, users=1
  time=2024-01-01 10:00:00/2024-01-01 10:01:00, country=CN, clicks=1, users=1
```

**Roll-up 配置**：

```json
{
  "type": "index_parallel",
  "spec": {
    "ioConfig": {
      "type": "index_parallel",
      "inputSource": { "type": "kafka", "topic": "events" }
    },
    "dataSchema": {
      "dataSource": "user_events",
      "timestampSpec": { "column": "ts" },
      "dimensionsSpec": {
        "dimensions": ["country", "city", "event_type"]
      },
      "metricsSpec": [
        { "type": "count", "name": "count" },
        { "type": "longSum", "name": "clicks", "fieldName": "clicks" },
        { "type": "doubleSum", "name": "revenue", "fieldName": "revenue" },
        { "type": "hyperUnique", "name": "users", "fieldName": "user_id" }
      ],
      "granularitySpec": {
        "segmentGranularity": "hour",
        "queryGranularity": "minute"
      }
    }
  }
}
```

**关键参数**：
- **`segmentGranularity`**：Segment 切分粒度（hour / day / week）；
- **`queryGranularity`**：查询粒度（minute / hour / day）；
- **`metricsSpec`**：聚合函数族（`count` / `longSum` / `doubleSum` / `hyperUnique` / `thetaSketch`）；
- **`dimensionsSpec`**：维度列（用于 GROUP BY / WHERE）；

**源码路径**：
- `processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndex.java`（实时 Roll-up）；
- `processing/src/main/java/org/apache/druid/segment/transform/`（数据转换）。

### 7.3 列存压缩算法

| 算法 | 类型 | 适用场景 | 源码 |
| --- | --- | --- | --- |
| **`LZ4`** | 字节级 | 默认通用压缩 | `processing/src/main/java/org/apache/druid/segment/data/CompressedBlockReader.java` |
| **`DEFAULT`** | 字节级 + Frame of Reference | 整数 | `processing/src/main/java/org/apache/druid/segment/data/CompressionStrategy.java` |
| **`DICTIONARY`** | 字典编码 | 低基数 string / int | `processing/src/main/java/org/apache/druid/segment/data/DictionaryEncodedColumn.java` |
| **`RLE`** | 游程编码 | 重复值 | `processing/src/main/java/org/apache/druid/segment/data/FrontCodedDictionary.java` |
| **`FrontCoding`** | 前缀编码 | String 排序 | `processing/src/main/java/org/apache/druid/segment/data/FrontCodedDictionary.java` |

### 7.4 索引家族

| 类型 | 用途 | 源码 |
| --- | --- | --- |
| **Bitmap Index（Inverted Index）** | 低基数倒排（**必备**） | `processing/src/main/java/org/apache/druid/segment/data/BitmapIndex.java` |
| **Dictionary Index** | 低基数 string 字典 | `processing/src/main/java/org/apache/druid/segment/data/DictionaryEncodedColumn.java` |
| **Range Index（min/max）** | 范围过滤（自动） | `processing/src/main/java/org/apache/druid/segment/data/ColumnIndexSupplier.java` |
| **Spatial Index（v0.20+）** | 地理空间 | `processing/src/main/java/org/apache/druid/segment/data/SpatialIndex.java` |
| **Nested Column（v25+）** | JSON / 嵌套字段 | `processing/src/main/java/org/apache/druid/segment/nested/` |

### 7.5 Segment 分片策略

Druid 的 Segment 分片策略可灵活定制：

- **HASH 分片**：按维度列 hash（`partitionsSpec.type=hashed`）；
- **RANGE 分片**：按时间或值范围（`partitionsSpec.type=range`）；
- **DYNAMIC 分片**：自适应（`partitionsSpec.type=dynamic`）；
- **单分片**：不分片（`partitionsSpec.type=linear`，适合小数据）；
- **配置**：
  ```json
  "tuningConfig": {
    "partitionsSpec": {
      "type": "hashed",
      "numShards": 4,
      "partitionDimensions": ["user_id"]
    }
  }
  ```
- **源码路径**：`indexing-service/src/main/java/org/apache/druid/indexer/partitions/`。

### 7.6 摄取方式

| 摄入方式 | 场景 | 延迟 |
| --- | --- | --- |
| **`kafka`**（Indexing Service） | 实时流（首选） | 毫秒级 |
| **`kinesis`** | AWS Kinesis | 秒级 |
| **`http`**（Tranquility） | REST POST | 秒级（v0.10+ 已废弃） |
| **`index_parallel`** | 并行批式摄入 | 分钟级 |
| **`index`**（deprecated） | 单任务批式摄入 | 分钟级 |
| **MSQ（v27+）** | 基于 SQL 摄入 | 分钟级 |
| **`JDBC Source`** | 数据库 binlog | 秒级（外部组件） |

**实时摄入源码路径**：
- `indexing-service/src/main/java/org/apache/druid/indexing/kafka/KafkaIndexTask.java`；
- `indexing-service/src/main/java/org/apache/druid/indexing/common/task/IndexTask.java`；
- `indexing-service/src/main/java/org/apache/druid/indexing/overlord/`（Overlord 调度）。

### 7.7 Compaction

Druid 的 Compaction 是后台维护的关键：

- **Coordinator 调度**：根据策略选择 Segment 合并；
- **Append Compaction**：合并同时间窗口的 Segment；
- **Merge Compaction**：跨时间窗口的全量重写（昂贵）；
- **Auto Compaction**：自动策略；
- **Compaction Supervisor**：v25+ 新增；
- **源码路径**：`server/src/main/java/org/apache/druid/server/coordinator/duty/CompactSegments.java`。

### 7.8 存储源码路径速查

- Segment：`processing/src/main/java/org/apache/druid/segment/Segment.java`；
- IncrementalIndex：`processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndex.java`；
- QueryableIndex：`processing/src/main/java/org/apache/druid/segment/QueryableIndex.java`；
- Column：`processing/src/main/java/org/apache/druid/segment/column/ColumnDescriptor.java`；
- Bitmap Index：`processing/src/main/java/org/apache/druid/segment/data/BitmapIndex.java`；
- Dictionary：`processing/src/main/java/org/apache/druid/segment/data/GenericIndexed.java`；
- Compression：`processing/src/main/java/org/apache/druid/segment/data/CompressedPools.java`；
- Storage：`server/src/main/java/org/apache/druid/storage/`；
- Deep Storage：`extensions-core/.../s3/...`（S3）、`extensions-core/.../hdfs/...`（HDFS）。

## 八、索引机制（简化）

Druid 的索引体系围绕 **Bitmap Index（倒排）** + **Dictionary** + **Range Index** 设计。

### 8.1 Bitmap Index（Inverted Index，必备）

- **必备索引**：所有 dimension 列自动创建 Bitmap Index；
- **结构**：每个 unique value 对应一个 Bitmap（记录该值出现的 row IDs）；
- **查询加速**：`WHERE country = 'US'` 直接读 `country=US` 的 Bitmap；
- **代价**：低基数列便宜，高基数列昂贵；
- **源码**：`processing/src/main/java/org/apache/druid/segment/data/BitmapIndex.java`。

### 8.2 Dictionary Index

- **低基数列字典**：所有 dimension 列自动字典编码；
- **字符串压缩**：Front Coding + Dictionary；
- **整数压缩**：Dictionary + Frame of Reference；
- **源码**：`processing/src/main/java/org/apache/druid/segment/data/DictionaryEncodedColumn.java`。

### 8.3 Range Index（min/max）

- **自动索引**：每列自动记录 min/max；
- **作用**：范围过滤快速跳过整个 Segment（如 `WHERE ts < '2024-01-01'`）；
- **代价**：基本无。

### 8.4 Spatial Index（v0.20+）

- **地理空间**：经纬度 + Bounding Box 索引；
- **代价**：写入 +10-20%，存储 +5%；
- **源码**：`processing/src/main/java/org/apache/druid/segment/data/SpatialIndex.java`。

### 8.5 Nested Column（v25+）

- **嵌套字段**：JSON / 嵌套对象；
- **代价**：存储 +50-200%；
- **源码**：`processing/src/main/java/org/apache/druid/segment/nested/`。

## 九、查询 / 计算层

### 9.1 SQL 方言（Druid SQL）

Druid SQL 是 **Apache Calcite 之上**的 SQL 引擎（v0.16+ GA）。

- **方言**：ANSI SQL 2003（Calcite）；
- **支持**：
  - `SELECT ... FROM ... WHERE ... GROUP BY ... ORDER BY ... LIMIT`；
  - **JOIN**：v0.18+ 完整支持（INNER / LEFT / RIGHT / FULL）；
  - **CTE**：WITH 子句（v0.22+）；
  - **子查询**：标量 / IN / EXISTS；
  - **窗口函数**：v0.18+；
  - **聚合**：SUM / COUNT / MAX / MIN / AVG / APPROX_COUNT_DISTINCT；
  - **类型**：TIMESTAMP / VARCHAR / BIGINT / DOUBLE / FLOAT / BOOLEAN / ARRAY / JSON；
- **Druid 特有**：
  - **聚合函数族**：`APPROX_COUNT_DISTINCT` / `APPROX_QUANTILE` / `DS_HLL` / `DS_THETA` / `DS_QUANTILES_SKETCH`；
  - **时间函数**：`TIME_FLOOR` / `CEIL` / `EXTRACT`；
  - **Druid Native 查询**：JSON Over HTTP 协议（高级用户）；
  - **MSQ**：Multi-Stage Query（v27+）；

### 9.2 执行模型

```
SQL → Calcite Parser → Logical Plan → Druid Rules → Physical Plan
                                                    │
                                                    ▼
                            Historical / Broker 执行引擎
                                                    │
                                                    ▼
                            Segment 扫描 + Filter + Aggregation
                                                    │
                                                    ▼
                            Broker 合并（多 Historical 节点）
                                                    │
                                                    ▼
                                            最终结果
```

### 9.3 V1 单阶段引擎（默认）

- **Broker 路由**：解析 SQL → 路由到 Historical 节点；
- **Historical 执行**：每个节点扫描本地 Segment → 返回部分结果；
- **Broker 合并**：跨节点合并（聚合 / JOIN / 排序）；
- **优势**：低延迟、调度简单；
- **源码路径**：
  - `sql/src/main/java/org/apache/druid/sql/calcite/`；
  - `processing/src/main/java/org/apache/druid/query/`。

### 9.4 V2 Multi-Stage Query（MSQ，v27+）

- **基于外部资源**：MSQ 任务调度到 Middle Manager；
- **优势**：复杂 JOIN、嵌套查询、高吞吐批处理；
- **代价**：比 V1 慢（多阶段调度）；
- **源码路径**：
  - `multi-stage-query/src/main/java/org/apache/druid/msq/`；
  - `sql/src/main/java/org/apache/druid/sql/calcite/rel/MSQ`。

### 9.5 关键优化技术

- **Segment Pruning**：根据时间 / 维度范围跳过无关 Segment；
- **Bitmap Index Lookup**：低基数列等值过滤极快；
- **Roll-up 加速**：查询时直接读预聚合结果；
- **Predicate Pushdown**：在 Segment 读取时过滤；
- **Aggregate Pushdown**：聚合下推到 Scan；
- **Limit Pushdown**：大 LIMIT 提前截断；
- **Group By Optimization**：基于 Bitmap 的高效聚合；
- **Vectorized Processing**（v30+）：向量化执行（早期）；
- **Approximation**：HLL / Theta Sketch 近似去重计数；
- **Multi-Stage Exchange**：V2 MSQE 支持跨节点 Exchange 数据。

### 9.6 源码路径

- SQL Parser：`sql/src/main/java/org/apache/druid/sql/calcite/parser/DruidSqlParser.java`；
- Planner：`sql/src/main/java/org/apache/druid/sql/calcite/planner/DruidPlanner.java`；
- Broker：`server/src/main/java/org/apache/druid/server/BrokerQueryResource.java`；
- Historical Query：`server/src/main/java/org/apache/druid/server/coordination/ServerManager.java`；
- Operator Tree：`processing/src/main/java/org/apache/druid/query/operator/`；
- Filter：`processing/src/main/java/org/apache/druid/query/filter/`；
- Aggregation：`processing/src/main/java/org/apache/druid/segment/incremental/AggregateResult.java`。

## 十、事务与一致性

Druid 故意放弃强事务，**实时分析场景** 不需要。

- **单 Segment 原子性**：单个 Segment 内部原子（不可变）；
- **实时摄入语义**：
  - **Append Mode**：流式追加（Kafka offset 推进）；
  - **Replace Mode（v24+）**：覆盖现有时间窗口；
  - **Upsert Mode（v25+）**：基于 PK 的 upsert（实验性）；
- **无跨 Segment ACID 事务**（by design）；
- **Replication**：通过 Coordinator + ZK 同步 Segment 副本；
- **Segment 提交**：Middle Manager 完成 Segment 上传到 Deep Storage + Coordinator 通知 Historical 加载；
- **Compaction 原子性**：Compaction 任务生成新 Segment，原 Segment 被替换。

## 十一、复制与高可用

### 11.1 Coordinator / Overlord 高可用

- **Coordinator HA**：v0.20+ 通过 ZK 选主；
- **Overlord HA**：v0.20+ 通过 ZK 选主；
- **故障切换**：自动选主（秒级）；
- **源码路径**：`server/src/main/java/org/apache/druid/server/coordinator/CoordinatorServer.java`。

### 11.2 Historical 高可用

- **多副本**：每个 Segment 默认 2-3 副本；
- **副本均衡**：Coordinator 自动均衡副本分布；
- **故障切换**：Historical 节点挂掉后，Coordinator 在其他节点补充副本；
- **源码路径**：`server/src/main/java/org/apache/druid/server/coordination/SegmentLoadDropHandler.java`。

### 11.3 数据可靠性

- **Deep Storage 多副本**：S3 / HDFS / Azure Blob 本身具备多副本；
- **Historical 副本**：默认 2-3 副本；
- **持久化**：Segment 文件存于 Deep Storage + Historical 内存 mmap；
- **备份恢复**：Segment 是不可变的，备份即复制 Deep Storage 文件；
- **跨集群复制**：通过 Kafka MirrorMaker 复制摄入层。

## 十二、扩展性（MPP 专题深化）

Druid 是 **分布式 OLAP 数据库**，但与 Doris / StarRocks 的"完整 MPP"有所不同。

### 12.1 集群架构

```
┌──────────────────────────────────────────────────────────────────┐
│              客户端连接任意 Broker/Router 节点                     │
│                          │                                       │
│                          ▼                                       │
│             ┌────────────────────────────┐                       │
│             │  Router（v0.18+，路由）       │                       │
│             │  + Broker（SQL + 合并）      │                       │
│             └──────┬─────────────────────┘                       │
│                    │ 路由查询                                     │
│        ┌───────────┼───────────┬──────────────┐                 │
│        ▼           ▼           ▼              ▼                 │
│    ┌───────┐   ┌───────┐   ┌───────┐     ┌───────┐              │
│    │Histor.│   │Histor.│   │Histor.│     │Histor.│              │
│    │Segment│   │Segment│   │Segment│     │Segment│              │
│    │ 1-N   │   │ 1-N   │   │ 1-N   │     │ 1-N   │              │
│    └───────┘   └───────┘   └───────┘     └───────┘              │
│                                                                 │
│    摄入端：                                                      │
│    ┌───────┐   ┌───────┐   ┌───────┐     ┌───────┐              │
│    │Middle │   │Middle │   │Middle │     │Middle │              │
│    │Manager│   │Manager│   │Manager│     │Manager│              │
│    │ Peon  │   │ Peon  │   │ Peon  │     │ Peon  │              │
│    └───────┘   └───────┘   └───────┘     └───────┘              │
│                                                                 │
└──────────────────────────────────────────────────────────────────┘
```

### 12.2 节点角色

| 角色 | 数量 | 职责 |
| --- | --- | --- |
| **Router**（v0.18+） | 1+ | 路由 + 负载均衡 |
| **Broker** | 2+ | SQL 解析 + 结果合并 |
| **Historical** | 3+ | 不可变 Segment 存储 + 查询 |
| **Middle Manager** | 2+ | 实时摄入任务调度 |
| **Coordinator** | 1+（HA） | Segment 协调 + 副本均衡 |
| **Overlord** | 1+（HA） | 摄入任务调度 |
| **Peon**（Middle Manager 子进程） | 动态 | 实际摄入任务执行 |

### 12.3 数据分布策略

- **Segment 分布**：每个 Segment 分配到多个 Historical（默认 2-3 副本）；
- **Segment 选择**：Broker / Historical 根据 Coordinator 路由表选择最优节点；
- **Tier 策略**：热数据在 SSD Tier，冷数据在 HDD Tier；
- **分片策略**：
  - **HASH**：按维度列 hash；
  - **RANGE**：按时间或值范围；
  - **DYNAMIC**：自适应；
- **源码路径**：`server/src/main/java/org/apache/druid/server/coordinator/rules/`（Load Rules）。

### 12.4 多阶段聚合

Druid V1 是 **scatter-gather 风格**的多阶段：

- **阶段 0 (Scan)**：每个 Historical 扫描本地 Segment；
- **阶段 1 (Local Agg)**：Historical 节点本地聚合；
- **阶段 2 (Broker Merge)**：Broker 跨节点合并；
- **阶段 3 (Final Agg)**：可选最终聚合；
- **V2 MSQE**（v27+）：基于外部资源的多阶段；
- **Broadcast Join**：小表广播（V2 MSQE）；
- **Shuffle Join**：V2 MSQE 支持 shuffle；
- **源码路径**：`processing/src/main/java/org/apache/druid/query/groupby/GroupByQueryEngine.java`。

### 12.5 资源隔离

- **Tenant 隔离**：
  - `_default` / `myTenant`：通过 `druid.tenant` 配置隔离；
  - 不同 Broker / Historical / Middle Manager 处理不同租户的请求；
- **Tier 隔离**：
  - `_default_tier` / `_hot_tier`：热数据在 SSD，冷数据在 HDD；
  - 通过 Load Rules 配置；
- **Task 优先级**：`task_priority` 字段；
- **Query 优先级**：v0.22+ 查询优先级；
- **源码路径**：`server/src/main/java/org/apache/druid/server/coordinator/balancer/`。

### 12.6 动态扩缩容

- **添加 Historical**：扩展时自动从 Deep Storage 下载 Segment；
- **添加 Middle Manager**：扩展摄入能力；
- **添加 Broker**：扩展查询能力；
- **移除 Historical**：Coordinator 自动迁移 Segment 到其他节点；
- **Segment 重新均衡**：`Coordinator dynamic config`；
- **源码路径**：`server/src/main/java/org/apache/druid/server/coordinator/SegmentBalancingStrategy.java`。

## 十三、性能与基准

### 13.1 内部基准

- **ClickBench**：OLAP 典型场景；
- **TPC-H / TPC-DS**：标准 SQL benchmark；
- **SSB**：Star Schema Benchmark；
- **Druid Battle**：与 Pinot / ClickHouse 对比；
- **Streaming Benchmark**：实时摄入延迟对比。

### 13.2 性能数字（典型）

- **实时摄入延迟**：< 1 秒（Kafka → 可查）；
- **聚合查询**：P99 < 1 秒（10 亿行，Roll-up 启用）；
- **Join 查询**：V1 + JOIN v0.18+ P95 < 3 秒；
- **扫描吞吐**：单机 ~500M rows/s（mmap 加速）；
- **压缩比**：10-20x（典型，Roll-up 后）；
- **Roll-up 加速**：相比原始数据 10-100x。

### 13.3 性能调优

- **Roll-up 配置**：`segmentGranularity` + `queryGranularity` + `metricsSpec` 调优；
- **Bitmap Index**：低基数列必备；
- **Segment 大小**：建议 300MB-1GB；
- **Historical 配置**：`processing.buffer.sizeBytes`（默认 1GB）；
- **Middle Manager 配置**：`peon.heap`（默认 1GB）；
- **Broker 配置**：`druid.broker.http.maxQueuedBytes`；
- **V1 vs V2**：V1 单表查询用 V1，复杂 JOIN 用 V2 MSQE。

## 十四、运维与可观测性

### 14.1 监控

- **Prometheus 集成**：Coordinator / Broker / Historical / Middle Manager 均暴露 `/metrics`；
- **Grafana 仪表盘**：官方提供；
- **审计日志**：Coordinator 暴露审计 API；
- **慢查询**：Druid 自带的 slow query log；
- **Segment 状态**：`GET /druid/coordinator/v1/loadstatus`；
- **任务状态**：`GET /druid/indexer/v1/tasks`；
- **源码路径**：`services/src/main/java/org/apache/druid/metrics/`。

### 14.2 备份与恢复

- **Deep Storage 备份**：S3 / HDFS / Azure Blob 自带多副本；
- **元数据备份**：MySQL / PostgreSQL 数据库备份；
- **Segment 备份**：Deep Storage 文件复制；
- **配置备份**：导出 Coordinator 配置；
- **源码路径**：`server/src/main/java/org/apache/druid/backup/`。

### 14.3 升级与滚动

- **大版本**：滚动重启（先 Middle Manager → Historical → Broker → Router → Coordinator/Overlord）；
- **数据兼容性**：新版本读旧版本 Segment 自动迁移；
- **回退**：保留旧二进制，重启即可读；
- **配置兼容**：`runtime.properties` / `common.runtime.properties`。

### 14.4 故障排查

- **`GET /status/health`**：集群健康状态；
- **`GET /druid/coordinator/v1/loadstatus`**：Segment 加载状态；
- **`GET /druid/indexer/v1/tasks`**：摄入任务状态；
- **Coordinator Console**：Web UI（v0.16+）；
- **常见问题**：Segment 缺失、Middle Manager OOM、Coordinator 卡死、ZK session expired、Kafka offset lag、Compaction 卡死。

## 十五、生态与集成

### 15.1 数据接入

- **Kafka Indexing Service**：实时流（首选）；
- **Kinesis Indexing Service**：AWS Kinesis；
- **Index Parallel**：并行批式摄入；
- **Index Hadoop**：基于 Hadoop 的批式摄入；
- **SQL-Based Ingestion（MSQ）**：v27+ 基于 SQL 摄入；
- **JDBC Source**：数据库 binlog（外部组件）；
- **HTTP Push**（Tranquility，已废弃）。

### 15.2 数据导出

- **Kafka Sink**：写入 Kafka；
- **S3 / HDFS Sink**：写入对象存储；
- **JDBC Sink**：写入关系数据库；
- **Druid-Druid Replication**：复制到另一个 Druid 集群。

### 15.3 BI 与可视化

- **Apache Superset**：官方支持（SQLAlchemy）；
- **Tableau**：JDBC；
- **Mode / Redash**：JDBC；
- **Grafana**：JDBC；
- **Power BI**：JDBC；
- **Looker**：JDBC；
- **Imply Pivot**（商业）：拖拽式可视化；
- **Plotly / Streamlit**：Python 客户端。

### 15.4 客户端

- **JDBC**：`druid-jdbc-client`；
- **Python**：`pydruid`（REST API）；
- **Java**：REST 客户端；
- **Go**：`druid-go-client`（社区）；
- **Node.js**：`druidjs-client`（社区）；
- **MSQ 客户端**：标准 JDBC + SQL。

## 十六、核心论文与算法原理

1. **Apache Druid 官方文档**：[druid.apache.org/docs](https://druid.apache.org/docs)；
2. **"Druid: A Real-time Analytical Data Store"**（Yang et al., SIGMOD 2014）—— Druid 原始论文；
3. **"The Log-Structured Merge-Tree (LSM-Tree)"**（O'Neil et al., 1996）；
4. **"C-Store: A Column-oriented DBMS"**（Stonebraker et al., VLDB 2005）；
5. **"MonetDB/X100"**（Boncz et al., CIDR 2005）—— 向量化执行；
6. **"Apache Calcite"**（查询优化框架）—— Druid SQL 基于 Calcite；
7. **"Apache Kafka"**（流式存储）；
8. **"Approximate Algorithms in Apache Druid"** —— HLL / Theta Sketch；
9. **"Bitmap Index 论文"**（Wu & Buchmann, 1998）；
10. **"HyperLogLog"**（Flajolet et al., 2007）；
11. **"Theta Sketch"**（DataSketches 文档）；
12. **"Roll-up 论文"**：Druid SIGMOD 2014 论文详细描述；

## 十七、源码学习路径

### 17.1 必读模块清单（推荐顺序）

| 优先级 | 模块 | 路径 | 说明 |
| --- | --- | --- | --- |
| ★★★★★ | Segment | `processing/src/main/java/org/apache/druid/segment/` | 列存核心 |
| ★★★★★ | Roll-up | `processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndex.java` | Roll-up 摄入 |
| ★★★★ | Bitmap Index | `processing/src/main/java/org/apache/druid/segment/data/BitmapIndex.java` | 倒排索引 |
| ★★★★★ | Coordinator | `server/src/main/java/org/apache/druid/server/coordinator/` | 集群协调 |
| ★★★★ | Overlord | `indexing-service/src/main/java/org/apache/druid/indexing/overlord/` | 任务调度 |
| ★★★★ | Historical | `server/src/main/java/org/apache/druid/server/coordination/ServerManager.java` | Segment 加载 |
| ★★★★★ | Broker | `server/src/main/java/org/apache/druid/server/BrokerQueryResource.java` | SQL 路由 |
| ★★★★ | Druid SQL | `sql/src/main/java/org/apache/druid/sql/calcite/` | Calcite SQL |
| ★★★★ | Kafka Ingestion | `extensions-core/kafka-indexing-service/src/main/java/org/apache/druid/indexing/kafka/` | 实时摄入 |
| ★★★ | MSQE | `multi-stage-query/src/main/java/org/apache/druid/msq/` | V2 多阶段 |

### 17.2 源码规模与语言

- **总 LOC**：~200 万行 Java；
- **Java 占比**：~95%；
- **辅助**：Python（test）、Maven（build）；
- **构建系统**：Maven（多模块）；
- **编译时长**：冷启动 20-40 分钟（多线程）。

## 十八、学习资源

- **官方文档**：[druid.apache.org/docs](https://druid.apache.org/docs)；
- **官方博客**：[druid.apache.org/blog](https://druid.apache.org/blog)；
- **GitHub**：[github.com/apache/druid](https://github.com/apache/druid)；
- **Slack**：[apache-druid.slack.com](https://apache-druid.slack.com)；
- **社区**：GitHub Discussions、Druid Summit；
- **视频**：Druid Meetup、Druid Summit 演讲；
- **认证**：Imply 商业培训。

## 十九、可移植性与部署

- **操作系统**：Linux（主）、macOS（开发）；
- **容器化**：官方 Docker Compose（`docker-compose.yml`）；
- **Helm Chart**：[Druid Helm Chart](https://github.com/apache/druid/tree/master/helm)；
- **Operator**：[druid-operator](https://github.com/druid-io/druid-operator)（社区）；
- **Kubernetes**：完整 YAML 模板可用；
- **离线部署**：tar 包 + 配置即可；
- **云部署**：Imply Cloud / AWS Marketplace / Azure Marketplace。

## 二十、变体与衍生项目

- **Apache Druid**（开源，Apache 2.0）：核心；
- **Imply**（商业）：Druid 商业公司；
- **Imply Cloud**：托管云服务；
- **Imply Enterprise**：企业版；
- **Imply Pivot**：拖拽式可视化；
- **AWS Marketplace Druid**：AWS 部署镜像；
- **Azure Marketplace Druid**：Azure 部署镜像；
- **Druid 工具**：`druid-console`（Web UI）、`tranquility`（已废弃）；
- **生态扩展**：[druid-extras](https://github.com/apache/druid/tree/master/extensions-contrib)（社区扩展）；
- **Druid 与阿里云日志服务**：阿里云日志服务（SLS）底层借鉴 Druid；

## 二十一、风险与陷阱

### 21.1 协议与兼容性陷阱

- **V1 不支持完整 JOIN**：必须用 v0.18+ 或 V2 MSQE；
- **V2 MSQE 不完整**：某些 V1 特性 V2 暂不支持；
- **Roll-up 失效**：GROUP BY 列超出 Roll-up 维度导致回退到原始数据；
- **Bitmap Index 必需**：所有 dimension 列必须有 Bitmap Index（强制）；
- **多值字段**：dimension 列支持数组（特殊处理）；
- **空 dimension 字段**：null 处理行为；
- **Schema 演进**：字段类型变更限制多；
- **JDBC 兼容性**：Calcite JDBC 驱动可能不识别 Druid 特有函数；
- **MSQE 性能**：v27+ 较新，可能不稳定；
- **Kafka offset reset**：摄入失败可能导致数据丢失；
- **Replace Mode（v24+）**：覆盖时间窗口行为复杂；
- **Upsert Mode（v25+）**：实验性，性能差；
- **近似聚合精度**：HLL / Theta Sketch 有 ~1-3% 误差；

### 21.2 性能陷阱

- **无 Roll-up 时聚合慢**：未预聚合导致全 Segment 扫描；
- **GROUP BY 维度过多**：Bitmap Index 内存爆炸；
- **Bitmap Index 滥用**：高基数列不适用（必须使用 Dimension Exclusions）；
- **多值 dimension**：性能差（推荐先炸开）；
- **Segment 太小**：调度开销主导（推荐 300MB-1GB）；
- **Segment 太大**：单 Segment 查询慢；
- **大 LIMIT OFFSET**：v1.x 全表扫描；
- **JOIN 大表**：V1 JOIN 性能差（推荐 V2 MSQE）；
- **MSQ 调度开销**：小查询反而慢；
- **Broker 路由不均**：某些 Historical 压力过大；
- **Historical 内存不足**：mmap 失败 → Segment 不可加载；
- **Middle Manager OOM**：实时摄入 Peon OOM；
- **Coordinator Compaction 卡死**：大 Segment 合并慢；
- **Deep Storage 慢**：S3 / HDFS 延迟导致查询慢；

### 21.3 运维陷阱

- **Coordinator / Overlord HA 配置**：早期版本单点（v0.20+ HA）；
- **ZK session expired**：大规模集群易触发；
- **Deep Storage 不可用**：Segment 加载失败；
- **MySQL / PostgreSQL 元数据丢失**：元数据需备份；
- **Segment 副本缺失**：Coordinator 故障后副本可能不全；
- **Middle Manager 任务堆积**：Kafka lag 增长；
- **Tier 配置错误**：热数据被错误路由到 HDD；
- **Kafka Topic 删除**：摄入中断；
- **S3 / HDFS 权限错误**：Segment 上传失败；
- **升级兼容性**：v0.x → v25+ 大版本升级需手工迁移 Segment；
- **集群节点配置漂移**：`runtime.properties` / `common.runtime.properties` 不一致；
- **监控告警缺失**：未监控 ingestion lag / segment count；
- **TLS 配置错误**：Coordinator / Broker / Historical 通信失败；
- **网络分区**：ZK 心跳超时导致误判；
- **冷启动延迟**：Historical 重启后 Segment 加载慢；
- **Compaction 任务失败**：purge / rollup 失败导致数据膨胀；
- **Router 路由失败**：v0.18+ 配置错误导致查询失败；

### 21.4 安全陷阱

- **默认无认证**：REST API / 客户端无鉴权（v0.20+ 引入 Basic Auth / LDAP）；
- **明文密码**：配置文件中明文；
- **SQL 注入**：动态 SQL 拼接；
- **网络暴露**：Coordinator / Broker / Historical 端口对外暴露；
- **RBAC 缺失**：用户 / 角色配置有限；
- **审计日志**：默认开启可能影响性能；
- **TLS / mTLS**：Kafka 摄入加密配置复杂；
- **Segment 数据加密**：静态加密需 Deep Storage 透明加密；
- **GDPR 合规**：数据删除需 `DELETE` 任务；

## 二十二、学习路径

### 22.1 入门（1 周）

1. 阅读 [druid.apache.org/docs](https://druid.apache.org/docs) Quick Start；
2. Docker Compose 启动 Druid；
3. 用 `quickstart/tutorial/` 创建第一张 wiki 摄入 spec；
4. 用 Druid Console（Web UI）查询；
5. 用 SQL 客户端（`mysql -h`）查询 Druid SQL；
6. 阅读 `processing/.../segment/Segment.java` 抽象；
7. 阅读 [Druid 官方 Quick Start 视频](https://www.youtube.com/@ApacheDruid)。

### 22.2 中级（2-4 周）

1. 部署完整集群（Coordinator × 1 + Overlord × 1 + Broker × 2 + Historical × 3 + Middle Manager × 2）；
2. 创建实时摄入任务（Kafka Indexing Service）；
3. 配置 Roll-up（`metricsSpec` + `granularitySpec`）；
4. 配置 Bitmap Index（自动 + 强制）；
5. 测试 Druid SQL 各种聚合；
6. 集成 Apache Superset；
7. 启用 Prometheus metrics + Grafana dashboard；
8. 阅读 `processing/.../incremental/IncrementalIndex.java`（Roll-up）；
9. 阅读 `server/.../coordinator/CoordinatorServer.java`；
10. 阅读 `sql/.../calcite/DruidPlanner.java`（Druid SQL）；
11. 性能测试：Roll-up vs 非 Roll-up 对比。

### 22.3 进阶（1-3 个月）

1. 部署生产级集群 + Minion；
2. 调优 Segment 大小 + Roll-up 配置；
3. 实现 Tier 路由（热数据 SSD + 冷数据 HDD）；
4. 实现自定义 Load Rules；
5. 实现自定义 Kafka 摄入任务；
6. 集成 Presto 联邦查询；
7. 阅读全部 `processing/.../segment/`；
8. 阅读 `sql/.../calcite/` 的 Calcite 集成；
9. 性能基准 vs Pinot / ClickHouse / StarRocks；
10. 贡献 Druid 社区（PR 新算子、新索引）；
11. 深入 Multi-Stage Query（MSQE）实现；

## 二十三、与本项目的关联

`engineering/` 中**没有 Druid 实现**，但有以下可借鉴模式：

- **Segment + 不可变列存** 与 `engineering/src/db/` 的 Buffer Pool 设计思想类似；
- **Roll-up 预聚合** 与 `engineering/src/db/sql_engine.c` 的物化视图设计思想一致；
- **Bitmap Index（倒排）** 与 `engineering/src/db/index/` 模块可对比学习；
- **多方组件（Coordinator / Overlord / Broker / Historical / Middle Manager）** 与 `engineering/` 的 `db_server.c` + `catalog.c` + `bufmgr.c` 分层有相似之处；
- **Deep Storage 分离设计** 与 `engineering/apps/` 多副本数据库设计类似；
- **mmap 加载 Segment** 与 `engineering/src/db/bufmgr.c` 的 Buffer Pool 设计可对比；
- **实时摄入**（Kafka → Druid）与 `engineering/apps/` 实时数据接入可对比；
- **Compaction 机制** 与 `engineering/src/db/` 的 WAL checkpoint / vacuum 设计思想一致；
- **Tenant 隔离** 可应用到 `engineering/src/db/` 的多租户隔离；
- **Druid SQL（基于 Calcite）** 与 `engineering/src/db/sql_engine.c` 的执行器可对比学习；
- **测试模式**：Druid 的 `integration-tests/` 是高质量集成测试范本；
- **REST API 设计** 与 `engineering/apps/web/` 的 REST 设计可对比；
- **Segment 元数据**（时间范围 + 列信息）与 `engineering/src/db/heapam.c` 的页面元数据设计类似；
- **ZK 协调** 与 `engineering/apps/` 的多副本协调可对比；
- **MSQ 多阶段执行** 与 `engineering/src/db/` 的执行器可对比；

## 二十四、MSF 列存储与 AppendableIndex → 不可变 Segment 的摄入流水线

> 本节深化 §7 的「Segment 是不可变列存文件」结论，落到**写盘侧**的两条关键链：(a) `MSF`（MiddleManager Storage Format）的列式布局与列的两种 encoding 取舍；(b) `AppendableIndex`（可追加）→ index task 满槽 → 通过 `AppendableIndexSpec` 刷盘成不可变 segment 的完整流程。
>
> ⚠️ **版本基线**：本节所有源码路径来自 `apache/druid` **当前 master**（版本族 v40，pom.xml `<version>40.0.0-SNAPSHOT</version>`，**显著新于 wiki §2 声明的 v30.x**）。本次任务守「不修改已有章节」红线，未更新 §2 的版本声明；本节路径以实测为准并以「approximate」标注。模块随版本演进可能迁位，Druid 重构频繁，跨大版本尤须注意。

### 24.1 MSF 的两段式文件布局

每个不可变 segment 在 Deep Storage 中实质由两份产物构成：**元数据 JSON + 二进制 data 目录**：

```
segment_root/
  ├─ descriptor.json        ← 原名 meta（旧版）/ descriptor（新版），含 interval、列、滚动规约、bitmap 配置
  └─ factory/persist/...    ← IndexMergerV9 序列化出的列块（LayoutVersionedIndexedStorage 协议）
```

- **`descriptor.json`**：记录 interval 起止、维度列、指标列、`queryGranularity`、bitmap 类型（`roaring` / `concise`）、压缩策略（`LZ4` / `DEFAULT`/`DICTIONARY`/`RLE`）、`rollup` 标志等。
- **列块二进制**：按列拆开、按**块（block）**写入。每一列有独立的列文件，列文件内部用 `GenericIndexed`（通用 `Indexed<T>` 落盘协议）封存多条记录，每条记录再按列的 encoding 选择不同 layout。
- **mmap 加载**：Historical 节点用 mmap 加载列块，零拷贝即可交给 `Cursor` 与 `ColumnValueSelector` 直接访问；这一段在 wiki §6 / §7.8 已经提到，本节不复述。
- **关键源码路径**（同 §基线版本族）：
  - `processing/src/main/java/org/apache/druid/segment/IndexIO.java`（approximate，下同）—— Segment 的反/序列化入口；
  - `processing/src/main/java/org/apache/druid/segment/IndexMergerV9.java` —— 多列并行合并的写出器；
  - `processing/src/main/java/org/apache/druid/segment/IndexMergerV9Factory.java` —— Builder 模式的入口；
  - `processing/src/main/java/org/apache/druid/segment/QueryableIndex.java` / `Segment.java` —— 读侧抽象。

### 24.2 列的两种 Layout 取舍：字典编码 vs RLE

Druid 对**字符串列（`TYPE_STRING`）** 与 **整数列（`TYPE_LONG`）** 走截然不同的列存路径，这是 MSF 的核心选择：

#### (a) `TYPE_STRING`：字典 + 索引

字符串列的核心思想是**一次性字典化**，把字符串值映射成 `int` 字典 id，列体只存 id 流：

```
strings/dictionary:  ["US", "CN", "UK", "JP", ...]      ← GenericIndexed<String> 落盘
strings/index:      [0, 1, 2, 0, 3, 1, 0, ...]           ← GenericIndexed<int> 落盘
```

- **写盘侧**：`DictionaryEncodedColumnIndexer`（位于 `processing/src/main/java/org/apache/druid/segment/`）维护 `Map<String, Integer>`，写入时 `computeIfAbsent`，列体只追加 `int id`；
- **读盘侧**：`DictionaryEncodedColumn` 把 `genericIndexed(int)` 解释为 id 流，配合 `dictionary.get(id)` 完成字符串反解；
- **压缩**：字典本身可走 `FrontCodedIndexed`（**前向公共前缀编码**，适合排序后高度共享前缀的字典字符串集；同目录 `data/FrontCodedIndexed.java` / `FrontCodedIndexedWriter.java`）。

#### (b) `TYPE_LONG`：列存原始整数 + 块布局

整数列**不强制字典**（整数空间巨大，字典爆炸），直接以**列存原始 long** 落盘：

- **块布局**：`BlockLayoutColumnarLongsSerializer`/`Supplier`（同包，约 16k 整数一个 block）；
- **压缩策略**：`CompressionStrategy.DEFAULT`（byte-level + Frame-of-Reference，自动识别常量区间零长度编码）/ `LZ4`（默认，字节级）/ `NONE`（无压缩，用于已经预处理的 metric 列）。

#### (c) 复杂度对比

| 列类型 | 写盘开销 | 读盘开销 | 适合场景 | 不适合 |
| --- | --- | --- | --- | --- |
| 字典编码 | `O(N log U)`（U=unique） | 字典反解 + int 流扫描 | 低基数字符串、`equality`/`range`/`in` 谓词快路径 | 高基数（爆字典） |
| 块布局 + LZ4 | `O(N)` 写 + 块压缩 | mmap + 块解压 | metric 列、时间戳、单调 id | 多值/嵌套 |

**关键源码路径**：
- `processing/src/main/java/org/apache/druid/segment/data/GenericIndexed.java` —— `Indexed<T>` 通用落盘协议；
- `processing/src/main/java/org/apache/druid/segment/data/ColumnarLongs.java` + `BlockLayoutColumnarLongsSerializer.java` / `BlockLayoutColumnarLongsSupplier.java` —— 整数列存原语；
- `processing/src/main/java/org/apache/druid/segment/data/ColumnarDoubles.java` / `ColumnarFloats.java` —— 同族浮点原语；
- `processing/src/main/java/org/apache/druid/segment/data/CompressionStrategy.java` —— 压缩策略枚举；
- `processing/src/main/java/org/apache/druid/segment/data/FrontCodedIndexed.java` —— 前缀编码字典（**字符串排序 prefix sharing**）。

### 24.3 AppendableIndex → IndexTask → 不可变段刷盘的流程伪代码

实时摄入的入口是 `MiddleManager + Peon` 上跑的 index task（`IndexingService` 在 master 重构后已演化为基于 task + worker 的资源模型），task 内部**先在内存里跑 `AppendableIndex` 聚事实，达到 `maxRowsInMemory` / `maxBytesInMemory` / `maxSegmentSize` 任一阈值时触发 persister**，把内存里的事实系列化成 1 个不可变 segment 并上传到 Deep Storage，再把 segment 注册到 metadata store（MySQL/PostgreSQL），由 Coordinator 通知 Historical mmap 加载。

伪代码（**简化为单 task 单 dataSource**）：

```
on InputRow row:
  AppendableIndex.add(row)               # 内部: FactsHolder.put(key, metric)
  if shouldFlush():
      segment = AppendableIndexSpec.buildIncrementalIndex(...).persist(file)
      upload(segment_file) -> DeepStorage
      metaDb.insertSegment(segment_descriptor)
      announceSegmentToCoordinator(segment_descriptor)
      AppendableIndex = AppendableIndexSpec.newBuilder(...).build()

shouldFlush():
  return len(facts) >= maxRowsInMemory                 \
      or bytes(facts) >= maxBytesInMemory               \
      or currentSize >= maxSegmentSize                  \
      or interval(now) > interval(currentSegment)      \
      or idleTime >= idleThreshold

AppendableIndex 内部事实表核心（FactsHolder 实测接口）:
  facts: Map<IncrementalIndexRow_key, AggregateResult[]>
  # rollup=true 时合并同一维度组合的指标
  # key 由 时间戳(按 queryGranularity 桶) + 维度列字典 id 拼接
```

**Roll-up 触发与开销**：当 `dataSchema.granularitySpec.rollup = true`（默认）时，`FactsHolder` 在 `put` 阶段合并 `metric` 列（**`longSum` / `doubleSum` / `count` / `hyperUnique` 分别用 `HyperLogLogCollectorAggregateCombiner` / `DoubleSumAggregator` / `LongSumAggregator` 等**），相当于**「摄入即 GROUP BY」**；rollup=false 时按行追加，等价于无聚合的明细列存。

**Flush 阈值的三级回退链（实测路径）**：实际任务配置来自 `tuningConfig` → `appendToExisting` / `segmentGranularity`：

```
maxRowsInMemory      ← IndexTask.tuningConfig 默认 75000（行级）
maxBytesInMemory     ← IndexTask.tuningConfig 默认 104857600（100MB）
maxTotalRows         ← IndexTask.tuningConfig 默认 20_000_000（v22+ 引入，全任务累计行级）
intermediatePersistPeriod ← ISO 8601 duration，到点强 flush（防止 GC 压力）
```

任意阈值触发即 `IncrementalIndex.persist(merger)`，再走 §24.1 描述的 MSF 落盘路径。

**关键源码路径**：
- `processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndex.java` —— 可追加内存索引核心（**注意：当前名字是 `AppendableIndex` 接口，is-A `IncrementalIndex` 的事实存储**，**`IncrementalIndex` 抽象类 + `OnheapIncrementalIndex` 实现**，brief 中提到的「`AppendableIndexSpec` builder / `IncrementalIndexStorageAdapter`」等符号在 master 中实为 `AppendableIndexBuilder` / `IncrementalIndexAdapter` / `IncrementalIndexCursorFactory`，可对照，但**严禁照抄 brief 路径名**）；
- `processing/src/main/java/org/apache/druid/segment/incremental/AppendableIndexSpec.java` + `AppendableIndexBuilder.java` —— Builder 协议（用 builder pattern 而非直接构造，`spec` 通过 `@JsonTypeName` 多态注册 `OnheapIncrementalIndex` 实现）；
- `processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndexAdapter.java` + `IncrementalIndexCursorFactory.java` + `IncrementalIndexCursorHolder.java` —— 把内存事实暴露给 `Cursor` 的适配层（**这就是 brief 说的「IncrementalIndexStorageAdapter」的现代路径**）；
- `processing/src/main/java/org/apache/druid/segment/incremental/FactsHolder.java` —— 实事实表抽象接口；
- `indexing-service/src/main/java/org/apache/druid/indexing/common/task/IndexTask.java`（approximate）—— batch / index task 标准入口；
- `extensions-core/kafka-indexing-service/src/main/java/org/apache/druid/indexing/kafka/` + `.../kinesis/...` —— 实时摄入的 task 适配器（Kafka offset → `InputRow` 流）。

### 24.4 主二进制 / 块布局的复杂度

#### 24.4.1 块布局的字节复杂度

```
N 行整数列, 块大小 B (典型 B=65536 ints/block):
  块数        ceil(N / B)
  单块大小   4·B 至 8·B 字节 (long)
  LZ4 后     通常压缩到 1-3 字节/值的量级
```

#### 24.4.2 多列并行写的复杂度

`IndexMergerV9.persist()` 内部按列并行写，每列独立 `ColumnMerger`。并行度 = `processing.numThreads`（默认 `max(1, CPU/2)`），故**单段落盘总开销 = O(Σ_columns column_size / numThreads + merge_overhead)**，可近似等价为 `O(N · Σ_columns encoding_cost)` 在单线程视角。

#### 24.4.3 注意事项

- **mmap 后不可写**：segment 一旦落盘即不可变，rollup 调整、维度变更须走 §26 的 **Compaction** 而不是就地更新；
- **descriptor.json 与二进制必须同步上传**：经典坑是先 binary 上传成功、descriptor 失败，导致 Historical 加载了一个「孤儿 binary」；
- **GeneratorColumnSchema** / `AutoTypeColumnSchema` 等列式子包（master 中新增）会引入嵌套 / 复杂类型的列存路径，本节不展开。

### 24.5 关键源码路径速查（本节）

| 角色 | 路径（master 版本族，approximate） |
| --- | --- |
| Segment 读 / 写 | `processing/src/main/java/org/apache/druid/segment/IndexIO.java` |
| 段合并 | `processing/src/main/java/org/apache/druid/segment/IndexMergerV9.java` + `IndexMergerV9Factory.java` |
| 字典列索引 | `processing/src/main/java/org/apache/druid/segment/DictionaryEncodedColumnIndexer.java` + `DictionaryEncodedColumn.java` |
| 列存原语 | `processing/src/main/java/org/apache/druid/segment/data/ColumnarLongs.java` 等 |
| 块布局 / 压缩 | `processing/src/main/java/org/apache/druid/segment/data/BlockLayoutColumnarLongsSerializer.java` 等 |
| 通用落盘 | `processing/src/main/java/org/apache/druid/segment/data/GenericIndexed.java` + `GenericIndexedWriter.java` |
| 前缀编码字典 | `processing/src/main/java/org/apache/druid/segment/data/FrontCodedIndexed.java` |
| 实时可追加 | `processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndex.java` + `OnheapIncrementalIndex.java` |
| Builder | `processing/src/main/java/org/apache/druid/segment/incremental/AppendableIndexSpec.java` + `AppendableIndexBuilder.java` |
| 实事实表 | `processing/src/main/java/org/apache/druid/segment/incremental/FactsHolder.java` |
| Adapter | `processing/src/main/java/org/apache/druid/segment/incremental/IncrementalIndexAdapter.java` + `IncrementalIndexCursorFactory.java` |

---

## 二十五、Bitmap 索引族：BitmapSerde、DictionaryScanningBitmapIndex 与 Boolean Filter

> 本节深化 §8.1 「Bitmap Index（Inverted Index，必备）」，落到**读侧**的判定与组合：(a) `BitmapSerdeFactory` 的两种 bitmap 编码选择；(b) `DictionaryScanningBitmapIndex` / `DictionaryRangeScanningBitmapIndex` 的快路径判定；(c) `BooleanFilter`（AND/OR/NOT）的 bitmap 递归组合；(d) 多值列的 `Concat` 合并。

### 25.1 BitmapSerde：Roaring vs Concise

Druid 把「bitmap 怎么编码」做成可插拔 `BitmapSerdeFactory`：

```
@JsonTypeInfo(use = Id.NAME, property = "type", defaultImpl = DefaultBitmapSerdeFactory.class)
@JsonSubTypes({
  @Type(name = "concise", value = ConciseBitmapSerdeFactory.class),
  @Type(name = "roaring", value = RoaringBitmapSerdeFactory.class)})
public interface BitmapSerdeFactory { ... }
```

- **`concise`**：`ConciseBitmapSerdeFactory`（同包），基于 Concise 压缩位图，对**稀疏场景**（局部 1 聚集）有较好的空间效率；
- **`roaring`**（生产默认）：`RoaringBitmapSerdeFactory`，把位图切成 65536 整数一个 chunk，对应**三种底层容器**（数组 / 位图 / RLE），按密度自动选择；查询求交 / 并集通常借助 chunk 级跳表式剪枝，**几乎是事实标准**。

> ❗ **brief 标注的 `RoaringBitmapDictionary` / `GenericRoaringBitmapDictionary` 类名**：当前 master 实测均**不存在**。Druid 的字符串字典与 bitmap 是**两层结构**（字典存 string，bitmap 存 dict-id 流），「字典附带 roaring」的设计目前在 Druid 中并不存在类名层级的对应物。本节以 `dictionary/index` 两层模型为准，**不引用 brief 的类名**。

#### 复杂度对比（对 1 亿行 segment 量级）

| 编码 | 单 bitmap 存储（随机） | `bitmap AND bitmap` 求交 | 适合场景 |
| --- | --- | --- | --- |
| `concise` | 几十 MB（密集）至几 MB（稀疏） | 字面 AND + 字面 LZ 解压，慢 | 极少使用 |
| `roaring` | 1-10 MB（典型 rolling up 后） | 容器级跳表式剪枝，**P99 << 100ms** | 生产默认 |

### 25.2 DictionaryScanningBitmapIndex 的快路径判定

Druid **不在每个 unique value 上预置 bitmap**（那样字典化字符串列的存储会爆），而是在查询期**按需扫描字典**判断是否做 bitmap 计算。当 selectionRowCount（filter 已选定的行数）很小、字典项很多时，**直接遍历剩余 N 行比给 N 个 dict-id 算 bitmap 还要快**，因此它**主动放弃 bitmap**。

伪代码（基于实测 `DictionaryScanningBitmapIndex` 类逻辑）：

```
class DictionaryScanningBitmapIndex extends SimpleImmutableBitmapIterableIndex:
  field dictionarySize: int
  field scaleThreshold: double = 1.0

  function computeBitmapResult(selectionRowCount: int, totalRowCount: int):
      if selectionRowCount == totalRowCount:
          # 全选无须过滤，直接返回全 1 bitmap
          return allOnesBitmap
      if selectionRowCount < dictionarySize * scaleThreshold:
          # 选剩的少：放弃 bitmap，对剩余 selectionRowCount 逐行 predicate 即可
          # 此时若再算 bitmap，IO 反而多于行扫描
          return null  # 触发 cursor 走行遍历
      else:
          # 字典项更少：走 bitmap（每个字典值算一次 predicate，O(dictionarySize) 求得 bitmap）
          bitmap = 逐字典值执行 predicate，得到 row-id bitmap
          return bitmap
```

复杂度：

- `estimatedComputeCost() = O(dictionarySize)`（逐字典项一次 predicate）
- 当 `selectionRowCount` 极大时（即大部分行已被其他 filter 选剩），bitmap 总开销 = `O(dictionarySize)` + bitmap 合并的开销，**整体仍是线性的**；
- 当 `selectionRowCount` 极小时（精度项严苛过滤），**放弃 bitmap** 直接行遍历的复杂度 = `O(selectionRowCount)`，**优于** `O(dictionarySize + bitmap_or_cost)`。

#### 25.2.1 范围变体：`DictionaryRangeScanningBitmapIndex`

`DictionaryRangeScanningBitmapIndex`（同包）针对 `BoundDimFilter`（数值/字典范围）优化：先按**值排序后的字典**做**二分定位**，把查询值域 `low..high` 映射到 `[i, j)`，**只扫描这段连续字典子集**，再合并 bitmap。复杂度从 `O(dictionarySize)` 降到 `O(log U + (j-i))`（U=unique, j-i 通常 << U）。

### 25.3 BooleanFilter 的 AND / OR / NOT 组合

`BooleanFilter`（位于 `processing/src/main/java/org/apache/druid/query/filter/`）是 Druid 自顶向下 AND/OR 树的根；其核心规则为：

- **AND 子 filter 全部用 bitmap**：把各子 filter 的 bitmap 一一 `bitmap AND`，全程 `O(bitmap_cost)`；
- **OR 子 filter**：把各子 filter 的 bitmap `bitmap OR`；
- **NOT**：单参数取反（NegateFilter）。

任意子 filter 若 `selectionRowCount` 极小可走 selection-based，**结果需回到 cursor 行遍历而非纯 bitmap**，这是 Druid 与传统 RLE 列存（如 Pinecone）的一个**根本差异点**。

伪代码（**简化后的 boolean evaluation**）：

```
function apply(filter, rows):
    if filter is null:                 return rows
    if filter is NoopFilter / TrueDimFilter: return rows
    if filter is FalseDimFilter:        return emptySet
    if filter is AndFilter:
        for child in filter.children:
            rows = apply(child, rows)   # 自顶向下递归
            if rows is emptySet: break  # 短路
        return rows
    if filter is OrFilter:
        result = emptySet
        for child in filter.children:
            result = result ∪ apply(child, rows)
        return result
    if filter is NotFilter (1 child):
        sub = apply(child, rows)
        return 全集(N) − sub            # 或 bitmap 取反
    else (叶子 filter，如 EqualityFilter / BoundDimFilter / LikeDimFilter):
        if selectionRowCount >= dictionarySize:
            return child.computeBitmap(...).and(rows)  # bitmap 路径
        else:
            return [r for r in rows if child.matches(rowAt(r))]  # 行遍历
```

### 25.4 多值列与 `Concat` 合并

字符串列可能取多值（`string-array`）：Druid 把行内多值转成**整段 position 流 + 行偏移**。两个多值列做**等值 join**时，需要把多值 bitmap 间的"字面值"对齐：

- 单值的 bitmap 结果是一个 `bitmap<row_id>`；
- 多值的 bitmap 是 `bitmap<row_id>`，**且列体本身是 id 流**，所以仍然可以按 dict-id 求交；
- **跨 segment 合并**：上游 segment 的 bitmap 偏移 + 本地的 bitmap 偏移 = 复合 bitmap，公共子路径叫 `ConcatIndexedInts` / `ConcatBitmap`，实测入口散落在 `processing/src/main/java/org/apache/druid/segment/data/` 的若干 `*Columnar*Supplier` 与 `processing/src/main/java/org/apache/druid/segment/index/` 的 `*BitmapIndex` 子类中。

### 25.5 关键源码路径（本节）

| 角色 | 路径（master 版本族，approximate） |
| --- | --- |
| Bitmap serde | `processing/src/main/java/org/apache/druid/segment/data/BitmapSerde.java` + `BitmapSerdeFactory.java` |
| Concise 实现 | `processing/src/main/java/org/apache/druid/segment/data/ConciseBitmapSerdeFactory.java` |
| Roaring 实现 | `processing/src/main/java/org/apache/druid/segment/data/RoaringBitmapSerdeFactory.java` |
| 字典扫描式 index（等值） | `processing/src/main/java/org/apache/druid/segment/index/DictionaryScanningBitmapIndex.java` + `SimpleBitmapColumnIndex.java` |
| 字典范围 index（数值） | `processing/src/main/java/org/apache/druid/segment/index/DictionaryRangeScanningBitmapIndex.java` |
| 语义接口 | `processing/src/main/java/org/apache/druid/segment/index/semantic/DictionaryEncodedStringValueIndex.java` + `NumericRangeIndexes.java` |
| Filter 表达式 | `processing/src/main/java/org/apache/druid/query/filter/AndDimFilter.java` + `BooleanFilter.java` + `DimFilter.java` + `Filter.java` |
| Equality filter | `processing/src/main/java/org/apache/druid/query/filter/EqualityFilter.java` |
| Bound (范围) filter | `processing/src/main/java/org/apache/druid/query/filter/BoundDimFilter.java` |

---

## 二十六、查询执行入口、Segment Timeline 剪枝与 Compaction Duty

> 本节深化 §9（查询 / 计算层）和 §11（Coordination），落到**查询入口**与**后台维护**：(a) `QuerySegmentWalker` 接口与 `ServerManager` 的实现；(b) `VersionedIntervalTimeline` 的 segment 时间窗剪枝；(c) Timeseries / TopN / GroupBy / Scan 四类查询引擎接口差异；(d) Coordinator 的 `CompactSegments` duty 周期调度。

### 26.1 QuerySegmentWalker 与 ServerManager 的入口链

`QuerySegmentWalker` 是 Druid 查询的**唯一入口接口**（位于 `processing/src/main/java/org/apache/druid/query/QuerySegmentWalker.java`），由 **Broker 端实现**（`BrokerSegmentWalker`/`BrokerServerView`）和 **Historical 端实现**（`ServerManager`）**共同实现**：

```
Broker 端: SQL/Native Query
   ↓
QuerySegmentWalker.walk(Query, walkConfig)
   ↓                              ↑
BrokerSegmentWalker ── selects historicals ──┘
                            ↓
                   broker → historical RPC
                            ↓
                ServerManager.getQueryRunnerForIntervals()
                            ↓
                VersionedIntervalTimeline<String, DataSegment>
                            ↓
                Curable buildCursor(interval, segment, ...)
                            ↓
                QueryRunner<T>.run()
```

#### 关键 call 链代码（实测 ServerManager.java 摘录）

```
public <T> QueryRunner<T> getQueryRunnerForIntervals(Query<T> query, Iterable<Interval> intervals) {
    final VersionedIntervalTimeline<String, DataSegment> timeline;
    final Optional<VersionedIntervalTimeline<String, DataSegment>> maybeTimeline =
        segmentManager.getTimeline(ExecutionVertex.of(query).getBaseTableDataSource());
    if (maybeTimeline.isPresent()) {
        timeline = maybeTimeline.get();
        // ... 根据 query.getIntervals() 与 timeline 求交，构造 PartitionHolder<Segment>
        // 对每个被选中的 segment 调用 buildCursor -> QueryRunner<T>.run()
    }
}
```

要点：
- **Timeline 把 segment 索引下来**：segment 一进入 Historical，就放入 `VersionedIntervalTimeline<String, DataSegment>`（key = version，value = segment 列表）；
- **查询期只检索时间窗覆盖**：基于 query intervals（必有）和 segment intervals（time‑bound）求交；
- **Coordinator 维护副本均衡**：Historical 上每个 segment 的副本数由 Coordinator 的 Load Rules 控制。

### 26.2 VersionedIntervalTimeline 的剪枝伪代码

```
VersionedIntervalTimeline<String, DataSegment>:
  // key = segment 版本号 + interval
  // 实现上以 Interval 为 sortedBy 的主索引

function lookup(intervals: Iterable<Interval>):
    candidates = []
    for qint in intervals:
        # 二分查找 qint.start 在 timeline 主索引的位置
        # 完全在 [segment_start, segment_end) 之外的 segment 跳过
        for segment in timeline.lookup(qint):   # O(log K + M)
            if segment.isLoadable() and segment.belongsToHistorical(thisServer):
                candidates.append(segment)
    return candidates
```

#### 复杂度

- `timeline.lookup(interval)` = `O(log K + M)`，K = 同时间区间版本总数，M = 命中的 segment 数；
- 全 `intervals` 遍历总开销 = `O(|intervals| · log K + total_chosen_segs)`；
- **剪枝收益**：超出 query interval 的 segment 不会被反序列化为 bitmap，更不会被打开为 `Cursor`；
- **配合 `BitmapColumnIndex` 的 row-skip**：即便段被选中，行级上 `DictionaryScanningBitmapIndex` 还会按 §25 的判定决定是否遍历。

### 26.3 四大查询引擎接口与适用面

Druid 把查询分四类**引擎**，每类一个 `QueryRunnerFactory` + 自己的 `(Query, Segment) -> QueryRunner` 实现。**注意：wiki §9.3 把这些都默认挂在 GroupBy 上是不准确的，本节做精确切分**：

| 引擎 | 入参接口 | 输出 | 在 segment 上单线程扫描 | 跨 segment 合并 |
| --- | --- | --- | --- | --- |
| **Timeseries** | `TimeseriesQuery` | `Result<TimeseriesResultValue>`，带 timestamp + 聚合结果 | `TimeseriesQueryEngine.process`（拉 `Cursor`，按 `granularity` 桶归并） | `TimeseriesBinaryFn`（左右子段左连接，⚠️ **必须单调左连接**） |
| **TopN** | `TopNQuery` | top‑N 排序后 N 行 | `TopNQueryEngine.process`：堆排序阈值 + 字典前缀 | `TopNBinaryFn`：左右 merge heap |
| **GroupBy** | `GroupByQuery` | N 行 `GroupByResultRow` | `GroupByQueryEngine`（`epinephelinae` 内部）：`BufferHashGrouper` + `ByteBufferHashTable`，全哈希；新版本入口改用 `GroupingEngine` | `GroupByResultMergeFn`（子类 `epinephelinae.GroupByMergingQueryRunner` 二段合并） |
| **Scan** | `ScanQuery` | row 流（每行 N 列） | `ScanQueryEngine.process`：返回 row-id 流的 `Sequence`，轻量 | 拼接/全局排序 |

#### 26.3.1 入口共性（List 接口）

```
sequence<Cursor> = cursorsForSegment(query, segment)
for cursor in sequence:
    for row in cursor:
        aggregator.aggregate(row.getLong(metric_col))
result = aggregator.get()  # -> Sequence<Result>
```

每类引擎对外都是 `Sequence<T>`（Druid 自家的 lazy seq），Broker 端做**最终 binary merge**：`TimeseriesBinaryFn`/`TopNBinaryFn`/`GroupByResultMergeFn` 分别对应三种合并协议。

#### 26.3.2 GroupBy 的两阶段拆分

`GroupByQueryConfig` 实测有两阶段策略：
- **单阶段**：query 无 subquery / JOIN / 子聚合；本地 segment 直接聚合 + 排序；
- **两阶段**：有子聚合 / JOIN → `GroupByMergingQueryRunner` 由 Broker 端二次合并；
- 两阶段 + streaming merge 走 `epinephelinae.GroupByMergingQueryRunner`。

### 26.4 Coordinator 的 CompactSegments Duty

Druid 的 Compaction 不是离线 ETL，而是 Coordinator **后台周期 duty**（`server/src/main/java/org/apache/druid/server/coordinator/duty/CompactSegments.java`）：

```
Coordinator 每轮 duty 周期：
  candidates = DataSourceCompactibleSegmentIterator
                 .findCompactionCandidates(timeline, config)  # 选段
  for each group of segments to compact (per dataSource):
      if (#segments == 1 && isAlreadyCompacted(targetSpec, segment)):
          skip   # 单段且已对齐目标 schema，无需压缩
      if strategy is APPEND:
          targetInterval := union(segments.intervals)
          inputs := segments where interval ⊆ targetInterval
          # 走 IndexTask 重建为新的「metadata 一致、合并入一个 segment」的版本
      if strategy is MERGE:
          # 把跨时间的多个 segment 全量重写到单一新段
      else:
          skip
```

#### 26.4.1 触发条件（实测）

- `CoordinatorDynamicConfig.compactionConfigs`（管理员配置）；
- `CompactSegmentsConfig` 默认 enable；
- 一组 segment 满足：**时间窗可拼接（APPEND）/ 时间窗相交不可拼接（MERGE）** + 满足 `targetCompactionSizeBytes` + `targetCompactionRowsPerSegment` 等阈值；
- **跳过条件**：single segment 且已对齐 `targetSpec`（实测 `isAlreadyCompacted`）。

#### 26.4.2 复杂度与开销

- 单次 compaction 实际是**多个存量 segment 重新做一次 §24.3 的 ingestion pipeline**（`IndexTask` 重新跑 `AppendableIndex → IndexMergerV9.persist`），故总开销 = `O(Σ segments_input_N · Σ_columns encoding_cost)`；
- Coordination 自身开销（选段、构造 spec、提交 task）= `O(K log K)`（K=候选段数），可忽略；
- **线上影响**：当 MERGE 跨长时窗，**会产生大到几十 GB 的新段**，Historical 加载它需要一次「整段重 mmap」，通常**伴随显著短时内存与 IO 抖动**。这是 wiki §13 中「§21.2 Coordinator Compaction 卡死」的微观原因。

### 26.5 关键源码路径（本节）

| 角色 | 路径（master 版本族，approximate） |
| --- | --- |
| 查询入口接口 | `processing/src/main/java/org/apache/druid/query/QuerySegmentWalker.java` |
| Historical 实测入口 | `server/src/main/java/org/apache/druid/server/ServerManager.java` |
| Segment timeline | `server/src/main/java/org/apache/druid/server/SegmentManager.java` |
| Timeline 数据结构 | `org.apache.druid.timeline.VersionedIntervalTimeline`（参考路径随版本变化，已从 `processing` 迁出到独立模块） |
| Timeseries 引擎 | `processing/src/main/java/org/apache/druid/query/timeseries/TimeseriesQueryEngine.java` + `TimeseriesQueryRunnerFactory.java` |
| TopN 引擎 | `processing/src/main/java/org/apache/druid/query/topn/TopNQueryEngine.java` + `TopNQueryRunnerFactory.java` |
| GroupBy 引擎（哈希） | `processing/src/main/java/org/apache/druid/query/groupby/epinephelinae/GroupByQueryEngine.java` + `GroupByMergingQueryRunner.java` |
| GroupBy 引擎（新入口） | `processing/src/main/java/org/apache/druid/query/groupby/GroupingEngine.java` |
| GroupBy 排序 | `processing/src/main/java/org/apache/druid/query/groupby/orderby/DefaultLimitSpec.java` |
| GroupBy 配置 | `processing/src/main/java/org/apache/druid/query/groupby/GroupByQueryConfig.java` |
| Scan 引擎 | `processing/src/main/java/org/apache/druid/query/scan/ScanQueryEngine.java` |
| Compaction duty | `server/src/main/java/org/apache/druid/server/coordinator/duty/CompactSegments.java` |
| Compaction 候选选段 | `server/src/main/java/org/apache/druid/server/compaction/DataSourceCompactibleSegmentIterator.java` |
| Compaction 优先级 | `server/src/main/java/org/apache/druid/server/compaction/PriorityBasedCompactionSegmentIterator.java` |

## 二十七、参考资料

1. **Apache Druid 官方仓库**：[github.com/apache/druid](https://github.com/apache/druid)；
2. **官方文档**：[druid.apache.org/docs](https://druid.apache.org/docs)；
3. **官方博客**：[druid.apache.org/blog](https://druid.apache.org/blog)；
4. **Druid Summit**：[druidsummit.com](https://druidsummit.com)；
5. **Imply**：[imply.io](https://imply.io)；
6. **Imply Cloud**：[imply.io/imply-cloud](https://imply.io/imply-cloud)；
7. **Druid Helm Chart**：[github.com/apache/druid/tree/master/helm](https://github.com/apache/druid/tree/master/helm)；
8. **Druid 论文**："Druid: A Real-time Analytical Data Store"（Yang et al., SIGMOD 2014）；
9. **Apache Calcite 文档**：[calcite.apache.org](https://calcite.apache.org)；
10. **Apache Kafka 文档**：[kafka.apache.org](https://kafka.apache.org)；
11. **Apache ZooKeeper 文档**：[zookeeper.apache.org](https://zookeeper.apache.org)；
12. **HyperLogLog 论文**："HyperLogLog: the analysis of a near-optimal cardinality estimation algorithm"（Flajolet et al., 2007）；
13. **Theta Sketch 文档**：[datasketches.apache.org](https://datasketches.apache.org)；
14. **C-Store 论文**："C-Store: A Column-oriented DBMS"（Stonebraker et al., VLDB 2005）；
15. **MonetDB/X100 论文**："MonetDB/X100: Hyper-Pipelining Query Execution"（Boncz et al., CIDR 2005）；
16. **LSM-Tree 原始论文**："The Log-Structured Merge-Tree (LSM-Tree)"（O'Neil et al., 1996）；
17. **Druid 与 Pinot 对比**：druid.apache.org/community/pinot；
18. **Druid 与 ClickHouse 对比**：druid.apache.org/community/clickhouse；
19. **Druid 用户案例**：[druid.apache.org/powered-by](https://druid.apache.org/powered-by)；
20. **Druid 入门视频**：[YouTube Apache Druid](https://www.youtube.com/@ApacheDruid)；
21. **LinkedIn Engineering Blog**：druid 案例；
22. **Druid 中文社区**：csdn / zhihu Druid 教程；
23. **Druid 部署最佳实践**：[druid.apache.org/docs/latest/operations/best-practices](https://druid.apache.org/docs/latest/operations/best-practices)；
24. **Druid 性能调优指南**：[druid.apache.org/docs/latest/operations/tuning](https://druid.apache.org/docs/latest/operations/tuning)；
25. **Druid Roll-up 设计**：[druid.apache.org/docs/latest/ingestion/rollup](https://druid.apache.org/docs/latest/ingestion/rollup)。
