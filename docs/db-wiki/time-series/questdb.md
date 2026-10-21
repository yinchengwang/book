# QuestDB Wiki（SQL 原生时序数据库）

> 参考版本：QuestDB 当前 main 分支（commit `2d9244fec3`，2025 era），Java 17 实现，Cairo 存储引擎 + Griffin SQL 编译器，原生列存 + 关系 + 时序混合模型。

## 一、项目定位

QuestDB 是 **QuestDB 公司** 开源的 **SQL 原生时序数据库**，以 **"零修改 SQL + 列存原生 + 极致时序吞吐 + 关系与时序统一"** 著称，是 **金融行情、IoT 遥测、APM 监控** 场景的新锐选择。它把 **列存（Native Columnar）+ 向量化执行 + 时间分区 + 关系模型** 融合到一个产品里，并提供 **InfluxDB Line Protocol / PostgreSQL Wire Protocol** 双协议入口。

- **痛点**：
  - 关系数据库（PG/MySQL）按时间分区 + 索引后写入吞吐急剧下降；
  - 时序数据库（InfluxDB/TDengine）要么 SQL 弱（InfluxQL/Flux/TQL），要么生态封闭；
  - ClickHouse 等列存 OLAP 偏分析、缺时序优化（last_value / ASOF JOIN）；
  - 需要 **金融 Tick / IoT Sensor / APM Trace / 业务事件** 等典型时序场景；
  - 需要 **标准 SQL + JDBC + Grafana + Kafka** 全生态兼容；
- **典型用户**：金融科技（量化、做市、Tick 回放）、电信运营商、IoT 平台、APM 厂商、广告平台；
- **核心口号**：**"Fast SQL for time series"** + **"Open-source database for time series and analytics"** + **"Zero modifications to SQL"**。

## 二、发展历史与社区

- **起源**：2014 年由 **Vlad Ilyushchenko（创始人）** 在 GitHub 发布，最初为个人项目；
- **里程碑**：
  - 2014-09：首个 commit，单进程 Java + 本地列存；
  - 2018-10：v4.0+, **PostgreSQL Wire Protocol** 完整兼容；
  - 2019-04：v5.0+，**InfluxDB Line Protocol** 接收；
  - 2020-07：v6.0+，**Cairo 引擎大重构**（`io.questdb.cairo` 包）；
  - 2021-04：v6.5+，**Griffin SQL 解析器**（`io.questdb.griffin`）+ **窗口函数 + JOIN**；
  - 2022-05：v7.0+，**Materialized Views**（物化视图 / 自动降采样）；
  - 2023-03：v7.1+，**多 Shard WAL + Replication**（异步复制）；
  - 2024-04：v8.0+，**Auth + TLS + 多租户**（生产级）；
  - 2025：v8.2+，**Iceberg 集成 + Parquet Export**（数据湖）；
- **维护状态**：极活跃，**每月 1 个 minor 版本**，GitHub ~14k stars；
- **社区**：Slack ~6k 人、GitHub Discussions、社区会议 QuestDB Summit、官方 YouTube 频道。

## 三、许可与商业限制

- **许可证**：**Apache 2.0**（完全开源，宽松）；
- **商业产品**：
  - **QuestDB Cloud**（AWS / GCP / Azure 托管）；
  - **QuestDB Enterprise**（商业版，包含 **多节点集群、Replication、Advanced Auth、Priority Support**）；
- **生态关系**：
  - 与 **PostgreSQL**：完整 Wire Protocol 兼容（psql / pgx / JDBC 均可连接）；
  - 与 **InfluxDB**：Line Protocol 兼容接收；
  - 与 **Kafka**：官方 Kafka Connect + Kafka Consumer 集成；
  - 与 **Grafana**：官方数据源；
  - 与 **Apache Kafka / Flink / Spark**：通过 JDBC / CDC 集成；
  - 与 **Apache Iceberg**：v8.2+ 导出 Iceberg 表。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **AdNavata / 早期客户**：金融 Tick 数据存储；
  - **IOG（Cardano）**：区块链链上数据；
  - **Telefonica**：电信网络监控；
  - **10xBank**：银行交易监控；
  - **Octopus Energy**：能源 IoT 遥测；
  - **Aquis Exchange**：交易所 Tick 数据；
  - **国内**：京东、字节跳动部分场景；
- **行业**：金融科技、电信、能源、区块链、IoT、电商、广告；
- **典型场景**：金融 Tick 回放（亿级/秒）、电信 CDR 分析、IoT 设备遥测、区块链链上数据、电商大促实时分析。

## 五、一句话对比定位

