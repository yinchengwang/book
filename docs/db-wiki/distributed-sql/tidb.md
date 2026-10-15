# TiDB Wiki（分布式 SQL / NewSQL / HTAP）

> 参考版本：TiDB 仓库当前 commit `6a3e36ccecf0f1863d015528b8db3ea508760ab7`（v8.x 系列）；主要 API 与数据布局在 v6.x 之后保持稳定。

## 一、项目定位

TiDB（PingCAP 出品）是一款**分布式 NewSQL 数据库**，对外提供 **MySQL 兼容协议与方言**，内核采用"计算与存储分层"架构：TiDB Server 无状态 SQL 层 + TiKV 分布式 KV 存储 + PD（Placement Driver）元数据与调度层 + TiFlash 列存副本。其主打场景是 **HTAP（Hybrid Transactional / Analytical Processing）**：同一份数据可被 OLTP 事务读，也可被 TiFlash 列存副本做 OLAP 加速。

- **痛点**：
  - MySQL 单机瓶颈（写入、容量都受限），分库分表又丢失事务与 JOIN；
  - HTAP 场景必须同时承担事务与分析查询，传统做法是 ETL 进 OLAP 仓库，时延以小时计；
  - 现有 NewSQL（如 CockroachDB）侧重 PostgreSQL 兼容，与国内 MySQL 生态对接成本高；
  - 大数据场景下，希望一份数据既能实时增删改查，又能做大规模聚合与列存分析。
- **典型用户**：互联网公司（小米、京东、Bilibili、美团、知乎、网易传媒、虎牙、知乎、Coursera）、金融（建设银行、微众银行、平安科技）、电信运营商、海外 SaaS。

## 二、发展历史与社区

- **起源**：2015 年夏天 PingCAP 三位创始人：刘奇（CEO，前豌豆荚基础设施）、黄东旭（CTO，前微软/360）、崔秋（工程 VP，前网易）。灵感来自 Google Spanner / F1 论文，目标是用 Go + Rust 写一个"同时支持 OLTP 和 OLAP"的 MySQL 协议分布式数据库。
- **里程碑**：
  - 2016 年 6 月开源（Apache 2.0）；
  - 2017 年 GA（1.0）；
  - 2018 年 TiFlash（HTAP 列存层）；
  - 2020 年 4.0（TiDB 通用 planner、TiFlash 成熟、备份恢复增强）；
  - 2021 年 5.0（TiDB 6.0 重写执行框架为 MPP，大量特性转向 session variable）；
  - 2022 年 6.0（TiProxy、TLS 增强）；
  - 2023 年 7.0+（资源管控、资源组、并行 DDL、`tidb_enable_unsafe_substitute` 完善）；
  - 2024 年 8.x（向量检索 placeholder、TiProxy 增强、Placement Rules 改写）；
  - 2025 年（基于本次快照 v8.x 系列向量化执行大规模替换 Volcano、按表并行 adaptive）。
- **维护状态**：活跃；每季度一个大版本（v8、v9 …），每版本支持约 6 个月（活跃）+ 3 个月（仅修复）。
- **社区**：TiDB User Group（北上广深 + 多伦多）、Forum（`ask.pingcap.com`）、GitHub Discussions、Slack。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（最宽松的 OSS 许可）。
  - 来源：`reference/distributed-sql/tidb/LICENSE`
  - 关键条款：允许商业使用、修改、分发、闭源衍生，无需付费；无专利限制、无 copyleft。
- **PingCAP 商业产品 / 服务**：
  - **TiDB Enterprise**：Enterprise 特性（在仓的 `enterprise/` 目录，独立 module），含企业级备份、加密、审计、Placement Rules；
  - **TiDB Cloud**：全托管 DBaaS，AWS / GCP / Azure / AliCloud 都有 region；提供 Serverless（按 RU 计费）+ Dedicated（专用 TiKV + TiFlash 节点）；
  - **TiDB 自支持** vs **PingCAP 支持**：订阅包含官方 SLA、24x7 支持、Hot-fix 通路。
- **企业特性闭源**：`enterprise/`、`pkg/config/`，需 Enterprise License 启动；社区版不包含。

## 四、业界主要使用方与案例

- **互联网 / SaaS**：小米（订单、广告投放链路）、京东（订单流转）、Bilibili（消息与评论）、美团（推荐系统标签）、知乎（问题与回答）；
- **金融**：建设银行（信用卡账务）、微众银行（核心账务部分子系统）、平安科技（投资交易）、中国银行（CRM）；
- **电信 / 政企**：中国移动（云客服）、中国电信（计费）、多家省 BOSS；
- **海外**：Shopee（东南亚电商）、Coursera、Datavise、Hulu。
- **实时数仓 / HTAP**：同盾科技、知乎 KOL 数据、网易传媒点击流。

## 五、一句话对比定位

