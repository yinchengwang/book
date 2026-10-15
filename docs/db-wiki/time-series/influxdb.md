# InfluxDB Wiki（时序数据库事实标准）

> 参考版本：InfluxDB v2.7 LTS（commit `0ccd35c253`，2023-11 时代），Go + Rust 实现，TSM1 存储引擎 + TSI1 倒排索引，Flux + InfluxQL 双查询语言。

## 一、项目定位

InfluxDB 是 **InfluxData** 开源的 **时序数据库（Time-Series Database, TSDB）**，以 **"专为时序优化的存储引擎 + 完整的时序生态工具链"** 著称，是 **DevOps 监控 / 物联网遥测 / 实时分析** 场景的事实标准之一。它把 **TSM（Time-Structured Merge Tree）存储引擎、TSI（Time-Structured Index）倒排索引、Continuous Query 降采样、Retention Policy 数据生命周期** 集成到同一个产品里，并搭配 **Telegraf（采集器）、Chronograf（可视化）、Kapacitor（流式处理）** 形成完整的 **TICK / IFX 栈**。

- **痛点**：
  - 关系数据库（MySQL/PG）按时间分区后，索引膨胀，写入吞吐迅速下降；
  - 通用 NoSQL（MongoDB / HBase）缺乏时序专用压缩（Delta-of-Delta、Gorilla），存储成本高；
  - 大数据方案（Hive / Druid）实时性不足，秒级聚合响应不可达；
  - 需要 **Metrics / Events / Traces / IoT Sensor / APM / 业务 KPI** 等典型时序场景；
  - 需要 **Prometheus 远程写入**、**Grafana 仪表盘**、**SQL-like 查询**、**数据降采样与冷热分层**等完整生态；
- **典型用户**：SRE / DevOps 团队、IoT 平台、APM/可观测性厂商、游戏数据分析、广告监控、金融行情；
- **核心口号**：**"Purpose-Built Time Series Platform"** + **"SQL-like Queries, Infinite Cardinality, Built-in Downsampling"** + **"Turn DevOps signals into business insight"**。

## 二、发展历史与社区

- **起源**：2013 年由 **Errplane** 公司（后改名 InfluxData）发布，最初用 LevelDB 存储；
- **里程碑**：
  - 2013-09：0.x 发布，基于 LevelDB，无 TSM；
  - 2014-10：0.9 引入 **InfluxQL** + **Shards**；
  - 2015-04：0.9.5 引入 **TSM1** 引擎（参考 LevelDB/BoltDB 设计），专为时序优化；
  - 2016-09：1.0 发布，引入 **TSI1**（Time-Structured Index）系列化索引 + Continuous Queries（CQ）；
  - 2017-10：1.3 引入 **Iterators**（统一游标抽象）+ Tasks（Flux 雏形）；
  - 2018-11：1.7 引入 **Tasks / Flux / Checks / Notifications**（TICK 收敛）；
  - 2019-10：1.7+ 引入 **Flux 语言** + **iox_query**（IOx 内核 Rust 重写实验）；
  - 2020-04：2.0 GA，**Flux 成为默认查询语言**（InfluxQL 兼容保留），TSM1 + TSI1 为主流生产配置；
  - 2021-03：2.0.3+ 引入 **iox_query_influxql**（In-Process IOx）+ **Storage Read Service**（Reads API）；
  - 2022-11：2.4 引入 **iox_query_influxql_rewrite**（IOx InfluxQL 子集）；
  - 2023-11：2.7 LTS（commit `0ccd35c253`），InfluxDB 3 Core/Enterprise 分裂前最后一版 2.x 稳定线，**TSM1 + TSI1 仍是默认生产路径**；
  - 2024-2026：**InfluxDB 3.x（InfluxDB 3 Core / Enterprise / Cloud）** 基于 **IOx + Apache Arrow + Parquet** 重写，存储从 TSM 转向 **Parquet + Object Storage**（S3 / GCS / Azure Blob），TSM 转为 InfluxDB 2.x 的遗留引擎。本 wiki 主要面向 **InfluxDB 2.x（TSM 时代）** 体系；
