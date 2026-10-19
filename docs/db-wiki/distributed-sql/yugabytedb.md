# YugabyteDB Wiki（分布式 SQL / NewSQL / 双 API）

> 参考版本：YugabyteDB 仓库当前 commit `49ee38185838a754728efb0ec937b5135dbc7d04`（v2024.x 系列）；主要 API 与数据布局在 v2.18 后稳定。

## 一、项目定位

YugabyteDB（Yugabyte / YB）是**双 API 分布式数据库**：同时提供 **PostgreSQL 兼容的 YSQL** 和 **Cassandra 兼容的 YCQL** 两条 SQL/CQL 入口，内核共用一个分布式存储 + 共识层 DocDB（文档导向 KV + Raft）。其目标是 "**一个集群，同时支撑 OLTP 关系型事务型工作负载与 Cassandra-style 时序 / IoT 工作负载**"。

- **痛点**：
  - 已有 SQL + NoSQL 双轨数据库（Postgres + Cassandra）需要维护两套集群、两份数据，两套 ETL 同步；
  - NewSQL 项目要么只支持 PG（CockroachDB）、要么只支持 MySQL（TiDB），无法承载既有的 Cassandra 数据；
  - 全球分布式部署、强一致、水平扩展，单一开源产品难以同时覆盖 SQL 与 CQL 两套生态；
  - 用户希望在不重写应用的前提下，把 Mongo / Cassandra 业务迁移到具备 RDBMS + Linearizability 的现代分布式存储。
- **典型用户**：互联网公司、电信运营商、SaaS、IoT 平台、需要"全球边缘写 + 中心读"的零售与广告平台。代表客户：Hyundai、Tokopedia、ContentSquare、Ameren、Napster、RioT（Roblox 数据团队）等。

## 二、发展历史与社区

- **起源**：2016 年成立于加州 Palo Alto，由 Karthik Ranganathan（曾任 Facebook Cassandra 团队负责人）、Sundog Ash Ranganathan、Mikhail Bautin 创立；公司名"Yugabyte"取自"Sanskrit/梵语 yugas（纪元）"——预示"新的数据库时代"。
- **里程碑**：
  - 2017 年 7 月开源（Apache 2.0，原生仅 YCQL — Cassandra）；
  - 2018 年 5 月发布 YSQL beta（PostgreSQL 兼容）；
  - 2019 年 v1.3（生产可用 YSQL）；
  - 2020 年 v2.0（架构成熟、多区域强化）；
  - 2021 年 v2.6（加密 at-rest、YBC CLI）；
  - 2022 年 v2.16（pg700、PG 14 支持、Geo-partitioning GA）；
  - 2023 年 v2.18+（升级集成、Sweeper、follower reads 增强）；
  - 2024 年 2024.1/v2.20（基于 PG 15、索引增强、xCluster 增强）。
- **维护状态**：活跃；2024 年起每年 2 个大版本（春季/秋季 LTS），每个版本支持 ~7 个月（active）+ 12 个月（security 修复）。
- **社区**：YugabyteDB Slack 频道（10k+ 成员）、DBM 提供商业论坛、GitHub Discussions、Forum。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（与 TiDB 一致，最宽松的 OSS 许可）。
  - 来源：`reference/distributed-sql/yugabytedb/LICENSE`
  - 关键条款：自由商用、修改、分发、闭源衍生。
- **商业版（YugabyteDB Anywhere / Managed）**：
  - **YugabyteDB Enterprise**：核心 OSS 不包含的功能（备份加密、配置对比、xCluster DR、扩展监控）由 Yugabyte 单独维护的 enterprise package 提供；
  - **YugabyteDB Anywhere (YBA)**：自托管运维 UI / 控制台（`managed/` 目录，Scala/Java + Go API server）；
  - **YugabyteDB Managed**：Yugabyte 自营全托管 DBaaS；
  - **YugabyteDB Voyager**：从 Oracle / MySQL / PostgreSQL / Cassandra 评估与迁移工具（含评估报告生成）。

## 四、业界主要使用方与案例

- **零售 / 电商**：Tokopedia（印尼电商，订单 + 用户画像）；
- **汽车 / 出行**：Hyundai Autoever / Kia（车联网、车队管理）；
- **媒体 / 娱乐**：Napster、Mubi、Roblox、ContentSquare；
- **通讯 / SaaS**：Truphone、Twifi、Connexin（IoT）；
- **能源 / 公共事业**：Ameren、Eversource、PNNL；
- **金融 / 区块链**：Galaxy Digital、CoinCircle、CoinDCX、Bitbns；
- **电信**：Claro、NTT Data、Oi、Reliance Jio；
- **政府 / 教育**：HBCSE / 多家印度教育 SaaS。

## 五、一句话对比定位

> YugabyteDB 是**"PostgreSQL + Cassandra 双 API + 自研 DocDB + Raft 多区"**的 NewSQL 代表；与 **CockroachDB（只 PG 兼容、Raft 内嵌、Pebble 存储）**、**TiDB（MySQL 兼容、计算存储分离、RocksDB + Percolator）**、**OceanBase（阿里自研、Paxos、列存优化）** 一起构成 NewSQL "四强"，但 YB 是唯一同时兼容 SQL 与 NoSQL 生态的开源代表。

## 六、整体架构

YugabyteDB 的内核是 **DocDB**（分布式强一致 KV 文档存储），上层叠加两个 SQL 解析层：**YSQL（PG fork）** 与 **YCQL（Cassandra fork）**。

### 6.1 总体分层

```
┌────────────────────────────────────────────────────────────────────────┐
│       客户端 (psql / ysqlsh / cassandra-driver / JDBC / psycopg2)     │
└───────────┬──────────────────────────┬─────────────────────────────────┘
            │ YSQL (PG wire)           │ YCQL (Cassandra native protocol)
┌───────────▼──────────────────────────▼─────────────────────────────────┐
│                           YSQL / YCQL 层                              │
│   ┌──────────────────────────┐    ┌─────────────────────────────────┐│
│   │  YSQL: PG fork            │    │  YCQL: Cassandra fork           ││
│   │ src/postgres/             │    │ src/yb/yql/cql/                 ││
│   │ - Parser                 │    │ - CQL parser                    ││
│   │ - Optimizer              │    │ - Query plan + Cassandra ops    ││
│   │ - Executor               │    │                                 ││
│   └──────────────────────────┘    └─────────────────────────────────┘│
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ 内部调用 (ybcc / pg yb extension / DocDB API)
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    DocDB (文档模型分布式 KV)                            │
│   src/yb/docdb/                                                       │
│   - DocKey (hash[0..2] + [range keys] + doc hybrid time + user key)  │
│   - DocHybridTime (microsecond + logical) — 单调                   │
│   - SubDocument 编码（SingleDoc / MultiDoc）                        │
│   - Schema、PackedRow v2、TTL                                          │
│   - RocksDB Wrapper (src/yb/rocksdb/)                                  │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ RocksDB API
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    Raft + Tablet 层 (src/yb/consensus/, tablet/)      │
│   - Tablet = DocDB 的"分片"，Raft group                              │
│   - 每个 Tablet 在 2f+1 副本上 Raft 复制                            │
│   - Auto-split / merge / load balance                                 │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ master RPC
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    Master (src/yb/master/)                            │
│   - yb-master：高可用 Raft + 集群元数据 + Catalog                    │
│   - 系统表：ysql.catalog、system.peers_v2、system.tablets            │
│   - 调度：Placement、LoadBalance、Tablet split                       │
│   - 权限、备份管理、CDC（master 端 CDC service）                       │
│   - xCluster 跨集群复制                                                │
└────────────────────────────────────────────────────────────────────────┘
```

源码入口：
- TServer 启动：`src/yb/tserver/tablet_server_main.cc:1`
- Master 入口：`src/yb/master/master_main.cc:1`
- DocDB API：`src/yb/docdb/docdb.h:1`

### 6.2 TServer / Master 进程模型

- **`yb-tserver` 进程**：每节点一个，运行 TabletServer，处理客户端读写、DocDB 操作、Raft 参与者；
- **`yb-master` 进程**：每节点一个（默认 RF=3 的 Master quorum），运行 Master 服务，管集群元数据与系统表；
- **`yb-postgres` 进程**：仅 YSQL 路径上有，run PG 后端进程，与 TServer 协程协作；
- **`yb-controldb`**：与 PG 共享的服务，统一 `/master/sys_catalog` 风格表存；
- **`yugabyted`**（CLI）：单节点开发体验用，封装 master+tserver+pg。

### 6.3 一条 YSQL 的端到端路径

```
client (psql) → 
  YSQL handler (PG fork) → 
  Optimizer → 
  DocExpr → 
  RPC to TServer (yb_client) → 
  DocDB Read/Write (RocksDB + Raft) → 
  回 YSQL 编码为 PG 结果集
```

源码入口：`src/postgres/src/backend/executor/ybcModifyTable.c`、`src/postgres/src/backend/optimizer/ybcplan.c`。

### 6.4 一条 YCQL 路径

```
client (cassandra-driver) → 
  CQL parser → 
  QL Processor → 
  DocExpr → 
  RPC to TServer → 
  DocDB → back
```

源码入口：`src/yb/yql/cql/ql_processor.cc`、`src/yb/yql/cql/ql_statement.cc`。

