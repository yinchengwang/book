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

---

## 二十五、CBO Cost Model Ver2 三维定价与 Cascades 搜索入口

§9.2 仅以「Cascades 风格 + 规则/代价」一句话概述了 TiDB 优化器演进，本节把最核心的两个 piece 拆出来：Cascades Optimizer 的入口与备忘录驱动方式 + Cost Model Ver2 的 16 维 factor + 算子级 cost 公式。

### 25.1 Cascades Optimizer 入口（`Optimizer.Execute`）

TiDB v8 起把 Cascades 入口收敛到 `pkg/planner/cascades/` 包，区别于早期分散的 `pkg/planner/core/casPlanner/`：

```go
// reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:29
type Optimizer struct {
    logic corebase.LogicalPlan   // 入口 logical plan（来自 logical_plan_builder）
    ctx   cascadesctx.Context    // memo + scheduler + rule mask
}

// NewOptimizer 用 logical plan 初始化 memo，并把首个 OptGroupTask 推入调度栈。
// reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:35
func NewOptimizer(lp corebase.LogicalPlan) (*Optimizer, error) {
    cas := &Optimizer{ logic: lp, ctx: NewContext(lp.SCtx()) }
    ge, err := cas.ctx.GetMemo().Init(lp)        // 1) 把初始 expression 入 memo group
    cas.ctx.GetScheduler().PushTask(task.NewOptGroupTask(cas.ctx, ge.GetGroup()))
    return cas, err
}

// Execute 由顶层入口（physical_optimize.go）调用。
// reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:51
func (c *Optimizer) Execute() error {
    return c.ctx.GetScheduler().ExecuteTasks()   // 2) 跑 task 直到栈空
}
```

关键事实：
- **Memo = Group + GroupExpr**：每个 Group 表示一组等价 expression（同样的 logical output）；GroupExpr 是某个 operator 在某 Group 上的实例化；
- **Task 调度**：`pkg/planner/cascades/task/task_scheduler.go` 的 `SimpleTaskScheduler` 用 stack 跑 OptGroupTask → OptGroupExpressionTask → ApplyRuleTask；
- **Rule mask**：`Context.ruleMask` 通过 `bitset.New(uint(rule.XFMaximumRuleLength)).SetAll()` 全部启用，配合 `SetRules(ids)` 让用户用 hint 关闭某些变换；
- **退出条件**：当 scheduler 无可执行 task 时停止，物理化阶段从 memo 中抽出最低代价 plan。

源码：`reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:29-72`、`reference/distributed-sql/tidb/pkg/planner/cascades/memo/memo.go`、`reference/distributed-sql/tidb/pkg/planner/cascades/task/task_opt_group.go`。

### 25.2 Cost Model Ver2 16 维 factor 表

TiDB v6 起启用 Cost Model Ver2（`tidb_cost_model_version=2`），把所有 cost 用「单位 × factor」的三维定价（CPU / 内存 / 磁盘 + 存储节点相关 network/scan factor）：

```go
// reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1284
var defaultVer2Factors = costVer2Factors{
    TiDBTemp:                {Name: "tidb_temp_table_factor",   Value: 0.00},
    TiKVScan:                {Name: "tikv_scan_factor",         Value: 40.70},
    TiKVDescScan:            {Name: "tikv_desc_scan_factor",    Value: 61.05},
    TiFlashScan:             {Name: "tiflash_scan_factor",      Value: 11.60},
    TiDBCPU:                 {Name: "tidb_cpu_factor",          Value: 49.90},
    TiKVCPU:                 {Name: "tikv_cpu_factor",          Value: 49.90},
    TiFlashCPU:              {Name: "tiflash_cpu_factor",       Value: 2.40},
    TiDB2KVNet:              {Name: "tidb_kv_net_factor",       Value: 3.96},
    TiDB2FlashNet:           {Name: "tidb_flash_net_factor",    Value: 2.20},
    TiFlashMPPNet:           {Name: "tiflash_mpp_net_factor",   Value: 1.00},
    TiDBMem:                 {Name: "tidb_mem_factor",          Value: 0.20},
    TiKVMem:                 {Name: "tikv_mem_factor",          Value: 0.20},
    TiFlashMem:              {Name: "tiflash_mem_factor",       Value: 0.05},
    TiDBDisk:                {Name: "tidb_disk_factor",         Value: 200.00},
    TiDBRequest:             {Name: "tidb_request_factor",      Value: 6000000.00},
    ANNIndexStart:           {Name: "ann_index_start_factor",   Value: 0.000144},
    ANNIndexScanRow:         {Name: "ann_index_scan_factor",    Value: 1.65},
    ANNIndexNoTopK:          {Name: "ann_index_no_topk_factor", Value: math.MaxUint64},
    InvertedIndexSearch:     {Name: "inverted_index_search_factor", Value: 139.2}, // (8 + 4) * TiFlashScan
    InvertedIndexScan:       {Name: "inverted_index_scan_factor",   Value: 1.5},
    LateMaterializationScan: {Name: "lm_scan_factor",          Value: 1.5},
}
```

