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

## 二十四、Segment 构建流水线与 StarTree 多维预聚合

本节把 §7 的「有哪些索引」推进到「索引怎么被造出来、查询时怎么被用上」。本节所有源码路径均经 apache/pinot `master`（`1.7.0-SNAPSHOT`，属 §2 的 v1.x 系列）sparse checkout 逐条核对；Pinot 迭代很快，跨大版本路径可能微调（approximate）。

### 24.1 Segment 构建的两趟扫描

离线 Segment 的生成不是单趟，而是**统计 → 构建**两趟：

```text
// 伪代码：SegmentIndexCreationDriverImpl.build()
collectStatsAndIndexCreationInfo();      // 第 1 趟：只统计，不写索引
indexCreator.init(config, totalDocs, columnStatsMap, tempIndexDir);
for (row : recordReader) {                // 第 2 趟：逐行灌索引
    transformPipeline.processRow(row) -> indexCreator.indexRow(row);
}
handlePostCreation();                     // 补建非 DURING_SEGMENT_CREATION 的索引
```

第 1 趟由 `StatsCollector` 产出每列的 `ColumnStatistics`，其中 `getUniqueValuesSet()` 携带去重值集合、`getCardinality()` 给出基数；`BaseSegmentCreator` 用它**先建字典**（`dictionaryCreator.build(columnStatistics.getUniqueValuesSet())`），再进入逐行写入。这样倒排索引、Range Index（min/max）这类**依赖全列统计**的索引，才能在数据流过之前就确定长度并预分配缓冲。

代价与收益：读取量翻倍（`O(2·N)`），换来字典 / min-max / 基数这些**一次性即可算出的元信息**。Pinot 源码对此留有显式 TODO：`_columnStatisticsMap` 在整个构建期间通过 `ColumnStatistics` 让每列唯一值**常驻堆内存**，构建大 Segment 时这是主要的堆压力来源。

### 24.2 索引类型是 SPI 注册的，不是硬编码分支

「某列要建哪些索引」不写死在 switch 里，而是遍历注册表：

```text
// 伪代码：BaseSegmentCreator.getIndexCreatorsByColumn()
Map<IndexType, IndexCreator> creatorsByIndex = new HashMap<>();
for (IndexType t : IndexService.getInstance().getAllIndexes()) {
    if (t.getIndexBuildLifecycle() != IndexType.BuildLifecycle.DURING_SEGMENT_CREATION) continue;
    tryCreateIndexCreator(creatorsByIndex, t, context, config);
}
```

`IndexService` 是**唯一权威容器**（源码注释原话 "the ultimate container ... and the source of truth"），`StandardIndexes` 只提供类型安全的取用入口，避免调用方硬编码字符串 ID。实测已注册的标准索引 ID：`forward_index`、`dictionary`、`nullvalue_vector`、`bloom_filter`、`fst_index`、`ifst_index`、`inverted_index`、`json_index`、`range_index`、`text_index`、`vector_index`、`open_struct_index`。

`BuildLifecycle` 的意义在于：**只有 `DURING_SEGMENT_CREATION` 的索引才能在逐行数据流里被喂数据**，其余生命周期的索引由构建后的离线 pass 在 `handlePostCreation()` 阶段补齐。这也是 Pinot 能插入第三方自定义索引类型（`IndexPlugin`）的扩展点。

索引创建器按类型分目录而非平铺（相对 `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/`）：

| 索引 | 路径 |
| --- | --- |
| 正排 forward | `segment/creator/impl/fwd/`（`SingleValueSortedForwardIndexCreator` 等） |
| 倒排 inverted / range | `segment/creator/impl/inv/`（`BitmapInvertedIndexWriter` / `RangeIndexCreator` / `BitSlicedRangeIndexCreator`） |
| 空值向量 | `segment/creator/impl/nullvalue/NullValueVectorCreator.java` |
| 文本 text | `segment/creator/impl/text/LuceneTextIndexCreator.java` |
| Bloom | `segment/creator/impl/bloom/OnHeapGuavaBloomFilterCreator.java` |
| 单列打包 | `segment/creator/impl/ColumnIndexCreators.java` |

`ColumnIndexCreators` 把一列的全部状态打包成一个对象：`SegmentDictionaryCreator`（可为 null = raw 列）+ `NullValueVectorCreator`（可空 = 非 nullable）+ `List<IndexCreator>`，`seal()` 用 `_isSealed` 标志保证幂等。

### 24.3 逐行的「一次解码、多索引扇出」

真正吃性能的内核在 `SegmentColumnarIndexCreator`：**一个值只从 ColumnReader 取一次，字典 id 只算一次，然后广播给该列所有索引创建器**。

