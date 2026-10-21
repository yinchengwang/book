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

## 十九、Series 索引：分片寻址与 TSI1 倒排结构

> 本节与 §八 互补：§八 讲的是 TSI1 的三层文件结构（LogFile / IndexFile / SeriesFile），本节下沉到**字节级**——series key 怎么编码、SeriesID 怎么算出来、一次 `WHERE` 条件怎么变成 SeriesID 集合。全部源码引用指向 `reference/time-series/influxdb/`（**InfluxDB 2.x / Go**，见文末版本判定）。

### 19.1 Series key 的字节编码

一个 series（measurement + 有序 tag 集合）在内存与磁盘上都被序列化成一段 **series key**。编码函数是 `tsdb/series_file.go:313` `AppendSeriesKey`：

```
┌────────────────────────────────────────────────────────────────────┐
│                         SeriesKey                                   │
├──────────┬──────────┬─────────┬────────┬───────┬──────┬──────┬───┤
│ TotalLen │ NameLen  │  Name   │ TagCnt │TagKeyL│ TagK │TagValL│ V │
│ uvarint  │  2 bytes │ N bytes │uvarint │2 bytes│ M    │2 bytes│ … │
└──────────┴──────────┴─────────┴────────┴───────┴──────┴──────┴───┘
```

- `TotalLen`（uvarint）只覆盖 `Name` 之后的全部内容，因此**可以从任意偏移 O(1) 切出一条完整 series key**（`tsdb/series_file.go:369` `ReadSeriesKey`）；
- `NameLen` / `TagKeyL` / `TagValL` 固定 2 字节大端，代价是 measurement 名和 tag key/value 被**限制在 64 KiB 以内**——这是为了让解码可以完全走 `binary.BigEndian.Uint16` 而不做变长解析；
- tag 必须**按 key 字典序排好**再编码（`tsdb/series_file.go:414` `parseSeriesKeyInto`），否则 `CompareSeriesKeys`（`tsdb/series_file.go:437`）的字节序比较无法与语义序对齐。

**存储层的复合 key**：TSM 文件里一个 key 其实是 `seriesKey + fieldKey`，用 `#!~#` 拼接（`tsdb/engine/tsm1/engine.go:77`），切分见 `tsdb/engine/tsm1/engine.go:3116` `SeriesAndFieldFromCompositeKey`（`bytes.Cut`）。这样**同一个 series 的不同 field 在 TSM 内是相邻的**，compaction 时一次顺序写就能覆盖全部 field。

### 19.2 SeriesID 的编码：8 分片 + 全局单调自增

SeriesID 不是一个哈希值，而是**可反解的分片自增序号**——这是 2.x 与 1.x InvertedIndex 最本质的差异：

```go
// tsdb/series_file.go:32
SeriesFilePartitionN = 8

// tsdb/series_file.go:300 —— key 落在哪个分片
func (f *SeriesFile) SeriesKeyPartitionID(key []byte) int {
    return int(xxhash.Sum64(key) % SeriesFilePartitionN)
}

// tsdb/series_file.go:280 —— ID 反推分片（不需要哈希，纯取模）
func (f *SeriesFile) SeriesIDPartitionID(id uint64) int {
    return int((id - 1) % SeriesFilePartitionN)
}

// tsdb/series_partition.go:445 —— 分配：每个分片的计数器按 8 递增
func (p *SeriesPartition) insert(key []byte) (id uint64, offset int64, err error) {
    id = p.seq
    offset, err = p.writeLogEntry(AppendSeriesEntry(nil, SeriesEntryInsertFlag, id, key))
    ...
    p.seq += SeriesFilePartitionN
    return id, offset, nil
}
```

**为什么 `+= 8` 而不是 `++`**：设 ID 为 `n`，则其分片必为 `(n-1) % 8`。`p.seq` 每次加 8，保证分片内的 ID **永远不与其它分片撞号**，且任意时刻「已分配 ID 的个数 = `seq/8`」。这是**分片自增序列**，不是哈希桶——代价是 ID 空间有 8 倍空洞（`SeriesFilePartitionN` 默认 8，InfluxDB 官方配置为可调，见 `tsdb/config.go` 与 `cmd/influxd/launcher/cmd.go`）。

**分片内部的三级寻址**：

```
SeriesKey ──xxhash──> Partition(8)
                          │
                          ├─ SeriesIndex (rhh.HashMap 内存 + mmap 磁盘 Robin Hood Hash)
                          │     keyIDMap:  series key ──> SeriesID
                          │     idOffsetMap: SeriesID  ──> offset
                          └─ SeriesSegment[] (append-only mmap log)
                                offset = (segmentID << 32) | pos   // tsdb/series_segment.go:345 JoinSeriesOffset
```