> QuestDB 是 **"零修改 SQL + 原生列存 + Cairo 时序优化 + PostgreSQL Wire Protocol + 关系与时序统一"** 的时序数据库；与 **ClickHouse（OLAP 列存）**、**TimescaleDB（PG 扩展）**、**InfluxDB（生态完整）**、**TDengine（IoT 端到端）**、**Druid（Roll-up）** 构成时序数据库阵营。QuestDB 的差异化是 **"SQL 原生 + 向量化 + 时序 + 关系统一 + 简单部署"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                     客户端协议层                                          │
│  PostgreSQL Wire Protocol（8812，PG 客户端直连）                            │
│  InfluxDB Line Protocol（9009，HTTP）                                       │
│  HTTP REST API（9000）                                                      │
│  - psql / pgx / JDBC / DBeaver / Grafana                                  │
│  - Kafka Client / Influx Telegraf / Prometheus Remote Write              │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ 协议解析
┌─────────────────────▼──────────────────────────────────────────────────────┐
│                  QuestDB Server（单进程 + 多线程池）                         │
│                                                                            │
│   ┌────────────────────────────────────────────────────────────────────┐ │
│   │  Griffin：SQL 解析 + 优化 + 执行                                   │ │
│   │  - core/src/main/java/io/questdb/griffin/                          │ │
│   │  - SqlParser → SqlOptimiser → SqlCodeGenerator                     │ │
│   │  - 算子：Filter / Join / Group By / Window / Union / CTE           │ │
│   └──────────────────┬─────────────────────────────────────────────────┘ │
│                      │ RecordCursor / PageFrame 链                          │
│   ┌──────────────────▼─────────────────────────────────────────────────┐ │
│   │  Cairo：存储引擎（CairoEngine + TableReader + TableWriter）       │ │
│   │  - core/src/main/java/io/questdb/cairo/                            │ │
│   │  - 表 = 多 Partition（按时间），Partition = 多 Column               │ │
│   │  - 列式内存表示 + 磁盘原生列存                                       │ │
│   │  - 索引：符号表（SymbolMap）+ Bitmap 索引                          │ │
│   │  - WAL：顺序写 + 后台 checkpoint + Replication                      │ │
│   └──────────────────┬─────────────────────────────────────────────────┘ │
│                      │ 磁盘 I/O                                            │
│   ┌──────────────────▼─────────────────────────────────────────────────┐ │
│   │  本地文件系统                                                       │
│   │  - data/（Cairo 数据库目录）                                       │ │
│   │  - wal/（Write-Ahead Log，多 Shard）                               │ │
│   │  - conf/（server.conf）                                            │ │
│   └────────────────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **列存原生（Native Columnar）**：磁盘按列存储，每个列一个文件，**向量化读取**（PageFrame 4096 行一批）；
- **Symbol 列优化**：低基数字符串列（如 `tag`、`host`）转 **int 索引 + 字典**，读写均优化；
- **时间分区**：表按 `designated timestamp` 自动分区，可按年/月/日/小时配置；
- **零 SQL 修改**：直接用标准 SQL 即可（`SELECT ... FROM ... WHERE ts > ...`），但 QuestDB 提供时序扩展（`LATEST ON`、`ASOF JOIN`、`SAMPLE BY`）；
- **单进程**：默认单进程部署（企业版多节点）；
- **内存映射**：通过 `Memory` 类 + `MemoryMAR`（mmap）+ `MemoryPAR`（paged）三种内存抽象；
- **无 LSM**：纯追加 + 后台 Merge + Checkpoint，**无显式 LSM 树**，更接近传统 OLTP。

## 七、Cairo 存储引擎

### 7.1 表结构

```
db/
  └── <table_name>/
      ├── <column1>.d      # 列存数据文件（append-only，按 PageFrame 4096 行）
      ├── <column1>.i      # 列存索引文件（可选，如 SymbolMap 或 Bitmap）
      ├── <column2>.d
      ├── <column2>.i
      ├── ...
      ├── <symbol>.sym     # Symbol 表（int ↔ string）
      ├── <symbol>.sym.v   # Symbol Value
      ├── <symbol>.sym.c   # Symbol Count
      ├── _meta            # 表元数据（schema、partition info）
      └── _txn             # 事务日志（DDL / DML 记录）
```

- **分区（Partition）**：每个 Partition 是一个时间区间（如一天），独立列文件；
- **列文件**：每个列文件由多个 **PageFrame（4096 行）** 组成，每 PageFrame 独立压缩；
- **元数据**：`_meta` 记录表 schema、partition 边界、symbol 数量等。

### 7.2 写入路径

```
HTTP/PG/LP 写入
  → CairoEngine 解析 → TableWriter 路由到对应 Partition
  → 内存中按 PageFrame 累计 → 满 4096 行后刷到磁盘
  → WAL 异步持久化（WAL Writer，enterprise 多 Shard）
  → 后台 Checkpoint Job：周期性 snapshot 内存状态到磁盘
```

源码：`io.questdb.cairo.TableWriter`、`io.questdb.cairo.wal.seq.TableSequencerAPI`、`io.questdb.cairo.CairoEngine`。

### 7.3 读取路径

```
SQL → SqlOptimiser → SqlCodeGenerator → RecordCursorFactory
  → PageFrame 链：Bitmap Index 过滤 → 时间范围裁剪 → 列存 PageFrame 读取
  → 向量化算子（Filter / Group By / Window）执行
  → 客户端流式消费
```

源码：`io.questdb.cairo.AbstractPageFrameCursor`、`io.questdb.cairo.FullPartitionFrameCursor`。

### 7.4 压缩

- **整数 / 时间戳**：Delta-of-Delta + ZigZag + Simple8b；
- **Float64**：Gorilla XOR（参考 Facebook Gorilla 论文）；
- **字符串**：LZ4；
- **Symbol**：int 索引 + 字典（重复值压缩率 ~10-100x）。

## 八、索引机制

### 7.1 Symbol 列（低基数字符串）

- 自动把低基数字符串列（如 `host`、`region`、`device_id`）转 int；
- SymbolMap 持久化为 `<symbol>.sym` / `<symbol>.sym.c` / `<symbol>.sym.v` 三文件；
- 读写均走 int 比较，**极大加速 GROUP BY / 过滤**。

### 7.2 Bitmap Index

- 对 Symbol 列或低基数 int 列显式建 Bitmap Index：`ALTER TABLE foo ALTER COLUMN host ADD INDEX`;
- 内部用 **Roaring Bitmap**（`io.questdb.cairo.BitmapIndexWriter`）；
- 适用：**等值过滤 + 范围扫描**，加速 10-100x。

### 7.3 时序专用索引

- **LATEST ON**：通过元数据定位最后一行（无需扫描）；
- **SAMPLE BY**：自动按时间桶聚合（Bucket 计算在元数据）；
- **ASOF JOIN**：通过内存中的排序键 + 双指针，无传统 Join 索引开销；
- **TIMESTAMP 索引**：分区文件直接定位，无 B-Tree。

## 九、降采样与物化视图

### 9.1 Materialized Views（v7.0+）

```sql
CREATE MATERIALIZED VIEW cpu_5m AS (
  SELECT timestamp, host, avg(usage_idle) AS usage_idle
  FROM cpu
  SAMPLE BY 5m
) PARTITION BY DAY;
```

- 后台 **MatView Refresh Job**（`io.questdb.griffin.engine.groupby.MicroTimestampSampler`）：
  - 增量刷新：根据源表最新时间戳，只处理新增 / 变更的 partition；
  - 全量刷新：首次创建或重建时跑全量；
- 查询直接走物化视图，**0 SQL 修改**；
- 支持 **Keyed MatView**（GROUP BY host）。

### 9.2 降采样策略对比