关键观察（取自源码注释 + 经验值）：
- **TiKV Scan (=40.7) > TiFlash Scan (=11.6) ~3.5×**：反映 TiFlash 列存单位成本低于 TiKV 行存 + Raft 开销；
- **TiDB CPU = TiKV CPU = 49.9**：CPU 单价在 TiDB 与 TiKV coprocessor 一致（都是标量算子 cost），但执行次数由 mem 网络放大决定；
- **TiFlash CPU = 2.4 vs TiDB/TiKV CPU = 49.9 ~21× 差**：TiFlash MPP 走列存向量化，每行计算量小；
- **TiDBRequest = 6,000,000**：每个 RPC 请求都极重（含 protobuf 序列化 + gRPC round-trip），对应 `tidb_distsql_scan_concurrency` 调度；
- **TiKVDescScan = 1.5× TiKVScan**：反向扫描需要 reverse iterator，引入额外开销；
- **ANNIndexNoTopK = MaxUint64**：HNSW 不带 top-k 视为不可接受，强制走 fallback（`tidb_enable_vector_search=ON` 时除外）。

源码：`reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1284-1306`。

### 25.3 factor 三维路由（任务类型 × StoreType）

Cost 公式并非单一常量，而是按 task type + store type 二维路由：

```go
// reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1330
func getTaskScanFactorVer2(p base.PhysicalPlan, storeType kv.StoreType, taskType property.TaskType) costusage.CostVer2Factor {
    if isTemporaryTable(getTableInfo(p)) {     // 1) 临时表优先
        return defaultVer2Factors.TiDBTemp
    }
    if storeType == kv.TiFlash {               // 2) storeType 直配 TiFlashScan
        return defaultVer2Factors.TiFlashScan
    }
    switch taskType {
    case property.MppTaskType:                 // 3a) 任务类型 MppTaskType → TiFlash
        return defaultVer2Factors.TiFlashScan
    default:                                  // 3b) 默认 TiKV（按 desc 区分）
        desc := isPhysicalDesc(p)
        if desc {
            return defaultVer2Factors.TiKVDescScan
        }
        return defaultVer2Factors.TiKVScan
    }
}
```

CPU、内存、network、request 全部按同样模式分发：

```go
// reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1308
func getTaskCPUFactorVer2(_ base.PhysicalPlan, taskType property.TaskType) costusage.CostVer2Factor {
    switch taskType {
    case property.RootTaskType:   return defaultVer2Factors.TiDBCPU   // TiDB 侧 root task
    case property.MppTaskType:    return defaultVer2Factors.TiFlashCPU // TiFlash MPP
    default:                      return defaultVer2Factors.TiKVCPU   // TiKV coprocessor
    }
}
```

`getTaskMemFactorVer2` / `getTaskNetFactorVer2` / `getTaskRequestFactorVer2` 同形；network factor 还会按 `PhysicalExchangeReceiver` / `PhysicalTableReader.ExchangeSender` 区分 TiDB→TiFlash MPP 走 `TiDB2FlashNet`，TiDB→TiKV Cop 走 `TiDB2KVNet`。

源码：`reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1308-1375`。

### 25.4 算子级 cost 公式（selection / scan / late-materialization）

每种物理算子都有自己的 cost 函数，统一从 `getPlanCost → p.GetPlanCostVer2` 分派：

| 算子 | 公式（来源：`plan_cost_ver2.go:65-260`） | 关键常量 |
|------|----------------------------------------|----------|
| **PhysicalSelection** | `cost = child_cost + input_rows × filter_factor` | `filterCostVer2` 用 `cpuFactor` |
| **PhysicalProjection** | `cost = child_cost + proj_cost / concurrency` | `projCost = input_rows × len(expressions) × cpu_factor`，`concurrency = ProjectionConcurrency` |
| **PhysicalTableScan (TiKV)** | `cost = rows × log2(row_size) × scan_factor`（见 `plan_cost_ver2.go:128` 注释） | `scan_factor` 来自 `getTaskScanFactorVer2`，全表扫加 `getTableScanPenalty` |
| **PhysicalTableScan (TiFlash)** | 同上 + `TiFlashStartupRowPenalty=10000`（v6 起的小查询反偏好，促小查询走 TiKV） | `plan_cost_ver2.go:123` |
| **Late-materialization (IndexLookUp)** | `lm_row_size × scan_factor × total_rows + rest_size × scan_factor × rows × late_mat_factor` | `late_mat_factor = LateMaterializationScan = 1.5` |
| **InvertedIndexSearch** | `rows × inverted_index_search_factor` (= 139.2 = (8+4)×TiFlashScan) | 文档检索专用 |