`SeriesID → offset` 的打包见 `tsdb/series_segment.go:345` `JoinSeriesOffset` / `tsdb/series_segment.go:350` `SplitSeriesOffset`——把 2 字节 segmentID 与 4 字节段内位置拼成一个 int64，**一个 uint64 走完整条反查路径**，无需 struct。

**查找算法**（`tsdb/series_index.go:185` `FindIDBySeriesKey`）：两级，先查内存 `rhh.HashMap`，miss 再查 mmap 磁盘表：

```go
// 1) 内存层：Robin Hood Hash（rhh），O(1)
if v := idx.keyIDMap.Get(key); v != nil { ... return id }

// 2) 磁盘层：mmap 的开放寻址 Robin Hood Hash
hash := rhh.HashKey(key)
for d, pos := int64(0), hash&idx.mask; ; d, pos = d+1, (pos+1)&idx.mask {
    elem := idx.keyIDData[pos*SeriesIndexElemSize:]
    elemOffset := int64(binary.BigEndian.Uint64(elem[:8]))
    if elemOffset == 0 { return 0 }                    // 空槽 → 未命中
    elemKey := ReadSeriesKeyFromSegments(segments, elemOffset+SeriesEntryHeaderSize)
    // Robin Hood 不变式：探测距离超过当前槽的"应在距离"即可提前终止
    if d > rhh.Dist(elemHash, pos, idx.capacity) { return 0 }
    if elemHash == hash && bytes.Equal(elemKey, key) { return id }
}
```

**复杂度**：

| 操作 | 平均 | 最坏 | 依据 |
|------|------|------|------|
| `key → SeriesID`（内存层命中） | O(1) | O(1) | `rhh.HashMap` 开放寻址 |
| `key → SeriesID`（磁盘层） | O(1) | O(capacity) | Robin Hood 早停保证期望 O(1) |
| `SeriesID → offset → SeriesKey` | O(1) | O(1) | 两跳定址，无搜索 |
| 新建 series | O(1) 摊还 | 摊还 | append-only + 计数分片 |
| 分片 compaction | O(N log N) | O(N log N) | `SeriesPartitionCompactor.Compact`，`tsdb/series_partition.go:528` |

> ⚠️ **纠正一个常见误解**：InfluxDB 2.x 的 series 索引用的是 **Robin Hood Hash（`pkg/rhh`）** 而**不是 B-tree**。TSM 侧的 compaction 才用 LSM 式的 generation/level 滚动（见 §二十一）。

### 19.3 TSI1：8 分片 × (1 LogFile + N IndexFile)

TSI1（`tsdb/index/tsi1/`）是**独立的倒排索引**，把「measurement → SeriesID 集合」按 **8 个 partition** 水平切开（`tsdb/index/tsi1/index.go:58` `DefaultPartitionN = 8`，可用环境变量 `INFLUXDB_EXP_TSI_PARTITIONS` 覆盖，见 `tsdb/index/tsi1/index.go:34-41`）。每个 partition 内部：

```
partition_N/
  ├── NNNNNNNN.log          # LogFile，Level() 恒为 0（tsdb/index/tsi1/log_file.go:231）
  ├── L1-00000001.tsi       # IndexFile，文件名 = "L%d-%08d" + ext
  │                          #   （tsdb/index/tsi1/index_file.go:528 FormatIndexFileName）
  └── L2-00000003.tsi
```

- **LogFile 是 append-only 的倒排日志**，只有 4 种 entry（`tsdb/index/tsi1/log_file.go:32-36`）：

  ```go
  LogEntrySeriesTombstoneFlag      = 0x01  // series 级删除
  LogEntryMeasurementTombstoneFlag = 0x02  // measurement 级删除
  LogEntryTagKeyTombstoneFlag      = 0x04  // tag key 级删除
  LogEntryTagValueTombstoneFlag    = 0x08  // tag value 级删除
  ```

  也就是说**「删除」通过 flag 复用同一个 entry 结构实现，不额外占 entry 类型**；`LogEntry` 本体只有 7 个字段（`tsdb/index/tsi1/log_file.go:1068`），series key 直接复用 §19.1 的 `AppendSeriesKey` 编码，因此 `execSeriesEntry`（`tsdb/index/tsi1/log_file.go:670`）能零拷贝地切出 measurement / tags。

