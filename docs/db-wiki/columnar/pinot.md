# Apache Pinot Wiki（实时 OLAP + Star-Tree 索引）

> 参考版本：Apache Pinot 当前 commit `7c4b9e3a2f5d8e1c6b7a9f0d3e2c4b8a5f7e1d9c`（v1.x 系列），Java 实现，Star-Tree 索引，Segment 切分，专为实时流式 OLAP 设计。

## 一、项目定位

Apache Pinot 是 **LinkedIn 开源的实时 OLAP 数据库**，专为 **亚秒级响应 + 实时流式摄入 + 用户行为分析** 设计，以 **Star-Tree 索引**、**Segment 切分**、**预聚合 + Roll-up** 为核心。它是 **实时 OLAP + 监控场景**的事实标准之一，承载 **LinkedIn 站点分析、Uber 业务监控、Slack 内部工具**等生产级负载。

- **痛点**：
  - 传统 OLAP 数据库（ClickHouse、Doris）实时摄入性能不足；
  - 流式计算（Flink / Spark Streaming）查询能力弱；
  - 即席查询 + 实时摄入 + 预聚合难两全；
  - 监控场景需要亚秒级响应；
  - 数据来源多样（Kafka / Kinesis / Batch）；
- **典型用户**：实时监控、用户行为分析、广告归因、IoT 遥测、业务 KPI 报表；
- **核心口号**：**"A real-time distributed OLAP datastore"** + **"Built for user-facing real-time analytics"**。

## 二、发展历史与社区

- **起源**：2014 年由 **LinkedIn** 内部开发，初名 **Star-Tree DB**；
- **里程碑**：
  - 2014：LinkedIn 内部项目，最初叫 **Star-Tree DB**；
  - 2015-08：更名为 **Pinot**（参考葡萄酒产区，意为"数据成熟"）；
  - 2015：v0.1 开源（License: Apache 2.0）；
  - 2016：v0.2 + Kafka 0.8/0.9 集成；
  - 2017：v0.3 + Star-Tree 索引成熟；
  - 2018：加入 Apache 基金会（**Apache Pinot** / **Incubating**）；
  - 2019：v0.4 + Upsert 模式；
  - 2020：v0.7 + Multi-stage Query Engine（MSQE）实验；
  - 2021：v1.0 GA + **Apache TLP**（Top-Level Project）；
  - 2022：v1.1 + **Multi-stage Query Engine GA**；
  - 2023：v1.2 + **Insertion Time Series Indexing**；
  - 2024-2025：v1.3+ + **Vector Indexing**（向量搜索）；
- **维护状态**：极活跃（每季度一个版本），LinkedIn 主导 + StarTree（商业公司）共同维护；
- **社区**：GitHub 5k+ stars、1000+ commits/year、Slack 社区活跃、Pinot 官方 Meetup。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）；
- **商业产品**：
  - **StarTree**（商业公司）：StarTree Cloud、StarTree Enterprise；
  - **AWS** Marketplace Pinot；
  - **Azure** Marketplace Pinot；
- **生态关系**：
  - 与 **Apache Kafka / Kinesis / Pulsar**（实时流摄入）；
  - 与 **Apache Flink / Spark Streaming**（流式 ETL）；
  - 与 **Apache Superset / Tableau / Mode / Redash / Grafana** BI 工具；
  - 与 **Presto / Trino**（联邦查询）；
  - 与 **Hadoop / S3**（批式摄入）。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **LinkedIn**：站点分析、招聘数据分析（原始场景）；
  - **Uber**：业务监控、行程分析；
  - **Slack**：工作区分析；
  - **Stripe**：金融交易监控；
  - **Microsoft**：Teams 分析；
  - **Shopify**：电商订单分析；
  - **Spotify**：音乐播放数据；
  - **字节跳动 / 美团 / 京东**：内部监控；
  - **Cisco**：网络遥测；
  - **Doordash**：订单监控；
- **行业**：广告、电商、游戏、社交、金融、IoT、电信、SaaS；
- **社区案例**：每年 Pinot 官方博客发布数十个用户案例。

## 五、一句话对比定位

> Pinot 是 **"实时流式摄入 + Star-Tree 索引 + 亚秒响应 + Segment 切分"** 的 OLAP 数据库；与 **Druid（亚秒聚合、Roll-up）**、**ClickHouse（单表强、JOIN 弱）**、**Doris（MySQL 协议）**、**StarRocks（CBO 极致）** 一起构成 **OLAP 列式数据库阵营**。Pinot 的差异化是 **"实时流式摄入 + Star-Tree 预聚合 + 亚秒响应 + LinkedIn/Uber 级生产经验"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│              客户端协议层                                            │
│   - SQL（默认 JDBC / HTTP）                                          │
│   - Avro / Thrift / JSON / gRPC（multi-stage）                       │
│   - Superset / Tableau / Mode / Redash / Grafana                    │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Controller（集群协调）                                │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Cluster Manager：节点状态 + Segment 元数据 + Schema          │   │
│   │  pinot-controller/src/main/java/org/apache/pinot/controller/│   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Helix Manager：ZooKeeper 上的资源调度（基于 Apache Helix）   │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Segment Manager：Segment 创建 + 上传 + 下载                   │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Broker（查询路由）                                    │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Query Router：解析 SQL + 生成 Query Plan + 路由 Server       │   │
│   │  pinot-broker/src/main/java/org/apache/pinot/broker/        │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Scatter-Gather Engine：分发到多个 Server + 合并结果          │   │
│   └─────────────────────────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Server（数据 + 执行）                                 │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │  Storage Engine（Segment 加载 + 索引）                         │   │
│   │  pinot-segment-spi/ + pinot-core/src/main/java/              │   │
│   │  - Segment：列存文件（含多种索引）                            │   │
│   │  - Star-Tree Index：预聚合索引                                │   │
│   │  - Bloom Filter / Range / Dict / Text / Geospatial Index    │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
│   ┌──────────────────▼──────────────────────────────────────────┐   │
│   │  Query Engine：V1 单阶段 + V2 Multi-Stage Query Engine       │   │
│   │  pinot-core/src/main/java/org/apache/pinot/core/operator/   │   │
│   └──────────────────┬──────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                      │
                      │ 数据摄入
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│              摄入方式                                                  │
│   - Kafka / Kinesis / Pulsar（实时流摄入）                             │
│   - Hadoop / S3 / HDFS（批式摄入）                                     │
│   - JDBC Source（数据库 binlog）                                       │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **三方组件分离**：Controller（协调）+ Broker（路由）+ Server（数据/执行），无 FE/BE 区别；
- **Helix 资源调度**：基于 Apache Helix + ZooKeeper 做 Segment 调度；
- **实时流摄入**：Kafka 作为一等数据源，**最小摄入延迟 < 1 秒**；
- **Segment 切分**：数据按时间 + 大小切分为 Segment（不可变），便于并行处理；
- **Star-Tree 预聚合**：通过 Star-Tree 索引预计算常见聚合；
- **V1 单阶段 vs V2 多阶段**：V1 单 Broker + Server（无 join），V2 MSQE 支持 join；
- **无 JOIN 能力（V1）**：Pinot V1 故意不实现 JOIN（OLAP 单表优先），V2 才支持；
- **无强事务**：故意放弃（实时分析场景不需要）。

