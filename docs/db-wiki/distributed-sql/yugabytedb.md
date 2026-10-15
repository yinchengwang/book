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

## 二十四、参考资料

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