## 七、存储引擎

### 7.1 DocDB

DocDB（`src/yb/docdb/`）是 YB 自研的**文档导向多版本分布式 KV 存储**：

- **DocKey = hash component + range components + doc hybrid time + user key**
  - `hash component` 把同表多行 sharding 到多个 Tablet；
  - `range components` 等价于 PostgreSQL 的 `PRIMARY KEY`（第二段）；
  - `doc hybrid time` 即版本戳；
  - `user key` 含列族与列名（如 collection/column1/path/column2）。
- **SubDocument**：单 key 内是树形 JSON-like 结构（支持 collection、子文档、单列）；
- **Schema-aware PackedRow v2** (v2.20+)：把同表的多列编码到一个 value 段，压缩与读放大更优；
- **Schema**：支持强类型 Schema 描述（与 PG/CQL 兼容）。

源码：`src/yb/docdb/doc_key.cc`、`src/yb/docdb/subdocument.cc`、`src/yb/docdb/packed_row.h`。

### 7.2 底层 RocksDB Wrapper

DocDB 把 SubDocument 落到 RocksDB（自维护的内嵌版本，源码：`src/yb/rocksdb/`）：

- **LSM-Tree** leveled compaction（与 CockroachDB Pebble 同源思想）；
- **Block Cache**：column-aware cache；
- **Bloom filter**：table key 检测；
- **WAL**：与 Tablet Raft log 共享存储路径。

源码：`src/yb/rocksutil/rocksdb_encryption.cc`、`src/yb/rocksdb/db.h`。

### 7.3 Tablet（Raft Group）

Tablet 是**一段连续 hash/range key**的"分片"，等价于 CockroachDB 的 Range、TiDB 的 Region：

- 默认 Tablet 大小上限（10 GiB 可调）；
- 自动 split 阈值（200 GB by default）；
- 每个 Tablet 是一个 Raft group。

源码：`src/yb/tablet/tablet.h:1`、`src/yb/tablet/tablet_options.h`。

## 八、索引机制

YB 提供三种索引模式：

| 索引类型 | 实现 | 备注 |
|----------|------|------|
| **Primary Key** | hash + range 编码到 DocKey | 同时承担表数据 shard |
| **Secondary Index (RANGE)** | 倒排 DocKey + 主表 lookup | 即普通 B-tree 风格 |
| **Secondary Index (HASH)** | 哈希打散写 | 解决热点 |
| **Partial / Expression Index** | PG 13+ 子集 | `CREATE INDEX ... WHERE ...` |
| **Covering Index** (INCLUDE) | PG 兼容 | 把主表列塞入索引 |
| **Bitmap Index** | 实验 | 部分工作负载 |
| **Vector Index (HNSW)** | `src/yb/hnsw/` | v2.20 GA，含 ANN 查询 |

**Vector Index 关键点**：

```sql
CREATE INDEX ON items USING hnsw (embedding vector_cosine_ops);
SELECT id FROM items ORDER BY embedding <-> '[1,2,3]' LIMIT 10;
```

源码：`src/yb/hnsw/hnsw.cc`、`src/yb/yql/cql/ql_expr.cc`。

## 九、查询 / 计算 / 接口层

### 9.1 YSQL — PostgreSQL 协议兼容

- **Wire Protocol**：PostgreSQL 协议（libpq / JDBC 兼容）；
- **SQL 方言**：PostgreSQL 13+ 语义，定期升级到上游新版本（v2024.1 + PG15 子集）；
- **不支持**：触发器（部分）、Recursive CTE（改进中）、自建 PG extension 仅社区维护版可装；
- **优化器**：fork 自 PG，使用 PG 原生 planner + yb 成本模型；
- **执行**：PG 原生 Volcano 执行器 + YB `yb_*` 计算（DocExpr 下推）。

源码入口：`src/postgres/src/backend/executor/ybc_*.c`、`src/postgres/src/backend/optimizer/ybcplan.c`。

### 9.2 YCQL — Cassandra 协议兼容

- **Wire Protocol**：Cassandra Native Protocol v3/v4/v5；
- **CQL 方言**：Cassandra 3.x/4.x CQL 子集（SELECT/INSERT/UPDATE/DELETE/BATCH、TTL、User-Defined Types、集合）；
- **执行**：自定义 QL 层（`src/yb/yql/cql/`），支持 secondary index、materialized view、function、aggregate 等。

### 9.3 DistSQL / Push Down

两个 SQL 层都通过 **"下推到 DocDB"** 减少数据传输：

- filter（point/范围）；
- project（列裁剪）；
- aggregate（count、sum、avg 的预聚合 / partial）；
- join（YSQL 上的部分 nested-loop / hash join 已下推）；
- sort（topK via `LIMIT`）。

源码：`src/yb/tablet/operations/`，`src/yb/yql/cql/ql_select.cc`。

### 9.4 接口与生态

- **PSQL client**：psql、pgx、psycopg2、JDBC 全可用；
- **cassandra-driver**：Java/Python/Node.js 的 DataStax driver + CQL 兼容；
- **ORM**：SQLAlchemy、Sequelize、Hibernate、GORM、Knex — 需 MySQL/PG 方言适配；
- **GUI**：DBeaver / DataGrip / TablePlus，需注意 PG / CQL 不同 schema；
- **CDC**：Debezium YB connector、XBeta 自带 CDC SDK；
- **变更数据捕获**：通过 TServer + 内部 schema_change_stream。

## 十、事务与一致性

### 10.1 隔离级别

| 隔离级别 | YSQL 是否支持 | YCQL 是否支持 | 说明 |
|----------|---------------|---------------|------|
| READ UNCOMMITTED | 是（语义等同 RC） | 是 | - |
| READ COMMITTED | 是（v2.18+ 起实践一致） | - | - |
| REPEATABLE READ | 是（YSQL 默认） | 是 | snapshot |
| SERIALIZABLE | 实验 | 否 | 计划中 |

### 10.2 分布式事务模型：DocDB Hybrid Time + 2PC

YB 采用**"Hybrid Logical Clock + 读写冲突检测 + 2PC"**模型（与 CockroachDB 同源于 Spanner 思想，但自研实现）：

```
client → TServer
  ├── BeginTxn: pick hybrid_time (assign via TServer local clock)
  ├── Read  (snapshot read at start_hybrid_time)
  ├── Write (deferred):
  │     • 写到 primary replica 的 provisional key (含 txn_id)
  │     • Lock 标记在 provisional record
  └── Commit:
        1) 2PC Phase-1 (Prepare):
           • 在 distributed txn 中心岗或 leader 排布 tentative commit record
           • 通过 Raft consensus 在 cohort replicas 写 Intents
        2) 2PC Phase-2 (Commit):
           • Global TSO 单调 hybrid_time 决定最终时间戳
           • 异步把 provisional records 提升为正式 records
           • 失败：Intents TTL 后回滚
```

**DocHybridTime**（`src/yb/common/hybrid_time.h`）= `physical_us << 12 | logical`，单调递增。Tablet 内部用 Raft 严格保序；跨 Tablet 通过本地 HLC 单调性约束，可能存在 short overlap 但收敛。

源码：
- `src/yb/tablet/operations/operation_tracker.cc`
- `src/yb/client/transaction.cc`
- `src/yb/tablet/transaction_participant.cc`（2PC participant）
- `src/yb/docdb/transactional_doc.h`

### 10.3 Raft

- **Raft 实现**：自研 C++ 实现（`src/yb/consensus/`），与 etcd-raft 同源思路 + YB 适配；
- **Multi-Raft**：一个 TServer 内可承载上千个 Tablet，每个 Tablet 一个 Raft group；
- **配置变更**：add / remove / replace peer 走 joint consensus；
- **Pre-Vote**：避免被分区节点发起无效选举；
- **Leader Lease**：基于 `follower_unreachable_election_timeout`；
- **Snapshot / Truncate**：定期做 compaction + snapshot transfer。

源码：`src/yb/consensus/raft_consensus.cc`、`src/yb/consensus/log.cc`（已通过 contrib 调整）。

### 10.4 时间源：Tablet Server HLC + TimeSync

YB 不依赖 TrueTime（如 Spanner GPS+原子钟），而是 **Tablet Server 间 HLC + 高精度 NTP + 主动心跳校验**，保证 hybrid_time 单调性：

- TimeSync service（`src/yb/tserver/heartbeater.cc`、`src/yb/tserver/server_main.cc`）确保节点间时钟偏差有界；
- 提供 bounded staleness（一说"在 NTP 漂移窗口内的强一致"）；
- 不依赖外部原子时钟 / GPS 接收器，使部署门槛低。

源码：`src/yb/util/clock_service.cc`、`src/yb/util/atomic_int.h`。

## 十一、复制与高可用（深度扩展）

### 11.1 多副本：Tablet + Raft

每个 Tablet 默认 3 副本（`RF=3`），由 Master 决定 placement：

- 副本策略：`replica_placement` 通过 `create table ... tablet placement` + `placement_uuid` / `num_replicas` / `placement_blocks` 表达；
- 故障：自动添加副本（挂起副本补齐）/摘除 / 重新复制；
- 跨 AZ / Region：可通过 `partition_by_region` 设置。

### 11.2 多区域容灾（Multi-Region / DR）

YB 提供**两类**多区部署：

#### 11.2.1 同集群多区域（同步）