> TiDB 是**"MySQL 兼容 + 计算存储分离 + HTAP"**的 NewSQL 代表；与 **CockroachDB（PostgreSQL 兼容、Raft 内嵌、Pebble 存储）**、**YugabyteDB（双 API、Cassandra 风格分片）**、**OceanBase（阿里自研、Paxos 多租户）** 一起构成 NewSQL "四强"。

## 六、整体架构

TiDB 采用**分层对等**架构，每一层都可独立水平扩展，由 PD 协调。

### 6.1 总体分层

```
┌────────────────────────────────────────────────────────────────────────┐
│                       客户端 (JDBC / MySQL CLI / Driver)                │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ MySQL Wire Protocol (5.7/8.0)
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    TiDB Server  (无状态 / 多副本对等)                  │
│     pkg/parser  → pkg/planner  → pkg/executor → pkg/store/copr        │
│     - SQL 层全部封装在 TiDB 进程内，可任意起多个                        │
│     - 同一份 SQL 在不同 TiDB 上执行，行为一致                           │
│     - 通过 MySQL 协议接受连接（默认端口 4000）                          │
│     - 元数据通过 PD 获取，缓存到 domain/schema                          │
│                                                                              │
│   ┌────────────┬────────────┬─────────────┐                            │
│   │ TiDB SQL   │  TiFlash    │  TiProxy     │ (可选)                   │
│   │  server    │  SQL node    │  代理层      │                            │
│   └────────────┴────────────┴─────────────┘                            │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ gRPC (Coprocessor / MVCC Get / Put / Txn)
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    存储引擎层 (TiKV + TiFlash)                         │
│   - TiKV：行存（RocksDB + Raft + MVCC），负责 OLTP 与行存扫描           │
│   - TiFlash：列存（DeltaTree + Raft + MVCC），负责 OLAP / MPP         │
│   - 每个 Region 在 2-3 个 TiKV 之间复制（TSoC 协议）                    │
│   - TiFlash 与 TiKV 异步同步，主从延迟可控（百毫秒~秒级）               │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ Raft / PD heartbeat
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    元数据与调度层 (Placement Driver / PD)              │
│     - 集群拓扑 + Region 位置 + 时戳 (TSO)                              │
│     - 调度器：热点均衡、副本重建、Region merge / split                 │
│     - 全局单调递增 TSO 由 PD Leader 集中分配                            │
└────────────────────────────────────────────────────────────────────────┘
```

源码入口：
- SQL 入口：`pkg/executor/adapter.go:1`（`executor.Next`、session 生命周期）
- TiDB ↔ TiKV：`pkg/store/copr/coprocessor.go:1`（Coprocessor 请求协议）
- KV 接口抽象：`pkg/kv/kv.go:1`（Transaction、MemBuffer、Staging）

### 6.2 TiDB Server 进程模型

- **每个 TiDB Server 单进程多 goroutine**：
  - **listener goroutine**：接受 MySQL 协议握手；
  - **session goroutine**：每条客户端连接一个；
  - **distSQL task goroutine**：驱动 Coprocessor 请求、流式返回结果；
  - **DDL worker**：集群内只有一个 TiDB 上跑（通过选主 `OWNED_DDL` Job）；
  - **load stats worker、auto analyze worker**：后台统计信息采集。
- **TiDB Server 无状态**：所有持久状态在 TiKV + PD；TiDB 重启不需要 reload，缓存 schema 直接走 PD。

### 6.3 一条 SQL 的端到端路径

1. **MySQL 协议解析** → `pkg/server/conn.go` 接收 query / prepared statement；
2. **SQL Parser** → `pkg/parser/parser.y`（LALR parser）生成 AST `ast.Node`；
3. **Preprocess + Logical Plan** → `pkg/planner/core/logical_plan_builder.go`；
4. **Optimizer**：
   - 规则优化：`pkg/planner/core/rule_*.go`（column pruning, subquery decorrelation, join reorder）；
   - 代价优化：`pkg/planner/core/optimize.go` + statistics/histogram；
   - 物理算子选择：coprocessor task（推到 TiKV）、TiFlash MPP task（推到 TiFlash）、TiDB Local task；
5. **Executor** → `pkg/executor/*.go`（每个算子一个 struct）；
   - 算子之间用 **chunk.MutRow** 风格的 `executor.Executor` 接口；
6. **结果回传** → `pkg/server/conn.go` 编码为 MySQL ResultSet。

源码入口：`pkg/executor/adapter.go:1`、`pkg/planner/core/planbuilder.go:1`。

## 七、存储引擎

### 7.1 行存：TiKV（RocksDB + Raft + MVCC）

每个 TiKV 节点负责若干 **Region**（默认上限 96 MiB），Region 是**一段连续的 key range**：

