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

## 十八、参考资料

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