反偏好设计（`plan_cost_ver2.go:123-126`）：

```go
const (
    MinNumRows               = 1.0    // 防止选择率近零时 plan 难区分
    MinRowSize               = 2.0    // 防止 row_size < 2 时退化
    TiFlashStartupRowPenalty = 10000  // 强制 TiFlash 用于大查询
    MaxPenaltyRowCount       = 1000   // 高风险 scan 惩罚上限
)
```

这 4 个常量让 cost 模型在「少行数 + 全表扫」时倾向 TiKV cop，在「大结果集 + 聚合」时倾向 TiFlash MPP。

### 25.5 Cascades Memo 与 Cost 计算的衔接

Cascades 搜索完成（`Optimizer.Execute` 返回 nil）后，物理化阶段从 memo 抽最低代价 plan：

```go
// 简化伪代码（按 plan_cost_ver2.go:42 + cascades/cascades.go 调用链）
func getPlanCost(p base.PhysicalPlan, taskType property.TaskType, option *costusage.PlanCostOption) (float64, error) {
    if p.SCtx().GetSessionVars().CostModelVersion == modelVer2 {
        if option.CostFlag.IsTrace() { option.WithCostFlag(costusage.CostFlagTrace) }
        return p.GetPlanCostVer2(taskType, option).GetCost(), nil  // 走 25.3 + 25.4
    }
    return p.GetPlanCostVer1(taskType, option)                     // 旧 cost model
}
```

物理化时按 plan tree 的每个节点算 cost，再 SumCostVer2 累加兄弟节点；非 root task 的 cost 会加上 `network_factor × data_size` 体现向 TiDB 回传的网络代价。

### 25.6 与本项目（教学级）的对照与复杂度

| 维度 | TiDB v8 工业级 | 本项目 `engineering/src/db/` |
|------|----------------|-----------------------------|
| 物理 plan 类型 | 50+ 种（TableReader / IndexReader / IndexLookUp / IndexMerge / HashJoin / MergeJoin / IndexHashJoin / IndexMergeJoin / ExchangeSender/Receiver / Projection / Selection / Limit / Sort / TopN / Window / CTE / PointGet / BatchPointGet ...） | 手写 heap scan + btree scan + simple filter，与 optimizer 解耦 |
| Cost 模型 | 16 维 factor × 算子级 cost 函数；hint 可调（`tidb_opt_*_factor`） | 无（按固定顺序扫表 + 全表 join） |
| Cascades | 完整 Optimizer + Memo + Task scheduler + ApplyRuleTask | 无 |
| 复杂度（单查询 plan） | O(N_log × R) with N_log = 逻辑 plan 节点数、R = rule 数；memo 集团去重限制爆炸 | O(N × log N) 全表 + nested loop |
| Session 切换 | `EXPLAIN FORMAT='cost_trace'` 输出 16 factor 实际值，便于调优 | 无 |

复杂度注释：
- 单 group 物理化：`O(best_per_group × child_count)`；
- 总体：`O(|group| × Σ_group_expression)`，被 memo group 合并去重；
- 通过 `CostModelVersion` 可切回 v1，v8 默认 v2。

源码（关键路径行号全部 `grep -n` 校验）：
- `reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:29` (`Optimizer struct`)
- `reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:35` (`NewOptimizer`)
- `reference/distributed-sql/tidb/pkg/planner/cascades/cascades.go:51` (`Execute`)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:42` (`GetPlanCost` 入口)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:114-126` (4 个 min/penalty 常量)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:128-154` (PhysicalIndexScan cost 公式)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1284-1306` (`defaultVer2Factors` 16 维值)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1308-1328` (`getTaskCPUFactorVer2` / `getTaskMemFactorVer2`)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1330-1353` (`getTaskScanFactorVer2`)
- `reference/distributed-sql/tidb/pkg/planner/core/plan_cost_ver2.go:1355-1368` (`getTaskNetFactorVer2`)

本项目对应：`engineering/src/db/sql/sql_planner.c`（手写 planner）、`engineering/src/db/optimizer/cost.c`（按行数 × filter 选择率 × join 顺序启发）。

---

## 二十六、Coprocessor 任务分片与 Bucket 路由（buildCopTasks + RegionBatchRequestSender）

§6.3 / §7.1 仅给出 Coprocessor 概念与 Region / MVCC key 编码，本节把"一次 Coprocessor 请求如何被切成多个 copTask 并按 TiKV bucket 路由到不同 store"打通。