```
Region A: TServer (3) + Master (1)
Region B: TServer (3) + Master (1)
Region C: TServer (3) + Master (1)
所有 Tablet 默认同集群同步复制；
通过 `tablespace` / `geo_partitioning` 把每条数据就近 shard：
  CREATE TABLESPACE us_east_1 WITH (replica_placement='{num_replicas:3, placement_blocks:[{cloud:aws,region:us-east-1,min_num_replicas:3}]}');
  CREATE TABLE orders PARTITION BY LIST (...) TABLESPACE us_east_1;
```

- 读：可走 follower reads（在 TServer `read_from_followers=ENABLED` 时）；
- 写：所有 region 多数派 ack（默认同步）。

#### 11.2.2 跨集群多区域（异步 xCluster DR）

- 通过 **xCluster**（以前称 `xCluster Replication`）：
  - Master 端维护 schema 同步 + publication 状态；
  - TServer 端通过 CDC 流把数据发给对端集群；
- **场景**：跨数据中心灾备、读扩展（Active-Active multi-master 实验性）；
- **冲突解决**：按 LWW（last-writer-wins）或外部 coordination service。

源码：`src/yb/master/xcluster_*`。

### 11.3 自动恢复与自愈

- **Load Balancer**：Master 持续监控 Tablet 副本分布、节点负载、磁盘使用；
- **Tablet Split**：自动按 Tablet 大小 + 吞吐阈值 split；
- **Recovery / Snapshot Restore**：手动 snapshot + 异步复制；
- **WAL/Log GC**：通过 retention 时间 + Tablet state 清理。

源码：`src/yb/master/cluster_balance.cc`、`src/yb/server/snapshots.cc`。

## 十二、扩展性（深度扩展）

### 12.1 分片策略

YugabyteDB 支持多种分片方式：

| 策略 | 描述 | 适用 |
|------|------|------|
| **Hash Sharding** | DocKey `[hash][range]` | 默认，写均匀、范围扫效率低 |
| **Range Sharding** | 纯 range | 范围扫强，但写入可能热点 |
| **Tablespace / Geo-Partitioning** | 按 zone 强制 placement | 多区域就近写 |
| **Tablet Split** | 自动按大小 / QPS / 数据 | 应对大表 |

### 12.2 水平扩容流程

```
1. 新 TServer 节点：通过 yb-ctl scale add_node
2. yb-master 自动把超容 / 节点副本数不均衡的 Tablet 迁移
3. 在线 ALTER TABLE ... SET TABLESPACE / PLACEMENT 控制迁移节奏
```

### 12.3 在线 Schema Change

- **YSQL**：通过 `ALTER TABLE`，DDL 直接 fork PostgreSQL schema_change 路径（同 PG 12+ 行为）；
- **YCQL**：通过 `ALTER TABLE` + `BEGIN SCHEMA_CHANGE ...` 自研路径；
- 大 DDL 通过 background job 执行，对读写影响可控。

源码：`src/yb/master/catalog_manager_util.cc`。

### 12.4 副本策略与 Tablespace

YSQL 中通过 Tablespace 表达副本策略：

```sql
CREATE TABLESPACE us_west WITH (
  replica_placement = '{
    "num_replicas": 3,
    "placement_blocks": [
      {"cloud":"aws","region":"us-west-2","zone":"us-west-2a","min_num_replicas":1},
      {"cloud":"aws","region":"us-west-2","zone":"us-west-2b","min_num_replicas":1},
      {"cloud":"aws","region":"us-west-2","zone":"us-west-2c","min_num_replicas":1}
    ],
    "leader_preference": [{ "cloud":"aws","region":"us-west-2","zone":"us-west-2a"} ]
  }'
);
```

源码：`src/yb/master/ysql_tablespace_manager.cc`。

### 12.5 Storage Compute Separability

- TServer 默认 co-locate；
- 实验：YugabyteDB Anywhere 也允许把 Tablet Server 与 CQL/YSQL 网关分机器（custom topology）。

## 十三、性能与基准

### 13.1 OLTP Sysbench

- 单 TServer，sysbench oltp_write_only 12k TPS；
- 线性扩展到 30+ 节点；
- v2.20 后 secondary index 加速使 read-heavy 工作负载翻倍。

### 13.2 TPC-C

- 官方公布 v2.18 + PG fork 在 60 节点（3-region）达到 ~600 万 tpmC；
- 与 CockroachDB / TiDB 在同等级硬件可比。

### 13.3 YCSB 与 Cassandra 兼容

- **YCQL + DocDB**：跑 YCSB A-E 工作负载，与 Cassandra 3.x 性能持平但提供 linearizability；
- 写放大：基于 LSM-Tree 的合理放大（与 RocksDB 同级）。

### 13.4 延迟分布

- 同 Region 写入 P99：5-20 ms；
- 跨 Region 同步写入 P99：30-200 ms（受 Raft 跨 Region 影响）；
- follower reads（同 Region）P99：1-5 ms。

## 十四、运维与可观测性

### 14.1 YugabyteDB Anywhere (YBA)

- **UI 仪表盘**：集群拓扑、副本分布、表级 metrics；
- **备份管理**：scheduled full + incremental + 内置/PITR；
- **配置对比**：不同 universe 之间的 config diff；
- **Rolling upgrade**：蓝绿升级；
- **监控告警**：内置 Prometheus + Alertmanager + Grafana。

源码：`managed/` 目录（Scala/Java + Go API server + React UI）。

### 14.2 内置 YB-Master Web UI

访问 `http://<master>:7000`：

- **Tablets**：副本分布、leader 分布；
- **Tables**：Tablet 数 / 大小；
- **Cluster**：节点健康；
- **Logs**：最近错误；
- **Performance**：rate per node。

### 14.3 备份与恢复

- **yb_backup**：内置 CLI，备份 DocDB SST 至 S3 / GCS / NFS；
- **dist snapshot copy**：通过 master snapshot 异步导出；
- **PITR**（v2.18+）：连续 WAL 上传 + restore；
- **XCluster DR**：跨集群异步复制。

源码：`src/yb/backup/backup_service.cc`、`src/yb/client/backup.h`。

### 14.4 CDC

YB 自带 CDC SDK（`java/yb-cdc/`），由 Debezium / 第三方 sink 消费：

- 一致性：同 Tablet 内严格有序，跨 Tablet 无全局顺序；
- 输出：JSON / Avro；
- Sink：Kafka / Pulsar / 文件。

源码：`java/yb-client/src/main/java/org/yb/cdc/cdc_service.java`、`src/yb/tablet/operations/operation_tracker.cc`。

### 14.5 Prometheus / Grafana

- 自带 `_status` Prometheus endpoint；
- 官方 Grafana Dashboard JSON 在 `docs/assets/patterns/grafana/`。

## 十五、生态与集成

### 15.1 客户端 SDK

| 语言 | YSQL | YCQL | 备注 |
|------|------|------|------|
| Go | `pgx` / `lib/pq` | `gocql` | - |
| Python | `psycopg2` / `psycopg3` | `cassandra-driver` | - |
| Java | `JDBC pgjdbc` | `datastax-java-driver` | - |
| Node.js | `pg` / `postgres` | `cassandra-driver` | - |
| Rust | `tokio-postgres` / `sqlx` | `scylla-rust-driver` | - |

### 15.2 数据迁移工具

- **YB Voyager**：评估 + 迁移（Oracle、MySQL、PG、Cassandra、MongoDB）；
- **yb-import**：bulk import via CSV / parquet（通过 CQL COPY FROM）；
- **cassandra-loader**：从 Cassandra SST 直接迁移到 YB；
- **mysqldump / pg_dump 兼容**：YSQL 部分兼容（DDL 大部分能用）。

### 15.3 Kubernetes Operator

`yugabyte/yugabyte-k8s-operator` 提供：

- 声明式 universe（CRD `YBUniverse`）；
- 自动故障转移、滚动升级；
- 内置监控 + backup。

### 15.4 周边生态

- **DBeaver / DataGrip / TablePlus** 均可连 YSQL；
- **Spark / Flink**：通过 JDBC connector；
- **Kafka Connect**：自带 Sink / Source connector；
- **DataStax Studio**：可直接操作 YCQL。

## 十六、核心论文与算法原理

- **Spanner 论文**（OSDI 2012）：YB 设计哲学承袭源头；
- **Raft 论文**（USENIX ATC 2014）：YB 自研 C++ Raft 实现；
- **Percolator 论文**（OSDI 2010）：YB 的 2PC 风格可与之对比（YB 走更显式 2PC）；
- **HLC 论文**：YB 的 HybridTime 是核心；
- **LSM-Tree 论文**（O'Neil 等，1996）：DocDB 的底层数据布局；
- **Cassandra 架构**（Lakshman & Malik 2009）：YCQL 兼容的源；
- **Aether 论文**（VLDV 2010）：YB LOG 设计的工程参考（见 `src/yb/consensus/README`）；
- **Viewstamped Replication**（Oki & Liskov）+ **VR Revisited**（Liskov 2012）：YB 设计灵感的对照参考。

## 十七、源码学习路径

### 17.1 推荐阅读顺序