| 维度 | QuestDB MV | InfluxDB CQ/Tasks | TimescaleDB Cagg |
|------|-----------|-------------------|------------------|
| 触发 | 增量（CDC） | 周期（CQ/Tasks） | 增量（hypertable） |
| 资源 | 增量扫描 | 持续 Flux/查询 | 后台 worker |
| 精度 | 任意 SAMPLE BY | 单一目标 | 多级（cagg） |
| 查询语法 | 透明（MV 视为普通表） | `GROUP BY` + bucket | 透明 |

## 十、保留策略与 TTL

### 10.1 Partition TTL（v8.0+）

```sql
ALTER TABLE cpu SET PARAM ttl = 30;  -- 30 天后删除
```

- 后台 **PartitionPurgeJob**（`io.questdb.cairo.PartitionPurgeJob`）周期扫描过期 partition；
- 删除粒度：**整 partition 物理删除**（不可恢复）；
- 备份恢复需配合外部 `pg_dump` 或自研快照工具。

### 10.2 WAL 保留

- WAL 文件按 **TTL** 保留（`-wal.ttl=24h`），Checkpoint 后自动清理；
- 多 Shard WAL（v7.0+）按 Shard 独立保留。

### 10.3 乱序写入处理

QuestDB 支持乱序写入（`LATEST ON` 按 timestamp 排序），但**默认表设计强烈依赖 designated timestamp 排序**：

- 写入性能与数据顺序强相关，**乱序写入会触发 partition split / 数据搬迁**；
- v8.0+ 引入 **`dedup` key** 字段，自动 dedup（同 key + ts 仅保留 last-write）；
- **生产建议**：数据源尽量保序写入，乱序场景用 `dedup` 兜底。

## 十一、查询接口层

### 11.1 SQL（Griffin）

- **标准 SQL**：完整 SELECT / INSERT / UPDATE / DELETE / CREATE / ALTER；
- **时序扩展**：
  - `SAMPLE BY 5m`：自动时间桶聚合；
  - `LATEST ON <col>`：取每组最新一行；
  - `ASOF JOIN`：时间序列非等值连接（金融 Tick 关键）；
  - `WINDOW` 函数：标准 SQL + 时序语义；
  - `UNION` + `JOIN`：完整支持；
  - `WITH`（CTE）：标准 SQL。

### 11.2 协议

- **PostgreSQL Wire Protocol**（`io.questdb.cutlass.pgwire.PgWireContext`）：
  - 完整兼容 psql / pgx / libpq；
  - JDBC 客户端无需修改；
- **InfluxDB Line Protocol**（`io.questdb.cutlass.line.LineTcpReceiver` / `LineHttpProcessor`）：
  - 兼容 InfluxDB 1.x Line Protocol；
  - 自动创建表；
- **HTTP REST API**（`io.questdb.cutlass.http.HttpContext`）：
  - `/exec`（执行 SQL）+ `/imp`（导入 CSV）。

### 11.3 客户端

- **PostgreSQL 客户端**：psql、pgx（Go）、psycopg（Python）、node-postgres（JS）、JDBC 全部兼容；
- **Grafana**：官方 QuestDB 数据源；
- **Kafka**：官方 Kafka Connect Sink；
- **JDBC**：QuestDB 官方 JDBC 驱动。

## 十二、事务与一致性

- **写一致性**：
  - 单节点默认 **Atomic Per Row**（单行原子）；
  - WAL 提供 **Crash Recovery**（Checkpoint 后丢失数据有限）；
  - 企业版 Replication 提供 **Quorum 写入**；
- **读一致性**：默认 **Read Committed**（读最近 Checkpoint + WAL）；
- **隔离级别**：基于 **MVCC + 锁**，每行有 `TxnId`；
- **乱序写入**：完全支持，自动 dedup（`dedup` key 配置）；

## 十三、复制与高可用

- **OSS 单节点**：单进程，可配外部备份；
- **企业版 Replication**：
  - **Primary → Replica** 异步复制；
  - WAL-based 复制（WAL 序列流式复制到 Replica）；
  - 故障切换：自动 + 手动；
- **云原生**：QuestDB Cloud 提供 HA + 自动 failover；
- **冷热分层**：企业版支持 S3 归档旧 partition。

## 十四、运维与生态

- **监控**：内置 `/metrics` Prometheus 端点；
- **备份恢复**：
  - 企业版 `pg_dump` 兼容工具；
  - 自研 `QuestDB Backup`（`io.questdb.tools`）；
- **运维工具**：
  - `pgrep` / `pkill`（与 PG 类似）；
  - Web Console（`io.questdb.tools.Console`）；
  - `qdb` CLI（企业版）；
- **Kubernetes Operator**：官方 Helm Chart + QuestDB Operator；
- **客户端 SDK**：
  - Java：JDBC + QuestDB Client；
  - Go：pgx 兼容；
  - Python：psycopg 兼容；
  - JS / Rust / C#：PG 客户端兼容。

## 十五、性能与基准

- **官方 benchmark**：`questdb/questdb` 仓库 `benchmarks/`：
  - 写入：**数百万 rows/sec**（列存 + 顺序写）；
  - 查询：**亚秒级**响应（向量化执行 + 列存）；
- **vs InfluxDB**：在 **Tick 数据 + 标准 SQL 场景** QuestDB 领先 5-10x；
- **vs ClickHouse**：在 **时序写入 + ASOF JOIN** 场景 QuestDB 强，**复杂聚合** ClickHouse 强；
- **vs TimescaleDB**：在 **写入吞吐 + 时序 SQL 表达力** QuestDB 强，**生态完备性** TimescaleDB 强（PG 兼容）；
- **vs TDengine**：在 **SQL 完整度** QuestDB 强，**IoT 端到端生态** TDengine 强；
- **第三方基准**：Yahoo TSBS、Time Series Benchmark Suite（TSBS）、ClickBench 部分场景收录。

## 十六、学习路径

### 16.1 入门（1-2 周）

1. **官方文档**：`questdb.io/docs/`（Quick Start、SQL、协议、客户端）；
2. **跑通 Docker**：`docker run -d -p 8812:8812 -p 9000:9000 questdb/questdb`；
3. **PostgreSQL Wire Protocol 接入**：`psql -h localhost -p 8812 -U admin questdb`；
4. **Grafana 看板**：导入官方 QuestDB Dashboard。

