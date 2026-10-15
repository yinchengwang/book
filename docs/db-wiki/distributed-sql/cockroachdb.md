# CockroachDB Wiki（分布式 SQL / NewSQL）

> 参考版本：CockroachDB 仓库当前 commit `8812064a015d2faf99d3fc7e15880f94042954b0`（v24.x 系列），主要 API 与数据布局在 v22.x 之后保持稳定。

## 一、项目定位

CockroachDB（绰号 "CRDB"）是一款以 **PostgreSQL 兼容性**为对外接口、**Raft 共识 + 分片复制**为内核的云原生**分布式 SQL 数据库**。它把 Google Spanner 的"全球分布式、强一致、SQL"理念落到工程实现中，所有节点对等（share-nothing），通过 Raft 组实现自动分片、容灾与水平扩容。

- **痛点**：
  - 传统分库分表（MySQL Sharding）需要应用层承担一致性、跨分片查询、扩缩容的痛苦；
  - NewSQL 的"既要又要"（强一致 + 横向扩展 + SQL + 高可用）以往只有在 Google Spanner/F1 内部可见；
  - 商业 MPP（Teradata / Vertica）运维复杂且价格昂贵。
- **典型用户**：云上的 SaaS 业务（金融、游戏、广告、零售）、多区域容灾的强一致交易平台、需要"无限横向扩容 + 标准 SQL"的创业团队。代表客户：DoorDash、Bloomberg、Comcast、Etsy、Lacerda、SpaceX 等。

## 二、发展历史与社区

- **起源**：2015 年 Spencer Kimball（前 Square 数据基础设施负责人）、Peter Mattis、Ben Darnell 在纽约创立 Cockroach Labs；公司名取自"Cockroach"（蟑螂），寓意"杀不死的小强"——极强的容灾能力。
- **里程碑**：2015 年开源（Apache 2.0）；2017 年 1.0（生产就绪）；2020 年 20.1（企业许可切换为 BSL）；2022 年 22.1（多区域 SQL 抽象 GA、レンジフォロー読み follower reads）；2023 年 23.1（存算分离、多区域区域表 `REGIONAL BY ROW`）；2024 年 24.1（向量索引、pgvector 兼容、`ALTER TABLE ... SET STORAGE PARAMETER`）。
- **维护状态**：活跃；每季度发布一个大版本（22.x、23.x、24.x ...），每个版本支持约 18 个月（含 LTS 周期）。提交规范见 `reference/distributed-sql/cockroachdb/.claude/rules/`。
- **社区**：Slack / Discuss / GitHub Discussions / CockroachDB University（免费认证课程）。工程师文化近乎"分布式系统 OS"。

## 三、许可与商业限制

- **许可证**：2020 年起切换为 **Business Source License (BSL/Elastic License v2 变体)**，但**核心代码仍可在非生产或非商业场景下自由使用**。
  - 来源：`reference/distributed-sql/cockroachdb/LICENSE`
  - 关键约束：禁止将 CockroachDB 直接作为"数据库即服务"对外提供（除非获得 Cockroach Labs 授权），但自家生产环境使用、自建内部平台免费。
- **CockroachDB Enterprise（CockroachDB Cloud、Self-Hosted）**：增加**企业特性闭源模块**（CCL — Closed-Core License）：
  - 物理复制 / 备份恢复（`pkg/ccl/`）
  - 多租户隔离、加密 secret
  - row-level TTL、变更数据捕获（CDC）到 Kafka/Pulsar
  - 主备物理备份、增量备份
- **CockroachDB Cloud**：Cockroach Labs 自营的全托管 DBaaS，提供 Dedicated（专用）与 Serverless（按 RU 计费）两种形态，部署于 AWS/GCP/Azure。

## 四、业界主要使用方与案例

- **金融 / 支付**：LPL Financial（证券交易底层）、BaiERS（巴西银行）、Garanti BBVA、Payit（英国支付平台）。
- **电商 / 物流**：DoorDash（订单配送状态）、Etsy（订单管线）、Comcast（XB 运营平台）。
- **娱乐 / 媒体**：Bloomberg、Audible、EchoStar。
- **SaaS**：SpaceX 内部工具、F5 Inc、Pretio、Imaginary Cloud。
- **云**：自部署覆盖 AWS / GCP / Azure / 私有 Kubernetes 集群，Helm Chart 与 Operator 由 Cockroach Labs 与社区共同维护。

## 五、一句话对比定位

> CockroachDB 是**"PostgreSQL 兼容性 + Raft 自动分片 + 全球一致"**的 NewSQL 代表；与 **TiDB（MySQL 兼容、HTAP、RocksDB 分片）**、**YugabyteDB（双 API、DocDB 文档存储）**、**OceanBase（阿里自研、Oracle/MySQL 兼容、Paxos）** 一起构成 NewSQL "四强"。