- **维护状态**：极活跃。InfluxDB 2.x 进入维护模式（安全补丁为主），InfluxDB 3.x 进入主线开发。GitHub ~30k stars，2023 年起主仓转向 InfluxDB 3 的 Rust + Go 混合架构；
- **社区**：InfluxDays 年度大会、InfluxDB University（免费认证）、社区 Slack ~20k 人、Office Hours、Forum、社区文章 1000+。

## 三、许可与商业限制

- **许可证**：**MIT（OSS Core）**——InfluxDB 2.x OSS 完全开源，宽松；
- **商业产品**：
  - **InfluxDB Cloud**（AWS / GCP / Azure / 阿里云 多区域托管）；
  - **InfluxDB Enterprise**（已与 Cloud 合并，提供集群版 InfluxDB 2.x + InfluxDB 3.x）；
  - **InfluxDB 3 Core**（开源免费，Apache 2.0 协议）；
  - **InfluxDB 3 Enterprise / Cloud Dedicated**（商业，支持高可用、备份、跨区域复制）；
- **生态关系**：
  - 与 **Telegraf**（同公司，300+ 插件采集器，Go 实现）紧耦合；
  - 与 **Grafana**（通过 Flux / InfluxQL HTTP API）无缝对接；
  - 与 **Prometheus**（Remote Write / Remote Read）双向互通；
  - 与 **Kapacitor**（流式处理，已部分被 Flux Tasks 取代）；
  - 与 **Kafka**（Kafka Input Plugin / InfluxDB Output Plugin）；
  - 与 **AWS Timestream / Azure Data Explorer** 形成竞品关系。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **InfluxData Cloud**：自营 SaaS 监控；
  - **Cisco**：网络设备遥测（IoT）；
  - **PayPal**：金融交易监控（部分场景）；
  - **IBM**：Cloud Monitoring 服务底座；
  - **Bosch**：IoT 设备遥测；
  - **Adobe**：Creative Cloud 监控；
  - **Honda / Toyota**：车联网遥测；
  - **Tesla**：电池遥测（早期案例）；
  - **SAP**：SAP Analytics Cloud 监控；
  - **国内大厂**：阿里云、腾讯云、华为云均提供 InfluxDB 兼容时序库或托管服务（阿里云 TSDB-for-InfluxDB、华为云 GeminiDB-InfluxDB）；
- **行业**：DevOps 监控、APM、IT 基础设施、IoT 平台、电信 5G、汽车、广告、金融行情、游戏服务器；
- **典型场景**：Kubernetes 集群监控（cAdvisor + Telegraf）、APM 自建（Trace + Metrics）、IoT 设备遥测、电商大促实时看板、金融行情回放。

## 五、一句话对比定位

