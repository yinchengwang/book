# TDengine Wiki（IoT 优化的端到端时序平台）

> 参考版本：TDengine 当前 main 分支（commit `efbda38be81`，2026 era），C + Go 混合实现，TSDB 双层架构 + 多级存储 + 数据订阅，是 IoT 端到端时序平台代表。

## 一、项目定位

TDengine（涛思数据）是中国 **涛思数据（TAOS Data）** 公司开源的 **IoT 优化的时序数据库**，以 **"一站式 IoT 大数据平台 + 端边云全栈 + 内置流计算 + 多级存储"** 著称，是 **工业 IoT、车联网、电力、能源、运营商** 等场景的主流选择。它把 **TSDB（时序存储）+ 流计算（Stream Processing）+ 缓存（Cache）+ 数据订阅（Subscription）+ 多级存储（Multi-Level Storage）** 集成到单一二进制，并提供 **TDengine Cloud、TDengine IDMP、工业互联网平台**等完整产品矩阵。

- **痛点**：
  - 工业 IoT 设备数大（>100k 单集群）；
  - 单设备采集频率高（10Hz-100Hz），数据点巨大；
  - 需要 **边端（Edge）+ 云端（Cloud）** 协同；
  - 需要 **数据订阅**（Kafka-like）替代额外 ETL；
  - 需要 **流计算（窗口、聚合、JOIN）** 而非简单查询；
  - 需要 **多级存储**（内存 → SSD → HDD → 对象存储）降低存储成本；
  - 需要 **标准 SQL + C/C++/Java/Python/Go/Rust 多语言 SDK**；
- **典型用户**：工业 IoT 平台、车联网（V2X）、电力、油气、运营商、智能制造、智慧城市、智慧农业；
- **核心口号**：**"物联网大数据平台"** + **"专为 IoT 而生"** + **"一站式时序数据处理"** + **"TDengine: Big Data Platform for IoT"**。

## 二、发展历史与社区

- **起源**：2017 年由 **陶建辉（Jeff Tao）** 在 GitHub 发布，2019 年正式开源（最初 GitHub 私有）；
- **里程碑**：
  - 2017-08：TDengine 1.0 闭源版本（陶建辉个人作品）；
  - 2019-07：TDengine 2.0 开源（GitHub 公开，AGPL 协议），纯 C 实现；
  - 2020-08：2.0.x 引入 **多级存储** + **数据订阅**；
  - 2021-08：3.0 引入 **流计算引擎**（Stream Processing）+ **TDgpt（AI 集成）**；
  - 2022-09：3.0.x 重写 **元数据 + RAFT 共识**（`source/dnode/mnode/sdb/`）；
  - 2023-05：3.0.x 引入 **Edge**（边端轻量版）+ **TDengine IDMP（工业数据管理平台）**；
  - 2024-03：3.0.x 引入 **Query Connector**（外部数据源 JOIN）+ **AI 异常检测**；
  - 2025-09：3.3.x 重写 **TAOSX 数据迁移 + 多集群联邦**；
  - 2026：3.4.x 引入 **Serverless / Cloud Native**（K8s Operator + S3 远端存储）；
- **维护状态**：极活跃，**每月 1 个 minor 版本**，GitHub ~23k stars，**200+ 贡献者**，**国际贡献者占比 ~30%**；
- **社区**：TDengine Summit（年度）、Meetups（中国/美国/欧洲/印度）、社区论坛 ~50k 用户、Slack、Discord、官方 YouTube 频道（中文为主）。

## 三、许可与商业限制

- **许可证**：
  - **TDengine OSS**：**AGPL 3.0**（强 copyleft，需注意 SaaS 兼容）；
  - **TDengine Enterprise**：商业闭源（多节点集群、Replication、高级功能）；
- **商业产品**：
  - **TDengine Cloud**（AWS / GCP / Azure / 阿里云 / 腾讯云）；
  - **TDengine Enterprise**（企业级，集群 + 复制 + 监控 + 商业支持）；
  - **TDengine IDMP**（工业数据管理平台，AI/BI 集成）；
  - **TDengine Edge**（边端轻量版，资源极低）；
- **生态关系**：
  - 与 **Grafana**：官方数据源；
  - 与 **Prometheus**：通过 Remote Write 接收；
  - 与 **Kafka**：双向兼容 + 替代品（数据订阅）；
  - 与 **Apache Flink / Spark**：通过 JDBC + Kafka；
  - 与 **Kubernetes**：官方 Operator + Helm Chart；
  - 与 **AWS IoT Core / Azure IoT Hub**：对接 IoT 平台。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **国家电网**：电力设备遥测（千亿级数据点）；
  - **中石油 / 中石化**：油气管道监控；
  - **中国移动 / 联通 / 电信**：运营商网络监控；
  - **京东物流**：车队 + 仓库 IoT；
  - **蔚来 / 小鹏 / 理想**：车联网遥测；
  - **比亚迪**：工厂设备监控；
  - **宁德时代**：电池生产监控；
  - **三一重工**：工程机械远程监控；
  - **隆基绿能**：光伏电站；
  - **大疆**：无人机遥测；
  - **海外**：Samsung、LG、现代、Schneider Electric 部分场景；
- **行业**：工业 IoT、车联网、电力、油气、运营商、智能制造、智慧城市、农业、物流、新能源；
- **典型场景**：百万设备遥测采集、车联网 V2X 数据回传、智能工厂 PLC 监控、光伏逆变器监控、油气 SCADA 数据、电力 SCADA。

## 五、一句话对比定位

> TDengine 是 **"IoT 端到端 + 内置流计算 + 数据订阅 + 多级存储 + AGPL 强 copyleft"** 的时序平台；与 **InfluxDB（生态完整）**、**TimescaleDB（PG 兼容）**、**QuestDB（SQL 原生）**、**IoTDB（工业协议）**、**VictoriaMetrics（Prometheus 兼容）** 构成时序数据库阵营。TDengine 的差异化是 **"一站式 IoT 平台 + 端到端（边-云）+ 内置流计算 + 数据订阅 + 多级存储"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                     客户端 SDK / 协议层                                    │
│  - C/C++/Java/Python/Go/Rust/Node.js/C#/R/Spark/Flink Connector            │
│  - taosc（原生协议）/ REST API / JDBC / ODBC                               │
│  - 边端：TDengine Edge（资源 < 50MB）                                       │
│  - 浏览器：taosExplorer（Web UI）                                         │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ taosc / JDBC / REST
┌─────────────────────▼──────────────────────────────────────────────────────┐
│                  taosd Server（多线程 + 多进程）                           │
│                                                                            │
│   ┌────────────────────────────────────────────────────────────────────┐ │
│   │  DNode：大数据节点（默认部署）                                      │ │
│   │  ├─ VNode（虚拟节点，资源隔离）                                    │ │
│   │  │  ├─ TSDB（时序存储引擎）                                        │ │
│   │  │  ├─ SMA（流式聚合 / Rollup）                                    │ │
│   │  │  ├─ TQ（数据订阅 / Kafka 替代）                                 │ │
│   │  │  ├─ Meta（元数据：DB / Table / Super Table）                    │ │
│   │  │  └─ Query（查询算子）                                           │ │
│   │  ├─ MNode（管理节点：集群元数据 + RAFT）                          │ │
│   │  ├─ QNode（查询节点：跨 VNode 查询）                              │ │
│   │  ├─ SNode（同步节点：异地容灾）                                   │ │
│   │  └─ BNode（后台节点：数据迁移）                                   │ │
│   └────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────┘
                              │
                              │ 多级存储
                              ▼
