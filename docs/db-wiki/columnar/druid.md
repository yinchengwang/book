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

## 二十四、参考资料

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
