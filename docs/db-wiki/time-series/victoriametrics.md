# VictoriaMetrics Wiki（Prometheus 兼容高性能时序数据库）

> 参考版本：VictoriaMetrics 当前 `app/vmstorage/` + `lib/storage/` 源码（commit `278d7db2c9`），Go 1.22+ 实现，单二进制部署，Gorilla/ZSTD 压缩 + LSM-style Merge。

## 一、项目定位

VictoriaMetrics（VM）是 **VictoriaMetrics 公司** 开源的 **Prometheus 兼容时序数据库**，以 **"极致资源效率 + 高基数友好 + Prometheus 完整兼容"** 著称，是 **大规模 Prometheus 替换 / Grafana 长期存储 / IoT 遥测** 场景的主流选择之一。它把 **写入路径（vminsert）、存储路径（vmstorage）、查询路径（vmselect）** 拆分为三个独立可扩展组件，并提供 **单二进制 VM（cluster 模式）和一站式 cluster（vmstorage/vminsert/vmselect 多副本）** 两种部署形态。

- **痛点**：
  - Prometheus 单机存储瓶颈（~10M 序列、~200GB 磁盘为实用边界）；
  - Cortex / Thanos / M3DB 部署复杂、资源占用高（Java/HBase 体系）；
  - 长期存储（>30d）成本高；
  - 高基数（百万到千万 Series）写入性能急剧下降；
  - 需要 **Prometheus Remote Write / Remote Read 协议兼容** + **Grafana / PromQL 原生支持**；
  - 需要 **InfluxDB Line Protocol / OpenTelemetry / DataDog** 等多协议接收；
- **典型用户**：SRE / DevOps 团队、APM / 可观测性 SaaS、IoT / 边缘计算厂商、电商 / 游戏监控、广告平台；
- **核心口号**：**"Best Practices for Large-Scale Monitoring"** + **"Prometheus-compatible, Cost-effective, Fast"** + **"Single Binary, Billion Metrics, Minimal Resources"**。

## 二、发展历史与社区

- **起源**：2016 年由 **Aliaksandr Valialkin（创始人）** 在 GitHub 发布，最初作为 Prometheus 长期存储；
- **里程碑**：
  - 2016-11：首个 commit，单二进制 VM；
  - 2017-04：引入 **vmstorage / vminsert / vmselect 集群拆分**；
  - 2018-03：v1.x GA，**Prometheus Remote Write / Remote Read** 完整兼容；
  - 2019-06：v1.30+，引入 **MetricsQL**（PromQL 扩展 + 自有算子）；
  - 2020-08：v1.40+，**Storage Data 系列化重构**（`lib/storage/`），引入 **Part** 抽象；
  - 2021-09：v1.70+，**Downsampling 正式发布**（自动降低历史精度）；
  - 2022-06：v1.80+，**Remote Storage 写入**（`vmstorage` → 远端 S3/GCS/Azure）；
  - 2023-04：v1.90+，**Multitenant 增强**（`AccountID` / `ProjectID` 完整路由）；
  - 2024-09：v1.100+，**Streaming Aggregation**（vmselect 边查边聚合）；
  - 2025：v1.110+，**VLSelect**（向量化查询引擎，Beta）；
- **维护状态**：极活跃，**每月 1-2 个 minor 版本**，GitHub ~13k stars，**80+ 贡献者**；
- **社区**：Slack ~5k 人、GitHub Discussions、社区博客、企业版 + Managed Cloud。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）；
- **商业产品**：
  - **VictoriaMetrics Cloud**（AWS / GCP / Azure 多区域托管）；
  - **VictoriaMetrics Enterprise**（商业版，包含 **Multiple retentions / Per-tenant stats / Enhanced security / SAML SSO** 等扩展）；
  - **Managed VictoriaMetrics by Cloudflare / DigitalOcean** 等合作伙伴；
- **生态关系**：
  - 与 **Prometheus**：100% 协议兼容（Remote Write / Remote Read / PromQL / Alerting Rules）；
  - 与 **Grafana**：官方数据源 + Alerting 集成；
  - 与 **OpenTelemetry**：通过 OTLP HTTP/gRPC 接收；
  - 与 **InfluxDB Line Protocol**（`vminsert/influx/`）；
  - 与 **DataDog / OpenTSDB / NewRelic / Zabbix / Graphite**：多协议接收器；
  - 与 **Kubernetes**：Helm Chart + VictoriaMetrics Operator + VMAgent（自研采集器）。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **Cloudflare**：全公司监控 + DNS 分析；
  - **Roblox**：游戏服务器 + 玩家行为监控；
  - **AdGuard**：DNS / 广告过滤遥测；
  - **Wix.com**：网站监控；
  - **Rohlik**：电商物流监控；
  - **ManyChat**：聊天平台监控；
  - **Ferrum Network**：区块链节点监控；
  - **国内**：小米、字节跳动、Bilibili 部分场景，**阿里云 Prometheus 托管**底层曾参考 VM 设计；
- **行业**：DevOps / SRE、APM、IoT、广告、电商、游戏、区块链、电信、金融；
- **典型场景**：Kubernetes 集群长期存储（替换 Thanos）、IoT 设备遥测（高基数 label）、企业私有 Prometheus 集群、Cloudflare 等 CDN 厂商全球监控。

## 五、一句话对比定位

> VictoriaMetrics 是 **"Prometheus 100% 兼容 + Go 单二进制 + 高基数友好 + Storage Data Part 抽象 + Downsampling 自动化"** 的时序数据库；与 **Prometheus（单机）**、**Thanos（Cortex 系对象存储）**、**M3DB（Go 分布式）**、**InfluxDB（生态完整）**、**TDengine（IoT 端到端）**、**TimescaleDB（PG 扩展）** 构成时序数据库阵营。VM 的差异化是 **"极致资源效率 + 一致性 PromQL + 简单部署 + 高基数友好"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                     写入路径（vminsert，可水平扩展）                          │
│  - Prometheus Remote Write 接收（promremotewrite/）                        │
│  - InfluxDB Line Protocol（influx/）                                       │
│  - OpenTelemetry OTLP（opentelemetry/）                                    │
│  - DataDog / OpenTSDB / NewRelic / Zabbix / Graphite / CSV / Native        │
│  - 公网入口：vminsert/HTTP/gRPC                                            │
│  - 内部协议：vmrpc（protobuf over TCP/HTTP）                                │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ vmrpc: rows{metric labels + timestamp + value}
                      ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                     存储路径（vmstorage，无状态化数据服务）                   │