┌────────────────────────────────────────────────────────────────────────────┐
│   Level 0：内存（Last Cache + SubmitCache）                                  │
│   Level 1：SSD / HDD（data/ 目录）                                          │
│   Level 2：对象存储（S3 / OSS / HDFS / MinIO）                              │
└────────────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **VNode 虚拟化**：单 DNode 内可承载多个 VNode（每个 VNode 是独立时序数据库），**资源隔离 + 故障隔离**；
- **双层 TSDB**：
  - **Last Cache（最近数据）**：内存 / SSD，跳表 + last_row 缓存；
  - **Data File（历史数据）**：列存文件 + 分段压缩；
- **多级存储**：数据按时间自动从 Level 0 → Level 1 → Level 2 流转，**存储成本下降 10-100x**；
- **数据订阅（TQ）**：内置 Kafka-like 流式订阅，**替代外部 ETL**；
- **流计算（SMA）**：内置窗口 / 聚合 / JOIN 算子，**无需 Flink**；
- **Super Table 模式**：设备元数据 + 子表自动建表，**省 schema 管理**；
- **AGPL**：强 copyleft，**修改需公开源码**；SaaS 部署需付费或开源所有代码。

## 七、TSDB 双层存储架构

### 7.1 内存层（Last Cache + SubmitCache）

- **Last Cache**（`source/dnode/vnode/src/tsdb/tsdbCache.c`）：
  - 每子表（Sub Table）一个 **跳表（Skip List）**，缓存每个时间序列的 **最新值**；
  - 查询 `SELECT LAST_ROW(col)` 直接走内存，**亚毫秒级响应**；
  - **last_row 优化**：金融、电力等场景核心需求；
- **SubmitCache**（写入缓冲）：
  - 批量合并写入请求；
  - 异步刷到 disk。

### 7.2 磁盘层（Data File）

```
data/
  └── vnode<vid>/
      └── tsdb/
          ├── data/                # 数据文件（TSDB2）
          │   ├── head<i>.mf       # MemTable（buffer，write-once）
          │   ├── data<i>.data     # 数据文件（last + ts + value 三列）
          │   └── data<i>.idx      # 数据索引（块索引）
          └── meta/                # 元数据
```

- **Data File 结构**：每文件由多个 **Block** 组成，每 Block 4096 行；
- **Block 结构**：
  - `Block Header`：min/max ts、block 类型、压缩方式；
  - **3 列存储**：
    - `last_value`：每行最后一列值（用于 `LAST()` 查询加速）；
    - `timestamp`：int64，Delta-of-Delta + ZigZag；
    - `value`：float64，Gorilla XOR（v3.0+）；
- **Column 压缩**：
  - 整数 / 时间戳：Delta-of-Delta + Simple8b；
  - Float64：Gorilla XOR；
  - 字符串：LZ4 / ZSTD；
  - **last 列**：每行最后一列的值特殊存储，**加速 `LAST()` 查询**。

源码：`source/dnode/vnode/src/tsdb/tsdbDataFileRW.c`、`tsdbCache.c`、`tsdbCommit.c`、`tsdbMerge.c`。

### 7.3 写入路径

```
taosc 写入
  → taosd 接收 → VNode 路由
  → VNode.SubmitCache 缓冲
  → VNode.TSDB：
    ├→ MemTable（head<i>.mf）追加写
    ├→ LastCache 更新（每子表 skip list）
    └→ 后台 commit → data file
      ├→ MemTable → Data File（flush）
      ├→ Last Cache 持久化
      └→ 后台 merge：合并小 file → 大 file
```

### 7.4 读取路径

```
SQL → Parser → Optimizer → Executor
  → VNode.Query → TSDB
    ├→ 时间区间路由：
    │  ├→ Last Cache（最近数据，亚毫秒）
    │  └→ Data File（历史数据，列读 + 压缩解码）
    ├→ SMA 流式聚合（如有 Rollup）
    └→ 算子链：Filter / Join / Group By / Window
  → 流式返回客户端
```

## 八、SMA 流式聚合与 TQ 数据订阅

### 8.1 SMA（Stream Materialized Aggregator）

```sql
-- 创建流式聚合（Rollup）
CREATE SMA TABLE rollup_5m TAGS (host NCHAR(64)) AS
SELECT _wstart AS ts, AVG(usage_idle) AS avg_usage
FROM cpu
INTERVAL(5m);
```

- 后台 **SMA Worker**（`source/dnode/vnode/src/sma/smaRollup.c`）：
  - 订阅源表的增量数据；
  - 实时维护 Rollup 表；
  - **类似 Materialized View** 但专用时序场景；
- **资源**：
  - 比 MV 资源占用更低（专用优化）；
  - 支持窗口函数、AVG、SUM、MIN、MAX、COUNT、LAST、LAST_ROW；

### 8.2 TQ 数据订阅（Kafka-like）

```sql
-- 创建订阅
CREATE TOPIC topic_offsets AS SELECT * FROM meters WHERE groupid = 1;

-- 客户端消费
taos_subscribe(topic_offsets) → 实时数据流
```

- **TQ（Time-series Queue）**（`source/dnode/vnode/src/tq/`）：
  - 订阅源表的 WAL / 增量数据；
  - 维护 **Offset**（消费进度）；
  - 支持 **Multiple Subscribers**；
  - 与 Kafka 协议兼容（**TDengine 3.2+ 提供 Kafka 协议入口**）；
- **优势**：
  - 比 Kafka 更简单（内置于 DB）；
  - 与数据写入强一致（无 Kafka offset 错位风险）；

## 九、多级存储（Multi-Level Storage）

### 9.1 配置示例

```sql
ALTER DATABASE db RETENTIONS 3650;  -- 总保留 10 年
ALTER DATABASE db KEEP 90,365,3650;  -- 90d 原始 + 365d 1m精度 + 3650d 1h精度
ALTER DATABASE db STORAGE_LEVEL 0,1,2;  -- 内存+本地+对象存储
```

### 9.2 数据流转

```
Level 0：内存 / SSD（最近 90 天，原始精度）
Level 1：HDD（90-365 天，1m 精度）
Level 2：对象存储 S3 / OSS（365-3650 天，1h 精度）
```

- 后台 **DataMover** 任务按策略自动迁移；
- **存储成本下降 10-100x**（vs 全 SSD）；
- **查询路由**：根据时间区间自动选 Level 0/1/2。

### 9.3 vs InfluxDB / VictoriaMetrics