```java
// SegmentColumnarIndexCreator 的 INT 列类型化快路径（源码结构化改写）
for (int docId = 0; docId < numDocs; docId++) {
    if (columnReader.isNull(docId)) { /* 走 Object 路径 + 标记空值向量 */ continue; }
    int value  = columnReader.getInt(docId);                     // 类型化取值，免装箱
    int dictId = (dictionaryCreator != null)
                 ? dictionaryCreator.indexOfSV(value) : -1;      // 字典只查一次
    for (IndexCreator creator : creatorsByIndex) {                // 扇出到所有索引
        creator.addInt(value, dictId);
    }
}
```

两个要点：

- **免装箱**：`getValue(docId) → indexOfSV(Object) → add(Object, ...)` 会为每行每列分配一个 `Integer`；类型化路径直接走 `getInt/getLong/getFloat/getDouble`。源码注释明确标注该路径 allocation-free，并且在 reader 返回的物理类型与 schema 声明不一致时**在读取任何值之前**退回 Object 路径（`return false`，不产生任何副作用）。
- **dictId 与原值双通道**：`addInt(value, dictId)` 同时给出两样东西 —— 正排索引要 `value`，倒排索引要 `dictId`。低基数列的倒排因此退化成对小整数 id 集合求交，完全不碰原始字节。

单列代价 `O(N·I)`（`I` = 该列启用的索引个数），整个 Segment 累加为 `O(N · Σᵢ Iᵢ)`。`build()` 入口会检测 `_dataSource instanceof ColumnarSegmentCreationDataSource` 并切到列式构建路径。

### 24.4 特殊优化：仅为 text index 存在的常量正排

`SameValueForwardIndexCreator` 是包在真实 forward creator 外的常量壳，`isDictionaryEncoded()` 固定返回 `false`。它只在**该列只需要 text index**时启用：此时正排没有读取需求，`BaseSegmentCreator` 用 `calculateRawValueForTextIndex(...)` 造一个占位值替换掉真正的正排创建器，逐行不落任何数据。

### 24.5 StarTree v2：排序驱动的星型拓扑生成

⚠️ **重要版本事实**：**StarTree v1（`StarTreeBuilder` / `StarTreeIndexViewer` / `StarTreeIndexSpec`）已在当前 master 下线**。`pinot-segment-local/.../segment/local/startree/` 下现存 `OffHeapStarTree.java`、`OffHeapStarTreeNode.java`、`StarTreeBuilderUtils.java` 与 `v2/` 子包。§7.3 / §7.7 引用的 `StarTreeBuilder.java`、`StarTreeIndex.java` 属旧版本路径，读当前源码应改看 `v2/`。

拓扑**不是**「DFS 枚举所有维度组合」，而是「一次排序 + 递归切区间」：

1. **记录表示**：每条记录 = `int[numDimensions]`（各维度的**段内字典 id**）+ 指标值，定义在 `BaseSingleTreeBuilder` 的 `Record` 内。
2. **全量排序**：比较器逐维比 `o1._dimensions[i]` 与 `o2._dimensions[i]`，`i` 从 0 到 `numDimensions-1`，即**字典序**。排序后把相同维度前缀的记录聚成一条（`sortAndAggregateSegmentRecords`）。
3. **递归切分**：

```text
// 伪代码：BaseSingleTreeBuilder.constructStarTree(node, startDocId, endDocId)
childDim = node._dimensionId + 1;
if (childDim == numDimensions) return;                  // 维度用尽，停止

children = constructNonStarNodes(start, end, childDim); // 按 childDim 的 id 切连续同值块
node._children = children;

if (!skipStarNodeCreationForDimensions.contains(childDim) && children.size() > 1) {
    children.put(ALL, constructStarNode(start, end, childDim));   // 星节点只在"有分叉"时才建
}

for (child : children.values()) {                     // 超阈值才继续往下裂
    if (child._endDocId - child._startDocId > maxLeafRecords) {
        constructStarTree(child, child._startDocId, child._endDocId);
    }
}
```

三个反直觉但关键的判定：

- **星节点只在 `children.size() > 1` 时创建**。若某维度在当前前缀内只有一个取值，星节点只是那份数据的另一份副本，没有任何过滤/聚合收益。
- **`skipStarNodeCreationForDimensions` 允许对指定维度强制不建星节点**。典型场景是某维度基数极高且几乎不被过滤/分组，为它建星节点会让节点数爆炸；代价是该维度永远无法被星节点覆盖。
- **`maxLeafRecords` 是递归深度闸门**，不是叶子容量上限 —— 区间里超过阈值就继续裂。`StarTreeV2BuilderConfig.DEFAULT_MAX_LEAF_RECORDS = 10_000`。

4. **聚合文档回填**决定每个内部节点对应哪条预聚合记录：