- **IndexFile 是 LogFile compaction 的产物**，是一个 mmap 的 **MeasurementBlock + 每 measurement 一个 TagBlock**（`tsdb/index/tsi1/index_file.go:47-54`）：
  - `MeasurementBlock`（`tsdb/index/tsi1/measurement_block.go:53`）：measurement → (SeriesID 集合, TagBlock 偏移, sketch)；
  - `TagBlock`（`tsdb/index/tsi1/tag_block.go:46`）：某 measurement 下 tag key/value → SeriesID 集合的三级表；
  - compaction 写出顺序见 `tsdb/index/tsi1/log_file.go:808` `CompactTo`：magic → measurement names → 各 measurement 的 tagset block → measurement block → trailer。

- **SeriesID 集合是真·位图**：`SeriesIDSet`（`tsdb/series_set.go:12-14`）内部就是 `roaring.Bitmap`，带 `sync.RWMutex`。因此 §八.2 里 `WHERE host='server01' AND region='us-west'` 的「交集」不是集合遍历，而是 **Roaring Bitmap 的 AND**：

  ```go
  // tsdb/series_set.go:155
  func (s *SeriesIDSet) And(other *SeriesIDSet) *SeriesIDSet
  // tsdb/series_set.go:107 —— 多路合并用 FastOr
  bms := make([]*roaring.Bitmap, 0, len(others)+1)
  result := roaring.FastOr(bms...)
  ```

  **复杂度**（Roaring 的容器分层是核心）：
  - 全部 SeriesID 落在同一个 16-bit 桶 → 单个 `Bitmap16` 容器，AND 为 O(min(|A|,|B|))；
  - 跨桶 → 每个桶独立做 AND/OR，再按基数自适应选择 Array（稀疏，≤4096 用有序数组）/Bitmap（稠密）容器；
  - **高基数下 AND 的复杂度是 O(交集基数)，与总基数无关**——这是选位图而非 `map[uint64]struct{}` 的唯一理由。

### 19.4 取舍：什么时候倒排赢、什么时候 B-tree / 哈希赢

```
判定伪代码（依据 series 基数 C 与单 measurement 的 SeriesID 集合大小 S）

for each measurement m with cardinality C_m:
    if C_m <= 1e4:
        # 集合小，倒排直查即可，无需 bitmap
        cost_inverted   = O(1) 访问 tag value → S 个 SeriesID
    else:
        # 集合大，bitmap AND 相对线性扫描的收益达到
        cost_inverted   = O(S / 64)   # roaring: 每个 64bit 机器字一次 AND
        cost_btree_scan = O(log S + S) # 树只能定位区间，仍需遍历

# 写入侧代价对比
per_new_series:
    倒排新增 = 在「每个 tag value 节点的 SeriesID 列表」中各追加 1 次 → O(#tags) 次写
    哈希新增 = 写 1 个 KV → O(1) 次写
```

**结论**：

| 场景 | 赢家 | 原因 |
|------|------|------|
| tag value 基数低（如 `host` 只有几台机器） | 倒排（1.x InvertedIndex / 2.x TSI1） | 每个 tag value 节点挂的 SeriesID 列表极短，查询一次哈希命中即得答案 |
| 单 measurement SeriesID 集合大（>10⁵） | 位图（TSI1 + Roaring） | 条件交集从 O(S) 降到 O(S/64) |
| 单 series 全 key 查询 | 正向索引（SeriesFile 哈希） | O(1)，不经过倒排 |
| 阈值监控类高频 WHERE | 二者都不是瓶颈，瓶颈在**写入侧 series 分配** | 每个新 series 要写 LogFile + Segment + Index 三处 |

### 19.5 ⚠️ 高基数防护：2.x 与 1.x 的行为差异（重要纠偏）

§八.3 提到的 `max-series-per-database` / `max-values-per-tag` **写入拒绝路径在 InfluxDB 2.x 中并不存在**——实测结论：

- 全仓 `grep -rn "max-series-per-database\|MaxSeriesPerDatabase"` **0 命中**；
- `tsdb/config.go:55` 的 `DefaultMaxValuesPerTag = 100000` **定义了但从未被引用**（全仓仅此一处出现），是 1.x 遗留常量。

2.x 实际做法是**不做硬拒绝，只做观测与告警**：