### 16.2 进阶（4-6 周）

1. **Cairo 引擎源码**（**必读**）：
   - `io.questdb.cairo.CairoEngine`：引擎入口；
   - `io.questdb.cairo.TableWriter`：写路径；
   - `io.questdb.cairo.TableReader`：读路径；
   - `io.questdb.cairo.AbstractRecordCursorFactory`：游标工厂；
   - `io.questdb.cairo.SymbolMap`：符号表；
   - `io.questdb.cairo.BitmapIndexWriter`：Bitmap 索引；
   - `io.questdb.cairo.wal.seq`：WAL 序列；
2. **Griffin SQL 引擎源码**：
   - `io.questdb.griffin.SqlParser`：SQL 解析；
   - `io.questdb.griffin.SqlOptimiser`：优化器；
   - `io.questdb.griffin.SqlCodeGenerator`：代码生成；
   - `io.questdb.griffin.engine.groupby.MicroTimestampSampler`：SAMPLE BY；
   - `io.questdb.griffin.engine.functions.latest.LatestBy*`：LATEST ON；
   - `io.questdb.griffin.engine.join.LtJoin*`：ASOF JOIN；
3. **协议层**：
   - `io.questdb.cutlass.pgwire.PgWireContext`：PG 协议；
   - `io.questdb.cutlass.line.LineTcpReceiver`：InfluxDB LP；
   - `io.questdb.cutlass.http.HttpContext`：HTTP REST。

### 16.3 高级（长期）

1. **WAL + Checkpoint**：`io.questdb.cairo.wal`；
2. **Replication**（企业版）：`io.questdb.griffin.engine.table.AsyncFilteredRecordCursor`；
3. **物化视图增量刷新**：`io.questdb.griffin.engine.groupby.MatViewRefreshJob`；
4. **Iceberg 集成**（v8.2+）：数据湖导出；
5. **JIT 执行**（实验）：C2 / GraalVM 集成。

### 16.4 推荐资源

- **官方**：QuestDB Docs、YouTube QuestDB、官方博客（`questdb.io/blog/`）；
- **论文**：
  - *"Gorilla: A Fast, Scalable and In-Memory Time Series Database"*（Facebook 2015）；
  - *"Roaring Bitmaps"*(Chambi et al. 2016)；
  - *"ASOF Join: Time Series Join in QuestDB"*（QuestDB Blog 2023）；
- **视频**：QuestDB Summit、CMU DB Group、FOSDEM；
- **社区**：QuestDB Slack、GitHub Discussions、Reddit r/questdb。

## 十七、风险与陷阱

1. **生产分区不均**：`designated timestamp` 选择错误（如用了更新时间戳而非事件时间）会导致**所有数据落到单一 partition**，查询极慢；
2. **Symbol 误用**：高基数列（UUID、用户 ID）误用 Symbol 会导致 **dictionary 爆炸**；用普通 String 列；
3. **Materialized View 全量刷新成本**：首次创建 MV 或 DDL 变更触发全量刷新，**对亿级表可能耗时数小时**；建议用 Keyed MV 缩小刷新范围；
4. **ASOF JOIN 内存**：ASOF JOIN 大表内存可能 OOM，**生产需控制单批次大小 + Streaming 算子**；
5. **Dedup 字段限制**：v8.0 dedup 仅支持最近 N 天 + 特定字段组合，**不适用于完整去重**；高基数场景慎用；
6. **企业版功能闭源**：Replication / Advanced Auth / Materialized View 增量刷新部分**仅企业版**，OSS 仅有基础功能；
7. **PostgreSQL 协议边界**：QuestDB 不支持 PG 完整协议（部分 DDL、Transaction 隔离级别有差异），**直接对接 PG 客户端可能踩坑**；
8. **乱序写入开销**：大量乱序写入会触发 partition split，**写入吞吐急剧下降**；生产需在数据源端去重 + 排序；
9. **Kafka Connect 限制**：官方 Connector 仅 Sink，**Source Connector 需自研 CDC**；
10. **Iceberg 导出 Beta**：v8.2 Iceberg 集成仍 Beta，**Schema Evolution 场景未充分测试**；
11. **HA 缺失 OSS 版**：OSS 单节点故障 = 全量不可用；生产必须用 Enterprise + Replication 或自建 DR；
12. **冷热分层缺失**：无原生冷热分层，**只能用外部 ETL + S3**，成本与查询延迟需权衡。

## 十八、与本项目的关联

本项目 `engineering/` 与 QuestDB 的**潜在借鉴点**：

- **Cairo 列存**：`engineering/src/db/storage/` 可参考 `io.questdb.cairo.TableWriter` 的 PageFrame 抽象；
- **Symbol 列**：`engineering/src/db/rel/` 可参考 SymbolMap 设计处理低基数字符串；
- **ASOF JOIN**：`engineering/src/db/sql/` 可参考 `LtJoin*` 实现非等值连接；
- **Materialized View 增量刷新**：`engineering/src/db/sql/matview.c` 可参考 `MatViewRefreshJob` 的增量逻辑；
- **PostgreSQL Wire Protocol**：`engineering/src/db/net/pg_proto.c` 可参考 `PgWireContext` 的协议实现；
- **Griffin SQL 编译**：`engineering/src/db/sql/parser.c` 可参考 `SqlParser` + `SqlOptimiser` 的解耦。

## 十九、Page Frame 向量化执行

§6 与 §7.3 只提到「PageFrame 4096 行一批」，这一节拆开讲：QuestDB 的查询执行**没有「逐行游标」这一层**，filter 与 aggregate 的最小工作单位不是一行，而是一个 **PageFrame**（列式分批）。三段：帧的表示、帧的分发与并行 reduce、以及零分配的原因。

### 19.1 PageFrame 的表示：裸地址而非 Java 数组

`PageFrame`（`core/src/main/java/io/questdb/cairo/sql/PageFrame.java:66` 起）是一个只读接口，描述「某一个 partition 的一段连续行区间 × 表里的若干列」：