### 26.1 buildCopTasks 入口与 rangesPerTask 切分

`pkg/store/copr/coprocessor.go:525` 是 Coprocessor 真正的"分片"函数：

```go
// reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:525
func buildCopTasks(bo *Backoffer, ranges *KeyRanges, opt *buildCopTaskOpt) ([]*copTask, error) {
    req, cache, eventCb, hints := opt.req, opt.cache, opt.eventCb, opt.rowHints

    // 1) 排序 + 单调化（合并 / 排序 key ranges）
    reordered := ensureMonotonicKeyRanges(bo.GetCtx(), ranges)
    cmdType := tikvrpc.CmdCop
    if req.StoreType == kv.TiDB {
        return buildTiDBMemCopTasks(ranges, req)            // 内存表走 local cop
    }

    // 2) 计算 ranges-per-task（默认 + failpoint 可调，详见 coprocessor.go）
    rangesPerTaskLimit := rangesPerTask                       // 来自 const，默认 ~2048
    failpoint.Inject("setRangesPerTask", func(val failpoint.Value) {
        if v, ok := val.(int); ok { rangesPerTaskLimit = v }
    })

    // 3) 按 TiKV bucket + region 切分（核心）
    var locs []*LocationKeyRanges
    if opt.skipBuckets {
        locs, _ = cache.SplitKeyRangesByLocations(bo, ranges, UnspecifiedLimit, false, false)
    } else {
        locs, _ = cache.SplitKeyRangesByBuckets(bo, ranges)  // 走桶级粒度切分
    }
    // 4) locs 按 store 聚合，每个 store 一个 copTask
    //    （详见 coprocessor.go:660-820 的 buildCopTaskForRange / buildCopTaskForTxnLocks）
}
```

`SplitKeyRangesByBuckets` 是 TiDB 区别于 CockroachDB 的关键 — TiKV 4.0 起引入"bucket"概念（每个 bucket 是 region 内一段连续 key range，由 PD 估算热点并预切分），让 TiDB 在生成 copTask 时**按 bucket 切分**，避免一个 region 内的热点全部打到 leader store：

```go
// 简化伪代码（按 RegionCache 接口）
func (c *RegionCache) SplitKeyRangesByBuckets(bo *Backoffer, ranges *KeyRanges) ([]*LocationKeyRanges, error) {
    var locs []*LocationKeyRanges
    for _, r := range ranges.Slice() {                     // 遍历每个 KeyRange
        buckets := c.findBucketsInRange(r.StartKey, r.EndKey) // 查 bucket 元数据
        for _, b := range buckets {                          // 按 bucket 边界切
            locs = append(locs, &LocationKeyRanges{
                Location: &tikv.KeyLocation{RegionID: b.RegionID, ...},
                Ranges:   b.Ranges,                          // bucket 内的子 range
            })
        }
    }
    return locs, nil
}
```

按 bucket 切分的好处是：一个大 region 内的查询（不在主键上的范围扫）能被分散到该 region 的**多个 bucket 上**，让不同 store 并行处理。

源码：`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:525-596`、`reference/distributed-sql/tidb/pkg/store/copr/region_cache.go`（`SplitKeyRangesByBuckets` 入口，详见同文件 `findBucketsInRange`）。

### 26.2 RegionBatchRequestSender.SendReqToAddr + onSendFailForBatchRegions

`copr/batch_request_sender.go` 把一批同 store 的 copTask 合并发送，并在失败时调 stale-region 自愈：

```go
// reference/distributed-sql/tidb/pkg/store/copr/batch_request_sender.go:60
func NewRegionBatchRequestSender(cache *RegionCache, client tikv.Client,
    oracle oracle.Oracle, enableCollectExecutionInfo bool) *RegionBatchRequestSender {
    return &RegionBatchRequestSender{
        rpcClient: client, regionCache: cache,
        enableCollectExecutionInfo: enableCollectExecutionInfo,
    }
}

// 把 req 路由到 rpcCtx 指定 store，覆盖 regionInfos 中所有 region
// reference/distributed-sql/tidb/pkg/store/copr/batch_request_sender.go:68
func (ss *RegionBatchRequestSender) SendReqToAddr(
    bo *Backoffer, rpcCtx *tikv.RPCContext,
    regionInfos []RegionInfo, req *tikvrpc.Request, timeout time.Duration,
) (resp *tikvrpc.Response, retry bool, cancel func(), err error) {

    // 1) 转发到底层 client
    resp, err = ss.rpcClient.SendRequest(bo.GetCtx(), rpcCtx.Addr, req, timeout)
    if err != nil {
        retry, cancel, err = ss.onSendFailForBatchRegions(bo, rpcCtx, regionInfos, err)
        return nil, retry, cancel, err
    }
    // 2) 若 regionVer / regionConfVer 与缓存不一致，先 cache.UpdateLeader
    for _, ri := range regionInfos {
        if resp.Resp == nil { break }
        if err = ss.cache.UpdateLeaderFromResp(bo, rpcCtx, ri.Region, resp); err != nil {
            return nil, true, func() {}, err
        }
    }
    return resp, false, func() {}, nil
}

// reference/distributed-sql/tidb/pkg/store/copr/batch_request_sender.go:95
func (ss *RegionBatchRequestSender) onSendFailForBatchRegions(
    bo *Backoffer, ctx *tikv.RPCContext, regionInfos []RegionInfo, err error,
) error {
    return ss.regionCache.OnSendFailForBatchRegions(bo,
        &tikv.Store{StoreID: ctx.StoreID, Addr: ctx.Addr}, regionInfos, true, err)
}
```