1. **精确统计**：`tsdb/store.go:1159` `SeriesCardinality` —— 对目标 database 下所有 shard 的 `SeriesIDSet` 做**位图并集**后取 `Cardinality()`，注释明确写了「exact」且「不能与其他结果合并」。
2. **近似统计（默认走这条）**：每个 partition 维护 HLL（HyperLogLog）sketch，跨 partition 合并——`tsdb/index/tsi1/index.go:397` `updateMeasurementSketches` / `tsdb/index/tsi1/index.go:411` `updateSeriesSketches`，实现在 `pkg/estimator/hll`。**基数为何无法用 roaring 位图精确统计？因为它要跨整个 database 的所有 shard 求并，位图内存随基数线性增长；HLL 只需固定几 KiB，标准误差约 1.04/√m。**
3. **暴露指标**：`tsdb/store.go:196` `series_cardinality` Gauge（按 bucket），`tsdb/store.go:202` `measurement_cardinality`。

**工程含义**：2.x 面对高基数爆炸时，**不会自动拒绝写入**，只会让写入路径的内存与延迟曲线恶化——因此防护必须靠**外部监控 + 主动治理**（`tsdb/shard.go` 控制 shard 数避免单 shard 过大、降采样、拆分 measurement），而不是指望某个 GUC 挡在前面。这与 §十七.1「~10M 后写入延迟急剧上升」的描述一致，但**触发方式不同**。

## 二十、TSM 块级编码：Gorilla 浮点 XOR 与「delta + Simple8b」时间戳

§七.1 已列出块结构，本节把两个编码器逐行拆开，并纠正一个流传很广的错误。

### 20.1 块的三段式布局

```
Block = [1 byte BlockType] [uvarint TimeBlockSize] [TimeBlock] [ValueBlock]
```

- `BlockType`（`tsdb/engine/tsm1/encoding.go:14-30`）：`BlockFloat64=0` / `BlockInteger=1` / `BlockBoolean=2` / `BlockString=3` / `BlockUnsigned=4`；
- **时间与值分成两个独立块**（`tsdb/engine/tsm1/encoding.go:372` `encodeFloatBlockUsing(buf, values, tsenc, venc)`）——因为同一 series 的所有 point 共享一套时间戳编码器（`TimeEncoder`，可跨字段复用），而值编码器按字段类型独立；
- **切分粒度**：一个 Block 最多 `DefaultMaxPointsPerBlock = 1000` 个点（`tsdb/config.go:52`）。这个常量同时被 compaction planner 当作「文件是否已写满、能否跳过」的判据（`tsdb/engine/tsm1/compact.go:527`）——**编码层的上限变成了调度层的信号**。

### 20.2 浮点：Gorilla XOR + 前导/尾随零窗口

`FloatEncoder`（`tsdb/engine/tsm1/float.go:29`）的完整决策树（`tsdb/engine/tsm1/float.go:87` `Write`）：

```
写入首字节控制头：b[0] = floatCompressedGorilla << 4     // float.go:23 / float.go:51
                    高 4 bit = 压缩算法，低 4 bit = BlockType（由调用方 OR 进去）

first := true
for each value v:
    if first:                       // 首值无参考，64 bit 全量
        writeBits(float64bits(v), 64); first = false; return

    Δbits := float64bits(v) XOR float64bits(prev)      // ★ float.go:101
    if Δbits == 0:                                       // 完全相同
        writeBit(0)                                     // ★ float.go:104
        prev = v; return

    writeBit(1)                                         // 有差异
    L := leadingZeros64(Δbits); T := trailingZeros64(Δbits)   // float.go:108-109

    if (L_prev 存在 && L >= L_prev && T >= T_prev):     // ★ float.go:118 窗口可复用
        writeBit(0)                                     //   控制位 0 = "沿用上一窗口"
        writeBits(Δbits >> T_prev, 64 - L_prev - T_prev)    // 只写有效位
    else:
        writeBit(1)                                     //   控制位 1 = "重设窗口"
        writeBits(L, 5)                                 // ★ float.go:125
        S := 64 - L - T
        writeBits(S, 6)                                 // ★ float.go:133（除以 2 后实测为 1/2）
        writeBits(Δbits >> T, S)                        //   只写有效位
        L_prev, T_prev = L, T

    prev = v

Flush(): writeBit(1) + 完整重设窗口承载 NaN 作为流结束哨兵   // ★ float.go:77-84
         解码端见到 NaN 即判定 finished
```

**逐 bit 的字节账**（`sigbits` 段实测为 `writeBits(S, 6)`）：

| 情况 | 成本 |
|------|------|
| 与前值完全相同 | **1 bit** |
| 沿用上一前导/尾随窗口 | 1 + 有效位（`sigbits`） |
| 重设窗口 | 1 + 1 + 5 + 6 + `sigbits` = **13 + sigbits** bit |