| 方法 | 位置 | 语义 |
|------|------|------|
| `getColumnCount()` | `PageFrame.java:66` | 帧内列数（允许为 0） |
| `getPageAddress(int col)` | `PageFrame.java:156` | 该列本帧的**连续内存起始地址**（`long`，由 mmap 得到） |
| `getPageSize(int col)` | `PageFrame.java:166` | 该列本帧的字节长度 = 行数 × `pow2SizeOf(type)` |
| `getAuxPageAddress(int col)` | `PageFrame.java:48` | 变长列（String / Varchar / Binary）的 offset 副向量地址 |
| `getPartitionLo()` / `getPartitionHi()` | `PageFrame.java:202` / `:192` | 所属 partition 的时间下界 / 上界 |
| `getFormat()` | `PageFrame.java:73` | `PartitionFormat.NATIVE` 或 `PARQUET` |

关键在返回类型是 `long` 而不是 `long[]`：一帧的数据本来就是一段连续的 mmap 地址，`Unsafe.getLong(addr + (row << 3))` 就能读任意一行，**中间没有任何对象、没有任何拷贝、没有任何装箱**。变长列走「主向量 + aux 向量」双地址结构，aux 里存相对偏移，于是变长列同样能被无指针解引用地顺序读。

### 19.2 惰性游标 → 帧序列 → 分片队列 → 并行 reduce

```
PageFrameCursor（惰性，next() 一次一帧）
   │  逐帧把地址写进 PageFrameAddressCache（扁平 DirectLongList）
   ▼
PageFrameSequence —— of(...) 绑定查询计划 + 帧缓存
   │  next() / next(dispatchLimit, countOnly) 按帧发任务
   ▼
MessageBus：分片 RingQueue<PageFrameReduceTask>
   │  N 个 PageFrameReduceJob（每 worker 一个 clone）轮流抢
   ▼
PageFrameReducer.reduce(workerId, record, task, ...)  ← filter / aggregate 在这里跑
```

- 帧缓存 `PageFrameAddressCache`（`cairo/sql/PageFrameAddressCache.java:54`）用**一张扁平表**存所有帧所有列的地址，索引公式就写在文件注释里（`:56`）：`frameIndex * columnCount + columnIndex`；`:108` 的 `add(frameIndex, frame)` 追加一帧，`:364` 的 `getPageAddresses()` 取出整个 `DirectLongList`。没有 per-frame 对象，也没有哈希表。
- 分片队列是**多条**而非一条（`MessageBus.java:146` `getPageFrameReduceQueue(int shard)`、`:144` `getPageFrameReducePubSeq(int shard)`），分片数来自 `getPageFrameReduceShardCount()`。分片的意义是压 CAS 竞争：worker A 正在抢 shard 0 的 CAS 时，shard 1 完全不受影响。
- worker 侧 `PageFrameReduceJob.run()`（`cairo/sql/async/PageFrameReduceJob.java:179`）从自己那份随机洗牌过的 shard 列表开始轮转——构造函数里先填 `shards[i] = i`，再用 Fisher-Yates 打乱（`PageFrameReduceJob.java:67`-`:85`），源码注释写得很直白：*shuffle shard indexes such that each job has its own pass order over the shared queues*。`consumeQueue`（`:196`）取任务，取到后：
  - `:220` 先判 `frameSequence.isActive()`——查询已被 cancel 的序列，任务直接丢弃（这是查询取消能快速响应的原因）；
  - `:274` `frameSequence.getReducer().reduce(...)` 真正干活；
  - `:243` 在 `finally` 里 `getReduceFinishedCounter().incrementAndGet()`——无论成功、异常还是被取消计数都会加，主线程的 `PageFrameSequence` 据此判断是否全部收齐。

### 19.3 零分配：worker 侧 filter + aggregate 伪代码

```java
// 伪代码：一次 reduce 只处理一帧，全程不在堆上 new 任何对象
void reduce(int workerId, PageFrameMemoryRecord record, PageFrameReduceTask task, ...) {
    final long[] addrs = addrCache.getPageAddresses();          // 复用，off-heap DirectLongList
    final int    fi    = task.getFrameIndex();                  // 帧号，不是行号
    final long   tsBase= addrs.getQuick(fi * colCount + tsIdx); // 扁平寻址
    final long   n     = addrCache.getFrameSize(fi);           // 本帧行数

    long m = 0;                                                 // 选出行号，写进复用的 DirectLongList
    for (long r = 0; r < n; r++) {
        long ts = Unsafe.getLong(tsBase + (r << 3));            // 直接读 mmap 地址
        if (ts >= lo && ts < hi) sel[m++] = r;                 // 时间范围谓词
    }
    // 把 m 个行号喂给分组聚合；聚合状态在 off-heap map 里，同样跨任务复用
    for (long i = 0; i < m; i++) agg.update(record, sel[i]);
}
```

复杂度：设帧数 F、每帧行数 R、总行数 N = F·R，则时间 O(N)，P 个 worker 并行后 O(N/P)；
空间 O(C)（C 是常量：地址表 + 选中行号缓冲 + 聚合状态），**与 N 无关**；
GC 压力 O(0)——`addrs` / `sel` / `map` 都是 off-heap 的 `DirectLongList` / `DirectMap`，跨任务复用。

### 19.4 与逐行游标的复杂度对比

| 维度 | 逐行 RecordCursor | PageFrame |
|------|------------------|-----------|
| 每行代价 | N 次虚调用 `record.getLong(col)`，内联失败即退化为接口分派 | 每列一段地址，谓词可整段交给 `Vect.*` 原语 |
| cache 局部性 | 按行取列 → 跨列跳跃 | 按列取段 → 顺序预取 |
| 跨线程 | 需要拷 row id 列表或重放过滤 | 直接派发帧号，worker 自己重算谓词 |
| 取消响应粒度 | 受限于已派发行的批次 | 受限于单帧大小 |

## 二十、乱序（O3）写入：commitLag 自适应与三路归并

§10.3 说「乱序写入会触发 partition split / 数据搬迁」。这一节讲清代价从哪来，以及 QuestDB 用两个机制把它压下去：**提交滞后（commitLag）反馈控制** + **只对重叠区做三路归并**。