| 维度 | TDengine 多级 | InfluxDB TSM | VM Downsampling |
|------|---------------|---------------|-----------------|
| 配置 | `KEEP + STORAGE_LEVEL` | `Retention Policy` | `-downsampling.period` |
| 存储后端 | 内存+本地+对象 | 本地 | 本地+远端 S3 |
| 查询路由 | 自动 | 仅本精度 | 自动（选最优精度） |
| 资源开销 | 0（后台） | 持续 CQ | 0（后台） |

## 十、保留策略与 TTL

### 10.1 Database / Table TTL

```sql
ALTER DATABASE db KEEP 365;             -- 保留 365 天
ALTER TABLE tb TTL 86400;                -- 子表 86400 秒后删除
```

- 后台 **TTL Job**（`source/dnode/vnode/src/meta/metaTtl.c`）：
  - 周期性扫描过期数据；
  - 整文件删除（不可恢复）；
- **保留 vs 多级存储**：TTL 控制总寿命，多级存储分层保留。

### 10.2 乱序写入处理

TDengine v3.x 支持乱序写入，但**默认假设数据顺序到达**：

- 写入时按 `ts` 排序追加到 Block；
- **Block 内**：乱序数据按 ts 排序后存储；
- **Block 间**：乱序写入会触发 **Last Cache 更新 + 数据搬迁**，开销较大；
- **生产建议**：边端先排序 + 批量提交，避免乱序。

## 十一、查询接口层

### 11.1 SQL（类标准 SQL + 时序扩展）

- **标准 SQL**：完整 SELECT / INSERT / CREATE / ALTER + 时序扩展：
  - `INTERVAL(5m)`：时间桶聚合；
  - `FILL(NULL/PREV/LINEAR/VALUE)`：空桶填充；
  - `LAST_ROW(col)`：取每组最后一行；
  - `PARTITION BY` + `SLIMIT` + `SOFFSET`：分片查询；
  - 窗口函数（v3.0+）；
  - 流计算。

### 11.2 协议

- **taosc 原生协议**（C / 自研 TCP）；
- **REST API**（`/rest/sql`）；
- **JDBC / ODBC**；
- **Kafka 协议**（v3.2+，作为 Kafka 替代）。

## 十二、事务与一致性

- **写一致性**：
  - 单节点：**Atomic Per Row**（行级原子）；
  - 集群：**RAFT 一致性**（`source/dnode/mnode/sdb/`，v3.0+）；
- **读一致性**：默认 **Linearizable**（走 RAFT leader）；
- **隔离级别**：无 MVCC，**读已提交**；
- **乱序写入**：完全支持，Block 内自动排序；
- **不支持事务回滚**：DDL 自动 commit，无 rollback。

## 十三、复制与高可用

- **VGroup 副本**：
  - 每个 VNode（时序库）有 N 个副本（默认 1，集群 3）；
  - 副本通过 **RAFT 共识** 同步；
- **SNode 异地容灾**：
  - SNode 异步复制其他 DNode 的数据；
  - 用于跨机房 / 跨区域灾备；
- **多集群联邦**：v3.3+ 支持多集群联邦查询；
- **TAOSX 数据迁移**（`tools/taosx/`）：跨集群、跨版本数据迁移工具。

## 十四、运维与生态

- **监控**：`taosd` 内置 `/metrics` Prometheus 端点；
- **备份恢复**：
  - `taosdump`（`tools/taosdump/`）：逻辑备份 + 恢复；
  - `taosX`（`tools/taosx/`）：跨集群迁移；
  - **无原生物理备份**，依靠集群复制；
- **Web UI**：`taosExplorer`（内置 Web 管理界面）；
- **Kubernetes Operator**：`TDengine` 官方 Operator + Helm Chart；
- **客户端 SDK**：
  - C/C++：原生 `taosc`；
  - Java：JDBC；
  - Python：`taospy`；
  - Go：`taosdata/driver-go`；
  - Rust / Node.js / C# / R：官方包；
  - Spark / Flink Connector。
- **生态集成**：Grafana、Prometheus、Apache Kafka、Apache Flink、Spark、Superset、Metabase。

## 十五、性能与基准

- **官方 benchmark**（`TDengine/research/benchmark/`）：
  - 写入：**数千万 rows/sec**（单实例 + 多级存储）；
  - 查询：**亚秒级**响应（含 Last Row / ASOF JOIN）；
- **TSBS（Time Series Benchmark Suite）**：TDengine 在 IoT 场景领先；
- **vs InfluxDB**：在 **设备数 >100k + 边缘 + 流计算** 场景 TDengine 领先 5-20x；
- **vs TimescaleDB**：在 **写入 + 边缘 + 流计算** 场景 TDengine 强，**生态完整度** TimescaleDB 强（PG 兼容）；
- **vs QuestDB**：在 **设备数 + 多级存储 + 边云协同** 场景 TDengine 强，**SQL 完整度** QuestDB 强；
- **vs IoTDB**：TDengine 通用 IoT 强，**工业协议（OPC UA / Modbus / MQTT）原生** IoTDB 强；
- **第三方基准**：TSBS、ClickBench 部分场景收录。

## 十六、学习路径

### 16.1 入门（1-2 周）

1. **官方文档**：`docs.taosdata.com`（中文）+ `docs.tdengine.com`（英文）；
2. **跑通 Docker**：`docker run -d -p 6041:6041 tdengine/tdengine`；
3. **taos CLI**：`taos` → 创建 database / table / insert / query；
4. **Grafana 接入**：官方 TDengine 数据源。

### 16.2 进阶（4-6 周）

1. **TSDB 源码**（**必读**）：
   - `source/dnode/vnode/src/tsdb/tsdbCache.c`：Last Cache；
   - `source/dnode/vnode/src/tsdb/tsdbCommit.c` / `tsdbCommit2.c`：Commit；
   - `source/dnode/vnode/src/tsdb/tsdbDataFileRW.c`：Data File；
   - `source/dnode/vnode/src/tsdb/tsdbMemTable.c`：MemTable；
   - `source/dnode/vnode/src/tsdb/tsdbMerge.c` / `tsdbMergeTree.c`：后台 Merge；
   - `source/dnode/vnode/src/tsdb/tsdbRead.c`：读路径；
2. **SMA 源码**：
   - `source/dnode/vnode/src/sma/smaRollup.c`：Rollup 实现；
   - `source/dnode/vnode/src/sma/smaEnv.c`：SMA 环境；
3. **TQ 源码**：
   - `source/dnode/vnode/src/tq/tq.c`：TQ 主入口；
   - `source/dnode/vnode/src/tq/tqScan.c`：扫描源数据；
   - `source/dnode/vnode/src/tq/tqOffset.c`：Offset 管理；
4. **VNode / MNode / DNode**：
   - `source/dnode/vnode/src/vnd/`：VNode 容器；
   - `source/dnode/mnode/sdb/`：集群元数据 + RAFT；
   - `source/dnode/dnode.c`：DNode 主入口；
5. **SQL 编译**：
   - `source/libs/parser/`：SQL 解析；
   - `source/libs/plan/`：执行计划；
   - `source/libs/executor/`：算子执行。