**为什么 Gorilla 有效**：`float64bits` 的高位是符号位+指数、低位是尾数。**CPU 温度 / 内存占用这类指标在小数位上高度稳定**，因此相邻值的 XOR 结果前导零极多（典型 20+ bit）、尾随零也稳定 → 绝大多数点落在「沿用窗口」分支，实测压缩率 **1.37 bit/point**（Gorilla 论文原文数据）。而 `uvnan = 0x7FF8000000000001`（`tsdb/engine/tsm1/float.go:26`）作为哨兵，保证 NaN 不可能是合法写入值（`float.go:89-92` 显式拒绝）。

### 20.3 ⚠️ 时间戳：InfluxDB 用的是「一阶 delta + 10 的幂约分 + Simple8b」，不是 delta-of-delta

这是最常见的误传。**delta-of-delta `(t[i] - t[i-1]) - (t[i-1] - t[i-2])` 是 Prometheus（以及 Gorilla 论文里被讨论后放弃的备选方案）的做法；InfluxDB TSM 从未采用。** 实测 `tsdb/engine/tsm1/timestamp.go` 的 `reduce()`（`timestamp.go:85`）是三段式：

```go
// tsdb/engine/tsm1/timestamp.go:85 —— 就地一阶差分 + 约分 + RLE 判定
func (e *encoder) reduce() (max, divisor uint64, rle bool, deltas []uint64) {
    deltas = e.ts
    max, divisor = 0, 1e12        // divisor 从 10^12 往下试
    rle = true
    for i := len(deltas) - 1; i > 0; i-- {
        deltas[i] = deltas[i] - deltas[i-1]     // ① 一阶差分（倒序，就地做）
        v := deltas[i]
        if v > max { max = v }
        for divisor > 1 && v%divisor != 0 {    // ② 找「最大的公共 10 的幂」
            divisor /= 10
        }
        rle = i == len(deltas)-1 || rle && (deltas[i+1] == deltas[i])  // ③ 全等 → RLE
    }
    return
}
```

然后 `Bytes()`（`timestamp.go:119`）做**三分支决策**：

```go
max, div, rle, dts := e.reduce()

if rle && len(e.ts) > 1 {                       // 分支 A：等间隔 → RLE，最省
    return e.encodeRLE(e.ts[0], e.ts[1], div, len(e.ts))
}
if max > simple8b.MaxValue {                    // 分支 B：delta 过大 → 存原始绝对时间
    return e.encodeRaw()
}
return e.encodePacked(div, dts)                 // 分支 C：Simple8b 位打包
```

三种编码的控制字节统一为「**高 4 bit = 编码类型，低 4 bit = log₁₀(divisor)**」（`timestamp.go:43-50` 定义 `timeUncompressed=0` / `timeCompressedPackedSimple=1` / `timeCompressedRLE=2`）：

| 分支 | 字节布局 | 典型场景 |
|------|----------|----------|
| RLE（`timestamp.go:194`） | `b[0]=2<<4 \| log10(div)` + 8B 首值 + uvarint(delta/div) + uvarint(重复次数) | 每秒/每分钟一个点的规则采样 |
| Packed（`timestamp.go:141`） | `b[0]=1<<4 \| log10(div)` + 8B 首个 delta + Simple8b 流 | 不规则上报但 delta 分布集中 |
| Raw（`timestamp.go:181`） | `b[0]=0<<4` + N × 8B 绝对纳秒 | 乱序 / 时间跳变 |

**解码是对称的**：`decodePacked`（`timestamp.go:295`）先读低 4 bit 还原 `div = 10^(b[0]&0xF)`，读出首个 delta，再把 Simple8b 流逐个取出，**最后做前缀和还原绝对时间**：

```go
last := deltas[0]
for i := 1; i < len(deltas); i++ {
    dgap := deltas[i] * div       // 先乘回约掉的 10 的幂
    deltas[i] = last + dgap       // 再前缀和
    last = deltas[i]
}
```

**复杂度与压缩比**：

| 编码 | 编码复杂度 | 解码复杂度 | 每点字节 | 适用 |
|------|-----------|-----------|---------|------|
| Raw | O(1)/点 | O(1)/点 | **8 B** | 兜底 |
| RLE | O(1)/点（只需头尾） | O(1)/点 | **≈ 0**（整段共用一组头） | 等间隔 |
| Packed + Simple8b | O(1)/点 | O(1)/点 | 典型 **1.3–2 B**（视 delta 方差） | 通用 |