`OnSendFailForBatchRegions` 内部处理：
- **Region 错误**：发现 Region Miss / Epoch Not Match 抛 `RegionCache` 重新发现；
- **Store 错误**：若 store 被标 blacklisted，标记 + 后续重试排除；
- **Leader 转移**：通过 heartbeats 失效后，`UpdateLeader` 选择新 leader。

源码：`reference/distributed-sql/tidb/pkg/store/copr/batch_request_sender.go:60-95`、`reference/distributed-sql/tidb/pkg/store/copr/region_cache.go:974` (`OnSendFailForBatchRegions`)。

### 26.3 copTask 生命周期与 worker 派发

每个 copTask 实际请求由 `copIteratorWorker.handleTiDBSendReqErr` 处理 RPC 错误与重试：

| 阶段 | 关键函数 / 行 | 行为 |
|------|--------------|------|
| **任务生成** | `coprocessor.go:525` (`buildCopTasks`) | 按 bucket / region 切分 |
| **worker 派发** | `coprocessor.go:1272` (`copIteratorTaskSender.run`) | 调度 worker pool；处理 resourcegroup.RunawayChecker |
| **发送** | `coprocessor.go:1378` (`sendToTaskCh`) | 写入 channel；触发返回 copTaskResult |
| **错误恢复** | `coprocessor.go:2761` (`handleTiDBSendReqErr`) | Region 错误 → 重新 SplitKeyRangesByLocations（`skipBuckets=true` 重切），全表过期 → 重新 checkCache |
| **批量发送** | `batch_coprocessor.go:1237` (`sendBatch`) | 同 store 多 region 合并请求 |
| **lite 模式** | `coprocessor.go:1532` (`liteCopIteratorWorker.liteSendReq`) | 单 store 单 range 走精简路径，跳过 batch 合并 |

`handleTiDBSendReqErr` 在 retry 时会做"先 cancel 旧 task 再产生新 task"，避免 stale read；这与 CockroachDB `sendToReplicas` 行为对位（§11.3 sendToReplicas stale descriptor 自愈）。

### 26.4 复杂度与调度参数

| 阶段 | 复杂度 | 来源 |
|------|--------|------|
| ranges 单调化 | O(N log N)，N = ranges 数 | `ensureMonotonicKeyRanges` |
| SplitKeyRangesByBuckets | O(N × B)，B = buckets 数 / range | `RegionCache.findBucketsInRange` |
| copTask 聚合（同 store） | O(M × log M)，M = locs 数 | sort by store |
| 发送 | O(1) / copTask（gRPC 复用） | `SendReqToAddr` |
| Region stale 重新发现 | O(L × log L)，L = region 数（双向遍历） | `OnSendFailForBatchRegions` |
| **总复杂度** | **O((N log N) + (N × B) + (M log M) + (L log L)) ≈ O(N log N)** | |

线程模型：
- 每个 TiDB session 独占一个 `copIterator`，内含一个 `worker pool`（worker 数 = `tidb_distsql_scan_concurrency`，默认 15）；
- region 请求是 request-per-region 加上 batch 合并（`batch_coprocessor.go`），最大化 gRPC 长连接利用率；
- `copIteratorTaskSender` 通过 chan 把 copTask 派发给空闲 worker，限流器 `GetSendRate` 防止打爆 TiKV。

源码：`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:99` (`Send`)、`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:124` (`BuildCopIterator`)、`reference/distributed-sql/tidb/pkg/store/copr/batch_request_sender.go:60-95`。

### 26.5 与本项目对照