```text
// 伪代码：BaseSingleTreeBuilder.createAggregatedDocs(node)
if (node 是叶子) {
    if (区间只有 1 条)  直接复用该条原始记录;            // 单文档直通，省一次聚合
    else                逐条 mergeStarTreeRecord(...) 后落盘;
} else {
    if (存在 star 子节点) 直接复用 star 子节点的聚合记录;  // 命中即短路，父节点不用重算
    else                  合并全部非 star 子节点的聚合记录后落盘;
}
// 所有分支共同的后处理：把 node._dimensionId 之后的维度置为 STAR 占位
for (i = node._dimensionId + 1; i < numDimensions; i++) {
    record._dimensions[i] = StarTreeV2Constants.STAR_IN_FORWARD_INDEX;   // 常量值 = 0
}
```

`STAR_IN_FORWARD_INDEX = 0` 是**保留的字典 id**：0 号槽恒为星节点占位，普通维度值不会撞上它（源码注释显式声明这条不变量）。空值处理上，null 维度值被分配到**最后一个真实 id 之后**的保留槽，使 null 排在一切实值之后并自成一个树节点，而不是折进该列的默认空值里。

5. **多树 → 单文件**：一份表配置可产出多套去重后的 builder config（`StarTreeBuilderUtils.generateBuilderConfigs`）。`MultipleTreesBuilder.build()` 把每棵树写到临时目录，`StarTreeIndexSeparator` 再把它们从共用文件里拆出来，`StarTreeIndexCombiner` 合并成最终的 `star_tree.index` + `star_tree_index_map`（文件名取自 `StarTreeV2Constants.INDEX_FILE_NAME` / `INDEX_MAP_FILE_NAME`）。对应元数据键：`split.order`、`function.column.pairs`、`max.leaf.records`、`skip.star.node.creation`、`null.handling.enabled`（统一挂在 `startree.v2.*` 前缀下）。

### 24.6 查询时的星树 traversal

读取侧在 `pinot-core/.../core/startree/`。`ImmutableSegmentImpl.getStarTrees()` 返回 `List<StarTreeV2>`；`StarTreeFilterPlanNode.run()` 产出 `StarTreeFilterOperator`。核心思想是**用星树把满足谓词的行压缩成 bitmap，剩下的谓词交给常规索引**：

```text
// 伪代码：StarTreeFilterOperator 的遍历（对齐源码类注释的七步）
// 输入：Map<column, List<CompositePredicateEvaluator>> filters、Set<String> groupByColumns
DFS(node):
    if (当前维度有谓词) {
        // 1) 算匹配的字典 id，只下钻这些子节点
        matched = { id : any(p.apply(id)) for id in node 的字典 id }
        visit(matched)
    } else if (当前维度 ∈ groupByColumns) {
        // 2) 无谓词但要分组 → 下钻全部非星子节点（星节点无法表达该维度分组）
        visit(所有非 star 子节点)
    } else {
        // 3) 既无谓词也不分组 → 直接用星节点；没有星节点才展开全部非星子节点
        if (存在 star 子节点) 累加该节点的预聚合文档
        else                  visit(所有非 star 子节点)
    }
    if (谓词全部匹配 && groupBy 全部匹配) { 累加 node 的 aggregatedDocId; return; }
    if (是叶子) 累加 [startDocId, endDocId) 的全部 docId;
    if (叶子仍有剩余谓词) 把该列记入 remainingColumns（需单独 BaseFilterOperator 兜底）

// 收尾
matching = BitmapBasedFilterOperator(docs, exclusive=false, numDocs);
result   = matching AND (对每个 remainingColumn 各建一个 BaseFilterOperator);
```

第 1 步的 `p.apply(id)` 是 `CompositePredicateEvaluator.apply(int dictId)`：它把多个谓词 evaluator **OR** 起来，每个元素自带一个取反位标志，从而支持形如 `(d1 > 10 OR NOT d1 > 50)` 的组合（源码注释给的正是这个例子）。

复杂度收益：设原始段 `N` 行、压缩后命中 `M` 个 docId、剩余谓词列 `R` 个。星树遍历为 `O(V_tree + M)`（`V_tree` 为树节点数，与 `N` 无关），逐谓词的字典求值为 `O(cardinalityᵢ)`；对比无星树的 `O(N·R)`。**加速比上限就是 `N/M`**，所以星树只对高选择性、高重复度的 GROUP BY 有效 —— 这正是 §13.2「10-100x」数字的来源，也是 §21.2「GROUP BY 维度过多 → Star-Tree 空间爆炸」告警的技术根因。

### 24.7 空间/精度权衡

树节点数与聚合记录数的粗略上界：

$$
\text{nodes} \;\lesssim\; \sum_{d=0}^{D-1} b(d)\cdot \min\!\big(\text{card}(D_d),\; N / \text{maxLeafRecords}^{\,D-d}\big)
$$

其中 `b(d) ∈ {0,1}` 是第 `d` 层是否建星节点的系数（由 `children.size() > 1` 与 `skipStarNodeCreationForDimensions` 共同决定）。`maxLeafRecords` 越小 → 树越深、节点越多、扫描越短但空间与构建时间线性上升；越大 → 叶更肥、省空间但要回落到原始记录。