## 六、整体架构

CockroachDB 是一个**单体二进制（`cockroach`）+ 多 SQL/KV 节点对等**的系统。每个节点同时承担 SQL 入口、KV 路由、存储副本、Raft 共识中的全部角色。

### 6.1 总体分层

```
┌────────────────────────────────────────────────────────────────────────┐
│                       客户端 (libpq / JDBC / pgx)                       │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ PostgreSQL Wire Protocol v3
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    SQL 层  (pkg/sql)                                    │
│   - Parser (pkg/sql/parser)                                            │
│   - OptBuilder  (pkg/sql/optbuilder → optimizer)                       │
│   - Optimizer  (pkg/sql/opt)  — Cascades 风格 + 规则/代价              │
│   - DistSQL Planner (pkg/sql/distsql) → DistSQL Spec                  │
│   - Executor / Session (pkg/sql/executor, pkg/sql/sessiondata)         │
│   - Schema Changer (pkg/sql/schemachanger) — online DDL                │
│   - CDC Sink (pkg/ccl/changefeed)                                      │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ 流式 DistSQL (gRPC)
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    KV 层  (pkg/kv)                                      │
│   - TxnCoordSender (pkg/kv/txn_coord) — 分布式事务协调器              │
│   - DistSender (pkg/kv/dist_sender)     — Range 路由                  │
│   - Cross-range Transactional Pipeliner                                │
│   - Admission Control (pkg/kv/admission) — 智能流控                   │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ internal client.Batch / gRPC
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    复制组层 (pkg/kv/kvserver)                            │
│   - Replica (含 Raft 状态机、Lease、MVCC、Lock Table)                  │
│   - Proposal Queue / Replica Application                              │
│   - Leaseholder 选举 (raft.Lease)                                      │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ Raft log replica
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    共识层 (pkg/raft)                                    │
│   - 基于 etcd-raft 改造的 Raft 实现                                    │
│   - PreVote / Learner / Joint Consensus                                │
│   - Snapshot / Log Truncation                                          │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ Pebble KV ops
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    存储层 (pkg/storage) — Pebble                       │
│   - LSM-Tree (leveled compaction)                                      │
│   - MVCC key encoding (timestamp-suffixed keys)                        │
│   - 块级加密 / 备份 / 导入 / 导出                                      │
└────────────────────────────────────────────────────────────────────────┘
```

源码入口：
- SQL 入口：`pkg/sql/catalog/catalog.go:1` (Catalog 接口与 Descriptor 抽象)
- KV API：`pkg/kv/kvpb/api.go:1` (BatchRequest/BatchResponse protobuf 协议)
- Raft 实现：`pkg/raft/raft.go:1` (Raft 状态机 `raft.Node` / `raft.RawNode`)

### 6.2 单节点进程模型

单进程（C++ 写成的 Go binary）内部通过 **goroutine 池**实现并发，关键 goroutine：

- **PGWire Server** (`pkg/sql/pgwire/`)：监听 26257 端口（默认），每个客户端连接一个独立 goroutine。
- **gRPC Server**：节点间 KV / DistSQL / 复制 / 心跳通信。
- **每 Range 一组 goroutine**：
  - Raft loop（选举、出队、apply）
  - Replica proposal queue
  - MVCC GC worker
  - Replica load split scanner
- **后台系统 goroutine**：
  - `timeseries` 采样
  - `status` 节点摘要广播
  - `gcworker`（TTL / 系统保留期清理）
  - `queue.gc`（Raft log compaction）
  - `rebalance`、`merge queue`、`split queue`（Range 拓扑自调整）

### 6.3 一条 SQL 的端到端路径（简化）

1. **PGWire 协议解析** → `pgwire/v3.go` 解析 startup / query message；
2. **SQL parser** → `parser/parse.go` 生成 AST `tree.Statement`；
3. **OptBuilder + Optimizer** → `opt/optbuilder` + `opt/optimizer` 生成逻辑计划 + 代价最优物理计划；
4. **DistSQL Planner** → 决定哪些 logical stage 可下推（DistSQL spec）→ 生成 `FlowSpec`；
5. **物理执行**：
   - 在 gateway 节点启动 `flowinfra/flow`，把 stage 派发到对应节点的 processor（gRPC）；
   - 各 processor 通过 `RowSource` 流式返回结果；
6. **结果回到 PGWire** → 编码为 DataRow/BatchComplete 消息返回客户端。

源码入口：`pkg/sql/distsql/run/flow.go:1`。

## 七、存储引擎

### 7.1 Key 编码与 MVCC