| 维度 | TiDB v8 | 本项目 `engineering/src/db/` |
|------|---------|-----------------------------|
| 任务切分粒度 | bucket → region → key range 三级 | 仅 key range（`kv_iter_seek` + `kv_iter_next`） |
| worker pool | `copIteratorTaskSender` + `tidb_distsql_scan_concurrency` | 单 goroutine 顺序扫 |
| stale region 自愈 | `OnSendFailForBatchRegions` 重新发现 | 无（直接失败） |
| 批量合并 | `sendBatch` 多 region 合并 | 无 |
| 限流 | `GetSendRate` token bucket | 无 |

源码：`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:525`、`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:1272`、`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:1364`、`reference/distributed-sql/tidb/pkg/store/copr/coprocessor.go:2761`、`reference/distributed-sql/tidb/pkg/store/copr/batch_coprocessor.go:1237`、`reference/distributed-sql/tidb/pkg/store/copr/region_cache.go:974`。

本项目对应：`engineering/src/db/copr/copr_router.c`（手写 router + range split）、`engineering/src/db/worker/pool.c`（线程池）。

---

## 二十七、Pessimistic/Optimistic 事务 Provider 派发与 Async-Commit / 1PC 提交路径

§10.2 仅 ASCII 描述 Percolator 主流程，本节聚焦 TiDB 层（区别于 TiKV 层）的事务隔离级别派发与 v6/v7 三个 commit 优化（1PC / Async-Commit / Pipelined-2PC）。

### 27.1 Provider 派发：Isolation → Provider

TiDB v6 重构事务框架为「`TxnContextProvider` 派发器」，按 `tidb_isolation_level` × 事务模式（causal / pessimistic）选 Provider：

```go
// reference/distributed-sql/tidb/pkg/sessiontxn/isolation/repeatable_read.go:38
type PessimisticRRTxnContextProvider struct {
    basePessimisticTxnContextProvider   // 嵌入悲观公共基类（prewrite + lock）
    // ForUpdateRead 用：from update statement, need latest for_update_ts
    forUpdateTS       uint64
    latestForUpdateTS uint64
}

// reference/distributed-sql/tidb/pkg/sessiontxn/isolation/repeatable_read.go:50
func NewPessimisticRRTxnContextProvider(sctx sessionctx.Context, causalConsistencyOnly bool,
) *PessimisticRRTxnContextProvider {
    provider := &PessimisticRRTxnContextProvider{
        basePessimisticTxnContextProvider: basePessimisticTxnContextProvider{
            baseTxnContextProvider: baseTxnContextProvider{
                sctx:                  sctx,
                causalConsistencyOnly: causalConsistencyOnly,
                onInitializeTxnCtx: func(txnCtx *variable.TransactionContext) {
                    txnCtx.IsPessimistic = true                       // 关键：标 IsPessimistic
                    txnCtx.Isolation     = ast.RepeatableRead
                },
                onTxnActiveFunc: func(txn kv.Transaction, _ sessiontxn.EnterNewTxnType) {
                    txn.SetOption(kv.Pessimistic, true)               // 关键：kv 层开 Pessimistic
                },
            },
        },
    }
    provider.getStmtReadTSFunc     = provider.getTxnStartTS
    provider.getStmtForUpdateTSFunc = provider.getForUpdateTs         // forUpdateTs 用 latest for update
    return provider
}
```

Provider 派发表（按 `tidb_isolation_level` × `tidb_txn_mode`）：

| Isolation × Mode | Provider | 来源 |
|------------------|----------|------|
| RR × Pessimistic（默认） | `PessimisticRRTxnContextProvider` | `isolation/repeatable_read.go:38` |
| RR × Optimistic | `OptimisticTxnContextProvider` | `isolation/optimistic.go` |
| RC × Pessimistic | `PessimisticRCTxnContextProvider` | `isolation/readcommitted.go` |
| SERIALIZABLE × Pessimistic | `PessimisticSerializableTxnContextProvider` | `isolation/serializable.go:33` |

悲观 vs 乐观的关键差异（取自 `sessiontxn/isolation/base.go:699` 基类）：

```go
// reference/distributed-sql/tidb/pkg/sessiontxn/isolation/base.go:699
type basePessimisticTxnContextProvider struct {
    baseTxnContextProvider
    // OnPessimisticStmtStart: 语句开始时拿 for_update_ts（prewrite 前）
    // OnPessimisticStmtEnd:   语句完成时检查 lock conflict，决定 retry
}

// OnPessimisticStmtStart
// reference/distributed-sql/tidb/pkg/sessiontxn/isolation/base.go:707
func (p *basePessimisticTxnContextProvider) OnPessimisticStmtStart(ctx context.Context) error {
    return p.preuseStmtForUpdateTS(ctx)   // PD 取最新 ts 作为 prewrite ts
}
```