```
原始 SQL:  INSERT INTO kv(k, v) VALUES ('user:1', 'alice');

编码 key (TiKV):
  't' (table prefix) '1' (table id) 'r' (row) '1' (handle)
       → 最终 key: 0x74810000000000000000 (binary prefix) _user:1_?
       → 真实原始 key: 't_58_r_1' (handler encoding)
       → TiKV 内部实际: zAdd tablePrefix {table_id} recordPrefixSep {handle} {col_id}
       → RocksDB 落盘: key + commit_ts (default CF) → MVCC 版本
```

- **MVCC Key**：`{raw_key}_{commit_ts}_{txn_type}` —— RocksDB 中以 `[key, ts)` 序列实现版本控制；
- **RocksDB**：每个 Region 一个独立的 RocksDB instance（或 share RocksDB + SST 分桶）；
- **Raft 复制**：每个 Region 在 N（默认 3）个 TiKV 上有副本，由 Region 的 Raft group 维持一致性。
- **关键源文件**：`pkg/store/tikv/kv.go:1`（Txn KV 接口）、`pkg/store/copr/coprocessor.go:1`（Coprocessor 流式扫描）。

### 7.2 列存：TiFlash（DeltaTree + Raft）

TiFlash 是**列式副本**，同样基于 Raft 同步（主从 TiKV → TiFlash raft learner）：

- **底层列存**：`DeltaTree`（与 ClickHouse / DuckDB 列存思路类似：列存 + delta + 主压缩）；
- **写入路径**：TiKV 上的 primary 副本写入后，raft log 同步到 TiFlash learner；TiFlash apply 时**不重做 RocksDB**，直接转列存；
- **读取路径**：TiDB 把分析型查询下推到 TiFlash，由 TiFlash 的 `mpp` worker 完成分布式 join / aggregate。

源码（v8 起已合并到主仓）：
- `pkg/distsql/mpp/`（MPP 框架）
- `pkg/sessionctx/variable/`（`tidb_allow_mpp`, `tidb_enforce_mpp` 等开关）

> 注：本快照仓库未含 TiFlash 的 C++ 源码（历史上 TiFlash 在 `tiflash` 仓库，v8.0 后正逐步合并到主仓）。本文涉及 TiFlash 的描述以**主仓 + 官方文档**为准。

### 7.3 表 → Region 映射

```
[TableInfo]
  handle = pk encoded via (handle_id | pk_columns)
  row prefix = t{tableID}_r
  index prefix = t{tableID}_i{indexID}

[Range] = [startKey, endKey)
  startKey = row_prefix{handle min} | index_prefix{...}
  endKey   = 物理分片边界（96 MiB）
```

- **Region Split** 由 PD 调度：基于 Region 上的吞吐与容量阈值，自动 split / merge。

## 八、索引机制

TiDB 通过把**主键编码 + 二级索引**沿用 MySQL 语义，但放到 RocksDB：

| 索引类型 | 实现 | 备注 |
|----------|------|------|
| **Primary Key (CLUSTERED)** | 表数据按 PK 顺序存储（RocksDB key 物理有序） | 默认 v5 起 TiDB 用 clustered index |
| **Secondary Index** | 索引列 + handle 编码 | 通过 handle 回查主键数据 |
| **Unique Index** | 唯一性校验通过 unique index + lock | 与 MySQL 一致 |
| **Composite Index** | 多列组合 prefix | 最左前缀匹配 |
| **Expression Index** | 函数计算结果 | MySQL 8 兼容 |
| **Multi-valued Index** | JSONB / Array 上的 secondary | 适配部分工作负载 |
| **Vector Index** | v8 起 placeholder（HNSW 实现中） | 同质与 TiDB 配合的 vector search |

主键约束：
- `INT PRIMARY KEY` 自动 clustered；
- `VARCHAR PRIMARY KEY` 用 hidden handle（强制 6 字节 auto id + PK 编码 payload）；
- `PRIMARY KEY NONCLUSTERED` 显式选择非聚簇索引（v5 引入）。

源码：`pkg/table/tables/tables.go:1`（表数据编码）、`pkg/ddl/index.go:1`（索引创建）。

## 九、查询 / 计算 / 接口层

### 9.1 SQL 方言与 MySQL 兼容性

TiDB 的 SQL 目标是**与 MySQL 5.7/8.0 兼容**，包括：

- **协议层**：MySQL Wire Protocol（prepare / execute / multi-resultset）；
- **语法层**：基本 DDL/DML、`SHOW`、`INFORMATION_SCHEMA`、`performance_schema`、用户 / 权限（mysql.user）；
- **函数层**：字符串、数值、日期、控制流、窗口函数、CTE、JSON 函数（与 MySQL 8 对齐）；**PostgreSQL 扩展** 明确不支持。