CockroachDB 用**单层 LSM-Tree + MVCC 多版本键**的方式统一事务与存储模型。

```
原始 SQL:  INSERT INTO kv(k, v) VALUES ('user:1', 'alice');
编码:
  /Table/58/1/user:1/0.123456789.000 -> 'alice'    (timestamp = 0.123456789.000)
读时:
  SELECT v FROM kv WHERE k = 'user:1';
  → 在 Range 内按 key 顺序扫描，取 <= read_ts 的最新版本
```

- **key prefix 编码**：`/Table/<tableID>/<indexID>/<indexColumns...>/<version_timestamp>`
- **MVCC 可见性**：每条 key 都带 timestamp（wall time + logical counter），事务通过 `read_ts` / `write_ts` 过滤；
- **垃圾回收**：默认保留 25 小时（`kv.gc.ttlseconds`），由 `gcworker` 后台按表清理旧版本；
- 关键源文件：`pkg/storage/engine.go:1`（MVCC API）、`pkg/storage/mvcc.go:1`（MVCC key/value 编解码）。

### 7.2 Pebble 引擎

`pkg/storage` 是对 **Pebble**（`cockroachdb/pebble`，也是 Cockroach Labs 自研的 Go 实现的 KV 引擎，曾被推荐作为 RocksDB 的替代）的封装：

- **LSM-Tree leveled compaction**（与 RocksDB 类似，但更激进的 compaction 调度与 WAL）；
- **Block Cache**、**Bloom filter**、**Merger** 抽象；
- **MVCC key 排序**：timestamp 在 key 末，保证新版本在前；
- **Range-level checkpoint**：每个 Range 独立做 snapshot / backup。

源码：`pkg/storage/pebble.go:1`。

### 7.3 SQL 表到 Range 的映射

```
[TableDescriptor] ┐
                  │   table_id, index_id, parent_id, ...
                  ▼
[RangeDescriptor]────  start_key, end_key, replicas[]
                  │
                  ▼
            Raft group
```

- 表数据按**主键有序**拆分成连续 key 区间的 Range（默认 64 MiB）；
- 副本数默认 3（`constraints` 配置），跨 zone 放置。

## 八、索引机制

CockroachDB 采用 **主键驱动的有序存储**，二级索引与 PostgreSQL 行为一致：

| 索引类型 | 实现 | 备注 |
|----------|------|------|
| **Primary Key** | 表数据本身按主键顺序排列 | 强制唯一（`NOT NULL` + `UNIQUE`） |
| **Unique Secondary Index** | 倒排 + 主键链路 | 写放大略大于 PostgreSQL |
| **Non-Unique Secondary** | 多版本重复键 + PK 链路 | 与 PG 一致 |
| **Hash-sharded Primary** | 内部多列 hash 分桶后追加 | 解决单调写入热点 |
| **Inverted Index** | JSONB / 数组倒排 | `jsonb_ops`、`array_ops` opclass |
| **Vector Index** (`vec2vec`) | 量化 + HNSW on expression | v24.1 起 GA |
| **Partial Index** | `WHERE` 谓词过滤 | PG 兼容 |
| **Expression Index** | 函数 / 表达式结果 | `CREATE INDEX ... ON t ((col->>'k'))` |

源码：`pkg/sql/catalog/descpb/structured.go`（`TableDescriptor.Indexes` 字段）。

### 8.1 Hash-sharded 主键

为解决**单调递增 PK（如时间戳、序列）导致的 Range 写热点**，CockroachDB 提供：

```sql
CREATE TABLE event (
  id UUID PRIMARY KEY USING HASH,
  ts TIMESTAMP,
  payload JSONB
) USING HASH WITH (bucket_count = 8);
```

`bucket_count` 把写入打散到多个 Range，代价是范围查询需要 fan-out。

## 九、查询 / 计算 / 接口层

### 9.1 SQL 语言与 PostgreSQL 兼容性

- **PostgreSQL 15+ wire 协议兼容**（含 prepared statement、extended query、COPY）；
- **SQL 方言**：以 PostgreSQL 为基准，针对 NewSQL 做了扩展：
  - 多区域表：`EXPERIMENTAL/REGIONAL BY ROW IN 'region'`
  - 地理分区：`PARTITION BY LIST (region)`
  - 向量列：`VECTOR(3)`、`halfvec`、`vec2vec` 距离函数
  - JSONB、ARRAY、UUID、INET、地理空间（部分）— PG 兼容
- 已知**与 PG 的差异**（受分布式事务影响）：
  - 没有 `OID` 与 `lo` 大对象
  - 函数稳定性分类强约束：`immutable / stable / volatile`
  - `pg_catalog` 系统表子集（`information_schema`、`pg_class`、`pg_namespace` 等都存在）