│  - 接收 vminsert 写入，按 (AccountID, ProjectID) 分流                       │
│  - 内存缓冲 → 定期 flush 到 Storage Parts                                  │
│  - 后台 Merge：小 Part 合并为大 Part                                       │
│  - 索引：MetricID ↔ MetricName（lib/storage/metric_name.go）                │
│  - Time Series Index（lib/storage/index_db.go）                            │
│  - 远端存储（vmstorage/remote/）：S3 / GCS / Azure Blob                     │
│  - Downsampling：自动生成 1h / 1d 精度的滚动数据                            │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ vmrpc: 查询请求（filter + time range）
                      ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                     查询路径（vmselect，可水平扩展）                          │
│  - Prometheus Remote Read / Prometheus HTTP API（prometheus/）             │
│  - PromQL + MetricsQL 扩展（promql/）                                     │
│  - Graphite HTTP API（graphite/）                                          │
│  - 路由到 vmstorage：search + fetch                                       │
│  - 内存中 Block Stream Merge（lib/storage/block_stream_merger.go）          │
│  - 结果流式返回给 Grafana / PromQL Client                                  │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │
                      ▼
                  ┌──────────────────────────┐
                  │   本地文件系统             │
                  │  /vmstorage-data/         │
                  │  ├─ data/（Storage Parts）│
                  │  ├─ indexdb/（Time Index）│
                  │  ├─ bigmerges/（合并缓冲） │
                  │  └─ cache/（In-Memory）   │
                  └──────────────────────────┘
```

### 关键设计取舍

- **存储-计算分离**：vmstorage 无状态化（除本地数据），vminsert / vmselect 完全无状态，水平扩展简单；
- **存储抽象**：用 **Storage Part**（不可变 SSTable-like）作为最小数据单元，按 **月份目录分桶**，后台 Merge 类似 LSM；
- **多租户**：通过 URL Path `accountID/projectID` 路由，每个租户独立 metricID namespace；
- **流式查询**：vmselect → vmstorage 之间数据 **Block Stream** 流式传输，内存占用低；
- **Gorilla 编码**：Float64 沿用 Gorilla XOR 压缩，Integer / Histogram 用 ZSTD / Snappy；
- **降采样内建**：通过 `-retentionPeriod` + `-downsampling.period` 自动生成 1h / 1d 精度数据，存储成本下降 10x+；
- **不支持事务**：纯 Append-Only + 弱读一致性，符合时序场景语义。

## 七、存储引擎与 LSM-style Merge

### 7.1 核心数据结构

- **Storage Part**（`lib/storage/part.go`）：不可变数据文件，类 SSTable：
  - **metaindex**：metricName ↔ Block Range 索引；
  - **index**：TSID ↔ Block 位置；
  - **columns**：
    - `metricName`：metric 全名（含 labels），用于按 label 检索；
    - `tsid`：u64，Time Series ID（metricName 哈希）；
    - `timestamp`：int64，按 **block 内 delta-of-delta 编码**；
    - `value`：float64，Gorilla XOR 编码；
- **IndexDB**（`lib/storage/index_db.go`）：当前活跃的 in-memory + mmap 索引，记录 `(date, metricID) → tsid set`；
- **MetricNameCache**（`lib/storage/metric_id_cache.go`）：metricName ↔ metricID 双向缓存；
- **DateMetricIDCache**：按日期分段的 metricID bitmap，避免全量扫描；
- **Block**：最小查询单元，含 `(metricName, tsid, timestamp range, values)`。

### 7.2 写入路径

```
vminsert 接收 Prometheus Remote Write
  → 解码 protobuf → 校验
  → 算 (AccountID, ProjectID) 路由
  → vminsert 通过 vmrpc 把数据推给 vmstorage
vmstorage:
  → 每个 (AccountID, ProjectID) 一个 InMemoryPart（lib/storage/inmemory_part.go）
  → 后台 flush：InMemoryPart → on-disk Storage Part
    → 写入 data/YYYY_MM/<part_id>/
    → 更新 indexdb
  → 后台 Merge：小 Part → 大 Part（lib/storage/merge.go）
    → 按 tsid + timestamp 合并
    → 删除过期数据（-retentionPeriod）
    → 触发 Downsampling
```

### 7.3 查询路径

```
vmselect 接收 PromQL / MetricsQL
  → 解析 → AST
  → 重写（rate / increase / histogram_quantile）
  → 对 vmstorage 发起 search request（按 date / metricID）
vmstorage:
  → IndexDB 查 (date, metricID) → tsid set
  → 读对应 Storage Part 的 Block
  → 通过 Block Stream 返回
vmselect:
  → Block Stream Merge（按 tsid + timestamp）
  → 算子执行（filter / aggregation）
  → 返回给 Grafana
```

源码：`lib/storage/inmemory_part.go`、`lib/storage/part.go`、`lib/storage/merge.go`、`lib/storage/block_stream_merger.go`、`lib/storage/index_db.go`。

## 八、降采样与 Streams Aggregation

### 8.1 Downsampling（自动化）

```
# 启动参数
-retentionPeriod=12   # 保留 12 个月
-downsampling.period=1m:5m:1h   # 1m 原始 + 5m 中等 + 1h 低精度
```

- vmstorage 后台 **Streaming Aggregation**：每个 Part flush 后，自动按指定精度生成降采样副本；
- **降采样副本** 与原精度共存，查询时 PromQL 自动选择最优精度；
- **存储成本**：原始 30d + 5m 中精度 90d + 1h 低精度 12m，存储成本下降 10-20x；
- **配置示例**：保留 1m（30d）、5m（180d）、1h（永久）。

### 8.2 Streaming Aggregation（查询时聚合）

`-`enablePromqlStreamAggregation=true` 时，vmselect 在查询时 **边拉边聚合**，避免把全量原始数据拉到内存。适合 **rate() over 1d** 这类长周期聚合查询。

### 8.3 vs InfluxDB CQ / TimescaleDB Cagg

| 维度 | VM Downsampling | InfluxDB CQ | TimescaleDB Cagg |
|------|-----------------|-------------|------------------|
| 触发 | 自动（后台） | 周期（CQ/Tasks） | 自动（hypertable） |
| 精度 | 多级任意配 | 单一目标 | 多级（cagg） |
| 资源 | 0 业务开销 | 持续 Flux | 后台 worker |
| 灵活性 | 静态配置 | 灵活 Flux | 灵活 SQL |

## 九、保留策略与远端存储

### 9.1 Retention