1. **顶层 README + `architecture/`** （仓库根）；
2. **`src/yb/consensus/README`**（Raft + WAL 设计文档，最权威）；
3. **`src/yb/docdb/doc_key.cc` + `subdocument.cc`**（DocKey 与 SubDoc 编码）；
4. **`src/yb/tablet/tablet.cc`** + **`src/yb/tablet/operation_tracker.h`**（Tablet 操作跟踪）；
5. **`src/yb/consensus/raft_consensus.cc`**（Raft 实现）；
6. **`src/yb/master/catalog_manager.cc`**（Master 元数据）；
7. **`src/yb/ysql` + `src/postgres/src/backend/executor/ybc_*.c`**（YSQL PG fork）；
8. **`src/yb/yql/cql/ql_processor.cc`**（YCQL CQL layer）；
9. **`src/yb/client/client.cc`** + **`src/yb/client/transaction.cc`**（客户端事务）；

### 17.2 关键文件清单

- `src/yb/tserver/tablet_server_main.cc:1`
- `src/yb/master/master_main.cc:1`
- `src/yb/consensus/raft_consensus.cc:1`
- `src/yb/consensus/log.cc:1`
- `src/yb/tablet/tablet.h:1`
- `src/yb/tablet/transaction_participant.cc:1`
- `src/yb/tablet/operations/operation_tracker.cc:1`
- `src/yb/docdb/docdb.cc:1`
- `src/yb/docdb/doc_key.cc:1`
- `src/yb/docdb/subdocument.cc:1`
- `src/yb/docdb/packed_row.h:1`
- `src/yb/common/hybrid_time.h:1`
- `src/yb/master/catalog_manager.cc:1`
- `src/yb/master/cluster_balance.cc:1`
- `src/yb/master/ysql_tablespace_manager.cc:1`
- `src/yb/client/transaction.cc:1`
- `src/yb/yql/cql/ql_processor.cc:1`
- `src/postgres/src/backend/executor/ybcModifyTable.c:1`

### 17.3 关键测试文件

- `src/yb/integrationtests/`（大量集成测试）
- `src/yb/tablet/transaction_participant-test.cc`
- `src/yb/consensus/raft_consensus-test.cc`
- `src/yb/docdb/docdb-test.cc`
- `src/yb/client/client-test.cc`
- `src/yb/yql/cql/ql-select-test.cc`
- `java/yb-pgsql/src/test/`

## 十八、源码规模与开发语言

- **语言**：C++ 为主（`src/yb`）、C（PG fork `src/postgres`）、Go（yugabyted / yba-api-server / OTel collector）、React（YBA 与 yugabyted-ui 前端）、Scala（YBA 后端）、Python（构建工具）；
- **代码规模**：master 分支约 350 万行（含 PG fork + YB + Managed）；
- **构建系统**：CMake（`src/`）+ YB 自研 `yb_build.sh`；Java 仓用 Maven / Gradle；
- **依赖管理**：CMake `find_package`、fetch_content；Java 用 Maven；
- **格式化**：clang-format / gofmt / Prettier 分别覆盖；

## 十九、学习资源

- **官方文档**：`https://docs.yugabyte.com/`（中文站点 `docs.yugabyte.com/zh/`）；
- **Architecture Docs**：`https://docs.yugabyte.com/preview/architecture/`（含关键论文摘要）；
- **Yugabyte Blog**：`https://www.yugabyte.com/blog/`
- **YouTube Channel**：`YugabyteDB`
- **Slack**：`yugabyte-db.slack.com`
- **GitHub Discussions**：`yugabyte/yugabyte-db/discussions`
- **Conference Talks**：CMU Database Talk Series、PODS、VLDB、SIGMOD；
- **书籍**：无系统化"教科书"，但官方 docs "architecture" 是工程参考；
- **Online Course**：Yugabyte University（部分免费）；

## 二十、可移植性与部署

- **二进制**：`yb-tserver` / `yb-master` / `yb-postgres` / `yugabyted`，静态/动态链接；
- **平台**：Linux x86_64 / arm64、macOS（开发用），Windows 不支持；
- **Docker**：`yugabytedb/yugabyte:latest`、独立进程的镜像组合；
- **Kubernetes**：`yugabyte/yugabyte-k8s-operator`；
- **YBA**：自部署版可通过 tarball / OVA / AWS AMI 安装；
- **DBaaS**：YugabyteDB Managed（AWS / GCP / Azure）；
- **嵌入式**：否；

## 二十一、变体与衍生项目

- **YugabyteDB Anywhere (YBA)**：自部署控制台；
- **YugabyteDB Managed (YB Cloud)**：全托管；
- **YugabyteDB Voyager**：迁移 / 评估工具；
- **核心 fork**：YBA 与核心 OSS 是不同 repo（`yugabyte/yugabyte-db` vs `yugabyte/yugabyte-managed`），独立演进。
- **Open source vs Enterprise**：核心 OSS 几乎完整，Enterprise 主要在 YBA + 备份 PITR + 部分监控能力。

## 二十二、风险与陷阱

| 风险 | 缓解 |
|------|------|
| **PG fork 升级延迟**：YB 一般落后 PG 上游 1-2 个版本 | 关注每年 LTS 版本升级；不要指望最新 PG 特性立即可用 |
| **HybridTime 漂移**：节点时钟漂移过大影响 read consistency | 部署 NTP / Chrony，启用 `time_source` 监控 |
| **Tablet Leader 不均衡**：单机被选举为多 tablet leader | 调整 `load_balancer_max_replicas_to_add_per_server` |
| **大表 hash sharding 范围扫慢**：hash 分片不利于范围查询 | 关键范围查询走 secondary index 或改 range sharding |
| **YCQL 与 YSQL 集群混用复杂度**：共享底层但 SQL 方言不混 | 业务域清晰分 schema |
| **xCluster DR 主从延迟**：跨 Region 写入异步 | 接受 bounded RPO；监控 `xcluster_lag` metric |
| **Vec 索引 cost 估算**：HNSW 调参复杂 | 参考官方 tuning 章节（`docs.yugabyte.com/vector`） |
| **二次查询延迟**：CQL BATCH 应避免跨 partition | CQL 单分片 BATCH 才有意义 |
| **PG extension 不齐**：与 PG 生态存在 gap | 关注官方已认证 extension list（PG 15 子集） |
| **备份 PITR 配置复杂**：开启 PITR 后需要调整 retention | 监控 `pitr_queue_size`、合理配置 `pitr_retention` |

## 二十三、与本项目的关联

- **存储引擎**：`engineering/src/db/storage/` 中的 heap + btree；YB 的 DocDB 是"工业级 SubDocument + LSM-Tree + Raft"，可作 maxite vs learning 对比。
- **PostgreSQL fork 思路**：`engineering/src/sql/` 手写教学 SQL parser 与 YSQL 的 fork-PG 思路对比看"自顶向下 fork vs 自底向上手写"。
- **KV 与事务**：`engineering/src/db/tx.h`、`engineering/src/db/kv.h` 的极简实现对比 DocDB 的复杂 MVCC / 2PC。
- **共识协议**：`engineering/src/dist/` 已有 raft + 2PC 学习实现；与 `src/yb/consensus/raft_consensus.cc` 工业级对照。
- **学习轨**：`learning/notes/` 内已含 NewSQL 与多区容灾章节，可结合 YB 的具体 API 强化。

## 二十四、DocDB 自管编码：DocKey 二进制布局与 PackedRow V2 Schema 折叠

YB 把「schema 维度 + 行内子文档 + 时间戳 + 类型字节」全部压到一个 RocksDB key + 一个 RocksDB value。本节逐字段拆解 DocKey 的二进制布局、PackedRow V2 的 schema-version + flags 编码、ValueControlFields 与 ValueEntryTypeAsChar——这些是 DocDB 区别于「裸 RocksDB KV」的关键设计，也是 §7.1 概念图背后真正的字节排布。

### 24.1 DocKey 二进制布局（CotableId/ColocationId + Hash + Range + DocHybridTime + UserKey）

`src/yb/dockv/doc_key.h:95` 的 `class DocKey` 与 `src/yb/dockv/doc_key.cc:252 DocKey::AppendTo(KeyBytes* out)` 暴露了 5 段式二进制布局：

```
+-----------------+----------------+-------------+-----------+------------+
| CotableId/      | [HashCode      | Hashed      | Range     | DocHybrid  |  <UserKey
| ColocationId    | (4B varint)]  | group       | group     | Time       |  (subcolumn path)>
+-----------------+----------------+-------------+-----------+------------+
```

- `CotableId` (`Uuid`)：当表是「ColocationId」模式时换成 `ColocationId`（u32 + varint）——`DocKey::AppendTo` 在 `:254-260` 根据 `cotable_id_.IsNil()` 切换两条构造链；
- `HashCode`（4 字节 varint，可选）：基于 hashed group 中列计算的 32-bit 哈希，作为「同一 hash 槽位」的快速定位标记；`:408 DocKey::DecodeHash` 提供仅解码 hash 的入口；
- `Hashed group`：构成 PRIMARY KEY 中「hash 部分」的列序列（YCQL 全列主键即 hash，YSQL 主键中标记为 hash 的列即 hashed group）；
- `Range group`：构成 PRIMARY KEY 中「range 部分」的列序列，对应 PostgreSQL 风格的主键第二段；
- `DocHybridTime`：版本戳（物理微秒 + logical），单调递增（`src/yb/common/hybrid_time.h`、`src/yb/common/doc_hybrid_time.h`）；
- `UserKey`：子文档/列的 path 编码，每段以前缀 `ValueEntryTypeAsChar` 的类型字节打头（§24.5）。