一阶差分对等间隔序列已经给出**常数** delta，再经 `divisor` 约掉 10 的幂（1s=10⁹ns、1min=6×10¹⁰ns 之类）后 delta 常常降到个位数，Simple8b 就能把 60 个值塞进 8 字节——**这正是 delta-of-delta 想解决的问题，但 InfluxDB 用「约分 + Simple8b」达到了同等效果且解码成本更低（无二阶前缀和的两层依赖）**。

### 20.4 其余类型的差异化策略

| 类型 | 编码 | 特点 |
|------|------|------|
| `Integer` (`int.go`) | Simple8b RLE，deltas 以 **1e18 为模**处理 | 计数器天然单调，delta 小 |
| `Unsigned` (`int.go`) | 同 Integer，但不做负数处理 | 节省符号位 |
| `Boolean` (`bool.go`) | Bit-packed + delta-of-delta（位级抖动极小） | 1 bit/点 |
| `String` (`string.go`) | Snappy + LZ4 | 唯一需要通用压缩器的类型 |

> ⚠️ 注意：只有 **Boolean** 用了 delta-of-delta 思路（连续布尔值天然二阶平坦），Float 与 Integer 都没有。

## 二十一、Compaction 调度与查询侧下推

### 21.1 文件名即层级：generation-sequence

TSM 文件名由 `tsdb/engine/tsm1/file_store.go:1258` 生成：

```go
func DefaultFormatFileName(generation, sequence int) string {
    return fmt.Sprintf("%09d-%09d", generation, sequence)   // 如 000000001-000000003
}
```

- **generation（前半）**：文件「代」，由 `FileStore.NextGeneration()`（`tsdb/engine/tsm1/file_store.go:379`）单调递增，同一 generation 的多个文件是同一批 snapshot 被 2GB 上限切开的分片；
- **sequence（后半）**：直接就是 **level**。`tsdb/engine/tsm1/compact.go:183`：

```go
func (t *tsmGeneration) level() int {
    // Level 0 永远由 cache compaction 产生：1 个文件、sequence = 1
    // Level 2 由多个 level 1 文件合并而来，level 3 由 level 2 合并而来
    // Level 4 及其它
    _, seq, _ := t.parseFileName(t.files[0].Path)
    if seq < 4 { return seq }
    return 4
}
```

snapshot 落盘时写死 `sequence = 1`（`tsdb/engine/tsm1/engine.go:1206`），即**所有新写入都从 level 0 出发**。

### 21.2 CompactionPlanner：三个 Plan 函数各管一件事

`tsdb/engine/tsm1/compact.go:95` 定义了 planner 接口，`tsdb/engine/tsm1/compact.go:113` 的 `DefaultPlanner` 实现它（在 `tsdb/engine/tsm1/engine.go:189` 由 `NewDefaultPlanner(fs, CompactFullWriteColdDuration)` 构造）：

```go
type CompactionPlanner interface {
    Plan(lastWrite time.Time) ([]CompactionGroup, int64)  // level ≥ 4 的常规合并
    PlanLevel(level int) ([]CompactionGroup, int64)       // level 1/2/3 的逐级滚动
    PlanOptimize() ([]CompactionGroup, int64)              // level 4 的索引重排优化
    Release(group []CompactionGroup)
    FullyCompacted() (bool, string)
    ForceFull()
    SetFileStore(fs *FileStore)
}
```

**共同的第一步**：`findGenerations(skipInUse bool)`（`compact.go:628`）把磁盘文件按 generation 分组，**倒序（最新在前）**返回，并**跳过已在别的 plan 里占用的文件**：

```go
genTime := c.FileStore.LastModified()
if !last.IsZero() && c.FileStore.LastModified().Equal(last) {
    return lastGen              // 文件系统 mtime 未变 → 直接复用上次结果，O(1)
}
for _, f := range tsmStats {
    gen, _, _ := c.ParseFileName(f.Path)
    if _, ok := c.filesInUse[f.Path]; skipInUse && ok { continue }
    ...
}
if !orderedGenerations.IsSorted() { sort.Sort(orderedGenerations) }
```

`filesInUse` 是**互斥集**，由 `acquire`（`compact.go:672`）占有、`Release`（`compact.go:696`）归还——保证**同一个文件不会被两个并发 compaction 同时改写**。

**三种 Plan 的门槛对比**：