### 16.3 高级（长期）

1. **TAOSX 数据迁移**（`tools/taosx/`）；
2. **多集群联邦**（v3.3+）；
3. **TDgpt AI 集成**（异常检测 + 预测）；
4. **TDengine IDMP**（工业数据管理平台）；
5. **Cloud Native 架构**（K8s Operator + S3 远端存储）。

### 16.4 推荐资源

- **官方**：TDengine Docs、博客（`tdengine.com/blog/`）、YouTube 中文频道；
- **论文**：
  - *"TDengine: An IoT-oriented High-performance Time Series Database"*（Tao 2020）；
  - *"Gorilla: A Fast, Scalable and In-Memory Time Series Database"*（Facebook 2015）；
  - *"In Search of an Understandable Consensus Algorithm"*（Raft，Ongaro 2014）；
- **书籍**：*时序数据库 TDengine 原理与实战*（陶建辉等 2023）；
- **视频**：TDengine Summit、DTCC、KubeCon；
- **社区**：GitHub Discussions、Slack、Discord、知乎 / CSDN。

## 十七、风险与陷阱

1. **AGPL 强 copyleft**：自研派生代码若不公开，**商业产品集成需付费或商业许可**；SaaS 服务尤其注意；
2. **企业版功能闭源**：VGroup 多副本 / RAFT / 部分运维工具 **仅 Enterprise**，OSS 单节点故障 = 全量不可用；
3. **VGroup 副本调整成本**：增加 / 减少副本会触发数据迁移，**对在线业务有性能影响**；建议规划阶段确定副本数；
4. **Super Table 误用**：未使用 Super Table 直接用普通表，**会失去设备元数据管理 + 自动建表能力**；
5. **乱序写入开销**：TDengine 假设顺序写入，乱序写入会触发 Block 重排 + Last Cache 更新，**生产需在边端先排序**；
6. **多级存储数据丢失**：远端存储（S3 / OSS）故障且本地已删除，**数据可能丢失**；生产必须配 SNode 异地容灾；
7. **流计算限制**：SMA 内置流计算只支持简单窗口 / 聚合，**复杂 CEP（复杂事件处理）仍需 Flink**；
8. **JDBC 性能**：Java 应用通过 JDBC 而非 taosc 连接，**性能下降 30-50%**；Java 关键路径应用 taosc；
10. **集群分裂问题**：v3.0.x 早期版本有 RAFT 实现不完善导致的脑裂，**升级到 3.0.5+ 较稳定**；
11. **备份恢复复杂**：无原生物理备份，**只有 taosdump（逻辑）**，大数据量恢复时间长（亿级 TB 数据需数小时到数天）；
12. **工业协议依赖 Edge / Connector**：原生工业协议（OPC UA、Modbus、MQTT）需 Edge / 第三方 Connector；TDengine 主仓不直接支持；
13. **冷热分层查询延迟**：远端存储查询延迟 ~100ms-1s，**关键监控需保证 SSD 上有最近数据**。

## 十八、与本项目的关联

本项目 `engineering/` 与 TDengine 的**潜在借鉴点**：

- **TSDB 双层架构**：`engineering/src/db/storage/` 可参考 `tsdbCache.c` + `tsdbCommit.c` 的内存+磁盘双层设计；
- **Last Cache 跳表**：`engineering/src/algo/skiplist.c` 可参考 TDengine 的 Last Cache 设计；
- **Gorilla 编码**：`engineering/src/algo/compression/` 可加入 `gorilla.c`；
- **数据订阅 TQ**：`engineering/src/db/queue/` 可参考 `tq.c` 的 offset 管理；
- **多级存储**：`engineering/src/db/storage/` 可参考 Level 0/1/2 自动迁移策略；
- **Super Table 模式**：`engineering/src/db/catalog/` 可参考设备元数据 + 子表模式。

## 十九、分片与存储文件体系：固定区间哈希与 fileset 组织

本节补 §六 / §七 只画了架构图、没有给算法的部分：**表名怎么落到 vgroup**、**一个 vnode 里的文件怎么命名和分层**。

### 19.1 表名 → vgroup：固定区间哈希分片（不是一致性哈希）

TDengine 的分片算法是**「把 32 位哈希空间等分成 N 段，每段绑定一个 vgroup」**，和 Redis / DragonflyDB 用的**一致性哈希环**是两回事。

mnode 建库时一次性把区间切好（`source/dnode/mnode/impl/src/mndVgroup.c:1141-1157`，函数 `mndAllocVgroup` 起于 `:1118`）：

```c
uint32_t hashMin     = 0;
uint32_t hashMax     = UINT32_MAX;
uint32_t hashInterval = (hashMax - hashMin) / pDb->cfg.numOfVgroups;   // :1143

for (uint32_t v = 0; v < pDb->cfg.numOfVgroups; v++) {
  pVgroup->hashBegin = hashMin + hashInterval * v;                     // :1153
  if (v == pDb->cfg.numOfVgroups - 1) {
    pVgroup->hashEnd = hashMax;                                        // 末段吃掉余数
  } else {
    pVgroup->hashEnd = hashMin + hashInterval * (v + 1) - 1;            // :1157
  }
}
```

客户端 / catalog 侧的查表路径（`source/libs/catalog/src/ctgUtil.c:1397` 的 `ctgGetVgInfoFromHashValue`，函数起于 `:1365`）：

```text
fullName = dbName + SEP + tbName
h        = taosGetTbHashVal(fullName)                 # MurmurHash3_32
vg       = 二分查找 vgArray 中满足 hashBegin <= h <= hashEnd 的那个
# 比较器 ctgHashValueComp（ctgUtil.c:1352，判定体 :1356）
```

复杂度：**定位 O(log V)**（`taosArraySearch` 在按 `hashBegin` 升序排好的数组上二分），**建表 O(1)** 追加。

与一致性哈希的取舍：

| 维度 | 固定区间哈希（TDengine） | 一致性哈希（Redis Cluster） |
|------|------------------------|---------------------------|
| 环结构 | 线性等分，无虚拟节点 | 哈希环 + 虚拟节点 |
| 扩容 | 必须重建区间 + 触发数据迁移 | 只搬 1/N 数据 |
| 负载均衡 | **天然均匀**（区间等长） | 靠虚拟节点逼近 |
| 查表复杂度 | O(log V) 二分 | O(1) 但要维护环 |

由于时序库是**按时间持续写入**的、扩容本来就要伴随数据搬迁，TDengine 用「迁移成本换均衡性」是合理取舍。

### 19.2 哈希函数与可裁剪前后缀

哈希函数固定为 **MurmurHash3_32**（实现 `source/util/src/thashutil.c:74`，声明 `include/util/thash.h:40`）。

`taosGetTbHashVal`（`include/util/tutil.h:297`）支持裁掉表名前后缀再哈希：