- `-retentionPeriod=12` 表示 12 个月（vmstorage 单 retention）；
- 多租户：通过 `-retentionPeriod.override=tenantID:value` 按租户覆盖；
- **删除实现**：后台 Merge 阶段按 `(now - retentionPeriod)` 跳过过期 Block；Storage Part 整体过期则删除；
- **查询优化**：查询时跳过过期数据，避免误返回。

### 9.2 远端存储（Remote Storage）

```bash
# vmstorage 启动参数
-remoteWrite.url=https://s3.amazonaws.com/bucket/vm
-remoteWrite.aws.accessKey=...
-remoteWrite.aws.secretKey=...
```

- 支持 **S3 / GCS / Azure Blob**；
- vmstorage 可同时写本地 + 远端，**Primary + DR 模式**；
- 第三方工具 `vmauth` / `vmbackup` / `vmrestore` 辅助数据迁移。

### 9.3 备份恢复

- `vmbackup`（`app/vmbackup/`）：增量备份到 S3/GCS/Azure；
- `vmrestore`（`app/vmrestore/`）：从备份恢复；
- 备份粒度：每天一次增量，每周一全量。

### 9.4 乱序写入处理

VM 默认 **接受乱序写入**，数据按 `timestamp` 直接落到对应 Block，**不影响查询结果**（Block 内部按 timestamp 排序）。`dedup.go` 处理重复写入（同 metricID + timestamp）按 **last-write-wins**。

## 十、查询接口层

### 10.1 PromQL 兼容

- 100% 兼容 Prometheus 查询语法（rate / increase / histogram_quantile / predict_linear 等）；
- **MetricsQL 扩展**（`app/vmselect/promql/`）：
  - `with` 子句（CTE）；
  - `lag` / `lead` / `deriv` / `timezone_offset`；
  - 布尔向量匹配扩展；
  - `rollup_func` 自定义算子；
  - `topk_min` / `topk_max` / `topk_avg`（Prometheus 仅 topk_max）；
- **执行模型**：基于 **Block Stream + Operator Tree**，流式执行 + 内存受控。

### 10.2 Graphite 兼容

- `/graphite/metrics/find` / `/graphite/render` HTTP API；
- 自动从 PromQL 翻译（`app/vmselect/graphite/`）。

### 10.3 第三方查询

- **vmalert**（`app/vmalert/`）：兼容 Prometheus Alerting Rules + Recording Rules；
- **vmauth**（`app/vmauth/`）：HTTP 反向代理 + 路由；
- **vmui**（`app/vmselect/vmui/`）：内置 Web UI，类 Grafana Explore。

## 十一、事务与一致性

- **写一致性**：vmstorage 单节点下 Append-Only，**At-Least-Once**；集群下 **多副本 + 链式复制（replicationFactor=N）**，**Quorum Write** 不可配置（VM cluster 默认 N=2 时不等 Quorum，性能优先）；
- **读一致性**：默认 **Eventual Consistency**（可能落后最近 ~1s 写入）；`-search.deduplicate` 启用跨副本去重；
- **隔离级别**：Append-Only 模型，无 MVCC；
- **乱序写入**：完全支持，按 `timestamp` 写入；重复写入（metricID+timestamp）默认 last-write-wins；
- **不支持 UPDATE/DELETE**（除 HTTP API `admin/api/v1/admin/tsdb/delete_series` 外，**仅删除整 metric**，慎用）。

## 十二、复制与高可用

### 12.1 单 VM 模式

- 一进程含 vminsert + vmstorage + vmselect；
- HA：部署 **2-3 个独立 VM 实例 + 外部负载均衡 + `vmauth`**；
- 数据冗余靠 `-storageDataPath` 在不同磁盘 / 主机。

### 12.2 Cluster 模式

```
                  ┌─────────────┐
                  │  vmauth     │（入口 + 鉴权 + 路由）
                  └──────┬──────┘
                         │
            ┌────────────┴─────────────┐
            ▼                          ▼
   ┌────────────────┐          ┌────────────────┐
   │ vminsert × N   │  ......  │ vminsert × N   │
   └────────┬───────┘          └────────┬───────┘
            └─────────────┬─────────────┘
                          ▼
              ┌─────────────────────┐
              │ vmstorage × M（分片） │
              │  - 存储节点无状态化   │
              │  - vminsert 把数据   │
              │    按 hash 路由到    │
              │    对应 vmstorage    │
              └─────────┬───────────┘
                        │
              ┌─────────▼───────────┐
              │ vmselect × K         │
              │  - 查询路由到所有     │
              │    vmstorage 并合并   │
              └─────────────────────┘
```

- **vmstorage 分片**：通过 `-storageNode` 哈希分（按 metricID）；
- **副本**：`-replicationFactor=2` 表示每个分片有 2 个副本；
- **vminsert / vmselect 完全无状态**，可任意扩缩容；
- **跨区域**：vmstorage 通过 `-remoteWrite.url` 异步复制到远端（DR）。

### 12.3 Multitenancy

- 每个请求 URL Path 携带 `<accountID>/<projectID>`；
- vmstorage 内部按 `(AccountID, ProjectID)` 独立存储；
- 不同租户配额通过 `-storageNode.maxLabelsPerTimeseries` 等参数控制。

## 十三、运维与生态

- **监控**：`/-/metrics` Prometheus 端点、`/-/ready` / `/-/health`；
- **客户端**：Prometheus Remote Write / Read 客户端（任意语言 Prometheus client 即可）；
- **采集器**：**vmauth / vmagent**（自研 + Prometheus 协议兼容）；
- **Kubernetes Operator**：`VictoriaMetrics/operator`（CRD: `VMServiceScrape` / `VMRule` / `VMAlert`）；
- **Helm Chart**：`victoria-metrics-k8s-stack`（Prometheus + Grafana + VM 一站式）；
- **迁移工具**：`vmctl`（`app/vmctl/`，从 InfluxDB / OpenTSDB / Prometheus TSDB 迁移）；
- **客户端 SDK**：
  - Go：`github.com/VictoriaMetrics/metrics`（指标暴露给 VM 端点）；
  - Java：`micrometer-registry-victoriametrics`；
  - Python：`prometheus_client`（VM 端点兼容）；
  - 其他：所有 Prometheus client 自动兼容。

## 十四、性能与基准

- **官方 benchmark**（`app/vmstorage/` `README.md` + `docs/guides/performance/`）：
  - 单节点：~300 万 metrics/sec 写入（Gorilla + ZSTD）；
  - 查询：~1 亿 metrics 扫描 < 1s；
  - **资源占用**：单节点 1M active series 仅需 ~2GB RAM（vs Prometheus 8-16GB）；