> 已知差异（受分布式一致性影响）：
> - 没有 `XA`、`XA PREPARE`（分布式事务切走）；
> - 没有 `LOCK TABLES`（事务语义不同）；
> - 自增 ID 不保证完全连续；
> - `SELECT ... FOR UPDATE` 跨节点走 Percolator lock，延迟较高。

### 9.2 优化器：Cascades 风格

TiDB 优化器演化路径：

- **v3 之前**：基于规则的简单优化器；
- **v3 - v5**：Cost-Based Optimizer (CBO) + Rule + Cost 优化，规则相对静态；
- **v5 之后**：引入 Cascades 风格（rule exploration + cost evaluation with memo）；
- **v7+**：基于 Cascades + 更细粒度的物理算子代价模型。

核心文件：`pkg/planner/core/casPlanner/cascades.go`、`pkg/planner/core/rule_*.go`。

### 9.3 执行模式：Volcano → MPP / Vectorized

- **Volcano 传统**（默认）：每个算子一个 `Next()` 调用，拉模式；
- **向量化**：`pkg/executor/internal/vec` 起，v7+ 在 TableReader / IndexReader / Projection / Selection 上向量化；
- **MPP（TiFlash）**：通过 `pkg/distsql/mpp/` 把可下推的 join / aggregate 直接生成 MPP fragmentation，分发到 TiFlash 节点并行执行；
- **Coprocessor（推到 TiKV）**：范围扫描、点查、简单 filter 通过 `pkg/store/copr/` 推到 TiKV。

源码：`pkg/executor/builder.go:1`（执行计划构建）、`pkg/store/copr/coprocessor.go:1`。

### 9.4 接口与生态

- **MySQL 客户端**全部可用：mysql CLI、JDBC Connector/J、Python `PyMySQL` / `mysqlclient`、Go `database/sql` + `go-sql-driver/mysql`、Node `mysql2`；
- **ORM**：GORM、Sequelize、Hibernate、SQLAlchemy 等原生兼容；
- **生态集成**：DBeaver / DataGrip / TablePlus 等 GUI；TiDB Dashboard（8080 端口）自带监控；
- **数据迁移**：TiDB DM（Data Migration，从 MySQL/MariaDB 增量同步）、TiDB Loader（CSV / SQL 导入）、TiDB Lightning（物理导入，绕过 SQL 层直接生成 SST）；
- **TiDB Binlog / TiCDC**：变更数据捕获 → Kafka / Pulsar / MySQL / 文件；
- **备份归档**：BR (Backup & Restore)，物理全量 + 增量备份到 S3 / GCS / NFS。

## 十、事务与一致性

### 10.1 隔离级别

| 隔离级别 | 是否支持 | 说明 |
|----------|----------|------|
| READ UNCOMMITTED | 支持 | 实际等同 READ COMMITTED |
| READ COMMITTED | 支持（v3+ 引入） | 与 MySQL RC 一致（基于 statement snapshot） |
| REPEATABLE READ | **默认** | snapshot isolation（MySQL RR 语义） |
| SERIALIZABLE | 不直接支持 | 但因 2PC + 默认 RR + unique key check，整体接近 Serializable |
| SNAPSHOT | 实验性 | `tidb_snapshot` session var |

### 10.2 分布式事务：Percolator 风格 2PC

TiDB（与 TiKV）走的是**Percolator 风格**的分布式事务，**与 CockroachDB 截然不同**：

```
client → TiDB Server → TxnBegin
        ├── 每个 write：Prewrite 时
        │     • 在 TiKV primary 上写 data + lock (含 txn_id)
        │     • 在 primary lock 同一行写 1 字节 primary 标识
        ├── Commit：两阶段
        │     • Phase 1：commit_ts 通过 PD TSO 获取（在 TiKV 上一致）
        │     • 把 primary lock 升级为 commit record，data 写入带 commit_ts
        │     • Phase 2：异步把 secondaries lock 升级为 commit
        │     • 失败：lock TTL 超时后异步回滚
        └── 读：scan 时跳过 lock；遇 primary lock 主动推进 2PC
```

关键点：
- **TSO 由 PD 单调分配**（物理时钟 + 序号），所有 commit_ts 单调；
- **2PC 内化为 Percolator**：避免显式 prepare/coordinator 阻塞；
- **dead lock detection**：TiKV 上 lock TTL 默认 60 秒（`txn-lock-timeout`），超时调度器回滚；
- **read consistency**：`tidb_replica_read = leader / follower / prefer-leader`，可走 follower。

源码：`pkg/store/tikv/txn.go:1`、`pkg/store/tikv/2pc.go:1`、`pkg/store/tikv/lock_resolver.go:1`。

### 10.3 Raft

- **TiKV Region Raft**：每个 Region 一个 Raft group；默认 3 副本；
- **Raft 实现**：`components/tikv`（TiKV C++ 仓库）使用 `raft-rs`（PingCAP 自研 Rust Raft）；
- **Raft learner**：TiFlash 节点以 learner 加入主 Region 的 Raft group，async apply。