- **悲观**：每个 Update/Delete 语句先走 prewrite（拿到 lock），语句内的点查直接走 committed snapshot；
- **乐观**：语句内读 snapshot，commit 时一次性 prewrite；如发现冲突 → 整事务重试。

源码：`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/repeatable_read.go:38`、`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/optimistic.go`、`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/base.go:699-757`、`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/serializable.go:33`。

### 27.2 三种 commit 优化：`tidb_enable_1pc` / `tidb_enable_async_commit`

TiDB 6.0 起引入 1PC，6.1 起引入 Async-Commit，7.0 起引入 Pipelined-2PC（与 Async-Commit 互斥，但可叠加 1PC）：

```go
// reference/distributed-sql/tidb/pkg/kv/option.go:58
const (
    // EnableAsyncCommit indicates whether async commit is enabled
    EnableAsyncCommit                                  // 6.1+, Async-Commit 开关
    // Enable1PC indicates whether one-phase commit is enabled
    Enable1PC                                         // 6.0+, 单 phase commit
)
```

开关定义：

```go
// reference/distributed-sql/tidb/pkg/sessionctx/variable/sysvar.go:2917
{
    Scope: vardef.ScopeGlobal | vardef.ScopeSession,
    Name:  vardef.TiDBEnableAsyncCommit,
    Value: BoolToOnOff(vardef.DefTiDBEnableAsyncCommit),  // 默认 ON（v8）
    Type:  vardef.TypeBool,
    SetSession: func(s *SessionVars, val string) error {
        s.EnableAsyncCommit = TiDBOptOn(val)
        return nil
    },
},
{
    Scope: vardef.ScopeGlobal | vardef.ScopeSession,
    Name:  vardef.TiDBEnable1PC,
    Value: BoolToOnOff(vardef.DefTiDBEnable1PC),          // 默认 ON（v8）
    Type:  vardef.TypeBool,
    SetSession: func(s *SessionVars, val string) error {
        s.Enable1PC = TiDBOptOn(val)
        return nil
    },
},
```

#### 27.2.1 1PC（One-Phase Commit）

适用场景：单 region、范围极小（PK 范围 ≤ `txn-entry-size-limit / 6`，约 6MB）。

```text
[原生 Percolator 2PC]
  prewrite(primary)  → prewrite(secondaries)
  commit(primary)    → commit(secondaries)
  共 2 RTT（commit phase）

[1PC 路径（无 secondary 写）]
  commit(primary)   ← 原子：data + lock + commit record 一次写入
  共 0.5 RTT（与 primary prewrite 并行）

[触发条件] tidb_enable_1pc = ON  且  事务内 key 全部落到 1 个 region  且  txn 大小 ≤ 6MB
```

TiKV 端把 1PC 视作"early commit"：在 prewrite primary 时直接落 commit record，省掉后续 commit round-trip。

#### 27.2.2 Async-Commit（异步 commit）

适用场景：多 region，但事务 commit 主键可被 1 RTT 内传播。

```text
[Async-Commit]
  prewrite(primary)        → 拿 commit_ts = PD.TSO()（在 primary prewrite 前）
  prewrite(secondaries)    → 携带 commit_ts（无需 2nd round）
  commit(primary)          → 异步刷新 commit record
  client 收到 committed    ← 一旦任何 prewrite 收到，其他副本后台补齐

[触发条件] tidb_enable_async_commit = ON  且  事务内 key 跨 ≤ TBD 区域  且  commit_ts 在 primary 上单调
```

关键改进是 **commit_ts 在 prewrite 阶段就确定**（取自 PD TSO），省掉传统 Percolator 的 commit primary RTT，达到"commit RTT = 1"。

源码：`reference/distributed-sql/tidb/pkg/sessionctx/variable/sysvar.go:2917-2924`、`reference/distributed-sql/tidb/pkg/sessionctx/variable/session.go:1484`（注释说明 EnableAsyncCommit 字段含义）。

#### 27.2.3 Pipelined-2PC（流水线 2PC，v7+）

Pipelined-2PC 与 Async-Commit 互斥，但可与 1PC 叠加：在 prewrite secondaries 还没完成时就启动 commit primary。

```text
[pipelined-2PC]
  prewrite(primary)        ┐
  prewrite(secondaries[1]) │    并行
  ...                       │
  prewrite(secondaries[n]) ┘
  commit(primary)               ← secondaries 全部完成时（无需 explicit wait）
  ↑ commit_ts 在 primary 取时已落库
```

Pipelined-2PC 在 `tidb_enable_async_commit = ON` 时自动启用（除非显式关）。

源码：`reference/distributed-sql/tidb/pkg/kv/option.go:58-62`、`reference/distributed-sql/tidb/pkg/sessionctx/variable/sysvar.go:2917-2924`。