### 9.2 DistSQL（分布式 SQL 执行）

- 物理计划被切分成 **Flow + Processor + Router** 三层：
  - `FlowSpec`：在每个节点上实例化的执行链
  - `Processor`：表扫、join、aggregator、project、limit 等算子
  - `Router`：决定数据如何在 processor 间路由（hash / range / mirror）
- **向量化**：每个 processor 内部是**逐行处理 + ColBatch**（不全向量化，但内存连续）—— 与 TiDB 矢量化执行不同。

源码：`pkg/sql/distsql/run/flow.go:1`、`pkg/sql/distsqlpb/processor.proto`。

### 9.3 接口与生态

- **PostgreSQL 客户端**全部可用：psql、pg_dump、JDBC、pgx、Python `psycopg2`、Node `pg`；
- **HBA**：标准 PG `pg_hba.conf` 格式；
- **SQL CLI**：`cockroach sql --insecure`；
- **HTTP Admin UI**：默认 `http://localhost:8080`（内置 DB Console，可以看 SQL Activity、KV Hot Ranges、Disk Usage）；
- **Prometheus** exporter：内置 `/_status/vars` endpoints。

## 十、事务与一致性

### 10.1 隔离级别

| 隔离级别 | 是否支持 | 说明 |
|----------|----------|------|
| READ COMMITTED | 否 | 历史版本曾部分支持，现已弃用 |
| REPEATABLE READ | 否 | - |
| SNAPSHOT | **默认** | 通过 `AS OF SYSTEM TIME` 指定历史 |
| SERIALIZABLE | 支持 | 默认开启 SERIALIZABLE |
| LINEARIZABLE | 是 | 借 Raft + 时钟 + READ COMMITTED 历史读 |

- 事务默认级别：`SERIALIZABLE`（写路径 linearizable，读路径可降级到 `SNAPSHOT` 通过 `SET TRANSACTION ISOLATION LEVEL SNAPSHOT`）。
- 误用 `SERIALIZABLE` 可能出现 **write-write conflict → 自动重试**，客户端需有重试逻辑（pgx 自动 retry，其他驱动可能需要手动）。

### 10.2 分布式事务模型：Cockroach DB 风格 2PC

CockroachDB 不使用 Percolator / Spanner-style TrueTime，而是用 **乐观 + OCC + 意向锁 + 心跳**：

```
client → gateway TxnCoordSender
  ├── BeginTxn: pick a random txn epoch + read_ts
  ├── Read  (any node, MVCC: visible when ts <= read_ts, intents skipped)
  ├── Write (deferred):
  │     • 在 leaseholder 上写入 Intent (带有 txn record 的指针)
  │     • Intent 可位于多个 Range
  └── Commit:
        1) Cleanup (写 txn record，写入 WriteTooOld 状态)
        2) 异步 Resolve Intents: 全部通过 Raft 把 Intent 升级为版本化 commit/abort
           - 成功：在每个 Range 异步 push resolution（无需 2PC，因为 txn record 已是最终结果）
           - 失败：异步 rollback intents
```

> 与 Percolator 的关键差异：**没有显式 2PC 的 prepare 阶段**。CockroachDB 把 "prepare" 内化到"写入 Intent + commit txn record"中，通过全局的 `txn record` 在 commit 时一锤定音——所有 Intent 的最终命运由一个 Raft-proposal 决定。

源码：`pkg/kv/txn_coord_sender.go:1`、`pkg/kv/txn.go:1`。

### 10.3 Raft 与 Leaseholder

- 每个 Range 是一个 Raft group；
- 每个 Raft group 同时有**一个 Leaseholder**（处理 Lease-based 心跳），Lease 是 Raft 之上的一层 read-quorum 优化：读请求只走 Leaseholder 即可拿到 linearizable 读；
- **Lease transfer** vs **Range lease transfer** —— 都是 RTO 优化的重要路径。

源码：`pkg/kv/kvserver/replica_raft.go:1`（Raft 主循环）、`pkg/kv/kvserver/replica_lease.go:1`（Lease 转移）。

### 10.4 时间戳与世界时钟

- 时间戳 = **wall time + logical counter**（节点 HLC — Hybrid Logical Clock）；
- HLC 通过 NTP + gossip 保持单调递增；
- v22.1+ 引入 **rangefeed + closedts** 让 follower 在次秒级看到一致时间戳；
- 没有 Spanner 那种 GPS / atomic clock，所以只承诺 **bounded staleness**。

源码：`pkg/util/hlc/hlc.go:1`、`pkg/kv/kvserver/closedts/`.

## 十一、复制与高可用（深度扩展）