> 注：TiDB 仓库仅含 Go 客户端代码；具体 Raft 实现位于姐妹仓 `tikv`（Rust）。本节基于官方文档理解。

### 10.4 时间戳源（TSO）

- **PD TSO**：全局单调递增 64-bit timestamp（物理 ms + 32-bit logical）；
- PD Leader 集中分配，避免节点间时钟漂移导致事务乱序；
- 故障时通过 follow-the-leader + PD 微集群（默认 3 节点）保护 TSO 可用性。

源码：`pkg/store/tikv/oracle/oracle.go:1`（client 侧 TSO 接口）。

## 十一、复制与高可用（深度扩展）

TiDB 集群的 HA 建立在这三层：**PD 高可用 → TiKV Raft 副本 → TiDB Server 无状态**。

### 11.1 PD 高可用

PD 至少 3 节点（推荐 5 节点），通过 etcd-raft 选举 Leader：

- 单 PD Leader 挂掉 → 短暂不可用（最多 seconds 级）；
- TSO 是依赖 PD 的唯一线性化点；
- PD 也负责**调度**：实时监控 Region 大小、QPS、节点负载，进行 split / merge / move-leader / add-replica。

源码（PD 仓内），本文据 `pkg/store/tikv/pd/` 客户端对接代码。

### 11.2 TiKV 多副本

- **Region Raft group**：每个 Region 默认 3 副本，副本位置由 PD 的 **Placement Rules** 决定；
- **故障域**：PD 支持机房、机架、AZ、DC 标签；通过 placement 配置表达约束；
- **副本变更**：joint consensus，原子切换；
- **副本恢复**：diskrecovery / online unsafe recovery（v7+ 提供工具）。

### 11.3 多区域容灾（Multi-Region / DR）

TiDB 不像 CockroachDB 那样把"多区域"做成 SQL 层抽象，但通过**Placement Rules + DR 站点** 可实现：

```
数据节点地理分布:
  Region A: TiKV(3) + PD(1) + TiDB(2)
  Region B: TiKV(3) + PD(1) + TiDB(2)   ← DR 站点
  Region C: TiFlash(2)                    ← AP/分析只读副本

通过 Placement Rules:
  PARTITION BY HASH(country) PARTITIONS 4
  ALTER TABLE orders ALTER PARTITION p_us RANGE LOCATION LABELS = ['region','us'] = 'a';
```

- **同城主备**：`schedule.replicate-scheduler` + `schedule.constraint` 强制跨 AZ；
- **跨城 DR**：通过异步 binlog / TiCDC 同步到另一站点的 TiDB（同 schema、同 location）；
- **异地多活**：通过 TiCDC + 全局时间窗口冲突检测（社区方案）。

源码：`pkg/ddl/placement/`、`enterprise/pkg/placement/`.

### 11.4 故障恢复与自愈

- TiKV 节点失联：PD 在 miss-heartbeat 阈值后（默认 60s）主动把副本按 placement 规则补齐；
- Region 副本缺额 → 自动追加 learner → 升级 voter；
- Region 热点 → 自动 split + balance；
- 数据损坏 → 在线修复工具 `tikv-ctl recover`（v6.6+）支持在线 unsafe recovery。

## 十二、扩展性（深度扩展）

### 12.1 分片策略

- **Range-based**：TiKV 把表按 PK 顺序切片，每个 Region 是一段连续 key range（默认 96 MiB / `region-max-size`）；
- **Hash-based Partition**：用户层用 `PARTITION BY HASH(col)` 让写入打散；
- **Composite Range Partition**：组合 hash + range，PD 不直接管，由 DDL 表达；
- **Region Merge**：冷数据相邻 Region 自动合并。

### 12.2 水平扩容流程

```
1. 部署新 TiKV/PD/TiDB 节点
2. update-monitor / scale 操作调集群拓扑
3. PD 自动调度 Region 把副本迁移到新节点
4. 在线 ALTER TABLE ... REORG PARTITION 控制热点迁移
```

### 12.3 在线 Schema Change (DDL)

TiDB 把所有 DDL 走统一框架：

```
DDL Job (插入 mysql.ddl_jobs)
  → 通过 etcd-raft elect owner
  → 进入 BACKFILL / REORG 阶段：
      - Add Index：按 PK 顺序扫表，把 index row 写 TiKV
      - Modify Column：分批 update，double write
      - 分区与 online state（public → write-only → delete-only → absent）
  → ON DONE 通过 schema version (schemaVersion++) 推动所有 TiDB Server 切版本
```

源码：`pkg/ddl/ddl.go:1`、`pkg/ddl/scheduler/scheduler.go`、`pkg/ddl/executor/executor.go`。