> InfluxDB 是 **"TSM 存储引擎 + TSI 倒排索引 + Flux/InfluxQL 双语言 + Continuous Query 降采样 + Retention Policy 生命周期"** 的时序数据库；与 **TimescaleDB（PG 扩展 + Hypertables）**、**Prometheus（Pull 模型 + PromQL）**、**TDengine（IoT 优化 + 多级存储）**、**QuestDB（SQL 原生 + 列存）**、**IoTDB（工业协议 + TsFile）**、**VictoriaMetrics（Prometheus 兼容 + 高基数）** 构成时序数据库阵营。InfluxDB 的差异化是 **"完整生态（TICK/IFX）+ 丰富时序特性 + 多语言 SDK"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                      客户端协议层                                          │
│  HTTP API（8086，默认）/ InfluxQL HTTP / Flux HTTP                          │
│  - InfluxDB CLI（influx）                                                  │
│  - Telegraf / Grafana / Prometheus Remote Write / Kafka / MQTT              │
│  - 多语言客户端（Go/Python/Java/JS/C#/.NET/Rust/Scala/Node.js）             │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ HTTP/Line Protocol
┌─────────────────────▼──────────────────────────────────────────────────────┐
│                    InfluxDB daemon（单进程 + 多协程）                       │
│                                                                            │
│   ┌────────────────────────────────────────────────────────────────────┐ │
│   │  query / storage read service：InfluxQL/Flux 解析 + 执行            │ │
│   │  - storage/reads/        Flux → Iterator 翻译                       │ │
│   │  - storage/readservice/  HTTP Reads Service                         │ │
│   └──────────────────┬─────────────────────────────────────────────────┘ │
│                      │ Iterator Tree                                       │
│   ┌──────────────────▼─────────────────────────────────────────────────┐ │
│   │  storage/engine.go：Storage Engine                                  │ │
│   │  - Write API：points → shards                                       │ │
│   │  - Read  API：measurement/tag/sets → cursors                         │ │
│   │  storage/retention.go：Retention Policy enforcement                 │ │
│   └──────────────────┬─────────────────────────────────────────────────┘ │
│                      │ points / cursors                                    │
│   ┌──────────────────▼─────────────────────────────────────────────────┐ │
│   │  tsdb/engine.go + tsdb/engine/tsm1/：TSM1 存储引擎                 │ │
│   │  - 写入：WAL → Cache → Compactor → TSM File                         │ │
│   │  - 读取：Cache → TSM Files → Block 迭代                            │ │
│   │  - 压缩：Full / Level Compaction（Snapshots / Index Files）         │ │
│   │  tsdb/DESIGN.md + tsdb/engine/tsm1/DESIGN.md                       │ │
│   └──────────────────┬─────────────────────────────────────────────────┘ │
│                      │ blocks                                              │
│   ┌──────────────────▼─────────────────────────────────────────────────┐ │
│   │  tsdb/index/tsi1/：TSI1 倒排索引                                   │ │
│   │  - LogFile（measurements / tag keys / tag values）                   │ │
│   │  - IndexFile（measurement→SeriesID）                                │ │
│   │  - SeriesFile（SeriesKey → SeriesID）                               │ │
│   └──────────────────┬─────────────────────────────────────────────────┘ │
│                      │ file I/O                                            │
│   ┌──────────────────▼─────────────────────────────────────────────────┐ │
│   │  BoltDB（kv/store.go）：bucket / source / authorization 元数据      │ │
│   │  github.com/boltdb/bolt（vendored）                                  │ │
│   └────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────┘
                              │
                              │ 数据文件（data/）/ WAL（wal/）/ meta（meta/）
                              ▼
                  ┌───────────────────────────┐
                  │  本地文件系统或 NFS         │
                  │  /var/lib/influxdb2/       │
                  │  ├─ engine/（TSM + Index） │
                  │  ├─ wal/（Write-Ahead Log）│
                  │  └─ meta/（BoltDB 元数据） │
                  └───────────────────────────┘