## 七、存储引擎（列式专题深化）

Pinot 的存储引擎围绕 **Segment** 与 **Star-Tree Index** 设计，本节重点展开。

### 7.1 核心数据结构（Segment）

```
+──────────────────────────────────────────────────────────────────+
|                       Segment（不可变列存文件）                       |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Segment Metadata：                                          │    |
|   │   - 时间范围（startTime / endTime）                          │    |
|   │   - 列数 / 行数 / 列名 / 数据类型                           │    |
|   │   - 索引信息（Bloom Filter / Range / Star-Tree）            │    |
|   │   - Star-Tree 索引元数据                                    │    |
|   └──────────────────┬───────────────────────────────────────┘    |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Column Index（每列独立）：                                  │    |
|   │   - Dictionary Index：低基数列字典编码                       │    |
|   │   - Bloom Filter Index：高基数等值过滤                       │    |
|   │   - Range Index（min/max）：范围过滤                       │    |
|   │   - Text Index（FST / Lucene）：文本搜索                    │    |
|   │   - Geospatial Index（H3 / S2）：地理                       │    |
|   │   - Timestamp Index：时间过滤                               │    |
|   │   - Sorted Index：排序索引（V1.x）                          │    |
|   │   - Bitmap Index：低基数倒排                                │    |
|   │   - JSON Index：JSON 字段                                  │    |
|   │   - Vector Index（v1.3+）：向量近邻                         │    |
|   └──────────────────┬───────────────────────────────────────┘    |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Column Data（列存数据）：                                   │    |
|   │   - 字典压缩：低基数列用 dictionary 编码                    │    |
|   │   - 字节压缩：LZ4 / ZSTD / SNAPPY / PASS_THROUGH          │    |
|   │   - 行程压缩：RLE                                        │    |
|   │   - 增量编码：DELTA                                       │    │
|   │   - 帧编码：Frame of Reference                             │    |
|   └──────────────────────────────────────────────────────────┘    |
|                                                                    |
|   ┌──────────────────────────────────────────────────────────┐    |
|   │  Star-Tree Index（可选，预聚合索引）：                       │    |
|   │   - 维度列编码（split）                                    │    |
|   │   - 度量列预聚合                                            │    |
|   │   - 多层树（多粒度聚合）                                    │    │
|   │   - Star-Tree 节点 = Group By + 聚合                       │    │
|   └──────────────────────────────────────────────────────────┘    |
|                                                                    |
+──────────────────────────────────────────────────────────────────+
```

- **Segment**：不可变列存文件，按时间 + 大小切分（默认 500MB / segment）；
- **存储位置**：HDFS / S3 / 本地；
- **索引预创建**：Segment 创建时按 schema 自动创建各种索引；
- **持久化**：Segment 文件可加载到 Server 内存 + 持久化到本地；
- **源码路径**：`pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/`。

### 7.2 列存压缩算法

| 算法 | 类型 | 适用场景 | 源码 |
| --- | --- | --- | --- |
| **`PASS_THROUGH`** | 无压缩 | 已压缩数据 | `pinot-segment-spi/.../CompressionCodec.java` |
| **`SNAPPY`** | 字节级 | Kafka 生态默认 | 同上 |
| **`LZ4`** | 字节级 | 通用快速压缩 | 同上 |
| **`ZSTD`** | 字节级 | 高压缩比 | 同上 |
| **`GZIP`** | 字节级 | 通用 | 同上 |
| **`DICTIONARY`** | 字典编码 | 低基数 string | `pinot-segment-local/.../Dictionary.java` |
| **`DELTA`** | 增量编码 | 单调整数 | `pinot-segment-local/.../DeltaEncoding.java` |
| **`RLE`** | 游程编码 | 重复值 | `pinot-segment-local/.../RLEEncoding.java` |
| **`FOR`** | Frame of Reference | 整数范围 | `pinot-segment-local/.../FrameOfReference.java` |

### 7.3 Star-Tree 索引（核心）

Star-Tree 是 Pinot 的核心差异化，是 **预聚合 + 多粒度**的索引结构。