- **vs Prometheus**：写入吞吐 **5-10x**，查询 **3-10x**，存储 **5-10x 压缩**；
- **vs InfluxDB**：高基数（>10M）**领先 3-5x**；
- **vs Thanos / Cortex**：资源占用 **5-10x 减少**（Go vs Java），部署简单一个数量级；
- **vs TimescaleDB**：时序写入 **5-10x**，复杂 SQL **TimescaleDB 强**；
- **第三方基准**：TechEmpower、Yahoo TSBS、`victoriametrics/victorialogs` benchmark 均收录。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. **官方文档**：`docs.victoriametrics.com/`（Quick Start、Single-Node、Cluster 部署）；
2. **跑通单二进制**：`docker run -d -p 8428:8428 victoriametrics/victoria-metrics` + 启动 vmagent / vmauth；
3. **配置 Remote Write**：从 Prometheus 切换到 VM（仅改 `-remote_write` URL）；
4. **Grafana 接入**：添加 VM 为 Prometheus 类型数据源。

### 15.2 进阶（4-6 周）

1. **存储引擎源码**（**必读**）：
   - `lib/storage/storage.go`：主入口；
   - `lib/storage/inmemory_part.go`：内存缓冲；
   - `lib/storage/part.go`：Storage Part 抽象；
   - `lib/storage/merge.go`：后台 Merge；
   - `lib/storage/index_db.go`：Time Series 索引；
   - `lib/storage/block_stream_*.go`：Block 流式读写；
   - `lib/storage/encoding/`：ZSTD / Gorilla / Snappy 编码；
2. **查询引擎源码**：
   - `app/vmselect/promql/eval.go`：PromQL 表达式求值；
   - `app/vmselect/promql/rollup.go`：rate / increase 等函数；
   - `app/vmselect/netstorage/netstorage.go`：vmselect → vmstorage 通信；
3. **集群架构**：
   - `app/vmstorage/vmstorage.go`：分片 + 副本；
   - `app/vminsert/main.go`：路由 + 负载均衡；
   - `app/vmselect/main.go`：查询 fan-out + merge；
4. **降采样 / 远端存储**：`app/vmstorage/main.go` + `-downsampling.period` 文档。

### 15.3 高级（长期）

1. **VLSelect**（向量化查询引擎，Beta）：`lib/storage/` 的 columnar 优化；
2. **vmalert 告警**：基于 VM 的 Alertmanager 替代品；
3. **vmauth 鉴权**：OAuth2 / JWT / 多租户；
4. **VictoriaLogs**（同公司姊妹项目，日志存储，**共用部分 `lib/storage/`**）。

### 15.4 推荐资源

- **官方**：docs.victoriametrics.com、GitHub Discussions、博客（`victoriametrics.com/blog/`）；
- **论文**：
  - *"Gorilla: A Fast, Scalable and In-Memory Time Series Database"*（Facebook 2015）；
  - *"Dremel: Interactive Analysis of Web-Scale Datasets"*（Google 2010，Parquet 雏形）；
  - *"The Log-Structured Merge-Tree (LSM-Tree)"*（O'Neil 1996）；
- **视频**：KubeCon / PromCon 历年演讲、VictoriaMetrics Office Hours；
- **书籍**：*Mastering Prometheus*（Poberezhnyi 2024，含 VM 章节）；
- **社区**：Slack、GitHub Discussions、Reddit r/PrometheusMonitoring。

## 十六、风险与陷阱

1. **Cluster 模式部署复杂**：vminsert/vmstorage/vmselect 三层 + vmauth + vmagent + vmauth 鉴权，**首次部署需 ~1 周**；建议先单 VM 跑通再升级；
2. **MetricsQL 与 PromQL 兼容性**：VM 扩展部分算子（`topk_avg` / `with`）在 Grafana / Prometheus 客户端可能不识别，**回写到 Prometheus 时语法可能报错**；
3. **vmstorage 副本一致性弱**：默认不等 Quorum，**节点宕机可能丢最近 N 秒数据**；生产必须配 `-storageNode` + 副本 + 远端存储双写；
4. **删除 API 危险**：`admin/api/v1/admin/tsdb/delete_series` 会删除整 metric，**不可恢复**；生产禁用，删除走 Grafana 短期 API；
5. **高基数查询性能**：单查询拉取 >100K series 时内存 + 网络可能 OOM；**生产限定 maxLabelsPerTimeseries + maxLabelValuesPerTimeseries**；
6. **远端存储一致性**：本地 → 远端异步复制，远端数据可能少 1-5s；用作 DR 而非主；
7. **降采样数据精度**：1h 降采样无法满足秒级监控需求；**关键指标保留原始 1m 精度 30 天以上**；
8. **流式查询内存上限**：`-search.maxMemoryPerQuery` 控制单查询内存，超出报错；长周期聚合（rate 30d）务必用 streaming aggregation；
9. **Multitenant 配额**：默认无硬限，多租户可能互相干扰；启用 Enterprise 或自研配额中间件；
10. **版本升级注意**：VM 集群升级需要 **先 vminsert → vmselect → vmstorage 滚动升级**，顺序错可能丢请求；建议用 operator 管理；
11. **Grafana 数据源限制**：Grafana 对 PromQL 的解析不一定支持 MetricsQL 扩展，复杂查询用 Grafana Explore 测试。

## 十七、与本项目的关联

本项目 `engineering/` 与 VM 的**潜在借鉴点**：

- **Storage Part 抽象**：`engineering/src/algo/` 可参考 `lib/storage/part.go` 实现简化版 Part，应用于 **指标采集器 / 内部审计日志**；
- **Gorilla + ZSTD 编码**：`engineering/src/algo/compression/` 可加入 `gorilla.c` + `zstd_wrapper.c`，用于时序压缩；
- **vmrpc 协议**：`engineering/src/db/net/rpc.c` 可参考 VM 的简单 Protobuf 协议设计；
- **集群路由**：`engineering/src/db/dist/router.c` 可参考 VM 的 hash 路由 + 副本策略；
- **VMAgent / vmauth**：`engineering/apps/` 下若做轻量采集器，可参考 VM 的 `-remoteWrite.url` 设计。

## 十八、Storage Part 文件布局与四层剪枝索引

> 本节全部行号对应 `reference/time-series/victoriametrics/` 子模块，commit `278d7db2c9`。
>
> ⚠️ **版本提示**：VM 早期版本（v1.x）的 `lib/storage/tsdb/` 目录与「每个 metric name 一对 index/data 文件」的布局，在当前 commit 中**已完全不存在**（`ls lib/storage/tsdb/` → No such file or directory）。现行结构是「按月分区的 Storage Part（4 个二进制文件）+ 三级索引」。本节只描述当前代码。