### 20.1 commitLag：把「乱序尾巴」留在内存里等下一批

配置项是 `CAIRO_COMMIT_LAG`，默认 10 分钟（`PropServerConfiguration.java:1884`），实际传给 writer 的是 `CAIRO_O3_MAX_LAG`（`:1885`，单位微秒），最终进 `TableWriter.o3Commit(long o3MaxLag)`（`cairo/TableWriter.java:8582`）。

滞后值不是固定常量，而是**带滑动窗口的反馈控制器**（`TableWriter.java:8634`-`:8679`）：

```
// 伪代码（对照 TableWriter.o3Commit）
lagError = 表内已提交 maxTimestamp - 本批最小时间戳    // > 0 说明本批与已有数据重叠
if (lagError > 0)  effLag += lagError * INCREASE(=1.5);  effLag = min(effLag, o3MaxLag)  // TableWriter.java:8648
else               effLag += lagError * DECREASE(=0.5);  effLag = max(effLag, 0)        // TableWriter.java:8652

// 窗口内取最大值，避免一次异常批次把 effLag 抬上去又迅速回落
for i in 0..n-1: spreads[i] = spreads[i+1]; m = max(m, spreads[i])
spreads[n] = effLag;  effLag = max(effLag, m)     // 数组长度见 TableWriter.java:619

threshold       = o3TimestampMax - effLag
lagThresholdRow = lower_bound(sortedTimestamps, threshold)   // 有界二分，TableWriter.java:8672
lagRowCount     = min(o3RowCount - lagThresholdRow - 1, maxUncommittedRows)
```

两个系数是接口默认实现：`getO3LagIncreaseFactor()` = 1.5（`cairo/CairoConfiguration.java:590`）、`getO3LagDecreaseFactor()` = 0.5（`:586`）。窗口长度是 `CAIRO_O3_LAG_CALCULATION_WINDOW_SIZE`，默认 4（`PropServerConfiguration.java:1873`）。

**取舍**：落在 `lagRowCount` 之上的行**不落盘**，留在内存 O3 缓冲（`CAIRO_O3_COLUMN_MEMORY_SIZE`，默认 8 MB，`PropServerConfiguration.java:1879`）等下一批合并。所以——

- 写入保序（时间戳单调递增）时 `lagError ≤ 0` 恒成立，`effLag → 0`，全部行直接落盘，走最快路径；
- 写入乱序且跨度大时 `effLag` 顶到 `o3MaxLag` 上限，最多只能压住尾部固定时长，其余部分必须真归并；
- 控制器的稳态是**不对称**的（1.5 涨 / 0.5 跌），意味着系统倾向于快速放大容忍度、快速收回，避免一批偶发乱序就把后续所有写入都拖进 lag 缓冲。

`maxUncommittedRows` 是硬上限：即使 `effLag` 允许留更多行，超过它的部分也会被截断（`TableWriter.java:8680`-`:8696`）；极端情况下（`o3MaxLag` 与 `maxUncommittedRows` 都盖不住）直接提交一半 lag 行（`:8692`）。

### 20.2 排序：阈值决定 radix sort 还是 quicksort

排序对象是 **16 字节的 (timestamp, rowId) 定长条目数组**。选择逻辑在 `TableWriter.java:8606`：

```
if (o3RowCount > 600 || !o3QuickSortEnabled)
    Vect.radixSortLongIndexAscInPlace(addr, n, scratch);  // TableWriter.java:8608  O(k·n)，k 固定 8 字节 → 线性
else
    Vect.quickSortLongIndexAscInPlace(addr, n);            // TableWriter.java:8610  O(n log n)，小 n 常数更小
```

`CAIRO_O3_QUICKSORT_ENABLED` 默认 `false`（`PropServerConfiguration.java:1887`），所以默认恒走基数排序。基数排序能线性是因为 key 是定长 8 字节——这跟变长字符串排序完全不同，后者做不到。

### 20.3 三路归并：prefix / merge / suffix

排序完成后，`O3PartitionJob.processPartition`（`cairo/O3PartitionJob.java:735`）把结果切成三段，用三个常量标记每段的来源（`cairo/TableWriter.java:168` `O3_BLOCK_NONE = -1`、`:169` `O3_BLOCK_O3 = 1`、`:166` `O3_BLOCK_DATA = 2`）：

```
已落盘 DATA:  [.................................]
本批 O3:                       [.......]
结果:            [prefix DATA][ merge ][suffix O3]
                              ↑ 只有这一段需要真归并
```

分支判定（源码里用 ASCII 画了四张示意图）：

| 情形 | 处理 | 源码位置 |
|------|------|----------|
| O3 整体落在已有数据之后 | `prefix = DATA` 全量、`suffix = O3`，**零归并** | `O3PartitionJob.java:1012` |
| O3 落在已有数据中间 | 二分切出 prefix，其余进 merge | `O3PartitionJob.java:1031` |
| O3 落在已有数据之前 | `prefix = O3`、`suffix = DATA`，**零归并** | 同函数另一分支 |

有个容易漏掉的细节：`mergeEquals`（`O3PartitionJob.java:990`）只在 **dedup / replace 模式**下才为 1——此时 `timestamp == o3TimestampHi` 的行必须被拉进 merge 区，否则去重会漏；非 dedup 时留 0，因为把这些相等行并进去会导致无谓的 partition 重写。

三段的执行分工是：`prefix` / `suffix` 走纯 memcpy，只有 `merge` 段进入归并。归并按列并行，分派入口是 `O3OpenColumnJob.openColumn`（`cairo/O3OpenColumnJob.java:1445`），按区间位置落到 `appendNewPartition`（`:2149`，分区还不存在）/ `appendFixColumn`（`:1802`，纯追加）/ `appendMidPartition`（`:1956`，追加到已有分区中间）/ `mergeFixColumn`（`:2410`，真正的重叠归并）/ `mergeMidPartition`（`:2818`）/ `mergeLastPartition`（`:2678`）。

### 20.4 归并索引：bit63 当来源标记

重叠区的归并**只做一次**，之后所有列共享这一个索引：