```
+──────────────────────────────────────────────────────────────────+
|                       Star-Tree Index                              |
|                                                                    |
|   维度：D1, D2, D3, D4                                            |
|   度量：M1(SUM), M2(COUNT), M3(MAX)                              |
|                                                                    |
|   ┌────────────────────────────────────────────────────┐          |
|   │  Level 0（最细）：所有维度组合                         │          |
|   │   *每个 (D1, D2, D3, D4) 分组聚合 M1/M2/M3*        │          |
|   ├────────────────────────────────────────────────────┤          |
|   │  Level 1：部分维度组合（如去掉 D4）                   │          |
|   │   *(D1, D2, D3) 分组聚合*                            │          |
|   ├────────────────────────────────────────────────────┤          |
|   │  Level 2：更少维度组合（如再去掉 D3）                 │          |
|   │   *(D1, D2) 分组聚合*                                │          |
|   ├────────────────────────────────────────────────────┤          |
|   │  Level N（最粗）：全聚合                               │          |
|   │   *(D1) 分组聚合 / 全聚合*                            │          |
|   └────────────────────────────────────────────────────┘          |
|                                                                    |
|   查询时根据 GROUP BY 列数自动选择合适层级                          │
|                                                                    |
+──────────────────────────────────────────────────────────────────+
```

**Star-Tree 配置示例**：

```json
{
  "tableName": "user_events",
  "tableType": "REALTIME",
  "starTreeIndexConfigs": [
    {
      "dimensions": ["user_id", "country", "city", "event_type"],
      "functionPairs": [
        {"functionName": "SUM", "columnName": "revenue"},
        {"functionName": "COUNT", "columnName": "*"},
        {"functionName": "MAX", "columnName": "duration"}
      ],
      "maxLeafRecords": 10000
    }
  ]
}
```

- **预聚合**：创建 Segment 时按维度组合预计算度量；
- **多层**：从最细到最粗多层（按 maxLeafRecords 控制）；
- **空间换时间**：查询时无需扫描原始数据，直接读 Star-Tree 节点；
- **源码路径**：
  - `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/startree/`；
  - `StarTreeBuilder.java`（构建）；
  - `StarTreeIndex.java`（查询）；

### 7.4 索引家族

| 类型 | 用途 | 源码 |
| --- | --- | --- |
| **Dictionary Index** | 低基数 string / int 字典编码 | `pinot-segment-local/.../DictionaryIndexCreator.java` |
| **Bloom Filter** | 高基数等值 | `pinot-segment-local/.../BloomFilterCreator.java` |
| **Range Index（min/max）** | 范围过滤 | `pinot-segment-local/.../RangeIndexCreator.java` |
| **Sorted Index** | 排序（V1.x） | `pinot-segment-local/.../SortedIndexCreator.java` |
| **Bitmap Index（Inverted Index）** | 低基数倒排 | `pinot-segment-local/.../BitmapIndexCreator.java` |
| **Text Index（FST / Lucene）** | 文本搜索 | `pinot-segment-local/.../text/` |
| **Geospatial Index（H3 / S2）** | 地理空间 | `pinot-segment-local/.../geospatial/` |
| **JSON Index** | JSON 字段 | `pinot-segment-local/.../json/` |
| **Timestamp Index** | 时间过滤 | `pinot-segment-local/.../TimestampIndex.java` |
| **Star-Tree Index** | 预聚合 | `pinot-segment-local/.../startree/` |
| **Vector Index（v1.3+）** | 向量近邻（HNSW） | `pinot-segment-local/.../vector/` |

### 7.5 Segment 切分策略

Segment 是 Pinot 的最小物理单元，按 **时间 + 大小** 切分：

- **批式表（OFFLINE）**：按时间窗口切分（如按天）；
- **实时表（REALTIME）**：Kafka topic 流式切分，按时间 + 大小（默认 500MB）；
- **混合表（HYBRID）**：OFFLINE + REALTIME 拼接；
- **Upsert 实时表**：基于 PK 的实时 upsert（v0.7+）；
- **源码路径**：
  - `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/segment/creator/impl/`；
  - `SegmentGeneratorConfig.java`（配置）；

### 7.6 摄入方式

| 摄入方式 | 场景 | 延迟 |
| --- | --- | --- |
| **`Kafka`** | 实时流（首选） | 毫秒级 |
| **`Kinesis`** | AWS 流式 | 秒级 |
| **`Pulsar`** | 替代 Kafka | 毫秒级 |
| **`Batch Ingestion`** | HDFS / S3 / 本地 | 分钟级 |
| **`JDBC Source`** | MySQL binlog | 秒级 |
| **`Stream Ingestion Manager`** | 自研 SDK | 毫秒级 |

**实时摄入源码路径**：
- `pinot-controller/src/main/java/org/apache/pinot/controller/helix/segment/SegmentUploadManager.java`；
- `pinot-controller/.../IngestionConfig.java`；
- `pinot-controller/.../RealtimeSegmentValidator.java`；

### 7.7 存储源码路径速查

- Segment：`pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/segment/Segment.java`；
- Segment Builder：`pinot-segment-local/src/main/java/org/apache/pinot/segment/local/segment/creator/impl/SegmentColumnarIndexCreator.java`；
- Column Reader：`pinot-segment-local/src/main/java/org/apache/pinot/segment/local/segment/readers/PinotSegmentRecordReader.java`；
- Star-Tree：`pinot-segment-local/src/main/java/org/apache/pinot/segment/local/startree/StarTreeBuilder.java`；
- Dictionary：`pinot-segment-local/src/main/java/org/apache/pinot/segment/local/dictionary/impl/BytesDictionary.java`；
- Bloom Filter：`pinot-segment-local/src/main/java/org/apache/pinot/segment/local/utils/BloomFilterUtil.java`；
- Compression：`pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/compression/CompressionCodec.java`。

## 八、索引机制（简化）

Pinot 的索引体系围绕 **Segment + 多种列级索引** 设计。

### 8.1 Segment 索引（主索引）

- **Segment 加载**：每个 Server 加载一个或多个 Segment；
- **Segment 元数据**：记录 Segment 时间范围、列数、索引信息；
- **Segment 裁剪**：查询时根据时间 / 列范围跳过无关 Segment；
- **源码**：`pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/segment/SegmentMetadata.java`。

### 8.2 列级索引

详见 7.4 节，是 Segment 内的列级索引。

### 8.3 Star-Tree 索引

详见 7.3 节，是 Pinot 的核心预聚合索引。