| Plan | 作用范围 | 最小合并单位 | 关键跳过规则 |
|------|---------|-------------|-------------|
| `PlanLevel(L)` `compact.go:241` | level 1/2/3 | `minGenerations = 4`；**level 1 放宽到 8**（`compact.go:300-303`） | 组内若混有 level ≤ 3 的文件则整组跳过（交给 level planner） |
| `Plan` `compact.go:428` | level ≥ 4 | `step = 4`（`compact.go:544`） | ① 满 2GB 且满 1000 点/块 → 跳过（`compact.go:527`）② **大小回退启发式**：`g.size()*2 < generations[i-1].size()` 则从这里截断（`compact.go:535`） |
| `PlanOptimize` `compact.go:337` | **只 level 4**（`compact.go:397`） | `< 4` 个 generation 且无 tombstone → 跳过（`compact.go:405`） | 满 2GB 满块且无 tombstone → 跳过（`compact.go:366`） |

`Plan` 里最值得抄的是那条**大小回退启发式**（`compact.go:531-539`）：

```go
// 多次 compaction 后头部文件会比尾部涨得快得多；只 compact 小的那些，让它们追上来
if i > 0 {
    if g.size()*2 < generations[i-1].size() {
        start = i
        break
    }
}
```

它让**文件大小沿时间轴保持单调递减**（新文件小、旧文件大），从而让查询端「时间范围 → 文件集合」的裁剪命中率最大化——LSM 最难调的部分，InfluxDB 用一条 `×2` 的比较就解决了。

### 21.3 调度循环：每秒一轮 + 优先级队列

`tsdb/engine/tsm1/engine.go:1994` `compact()` 是整个调度器：

```go
func (e *Engine) compact(wg *sync.WaitGroup) {
    t := time.NewTicker(time.Second)               // ① 固定 1s tick
    for {
        select { case <-quit: return; case <-t.C:
            // ② 每一轮同时向四个 plan 要任务
            level1Groups, len1 := e.CompactionPlan.PlanLevel(1)
            level2Groups, len2 := e.CompactionPlan.PlanLevel(2)
            level3Groups, len3 := e.CompactionPlan.PlanLevel(3)
            level4Groups, len4 := e.CompactionPlan.Plan(e.LastModified())

            // ③ 全局 compaction 没活干时，才做索引重排
            if len(level4Groups) == 0 {
                level4Groups, len4 = e.CompactionPlan.PlanOptimize()
            }

            // ④ 把队列深度交给优先级调度器（level 1 优先级高于 level 4）
            e.scheduler.setDepth(1, len(level1Groups))
            ...
            e.scheduler.setDepth(4, len(level4Groups))

            // ⑤ 只启动优先级最高的那一个
            if level, runnable := e.scheduler.next(); runnable {
                switch level {
                case 1: e.compactLevel(level1Groups[0], 1, false, wg)
                ...
                case 4: e.compactFull(level4Groups[0], wg)
                }
            }
            // ⑥ 没启动的全部归还文件锁
            e.CompactionPlan.Release(level1Groups)
            ...
        }
    }
}
```

注意 **④⑤⑥ 三步**：调度器只看队列深度选一个 level，本轮只启一个 compaction，其余 plan 立即 `Release`。这保证了 **单个 shard 同时最多一个 level compaction 在跑**，把 compaction 对写入延迟的干扰压到最低。

另一条独立的循环负责 **snapshot**（`tsdb/engine/tsm1/engine.go:1980` `ShouldCompactCache`）：

```go
func (e *Engine) ShouldCompactCache(t time.Time) bool {
    sz := e.Cache.Size()
    if sz == 0 { return false }
    if sz > e.CacheFlushMemorySizeThreshold { return true }              // 内存水位
    return t.Sub(e.Cache.LastWriteTime()) > e.CacheFlushWriteColdDuration // 冷写超时
}
```

触发后走 `WriteSnapshot`（`tsdb/engine/tsm1/engine.go:1820`）：

```
Cache.Snapshot()            // cache.go:375 —— 原子换出一个新 cache
  └─ snapshot.Deduplicate()  // engine.go:1874 + cache.go:424 —— 去重 + 排序（最贵的一步）
  └─ Compactor.WriteSnapshot() // compact.go:833 —— 多 reader + iterator 归并，按 key 分块写
       └─ 新文件：generation = NextGeneration(), sequence = 1  ⇒ level 0
  └─ 成功后才清 cache 段并删对应 WAL 段   // engine.go:1941
```

### 21.4 查询侧下推：三层裁剪

聚合不在本仓（`mean`/`sum` 实现于外部模块 `github.com/influxdata/flux v0.194.4`，见 `go.mod`），但**下推的三层裁剪全在 TSM 仓内**，这才是性能来源：