```

### 关键设计取舍

- **写路径顺序写**：同 Series 的点写入 **Cache（in-memory skip-list） + WAL（mmap append-only）**，后台 Compactor 把 Cache 刷成 **TSM File（不可变 SSTable-like）**。同 LSM 但 TSM 用 **时序块（Time Range + SeriesID + Values）** 替代通用 KV；
- **读路径多级合并**：查询时同时打开 Cache + 所有 TSM File + IndexFile，**Block-level merge（按时间区间）** 后输出游标；
- **索引与数据分离**：TSI 倒排索引独立于 TSM 数据文件，**LogFile + IndexFile + SeriesFile 三层结构**，后台 LogFile compaction 生成 IndexFile；
- **生命周期分离**：Retention Policy / Bucket Shard Group Duration / Delete Partition 控制 **何时把旧 Shard Group 整体删除或降采样**；
- **查询与存储耦合**：InfluxQL / Flux 直接调用 `StorageReader` 接口（`storage/reads/`），避免传统数据库的优化器 / 执行器分层。

## 七、TSM 存储引擎

### 7.1 核心数据结构

- **TSM File**（不可变，类 SSTable）：由一个或多个 **Block** 组成，每个 Block 包含：
  - **Min/Max Time**：时间范围；
  - **SeriesID**（SeriesKey 哈希 → u64）；
  - **Block Type**（Float64 / Integer / Unsigned / Boolean / String）；
  - **Encoded Values**：根据类型选择编码：
    - **Float64**：**Gorilla 编码**（论文 *"Gorilla: A Fast, Scalable and In-Memory Time Series Database"*，Facebook 2015），XOR + Leading/Trailing Zero 压缩；
    - **Integer**：`Simple8b`/`RLE`；
    - **Boolean**：Bit-packed；
    - **String**：Snappy + LZ4；
- **WAL**：每个 **Shard** 一个 WAL 文件，写入为 `CRC + Length + Type + Data`，mmap append-only，Compactor 完成后删除；
- **Cache**：`map[SeriesID]CacheBucket`，每 bucket 为 **RingBuffer + SkipList**，按时间排序；
- **索引**：每 TSM File 末尾一个 **IndexBlock**，记录 **SeriesID → (MinTime, MaxTime, Offset, Size)**，常驻内存。

### 7.2 写入流程

```
HTTP POST /write（Line Protocol）
  → points_writer.go（验证 + SeriesKey 解析）
  → storage/engine.go WritePoints（按 Bucket/Shard/Retention 路由）
  → tsdb/engine.go WritePoints（SeriesID 分配）
    ├→ TSI1.AddSeries（更新倒排索引）
    └→ TSM1.Write（按 ShardID）
       ├→ WAL.Write（append CRC + data）
       ├→ Cache.Write（更新 ring buffer + skip list）
       └→ Compactor（后台触发）
          ├→ Snapshot Cache → TSM File
          ├→ Full Compaction（合并小 TSM File）
          └→ Level Compaction（多级合并）
```

源码：`tsdb/engine/tsm1/engine.go:WritePoints`、`tsdb/engine/tsm1/writer.go:Write`。

### 7.3 TSM2（实验性，未广泛部署）

TSM2 设计目标是把 **时序块、倒排索引、SeriesID 映射** 全部融合到单一文件，避免 TSM1 + TSI1 的双文件开销。InfluxDB 2.x LTS **未默认启用 TSM2**，InfluxDB 3.x 已用 **Parquet + Arrow** 替代 TSM 路线，本 wiki 不展开。

## 八、TSI1 倒排索引

### 8.1 文件结构

```
index/
  ├── <id>-i.log   # LogFile（追加写，定期 compaction）
  ├── <id>-i.index # IndexFile（measurements / tag keys 索引）
  ├── <id>-i.measurement # SeriesID Set（按 measurement 划分）
  └── ...
series/
  └── <seriesID-to-key>  # SeriesFile（SeriesID ↔ SeriesKey 映射）
```

- **LogFile**（`tsdb/index/tsi1/log_file.go`）：mmap append-only，存储以下条目类型：
  - `SeriesID → SeriesKey`（创建序列）；
  - `SeriesID → Measurement`（measurement → series 集合）；
  - `Measurement → TagKey`（tag 维度列表）；
  - `Measurement → TagKey → TagValue`（具体 tag value）；
  - `SeriesID → TagValue`（倒排）；
- **IndexFile**（`tsdb/index/tsi1/index_file.go`）：由 LogFile compaction 生成，把 measurement / tag key / tag value 索引分段存为 **mmap 区段**，提供 **O(log N) 查找 + 位图交集**；
- **SeriesFile**（`tsdb/index/tsi1/series_file.go`）：存放 `SeriesID ↔ SeriesKey` 映射，支持 **SeriesID → TagValues 快速反查**。

### 8.2 查询路径

```
WHERE measurement='cpu' AND host='server01' AND region='us-west'
  → TSI1.Regex/Equal/NotEqual 解析
  → 找到 'cpu' measurement → 找 host='server01' tag-value 的 SeriesID Set
  → 找 region='us-west' tag-value 的 SeriesID Set
  → BitMap.Intersect → 命中 SeriesID 列表
  → 遍历 SeriesID，在 TSM File / Cache 中读时间区间