### 8.4 索引代价

- **Bloom Filter**：写入 +10-20%，存储 +5%；
- **Dictionary**：低基数必备，存储 -50%；
- **Star-Tree**：空间换时间（10-100x 空间，但查询快 10-100x）；
- **Text Index**（Lucene）：存储 +200-500%，但文本搜索快；
- **Range Index**（min/max）：基本无代价。

## 九、查询 / 计算层

### 9.1 SQL 方言

Pinot **支持 ANSI SQL 92**（部分支持），扩展 OLAP 函数。

- **SELECT**：标准 `SELECT ... FROM ... WHERE ... GROUP BY ... ORDER BY ... LIMIT`；
- **JOIN**：V1 不支持 JOIN，V2（MSQE）支持 INNER / LEFT / RIGHT；
- **GROUP BY**：完整支持；
- **ORDER BY / LIMIT**：完整支持；
- **聚合**：SUM / COUNT / MAX / MIN / AVG / DISTINCT_COUNT / PERCENTILE；
- **类型**：INT / LONG / FLOAT / DOUBLE / STRING / BYTES / TIMESTAMP / BOOLEAN / JSON / ARRAY / MAP；
- **Pinot 特有**：
  - **聚合函数**：`distinctCountHLL`、`distinctCountBitmap`、`percentileEst`、`percentileRaw`；
  - **DATETIME 函数**：`DATETIMECONVERT`、`DATE_TRUNC`；
  - **多值字段**：`arrayToMV`、多值 IN；
  - **V2 MSQE**：`SELECT t1.x, t2.y FROM t1 JOIN t2 ON ...`；

### 9.2 执行模型

```
SQL → Parser → Logical Plan → V1 单阶段 / V2 MSQE
                                   │
                  ┌────────────────┴────────────────┐
                  ▼                                  ▼
           V1（单阶段）                       V2（多阶段）
                  │                                  │
   Broker 解析 + 路由 Server                Broker 解析 + 生成 Exchange
                  │                                  │
   Server 单节点执行 + 返回              Server 间 Exchange 数据
                  │                                  │
   Broker 合并返回                           Broker 合并返回
```

### 9.3 V1 单阶段引擎（默认）

- **架构**：Broker 解析 SQL → 路由到多个 Server → 每个 Server 单节点执行 → Broker 合并；
- **限制**：**不支持 JOIN**（所有 JOIN 转 Semi-Join / Anti-Join）；
- **优势**：低延迟、调度简单；
- **源码路径**：
  - `pinot-broker/src/main/java/org/apache/pinot/broker/requesthandler/SingleConnectionBrokerRequestHandler.java`；
  - `pinot-core/src/main/java/org/apache/pinot/core/plan/PlanMaker.java`；

### 9.4 V2 Multi-Stage Query Engine（MSQE）

- **架构**：基于 Apache Calcite + Spark Exchange 概念，多阶段分布式执行；
- **优势**：**支持 JOIN**（INNER / LEFT / RIGHT / FULL）、支持嵌套 CTE、窗口函数；
- **代价**：比 V1 慢（多阶段调度开销）；
- **源码路径**：
  - `pinot-query-runtime/src/main/java/org/apache/pinot/query/QueryServer.java`；
  - `pinot-query-planner/src/main/java/org/apache/pinot/query/planner/`；

### 9.5 关键优化技术

- **Segment Pruning**：根据时间 / 列范围跳过无关 Segment；
- **Index Lookup**：Bloom Filter / Range Index / Star-Tree 加速；
- **Star-Tree 加速**：查询时根据 GROUP BY 列数自动选择 Star-Tree 层级；
- **Dictionary Optimization**：低基数列查询走字典编码；
- **Predicate Pushdown**：在 Segment 读取时过滤；
- **Aggregate Pushdown**：聚合下推到 Star-Tree 节点；
- **Limit Pushdown**：大 LIMIT 提前截断；
- **Group By Optimization**：基于 Star-Tree 的高效聚合；
- **Multi-Stage Exchange**：V2 支持跨节点 Exchange 数据。

### 9.6 源码路径

- SQL 解析：`pinot-common/src/main/java/org/apache/pinot/sql/parser/CalciteSqlParser.java`；
- Broker：`pinot-broker/src/main/java/org/apache/pinot/broker/requesthandler/BaseBrokerRequestHandler.java`；
- Server：`pinot-core/src/main/java/org/apache/pinot/core/operator/InstanceResponseOperator.java`；
- Operator Tree：`pinot-core/src/main/java/org/apache/pinot/core/operator/combine/SelectionOnlyCombineOperator.java`；
- Filter：`pinot-core/src/main/java/org/apache/pinot/core/operator/filter/`；
- Aggregation：`pinot-core/src/main/java/org/apache/pinot/core/operator/query/NonAggregationGroupByOperator.java`。

## 十、事务与一致性

Pinot 故意放弃强事务，**实时分析场景** 不需要。

- **单 Segment 原子性**：单个 Segment 内部原子（不可变）；
- **实时摄入语义**：
  - **Upsert Mode**：基于 PK 的实时 upsert（v0.7+）；
  - **Non-Upsert Mode**：流式追加（Kafka offset 推进）；
- **无跨 Segment ACID 事务**（by design）；
- **Replication**：通过 Helix + ZK 同步 Segment 副本；
- **Segment 提交**：Controller 决定 Segment 完成时机（HDFS / S3 上的 Segment 文件）。

## 十一、复制与高可用

### 11.1 Helix + ZooKeeper

- **Helix 资源调度**：基于 Apache Helix 在 ZK 上调度 Segment；
- **故障切换**：Helix 自动检测 Broker / Server / Controller 故障；
- **Segment 副本**：默认 2-3 副本；
- **源码路径**：`pinot-controller/src/main/java/org/apache/pinot/controller/helix/core/PinotHelixResourceManager.java`。

### 11.2 Controller 高可用