| 旋钮 | 收紧（值变小 / 维度加多） | 放松（值变大 / 维度去掉） |
| --- | --- | --- |
| `maxLeafRecords` | 树更深、节点更多、命中更早短路 | 叶更肥、省空间、查询回落原始扫描 |
| `dimensionsSplitOrder` | 前置高基数维度 → 每层分叉多、树大 | 前置低基数维度 → 树小，但高基数维度只能落在叶子 |
| `skipStarNodeCreationForDimensions` | 树更小 | 该维度可被星节点覆盖，覆盖更早 |

**排序由 `dimensionsSplitOrder` 决定，而非配置中 `dimensions` 的书写顺序**：`_numDimensions = dimensionsSplitOrder.size()`，比较器严格按 `i = 0..numDimensions-1` 逐维比较，因此该顺序**直接决定树的形状与高度**。这是 StarTree 调优中最容易踩的一环。

源码路径（本节，approximate，同 §24 引言的版本基线）：

- `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/segment/creator/impl/SegmentIndexCreationDriverImpl.java`
- `.../segment/creator/impl/SegmentColumnarIndexCreator.java`
- `.../segment/creator/impl/BaseSegmentCreator.java`
- `.../segment/creator/impl/ColumnIndexCreators.java`
- `.../segment/local/startree/v2/builder/{BaseSingleTreeBuilder, OnHeapSingleTreeBuilder, OffHeapSingleTreeBuilder, MultipleTreesBuilder, StarTreeIndexCombiner, StarTreeIndexSeparator, StarTreeV2BuilderConfig}.java`
- `.../segment/local/startree/{StarTreeBuilderUtils, OffHeapStarTree, OffHeapStarTreeNode}.java`
- `pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/index/startree/{StarTreeV2, StarTreeNode, StarTreeV2Constants, StarTreeV2Metadata}.java`
- `pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/index/{IndexService, IndexType, StandardIndexes}.java`
- `pinot-core/src/main/java/org/apache/pinot/core/startree/{StarTreeFilterOperator, StarTreeUtils, CompositePredicateEvaluator}.java`
- `pinot-core/src/main/java/org/apache/pinot/core/startree/plan/StarTreeFilterPlanNode.java`

## 二十五、实时摄入、Flush 阈值与 Upsert 可见性模型

§10 只说了「Upsert 基于 PK」；本节给出摄入侧的真实控制流。源码路径同样按 §24 引言的版本基线核对。

### 25.1 可变段 → 不可变段的边界由三级阈值决定

`_segmentMaxRowCount`（可变段的行数上限）在 server 侧按三级回退确定：

```text
// 伪代码：RealtimeSegmentDataManager 初始化（源码结构化改写）
int n = segmentZKMetadata.getSizeThresholdToFlushSegment();  // ① ZK 元数据（由 Controller 按段定制）
if (n <= 0) n = streamConfig.getFlushThresholdRows();        // ② 表配置里的流级阈值
if (n <= 0) n = StreamConfig.DEFAULT_FLUSH_THRESHOLD_ROWS;   // ③ 内置默认
_segmentMaxRowCount = n;
mutableSegmentConfig.setCapacity(n);                         // 可变段按此容量预分配
```

⚠️ **命名陷阱**：ZK 字段名是 `Segment.Realtime.FLUSH_THRESHOLD_SIZE`，getter 是 `getSizeThresholdToFlushSegment()`，但它存的**是行数不是字节数** —— `SizeBasedSegmentFlushThresholdComputer` 里 `int rowsThreshold = committingSegmentZKMetadata.getSizeThresholdToFlushSegment();` 紧接着就以 "rows threshold" 打日志。按名字理解会算错容量。

同一行的 `.setCapacity(_segmentMaxRowCount)` 说明阈值不只是「什么时候封段」，还**决定可变段各索引的初始容量**——所以阈值设小会让可变段频繁重建。

### 25.2 seal 判定：五条件或

```text
// 伪代码：RealtimeSegmentDataManager.shouldStopWaitingForOomProtection()（源码结构化改写）
if (_shouldStop || _state != State.INITIAL_CONSUMING) return true;
return now() >= _consumeEndTime                  // ① 时间阈值（streamConfig.getFlushThresholdTimeMillis()）
    || _numRowsIndexed >= _segmentMaxRowCount    // ② 行数阈值
    || _endOfPartitionGroup                      // ③ 消费组内所有分区都已被本段覆盖
    || _forceCommitMessageReceived               // ④ Controller 强制提交
    || !canAddMore();                            // ⑤ 内存 / 配额保护
```

注意 `_numRowsIndexed`（已**入索引**的行数）与 `_numRowsConsumed`（已消费行数）是两个量：畸形数据会在消费与入索引之间被丢弃，因此实际封段点由 `Indexed` 决定。