CockroachDB 的存活与扩展建立在三层：**进程对等 → Range 级 Raft → 集群级 zone/topology 约束**。

### 11.1 多副本：Raft 组与副本分布

每个 Range 默认 3 副本，分布在不同节点 / zone（同 zone 同机架反亲和）：
- 副本进入 Raft group 通过 `confChange` —— Learner → Voter 的两阶段切换，避免切换期间出现可用性空窗；
- 写入走 `Raft` 多副本 commit，故障域 ≤ (N/2)-1 时仍可服务。
- 副本失联 timeout（10s）后触发选举，新 leader 立即接管。

### 11.2 多区域容灾（Multi-Region Abstraction）

CockroachDB 自 v22.1 起把多区域建模为一类**对象生命周期**：

```sql
SET CLUSTER SETTING cluster.organization = 'My Company';
ALTER DATABASE app SET MULTI_REGION ZONE='us-east1';
-- 自动在每个 region 落一份副本；就近写
ALTER TABLE orders SET PRIMARY REGION 'us-east1';
ALTER TABLE orders ADD REGION 'us-west1', 'eu-west1';
CREATE TABLE orders (
  id UUID PRIMARY KEY,
  region crdb_internal_region,
  ...
) LOCALITY REGIONAL BY ROW;
```

- **REGIONAL BY ROW**：每行根据自己的 `region` 列写在对应 region 的 leaseholder，其他 region 副本做 follower；读可走 follower，强制最近 read 时会写到对应 region。
- **REGIONAL BY TABLE**：整张表固定一个 home region，但保留跨 region 的 follower。
- **GLOBAL**：纯参考表，所有 region 都维护一份全量数据。

优势：
1. 就近写（低延迟）；
2. 单 region 离线不丢服务；
3. RPO≈0、RTO≈节点重启时间。

源码：`pkg/sql/multi_region/`、`pkg/ccl/multiregionccl/`。

### 11.3 自动恢复与自均衡

- Replica 自平衡器（allocator）实时调整副本分布，使负载 + 存储尽量均匀：
  - `rebalance_concurrency` 控制并行；
  - `store_load` 维度：CPU、读、写入、Range 数；
  - 由 `pkg/kv/kvserver/allocator/` 实现。
- 节点加入 / 离开 / 临时故障 / 永久 decommission 均走原子化的 **Membership Change**（joint consensus）。

## 十二、扩展性（深度扩展）

### 12.1 分片（Range Split）

- 默认 Range 上限 64 MiB（`kv.range_split.load_threshold`）；
- **自适应拆分**：当热 Range 上的 QPS 超过阈值，触发 split；冷 Range 之间可 merge；
- 一个 Range 的副本只能位于不同的节点，不会跨节点分片。

### 12.2 水平扩容流程

```
1. 新节点 cockroach start --join <load balancer>
2. 自动 bootstrap 到现有 cluster；gossip 拓扑；
3. allocator 把超负荷 Range 迁走若干副本到新节点；
4. (可选) 主动 ALTER TABLE ... SPLIT AT (value) 控制预切片。
```

### 12.3 在线 Schema Change

CockroachDB 把所有 DDL 当作 **Schema Changer 作业**：

- **声明式 Schema Changer**（`pkg/sql/schemachanger/`）：用事件驱动 + 反向操作自动回滚，目前覆盖 CREATE / ALTER / DROP；
- **旧事务式 Schema Changer**（`pkg/sql/schema_changer/`）：以 JOBS 表为载体，分阶段执行（写双份、backfill、cutover、清理）；
- 在线变更对用户无感知，过程中继续接受 DML。

### 12.4 弹性副本策略：Survival Goals

```sql
ALTER DATABASE app SURVIVE ZONE FAILURE;       -- 默认：1 zone 离线不影响
ALTER DATABASE app SURVIVE REGION FAILURE;     -- 1 region 离线不影响
```

- `ZONE FAILURE`：副本默认 3，任意 1 zone 挂 → 不影响 quorum；
- `REGION FAILURE`：区域级副本数需要 5（`replication_factor=5`）+ 3-region 部署。

源码：`pkg/ccl/multiregionccl/zonecfg.go`。

## 十三、性能与基准

### 13.1 TPC-C

- 单集群水平扩展：v23.x 在 25 节点 v23.1 上跑出 **6.46M tpmC**（官方公布）；
- 与代表商用产品（Oracle Exadata、TiDB、OceanBase）的同等级公开数据比较，CockroachDB 在跨 region 一致性与弹性上更强，峰值吞吐略低于专门 OLTP 数据库。

### 13.2 Sysbench / YCSB / TPC-H