```c
if ((prefix == 0 && suffix == 0) || tblen <= (prefix + suffix) || prefix * suffix < 0) {
  return MurmurHash3_32(tbname, tblen);              // :301  全名哈希
} else if (prefix > 0 || suffix > 0) {
  return MurmurHash3_32(tbname + prefix, tblen - prefix - suffix);   // :303  掐头去尾
} else {
  /* prefix<0 掐头 / suffix<0 去尾，拼到临时缓冲 tbName 上再哈希 */   // :305-330
  return MurmurHash3_32(tbName, offset);             // :330
}
```

**这解决了 IoT 表名的天然倾斜问题**：设备命名若是 `site01_sensor_0001 … site01_sensor_9999`，设 `prefix=7, suffix=0` 后这 9999 张表哈希的是同一段 `sensor_0001…sensor_9999`，落到同一 vgroup 的概率被大幅拉平，单 vnode 不会被打爆。

> 细节：形参 `method`（`SDbCfg.hashMethod`）目前**未参与任何分支**，算法被硬编码为 MurmurHash3_32；读代码时不要以为改 `hashMethod` 能换算法。

### 19.3 vnode 目录、fid（fileset）与文件后缀

**目录形态**（`tsdbTFileName`，`source/dnode/vnode/src/tsdb/tsdbFile2.c:359`）：

```text
<diskPath>/<SEP>vnode<SEP>vnode<vid>/<SEP><tsdbName>/<SEP>v<vid>f<fid>c<cid>ver<ver>.<suffix>
                                     ^^^^^^^^^^^^^^^ mount 分支格式串 tsdbFile2.c:369
<diskPath>/<SEP><tsdbPath>/<SEP>v<vid>f<fid>c<cid>ver<ver>.<suffix>              普通分支 :385
<diskPath>/<SEP><tsdbPath>/<SEP>v<vid>f<fid>c<cid>ver<ver>.m<mid>.<suffix>      迁移分支 :397
```

其中 `<tsdbName>` 由 vnode 建目录时写入（`source/dnode/vnode/src/tsdb/tsdbOpen.c:64`），默认值常量是 `VNODE_TSDB_DIR "tsdb"`（`source/dnode/vnode/src/inc/vnodeInt.h:100`，名字缓冲长度 `VNODE_TSDB_NAME_LEN 6` 见同文件 `:98`）。

**5 种文件后缀**（表驱动，`source/dnode/vnode/src/tsdb/tsdbFile2.c:38-42`）：

```c
[TSDB_FTYPE_HEAD] = {"head", ...};   // memtable 落盘缓冲
[TSDB_FTYPE_DATA] = {"data", ...};   // 旧版行/列存数据文件
[TSDB_FTYPE_SMA]  = {"sma",  ...};   // 流式聚合（rollup）结果
[TSDB_FTYPE_TOMB] = {"tomb", ...};   // 删除墓碑
[TSDB_FTYPE_STT]  = {"stt",  ...};   // Sorted Time Table，主力时序文件
```

**一个 fid = 一个 fileset = 一个时间片**，这是 TDengine 存储层最核心的抽象。`STFileSet`（`source/dnode/vnode/src/inc/tsdb.h:676`）同时挂 4 个单例文件（head/data/sma/tomb）和一个 STT 层级数组：

```c
struct SDFileSet {                       // tsdb.h:676
  SDiskID    diskId;
  int32_t    fid;
  SHeadFile *pHeadF;
  SDataFile *pDataF;
  SSmaFile  *pSmaF;
  uint8_t     nSttF;
  SSttFile  *aSttF[TSDB_STT_TRIGGER_ARRAY_SIZE];   // :683
};
```

**fid 的 O(1) 换算**（`tsdbKeyFid`，`source/dnode/vnode/src/tsdb/tsdbUtil.c:555`）：

```text
fid = floor(ts / tsTickPerMin[precision] / days)     // days = keepCfg.days
反查： tsdbFidKeyRange(fid, days, precision, &minKey, &maxKey)   // tsdbUtil.c:566
      minKey = tsTickPerMin[precision] * fid * days
      maxKey = minKey + tsTickPerMin[precision] * days - 1
```

查询时直接用时间窗两端夹出要碰的 fid 区间，**不做目录遍历**（`source/dnode/vnode/src/tsdb/tsdbScan.c:391`）：

```c
int32_t minFid = tsdbKeyFid(tw->skey, tsdb->keepCfg.days, tsdb->keepCfg.precision);   // :391
int32_t maxFid = tsdbKeyFid(tw->ekey, tsdb->keepCfg.days, tsdb->keepCfg.precision);   // :392
TARRAY2_FOREACH(tsdb->pFS->fSetArr, fset) {
  if (fset->fid < minFid || fset->fid > maxFid) continue;   // :396  O(|fSetArr|) 直接跳
  ...
}
```

fileset 之间按 `fid` 升序排序，比较函数 `tsdbTFileSetCmprFn`（`source/dnode/vnode/src/tsdb/tsdbFSet2.c:776`，声明 `tsdbFSet2.h:59`）：

```c
if (fset1[0]->fid < fset2[0]->fid) return -1;
if (fset1[0]->fid > fset2[0]->fid) return 1;
return 0;
```

### 19.4 超级表与 tag 不在 tsdb 文件里

一个常见误解：以为 2.x 的 `.stb`（超表定义）/ `.tsd`（tag 数据）/ `.tsu`（tag 索引）文件还在。在当前 main 分支里**这些后缀已不存在**，超表定义、tag 值、tag 索引、子表列表全部搬进了 **vnode meta**（`source/dnode/vnode/src/meta/`），底层是一组 `tdb` B-tree。

tag 索引的键结构（`source/dnode/vnode/src/inc/meta.h:145-151`）：

```c
#pragma pack(push, 1)
typedef struct {
  tb_uid_t suid;          // 超级表 uid
  int32_t  cid;           // tag 列 id
  uint8_t  isNull : 1;
  uint8_t  type  : 7;     // 值类型
  uint8_t  data[];        // tag 值 + 命中的子表 uid 列表
} STagIdxKey;
#pragma pack(pop)
```

比较器 `tagIdxKeyCmpr`（`source/dnode/vnode/src/meta/metaOpen.c:946`）的键序是 **suid → cid → isNull（NULL 最小，`:976`）→ type → data 值比较**，所以「某超级表某 tag 列的某个取值」在 B-tree 里是一个连续区间，等值条件可直接做范围扫描——这是 §二十一 裁剪算法的基础。

同族的兄弟索引键也在 `source/dnode/vnode/src/inc/meta.h` 里：`SCtbIdxKey`（suid+uid 子表索引，`:139-142`）、`SBtimeIdxKey`（btime+uid，`:166-169`）、`SNcolIdxKey`（列数+uid，`:171-174`）。

## 二十、WAL、提交与 Compaction：从内存表到 STT 层级

### 20.1 写入路径：RAFT 日志 → WAL → 内存表

vnode 的写入**不是**「client → taosd → 直接进 TSDB」，而是先过 Raft 日志持久化（`source/dnode/vnode/src/vnd/vnodeSvr.c:4004` 的 `vnodeProcessSubmitReq` 拆成三步）：