### 27.3 Commit BatchSize（事务内临时合并）

`TiDBTxnCommitBatchSize` 控制 commit 阶段向 TiKV 发请求的批大小（默认 16 KiB / request）：

```go
// reference/distributed-sql/tidb/pkg/sessionctx/variable/sysvar.go:1021
{
    Scope:    vardef.ScopeGlobal,
    Name:     vardef.TiDBTxnCommitBatchSize,
    Value:    strconv.FormatUint(tikvstore.DefTxnCommitBatchSize, 10),
    Type:     vardef.TypeUnsigned, MinValue: 1, MaxValue: 1 << 30,
    SetGlobal: func(s *SessionVars, val string) error {
        return setTidbOptUint64(&tikvstore.TxnCommitBatchSize, val, tikvstore.DefTxnCommitBatchSize)
    },
}
```

实际服务端 batch 处理：`pkg/executor/load_data.go:612 commitWorker.commitWork` 与 `:654 commitOneTask`：每个 commitWorker 处理多个 `commitTask` 任务，过 Pipelined-2PC 大事务（LOAD DATA 场景）能稳定到 100MB+。

### 27.4 错误重试与 Stale Read 回退

悲观事务遇到 `kv.ErrTxnRetryable` 时通过 `basePessimisticTxnContextProvider` 走 lock TTL 刷新 + 整事务重试：

```go
// 简化伪代码（按 sessiontxn/isolation/base.go:748 retryFairLockingIfNeeded 与本项目 transaction.c 对应）
func (p *basePessimisticTxnContextProvider) handleTxnError(ctx context.Context, err error) {
    switch e := err.(type) {
    case *tikverr.ErrWriteConflictInPrewrite:        // 写写冲突
        p.refreshLockTTL(ctx)                          // 拉长 lock TTL（与 retryMaxTime 比对）
        p.scheduleRetry(ctx)                           // 重新发起 begin-prewrite
    case *tikverr.ErrLockNotFound:                    // lock TTL 超时被 rollback
        p.scheduleRetry(ctx)                           // 整事务重做
    case *tikverr.ErrTxnNotFound:                     // region 未收到 lock 通知
        p.scheduleRetry(ctx)
    }
}
```

`onTxnRollback` / `onTxnFailed` 由 `baseTxnContextProvider` 提供默认实现，最终统一把事务置为 invalid 并触发 `provideTxnErr` 给上层 session。

源码：`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/base.go:748-757`（retryFairLockingIfNeeded + cancelFairLockingIfNeeded）。

### 27.5 复杂度与本项目对照

| 维度 | TiDB v8 | 本项目 `engineering/src/db/` |
|------|---------|-----------------------------|
| Isolation × Mode 矩阵 | 2×2 = 4 种 Provider + base 抽象 | 单一 RR + 写写冲突时 retry |
| Commit 模式 | 1PC / Async-Commit / Pipelined-2PC / 经典 2PC | 仅经典 2PC |
| 重试原因分类 | WriteConflict / LockNotFound / TxnNotFound / Read-Conflict | 简单 retry loop |
| Stale Read | `tidb_read_staleness` 会话变量控制 | 无 |

复杂度：
- **Provider 选择**：O(1)（直接读 `tidb_txn_mode` + `tidb_isolation_level`）；
- **ForUpdateTs 取 PD TSO**：1 RTT（PD 单点）；
- **Pessimistic 重试**：最多 `tidb_pessimistic_txn_max_retry_count` 次（默认 256），单次 TTL `txn-lock-timeout` 默认 60s；
- **Async-Commit commit RTT**：1（等同 1PC 的 worst-case）；Pipelined-2PC 的 secondaries prewrite 与 primary commit 是重叠的。

源码：`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/base.go:699-757`、`reference/distributed-sql/tidb/pkg/sessiontxn/isolation/repeatable_read.go:38-72`、`reference/distributed-sql/tidb/pkg/kv/option.go:58-62`、`reference/distributed-sql/tidb/pkg/sessionctx/variable/sysvar.go:1021`、`reference/distributed-sql/tidb/pkg/sessionctx/variable/sysvar.go:2917-2924`、`reference/distributed-sql/tidb/pkg/sessionctx/variable/session.go:1484`。

本项目对应：`engineering/src/db/tx.c`（手写事务模块）、`engineering/src/db/pessimistic.c`（pessimistic 模式重试）、`engineering/src/db/async_commit.c`（TODO placeholder）。

---

## 二十八、WD-9 任务附加信息（占位）

> 本任务为 wiki-deepen 任务 9，受 brief 限制最多追加 3 节内容已用尽（二十五 / 二十六 / 二十七）。本节作为后续扩展位（保留编号空间），不新增内容。