### 25.3 自适应阈值：按上一段实测「行数/字节」比反推

固定阈值在高吞吐表上要么封出过多小段、要么封出超大段。Controller 侧的 `SizeBasedSegmentFlushThresholdComputer` 用上一段的实测数据反推本段阈值：取 `segmentRowsToSizeRatio`（行/字节）与目标 `desiredSegmentSizeBytes`，导出 `rowsThreshold`。两条退路：

- 上一段缺少「行数/大小」比（老版本段）→ 直接沿用其 `getSizeThresholdToFlushSegment()`；
- 当前段 `rowsIndexed < rowsThreshold && size < desiredSegmentSizeBytes` → 判定为「没吃满」，直接复用上一段阈值，不重新计算。

阈值算完后写回 ZK（`SegmentZKMetadata.setSizeThresholdToFlushSegment`），并通过 `PinotLLCRealtimeSegmentManager` 打点到 `ControllerGauge.NUM_ROWS_THRESHOLD`。策略接口是 `FlushThresholdUpdater`，有 `Default` / `Fixed` / `SegmentSizeBased` 三种实现，由 `FlushThresholdUpdateManager` 按 stream config 选择。

### 25.4 消费串行化与「封段前提前放行」

同一消费组内**任何时刻只允许一个 Segment 处于 `INITIAL_CONSUMING`**，由 `ConsumerCoordinator` 的信号量保证（构造参数 `enforceConsumptionInOrder`）。若要 seal 正在消费的段，先要拿到其 `UnsealedRelease(segmentName, stoppedAtOffset)` 记录 —— 这条记录同时服务于后续的「段完成协议」，让被 seal 的段在 ZK 上留下确定的 offset 边界。

超时与容错常量（`RealtimeSegmentDataManager` 侧实测）：`MINIMUM_CONSUME_TIME_MINUTES = 10`（消费不足此时长会被视为异常）、`BUILD_TIME_LEASE_SECONDS = 30`（构建期租约）、`MAX_CONSECUTIVE_ERROR_COUNT = 5`、`CONSUMER_THREAD_INTERRUPT_INTERVAL_MS = 10000`。Controller 侧另有 `MAX_SEGMENT_COMPLETION_TIME_MILLIS`（默认 300_000，即 5 分钟）。

### 25.5 Upsert：哈希表 + RoaringBitmap 失效，**不是 MVCC**

⚠️ **纠正一个常见误解**：Pinot 的 Upsert **不维护 `validFrom` / `validTo` 的 MVCC 版本链**。它的可见性模型是「**PK → 最新记录位置**的哈希表 + `validDocIds` 位图失效」：

```text
// 核心状态（ConcurrentMapPartitionUpsertMetadataManager）
ConcurrentHashMap<hashedPrimaryKey, RecordLocation> _primaryKeyToRecordLocationMap;
RecordLocation = (IndexSegment segment, int docId, Comparable comparisonValue)

// 伪代码：addRecord 简化（源码结构化改写）
for (RecordInfo r : recordInfoIterator) {           // RecordInfo = (PK, docId, comparisonValue, isDeleteRecord)
    newValue = r.getComparisonValue();
    map.compute(hash(r.getPrimaryKey()), (pk, cur) -> {
        if (cur == null) return new RecordLocation(segment, r.getDocId(), newValue);   // 新 PK
        if (cur.segment() == segment) {               // 同段：新旧 docId 都在内存可变段里
            if (newValue.compareTo(cur.comparisonValue()) >= 0) {   // 相等也取新的（保持 docId 单调）
                replaceDocId(segment, validDocIds, queryableDocIds, cur.docId(), r.getDocId(), r);
                return new RecordLocation(segment, r.getDocId(), newValue);
            }
            return cur;                               // 乱序旧值，丢弃
        }
        // 跨段：新记录胜出时，把旧段对应 docId 从旧段的 validDocIds 里摘掉
        if (newValue.compareTo(cur.comparisonValue()) >= 0) {
            invalidate(oldSegment, cur.docId());
            return new RecordLocation(segment, r.getDocId(), newValue);
        }
        return cur;
    });
}

// 失效动作（BasePartitionUpsertMetadataManager.replaceDocId）
UpsertUtils.doRemoveDocId(oldSegment, oldDocId);                       // 旧段 validDocIds 清位
UpsertUtils.doAddDocId(validDocIds, queryableDocIds, newDocId, info);  // 新段置位
```

由此推出四条**工程性质**：