```text
vnodeProcessSubmitReq (vnodeSvr.c:4004)
  ├─ vnodeScanSubmitReq          扫描请求，校验 schema version
  ├─ vnodeHandleAutoCreateTable  (vnodeSvr.c:3047) 超级表自动建子表
  └─ vnodeHandleDataWrite        (vnodeSvr.c:3179) 逐子表写数据
        └─ raftLogAppendEntry    (source/libs/sync/src/syncRaftLog.c:255)
              ├─ walAppendLog    (syncRaftLog.c:266 → walWrite.c:876) 先写 WAL log 文件
              └─ walFsync        (syncRaftLog.c:283 → walWrite.c:903) 再 fsync
```

WAL 层有自己的 log/idx 双文件：`walCommit`（`source/libs/wal/src/walWrite.c:105`）推进提交点，`walChangeWrite`（`walWrite.c:114`）在版本跳跃时关闭旧文件、按 `firstVer` 二分定位新文件对。

写成功后才异步提交（`vnodeSyncCommit`，`source/dnode/vnode/src/vnd/vnodeCommit.c:381`）：

```c
int32_t vnodeSyncCommit(SVnode *pVnode) {
  int32_t code = vnodeAsyncCommit(pVnode, false);     // 丢到 commitTask
  vnodeAWait(&pVnode->commitTask);                    // 等它落地
  return code;
}
```

实际干活的 `vnodeCommitImpl`（`vnodeCommit.c:397`）顺序是：

```text
walPersist(vnode->pWal)               ← 先确保 WAL 落盘
syncBeginSnapshot(vnode->sync, committed)
tsdbCommitBegin(vnode->pTsdb, pInfo)  ← 进入 TSDB 提交（见 20.2）
tsdbCacheCommit(vnode->pTsdb)         ← last cache 持久化
bseCommit(vnode->pBse)                ← blob 存储引擎提交
```

### 20.2 提交 = 内存表与存量 STT 文件的 k 路归并

内存表 `SMemTable`（`source/dnode/vnode/src/inc/tsdb.h:463`）内部是 **hash 桶 + 红黑树 + 每子表一个跳表**：`STbData`（`tsdb.h:450`）持有 `SMemSkipList sl`（`tsdb.h:441`），`SMemTable` 用 `aBucket` + `tbDataTree` 两级定位子表（`tsdb.h:476-477`），桶数不够时由 `tsdbMemTableRehash`（`source/dnode/vnode/src/tsdb/tsdbMemTable.c:366`）扩容。建表入口 `tsdbMemTableCreate` 在 `tsdbMemTable.c:52`，写入入口 `tsdbInsertTableData` 在 `tsdbMemTable.c:119`。

提交某个 fileset 时，`tsdbCommitFileSet`（`source/dnode/vnode/src/tsdb/tsdbCommit2.c:373`）按 Begin → 写 TS → 写 tomb → End 四步走。关键在 `tsdbCommitOpenIter`（`tsdbCommit2.c:267`）：它把**内存表和该 fileset 下所有 STT 文件**各开一个迭代器，然后用同一套归并器合流：

```c
config.type = TSDB_ITER_TYPE_MEMT;  config.memt = committer->tsdb->imem;   // :275-288 内存表 data/tomb
TARRAY2_FOREACH(committer->sttReaderArray, sttReader) {
  config.type = TSDB_ITER_TYPE_STT;      ...                            // :297  每个 STT 一个 reader
  config.type = TSDB_ITER_TYPE_STT_TOMB; ...                            // :304
}
tsdbIterMergerOpen(committer->dataIterArray, &committer->dataIterMerger, false);  // :313
tsdbIterMergerOpen(committer->tombIterArray, &committer->tombIterMerger, true);   // :314
```

归并器本体 `tsdbIterMergerOpen`（`source/dnode/vnode/src/tsdb/tsdbIter.c:677`）是经典的 **k 路归并 + RB-tree 小顶堆**：

```c
tRBTreeCreate(merger[0]->iterTree, isTomb ? tsdbTombIterCmprFn : tsdbIterCmprFn);  // :688/:690
TARRAY2_FOREACH(iterArray, iter) {  /* 每个迭代器先塞一条进堆 */ }                // :692-704
return tsdbIterMergerNext(merger[0]);
```

比较键是 **`(suid, uid, ts)` 三元组**：`tsdbIterCmprFn`（`tsdbIter.c:639`）→ `tRowInfoCmprFn`（`source/dnode/vnode/src/tsdb/tsdbCommit.c:18`，先比 suid `:22-26`、再比 uid `:28-32`、最后 `tsdbRowCompare` `:34`）。

**复杂度**：k 个输入、总 N 行 → **时间 O(N log k)**，空间 O(k)（每路只需驻留 1 行 + 堆内指针）。

### 20.3 Compaction 触发条件与 STT 层级

Compaction 不是独立定时器，而是**挂在文件系统编辑提交钩子上**——`tsdbFSEditCommit`（`source/dnode/vnode/src/tsdb/tsdbFS2.c:932`）在提交完一次 edit 之后反向扫描所有 fileset：

```c
int32_t sttTrigger = fs->tsdb->pVnode->config.sttTrigger;              // :946
if (sttTrigger > 1 && !fs->tsdb->bgTaskDisabled) {
  TARRAY2_FOREACH_REVERSE(fs->fSetArr, fset) {
    SSttLvl *lvl = TARRAY2_FIRST(fset->lvlArr);                        // :955 只看第 0 层
    if (lvl->level != 0) { tsdbFSSetBlockCommit(fset, false); continue; }   // :956
    int32_t numFile = TARRAY2_SIZE(lvl->fobjArr);
    if (numFile >= sttTrigger && !vnodeATaskValid(&fset->mergeTask)) {
      vnodeAsync(MERGE_TASK_ASYNC, EVA_PRIORITY_HIGH, tsdbMerge, ...); // :973 异步起 merge
    }
    if (numFile >= sttTrigger * BLOCK_COMMIT_FACTOR) {                  // :977
      tsdbFSSetBlockCommit(fset, true);     // 文件堆太高，顺带把该 fid 的新写入堵住
    } else {
      tsdbFSSetBlockCommit(fset, false);
    }
  }
}
```

三个关键常量 / 判据：

| 项 | 值 / 位置 | 含义 |
|----|----------|------|
| `sttTrigger` | 默认 `TSDB_DEFAULT_SST_TRIGGER = 2`（`include/util/tdef.h:620`），落在 `SVnodeCfg.sttTrigger`（`source/dnode/vnode/inc/vnode.h:389`） | 第 0 层攒到几个 STT 文件就合并 |
| `BLOCK_COMMIT_FACTOR` | `3`（`source/dnode/vnode/src/tsdb/tsdbFS2.c:22`） | 文件数达 `sttTrigger * 3` 时触发 block commit 反压 |
| 兜底判据 | `tsdbDoMerge`（`source/dnode/vnode/src/tsdb/tsdbMerge.c:430`）`lvl->level != 0 \|\| 文件数 < sttTrigger` 直接 return（`:437`） | 双保险，防止重复调度 |