```
// 伪代码（对照 O3PartitionJob.createMergeIndex，cairo/O3PartitionJob.java:1806）
long idx = malloc((m + n) * 16);
Vect.mergeTwoLongIndexesAsc(
        srcDataTimestampAddr, mergeDataLo, mergeDataHi - mergeDataLo + 1,     // 已落盘的一段（裸 int64 数组）
        sortedTimestampsAddr + mergeOOOLo * 16, mergeOOOHi - mergeOOOLo + 1,    // O3 的一段（16 字节条目）
        idx);
```

`Vect.mergeTwoLongIndexesAsc` 声明在 `std/Vect.java:266`，落到 native 的 `binary_merge_ts_long_index`（`core/src/main/c/share/ooo.cpp:456`）。核心是一个**双指针归并**，`ooo.cpp:470` 是整个设计的关键一行：

```c
dest[r++].i = (1ull << 63) | its;   // bit63 = 1 → 这一行来自已落盘的 data
```

O3 条目的 `rowId` 是纯行号（bit63 = 0）。于是归并索引里的 16 字节条目自带「我该去哪个源取第几行」的信息，**不需要额外的 source 数组**。

每列的搬运是一次无分支 gather（`core/src/main/c/share/ooo_dispatch.h:168`）：

```cpp
const T *sources[] = {src2, src1};                    // ooo_dispatch.h:169  0 = O3 缓冲, 1 = 已有数据
for (int64_t i = 0; i < count; i++) {
    MM_PREFETCH_T0(index + i + 64);                   // 软件预取
    const auto r    = reinterpret_cast<uint64_t>(index[i].i);
    const auto pick = r >> 63u;                       // ooo_dispatch.h:173  取出 bit63
    const auto row  = r & ~(1ull << 63);
    dest[i] = sources[pick][row];                     // ooo_dispatch.h:175
}
```

`O3CopyJob.mergeCopy`（`cairo/O3CopyJob.java:402`）按列类型选具体原语：变长列（String / Varchar / Array / Bin）走 `ColumnType.getDriver(...).o3ColumnMerge`（`:415`-`:424`）；designated timestamp 列直接 `Vect.oooCopyIndex`——归并索引本身就是输出，索引即数据（`:427`）；其余定长列按字节宽度选 `Vect.mergeShuffle{8,16,32,64,128,256}Bit`（`:430` 起，BOOLEAN/BYTE→8Bit、SYMBOL/INT→32Bit 等）。

复杂度：归并区 m 行已有数据 + n 行 O3 → 双指针建索引 O(m + n)，逐列 gather 也是 O(m + n)，额外空间 O(m + n) × 16 字节索引。与「整分区重写」的 O(partitionSize) 相比，非重叠部分完全不动。

### 20.5 列版本 MVCC 与 TxnScoreboard

QuestDB 的 MVCC 是**列级**而非行级：`_cv` 文件里每条记录是 `(partitionTimestamp, columnIndex) → (txn, columnTop)`，写入接口是 `ColumnVersionWriter.upsert`（`cairo/ColumnVersionWriter.java:241`）。`columnTop` 表示该列在这个 partition 里从第几行开始才有数据（后加的列、被删过的列），查询时 `[0, columnTop)` 读作 NULL。

读侧的一致性判定交给 `TxnScoreboard`（接口 `cairo/TxnScoreboard.java:56`，实现 `cairo/TxnScoreboardV2.java:226`）。算法是**位图 + CAS 槽位**：

```
// 伪代码（对照 TxnScoreboardV2.isRangeAvailable，cairo/TxnScoreboardV2.java:226）
updateMax(toTxn);                         // 先把 toTxn 推到最新，堵住与 acquireTxn 的竞态
if (activeReaderCount == 0) return true;  // 没人持锁 → 直接可读

for i in 0..bitmapCount:
    b = volatileRead(bitmapMem[i])        // 稀疏：只扫置了位的字
    while b != 0:
        id = i * 64 + ctz(b)              // Long.numberOfTrailingZeros
        t  = volatileRead(entriesMem[id])
        if (t > UNLOCKED && fromTxn <= t && t < toTxn) return false
        b &= b - 1                        // 清最低位
return true
```

写侧是 CAS 抢槽位（`TxnScoreboardV2.java:206` 的 `incrementTxn`：`Unsafe.cas(..., UNLOCKED, txn)` 成功才置位图）；`isOutdated(txn)` 就是 `txn < getMax()`（`:221`）。

复杂度 O(k)，k 是当前活跃 reader 数，**与槽位总数无关**——位图字内的 `ctz` 循环天然跳过空槽。槽位总数由 `CAIRO_O3_TXN_SCOREBOARD_ENTRY_COUNT` 决定，默认 16384 并向上取 2 的幂（`PropServerConfiguration.java:1956`）。

## 二十一、SQL 编译优化与 SAMPLE BY 分桶填充

§11.1 列了 `SAMPLE BY` / `LATEST ON` / `ASOF JOIN` 这些时序扩展。这一节讲它们在编译器里到底被改写成了什么。

### 21.1 编译链路与「英式拼写」

```
SqlCompiler（接口）
  → SqlCompilerImpl 实现               core/.../griffin/SqlCompilerImpl.java:159
      → SqlParser 解析成 QueryModel 树   griffin/SqlParser.java:107
      → SqlOptimiser.optimise(...)       griffin/SqlOptimiser.java:116 类声明 / :13140 方法
      → SqlCodeGenerator 生成游标工厂树   griffin/SqlCodeGenerator.java:413
```

**注意类名是 `SqlOptimiser`（英式 -ise），不是 `SqlOptimizer`**（`SqlOptimiser.java:116`）。

QuestDB 的优化器不是「规则匹配 + 代价估计」，而是**对 QueryModel 树做结构改写**，每个 `rewriteXxx` 返回一棵新树：`rewriteSampleBy`（`:9540`）、`rewriteSampleByFromTo`（`:9971`）、`rewriteSelectClause`（`:10482`）、`rewriteOrderBy`（`:9063`）、`rewriteDistinct`（`:8672`）。

### 21.2 SAMPLE BY → GROUP BY timestamp_floor

`SAMPLE BY 5m` 本身不进入执行器，它被改写成普通的 `GROUP BY timestamp_floor('5m', ts)`。改写入口 `SqlOptimiser.rewriteSampleBy`（`SqlOptimiser.java:9540`）有几道硬门槛（`:9592`-`:9600`）：