源码：`reference/distributed-sql/yugabytedb/src/yb/dockv/doc_key.h:95-340`（`DocKey` / `DocKeyEncoder` / `DocKeyEncoderAfterHashStep` / `DocKeyEncoderAfterTableIdStep`），`reference/distributed-sql/yugabytedb/src/yb/dockv/doc_key.cc:252 DocKey::AppendTo`、`:408 DocKey::DecodeHash`。

### 24.2 DocKeyEncoder 链式 API

`src/yb/dockv/doc_key.h:339` 的 `class DocKeyEncoderAfterHashStep` 与 `:352` 的 `class DocKeyEncoderAfterTableIdStep` 实现了「先 CotableId → Hash → Range」的链式编码器，强制使用者按既定顺序生成字节流，避免漏段：

```c++
// src/yb/dockv/doc_key.h:339-370 真实链式 API
class DocKeyEncoder {
  DocKeyEncoderAfterTableIdStep CotableId(const Uuid& cotable_id);
  DocKeyEncoderAfterTableIdStep ColocationId(ColocationId colocation_id);
};
class DocKeyEncoderAfterTableIdStep {
  DocKeyEncoderAfterHashStep NoHash();
  DocKeyEncoderAfterHashStep Hash(bool hash_present,
                                   DocKeyHash hash,
                                   const KeyEntryValues& hashed_group);
};
class DocKeyEncoderAfterHashStep {
  // 末尾追加 Range group + DocHybridTime + UserKey
  DocKeyEncoder& Range(const KeyEntryValues& range_group);
};
```

调用侧（`doc_key.cc:252 DocKey::AppendTo`）：

```c++
void DocKey::AppendTo(KeyBytes* out) const {
  auto encoder = DocKeyEncoder(out);
  if (!cotable_id_.IsNil()) {
    encoder.CotableId(cotable_id_).Hash(hash_present_, hash_, hashed_group_).Range(range_group_);
  } else {
    encoder.ColocationId(colocation_id_)
           .Hash(hash_present_, hash_, hashed_group_)
           .Range(range_group_);
  }
}
```

编译期强约束编码形态（hash 必须在 range 之前，table id 必须在 hash 之前），源码层杜绝「半成品 key」。

### 24.3 PackedRow V2 编码：schema_version + flags + 列数据

§7.1 提到的「Schema-aware PackedRow v2」的真实字节布局来自 `src/yb/dockv/packed_row.h:148 RowPackerV1` / `:190 RowPackerV2`：

- **PackedRow V2 header**（`packed_row.h:234 PackedRowV2Header`）：
  ```
  +-----------------+----------+
  | schema_version  | flags    |
  | (varint)        | (uint8)  |
  +-----------------+----------+
  | <逐列 packed_value>...    |
  +----------------------------+
  ```
- `flags` 由两个位组成（`packed_row.h:197-199`）：
  - `kHasNullsFlag = 0x1`：行内有 NULL 列；
  - `kIsUpdateFlag = 0x2`：来源是 UPDATE/UPSERT（非 INSERT）。
- 列数据采用「按 schema 列顺序」连续编码，每列长度前缀 varint + 自身类型字节 + 序列化值（packed_row.h:190-227 RowPackerV2::AddValue 多次重载）；
- 行若「无 NULL」，不写 null mask 段，进一步压缩。

V1 vs V2 对比（packed_row.h:151 / :192）：

| 维度 | V1 (`kPackedRowV1`, 'z') | V2 (`kPackedRowV2`, '\|') |
|------|----------------------------|-----------------------------|
| schema_version 字段 | varint | varint |
| flags 字段 | 无 | `kHasNullsFlag` + `kIsUpdateFlag` |
| NULL 处理 | 每列自带 null 标记 | 行级 flags，NULL 列只在 flags 标记时存在 |
| 内部版本号 | 老表无前缀 | 普遍 |

源码：
- `reference/distributed-sql/yugabytedb/src/yb/dockv/packed_row.h:148 RowPackerV1`
- `reference/distributed-sql/yugabytedb/src/yb/dockv/packed_row.h:190 RowPackerV2`
- `reference/distributed-sql/yugabytedb/src/yb/dockv/packed_row.h:234 PackedRowV2Header`
- `reference/distributed-sql/yugabytedb/src/yb/dockv/schema_packing.h:77 SchemaPacking`、`:151 SchemaPackingRegistry`、`:186 SchemaPackingStorage`
- `reference/distributed-sql/yugabytedb/src/yb/dockv/schema_packing.cc:499 SchemaPacking(TableType, const Schema&)`、`:538 SchemaPacking(const SchemaPackingPB&)`

`SchemaPacking` 是 PackedRow 的反向工程器：把「表 schema + schema_version」映射成「列在 packed 字节流中的偏移表」，提供 `column_packing_data(idx)`、`GetValue(idx, packed)`、`GetBounds`（用于 range 范围裁剪）等接口；同表多 schema_version 时通过 `SchemaVersionMapper` 协调多版本读（`schema_packing.h:75`、`schema_packing.cc:904 InsertSchemas`）。

### 24.4 ValueControlFields（TTL + user timestamp + merge flags）

每个 value 的控制字段来自 `src/yb/dockv/value.h:27 struct ValueControlFields`：

```c++
// src/yb/dockv/value.h:27-63 真实结构
struct ValueControlFields {
  static const MonoDelta kMaxTtl;
  static const MonoDelta kResetTtl;   // CQL 用，0 TTL 含义
  static const int64_t kInvalidTimestamp;
  static const uint64_t kTtlFlag = 0x1;

  uint64_t merge_flags = 0;            // 0x1 = TTL-only / 0x3 = Value-only
  MonoDelta ttl = kMaxTtl;             // 毫秒；==kMaxTtl 时不写入字节流
  UserTimeMicros timestamp = kInvalidTimestamp;  // 显式用户时间戳

  void AppendEncoded(ValueBuffer* out) const;
  void AppendEncoded(std::string* out) const;
  std::string ToString() const;
  static Result<ValueControlFields> Decode(Slice* slice);
  static Result<ValueControlFields> DecodeWithIntentDocHt(Slice* slice, Slice* intent_doc_ht);

  bool has_ttl() const { return !ttl.Equals(kMaxTtl); }
  bool has_timestamp() const { return timestamp != kInvalidTimestamp; }
};
```

要点：
- TTL 仅在「非 kMaxTtl」时编码进字节流（`has_ttl()` 返回 false 即不写 TTL 段）——避免每个 value 都背负 8 字节 TTL 开销；
- `merge_flags` 用 bit flag 表达 TTL-only（`0x1`）或 value-only（`0x3`）合并记录（用于「先写 TTL，后异步追加 value」的合并路径）；
- `kResetTtl` 是 CQL 语义下「0 TTL = 无 TTL」特例的别名；
- `DecodeWithIntentDocHt` 额外吐出 intent doc hybrid time，给 transactional apply 路径用。

### 24.5 ValueEntryTypeAsChar 类型字节

`src/yb/dockv/value_type.h:161 DOCDB_VALUE_ENTRY_TYPES` 用单字节 ASCII 字符表达「value 类型」+「key 字段类型」+「编码控制字」。下表节选与 packed_row 紧密相关的字节（`value_type.h:208-214`）：

| 字节 (ASCII) | 枚举名 | 用途 |
|--------------|--------|------|
| `'z'` (122) | `kPackedRowV1` | PackedRow V1 编码值（packed_row.h:151） |
| `'\|'` (124) | `kPackedRowV2` | PackedRow V2 编码值（packed_row.h:214） |
| `'w'` (119) | `kWriteId` | 同一事务内 write 序号（`DecodeWithIntentDocHt` 路径） |
| `'x'` (120) | `kTransactionId` | intent record 的 transaction id |
| `'y'` (121) | `kTableId` | DocKey 中的 table id |
| `'l'` (108) | `kRowLock` | 行锁 intent |
| `'t'` (116) | `kTtl` | TTL 字段前缀（ValueControlFields 编码） |
| `'u'` (117) | `kUserTimestamp` | 用户时间戳字段前缀 |
| `'X'` (88)  | `kTombstone` | 删除标记（DocDB 列级软删除） |
| `'V'` (86)  | `kVectorId` | 向量索引 leaf id（hnsw） |
| `'R'` (82)  | `kVector` | HNSW 邻接向量 |

类型字节的 ASCII 顺序经过设计——`kNullLow '$'` (36) 是最低、其它 primitive 升序排列，使得「按 DocKey 前缀迭代」可直接用 memcmp 而无需 schema 介入。

源码：`reference/distributed-sql/yugabytedb/src/yb/dockv/value_type.h:108-219 DOCDB_VALUE_ENTRY_TYPES`（含完整 key/字节编码表）。

### 24.6 复杂度与本项目对照