### 18.1 目录与文件布局

```
-data/
  ├─ small/2026_10/            # small part 目录，按自然月分桶
  │   ├─ 0000000000000000/     # part 目录（16 位十六进制 merge index）
  │   │   ├─ metadata.json     # partHeader: RowsCount/BlocksCount/Min+MaxTimestamp/MinDedupInterval
  │   │   ├─ metaindex.bin     # 索引层 1: metaindexRow 数组，整体 ZSTD 压缩
  │   │   ├─ index.bin         # 索引层 2: blockHeader 数组，分块 ZSTD 压缩
  │   │   ├─ timestamps.bin    # 数据层 1: 各 block 编码后的 timestamps
  │   │   └─ values.bin        # 数据层 2: 各 block 编码后的 values
  │   └─ parts.json            # part 名列表
  ├─ big/2026_10/              # big part 目录，同结构
  └─ indexdb/2026_10/          # 该月活跃 metricID 集合
```

- 分区按**自然月**切分，目录名格式 `2006_01`（`lib/storage/time.go:73-76` `timestampToPartitionName`）；时间范围由 `fromPartitionTime`（`lib/storage/time.go:95-101`）精确算成 `[当月 1 日 00:00:00, 次月 1 日 00:00:00 − 1ms]`。
- 4 个文件名常量定义在 `lib/storage/filenames.go:4-7`：`metaindexFilename` / `indexFilename` / `valuesFilename` / `timestampsFilename`；`small` / `big` 目录名在 `lib/storage/filenames.go:24-25`。
- 打开 part 时 4 个文件**并行 open**（`lib/storage/part.go:50-84` `mustOpenFilePart`，用 `fs.ParallelReaderAtOpener`）——NFS / Ceph 上串行 open 的固定延迟不可忽略。
- v1.90.0 之前 part 目录名本身就编码了元数据，格式为 `RowsCount_BlocksCount_MinTimestamp_MaxTimestamp_Garbage`（`lib/storage/part_header.go:88-90`）；新版本改用 `metadata.json`，`MustReadMetadata`（`lib/storage/part_header.go:134-167`）在文件缺失时才回退到旧解析。

### 18.2 两级索引记录

**metaindexRow**（`lib/storage/metaindex_row.go:14-33`）——语义是「指向一个 index block」：

| 字段 | 作用 |
|------|------|
| `TSID` | 该 index block 内**最小**的 TSID（块内其余 TSID 必 `>=` 它，注意是下界不是区间上界） |
| `MinTimestamp` / `MaxTimestamp` | 该 index block 内所有 blockHeader 时间范围的并集 |
| `IndexBlockOffset` / `IndexBlockSize` | 在 `index.bin` 中的字节区间 |
| `BlockHeadersCount` | 该 index block 内 blockHeader 条数（解码时用于校验） |

**blockHeader**（`lib/storage/block_header.go:19-82`）——语义是「指向一个 block 的数据」：

- **定位类**：`TSID`、`MinTimestamp`、`MaxTimestamp`、`TimestampsBlockOffset`、`ValuesBlockOffset`、`TimestampsBlockSize`、`ValuesBlockSize`、`RowsCount`；
- **解码类**：`Scale`（value 的 $10^{\text{Scale}}$ 定点缩放因子）、`TimestampsMarshalType`、`ValuesMarshalType`、`PrecisionBits`；
- `FirstValue` 之所以从编码 payload 里单独提出来，是因为首值与后续 delta 差异大，单独存可提升压缩率（`lib/storage/block_header.go:34-39` 的注释明确说明这一点）。

**Block 形状**：单 block 上限 `maxRowsPerBlock = 8 * 1024 = 8192` **行**，字节上限 `maxBlockSize = 8 * maxRowsPerBlock = 65536`（`lib/storage/block.go:13-19`）。这是行数上限，**不是**「8192 series × 120 timestamps」——brief 给的旧常量在本 commit 中不存在。写入侧由 `Block.tooBig()`（`lib/storage/block.go:142-150`）判定；归并侧由 `len(tmpBlock.timestamps) <= maxRowsPerBlock`（`lib/storage/merge.go:127`）判定。

**Index block 形状**：`len(bsw.indexData) + len(headerData) > maxBlockSize` 时强制刷盘（`lib/storage/block_stream_writer.go:152`），刷盘时整块 ZSTD 压缩写入 `index.bin`，同时向 `metaindex.bin` 追加一条 metaindexRow（`lib/storage/block_stream_writer.go:185-207` `flushIndexData`）。因此 `metaindexRow` 条数 ≈ `block 数 × sizeof(blockHeader) / maxBlockSize`。

### 18.3 四级剪枝伪代码

```text
search(tr, tagFilters):
  L1  Partition 剪枝: table.GetPartitions(tr) 只返回 tr.overlapsWith(pt.tr) 的月分区
                      (lib/storage/table.go:629-643) —— 通常只打开 1~2 个月
  L2  Part 剪枝:     partSearch.Init() 比较 partHeader 的 [Min,Max] 与 tr;
                      不相交则 tsids 置空, 整个 part 直接跳过
                      (lib/storage/part_search.go:64-81)
  L3  metaindex 剪枝: nextBHS() 先按 TSID 二分定位 mr, 再按 mr 的 [Min,Max] 与 tr
                      相交剪枝; 命中才 ReadAt + ZSTD 解压出整个 index block
                      (lib/storage/part_search.go:150-202)
  L4  blockHeader 剪枝: searchBHS() 在已解压的 blockHeader 数组上继续剪枝
                      (lib/storage/part_search.go:254-309)

L3 的两个关键细节:
  a) 定位必须回退一行. 目标 tsid 可能落在"上一个 index block 的尾部",
     因为 mr.TSID 是块内最小值而非区间上界.
     skipSmallMetaindexRows() 返回 sort.Search 结果的 n-1 行
     (lib/storage/part_search.go:223-234, 注释里给了 [tsid10,tsid20,tsid30]
      的完整对照表).
  b) L4 的时间剪枝用线性扫描而非二分: 同一 TSID 的多个 block 时间范围
     可能重叠, 不满足二分的单调性 (lib/storage/part_search.go:282-298).
```

### 18.4 时间戳块去重（VM 针对高基数的独特优化）