```
查询「measurement=cpu, field=usage_idle, host∈S, t ∈ [t0,t1]」

第 1 层 · Series 裁剪（倒排）
    TSI1 → 每个 host 的 Roaring Bitmap → AND → SeriesID 集合
    复杂度 O(Σ S_i / 64)

第 2 层 · 文件裁剪（generation/level 顺序 + Min/Max Time）
    文件名按 generation 有序；BlockIterator 逐 key 取该 key 的全部 IndexEntry
    IndexEntry{MinTime, MaxTime, Offset, Size}  → 与 [t0,t1] 不交则不解码
    判定：IndexEntry.Contains(t)                 // tsdb/engine/tsm1/writer.go:223
    定位：directIndex.Entry(key, t)              // tsdb/engine/tsm1/writer.go:357

第 3 层 · 块内裁剪（二分定位）
    游标 reset 时用 sort.Search 对已解码的 block 做 O(log N) 二分定位
    // tsdb/engine/tsm1/array_cursor.gen.go:45（cache）与 :51（tsm block）
    c.cache.pos = sort.Search(len(c.cache.values), func(i int) bool {
        return c.cache.values[i].UnixNano() >= seek
    })
```

之后才是 **Cache 与 TSM 的归并**（`tsdb/engine/tsm1/array_cursor.gen.go:73` `Next()`）——两路都按时间升序，用双指针线性归并，**不做比较-排序**：

```go
for pos < cap && c.tsm.pos < len(tvals.Timestamps) && c.cache.pos < len(cvals) {
    ckey := cvals[c.cache.pos].UnixNano()      // cache 路
    tkey := tvals.Timestamps[c.tsm.pos]         // tsm block 路
    if ckey == tkey { res[pos] = ckey; cache.pos++; tsm.pos++ }   // 同点：TSM 覆盖 cache
    else if ckey < tkey { res[pos] = ckey; cache.pos++ }          // cache 更早
    else              { res[pos] = tkey; tsm.pos++ }              // tsm 更早
    pos++
    if c.tsm.pos >= len(tvals.Timestamps) { tvals = c.nextTSM() }  // 换下一个块
}
// 收尾优化：整块都能塞进结果 buffer 时直接 memcpy（array_cursor.gen.go:108-114）
```

**复杂度总账**（设命中 S 个 series、每 series P 个点、块大小 B=1000）：

| 阶段 | 复杂度 | 相对「全量解码」 |
|------|--------|----------------|
| Series 裁剪 | O(Σ S_i / 64) | 决定读多少个 series |
| 文件/块裁剪 | O(命中块数)，**不解码被跳过的块** | 决定读多少字节 |
| 块内二分 | O(log P) 每 series | 仅定位起点 |
| Cache×TSM 归并 | O(输出点数) | 两路有序，无排序 |
| **合计** | **O(输出点数 + 命中块数)**，与库中总点数**无关** | — |

**这就是 TSM 的核心复杂度承诺**：查询成本只与**结果集大小**成正比，与**存量数据总量**无关。对比之下，若每个点都要先反序列化解码再过滤，成本是 O(存量点数)。

### 21.5 Predicate 剪枝：compaction 时的额外一层过滤

除了时间与 series 裁剪，`tsdb/engine/tsm1/predicate.go:129` 的 `predicateMatcher.Matches` 提供了一条**「喂 tag 直到有定论就早退」**的短路匹配：

```go
func (p *predicateMatcher) Matches(key []byte) bool {
    p.state.Reset()
    key, _ = SeriesAndFieldFromCompositeKey(key)     // 拆出 series key
    popTag := predicatePopTag
    if bytes.IndexByte(key, '\\') != -1 { popTag = predicatePopTagEscape }  // 有转义字符走慢路径
    for len(key) > 0 {
        tag, value, key = popTag(key)
        if tag == nil || !p.state.Set(tag, value) { continue }
        resp := p.root.Update()                       // 布尔谓词树增量求值
        if resp == predicateResponse_true  { return true }   // 立刻返回
        if resp == predicateResponse_false { return false }
    }
    return false
}
```

**复杂度**：谓词是 tag 的合取时，最坏需读完整个 key（O(#tags)）；但**任一 tag 判定为假即可短路返回**（期望 O(1~2) 次 pop）。谓词树结构见 `predicateNodeAnd`（`predicate.go:426`）/ `predicateNodeOr`（`predicate.go:467`）/ `predicateNodeComparison`（`predicate.go:509`），状态做 **per-cursor 克隆**（`predicate.go:118` `Clone`）以支持并发。

---

## 二十二、参考资料

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