| 操作 | YB DocDB | 本项目 `engineering/src/db/storage/` |
|------|----------|--------------------------------------|
| DocKey 编码 | O(N)，N = PRIMARY KEY 列数 | 朴素 row id = file offset |
| PackedRow V2 pack | O(C) 列数，每列 O(1) AddValue | 不存在 |
| ValueControlFields 编码 | O(1)（TTL 不写时）或 O(1) 写 TTL 段 | 不存在 |
| DocKey 解码（PartiallyDecode） | O(1)（仅 hash）/ O(N) 完整 | 不存在 |
| Row lookup | RocksDB Get + PackedRow 解码（O(C)） | bufmgr get page + tuple scan |

复杂度分析（DocKey encode/decode）：
- **DocKey::AppendTo**：写入 N 个 hashed group + M 个 range group + DocHybridTime 共 O(N + M) 字节；
- **DocKey::DecodeHash**：`O(1)`，仅消费 hash code 4-byte varint；
- **PackedRow V2 解码**：先读 header `O(1)`，再按 column_packing_data 顺序 `O(C)` 取出每列。

本项目对应：`engineering/src/db/storage/btreeam.c` 用朴素 `file_offset + slot_offset` 作为 row id，无 DocKey 多段编码、无 PackedRow，schema 增列需 migration。DocDB 的 schema-version 折叠 + 多版本读为学习轨「行存 vs 列存 schema 演进」提供工程参考。

---

## 二十五、Raft 自维护：UpdateReplica 6 步流水线与 OperationTracker 内存门控

YB Raft（`src/yb/consensus/raft_consensus.cc`）与 etcd-raft 的关键差异在于「follower 侧的 apply 流水线」：follower 接收到 leader 的 batch 后不是直接写 RocksDB，而是先 enqueue prepare、再 enqueue WAL、再 commit、再异步 apply，每一步都可在 failure injection 下暂停测试。本节拆解这 6 步流水线，以及 OperationTracker / VoteCounter / LeaderLease 这三件配套机制。

### 25.1 UpdateReplica 6 步流水线（`RaftConsensus::UpdateReplica`）

`src/yb/consensus/raft_consensus.cc:1966 Result<UpdateReplicaResult> RaftConsensus::UpdateReplica` 是 follower 侧的处理入口。源码注释 `:1978-2057` 显式列出 6 步（编号 0–5）：

```c++
// src/yb/consensus/raft_consensus.cc:1966-2166 真实 6 步骨架
Result<RaftConsensus::UpdateReplicaResult> RaftConsensus::UpdateReplica(
    const std::shared_ptr<LWConsensusRequestPB>& request_ptr,
    LWConsensusResponsePB* response) {

  const auto& request = *request_ptr;
  if (request.has_propagated_hybrid_time()) {              // 传播 HLC
    clock_->Update(HybridTime(request.propagated_hybrid_time()));
  }
  ReplicaState::UniqueLock lock;
  RETURN_NOT_OK(state_->LockForUpdate(&lock));              // 加锁
  RETURN_NOT_OK(CheckLeaderRequestUnlocked(...));           // term 检查 + leader 校验

  SnoozeFailureDetector(DO_NOT_LOG);                       // 续 lease
  withhold_votes_until_.store(now + MinimumElectionTimeout());  // 拒投
  RETURN_NOT_OK(EarlyCommitUnlocked(request, deduped_req)); // (1) Early commit pending ops
  if (!VERIFY_RESULT(EnqueuePreparesUnlocked(...))) { return UpdateReplicaResult(); }  // (2) enqueue prepares
  RETURN_NOT_OK(EnqueueWritesUnlocked(...));               // (3) enqueue WAL writes
  RETURN_NOT_OK(MarkOperationsAsCommitted(...));            // (4) mark committed
  RETURN_NOT_OK(state_->WaitForWritesDurable(...));         // (5) wait for fsync
  return UpdateReplicaResult();                              // (6) respond to leader
}
```

6 步语义：

1. **0-Dedup**：去重已收到 op-id，幂等（`:1991-1994` 注释）；
2. **1-EarlyCommit**：把「leader 已 commit、自己尚未 apply」的旧 op 提前 apply，释放 MRS 内存压力（`:2114 EarlyCommitUnlocked`）；
3. **2-EnqueuePrepares**：把 op 提交到 consensus queue，由 OperationDriver 解码 + DocDB 写锁；
4. **3-EnqueueWrites**：prepare 成功的 op 才允许 enqueue 到 WAL；若 WAL 写失败则整 replica crash（保持「prepare 成功必 log 成功」不变量）；
5. **4-MarkCommitted**：把 commit 标记写到 OperationState，触发 Apply 异步执行；
6. **5-WaitForDurable**：在 respond 给 leader 前等 WAL fsync；6-Respond 把「已 replicated watermark」写回。

源码：`reference/distributed-sql/yugabytedb/src/yb/consensus/raft_consensus.cc:1966 RaftConsensus::UpdateReplica`、`:2114 EarlyCommitUnlocked`、`:2117 EnqueuePreparesUnlocked`、`:2125 EnqueueWritesUnlocked`、`:2166 TRACE("UpdateReplica() finished")`。

### 25.2 OperationTracker 内存门控（Add/Release）

`src/yb/tablet/operations/operation_tracker.h:59 class OperationTracker` 跟踪每个 TabletPeer 上「prepare 完成但 apply 未完成」的 in-flight operation。其 `Add`（`:68`）的关键实现：

```c++
// src/yb/tablet/operations/operation_tracker.cc:160 Add 真实实现
Status OperationTracker::Add(OperationDriver* driver) {
  int64_t driver_mem_footprint = driver->SpaceUsed();
  MemTracker* blocking_mem_tracker = nullptr;
  if (mem_tracker_ && !mem_tracker_->TryConsume(driver_mem_footprint, &blocking_mem_tracker)) {
    if (metrics_) { metrics_->operation_memory_pressure_rejections->Increment(); }
    auto* operation = driver->operation();
    TabletPtr tablet = operation ? operation->tablet_nullable() : nullptr;
    auto consumption = blocking_mem_tracker->consumption();
    auto limit = blocking_mem_tracker->limit();
    string msg = Substitute("Operation of type $0 failed: tablet $1 hit the limit $3 ...",
        AsString(driver->operation_type()), tablet ? tablet->tablet_id() : "(unknown)",
        consumption, limit, AsString(blocking_mem_tracker), driver_mem_footprint);
    YB_LOG_EVERY_N_SECS(WARNING, 1) << msg;
    return STATUS(ServiceUnavailable, msg);                    // 拒绝 + ServiceUnavailable
  }
  IncrementCounters(*driver);
  State st;
  st.memory_footprint = driver_mem_footprint;
  std::lock_guard lock(mutex_);
  CHECK(pending_operations_.emplace(driver, st).second);       // pending_operations_ 是 hashmap<driver*, State>
  return Status::OK();
}
```

要点：
- **MemTracker 门控**：每 op 占用 `driver_mem_footprint = driver->SpaceUsed()`，超 limit 时返回 `STATUS(ServiceUnavailable, ...)` 拒绝入队——把 follower 的内存压力挡在 prepare 阶段（不污染 RocksDB / WAL）；
- **`pending_operations_` 是 hashmap<OperationDriver*, State>**（`operation_tracker.h:127`）：通过 `Release(driver, applied_op_ids)` 移除（`:225`），同时把累计 applied op-id 返回给上层推进 watermark；
- `WaitForAllToFinish(timeout)`（`:285`）：副本 shutdown / failover 时阻塞等待，规避「副本消失但 op 还在飞」的不一致。

源码：`reference/distributed-sql/yugabytedb/src/yb/tablet/operations/operation_tracker.h:59 class OperationTracker`、`:68 Status Add(...)`、`:72 void Release(...)`、`:127 pending_operations_ hashmap`；`reference/distributed-sql/yugabytedb/src/yb/tablet/operations/operation_tracker.cc:160 OperationTracker::Add`、`:213 DecrementCounters`、`:225 Release`、`:285 WaitForAllToFinish`。

### 25.3 VoteCounter 与 LeaderElection

`src/yb/consensus/leader_election.h:69 class VoteCounter` 是选举票收集器：

```c++
// src/yb/consensus/leader_election.h:69-117 真实 API
class VoteCounter {
 public:
  VoteCounter(size_t num_voters, size_t majority_size);
  Status RegisterVote(const std::string& voter_uuid, ElectionVote vote, bool* is_duplicate);
  // If vote not yet decided, returns ElectionVote::kUnknown.
  ElectionVote GetDecision() const;
  size_t GetTotalVotesCounted() const;
  bool AreAllVotesIn() const;

 private:
  bool IsDecided() const;       // decision != ElectionVote::kUnknown
  ElectionVote decision = ElectionVote::kUnknown;
  typedef std::map<std::string, ElectionVote> VoteMap;
  VoteMap votes_;                // voter_uuid -> vote (kGranted/kDenied)
  size_t majority_size_;
};
```

关键设计（`YB_DEFINE_ENUM(ElectionVote, (kDenied)(kGranted)(kUnknown))` 在 `:65`）：
- **3 态决策**：`kDenied` / `kGranted` / `kUnknown`，`GetDecision()` 一直返回 `kUnknown` 直到「kGranted 数达到 majority_size」或「kDenied 数超过 num_voters - majority_size」；
- **去重**：`RegisterVote` 返回 `is_duplicate`，同一 voter 的二次投票不增加票数；
- **Pre-Vote 阶段**：`LeaderElection::RunElection`（`leader_election.cc:147`）用单独 VoteCounter 跑 pre-vote，通过后才提 term 跑正式 vote（避免被网络分区节点反复提 term）。

