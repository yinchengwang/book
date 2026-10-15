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

## 十九、参考资料

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