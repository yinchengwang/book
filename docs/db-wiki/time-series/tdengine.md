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

## 十九、参考资料

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