### 12.4 资源管控

v6.6+ 引入 **Resource Control**：

- `CREATE RESOURCE GROUP` 定义租户 / 业务组；
- session variable `tidb_resource_group = 'low' / 'high' / 'oltp' / 'olap'`；
- TiDB Server / TiKV 协同按 RU (Request Unit) 配额调度；
- 高负载业务可对低优先级业务降级。

源码：`pkg/resourcegroup/`、`enterprise/pkg/rm`.

### 12.5 副本策略：Placement Rules

通过 SQL/PD 配置表达副本分布：

```sql
ALTER TABLE orders ALTER PARTITION p_us
  PLACEMENT POLICY = 'us_west_only_3';
```

策略支持：
- 副本数；
- 强制机房 / 机架 / AZ 标签；
- leader 强制候选角色；
- follower 数限制。

## 十三、性能与基准

### 13.1 Sysbench OLTP

- 单 TiDB 节点：OLTP insert 30k TPS，OLTP point select 100k QPS；
- 随 TiKV 节点增加写入 / 读吞吐近似线性；
- v7+ 向量化使 OLTP point select 在大集群上比 v5 高 30-50%。

### 13.2 TPC-C / TPC-H

- **TPC-C**：TiDB 官方公布 v7.5 在 100 TiKV 节点达到 ~1500 万 tpmC；
- **TPC-H**：TiFlash MPP 在 30 节点集群跑 1TB TPC-H 显著优于 TiDB row-only；
- **CH-benchmark**：与 ClickHouse / Doris / StarRocks 同档，对标产品。

### 13.3 延迟分布

- 跨 AZ 写入 P99：20-100 ms；
- 点查 P99：1-5 ms（命中本地副本）；
- 范围扫描 P99：10-100 ms（依赖 Region 数 + pushdown）。

## 十四、运维与可观测性

### 14.1 TiDB Dashboard

启动后访问 `http://<tidb>:2379/dashboard`：

- **Overview**：QPS、延迟、错误率、版本、拓扑；
- **SQL Statements**：topN 慢 SQL；
- **Slow Queries**：含 plan、绑定变量、KV 请求；
- **Key Visualizer**：热点 key；
- **Diagnostics**：日志、profile；
- **Cluster Info**：PD/TiKV/TiDB/TSO 状态。

### 14.2 备份恢复 (BR)

```bash
tiup br backup full -s "s3://br-backup/2025" --pd "<pd_endpoint>"
tiup br restore full -s "s3://br-backup/2025" --pd "<pd_endpoint>"
```

- 全量备份 + **log backup**（持续 PITR）— v6+；
- 单表 / 单库 / 集群粒度；
- BR UI 与命令行双模式。

源码：BR 是独立仓 `tikv/br`，仓库内可参考 `enterprise/pkg/backup/`.

### 14.3 TiCDC

TiCDC 通过读取 TiKV 的 raft log 流式输出到下游：

- 输出格式：Kafka（Avro / JSON / Canal）、MySQL、Pulsar、文件；
- 顺序保证：按 Region 与 commit_ts 顺序；
- 灾备与多活核心工具。

源码：`tidb-binlog`（旧）、`ticdc`（独立仓）。

### 14.4 Prometheus / Grafana

- 自带 `_status` 子命令导出 metrics；
- 官方 Grafana Dashboard JSON 在 `metrics/grafana/`。

## 十五、生态与集成

### 15.1 客户端 SDK

| 语言 | 驱动 | 备注 |
|------|------|------|
| Go | `go-sql-driver/mysql` | 通过 `?parseTime=true` 即可 |
| Java | MySQL Connector/J | `mysql-connector-j` 8.x |
| Python | `PyMySQL`、`mysqlclient`、`mysql-connector-python` | 标准驱动 |
| Node.js | `mysql2` | `mysql2/promise` 也常用 |
| Rust | `mysql_async`、`sqlx-mysql` | 异步友好 |
| PHP | `PDO_MySQL` | 兼容 |

> 推荐：与 MySQL 协议完全兼容，任何能连 MySQL 的客户端都可以连 TiDB。

### 15.2 数据迁移工具

- **TiDB Data Migration (DM)**：从 MySQL/MariaDB 实时增量同步 + 全量初始同步；
- **TiDB Lightning**：把大表通过物理 SST 导入（绕过 SQL 层，速度可达 100GB/hr）；
- **TiDB Dumpling**：导出全量（替代 mysqldump）；
- **Sync-Diff-Inspector**：上下游一致性比对。

### 15.3 Kubernetes Operator

`pingcap/tidb-operator` 提供：

- 声明式部署（CRD: TidbCluster）；
- 自动故障恢复（HPA、备份、扩缩容）；
- TLS 证书管理；
- 监控（基于 Prometheus Operator）。

### 15.4 周边生态