源码：`reference/distributed-sql/yugabytedb/src/yb/consensus/leader_election.h:65 ElectionVote 3 态枚举`、`:69 VoteCounter`、`:209 RecordVoteUnlocked`；`reference/distributed-sql/yugabytedb/src/yb/consensus/leader_election.cc:147 RunElection`、`:225 VoteResponseRpcCallback`、`:312 RecordVoteUnlocked`。

### 25.4 LeaderLease（leader_lease_duration_ms）

`src/yb/consensus/raft_consensus.h:86` 定义 `kDefaultLeaderLeaseDurationMs = 2000`（2 秒）。`FLAGS_leader_lease_duration_ms` 在 `raft_consensus.cc:217 DEFINE_NON_RUNTIME_int32` 注册，`:223 DEFINE_validator` 用 `FLAG_DELAYED_COND_VALIDATOR` 强制「leader_lease 严格大于 raft_heartbeat_interval」：

```c++
// src/yb/consensus/raft_consensus.cc:217-226 真实注册
DEFINE_NON_RUNTIME_int32(leader_lease_duration_ms, yb::consensus::kDefaultLeaderLeaseDurationMs,
             "Leader lease duration. A leader keeps establishing a new lease or extending the "
             "existing one with every UpdateConsensus. A new server is not allowed to serve as a "
             "leader (i.e. serve up-to-date read requests or acknowledge write requests) until a "
             "lease of this duration has definitely expired on the old leader's side.");
DEFINE_validator(leader_lease_duration_ms,
    FLAG_DELAYED_COND_VALIDATOR(
        FLAGS_raft_heartbeat_interval_ms < _value,
        yb::Format("Must be strictly greater than raft_heartbeat_interval_ms: $0",
            FLAGS_raft_heartbeat_interval_ms)));
```

- 配对的 `raft_heartbeat_interval_ms` 也有反向 validator（`:229-233`）：`_value < FLAGS_leader_lease_duration_ms` 强制「heartbeat 严格小于 leader_lease」；
- `FLAG_DELAYED_COND_VALIDATOR` 是「延迟校验」：两个 flag 注册顺序可能不固定，先注册的先定义；启动时如果发现 `leader_lease <= heartbeat` 不报错，到 validator 触发时才报错（避免 init 期顺序敏感）；
- 真实约束意图：每次 heartbeat 后 leader 的 lease 还有 ≥ heartbeat 间隔的余量，保证新 leader 选举前旧 leader 一定先 lease expire。


`UpdateReplica` 收到 leader 请求时（`:2097 state_->UpdateOldLeaderLeaseExpirationOnNonLeaderUnlocked`）：
- `CoarseTimeLeaseUpdate.holder_uuid = deduped_req.leader_uuid`；
- `CoarseTimeLeaseUpdate.expiration = CoarseMonoClock::now() + leader_lease_duration_ms * 1ms`；
- `PhysicalComponentLeaseUpdate.expiration = request.ht_lease_expiration()`（来自 leader 的 HLC lease）。

双 lease 模型：
- **CoarseTime lease**（粗粒度 wall-clock）：用于 reject 旧 leader 写入；
- **PhysicalComponent lease**（HLC 维度）：用于 read 路径的 followable read 判定。

`LeaderLeaseStatus`（raft_consensus.cc:1307-1314）3 态枚举 `OLD_LEADER_MAY_HAVE_LEASE / LEASE_HAS_EXPIRED / LEADER_CURRENT`，决定是否能投新 candidate 的票。

源码：`reference/distributed-sql/yugabytedb/src/yb/consensus/raft_consensus.h:86 kDefaultLeaderLeaseDurationMs`；`reference/distributed-sql/yugabytedb/src/yb/consensus/raft_consensus.cc:217 DEFINE_NON_RUNTIME_int32(leader_lease_duration_ms)`、`:2097 UpdateOldLeaderLeaseExpirationOnNonLeaderUnlocked`、`:1307 LeaderLeaseStatus 3 态`。

### 25.5 复杂度与本项目对照

| 操作 | YB Raft | 本项目 `engineering/src/dist/raft.c` |
|------|---------|--------------------------------------|
| UpdateReplica 单 batch | 6 步流水线 O(N) op 数 + MemTracker TryConsume O(log M) | 朴素 apply，无内存门控 |
| VoteCounter.RegisterVote | O(log V) Voter 哈希 + 票数累加（V = voter 数，V≤5） | 朴素多数派循环 |
| OperationTracker Add/Release | O(1) hashmap 插入/删除 + TryConsume 限流 | 无 |
| LeaderLease 续约 | O(1) wall-clock + HLC 比较 | 无（朴素的 election timeout） |

复杂度分析（典型 batch）：
- **batch 大小 N**（默认 Raft max batch 1 MB / 数 K ops）：
  - 0-Dedup：O(N) 按 op-id hashmap 查找；
  - 1-EarlyCommit：O(K) K = 已 pending 数；
  - 2-EnqueuePrepares：O(N) prepare 提交到 queue；
  - 3-EnqueueWrites：O(N) WAL enqueue；
  - 4-MarkCommitted：O(N) OperationState 更新；
  - 5-WaitForDurable：阻塞到 WAL fsync（fsync 时间主导）；
- **总**：单 batch 处理 O(N + K)；batch 内 op 与 pending 操作量线性。

本项目对应：`engineering/src/dist/raft.c` 实现了基本 election/append，但缺少「UpdateReplica 6 步 + MemTracker 门控 + 双 lease 模型」这些生产级机制，是从学习到生产的关键差距。

---

## 二十六、xCluster 跨集群复制：CDC 流控与 producer/consumer 主端生命周期

YB 的 xCluster 用「同集群同步 + 跨集群异步」两层实现 DR：同集群内仍是 Raft 同步；跨集群（DR/异地多活/读扩展）通过 CDC 流把 source cluster 的 WAL 推到 target cluster，由 target 端的 WAL applier 重放。本节聚焦「CDC GetChanges 流控 + producer/consumer 生命周期 + tablet split 透传」，补齐 §11.2.2 / §14.4 仅有的 high-level 描述。

### 26.1 xCluster 拓扑与主端元数据

xCluster 由 master 端 + tserver 端两层组成：

- **Master 端**（`src/yb/master/xcluster/`）：维护 source/target cluster 之间的 `XClusterReplicationStatusPB`（`master/xcluster/xcluster_manager_if.h:41`）、`XClusterConsumerClusterMetrics`（`master/xcluster/xcluster_consumer_metrics.h:47`）、schema 同步任务（`xcluster/add_table_to_xcluster_source_task.cc` / `add_table_to_xcluster_target_task.cc`）；
- **TServer 端**：每个 tablet 的 CDC 流由 `CDCServiceImpl::GetChanges`（`src/yb/cdc/cdc_service.cc:1622`）响应 consumer 的 poll，吐出该 tablet WAL 中 `[from_op_id, to_op_id]` 段内的变更记录；
- **Target 端**：poll 拿到变更后写入 target cluster 的 DocDB（同样走 Raft），由 `XClusterConsumerReplicationStatusPB` 同步 checkpoint。

### 26.2 CDC GetChanges 流控（`CDCServiceImpl::GetChanges`）

`src/yb/cdc/cdc_service.cc:1622 void CDCServiceImpl::GetChanges(GetChangesRequestPB* req, GetChangesResponsePB* resp, RpcContext context)` 是 producer 端的核心入口。真实骨架（`:1622-1710`）：

```cpp
// src/yb/cdc/cdc_service.cc:1622-1710 真实骨架
void CDCServiceImpl::GetChanges(const GetChangesRequestPB* req,
                                GetChangesResponsePB* resp,
                                RpcContext context) {
  const auto start_time = MonoTime::Now();
  RPC_CHECK_AND_RETURN_ERROR(
      get_changes_rpc_sem_.TryAcquire(),
      STATUS(LeaderNotReadyToServe, "Not ready to serve"),
      resp->mutable_error(), CDCErrorPB::LEADER_NOT_READY, context);   // (a) 信号量限流
  auto scope_exit = ScopeExit([this] { get_changes_rpc_sem_.Release(); });

  if (!CheckOnline(req, resp, &context)) { return; }                   // (b) 在线校验
  auto status = TEST_SimulateError();                                  // (d) 错误注入
  if (!status.ok()) { /* RPC_STATUS_RETURN_ERROR */ }

  RPC_CHECK_AND_RETURN_ERROR(req->has_tablet_id(), /* InvalidArgument */);  // (e) tablet_id 必填
  RPC_CHECK_AND_RETURN_ERROR(req->has_stream_id() || req->has_db_stream_id(), /* ... */);

  ash::WaitStateInfo::UpdateCurrentTabletId(req->tablet_id());        // (f) ASH 监控埋点

  auto stream_id = RPC_VERIFY_STRING_TO_STREAM_ID(
      req->has_db_stream_id() ? req->db_stream_id() : req->stream_id());

  CoarseTimePoint deadline = GetDeadline(context, client());

  TabletStreamInfo producer_tablet = {.stream_id = stream_id, .tablet_id = req->tablet_id()};
  status = CheckTabletValidForStream(producer_tablet);                  // (g) 流-表绑定
  if (!status.ok()) { /* Tablet split / Invalid request */ }

  auto tablet_peer = context_->LookupTablet(req->tablet_id());
  auto original_leader_term = tablet_peer ? tablet_peer->LeaderTerm() : OpId::kUnknownTerm;
  bool isNotLeader = tablet_peer && tablet_peer->IsNotLeader();
  if (!tablet_peer || isNotLeader) {
    if (req->serve_as_proxy()) {                                       // (h) K8s 代理
      TabletLeaderGetChanges(req, resp, std::make_shared<RpcContext>(std::move(context)),
                             tablet_peer);
      return;
    }
  }
  if (isNotLeader) { tserver::FillTabletConsensusInfo(...); }

  RPC_CHECK_NE_AND_RETURN_ERROR(tablet_peer, nullptr, /* NotFound */);   // (i) tablet 存在性
  RPC_CHECK_AND_RETURN_ERROR(!tablet_peer->IsNotLeader(), /* NotFound */);
  RPC_CHECK_AND_RETURN_ERROR(tablet_peer->IsLeaderAndReady(), /* LeaderNotReadyToServe */);
  // 后续：从 WAL 读 [checkpoint+1, latest]，构造 CDC record 列表
}
```