- **YCSB**：写多读少场景 LSM-Tree 表现优于 B-Tree；CockroachDB 在 YCSB Workload A (50% read/50% update) 上 10k-100k ops/s/节点。
- **TPC-H**：受限于 MPP 化程度不高（尚未全面向量化），TPC-H 100GB 数十节点规模比 Greenplum、DuckDB 慢 2-5 倍；推荐 HTAP 工作负载的复杂查询改走专用 OLAP 引擎。

### 13.3 延迟分布

- 同城 Region 内读写 P99 通常 5-30 ms（1 KB payload）；
- 跨 Region 写入 P99 100-300 ms（受 Raft 跨 Region commit + 时钟漂移影响）；
- 大量小 Range 的扫描 O(N log N)，百万级 Range 数据集需预切片。

## 十四、运维与可观测性

### 14.1 DB Console（Web UI）

启动后访问 `http://<node>:8080`：

- **Overview**：QPS、CPU、内存、副本数、SQL 错误率；
- **SQL Activity**：实时 + 历史慢查询（fingerprint 归并）；
- **Statements**：topN 慢 SQL；
- **KV Hot Ranges**：识别热点 Range，指导 `ALTER TABLE ... SPLIT AT`；
- **Storage**：每个 store 的 LSM 详情、SST 数、compaction 队列；
- **Jobs**：Schema Changer、Backup、Restore、Changefeed 状态；
- **Network**：RPC 错误、P99、P95。

### 14.2 备份与恢复

- **全量备份** + **增量备份**（企业版）：
  - `BACKUP DATABASE app INTO 's3://bucket/path?AWS_ACCESS_KEY_ID=...';`
  - 自带时间戳旅行；可指定 `AS OF SYSTEM TIME '-10s'`；
- **恢复**：`RESTORE DATABASE app FROM 'latest' IN 's3://...';`
- **企业特性**：`BACKUP ... WITH revision_history`（PITR）；
- **导入 / 导出**：`IMPORT INTO ... CSV` / `EXPORT INTO CSV`—— 直接通过 DistSQL 流式 ETL，与备份共享底层存储格式。

源码：`pkg/ccl/backupccl/`、`pkg/sql/importer/`、`pkg/ccl/exportccl/`。

### 14.3 CDC（变更数据捕获）

`CREATE CHANGEFEED FOR TABLE orders INTO 'kafka://...'`:

- 顺序保证（range 内），全局顺序需外部排序键；
- 多种 sink：Kafka / Pulsar / Cloud Storage / Webhook；
- 字段过滤、projection、computed columns；
- 早期按 `rangefeed` 实现，由 `pkg/ccl/changefeedccl/` 承载。

### 14.4 Prometheus / Grafana

- 自带 `_status/vars` (Prometheus 文本格式)；
- 官方 Grafana Dashboard JSON 在 `pkg/ui/oss/`。

## 十五、生态与集成

### 15.1 客户端 SDK

| 语言 | 驱动 | 备注 |
|------|------|------|
| Go | `pgx` / `lib/pq` | 推荐 `pgx` |
| Python | `psycopg2` / `psycopg3` | 推荐 `psycopg3` |
| Java | `JDBC pgjdbc` | `org.postgresql:postgresql` |
| Node.js | `pg` / `postgres` | 推荐 `postgres` (`porsager/postgres`) |
| Ruby | `pg` gem | 标准 |
| Rust | `tokio-postgres` | 异步友好 |

**注意**：因实现 SERIALIZABLE 自动重试 + 中间重试需要事务 hint，官方推荐 SDK 配置 `application_name = ...` 便于排查；连接串可加 `?sslmode=verify-full&sslrootcert=...`。

### 15.2 应用迁移

- 从 PostgreSQL 迁入：`IMPORT INTO ... PGDUMP` —— 解析 pg_dump 输出的 SQL 与 COPY 数据；
- 从 MySQL 迁入：`IMPORT INTO ... MYSQLDUMP`（企业版）；
- 从 MongoDB 迁入：`IMPORT INTO ... MONGOEXPORT`。

### 15.3 Kubernetes Operator

`cockroachdb/cockroach-operator` 提供声明式部署、自动恢复、证书管理、备份集成；Helm Chart 同样由官方维护。

### 15.4 周边生态

- **CDC → Kafka → ClickHouse / Snowflake / Databricks** 是常见管线；
- **DBeaver / DataGrip / TablePlus** 等 GUI 均支持 PG 协议；
- **Flyway / Liquibase** 通过 PG 兼容协议可用，但对 CockroachDB 特定 DDL（multi-region、AS OF SYSTEM TIME）需自定义。

## 十六、核心论文与算法原理