- **比较器是「大于等于取新」**：乱序到达的旧事件被直接丢弃（`_largestSeenComparisonValue` 记录见过的最大值），因此 Upsert 表要求上游提供单调的比较列（event time / sequence），否则旧事件会污染结果。
- **可见性是位图级 O(1)**：`ThreadSafeMutableRoaringBitmap` 上摘一位即可，代价与「该 PK 历史上出现过多少次」无关 —— 这是它比 MVCC 便宜得多的原因，也是 §10「Pinot 放弃强事务」的具体兑现方式。
- **元数据本身有 TTL**：`doRemoveExpiredPrimaryKeys()` 以 `_largestSeenComparisonValue` 为原点，用 `_metadataTTL` / `_deletedKeysTTL` 剪掉过期条目，防止哈希表无限增长。
- **一致性删除是独立变体**：`ConcurrentMapPartitionUpsertMetadataManagerForConsistentDeletes` 处理 `RecordInfo.isDeleteRecord()`，语义更强。

**Partial upsert** 走另一条路：`PartialUpsertHandler` 用配置好的 merge 策略把新行与旧行合并（可用 `PartialUpsertMerger` 实现整行级自定义合并，行级 merger 优先级高于列级 merger）。去重（dedup）同理但不维护 comparison value，见 `segment/local/dedup/`。

### 25.6 可变段的可变索引

可变段位于 `pinot-segment-local/.../indexsegment/mutable/MutableSegmentImpl.java`，其索引实现全在 `pinot-segment-spi/.../index/mutable/`：`MutableDictionary`、`MutableForwardIndex`、`MutableInvertedIndex`、`MutableTextIndex`、`MutableJsonIndex`，以及承载可见性的 `ThreadSafeMutableRoaringBitmap`。封段后由 `MultipleTreesBuilder` / `BaseTableDataManager` 走 §24 的离线构建路径落成不可变段。

源码路径（本节）：

- `pinot-core/src/main/java/org/apache/pinot/core/data/manager/realtime/{RealtimeSegmentDataManager, ConsumerCoordinator, RealtimeTableDataManager, SegmentCommitterFactory}.java`
- `pinot-controller/src/main/java/org/apache/pinot/controller/helix/core/realtime/PinotLLCRealtimeSegmentManager.java`
- `pinot-controller/src/main/java/org/apache/pinot/controller/helix/core/realtime/segment/{FlushThresholdUpdater, DefaultFlushThresholdUpdater, FixedFlushThresholdUpdater, SegmentSizeBasedFlushThresholdUpdater, SizeBasedSegmentFlushThresholdComputer}.java`
- `pinot-controller/src/main/java/org/apache/pinot/controller/validation/RealtimeSegmentValidationManager.java`
- `pinot-common/src/main/java/org/apache/pinot/common/metadata/segment/SegmentZKMetadata.java`
- `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/upsert/{BasePartitionUpsertMetadataManager, ConcurrentMapPartitionUpsertMetadataManager, RecordInfo, UpsertUtils, UpsertContext, PartialUpsertHandler}.java`
- `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/dedup/PartitionDedupMetadataManager.java`
- `pinot-segment-local/src/main/java/org/apache/pinot/segment/local/indexsegment/mutable/MutableSegmentImpl.java`
- `pinot-segment-spi/src/main/java/org/apache/pinot/segment/spi/index/mutable/`

## 二十六、MSQE 规划器规则链与 Filter/GroupBy 执行优化

§9.5 只列了优化项的名字，本节补上**规则链怎么装配**与**判定条件**。

### 26.1 规则集是 SPI：6 个 Phase 的分阶段优化

V2 MSQE 不用自研 `RuleExecutor`，而是把规则集做成了可插拔的 SPI。装配链是三段：

```text
META-INF/services/org.apache.pinot.query.planner.spi.RuleSetCustomizer
        │  ServiceLoader 发现（context classloader + 插件 classloader 双通道，按类名去重）
        ▼
PinotRuleSet.loadFromServiceLoader()  →  PinotRuleSet(rulesFor(Phase))
        │  按 Phase 分发
        ▼
DefaultRuleSetCustomizer.customize(phase, rules)  →  填充 PinotQueryRuleSets.{PHASE}_RULES
        │  按序交给 Calcite VolcanoPlanner（FILTER_PUSHDOWN / PROJECT_PUSHDOWN 用 RuleCollection 单指令多规则）
        ▼
VolcanoPlanner 各优化阶段
```

实测的 6 个 Phase 与规则来源：

| Phase | 规则集常量 | 职责 |
| --- | --- | --- |
| `BASIC` | `BASIC_RULES` | Calcite 通用算子改写 |
| `FILTER_PUSHDOWN` | `FILTER_PUSHDOWN_RULES` | 谓词下推（单条 HepInstruction 尽量推到底） |
| `PROJECT_PUSHDOWN` | `PROJECT_PUSHDOWN_RULES` | 投影下推与合并 |
| `PRUNE` | `PRUNE_RULES` | 消除冗余 Project/Filter/Aggregate |
| `POST_LOGICAL` | `POST_LOGICAL_RULES` | 插入 Exchange、消除 Exchange、Literal 求值 |
| `POST_LOGICAL_PHYSICAL_OPT` | `PINOT_POST_RULES_V2` | 物理优化器开启时的 Pinot 后置规则 |