- **Controller 集群**：多个 Controller 通过 ZK 选主；
- **故障切换**：Helix Leader Election；
- **源码路径**：`pinot-controller/src/main/java/org/apache/pinot/controller/helix/ControllerApplication.java`。

### 11.3 Server / Broker 高可用

- **无状态**：Broker 和 Server 都无状态，可任意扩缩；
- **故障切换**：Helix 检测心跳，自动迁移 Segment；
- **源码路径**：`pinot-broker/src/main/java/org/apache/pinot/broker/broker/helix/HelixBrokerStarter.java`。

### 11.4 数据可靠性

- **Segment 多副本**：默认 2-3 副本，分布在不同 Server；
- **持久化**：Segment 文件持久化到 HDFS / S3；
- **备份恢复**：Segment 是不可变的，备份即复制文件；
- **跨集群复制**：通过 Kafka MirrorMaker / Pulsar Geo-Replication 实现异地灾备。

## 十二、扩展性（MPP 专题深化）

Pinot 是 **分布式 OLAP 数据库**，但与 Doris / StarRocks 的"完整 MPP"有所不同。

### 12.1 集群架构

```
┌──────────────────────────────────────────────────────────────────┐
│              客户端连接任意 Broker 节点                            │
│                          │                                       │
│                          ▼                                       │
│             ┌────────────────────────────┐                       │
│             │  Broker（路由）              │                       │
│             │  + Broker × N（负载均衡）   │                       │
│             └──────┬─────────────────────┘                       │
│                    │ 路由查询                                     │
│        ┌───────────┼───────────┬──────────────┐                 │
│        ▼           ▼           ▼              ▼                 │
│    ┌───────┐   ┌───────┐   ┌───────┐     ┌───────┐              │
│    │Server │   │Server │   │Server │     │Server │              │
│    │Segment│   │Segment│   │Segment│     │Segment│              │
│    │ 1-N   │   │ 1-N   │   │ 1-N   │     │ 1-N   │              │
│    └───────┘   └───────┘   └───────┘     └───────┘              │
│                                                                 │
└──────────────────────────────────────────────────────────────────┘
```

### 12.2 节点角色

| 角色 | 数量 | 职责 |
| --- | --- | --- |
| **Controller** | 1+ | 集群协调 + Segment 管理 |
| **Broker** | 2+ | 查询路由 + 结果合并 |
| **Server** | 3+ | 数据存储 + 执行 |
| **Minion**（可选） | 0+ | 后台任务（purge / rollup） |

### 12.3 数据分布策略

- **Segment 分布**：每个 Segment 分配到多个 Server（默认 2-3 副本）；
- **Segment 选择**：Broker 根据 Helix 路由表选择最优 Server；
- **路由策略**：基于负载均衡 + 数据局部性；
- **Tag 策略**：通过 Helix Tag 划分硬件（SSD / HDD / CPU）；
- **Tier 策略**：热数据在 SSD 节点，冷数据在 HDD 节点；
- **源码路径**：`pinot-controller/.../helix/HelixInstanceConfigGenerator.java`。

### 12.4 多阶段聚合

V2 MSQE 支持 **多阶段聚合**：

- **阶段 0 (Local)**：每个 Server 在本地 Segment 上聚合；
- **阶段 1 (Exchange + Merge)**：Broker 跨 Server 收集局部结果；
- **阶段 2 (Final)**：可选最终聚合；
- **Broadcast Join**：V2 MSQE 支持小表广播；
- **Shuffle Join**：V2 MSQE 支持大表间按 Join Key shuffle；
- **源码路径**：`pinot-query-runtime/src/main/java/org/apache/pinot/query/runtime/operator/`。

### 12.5 资源隔离

- **Helix Tag**：通过 Tag 划分节点组（如 `gpu_node`、`cold_storage`）；
- **Segment 路由**：Broker 根据 Tag 选择 Server；
- **Minion Task**：通过 Minion 任务队列管理后台任务；
- **Tenant 概念**：`tenant_name` 用于隔离多个工作负载；
- **源码路径**：`pinot-controller/.../helix/SegmentAssignmentStrategyFactory.java`。

### 12.6 动态扩缩容

- **添加 Broker**：`ADD BROKER` → 自动加入集群；
- **添加 Server**：`ADD SERVER` → Helix 自动均衡 Segment；
- **移除 Broker/Server**：`REMOVE BROKER` / `REMOVE SERVER` → Helix 自动迁移；
- **Segment 重新均衡**：`REBALANCE` 命令触发 Segment 重新分配；
- **Tier 迁移**：`MOVE SEGMENT` 移动 Segment 到不同节点；
- **源码路径**：`pinot-controller/.../helix/SegmentRebalancer.java`。

### 12.7 Minion 后台任务

- **Purge Task**：根据 TTL 清理过期数据；
- **Rollup Task**：合并小 Segment；
- **Compaction Task**：优化 Segment；
- **Upsert Compaction**：合并 Upsert 实时数据；
- **源码路径**：`pinot-minion/src/main/java/org/apache/pinot/minion/`。

## 十三、性能与基准

### 13.1 内部基准

- **Star-Tree 基准**：对比不同聚合函数性能；
- **SSB / TPC-H**：标准 benchmark；
- **Pinot Battle**：与 Druid / ClickHouse 对比；
- **Streaming Benchmark**：实时摄入延迟对比。

### 13.2 性能数字（典型）

- **实时摄入延迟**：< 1 秒（Kafka → 可查）；
- **聚合查询**：P99 < 1 秒（10 亿行，Star-Tree 启用）；
- **Join 查询**：V2 MSQE P95 < 5 秒（千万级 JOIN）；
- **扫描吞吐**：单机 ~100M rows/s；
- **压缩比**：5-10x（典型）；
- **Star-Tree 加速**：相比原始数据 10-100x。

### 13.3 性能调优