```

### 8.3 高基数问题

InfluxDB v2.7 默认 **Series Cardinality 软上限 ~10M**（由 `max-values-per-tag` 与 `max-series-per-database` 限制）。当 SeriesID 超过阈值时，写入会拒绝；高基数场景需借助 **InfluxDB Tasks 降采样** 或迁移到 **InfluxDB 3 / VictoriaMetrics / TDengine**。

## 九、降采样与 Continuous Query（CQ）

### 9.1 InfluxDB 1.x CQ

```sql
CREATE CONTINUOUS QUERY "cq_5m_avg" ON "telegraf"
BEGIN
  SELECT mean(usage_idle) AS usage_idle
  INTO "telegraf"."downsampled"."cpu_5m"
  FROM "telegraf"."autogen"."cpu"
  GROUP BY time(5m), host
END
```

CQ 由 **influxd 内部 ticker**（`services/continuous_querier`）调度，每 N 秒扫描 `NOW() - GROUP BY time()` 范围，写入目标 measurement。**CQ 1.x 的痛点**：范围计算复杂、配置难调试、运维不直观。

### 9.2 InfluxDB 2.x Flux Tasks

```flux
option task = {name: "downsample_5m", every: 1m}

from(bucket: "telegraf")
  |> range(start: -1m)
  |> filter(fn: (r) => r._measurement == "cpu")
  |> aggregateWindow(every: 5m, fn: mean, createEmpty: false)
  |> to(bucket: "telegraf_downsample")
```

Tasks 由 **task service**（`task/`）调度，支持 `every` / `cron` 触发，结果写入下游 bucket。**比 CQ 更灵活**，但 **资源消耗更高**（每次都跑 Flux 解析 + 执行）。

### 9.3 InfluxDB 3.x

已不再有 CQ 概念，改为 **Processing Engine（基于 Apache Arrow DataFusion）** 或外部 **Grafana Aggregations / Materialized Views**。

## 十、保留策略（Retention Policy）

- **InfluxDB 1.x RP**：
  - 每个 Database 可建多个 RP（`autogen` 默认）；
  - RP 定义 `duration`（数据保存时长）+ `shard group duration`（shard 分段粒度）+ `replication`（副本数，集群版）；
  - Shard Group 是 **删除单位**，过期整段删除；
- **InfluxDB 2.x Bucket**：
  - 把 RP + Database 合并为 **Bucket** 概念；
  - 每个 Bucket 独立设置 `retention period`；
  - 通过 `influx bucket update --retention <duration>` 调整；
- **删除实现**：`tsdb/engine.go` 后台 goroutine 每 `shard-precreation-create-check-interval` 检查过期 Shard Group，调用 `tsdb/store.go:DeleteShard` 整段删除（不可恢复！生产必须配备份）；
- **冷热分层**：InfluxDB OSS 2.x **无内置分层**，需借助 **InfluxDB Enterprise / Cloud** 或 **外部 ETL（Telegraf → S3 / Parquet）**；
- **乱序写入处理**：InfluxDB 2.x 默认 **允许乱序写入**（配置 `max-out-of-order-write-delay`，默认 1m），数据落到对应 Shard 后等待 Compactor 合并；超过延迟窗口的乱序数据会被拒绝。

## 十一、查询接口层

### 11.1 InfluxQL（类 SQL）

```sql
SELECT mean(usage_idle)
FROM telegraf.autogen.cpu
WHERE time > now() - 1h AND host = 'server01'
GROUP BY time(5m), host
```

- 解析：`influxql/parser.go` → AST；
- 优化：`influxql/into_ast.go` → `storage/reads/` 的 `ReadRequest`；
- 执行：`tsdb/cursor.go` → `tsm1` Cursor 链。

### 11.2 Flux（函数式）

```flux
from(bucket: "telegraf")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "cpu" and r.host == "server01")
  |> aggregateWindow(every: 5m, fn: mean)
  |> yield()