后台线程入口 `tsdbMerge`（`tsdbMerge.c:483`）：先把 fileset **拷贝一份快照**再干活（`tsdbMergeGetFSet`，`:455`），避免和并发写入打架；`sttTrigger <= 1` 时直接返回（`:495`），即**关掉 compaction**。真正干活的是 `tsdbMergeFileSet`（`tsdbMerge.c:372`）：open reader → open iter → open writer → 归并 → `TSDB_FEDIT_MERGE` 编辑文件系统。

合并时的排序同样用堆：`tMergeTreeNext`（`source/dnode/vnode/src/tsdb/tsdbMergeTree.c:1234`）每次取堆顶、推进后与新堆顶比较：

```c
pIter = (SLDataIter *)tRBTreeMin(&pMTree->rbt);                    // :1254
if (pMTree->pIter && pIter) {
  int32_t c = pMTree->rbt.cmprFn(&pMTree->pIter->node, &pIter->node);   // :1256
  if (c > 0) { tRBTreePut(&pMTree->rbt, (SRBTreeNode *)pMTree->pIter);  // :1258
               pMTree->pIter = NULL; }
  else if (!c) { continue; }   // 键相等 → 跳掉重复行（合并去重）      // :1260-1261
}
```

`else if (!c) { continue; }` 就是 **compaction 去重的关键**：同 `(suid, uid, ts)` 只保留一条。多路合并仍为 **O(N log k)**。

### 20.4 KEEP 0/1/2 → level 的映射公式

注意区分：§九 讲的 **storage level（内存 / SSD / 对象存储）** 是「数据搬到哪」，而 `tsdbFidLevel`（`source/dnode/vnode/src/tsdb/tsdbUtil.c:571`）算的是**「这个 fid 属于第几代保留策略」**，直接决定合并输出写到哪个 `SSttLvl`。

```c
nowSec = nowSec - keepTimeOffset * tsTickPerHour[precision];                       // :586
aFid[0] = tsdbKeyFid(nowSec - keep0 * tsTickPerMin[precision], days, precision);   // :588-589
aFid[1] = tsdbKeyFid(nowSec - keep1 * tsTickPerMin[precision], days, precision);   // :590-591
aFid[2] = tsdbKeyFid(nowSec - keep2 * tsTickPerMin[precision], days, precision);   // :592-593

if      (fid >= aFid[0]) return 0;      // :595  最新一段 → level 0（原始精度）
else if (fid >= aFid[1]) return 1;      // :597
else if (fid >= aFid[2]) return 2;      // :599
else                    return -1;     // :601  已超出总保留期，等 TTL 删
```

调用点有两处，都把它写进 commit/merge 的上下文：提交侧 `committer->ctx->expLevel = tsdbFidLevel(...)`（`tsdbCommit2.c:333`），合并侧 `.expLevel = tsdbFidLevel(merger->ctx->fset->fid, ...)`（`tsdbMerge.c:278`）。

- **单次计算 O(1)**（3 次除法 + 3 次比较）；
- 效果：越老的数据合并后落在越深的 level，越新的数据在 level 0 保持高压缩比原始精度——这就是 `ALTER DATABASE ... KEEP 90,365,3650` 的**存储层实现**。

## 二十一、TAG 索引裁剪与窗口推进算法

### 21.1 WHERE 条件的四分类切分

TDengine 的一条 `WHERE` 里混着时间戳条件、tag 条件、tag 值下推条件和普通列条件。执行计划需要先把它们拆开，各自交给不同层。

`filterPartitionCond`（`source/libs/scalar/src/filter.c:6330`）就是拆解器，四个输出分别是 **primary key / tag index / tag / other**：

```c
if (是 AND 逻辑节点) {
  return partitionLogicCond(...);                        // :6332-6335  递归拆 AND 链
}
switch (filterClassifyCondition(*pCondition)) {          // :6346
  case COND_TYPE_PRIMARY_KEY:  *pPrimaryKeyCond = *pCondition; break;      // :6347
  case COND_TYPE_TAG_INDEX:    *pTagIndexCond = *pCondition;               // :6353
                               /* 同时 clone 一份给 pTagCond，保证即使索引不可用也能全表校验 */
                               nodesCloneNode(*pCondition, &pTempCond);   // :6362
  case COND_TYPE_TAG:          *pTagCond = *pCondition; break;            // :6371
  default:                     *pOtherCond = *pCondition; break;          // :6377-6382
}
```

`COND_TYPE_TAG_INDEX` 分支的**双写**（`filter.c:6358-6368`）是整个裁剪链路的保险丝：tag 值既推给索引做候选集，也留一份在扫描阶段逐表复验。索引可能因为**类型不支持、索引缺失、统计信息过期**而返回 `SFLT_NOT_INDEX`（`include/libs/index/index.h:242` 定义了三态：`SFLT_NOT_INDEX / SFLT_COARSE_INDEX / SFLT_ACCURATE_INDEX`），此时仍能靠 `pTagCond` 保证结果正确。

### 21.2 两段式裁剪：索引预筛 + 逐表校验

拿到切分好的条件后，`metaGetChildUidsByTagCond`（`source/dnode/vnode/src/meta/metaTable2.c:3470`）用**三步法**把「超级表下符合条件的所有子表 uid」缩成一个小数组：

```text
Step 1  索引预筛（pTagIndexCond != NULL 时）
        candidates = doFilterTag(pTagIndexCond, ...)            // :3500
        if 失败 或 status == SFLT_NOT_INDEX:
            candidates = NULL                                    // :3501-3508 回退

Step 2  索引没命中 → 全量枚举子表
        cur = metaOpenCtbCursor(pVnode, suid, 1, 0)            // :3518
        while ((uid = metaCtbCursorNext(cur)) != 0) push(candidates, uid)   // :3524-3532

Step 3  逐表复验 pTagCond
        if (pTagCond == NULL): 直接 swap，全部通过           // :3538-3540
        else: for uid in candidates:
                  metaIsChildTableQualified(pMeta, uid, pTagCond, &qualified)  // :3546
                  if qualified: push(pUidList, uid)                          // :3553
```

索引入口 `doFilterTag`（`source/libs/index/src/indexFilter.c:1318`）先探测能力再执行：

```c
SIdxFltStatus st = idxGetFltStatus(pFilterNode, pAPI);   // :1320
if (st == SFLT_NOT_INDEX) { *status = st; return 0; }   // :1321-1324  短路，不做任何工作
int32_t code = sifCalculate((SNode *)pFilterNode, &param);   // :1334  真正下推求候选 uid
taosArrayAddAll(result, param.result);                       // :1345  并入结果集
```

`idxGetFltStatus`（`indexFilter.c:1354`）内部是 `sifGetFltHint` 的能力探测：判断这个 SNode 条件树里有没有「索引能回答的原子谓词」。

**复杂度对比**（N = 该超级表下子表数，K = 索引筛出的候选数）：