`blockStreamWriter.WriteExternalBlock` 在写 `timestamps.bin` 之前，先比对当前 block 的**编码后** timestamps 与上一个 block 是否逐字节相同（`lib/storage/block_stream_writer.go:143-153`）：

```go
usePrevTimestamps := len(bsw.prevTimestampsData) > 0 &&
                    bytes.Equal(timestampsData, bsw.prevTimestampsData)
if usePrevTimestamps {
    // 把 TimestampsBlockOffset 指回上一个块, 物理上不再写第二份
    headerData, timestampsData, valuesData =
        b.MarshalData(bsw.prevTimestampsBlockOffset, bsw.valuesBlockOffset)
    timestampsBlocksMerged.Add(1)
    timestampsBytesSaved.Add(uint64(len(timestampsData)))
}
```

动机很直接：高基数场景下大量 series 以固定 15s / 60s 抓取，**相邻 series 的 timestamps 块完全一致**，物化一次即可被 N 个 block 共享。注意比较发生在**编码之后**（`MarshalData` 已返回 `timestampsData`），所以「相同」是字节级严格相等，不受 scale / marshalType 差异影响。对应指标为 `vm_timestamps_blocks_merged_total` / `vm_timestamps_bytes_saved_total`（`lib/storage/block_stream_writer.go:169-172`）。

### 18.5 复杂度

设某 partition 内有 `B` 个 block、`I` 条 metaindexRow、查询命中 `S` 个 series、覆盖 `P` 个月分区：

| 层级 | 定位方式 | 代价 |
|------|----------|------|
| L1 Partition | 线性扫描 | `O(P)`，`P` ≈ 保留月数 |
| L2 Part | 常数比较 | `O(1)` |
| L3 metaindexRow | 二分 + 线性 | `O(log I + 命中行数)` |
| L4 blockHeader | 二分 + 线性 | `O(log B_block + 命中 block 数)` |
| 数据读取 | 按 offset `ReadAt` | `O(命中 block 字节数)` |

关键性质：**L1~L4 的剪枝全部只读内存中的 `metaindex` 切片和已解压的 index block，不触碰 `timestamps.bin` / `values.bin`**；后者只在最终命中的 block 上按 `TimestampsBlockOffset` / `ValuesBlockOffset` 做 `ReadAt`。且 index block 走 `blockcache`（`lib/storage/part_search.go:178-193`）复用，避免重复 ZSTD 解压。

## 十九、数值编码：定点化 + nearest-delta 家族 + ZSTD 自适应回退

> ⚠️ **重要纠正**：VM 早期以 Gorilla 论文的 **float64 XOR + 前导/尾随零计数** 作为主力编码。当前 commit 中已**完全移除**——`grep -rn "XOR\|xor" --include=*.go lib/encoding/ lib/storage/`（排除测试）返回 **0 行**。本 wiki §六「Gorilla 编码」一节描述的是旧版本行为。

### 19.1 定点化：float64 → int64 × 10^Scale

磁盘上**不存 float64**。写入时 `decimal.AppendFloatToDecimal`（`lib/decimal/decimal.go:173`）把一段 float64 序列**整体**缩放成 int64 并返回公共指数 `Scale`（调用点 `lib/storage/raw_row.go:137,147`），使得：

$$\text{scale} = \min_i e_i, \qquad \text{int}_i = \operatorname{round}\!\big(f_i \cdot 10^{\,e_i - \text{scale}}\big)$$

其中 $e_i$ 是 $f_i$ 的十进制指数。若某些点强行放大到 `minExp` 会溢出 int64，代码会下调 `downExp`（`lib/decimal/decimal.go:212-224`）后重算。`Scale` 写进 blockHeader，读取时由 `decimal.AppendDecimalToFloat`（`lib/decimal/decimal.go:100`，调用点 `lib/storage/block.go:327`）还原。对典型监控值（整数计数、1 位小数百分比），缩放后仍是小整数——**这正是 delta 编码能压得动的前提**；若 scale 过大（值本身很碎），delta 编码反而退化。

### 19.2 六种 MarshalType 与选择顺序

`lib/encoding/encoding.go:20-43` 定义 6 种 `MarshalType`：

| 值 | 名称 | 适用场景 |
|----|------|----------|
| 1 | `MarshalTypeZSTDNearestDelta2` | counter（单调递增） |
| 2 | `MarshalTypeDeltaConst` | 等差数列（如匀速爬坡的 gauge） |
| 3 | `MarshalTypeConst` | 常量序列 |
| 4 | `MarshalTypeZSTDNearestDelta` | gauge（任意波动） |
| 5 | `MarshalTypeNearestDelta2` | 同 1，但 ZSTD 无收益时回退 |
| 6 | `MarshalTypeNearestDelta` | 同 4，但 ZSTD 无收益时回退 |

`marshalInt64Array`（`lib/encoding/encoding.go:119-171`）是**同一段代码同时负责 timestamps 和 values**——`MarshalTimestamps` / `MarshalValues`（`:82-105`）都只是它的薄封装。决策顺序：

```text
marshalInt64Array(a, precisionBits):
  if isConst(a):        return Const, first=a[0]                        # payload 0 字节
  if isDeltaConst(a):   return DeltaConst, first=a[0], varint(a[1]-a[0])
  if isGauge(a):                                                        # encoding.go:331-369
      pb = precisionBits < 6 ? precisionBits + 2 : precisionBits      # gauge 补偿 2 bit
      payload = nearestDelta(a,  pb)                                   # 一阶差分
      mt = ZSTDNearestDelta
  else:
      payload = nearestDelta2(a, precisionBits)                        # 真正的 delta-of-delta
      mt = ZSTDNearestDelta2
  if len(payload) < 128:              # minCompressibleBlockSize，压了也是白压
      mt = 对应的非 ZSTD 版本
  else:
      out = zstd(payload, getCompressLevel(len(a)))
      if len(out) > 0.9 * len(payload):   # 压缩收益 < 10%, 丢弃压缩结果
          out = payload; mt = 对应的非 ZSTD 版本
  return out, mt, first
```

**gauge / counter 判别**（`isGauge`，`lib/encoding/encoding.go:331-369`）不查元数据，纯靠数值形态推断：

1. 首值为负 → 必是 gauge（counter 不会为负）；
2. 任一点 $v < 0$ → gauge；
3. 任一点 $v < v_{prev} \gg 3$（即掉到 1/8 以下）→ 判为 gauge 的「下降」，立即返回 true；
4. 否则记一次「counter reset」，`resets > len(a)/8` 才判 gauge；`resets <= 2` 判 counter（容忍进程重启）。