`PinotRuleSet` 在 Phase 缺失默认值时**显式抛异常**而非静默降级（`DefaultRuleSetCustomizer` 的 `default:` 分支抛 `IllegalStateException`），新增 Phase 时强制补齐规则。

### 26.2 实测规则清单（节选，均为当前 master）

**FILTER_PUSHDOWN**：`PinotFilterIntoJoinRule`、`FilterAggregateTransposeRule`、`FilterSetOpTransposeRule`、`FilterProjectTransposeRule`。

**PROJECT_PUSHDOWN**：`ProjectFilterTransposeRule`、`PinotProjectJoinTransposeRule`、`ProjectMergeRule`。

**PRUNE**：`AggregateProjectMergeRule`、`ProjectMergeRule`、`ProjectRemoveRule`、`FilterMergeRule`、`AggregateRemoveRule`、`AggregateProjectPullUpConstantsRule.Config.ANY`。最后一条值得单独一提：当 Aggregate 输入可证某列为常量（典型来自等值过滤）时，把它从 GROUP BY key 里摘掉，**直接缩窄 shuffle key 宽度**；源码注释给的场景就是 `WHERE tenant_id = 'X' GROUP BY tenant_id, ...` 这类多租户查询。

**POST_LOGICAL**（顺序即优先级）：

```text
PinotSortExchangeNodeInsertRule, PinotSortExchangeCopyRule,
PinotSingleValueAggregateRemoveRule,
PinotJoinExchangeNodeInsertRule,
PinotAggregateExchangeNodeInsertRule.{SortProjectAggregate, SortAggregate, WithoutSort},
PinotWindowSplitRule, PinotWindowExchangeNodeInsertRule, PinotSetOpExchangeNodeInsertRule,
PinotJoinToDynamicBroadcastRule,          // exchange 插入之后再判定能否降级为广播
PinotExchangeEliminationRule,             // 存在重复数据时消除 exchange
CoreRules.FILTER_REDUCE_EXPRESSIONS,      // 常量求值
PinotTableScanConverterRule               // 最后才转成物理扫描节点
```

`PinotAggregateExchangeNodeInsertRule` 的三个变体对应三种聚合形态（带排序的聚合 / 排序聚合 / 无排序聚合），这是 MSQE 把「排序 + 聚合」下推到 leaf 阶段的关键；而 `PINOT_POST_RULES_V2` 用 `PinotLogicalAggregateRule` 的对应变体取代它们，用于启用物理优化器的路径。

「先插 Exchange 再消除 Exchange」的两段式是有意设计：`PinotExchangeEliminationRule` 的存在说明存在可以省掉的 shuffle —— 广播已覆盖全数据的 join 就不必再发一次。

### 26.3 执行侧：Filter 的优先级重排

`pinot-query-runtime/.../query/runtime/operator/` 是 MSQE 的运行时算子树（`OpChain`、`LeafOperator`、`FilterOperator`、`MailboxSendOperator` / `MailboxReceiveOperator`、`exchange/`、`groupby/`、`join/`，以及 `MultistageAggregationExecutor` / `MultistageGroupByExecutor`）。谓词的物理下推发生在 server 侧 `pinot-core/.../core/operator/filter/`。

**核心优化：AND 的多个叶子算子按代价重排，索引算子优先**，从而让后续算子处理尽可能少的 docId：

```text
// 伪代码：FilterOperatorUtils.reorderAndFilterChildOperators()（源码结构化改写）
sort by getPriority(o):
    if (o instanceof PrioritizedFilterOperator && 有显式优先级) return 该优先级;   // 0
    SortedIndexBasedFilterOperator          -> HIGH_PRIORITY      = 0
    BitmapBasedFilterOperator | InvertedIndexFilterOperator -> MEDIUM_PRIORITY = 100
    RangeIndexBased | TextMatch | JsonMatch | H3Index | H3Inclusion -> LOW_PRIORITY = 200
    AndFilterOperator                       -> AND_PRIORITY      = 300
    OrFilterOperator                        -> OR_PRIORITY       = 400
    ScanBasedFilterOperator                 -> SCAN_PRIORITY     = 500 + (多值 ? 50 : 0)
    ExpressionFilterOperator                -> EXPRESSION_PRIORITY = 1000
    兜底                                    -> UNKNOWN_FILTER_PRIORITY = 10000

// 常量出处：PrioritizedFilterOperator
```

`+ 50` 是多值列惩罚项（`getScanBasedFilterPriority`），可用 `queryContext.isSkipScanFilterReorder()` 整体关掉重排。重排前会先剔除 `MatchAllFilterOperator` / `EmptyFilterOperator` 这类零成本算子。

### 26.4 `BitmapBasedFilterOperator`：三值语义的位图