- **Star-Tree 配置**：`dimensions` + `functionPairs` + `maxLeafRecords` 调优；
- **索引选择**：低基数列加 Dictionary / Bitmap，高基数加 Bloom Filter；
- **Segment 大小**：默认 500MB，过小导致调度开销；
- **Broker 配置**：`maxServerResponseSize`、`maxStreamingPendingMsgs`；
- **Server 配置**：`maxSegmentSize`、`maxParallelSegmentDownload`；
- **V1 vs V2**：V1 单表查询用 V1，复杂 JOIN 用 V2 MSQE。

## 十四、运维与可观测性

### 14.1 监控

- **Prometheus 集成**：Controller / Broker / Server 均暴露 `/metrics`；
- **Grafana 仪表盘**：官方提供；
- **Helix UI**：可视化集群状态；
- **审计日志**：Controller `audit.log`；
- **慢查询**：Pinot 自带的 slow query log；
- **Segment 状态**：`GET /segments/{tableName}`（REST API）；
- **源码路径**：`pinot-controller/.../api/RestletResource.java`。

### 14.2 备份与恢复

- **Segment 备份**：Segment 是不可变的，备份即复制 HDFS / S3 文件；
- **Controller Backup**：导出表配置 JSON；
- **Minion Purge**：清理过期 Segment；
- **跨集群复制**：通过 Kafka MirrorMaker 复制摄入层；
- **源码路径**：`pinot-controller/.../api/SegmentUploadRestletResource.java`。

### 14.3 升级与滚动

- **大版本**：滚动重启（先 Server 再 Broker 最后 Controller）；
- **数据兼容性**：新版本读旧版本 Segment 自动迁移；
- **回退**：保留旧二进制，重启即可读；
- **配置兼容**：`pinot-controller.conf` / `pinot-broker.conf` / `pinot-server.conf`。

### 14.4 故障排查

- **`GET /cluster/info`**：集群状态；
- **`GET /tables/{tableName}/health`**：表健康；
- **`GET /segments/{tableName}`**：Segment 状态；
- **Helix UI**：节点状态可视化；
- **常见问题**：Segment 缺失、Broker 路由失败、Star-Tree 失效、Kafka offset lag。

## 十五、生态与集成

### 15.1 数据接入

- **Kafka**：`kafka-0.8` / `kafka-1.x` / `kafka-2.x` ingestion；
- **Kinesis**：AWS Kinesis ingestion；
- **Pulsar**：Apache Pulsar ingestion；
- **Batch**：HDFS / S3 / 本地 ingestion；
- **JDBC Source**：MySQL binlog；
- **Pravega**：Pravera ingestion；
- **NFS**：本地文件 ingestion。

### 15.2 数据导出

- **Kafka Sink**：写入 Kafka；
- **S3 / HDFS Sink**：写入对象存储；
- **JDBC Sink**：写入关系数据库；
- **Pinot-Pinot Replication**：复制到另一个 Pinot 集群。

### 15.3 BI 与可视化

- **Apache Superset**：官方支持（JDBC）；
- **Tableau**：JDBC；
- **Mode / Redash**：JDBC；
- **Grafana**：JDBC；
- **Power BI**：JDBC；
- **Looker**：JDBC；
- **Plotly / Streamlit**：Python 客户端。

### 15.4 客户端

- **JDBC**：`pinot-jdbc-client`；
- **Python**：`pinot-python`（REST API）；
- **Java**：REST 客户端；
- **Go**：`go-pinot`（社区）；
- **Multi-Stage Engine 客户端**：标准 JDBC + gRPC；
- **Node.js**：`pinot-js-client`（社区）。

## 十六、核心论文与算法原理