- **Spanner 论文**：James C. Corbett et al., "Spanner: Google's Globally Distributed Database", OSDI 2012 — CockroachDB 的设计哲学承袭。
- **Raft 论文**：Diego Ongaro, "Consensus: Bridging Theory and Practice", Stanford PhD 2014 — `pkg/raft/` 直接来源于 etcd-raft。
- **CockroachDB Architecture 文档**：`https://www.cockroachlabs.com/docs/stable/architecture/overview.html` —— 工程实践权威。
- **Percolator 论文**：Dewey et al., "Large-scale Incremental Processing Using Distributed Transactions and Notifications", OSDI 2010 — 对照 CockroachDB 的"无 2PC"事务设计。
- **HLC 论文**：Sandberg et al., "The Hybrid Logical Clock", 2017 — `pkg/util/hlc` 实现。

## 十七、源码学习路径

### 17.1 推荐阅读顺序

1. **顶层 README + ARCHITECTURE.md**（仓库根）；
2. **`pkg/sql/CLAUDE.md`** + **`pkg/sql/catalog/catalog.go`** （Descriptor 模型）；
3. **`pkg/kv/txn_coord_sender.go`** （分布式事务协调，理解 txn record 如何作为"prepare anchor"）；
4. **`pkg/kv/kvserver/replica.go`** （Replicator、状态机、Raft loop、MVCC）；
5. **`pkg/raft/raft.go`** （raft.Node、election、log replication）；
6. **`pkg/storage/pebble.go`** + **`pkg/storage/mvcc.go`** （落盘与 MVCC）；
7. **`pkg/sql/opt/optimizer.go`** + **`pkg/sql/distsql/run/flow.go`** （优化器 + DistSQL）。

### 17.2 关键文件清单

- `pkg/sql/catalog/catalog.go:1`
- `pkg/sql/catalog/descpb/structured.go:1`
- `pkg/kv/kvserver/replica_state.go:1`
- `pkg/kv/kvserver/replica_raft.go:1`
- `pkg/kv/txn_coord_sender.go:1`
- `pkg/raft/raft.go:1`
- `pkg/storage/mvcc.go:1`
- `pkg/storage/pebble.go:1`
- `pkg/sql/opt/optimizer.go:1`
- `pkg/sql/distsql/run/flow.go:1`
- `pkg/ccl/multiregionccl/zonecfg.go:1`
- `pkg/util/hlc/hlc.go:1`

### 17.3 关键测试文件

- `pkg/kv/kvserver/client_raft_test.go`
- `pkg/kv/kvserver/client_merge_test.go`
- `pkg/kv/txn_test.go`
- `pkg/storage/mvcc_test.go`
- `pkg/sql/schemachanger/`（schema changer 的依赖图测试）

## 十八、源码规模与开发语言

- **语言**：99% Go（`pkg/` 全部 Go 代码），少量 Bazel `BUILD`、Shell（`dev` 工具脚本）、Proto；
- **代码规模**：master 分支约 130+ 万行 Go；
- **构建系统**：Bazel（主仓 `WORKSPACE`）+ `./dev` 脚本；
- **依赖管理**：Bazel Go module + 内部 `go.mod`；
- **格式化**：`crlfmt`（Go 源码专用，仓库根有 `./dev` 调用封装）。

## 十九、学习资源

- **官方文档**：`https://cockroachlabs.com/docs/stable/`（含 Architecture 章节）
- **CockroachDB University**：免费认证课程（`learn.cockroachlabs.com`）
- **CockroachDB Blog**：工程团队博客，包含许多 deep-dive（Multi-Region、CDC、Pebble 等）
- **GitHub Discussions**：https://github.com/cockroachdb/cockroach/discussions
- **Conference Talks**：KubeCon、GopherCon、SIGMOD、PODS
- **书籍**：无完整版"教科书"，但官方 Academy 课程可视为权威教材；
- **Podcast**：Database Internals Podcast（Cockroach 工程师常客）

## 二十、可移植性与部署

- **二进制**：单文件 `cockroach` 静态链接，可直接 `tar` 到 Linux/macOS；
- **Docker**：`cockroachdb/cockroach:latest`；
- **Kubernetes Operator**：`cockroachdb/cockroach-operator`（Helm Chart 同仓库）；
- **平台**：Linux x86_64 / arm64、macOS x86_64 / arm64；Windows **不官方支持**（仅开发构建）；
- **底层依赖**：Go runtime，无需 C++ 动态库；
- **嵌入式使用**：理论上可行（`cockroach start --single-node`），但生产定位仍是集群数据库。

## 二十一、变体与衍生项目