源码注释明确说明这个偏向是刻意的：**误把 counter 判成 gauge 无所谓**（delta 码对单调序列仍然有效），所以只在证据充分时才升级为 gauge。

### 19.3 nearest-delta 与 precisionBits：可控有损压缩

`nearestDelta`（`lib/encoding/nearest_delta.go:83-121`）**不是无损的**。做法是：设当前值的有效位数为 `originBits = bits.Len64(|origin|)`，若 `originBits > precisionBits`，则把 delta 的低 `originBits − precisionBits` 位直接抹零：

$$d' = d \mathbin{\&} \Big( (2^{64}-1) \ll \text{tz} \Big), \qquad \text{tz} = \text{originBits} - \text{precisionBits}$$

抹零让后续 varint 少编几个字节，代价是引入相对误差。为避免把「计数器重置」误判成「低位可抹」，代码用 `prevTrailingZeros` 跟踪上一轮抹了多少位：若本次 `tz` 比上次多 4 位以上（或少 4 位以上），判定为计数器重置，**放弃抹零并返回全精度**（`lib/encoding/nearest_delta.go:101-110`）；gauge 路径则用 `decIfNonZero` 让 trailingZeros 单调衰减。

`nearestDelta2`（`lib/encoding/nearest_delta2.go:15-51`）在 nearest-delta 之上再加一层 **delta-of-delta**：

```text
d1 = a[1] - a[0]                              # 首个 delta 单独用 varint 存
prev_d1 = d1
for i in 2..n-1:
    d2 = nearestDelta(a[i] - a[i-1], prev_d1) # 对"delta 的差分"再做一次 nearest-delta
    prev_d1 += d2
    emit varint(d2)
```

解码端（`lib/encoding/nearest_delta2.go:57-100`）是纯前缀累加 `d1 += d2; v += d1`——**O(n) 时间、O(1) 额外空间**（除输出缓冲）。

`precisionBits` 由启动参数 `-precisionBits` 给出，默认 64（即无损），定义在 `app/vmstorage/vmstorage.go:24-25`，经 `Storage.AddRows(mrs, precisionBits)`（`lib/storage/storage.go:1670`）传到每一行。语义在 `lib/storage/block_header.go:71-81` 有完整说明：`n` 位精度 ⇒ 最大相对误差 $100/2^n\ \%$，`64` 表示精确值；降 precisionBits 同时改善压缩率与速度。

### 19.4 ZSTD 层级与压缩等级

`getCompressLevel`（`lib/encoding/encoding.go:371-385`）按行数分档返回 1~5 级：≤ 64 行用 level 1，≤ 256 用 2，≤ 1024 用 3，≤ 4096 用 4，更多用 5。行数少时熵不足，高等级 ZSTD 收益有限而 CPU 开销显著。

**兜底阈值**：ZSTD 作用在已经 delta 化的 int64 varint 流上，两者信息高度重叠，收益有限。VM 用保守的 **0.9 阈值**（`lib/encoding/encoding.go:156`）做兜底——若压缩后仍超过原始 payload 的 90%，就整体回退到不压缩并改写 `MarshalType`，宁可多占一点磁盘，也不让解码路径每读一个 block 都多一次 ZSTD 调用。这个「编码期自适应 + 回退」的设计比固定用 ZSTD 更适合监控数据（大量 series 只有寥寥几个点，压缩不划算）。

### 19.5 复杂度与与 Gorilla 的对比

| 编码 | 编解码时间 | 额外空间 | 典型 counter@1m 的字节/点 |
|------|-----------|----------|---------------------------|
| `Const` | `O(n)` | `O(1)` | **0** |
| `DeltaConst` | `O(n)` | `O(1)` | ≈ 1 |
| `NearestDelta` | `O(n)` | `O(n)` varint 缓冲 | ≈ 1~2 |
| `NearestDelta2` | `O(n)` | `O(n)` | ≈ 0.3~1 |
| 任一 + ZSTD | `O(n)`，常数更大 | `O(n)` | 再降 20~40% |

与 Gorilla 的本质差异：Gorilla 的 XOR 路径**严格无损**且专为 float64 的尾数结构设计；VM 的 nearest-delta 路径作用在**定点化后的 int64** 上，**默认无损**（`precisionBits=64` 时不抹零，走 `nearest_delta.go:26-32` 的 fast path）、**可配置有损**。在单调计数序列上 VM 通常更优（delta-of-delta 对计数器天然友好），但依赖定点化成功——`Scale` 过大时反而退化。

## 二十、LSM 分层合并：写放大最优的 Part 选择与归并流水线

### 20.1 三级 tier 与目标体积

每个 partition 内的 part 分三类（均在 `lib/storage/partition.go`）：

| tier | 位置 | 后台 worker | 目标体积 |
|------|------|-------------|----------|
| inmemory | 进程内存 | `inmemoryPartsMerger`（`lib/storage/partition.go:535`） | 受 `maxInmemoryParts = 60`（`:34`）约束 |
| small | `data/small/<月>/` | `smallPartsMerger`（`:568`） | `getMaxSmallPartSize`（`:1149-1160`） |
| big | `data/big/<月>/` | `bigPartsMerger`（`:601`） | `getMaxBigPartSize`（`:1162-1167`） |

后两者都用 `getMaxOutBytes(path, workersCount)`（`lib/storage/partition.go:1169-1177`）按**剩余磁盘空间 / 并发合并数**动态定容，并硬顶 `maxBigPartSize = 1e12`（`lib/storage/partition.go:28`，注释说明这个上限是为了「限制构建单个 big part 所需时间不应超过几天」）。small tier 的并发数取 `len(smallPartsConcurrencyCh)`，big tier 固定 4 个 worker（源码注释指向 issue #4915）。这里刻意**不**预先扣掉 `freeDiskSpaceLimitBytes`——注释解释扣掉会导致「一堆小 part 永远合并不掉」。

### 20.2 写放大最优的合并窗口选择

朴素做法是「取最小的 N 个 part 合并」。VM 改用**穷举搜索最小化写放大**（`appendPartsToMerge`，`lib/storage/partition.go:1672-1728`）：