```text
// 伪代码：BitmapBasedFilterOperator（按源码类注释结构化改写）
状态：_docIds（真值集合）、_exclusive（取反标志）、_nullBitmap（UNKNOWN 集合）
不变量：三者必须划分全集 —— _nullBitmap ⊆ _docIds（exclusive 时）或 ∩ _docIds = ∅（非 exclusive 时）

// FilterPlanNode.constructPhysicalOperator 中的典型用法
IS_NULL 谓词 → new BitmapBasedFilterOperator(nullValueVector.getNullBitmap(), false, numDocs)
//                空值向量直接就是一个 RoaringBitmap，无需逐行求值

// 能力接口
canOptimizeCount()  -> 该算子可被上层「提前终止计数」的优化命中
getNumMatchingDocs() -> O(1) 计数（缓存的 cardinality）
canProduceBitmaps()  -> 可作为位图生产者参与 §26.3 的重排
getBitmaps()         -> 交出内部位图
```

这解释了 §7.4 表格里为什么 Bloom Filter 标「高基数等值」、Bitmap 标「低基数倒排」：`BitmapBasedFilterOperator` 走的是**结果缓存**路线，而 `InvertedIndexFilterOperator` 走的是**在线求交**路线，两者在 §26.3 的优先级表里同为 `MEDIUM_PRIORITY`，由具体谓词决定谁胜出。

### 26.5 GroupBy：容量启发式、trim 阈值与有序/无序双路径

Pinot 的 GroupBy **始终是哈希表（`IndexedTable`）**，不存在「sort-based 还是 hash-based」的二选一；真正的分岔在**是否带 `ORDER BY`**：

```text
// 伪代码：SimpleIndexedTable.upsert()（源码结构化改写）
if (_hasOrderBy) {                        // SELECT ... GROUP BY ... ORDER BY ...
    addOrUpdateRecord(key, record);        // 全量保留，先排序再截断
    if (_lookupMap.size() >= _trimThreshold) resize();
} else {                                  // 无 ORDER BY
    if (_lookupMap.size() < _resultSize) addOrUpdateRecord(key, record);
    else                                  updateExistingRecord(key, record);   // 只更新已有 key
}
```

无 `ORDER BY` 路径是**提前停止接纳新 key**（`_resultSize` 即 `numGroupsLimit`）：一旦分组数触顶，后来出现的全新 key 直接丢弃，已有的 key 继续累加。这让无序聚合的内存严格有界，代价是高基数场景下结果不再精确 —— 想保精度就得调大 `numGroupsLimit`。

两个容量公式（`GroupByUtils`，实测常量）：

$$
\text{tableCapacity}(limit, minNumGroups) = \max(5 \cdot limit,\; minNumGroups),\quad \text{DEFAULT\_MIN\_NUM\_GROUPS} = 5000
$$

$$
\text{trimThreshold} = \max(\text{configured},\; 2 \cdot \text{trimSize});\quad
\text{若 } \text{configured} \le 0 \text{ 或 } > 10^{9} \text{ 或 } \text{trimSize} > 5\times10^{8} \;\Rightarrow\; \text{trim 关闭}
$$

`5·limit` 的余量是为「分组数最终超过 limit 后还要继续累加已有分组」留的；`2·trimSize` 的下限是为「resize 时的拷贝不致抖动」留的。可用的表实现：`SimpleIndexedTable`（单线程）、`ConcurrentIndexedTable`（读写锁 + 批量 resize）、`DeterministicConcurrentIndexedTable`（结果顺序确定）、`UnboundedConcurrentIndexedTable`。

源码路径（本节）：

- `pinot-query-planner/src/main/java/org/apache/pinot/query/planner/rules/{PinotRuleSet, DefaultRuleSetCustomizer}.java`
- `pinot-query-planner/src/main/java/org/apache/pinot/calcite/rel/rules/PinotQueryRuleSets.java`
- `pinot-query-planner/src/main/java/org/apache/pinot/calcite/rel/rules/{PinotAggregateExchangeNodeInsertRule, PinotExchangeEliminationRule, PinotLogicalAggregateRule, PinotTableScanConverterRule}.java`
- `pinot-query-runtime/src/main/java/org/apache/pinot/query/runtime/operator/{OpChain, LeafOperator, FilterOperator, MailboxSendOperator, MailboxReceiveOperator}.java`
- `pinot-core/src/main/java/org/apache/pinot/core/plan/FilterPlanNode.java`
- `pinot-core/src/main/java/org/apache/pinot/core/operator/filter/{FilterOperatorUtils, PrioritizedFilterOperator, BitmapBasedFilterOperator}.java`
- `pinot-core/src/main/java/org/apache/pinot/core/data/table/{SimpleIndexedTable, ConcurrentIndexedTable, DeterministicConcurrentIndexedTable}.java`
- `pinot-core/src/main/java/org/apache/pinot/core/util/GroupByUtils.java`

## 二十七、参考资料

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