- 必须显式给出 `sampleByOffset`——`ALIGN TO FIRST OBSERVATION`（offset 为 null）走不了快路径；
- `!hasLinearFill(fill)`——`FILL(LINEAR)` 被踢回遗留 SAMPLE BY 路径（判定函数在 `:545`，逐项扫 fill 列表找 `linear` 关键字）；
- `FROM` / `TO` 不能是绑定变量或函数调用（`:9599` 排除了 `BIND_VARIABLE` / `FUNCTION` / `OPERATION` 三种 node type）。

另有几条直接报错的规则：`GROUP BY` 与 `SAMPLE BY` 不能并存（`:9563`）、`SAMPLE BY` 不能配 `WINDOW JOIN` / `HORIZON JOIN`、select 列不允许通配符 `*`。

改写后的形态：

```
-- 用户写的
SELECT avg(usage) FROM cpu WHERE ts IN '2026-07' SAMPLE BY 5m FILL(NULL);

-- rewriteSampleBy 之后的模型（伪代码，非字面 SQL）
SELECT avg(usage), timestamp_floor('5m', ts) AS ts     ← bucket key 提升为 group-by key
  FROM cpu WHERE ts IN '2026-07'
 GROUP BY 1 ALIGN TO CALENDAR
-- 再由 generateFill 在 groupBy 工厂外面套一层填充游标（见 21.3）
```

桶边界由 `timestampSampler.round(ts + tzOffset)` 算出（`griffin/engine/groupby/AbstractNoRecordSampleByCursor.java:252`）。这个 `+ tzOffset` 不是装饰：桶是**本地日历桶**，DST 切换日「一天」可能是 23h 或 25h。`nextSamplePeriod`（`:258`）专门处理回拨——`round` 向下取整可能把新桶的边界落在切换点之前，于是要按边界时刻的偏移量修正（`:267`）。

### 21.3 FILL 补空桶

聚合只会产出**有数据的桶**。`FILL` 负责在空桶上补值，这一层是独立套在 groupBy 外面的游标。入口 `SqlCodeGenerator.generateFill`（`griffin/SqlCodeGenerator.java:3867`）先做三件事：

1. `:3912` `FILL(NONE)` 短路——直接把底层 groupBy 工厂原样返回，一个包装游标都不建；
2. `:3922` 断言 `LINEAR` 不会走到这里（它必须留在遗留路径，否则会静默退化成 PREV 语义）；
3. `:3923` `PREV` 在快路径里只是个占位 `NullConstant.NULL`，真正的填充交给游标自己按行判断。

工厂选择分两代：

| 代 | 工厂 | 触发条件 |
|----|------|----------|
| 新（快路径） | `SampleByFillRecordCursorFactory`（`SqlCodeGenerator.java:4565`） | 走 `generateFill`，每列一个 `fillModes`：`FILL_CONSTANT=-1` / `FILL_PREV_SELF=-2` / `FILL_KEY=-3`（`griffin/engine/groupby/SampleByFillRecordCursorFactory.java:89`-`:91`） |
| 旧（按 FILL 类型分派） | 全部在 `SqlCodeGenerator` 里直接 `new`：`SampleByFillPrevRecordCursorFactory`（`SqlCodeGenerator.java:8923`）/ `SampleByFillNoneRecordCursorFactory`（`:9002`）/ `SampleByFillNullRecordCursorFactory`（`:9119`）/ `SampleByFillValueRecordCursorFactory`（`:9201`），keyed 与 not-keyed 各一份 | `FILL(LINEAR)` 等遗留路径 |

补桶有**两种策略**：

```
// keyed 路径（griffin/engine/groupby/SampleByFillPrevRecordCursor.java:149 buildMap）
// 一次扫描把整个时间域的桶建进 Map，然后线性 emit
buildMap();  for (bucket in map) emit(bucket);
// 时间 O(N + G)，内存 O(G)，G = 桶数

// not-keyed 单序列路径（griffin/engine/groupby/SampleByFillNoneRecordCursor.java:102 hasNext）
initTimestamps();
if (mapCursor.hasNext()) return true;    // 上一批还没吐完
if (baseRecord == null)    return false; // 底游标耗尽 → 结束
buildMap();                               // 边扫边建，吐空即回收
return mapCursor.hasNext();
```

`FILL(LINEAR)` 的插值是一次线性公式（`griffin/engine/groupby/InterpolationUtil.java:46`）：

```java
public static double interpolate(long x, long x1, double y1, long x2, double y2) {
    return (y1 * (x2 - x) + y2 * (x - x1)) / (x2 - x1);
}
```

每个空桶 O(1)，但需要记住左右两个已知桶 `(x1,y1)` / `(x2,y2)`；左侧没有左邻时走 `interpolateBoundary`（`:155`），由各 `GroupByFunction` 自己定义边界策略，不支持的函数直接抛 `CairoException`。

**整体复杂度**：`SAMPLE BY 5m FILL(NULL)` 查一个月 = O(N) 分桶聚合 + O(G) 补桶，其中 G ≈ 时间域 / 5min ≈ 8640。所以 bucket 宽度是补桶成本的主导项——想省这块开销应该加大 bucket，而不是关掉 FILL。

## 二十二、参考资料

1. QuestDB 官方文档：<https://questdb.io/docs/>
2. QuestDB GitHub：<https://github.com/questdb/questdb>
3. QuestDB 博客：<https://questdb.io/blog/>
4. ASOF JOIN 设计：<https://questdb.io/blog/asof-join-questdb/>
5. Materialized View 文档：<https://questdb.io/docs/guides/mat-views/>
6. Roaring Bitmaps 论文：<https://roaringbitmap.org/>
7. Gorilla 论文：<https://www.vldb.org/2015/papers/p1811-pelkonen.pdf>
8. QuestDB vs InfluxDB 对比：<https://questdb.io/blog/influxdb-vs-questdb/>
9. QuestDB vs TimescaleDB 对比：<https://questdb.io/blog/timescaledb-vs-questdb/>
10. QuestDB Summit 视频：<https://www.youtube.com/c/QuestDB>