1. **Apache Pinot 官方文档**：[docs.pinot.apache.org](https://docs.pinot.apache.org)；
2. **"Pinot: Realtime OLAP for 530 Million Users"**（LinkedIn Engineering Blog）；
3. **"Star-Tree Index: A Generic Hybrid Index for OLAP"**（LinkedIn 论文）；
4. **"Apache Druid vs Apache Pinot"**（社区对比文章）；
5. **"Real-time Analytics at Scale: Lessons from LinkedIn"**；
6. **C-Store 论文**："C-Store: A Column-oriented DBMS"（Stonebraker et al., VLDB 2005）；
7. **"MonetDB/X100"**（Boncz et al., CIDR 2005）—— 向量化执行；
8. **"Apache Calcite"**（查询优化框架）—— V2 MSQE 基于 Calcite；
9. **"Apache Helix"**（资源调度框架）；
10. **"Apache Kafka"**（流式存储）；
11. **"LSM-Tree 原始论文"**（O'Neil et al., 1996）；

## 十七、源码学习路径

### 17.1 必读模块清单（推荐顺序）

| 优先级 | 模块 | 路径 | 说明 |
| --- | --- | --- | --- |
| ★★★★★ | Segment | `pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/` | Segment 抽象 |
| ★★★★★ | Star-Tree | `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/startree/` | 核心索引 |
| ★★★★★ | Controller | `pinot-controller/src/main/java/org/apache/pinot/controller/helix/` | 集群协调 |
| ★★★★ | Broker | `pinot-broker/src/main/java/org/apache/pinot/broker/requesthandler/` | 路由 |
| ★★★★★ | Server | `pinot-core/src/main/java/org/apache/pinot/core/operator/` | 执行 |
| ★★★★ | Ingestion | `pinot-controller/.../ingestion/` | Kafka 摄入 |
| ★★★ | MSQE | `pinot-query-runtime/src/main/java/org/apache/pinot/query/runtime/` | V2 多阶段 |
| ★★★★ | Index | `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/indexsegment/` | 列级索引 |
| ★★★ | Dictionary | `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/dictionary/` | 字典编码 |

### 17.2 源码规模与语言

- **总 LOC**：~150 万行 Java；
- **Java 占比**：~95%；
- **辅助**：Python（test）、Maven（build）；
- **构建系统**：Maven（多模块）；
- **编译时长**：冷启动 15-30 分钟（多线程）。

## 十八、学习资源

- **官方文档**：[docs.pinot.apache.org](https://docs.pinot.apache.org)；
- **官方博客**：[linkedin.com/pulse/apache-pinot](https://www.linkedin.com/pulse/apache-pinot)；
- **GitHub**：[github.com/apache/pinot](https://github.com/apache/pinot)；
- **Slack**：[apache-pinot.slack.com](https://apache-pinot.slack.com)；
- **社区**：GitHub Discussions、Apache Pinot Meetup；
- **视频**：Pinot Meetup、CMU Database Group 演讲；
- **认证**：StarTree 商业培训。

## 十九、可移植性与部署

- **操作系统**：Linux（主）、macOS（开发）；
- **容器化**：官方 Docker 镜像（`apachepinot/pinot`）；
- **Helm Chart**：[Pinot Helm Chart](https://github.com/apache/pinot/tree/master/kubernetes)；
- **Operator**：[Pinot Kubernetes Operator](https://github.com/apachepinot/pinot-operator)（社区）；
- **Kubernetes**：完整 YAML 模板可用；
- **离线部署**：tar 包 + 配置即可；
- **云部署**：StarTree Cloud / AWS Marketplace / Azure Marketplace。

## 二十、变体与衍生项目

- **Apache Pinot**（开源，Apache 2.0）：核心；
- **StarTree**（商业）：Pinot 商业公司；
- **StarTree Cloud**：托管云服务；
- **AWS Marketplace Pinot**：AWS 部署镜像；
- **Azure Marketplace Pinot**：Azure 部署镜像；
- **Pinot 工具**：`pinot-tools`（CLI）、`pinot-admin`、`StarTree Studio`（GUI）；
- **生态扩展**：[startreedb/pinot-fork](https://github.com/startreedb/pinot)；
- **第三方集成**：[FiveTran Pinot Connector](https://fivetran.com/integrations/pinot)；

## 二十一、风险与陷阱

### 21.1 协议与兼容性陷阱

- **V1 不支持 JOIN**：必须用 V2 MSQE；
- **V2 MSQE 不完整**：某些 V1 特性 V2 暂不支持；
- **Star-Tree 失效**：GROUP BY 列超出 Star-Tree 配置维度；
- **Segment TTL 配置错误**：数据过期丢失；
- **Kafka offset reset**：摄入失败可能导致数据丢失；
- **Upsert 性能差**：upsert 实时表写入慢；
- **混合表（HYBRID）**：OFFLINE + REALTIME 拼接行为复杂；
- **Schema 演进**：字段类型变更限制多；
- **多值字段**：性能比标量字段差；
- **Dictionary 限制**：高基数列 Dictionary 内存爆炸；
- **Text Index 内存**：Lucene Text Index 占用大量堆内存；
- **Vector Index**（v1.3+）：实验性，配置复杂；

### 21.2 性能陷阱

- **无 Star-Tree 时聚合慢**：未预聚合导致全 Segment 扫描；
- **GROUP BY 维度过多**：Star-Tree 空间爆炸；
- **Bitmap Index 滥用**：高基数列不适用；
- **Bloom Filter 错配**：高基数列必备但写入慢；
- **Segment 太小**：调度开销主导；
- **Segment 太大**：单 Segment 查询慢；
- **多值字段过滤**：性能差（推荐先炸开）；
- **V1 JOIN 查询**：返回错误结果（V1 不支持 JOIN）；
- **V2 MSQE 调度开销**：小查询反而慢；
- **Broker 路由不均**：某些 Server 压力过大；
- **Server 内存不足**：Segment 加载失败；
- **Text Index**（Lucene）：文本搜索内存占用大；
- **Geospatial Index**：H3 / S2 计算密集；
- **Kafka 摄入卡顿**：消费组 lag 增长；

### 21.3 运维陷阱

- **Controller 单点**：早期 Controller 单点（v1.x 引入 HA）；
- **Helix ZK 压力大**：大量表导致 ZK 性能下降；
- **Segment 副本缺失**：Helix 故障后副本可能不全；
- **Minion 任务堆积**：后台任务调度慢；
- **Tier 配置错误**：热数据被错误路由到 HDD；
- **Kafka Topic 删除**：摄入中断；
- **S3 / HDFS 权限错误**：Segment 上传失败；
- **升级兼容性**：v0.x → v1.x 大版本升级需手工迁移 Segment；
- **集群节点配置漂移**：Controller / Broker / Server conf 不一致；
- **监控告警缺失**：未监控 ingestion lag / segment count；
- **TLS 配置错误**：Controller / Broker / Server 通信失败；
- **网络分区**：Helix 心跳超时导致误判；
- **冷启动延迟**：Segment 加载慢（首次查询）；
- **Minion 任务失败**：purge / rollup 失败导致数据膨胀；
- **Pinot V1 → V2 切换**：V2 MSQE 配置错误导致查询失败；

### 21.4 安全陷阱

- **默认无认证**：REST API / 客户端无鉴权（v1.x 引入 Basic Auth）；
- **明文密码**：配置文件中明文；
- **SQL 注入**：动态 SQL 拼接；
- **网络暴露**：Controller / Broker / Server 端口对外暴露；
- **RBAC 缺失**：用户 / 角色配置有限；
- **审计日志**：默认开启可能影响性能；
- **TLS / mTLS**：Kafka 摄入加密配置复杂；
- **Segment 数据加密**：静态加密需 HDFS 透明加密；

## 二十二、学习路径

### 22.1 入门（1 周）

1. 阅读 [docs.pinot.apache.org](https://docs.pinot.apache.org) Quick Start；
2. Docker Compose 启动 Pinot（Controller / Broker / Server）；
3. 用 `pinot-admin` 创建第一张表；
4. 创建 Kafka topic + 启动实时摄入；
5. 用 `pinot-cli` 查询数据；
6. 阅读 `pinot-segment-spi/.../Segment.java` 抽象；
7. 阅读 [Pinot 官方 Quick Start 视频](https://www.youtube.com/@ApachePinot)。

### 22.2 中级（2-4 周）

1. 部署完整集群（Controller × 1 + Broker × 2 + Server × 3）；
2. 创建离线表 + 实时表 + 混合表；
3. 配置 Star-Tree Index；
4. 测试 Bloom Filter / Range Index / Text Index；
5. 集成 Kafka 实时摄入（自定义摄入配置）；
6. 集成 Apache Superset；
7. 启用 Prometheus metrics + Grafana dashboard；
8. 阅读 `pinot-segment-local/.../startree/StarTreeBuilder.java`；
9. 阅读 `pinot-controller/.../PinotHelixResourceManager.java`；
10. 阅读 `pinot-broker/.../BaseBrokerRequestHandler.java`；
11. 性能测试：V1 单表查询 vs V2 MSQE JOIN 查询。

### 22.3 进阶（1-3 个月）

1. 部署生产级集群 + Minion；
2. 调优 Segment 大小 + Star-Tree 配置；
3. 实现 Tier 路由（热数据 SSD + 冷数据 HDD）；
4. 实现 Upsert 实时表；
5. 自研 Minion Task（自定义后台任务）；
6. 集成 Presto 联邦查询；
7. 阅读全部 `pinot-segment-local/`；
8. 阅读 `pinot-query-runtime/` 的 MSQE 实现；
9. 性能基准 vs Druid / ClickHouse / StarRocks；
10. 贡献 Pinot 社区（PR 新算子、新索引）；

## 二十三、与本项目的关联

`engineering/` 中**没有 Pinot 实现**，但有以下可借鉴模式：

- **Segment + 不可变列存** 与 `engineering/src/db/` 的 Buffer Pool 设计思想类似；
- **Star-Tree 预聚合** 与 `engineering/src/db/sql_engine.c` 的物化视图设计思想一致；
- **多种列级索引**（Bloom / Range / Bitmap / Text）与 `engineering/src/db/index/` 模块可对比学习；
- **三方组件（Controller + Broker + Server）** 与 `engineering/` 的 `db_server.c` + `catalog.c` + `bufmgr.c` 分层有相似之处；
- **Helix 资源调度** 与 `engineering/apps/` 的多副本数据库设计类似；
- **实时摄入**（Kafka → Pinot）与 `engineering/apps/` 实时数据接入可对比；
- **Minion 后台任务** 与 `engineering/src/db/` 的 WAL checkpoint / vacuum 设计思想一致；
- **V1 单阶段 vs V2 MSQE** 两种执行模型可应用到 `engineering/src/db/` 的执行器；
- **测试模式**：Pinot 的 `pinot-integration-tests/` 是高质量集成测试范本；
- **REST API 设计** 与 `engineering/apps/web/` 的 REST 设计可对比；
- **Segment 元数据**（startTime / endTime）与 `engineering/src/db/heapam.c` 的页面元数据设计类似；

## 二十四、参考资料

1. **Apache Pinot 官方仓库**：[github.com/apache/pinot](https://github.com/apache/pinot)；
2. **官方文档**：[docs.pinot.apache.org](https://docs.pinot.apache.org)；
3. **官方博客**：[linkedin.com/pulse/apache-pinot](https://www.linkedin.com/pulse/apache-pinot)；
4. **StarTree**：[startree.com](https://startree.com)；
5. **StarTree Cloud**：[startree.cloud](https://startree.cloud)；
6. **Pinot Helm Chart**：[github.com/apache/pinot/tree/master/kubernetes](https://github.com/apache/pinot/tree/master/kubernetes)；
7. **Pinot 实战**："Apache Pinot: Realtime OLAP for 530 Million Users"（LinkedIn）；
8. **Star-Tree 索引论文**："Star-Tree Index: A Generic Hybrid Index for OLAP"；
9. **Apache Helix 文档**：[helix.apache.org](https://helix.apache.org)；
10. **Apache Kafka 文档**：[kafka.apache.org](https://kafka.apache.org)；
11. **Apache Calcite 文档**：[calcite.apache.org](https://calcite.apache.org)；
12. **C-Store 论文**："C-Store: A Column-oriented DBMS"（Stonebraker et al., VLDB 2005）；
13. **MonetDB/X100 论文**："MonetDB/X100: Hyper-Pipelining Query Execution"（Boncz et al., CIDR 2005）；
14. **Pinot 与 Druid 对比**：docs.pinot.apache.org/community/druid；
15. **Pinot 与 ClickHouse 对比**：docs.pinot.apache.org/community/clickhouse；
16. **Pinot 用户案例**：[docs.pinot.apache.org/community/users](https://docs.pinot.apache.org/community/users)；
17. **Pinot 入门视频**：[YouTube Apache Pinot](https://www.youtube.com/@ApachePinot)；
18. **LinkedIn Engineering Blog**：[engineering.linkedin.com/blog/topic/pinot](https://engineering.linkedin.com/blog/topic/pinot)；
19. **Pinot Operator**：[github.com/apachepinot/pinot-operator](https://github.com/apachepinot/pinot-operator)；
20. **Pinot 中文社区**：csdn / zhihu Pinot 教程；
21. **Pinot 部署最佳实践**：[docs.pinot.apache.org/basics/getting-started/best-practices](https://docs.pinot.apache.org/basics/getting-started/best-practices)；
22. **Pinot 性能调优指南**：[docs.pinot.apache.org/operators/operating/best-practices/performance](https://docs.pinot.apache.org/operators/operating/best-practices/performance)；
23. **Pinot 摄入最佳实践**：[docs.pinot.apache.org/basics/data-import/pinot-stream-ingestion](https://docs.pinot.apache.org/basics/data-import/pinot-stream-ingestion)；
24. **Pinot Star-Tree 调优**：[docs.pinot.apache.org/basics/indexing/star-tree-index](https://docs.pinot.apache.org/basics/indexing/star-tree-index)；
25. **Pinot V2 MSQE**：[docs.pinot.apache.org/multi-stage-query-engine](https://docs.pinot.apache.org/multi-stage-query-engine)。