- **CockroachDB Enterprise**：闭源扩展（CockroachCloud / Self-hosted Enterprise），含物理备份、更高级的权限、加密密钥管理；
- **CockroachDB Serverless** vs **Dedicated**：按 RUs 计费 vs 专用资源池；
- **CockroachDB Cloud**：多云 DBaaS；
- **社区接手分支**：`cockroachdb/cockroach` 是唯一上游；Cockroach Labs 商业服务基于同一仓；
- **API 兼容**：PostgreSQL 完全兼容（pgwire 协议），同时也提供 `EXPERIMENTAL` 的 MySQL 协议（v22.x 删了，未来不回头）。

## 二十二、风险与陷阱

| 风险 | 缓解 |
|------|------|
| SERIALIZABLE 自动重试导致客户端逻辑看似成功但中间回滚 | 用 `SAVEPOINT` + 重试包装器；SDK 选支持 retry 的（如 `pgx` + 自定义 hooks） |
| 写单调时戳仍会产生单 Range 热点 | 用 `USING HASH` 主键、Range split 预设、`ALTER TABLE ... SPLIT AT` |
| Schema Changer 进度的 long-running job 占用资源 | 通过 `SHOW JOBS` 控制；拆分大表分批 ALTER |
| CDC Sink 故障导致 changefeed lagging | 用 `EXPERIMENTAL CHANGEFEED ... WITH resilience` + 监控 `*changefeed.failures` |
| 多 region 写入流量被 home region 拖慢 | 选 `REGIONAL BY ROW` + 就近写；不要无脑 `GLOBAL` |
| large Range merge 后性能骤降 | 监控 `kv.merge_queue` 自适应合并；不要手动 `ALTER TABLE ... UNSPLIT` |
| 企业版特性（备份 / 加密）开源不可用 | 接受自研备份管线 / 接 Enterprise / 评估 PolarDB / TiDB |
| BSL 许可阻碍二次发行 | 自用 OK；做 DBaaS 或分发需与 Cockroach Labs 协商 |
| PQ 协议兼容但 PG extension 不全 | `LIST EXTENSION` 看支持范围；PostGIS、pg_partman 等不可用 |
| `pg_dump` 导出后跨节点加载可能顺序不一致 | `IMPORT INTO ... PGDUMP` 自带 secondary index check |

## 二十三、与本项目的关联

- **存储引擎**：`engineering/src/db/storage/`、`engineering/include/db/storage.h` — 是教学级 B+Tree heap；CockroachDB 的 Pebble + MVCC 是"工业级 + 多版本"参考实现，可作为对照参照。
- **SQL 解析器**：`engineering/src/sql/` 实现的极简 SQL parser (`sql_lex.c` / `sql_parse.y`) 与 CockroachDB 的现代优化器对比看，可理解**手写解析 vs LALR/递归下降 + cascades optimizer** 的差距。
- **WAL / Buffer Pool**：`engineering/src/db/wal/` `engineering/src/db/buf/` 也实现了基础 Buffer Pool + WAL，与 CockroachDB 的 Raft log + Pebble WAL 形成层次对照。
- **共识协议**：`phase9-distributed-evolution.md` 已经过一遍 raft.h/c、dist_txn.h/c，本工程仓的 `engineering/src/dist/` 下有简化版 Raft + 2PC 学习实现，可与 CockroachDB 的生产实现对比看简化路径。
- **学习轨对照**：`learning/code-solutions/c/` 有 cockroachdb 的小型 demo 实现（极简化，不含分布式），可作为零基础入门对比。

## 二十四、参考资料

1. Cockroach Labs 官方文档：https://www.cockroachlabs.com/docs/stable/
2. CockroachDB Architecture 概述：https://www.cockroachlabs.com/docs/stable/architecture/overview.html
3. Spanner: Google's Globally Distributed Database (OSDI 2012)：https://research.google/pubs/spanner-googles-globally-distributed-database/
4. In Search of an Understandable Consensus Algorithm (Raft, USENIX ATC 2014)：https://raft.github.io/raft.pdf
5. Pebble: A KV Store Built for CockroachDB（VLDB 2025）：https://www.cockroachlabs.com/techlog/pebble-a-kv-store-built-for-cockroachdb/
6. CockroachDB GitHub：https://github.com/cockroachdb/cockroach
7. CockroachDB BSL 许可 FAQ：https://www.cockroachlabs.com/docs/stable/licensing-faqs
8. Hybrid Logical Clocks：https://www.cockroachlabs.com/blog/hybrid-logical-clocks/
9. Multi-Region in CockroachDB v22.1（Blog）：https://www.cockroachlabs.com/blog/multi-region-an-improved-version-now-generally-available/
10. TPC-C 6.46M tpmC 报道：https://www.cockroachlabs.com/blog/6-46m-tpmc-with-cockroachdb-on-25-nodes/