| 场景 | 时间 | 说明 |
|------|------|------|
| 索引命中且无需复验（`pTagCond == NULL`） | **O(log N + K)** | 直接取 `doFilterTag` 结果 |
| 索引命中 + 复验 | O(log N + K · c) | c = 单表 tag 判定成本 |
| 索引不可用（`SFLT_NOT_INDEX`） | **O(N · c)** | 全量枚举 + 全量复验，退化但正确 |

索引为什么能只花 `O(log N)`：§19.4 讲过，`STagIdxKey` 的键序是 `(suid, cid, isNull, type, data)`（`tagIdxKeyCmpr`，`source/dnode/vnode/src/meta/metaOpen.c:946`），等值条件 `tag_col = 'cpu0'` 正好对应一段连续 B-tree 区间。

对外的统一入口是 `metaGetChildUidsByWhere`（`source/dnode/vnode/src/meta/metaTable2.c:3571`），它把「克隆 WHERE（`metaTable2.c:3583`）→ `filterPartitionCond`（`metaTable2.c:3589`）→ `metaGetChildUidsByTagCond`（`metaTable2.c:3595`）」打包成一次调用。**先克隆再拆**是必须的，因为 `filterPartitionCond` 会把原节点置空（`source/libs/scalar/src/filter.c:6387`），直接传会破坏调用方的计划树。

### 21.3 session window：按 gap 推进的单遍扫描

session window（会话窗口）没有预定义边界，边界由**相邻两行时间戳的间隔**决定：间隔 ≤ gap 就并进当前窗口，超过 gap 就开新窗口。

逻辑节点 `SSessionWindowNode`（`include/libs/nodes/querynodes.h:571`）只有三个字段，`pGap` 注释明确单位是**微秒**：

```c
typedef struct SSessionWindowNode {     // querynodes.h:571
  ENodeType    type;
  SColumnNode* pCol;    // session window 使用的时间戳列
  SValueNode*  pGap;    // gap between two session window(in microseconds)   :574
} SSessionWindowNode;
```

执行侧 `doSessionWindowAggImpl`（`source/libs/executor/src/timewindowoperator.c:2586`）是**一次遍历、零回看**的推进算法：

```text
prevTs = INT64_MIN                                        // :2604 首行哨兵
for j in [0, block.rows):                                 // :2613
    ts = tsList[j]
    if (换表 gid != groupId) 或 prevTs == INT64_MIN:       // :2618
        开新窗口; keepTuple(ts)
    elif ts == prevTs:                                    // :2621 重复时间戳 → 同窗口
        keepTuple(ts)
    else:
        absDiff = |ts - prevTs|                           // :2625 绝对差，兼容倒序扫描
        if absDiff <= gap:                                // :2626
            keepTuple(ts)                                 //   并入当前窗口
        else:                                             // :2629
            flush(当前窗口)                                //   产出聚合结果
            开新窗口; keepTuple(ts)
    prevTs = ts
flush(最后一个窗口)                                        // :2665-2680 块尾窗口必落
```

伪代码：

```c
prevTs = INT64_MIN; win = NULL;
for (ts : rows) {
  if (ts - prevTs > gap) { flush(win); win = newWin(ts); }   // 差值 > gap → 切窗
  win.add(ts); prevTs = ts;
}
flush(win);
```

- **复杂度 O(n)**（每行只做一次减法 + 一次比较 + 一次入列），无二分、无回溯；
- `absDiff` 用绝对值而非 `ts - prevTs`（`:2625`），是为了让**倒序扫描**（`TDSDB_ORDER_DESC`）复用同一套代码；
- `prevTs` 跨块保留（只在 `!pInfo->reptScan` 的首次进入时重置，`:2602-2605`），保证**跨数据块的窗口不会被误切断**；
- 块尾窗口在 `:2665` 强制 flush，避免最后一个窗口丢失。

### 21.4 周期/滑动窗口：窗口推进 + 二分定位

周期窗口（interval / tumbling）与滑动窗口（sliding）走另一条路。物理节点 `SIntervalPhysiNode`（`include/libs/nodes/plannodes.h:1064`）同时带 **interval / offset / sliding / slidingUnit / firstDayOfWeek / timezone**：

```c
typedef struct SIntervalPhysiNode {   // plannodes.h:1064
  SWindowPhysiNode window;
  int64_t interval;  int64_t offset;  int64_t sliding;
  int8_t  intervalUnit; int8_t slidingUnit;
  int8_t  firstDayOfWeek;  void* timezone;  ...
} SIntervalPhysiNode;
```

推进与定位在 `getNextQualifiedWindow`（`source/libs/executor/src/timewindowoperator.c:415`）：

```text
① 窗口是否与当前数据块有交集？
   inCalSlidingWindow: interval != sliding 且 (win.ekey < calStart || win.skey > calEnd) → 不合格  :403-404
② 定位窗口首行位置
   若 sliding == interval（tumbling 特例）且已知上次位置 → startPos = prevPosition + 1   :436-437  O(1)
   否则 → binarySearchForKey(primaryKeys, rows, skey, order)                          :442      O(log rows)
③ 窗口尾部推进（跨越当前窗口尾部时整格跳）
   ekey += ceil((next - ekey) / sliding) * sliding     :466
   skey -= ceil((skey - next) / sliding) * sliding     :476
```

`interval == sliding` 即 tumbling window，是 sliding window 的特例（`:435` 注释写得很直白），此时相邻窗口不重叠，可以用 `prevPosition + 1` 常数级续扫；一旦 `sliding < interval`，窗口重叠，必须二分回退定位——**这条分叉正是周期窗口 O(n)、滑动窗口 O(n log n) 的根源**。

流式查询侧对应的窗口类型枚举在 `include/common/streamMsg.h:1679`：

```c
typedef enum EWindowType {
  WINDOW_TYPE_INTERVAL = 1,   // 周期
  WINDOW_TYPE_SESSION,        // 会话
  WINDOW_TYPE_STATE,
  WINDOW_TYPE_EVENT,
  WINDOW_TYPE_COUNT,
  WINDOW_TYPE_ANOMALY,
  WINDOW_TYPE_EXTERNAL,
  WINDOW_TYPE_PERIOD
} EWindowType;
```

## 二十二、参考资料

1. TDengine 官方文档（中文）：<https://docs.taosdata.com/>
2. TDengine 官方文档（英文）：<https://docs.tdengine.com/>
3. TDengine GitHub：<https://github.com/taosdata/TDengine>
4. TDengine 论文："TDengine: An IoT-oriented High-performance Time Series Database"：<https://www.taosdata.com/en/blog/2020/08/03/1650.html>
5. TDengine vs InfluxDB 对比：<https://www.taosdata.com/en/tdengine-vs-influxdb/>
6. TDengine vs TimescaleDB 对比：<https://www.taosdata.com/en/tdengine-vs-timescaledb/>
7. Gorilla 论文：<https://www.vldb.org/2015/papers/p1811-pelkonen.pdf>
8. Raft 论文：<https://raft.github.io/raft.pdf>
9. TDengine Summit 视频：<https://www.youtube.com/c/TDengine>
10. TDengine 博客：<https://tdengine.com/blog/>