- **CDC → Kafka → Flink / ClickHouse / Doris** 是常见链路；
- **DBeaver / DataGrip / Navicat / TablePlus** 均支持；
- **BI 工具**：Tableau / Power BI 通过 MySQL 协议可直连；
- **大数据**：与 Spark / Flink 集成有官方 connector。

## 十六、核心论文与算法原理

- **Spanner 论文**：James C. Corbett et al., OSDI 2012 — TiDB 设计的核心参考。
- **Percolator 论文**：Dewey et al., OSDI 2010 — TiDB 的 2PC 实现核心（与 CockroachDB 风格对比）。
- **Cascades 优化器论文**：Freytag et al., 1994 — TiDB v5+ 优化器的实现参考。
- **Raft 论文**：Diego Ongaro, 2014 — Raft 实现基础；`raft-rs` 来自 PingCAP 自研 Rust 实现。
- **向量索引相关（v8+）**：HNSW（Hierarchical Navigable Small World）论文（Malkov & Yashunin, 2016）。
- **DeltaTree**（TiFlash）：与 LSM 列存思想类似，参考自 ClickHouse / DuckDB。

## 十七、源码学习路径

### 17.1 推荐阅读顺序

1. **Top-level README + docs/agents/architecture-index.md**（仓库官方指引）；
2. **`pkg/parser/parser.y`** — LALR 文法入口；
3. **`pkg/planner/core/logical_plan_builder.go`** + **`pkg/planner/core/casPlanner/`** — 优化器；
4. **`pkg/executor/adapter.go`** + **`pkg/executor/builder.go`** — 执行器；
5. **`pkg/store/tikv/txn.go`** + **`pkg/store/tikv/2pc.go`** — 分布式事务；
6. **`pkg/store/copr/coprocessor.go`** — Coprocessor 下推；
7. **`pkg/ddl/ddl.go`** + **`pkg/ddl/executor/`** — DDL；
8. **`pkg/infoschema/infoschema.go`** — 缓存 schema；
9. **`pkg/session/`** — Session 生命周期；
10. **`pkg/statistics/`** + **`pkg/planner/cardinality/`** — CBO 与统计信息。

### 17.2 关键文件清单

- `pkg/parser/parser.y:1`
- `pkg/planner/core/planbuilder.go:1`
- `pkg/planner/core/casPlanner/cascades.go:1`
- `pkg/executor/adapter.go:1`
- `pkg/executor/builder.go:1`
- `pkg/kv/kv.go:1`
- `pkg/store/tikv/kv.go:1`
- `pkg/store/tikv/txn.go:1`
- `pkg/store/tikv/2pc.go:1`
- `pkg/store/copr/coprocessor.go:1`
- `pkg/store/tikv/oracle/oracle.go:1`
- `pkg/store/tikv/lock_resolver.go:1`
- `pkg/ddl/ddl.go:1`
- `pkg/ddl/index.go:1`
- `pkg/table/tables/tables.go:1`
- `pkg/infoschema/infoschema.go:1`
- `pkg/session/session.go:1`

### 17.3 关键测试文件

- `pkg/parser/parser_test.go`
- `pkg/planner/core/casPlanner/testdata`
- `pkg/store/tikv/txn_test.go`
- `pkg/store/tikv/2pc_test.go`
- `pkg/store/copr/coprocessor_test.go`
- `pkg/ddl/ddl_test.go`
- `tests/realtikvtest/`（需要本地 playground）

## 十八、源码规模与开发语言

- **语言**：TiDB 主仓 99% Go（含 Calcite-style SQL planner、parser、executor）；
- **代码规模**：master 分支约 130 万行 Go；
- **构建系统**：Make + Go modules（同时也支持 Bazel，`make bazel_prepare` 后构建）；
- **依赖管理**：Go modules（`go.mod` / `go.sum`）；
- **格式化**：`gofmt -s -w` + import group 规范；
- **测试**：标准 testing + 录制式 integration test (`tests/integrationtest/`) + RealTiKV test (`tests/realtikvtest/`)。

## 十九、学习资源

- **官方文档**：`https://docs.pingcap.com/`
- **PingCAP Blog**：含大量 deep-dive 与版本特性解读；
- **PingCAP University**（前 TiDB University）：免费认证课程；
- **Forum**：`https://ask.pingcap.com/`；
- **GitHub Discussions**：`https://github.com/pingcap/tidb/discussions`；
- **Conference Talks**：FOSDEM、ArchSummit、Gopher China、SIGMOD；
- **书籍**：《TiDB 6.x 原理与实践》、《大规模分布式存储系统》等可作背景；
- **视频**：PingCAP YouTube 频道、TUG Meetup。

## 二十、可移植性与部署