关键点：
- **get_changes_rpc_sem_**（`:1626`）：信号量级别的并发限流——`TryAcquire` 失败时立即返回 `LEADER_NOT_READY`（不是排队阻塞），防止 consumer 突发流把 producer 打挂；
- **stream_id vs db_stream_id 双轨**（`:1649-1654`）：老 API `stream_id` 单流；新 API `db_stream_id` 整库流（CDCSQL），两者只能存在其一；
- **tablet leader 路由**（`:1677-1687`）：如果 GetChanges 落到非 leader tserver 且 `serve_as_proxy=true`，转发到 leader；这是 K8s 多 Pod 部署下的关键代理机制；
- **tablet split 透传**（`:1669 status.IsTabletSplit()`）：GetChanges 返回 `CDCErrorPB::TABLET_SPLIT`，让 consumer 知道要重新查 tablet 表（消费者端会重新解析 parent → children 关系）。

源码：`reference/distributed-sql/yugabytedb/src/yb/cdc/cdc_service.cc:1622 CDCServiceImpl::GetChanges`、`:432 TabletLeaderGetChanges`；`reference/distributed-sql/yugabytedb/src/yb/cdc/cdc_service.proto:339 GetChangesRequestPB`、`:590 GetChangesResponsePB`。

### 26.3 CDC 流身份与 checkpoint 双轨

`src/yb/cdc/cdc_service.proto:339 GetChangesRequestPB` 定义了请求字段（18 个字段，字段号 1-18）：

```protobuf
// src/yb/cdc/cdc_service.proto:339-394 真实字段
message GetChangesRequestPB {
  optional bytes stream_id = 1;                       // 老 API 单 stream 标识
  optional bytes tablet_id = 2;                       // 必填
  optional CDCCheckpointPB from_checkpoint = 3;       // consumer 上次确认的 checkpoint
  optional uint32 max_records = 4;                    // 最多读多少条记录
  optional bool serve_as_proxy = 5 [default = true];  // 非 leader tserver 是否代理转发
  optional bytes db_stream_id = 6;                    // 新 API 整 db stream 标识（CDCSQL）
  optional bytes table_id = 7;                        // colocated 表场景下用
  optional CDCSDKCheckpointPB from_cdc_sdk_checkpoint = 8;
  optional bool need_schema_info = 9 [default = false];
  optional CDCSDKCheckpointPB explicit_cdc_sdk_checkpoint = 10;
  optional int64 safe_hybrid_time = 11 [default = -1];
  optional int32 wal_segment_index = 12 [default = 0]; // 已流过的 WAL 段索引
  optional uint32 auto_flags_config_version = 13;     // xCluster auto-flag 兼容用
  optional CDCSDKRequestSource cdcsdk_request_source = 14 [default = DEBEZIUM];
  optional int64 raft_config_opid_index = 15;         // consumer 端已知 raft config op-id
  optional uint64 getchanges_resp_max_size_bytes = 16;// CDCSDK 模式下响应字节上限
  optional bool force_update_cdc_state_checkpoint = 17;
  optional int64 max_index_in_sort_window = 18 [default = 0]; // 上次 sort window 的 max op-id
}
```

`GetChangesResponsePB`（`cdc_service.proto:590-614`）返回 `error`（`CDCErrorPB` 错误码）+ `checkpoint`（consumer 回传标已 commit 的位点，用于下一次 GetChanges 的 `from_checkpoint`）+ `cdc_sdk_checkpoint`/`records[]`/`safe_hybrid_time`/`wal_segment_index`/`max_index_in_sort_window` 等。`CDCErrorPB`（`:82`）定义错误枚举，包含 `TABLET_SPLIT` / `LEADER_NOT_READY` / `AUTO_FLAGS_CONFIG_VERSION_MISMATCH` 等。

stream_id/db_stream_id 的关系：
- 老 API（v2.0 ~ v2.14）：每个 table 一个 `stream_id`，由 master `XClusterOutboundReplicationGroup` 协调；
- 新 API（v2.16+）：一个 db 一个 `db_stream_id`，由 master `CDCSysCatalogEntry` 维护；
- 两者本质都是「UUID + metadata」，区别在 master 端维护粒度；`req->has_stream_id() || req->has_db_stream_id()` 二者必有其一（`cdc_service.cc:1649-1654`）。

源码：`reference/distributed-sql/yugabytedb/src/yb/cdc/cdc_service.proto:339 GetChangesRequestPB`、`:590 GetChangesResponsePB`；`reference/distributed-sql/yugabytedb/src/yb/cdc/cdc_state_table.cc`（stream metadata 持久化）。

### 26.4 Tablet Split 透传

`src/yb/cdc/cdc_service.cc:1669` 的 `status.IsTabletSplit()` 返回 `CDCErrorPB::TABLET_SPLIT`，consumer 端收到后必须：
1. 调 master `GetTableLocations` 重新解析原 parent tablet_id 的 children（children 已分裂成两个新 tablet）；
2. 对每个 child 重新发 `GetChanges`，`from_op_id` = parent 的 last checkpoint。

`TabletLeaderGetChanges`（`:432`）处理非 leader 路由；CDC 流的「producer 表变化」由 `cdc_consumer_registry_service.cc` 协同。

源码：`reference/distributed-sql/yugabytedb/src/yb/cdc/cdc_service.cc:1669 status.IsTabletSplit() → TABLET_SPLIT`；`reference/distributed-sql/yugabytedb/src/yb/cdc/cdc_error.h`（`IsTabletSplit` 实现）；`reference/distributed-sql/yugabytedb/src/yb/master/xcluster_consumer_registry_service.cc`（CDC stream 注册）。

### 26.5 复杂度与本项目对照

| 操作 | YB xCluster | 本项目（无对应实现） |
|------|-------------|----------------------|
| GetChanges 单次 poll | O(N) N = WAL 段内 op 数，受 `max_bytes_of_response` 截断 | 不存在 |
| CDC 流身份解析 | O(1) hashmap<stream_id, CDCStreamInfo> | 不存在 |
| Tablet split 透传 | O(1) 错误返回，consumer 重查 `O(R)` R = child tablet 数 | 不存在 |
| GetChanges 信号量限流 | O(1) TryAcquire | 不存在 |
| Consumer applier 重放 | O(M) M = 待 apply op 数，写入 target DocDB | 不存在 |

复杂度分析（一次 poll）：
- **WAL 读**：O(N) 按 segment index + offset 顺序读 + 反序列化 CDC record；
- **checkpoint 推进**：O(1) 在响应里回传 max_committed_op_id；
- **总**：单次 poll `O(N)`，其中 N 受 `max_bytes_of_response`（默认 1 MB）/ `max_num_records`（默认 1024）双重限流。

本项目对应：`engineering/src/db/replication/` 当前为空目录（占位），未来扩展可参考 YB 的「master 端元数据 + tserver 端 CDC producer/consumer」双层架构，把本项目的 `engineering/src/db/wal.c` 改造为 CDC-compatible WAL reader，提供跨进程 WAL 推送能力。

## 二十七、参考资料

1. YugabyteDB 官方文档：https://docs.yugabyte.com/
2. YugabyteDB Architecture 章节：https://docs.yugabyte.com/preview/architecture/
3. Spanner (OSDI 2012)：https://research.google/pubs/spanner-googles-globally-distributed-database/
4. Percolator (OSDI 2010)：https://research.google/pubs/large-scale-incremental-processing-using-distributed-transactions-and-notifications/
5. Raft (USENIX ATC 2014)：https://raft.github.io/raft.pdf
6. Aether: A Scalable Approach to logging (VLDB 2010)：https://infoscience.epfl.ch/record/149436
7. Cassandra 架构 (Lakshman & Malik, 2009)：https://www.cs.cornell.edu/projects/ladis2009/papers/Lakshman-ladis2009.PDF
8. LSM-Tree (O'Neil et al., 1996)：http://www.cs.berkeley.edu/~brewer/cs262/LCS.pdf
9. YugabyteDB 源码 GitHub：https://github.com/yugabyte/yugabyte-db
10. YB 基准发布博客：https://www.yugabyte.com/blog/ (搜索 "tpmC" / "benchmark")