```text
输入: src[]  已按 size 升序、同 size 按 MinTimestamp 降序
              (sortPartsForOptimalMerge, lib/storage/partition.go:1735-1745)
      maxPartsToMerge = defaultPartsToMerge = 15     (partition.go:36-40)
      maxOutBytes

1. 过滤掉 size > maxOutBytes / 1.7 的 part        # minMergeMultiplier, partition.go:1663-1669
   —— 先压缩候选空间, 抑制后面 O(N^2) 的搜索规模
2. 枚举合并窗口大小 i ∈ [ max( (15+1)/2, 2 ), min(15, len(src)) ]
3. 对每个 i, 枚举所有长度为 i 的连续窗口 a = src[j : j+i], 要求:
     - 尺寸均衡: a[0].size * i >= a[i-1].size   否则 skip (避免大小悬殊导致合并后仍失衡)
     - 体积可行: getPartsSize(a) <= maxOutBytes   否则 break (已排序, 后面只会更大)
4. 记录 m = outSize / a[i-1].size, 取 m 最大者作为选中集合
5. 兜底: 若 maxM < max(15/2, 1.7) = 7.5, 本轮不合并
   —— 倍数太小说明合并省下的空间抵不上重写全部数据的磁盘 IO
```

写放大的定义为：

$$\text{WA} = \frac{\sum_{p \in a}\text{size}(p)}{\text{size}(\text{out})} = \frac{\text{outSize}}{\text{outSize} - \text{bytesSaved}} \approx \frac{\text{outSize}}{\max_{p \in a}\text{size}(p)}$$

算法在**最大化** $m = \text{outSize} / \max\text{size}(a)$，等价于**最小化**写放大。`minMergeMultiplier = 1.7` 的注释把权衡写得很直白：「Higher value reduces write amplification ... while increases the number of unmerged parts」。`defaultPartsToMerge = 15` 的注释同样写明是「empirically 给出的最低开销」。

**复杂度**：过滤后为 `O(N² × 15)`，其中 `N` 是第一步过滤后的 part 数。因为分区按月分桶、part 数又受体量上限约束，`N` 实际在几十量级，穷举完全可行——而相比贪心「取最小 15 个」能显著降低长期写放大。

### 20.3 归并流水线

`mergeBlockStreamsInternal`（`lib/storage/merge.go:42-157`）是实际的 k 路归并。`blockStreamMerger.NextBlock`（`lib/storage/block_stream_merger.go:72`）每次吐出一个已按 `(TSID, MinTimestamp)` 排序的 block：

```text
pendingBlock = 空
for b in bsm.NextBlock():                    # k 路归并输出, 按 (TSID, MinTs) 有序
    if b.TSID.MetricID ∈ dmis (已删除 metric):  localRowsDeleted += b.RowsCount; continue
    if b.MaxTimestamp < retentionDeadline:      localRowsDeleted += b.RowsCount; continue
    if pendingBlock 为空: pendingBlock = b; continue
    if pendingBlock.TSID ≠ b.TSID:             flush(pendingBlock); pendingBlock = b; continue
    if pendingBlock.tooBig() 且 与 b 不重叠:     flush(pendingBlock); pendingBlock = b; continue
    # 同一 series —— 真正合并
    unmarshalAndCalibrateScale(pendingBlock, b)        # merge.go:227-239, 统一 Scale
    tmpBlock = mergeBlocks(pendingBlock, b)            # merge.go:160-203
    if len(tmpBlock) <= 8192:  swap(tmpBlock, pendingBlock); continue
    flush(tmpBlock[0:8192]); pendingBlock = tmpBlock[8192:]   # 溢出切块
flush(pendingBlock)
```

- **两路归并**（`mergeBlocks`，`lib/storage/merge.go:160-203`）：先用 `ib1.Max < ib2.Min` / `ib2.Max < ib1.Min` 两条 fast path 整块拼接（`:168-179`），否则进入 `swap` 式双指针归并——`merge.go:201` 的 `ib1, ib2 = ib2, ib1` 是它省掉显式 while 循环的技巧，代价 `O(m + n)`。
- **retention 剪枝**（`skipSamplesOutsideRetention`，`lib/storage/merge.go:205-220`）：retention **不删除文件**，只在归并时跳过过期行（整块判断在 `merge.go:85-90`）。所以收缩 retention 是 O(1) 的元数据操作，磁盘回收推迟到下一次 merge 发生——这修正了本 wiki §9.1「Storage Part 整体过期则删除」的简化说法。
- **Scale 校准**（`unmarshalAndCalibrateScale`，`lib/storage/merge.go:227-239`）：不同 flush 批次独立定点化，`Scale` 可能不同；合并前用 `decimal.CalibrateScale`（`lib/decimal/decimal.go:13`）求公共指数并双向重标，否则 int64 直接相加会得到错误值。
- **去重**（`DeduplicateSamples`，`lib/storage/dedup.go:30-92`）：按 `-dedupInterval` 对齐时间桶，同桶内**取最大值**（`dedup.go:50-62`，对应 issue #3333），并且 `StaleNaN` 被特殊对待——只要同桶内有非 StaleNaN 的值就优先用它（issue #10196）。这**修正了本 wiki §9.4 / §十一 中「last-write-wins」的描述**。

### 20.4 复杂度汇总

| 操作 | 时间 | 空间 |
|------|------|------|
| 合并窗口选择 | `O(N² · 15)` | `O(N)` |
| k 路归并（全量） | `O(R log k)`，`R` = 总行数，`k` = 输入 part 数 | `O(k)` 个读流 + 1 个 block 缓冲 |
| 单 series 内两路归并 | `O(m + n)` | `O(m + n)` |
| Scale 校准 | `O(m + n)` | `O(1)` |
| 去重 | `O(n)` | 原地（复用 `src` 切片，零分配） |

## 二十一、参考资料

1. VictoriaMetrics 官方文档：<https://docs.victoriametrics.com/>
2. VM GitHub：<https://github.com/VictoriaMetrics/VictoriaMetrics>
3. Gorilla 论文：<https://www.vldb.org/2015/papers/p1811-pelkonen.pdf>
4. LSM-Tree 论文：<https://www.cs.umb.edu/~ponnem/csci645/slides04/cs645-04-lsm-tree.pdf>
5. Prometheus Remote Write 协议：<https://prometheus.io/docs/prometheus/latest/configuration/configuration/#remote_write>
6. TechEmpower Benchmarks：<https://www.techempower.com/benchmarks/>
7. VictoriaMetrics vs Prometheus 对比：<https://docs.victoriametrics.com/faq/#why-victoriametrics>
8. Cloudflare 使用 VM 案例：<https://blog.cloudflare.com/victoriametrics-how-cloudflare-tracks-millions-of-metrics/>
9. Roblox 使用 VM 案例：<https://victoriametrics.com/blog/roblox-case-study/>
10. Mastering Prometheus：<https://www.packtpub.com/product/mastering-prometheus/9781839216258>