- **二进制**：`tidb-server` 静态 / 动态链接；Linux / macOS / Windows；
- **Docker**：`pingcap/tidb:latest`、`pingcap/tikv:latest`、`pingcap/pd:latest`；
- **Kubernetes**：`pingcap/tidb-operator` 官方 operator；
- **平台**：Linux x86_64 / arm64、macOS x86_64 / arm64；Windows 仅 dev。
- **TiUP**：PingCAP 推出的本地多组件管理工具，`tiup playground` 一键拉起开发集群；
- **嵌入式**：否，TiDB 天然是分布式系统，不建议嵌入式。

## 二十一、变体与衍生项目

- **TiDB Enterprise**：闭源扩展（备份 / 审计 / Placement Rules / Resource Control）；
- **TiDB Cloud**：全托管 DBaaS（Serverless / Dedicated）；
- **TiDB Open Source Community** vs Enterprise：Apache 2.0 分支对应社区版；
- **新合并项目**：自 v8 起 TiFlash 正在合并到主仓，未来单仓即可构建。
- **学术衍生**：AnalyticsDB、Cloud-TiDB 等内部研究分支。
- **API 兼容**：MySQL 完全兼容，部分兼容 MariaDB、TiDB 自身不做 MariaDB 兼容。

## 二十二、风险与陷阱

| 风险 | 缓解 |
|------|------|
| **Percolator 死锁**：高并发下 data 与 primary lock 顺序错乱 | 设置合适 `txn-retryable` 策略；监控 `tikv_thread_io_writer_longrun` |
| **大 DDL 阻塞**：ADD INDEX 在千万级表上可能耗时小时 | 用 multi-schema / parallel DDL；分批 ALTER |
| **Region hotspot**：单调主键（自增 ID）会全部打到一个 Region | 用 `SHARD_ROW_ID_BITS`；或在写入层 hash |
| **MPP 内存溢出**：TiFlash MPP 估算不准可能 OOM | 调整 `tidb_distsql_scan_concurrency`、`mpp_mem_limit` |
| **CDC 延迟升高**：TiCDC 在大流量下积压 | 增加 `sink-uri` worker；监控 `ticdc_owner_checkpoint_ts` |
| **资源控制误配**：`CREATE RESOURCE GROUP` 没写默认 fallback | 设置 `tidb_resource_group_name` + `default RU` |
| **MySQL 函数不兼容**：GROUP BY 中字段带表达式 + alias 报错 | 参考官方 compatibility 列表；先用 `tidb_enable_no_op_functions` |
| **DM 同步延迟**：binlog 格式限制 | 用 `ROW` 格式 binlog；配置足够 `worker-count` |
| **大事务**：`txn-entry-size-limit` 与 `txn-total-size-limit` 拒绝 | 拆事务；调整阈值 |
| **集群时钟回拨**导致 TSO 暂停 | 启用 NTP / chrony 同步，避免时钟跳变 |

## 二十三、与本项目的关联

- **存储引擎**：`engineering/src/db/storage/` 中的 heap 引擎、btree 引擎；与 TiKV 的 RocksDB + MVCC 实现可形成"教学级 vs 工业级"对照。
- **SQL 解析器**：`engineering/src/sql/sql_yacc.y`、`engineering/src/sql/parser.c` 是手写教学解析器；与 TiDB 的 LALR(C) 自动生成解析器对比。
- **KV / MVCC**：`engineering/src/db/kv.h` 的极简 KV 实现 vs TiDB 的 RocksDB + Percolator。
- **事务**：`engineering/src/db/tx.h` 的本地事务 vs TiDB 的 2PC + TSO。
- **共识协议**：`phase9-distributed-evolution.md` 已写过 `engineering/src/dist/` 下的 raft + 2PC 教学实现；与 TiDB 的 raft-rs / Percolator 工业实现对比。
- **学习轨**：`learning/code-solutions/c/` 有 TiDB 极简化 demo（仅 KV 与 SQL 解析），可作为零基础入门；`learning/notes/` 中也含分布式事务章节。

## 二十四、参考资料

1. TiDB 官方文档：https://docs.pingcap.com/
2. TiDB 内部系列博客：https://en.pingcap.com/blog/
3. Spanner (OSDI 2012)：https://research.google/pubs/spanner-googles-globally-distributed-database/
4. Percolator (OSDI 2010)：https://research.google/pubs/large-scale-incremental-processing-using-distributed-transactions-and-notifications/
5. Raft (USENIX ATC 2014)：https://raft.github.io/raft.pdf
6. Cascades (Freytag et al., 1994)：https://15721.courses.cs.cmu.edu/spring2016/slides/13-history.pdf
7. TiDB GitHub：https://github.com/pingcap/tidb
8. TiKV GitHub：https://github.com/tikv/tikv
9. PD GitHub：https://github.com/tikv/pd
10. TPC-C / HTAP 基准：PingCAP 公开基准报告（搜索 "TPC-C TiDB benchmark"）。