```

- 解析：`flux/parser/` → Flux AST；
- 执行：`flux/runtime/` → 内置算子 + Storage Reader；
- IOx 后端（InfluxDB 2.x 后期）：`iox_query_influxql_rewrite` + `iox_query_influxql`，部分查询通过 **gRPC 转 Rust** 跑 IOx。

### 11.3 InfluxDB 3 SQL

InfluxDB 3.x 直接支持 **标准 SQL（基于 DataFusion）**：

```sql
SELECT time, host, avg(usage_idle)
FROM cpu
WHERE time > now() - INTERVAL '1 hour'
GROUP BY time(5m), host;
```

## 十二、事务与一致性

- **InfluxDB 2.x**：
  - **写一致性**：单节点下 WAL + BoltDB 元数据 `batch` 提供 **At-Least-Once**，集群版（Enterprise）通过 **HashiCorp Raft（HCS Consensus）** 提供 **Linearizable**；
  - **读一致性**：默认 **Eventual Consistency**（读可能落后最近 N 秒写入），可调 `consistency` 参数到 `{any, one, quorum, all}`；
  - **隔离级别**：基于 **TSM File 的不可变性**，写不影响读，无 MVCC；
  - **乱序写入**：`max-out-of-order-write-delay` 控制容忍窗口（默认 1m），超时拒绝；
- **InfluxDB 3.x**：基于 **Parquet（不可变）+ Object Storage WAL（云原生 S3）**，事务模型进一步弱化，更接近 **流式追加** 语义。

## 十三、复制与高可用

- **OSS 单节点**：无内建 HA，需借助 **外部 NFS / DRBD / ZFS 镜像**；
- **Enterprise 集群**：
  - **Data Node**：存 TSM 数据 + 索引；
  - **Meta Node**：存 BoltDB 元数据（Raft 共识）；
  - **Anti-entropy**：Data Node 间 **Hinted Handoff + Merkle Tree 对账**；
  - **Data Node HA**：通过 `replication-factor` 跨节点副本（典型 2-3）；
- **InfluxDB 3.x Cloud**：基于 **S3 / GCS / Azure Blob + 无状态 Compute Node**，HA 由云存储提供；
- **跨区域**：InfluxDB 3.x 提供 **Edge → Cloud** 数据复制（`influxdb3-replicate` CLI）。

## 十四、运维与生态

- **监控**：内置 `/_metrics` 端点（Prometheus 格式）；
- **备份恢复**：
  - 2.x：`influx backup` / `influx restore`（Enterprise 全量 + 增量）；
  - 3.x：基于 **Parquet on S3**，可直接用 S3 快照；
- **客户端 SDK**：
  - Go：`github.com/influxdata/influxdb-client-go`；
  - Python：`influxdb-client-python`；
  - Java：`influxdb-client-java`；
  - JS/Node：`@influxdata/influxdb-client`；
  - C#：`InfluxDB.Client`；
  - Rust：`influxdb2` crate；
- **Telegraf**：300+ Input / Output / Processor / Aggregator 插件；
- **Grafana**：官方 InfluxDB 数据源 + Flux 数据源；
- **Helm / Operator**：InfluxData 官方 Helm Chart（`influxdata/influxdb`）。

## 十五、性能与基准

- **官方 benchmark**：在 **DevOps 监控** 场景下，InfluxDB 2.x 单节点可达 **百万级 data points/sec**（参考 `influxdata/influxdb-comparisons` 仓库 `tsbs`）；
- **vs TimescaleDB**：在 **写入吞吐** 上 InfluxDB 领先（5-10x），但 **复杂查询**（JOIN、Window Function）TimescaleDB 借助 PG 更强；
- **vs Prometheus**：InfluxDB 支持 **Push + Pull**，Prometheus 仅 Pull；**长期存储** InfluxDB 强，**告警生态** Prometheus 强；
- **vs VictoriaMetrics**：在高基数（>10M 系列）场景下 VM 更优，InfluxDB 在 **生态完整度 + 降采样工具链** 上领先；
- **vs TDengine**：在 **IoT 设备数 >100k** 场景 TDengine 更优，InfluxDB 在 **多语言 SDK + Grafana 集成** 上领先；
- **第三方基准**：TechEmpower、Yahoo TSBS、Timescale 自家对比均收录 InfluxDB。

## 十六、学习路径

### 16.1 入门（1-2 周）

1. **官方文档**：
   - `docs.influxdata.com/influxdb/v2.7/`（安装、写入、查询、Tasks、Telegraf）；
   - **InfluxDB University**（`university.influxdata.com`，免费视频课 + 认证）；
2. **跑通 Telegraf self-influx stack**：`docker run -d -p 8086:8086 influxdb:2.7` + `telegraf` 配置 → Grafana 看板；
3. **理解 Line Protocol**：`measurement,tag1=v1,tag2=v2 field1=0.64,field2="text" 1465839830100400200`。

### 16.2 进阶（4-6 周）

1. **TSM1 引擎源码**（**必读**）：
   - `tsdb/engine/tsm1/DESIGN.md`：官方设计文档；
   - `tsdb/engine/tsm1/engine.go:WritePoints`：写入主路径；
   - `tsdb/engine/tsm1/compact.go`：Compactor 实现；
   - `tsdb/engine/tsm1/cache.go`：Cache + SkipList ；
   - `tsdb/engine/tsm1/encoding.go`：Gorilla / Simple8b / RLE 编码；
2. **TSI1 索引源码**：
   - `tsdb/index/tsi1/DESIGN.md`：索引设计；
   - `tsdb/index/tsi1/log_file.go`：LogFile 实现；
   - `tsdb/index/tsi1/index_file.go`：IndexFile；
   - `tsdb/index/tsi1/series_file.go`：SeriesID 映射；
3. **Continuous Query / Tasks**：`task/` 服务源码，理解 Flux → Iterator 翻译；
4. **InfluxQL 解析**：`influxql/parser.go` + `influxql/into_ast.go` → `storage/reads/`。

### 16.3 高级（长期）

1. **IOx（Rust 内核）**：`iox_query_influxql/` + `iox_query_influxql_rewrite/`，理解 Rust 实现；
2. **InfluxDB 3.x 架构**：Apache Arrow + Parquet + DataFusion；
3. **BoltDB 元数据层**：`bolt` vendored 库，理解元数据事务；
4. **Prometheus Remote Write 协议**：`services/influxdb` 双向兼容。

### 16.4 推荐资源

- **官方**：InfluxDB University、C Docs、Tech Blog（`influxdata.com/blog`）；
- **论文**：
  - *"Gorilla: A Fast, Scalable and In-Memory Time Series Database"*（Facebook 2015）；
  - *"The InfluxDB Storage Engine"*（Paul Dix 博客 2017）；
  - *"Time-Structured Merge Tree"*（Baron Schwartz 博客 2018）；
- **书籍**：*Time Series Databases: A New Way of Looking at Data*（Ted Hills, 2019）；
- **视频**：InfluxDays 历年演讲、GrafanaCon、PromCon；
- **社区**：InfluxDB Community Forum、Reddit r/influxdb、Hacker News 讨论。

## 十七、风险与陷阱

1. **高基数爆炸**：Series 数量超过 ~10M 后，写入延迟急剧上升；监控 `_internal` database 中的 `tsm1_cardinality` 指标；
2. **CQ / Tasks 资源黑洞**：每分钟跑全量 Flux 查询会占用大量 CPU/内存，**生产必须限定时间范围**（`range(start: -2m)`）+ 用 `to()` 落地 bucket，避免依赖 Source bucket 实时查询；
3. **Retention 误删**：删除是 **整段 Shard Group 物理删除**，**不可恢复**！生产必须：
   - 启用 InfluxDB Enterprise 备份（`influx backup`）；
   - 或落 S3 / OSS（Telegraf `outputs.file` + 周期归档）；
4. **乱序写入丢数**：`max-out-of-order-write-delay` 默认 1m，超时拒绝；网络抖动场景需调大到 5-15m；
5. **InfluxQL 与 Flux 不互通**：同一查询两种语言结果可能不一致；InfluxDB 2.x 内部已统一用 Iterator Tree，但语法需切换；
6. **集群版 Enterprise 闭源**：HA 需购买 Enterprise，与 OSS 2.x 不可互通；自建 HA 只能用外部存储复制；
7. **TSM File 损坏恢复**：单 TSM File 损坏只能 **删整个 Shard Group**；生产必须开启监控 `tsm1_file_count` + `tsm1_compactions_active`；
8. **Schema 强制**：Line Protocol 默认 **类型严格**（同一 field 写入不同类型 → 写入失败）；schema 演进需 Telegraf `processor.converter` 显式 cast；
9. **Flux 调试痛苦**：Flux 编译错误信息晦涩，需借助 `flux repl` 交互式排查；
10. **2.x → 3.x 迁移成本高**：TSM 数据不能直接迁 Parquet，需要 **`influxdb3-replicate`** 或第三方 ETL（InfluxData 官方提供迁移工具但 Beta）。

## 十八、与本项目的关联

本项目 `engineering/` 目前**未实现**独立的 TSDB 引擎，但**相关借鉴点**：

- **WAL + 后台 Compaction 思想**：`engineering/src/db/core/wal.c` 已实现简化 WAL，可参考 TSM 的 **时序块 + 多级合并** 设计；
- **Gorilla 编码**：参考 `tsdb/engine/tsm1/encoding.go:GorillaFloatEncoder`，可在 `engineering/src/algo/` 添加 `compression/gorilla.c`；
- **SeriesID 哈希 + 倒排索引**：参考 `tsdb/index/tsi1/log_file.go`，可在 `engineering/src/index/` 添加 `series_card.c` 简易实现；
- **多语言 SDK**：`engineering/sdk/go/`、`engineering/sdk/python/` 等可借鉴 InfluxDB 客户端 SDK 的 `WriteApiBlocking` / `WriteApi` 双模式设计。

## 十九、参考资料

1. InfluxDB v2.7 官方文档：<https://docs.influxdata.com/influxdb/v2.7/>
2. TSM1 Design：<https://github.com/influxdata/influxdb/blob/0ccd35c253/tsdb/engine/tsm1/DESIGN.md>
3. TSI1 Design：<https://github.com/influxdata/influxdb/blob/0ccd35c253/tsdb/index/tsi1/DESIGN.md>
4. Gorilla 论文：<https://www.vldb.org/2015/papers/p1811-pelkonen.pdf>
5. InfluxDB Storage Engine 博客：<https://www.influxdata.com/blog/storage-engine-internals/>
6. InfluxDB vs TimescaleDB 官方对比：<https://www.influxdata.com/blog/influxdb-vs-timescaledb-comparison-time-series-databases/>
7. InfluxDB 3.0 架构：<https://www.influxdata.com/blog/influxdb-3-0-highlights/>
8. Ted Hills *Time Series Databases*：<https://www.oreilly.com/library/view/time-series-databases/9781492045998/>
9. TechEmpower Benchmarks：<https://www.techempower.com/benchmarks/>
10. InfluxDB GitHub：<https://github.com/influxdata/influxdb>