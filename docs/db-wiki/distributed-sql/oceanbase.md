# OceanBase Wiki（分布式 SQL / NewSQL / HTAP）

> 参考版本：OceanBase 仓库当前 master 分支（commit 与上游同步），主要 API 与数据布局在 v4.0（"OB Cloud Native"）之后稳定；本文档基于 master HEAD（约 v4.3.x 系列）。

## 一、项目定位

OceanBase Database（OB）是**由蚂蚁集团完全自研的分布式关系型数据库**，对外提供 **MySQL 兼容协议与方言（同时支持 Oracle 兼容模式）**，内核基于 **Paxos 共识 + 多副本 LogStream + LSM-Tree 列存 + 向量化执行**。其首要定位是**金融级 RPO=0、RTO<8 秒**的强一致、可线性扩展的 HTAP 数据库——曾多次刷新 TPC-C / TPC-H 性能世界纪录。

- **痛点**：
  - 商业集中式数据库（Oracle / DB2 / SQL Server）扩展性差、价格高，国产替代强需求；
  - 经典"分库分表"方案（MySQL Sharding / DRDS）牺牲事务完整性，复杂查询受限，扩容痛苦；
  - 互联网大促场景（双 11 峰值订单洪流）需要"水平无限扩容 + 强一致 + 海量数据归档"的国产自研能力；
  - 银行、保险、券商要"两地三中心 / 三地五中心"多活容灾，要求 RPO=0、RTO 秒级；
  - 同时承载事务型与分析型查询，HTAP 需求迫切。
- **典型用户**：蚂蚁集团、阿里巴巴、中国工商银行（2020 年起 OB 在分布式核心系统替换 Oracle）、中国邮政储蓄银行、招商银行、华夏银行、东莞银行、中国移动（CRM / 计费）、多家证券、券商、电信运营商。

## 二、发展历史与社区

- **起源**：2010 年由阳振坤博士（北京大学博士，后阿里 / 蚂蚁首席科学家）发起，从淘宝 / 支付宝"去 IOE"项目中长出来；最早是为了替换支付宝交易库 Oracle，目标是"一个集群顶住双 11"；
- **里程碑**：
  - 2010 - 2013：OceanBase 1.0（多列 KV 引擎，单集群 100+ 台机器）；
  - 2014：OB 1.0 全面接管支付宝交易库、账务库；
  - 2015：上线春晚红包项目（承担峰值 10w+ TPS）；
  - 2016：OceanBase 2.0（基于 Paxos 的多副本 RPO=0）；
  - 2019 年 9 月：OB 2.2 跑 TPC-C 突破 **6,070 万 tpmC**（世界纪录，刷新 Oracle 19c）；
  - 2020 年 5 月：开启 OpenSource 道路（`oceanbase/oceanbase` GitHub 公开，Apache 2.0 协议）；
  - 2020 年 9 月：TPC-C 单集群跑 **7.07 亿 tpmC**（再次刷新世界纪录）；
  - 2022 年 OB 4.0（云原生架构、Kubernetes 化、内核资源池化、tenant 隔离增强、副本可任意跨 region / AZ）；
  - 2023 年 4.x LTS：列存副本（Columnar Replica）强化 HTAP、OBKV（KV 接口）、OMS 数据迁移；
  - 2024-2025 年 4.3+：基于共识组的 Paxos 协调、AI workload 支持；
- **维护状态**：活跃；每年 1-2 个大版本迭代（含 LTS）；商业版 OceanBase 4.x 由北京奥星贝斯（OceanBase）公司主导，社区版持续演进；2023 年起全核心代码在主仓开源。
- **社区**：OBCO (OceanBase Community Organization)、OCEA（OceanBase Community Ecosystem Alliance）、CSDN / 知乎 / B 站活跃；定期举办"OB 大赛"（技术认证）。

## 三、许可与商业限制

- **许可证**：早期 OB 私有化（"Open License" 协议），从 2023 年起 **核心全部 Apache 2.0**（核心代码、工具）；
  - 来源：`reference/distributed-sql/oceanbase/LICENSE`（位于 `LICENSE` 文件）；
  - 第三方工具（OMS、OCP）部分采用 MulanPSL-2.0 / 商业闭源；
- **商业版 / 服务**：
  - **OceanBase Database Enterprise Edition**：增加 OMS（数据迁移）、OCP（运维平台）、ODP（智能代理增强）；
  - **OceanBase Cloud（OB Cloud）**：阿里云上提供集群版 + 租户版（多租户 DBaaS）；
  - **自运维 OCP**（开源部分）：社区版 OCP 在 `oceanbase/oceanbase` 仓下子仓提供；
  - **培训认证**：OCA / OCP / OCE 多级。

## 四、业界主要使用方与案例

- **金融 / 银行**：工商银行（ICBC）核心账务部分子系统（2021 年分布式核心系统改造）、招商银行、华夏银行、东莞银行、常熟农商行、紫金农商行、上海银行、浦发银行；
- **证券 / 保险**：东海证券、平安产险部分系统、安心财险；
- **电信运营商**：中国移动业务支撑系统、计费、CRM 部分；中国电信运营商支撑；
- **互联网**：蚂蚁集团（支付宝 / 蚂蚁链）、阿里巴巴、淘宝、天猫、菜鸟、高德；字节跳动部分业务；
- **公共 / 政府**：国家税务总局电子发票平台、北京公积金管理中心、海关总署部分系统。

TPC 公开里程碑：
- 2019-09：6,070 万 tpmC（30 节点）；
- 2020-09：7.07 亿 tpmC（1500+ 节点创纪录）；
- 2021：TPC-H 15.26 million QphH @30000GB（HTAP 标杆）。

## 五、一句话对比定位

> OceanBase 是**"原生分布式 + 自研 Paxos + 多租户 + HTAP"**的 NewSQL 旗舰代表；与 **CockroachDB（PG 兼容、Pebble、Raft）**、**TiDB（MySQL 兼容、RocksDB、Percolator）**、**YugabyteDB（双 API、DocDB）** 一起构成 NewSQL "四强"，且是国产化、HTAP、世界 TPC 性能纪录的代表。

## 六、整体架构

OB 的内核分层是 **RootService 总控 + 多 LogStream 副本 + 强一致 Paxos + LSM-Tree 列存**。

### 6.1 总体分层

```
┌────────────────────────────────────────────────────────────────────────┐
│                       客户端 (MySQL CLI / JDBC / Driver)                │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ MySQL Wire Protocol (5.7/8.0 子集)
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    OBProxy / ODP (连接池 + SQL 路由)                  │
│   - 协议解析、读写路由、连接池                                            │
│   - 多 ob-server 集群寻址                                                │
│   - 可选 SQL 限流、分片路由                                                  │
└─────────────────────────┬──────────────────────────────────────────────┘
                          │ MySQL protocol to observer
┌─────────────────────────▼──────────────────────────────────────────────┐
│                    ob-server 节点 (多副本/Paxos)                       │
│ ┌──────────────────────────────────────────────────────────────────────┐│
│ │ SQL 层 (src/sql, src/observer):                                       ││
│ │   - MySQL Parser + Resolver                                           ││
│ │   - Plan / Optimizer (Cascades + Rule)                                ││
│ │   - DML/DDL Executor (src/sql/executor, src/sql/engine)              ││
│ │   - 协议端 (src/observer/mysql) ←→ MySQL frontend                      ││
│ │   - Plan cache、PS cache                                              ││
│ │   - Out/In-bound SQL 路由                                              ││
│ └──────────────────────────────────────────────────────────────────────┘│
│ ┌──────────────────────────────────────────────────────────────────────┐│
│ │ 存储层 (src/storage):                                                  ││
│ │   - LS = LogStream = 一组 tablet 的"分区单位"                       ││
│ │   - Memtable + Block SST (LSM-Tree) + BloomFilter                    ││
│ │   - 列存副本 (Column Store Replica) - HTAP                            ││
│ │   - ClogPB 字典压缩、编码                                              ││
│ │   - Macro Block (2 MiB / 4 MiB / 8 MiB 块)                           ││
│ │   - Compaction、Schema 演进                                              ││
│ │                                                                          ││
│ │   - 事务: multi-version snapshot                                          ││
│ │   - 锁: 行锁 + 表锁 + 自旋锁                                                ││
│ │   - Concurrency control + deadlock detection                             ││
│ │                                                                          ││
│ │   - 备份 / 恢复 / 物理备份（直接 SST 拷贝）                               ││
│ │   - Replication: Paxos (multi-Paxos) for LogStream                      ││
│ └──────────────────────────────────────────────────────────────────────┘│
│ ┌──────────────────────────────────────────────────────────────────────┐│
│ │ 日志 / 共识层 (src/logservice):                                        ││
│ │   - 基于自研的 Paxos 实现 (src/logservice/palf/)                        ││
│ │   - 多 PAXOS 实例 / 副本 / leader                                      ││
│ │   - LogStream 上的"redo log"                                             ││
│ │   - LogArchive、BackupSync                                              ││
│ └──────────────────────────────────────────────────────────────────────┘│
│ ┌──────────────────────────────────────────────────────────────────────┐│
│ │ RootService 总控 (src/rootserver):                                      ││
│ │   - 集群拓扑、rebalance、partition 副本分布                                ││
│ │   - 单位调度: Balance / Failover / SwitchRole / DDL                    ││
│ │   - Resource Manager / Tenant 管理                                      ││
│ │   - SQL Service 内嵌（系统表读写）                                         ││
│ └──────────────────────────────────────────────────────────────────────┘│
└────────────────────────────────────────────────────────────────────────┘
```

源码入口：
- Observer 启动：`src/observer/main.cpp:1`
- RootService：`src/rootserver/ob_root_service_main.cpp:1`（或 `src/observer/ob_server_main.cpp`）
- LS（LogStream）：`src/storage/ls/ob_ls.cpp:1`
- SQL 入口：`src/sql/engine/ob_engine_executor.cpp`（粗略入口）
- Palf（Paxos）：`src/logservice/palf/palf_handle.cpp:1`

### 6.2 单进程：ob-server

单 `ob-server` 进程内承载：

- **MySQL 协议监听线程**（src/observer）；
- **SQL 计划 / 优化 / 执行工作线程池**（多线程并行）；
- **LS worker threads**：每个 LogStream 一组 goroutine-like worker 处理事务、replay、compaction；
- **Palf replica worker**：维护 log 复制、心跳；
- **后台线程**：compaction、GC、统计信息采集、备份；
- **Cgroup 隔离 / Resource Unit (Unit)**：基于 cgroup 与 CPU 配额（4.0+ 引入）；
- **Tenant 隔离**：每个租户可以独立 CPU / 内存配额。

### 6.3 一条 SQL 的端到端路径

```
client → OBProxy  → ob-server (MySQL protocol)
  → Parser (yacc/lex)
  → Resolver → BindInfo check
  → Optimizer (LogPlan → PhyPlan)
  → Executor (Volcano 风格，部分算子矢量化)
  → Send to Memtable/Block SST (LSM-Tree read)
  → 返回结果集 编码为 MySQL ResultSet
```

源码：`src/sql/parser/parse_node.cpp`、`src/planner`、`src/executor`、`src/storage/blocksstable/cs_encoding/`。

## 七、存储引擎

### 7.1 LSM-Tree + Macro Block

OB 的底座是经典 **LSM-Tree**，但以**大块 Macro Block**（2/4/8 MiB）作为合并/查询单位：

```
┌─────────────────────────────────────────────────────────────────┐
│                  MemTable (active/immutable)                    │
│  - Hash Index (SkipList)                                          │
│  - Row format: rowkey + columns (ClogPB 字典)                   │
│  - Flush 触发: 内存阈值或 timing                                  │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Flush (compaction → 0 阶 SST)
┌─────────────────────────▼───────────────────────────────────────┐
│             SST (Sorted String Table) on Macro Block            │
│  - 每个 SST = 多个 Macro Block（block_id + offset）             │
│  - Macro Block 含 Block Index、BloomFilter、RLE delta          │
│  - 块级压缩：zstd / lz4 / zlib / snappy                         │
└─────────────────────────┬───────────────────────────────────────┘
                          │ Compaction（minor / major / freeze）
                          ▼
                  本盘 Macro Block Pool
```

- **Block ID + Offset**：OB 的所有 IO 是**块定位**而非字节定位，备份/迁移都是"复制 SST + 块映射"；
- **Block Cache**：每个 macro block 可放入 block_cache，避免读盘；
- **多版本**：MemTable 与 0 阶 SST 上有"按 schema version"重组。

源码：`src/storage/blocksstable/ob_block_manager.cpp`、`src/storage/blocksstable/ob_macro_block_writer.cpp`、`src/storage/memtable/ob_memtable.cpp`。

### 7.2 ClogPB 列存 (Column 1.0 之前为基线)

OB 自研的列存引擎 **ClogPB**：

- 一列在多个块中以 **PB-encoded** 形式紧凑存储；
- 支持字典压缩（dictionary on column）；
- 列存上的大部分 IO 可**只读必要的列**；
- 写入是新版本切换（`schema_version++`），不修改旧版本。

源码：`src/storage/blocksstable/cs_encoding/ob_column_oriented_encoding.cpp`。

### 7.3 列存副本（HTAP / 4.0+）

OB 4.0 引入了**列存副本**——同一个 LogStream 上可以配置：

- **行存副本**（F / Full 类型）用于事务；
- **列存副本**（R / ReadOnly 类型）用于 HTAP 分析；
- 副本之间通过 redo log 同步，列存副本异步 replay。

源码：`src/storage/column_store/ob_column_store_replica.cpp`。

### 7.4 LogStream（LS）

LogStream（LS）是 OB 的"分区"单位，对应 Tablet/Partition/Shard：

- 多个 tenant 共享一个集群，tenant 表位于某 LogStream 上；
- 每个 LS 在 N 个副本（由 `paxos_replica_num` 控制）上做 Paxos 复制；
- LS 的扩容、调优、迁移是 RootService 控制。

源码：`src/storage/ls/ob_ls.cpp`、`src/storage/ls/ob_ls_meta.cpp`。

## 八、索引机制

| 索引类型 | 实现 | 备注 |
|----------|------|------|
| **Primary Key** | 表数据按 PK 顺序存储在 LS 中；MemTable + SST 物理有序 | 默认 clustered |
| **Global Secondary Index** | 表内多张内部表（按 `_idx` 命名）+ 主表关联 | 4.0 起 GA 在 MySQL 模式 |
| **Local Index** | 4.0+ 与本地列存副本搭配加速 HTAP | 实验 |
| **Partial / Functional Index** | `WHERE` 谓词 + 表达式 | MySQL 模式兼容 |
| **Multi-valued Index** | JSON / Array | MySQL 8 兼容 |
| **Vector Index (HNSW)** | v4.3+ 提供 SQL 函数 `OB_VECTOR_DISTANCE` | 内置 ANN |

源码：`src/storage/access/ob_pdx_handler.cpp`、`src/sql/ob_index_ddl_service.cpp`。

### 8.1 全局二级索引（GSI）

OB 的 GSI 实现为**独立逻辑表**（4.0 之前是"物化视图方式"，4.0 起作为真实 index）；

- 写路径：双写（按异步事务）；
- 读路径：通过 optimizer 选 GSI；
- 限制：索引 lock 时延，可能与 PG / TiDB 的"实时二级索引"略有差距，但金融场景可接受。

## 九、查询 / 计算 / 接口层

### 9.1 SQL 方言与 MySQL / Oracle 兼容

OB 同时支持两种方言：

#### 9.1.1 MySQL 模式（默认）

- 协议：MySQL 5.7/8.0 兼容；
- 数据类型：标准 MySQL + 一些 OB 扩展；
- 函数：与 MySQL 高度匹配；
- DDL：`CREATE TABLE`、`CREATE INDEX`、`PARTITION BY`（含 OB 自创的 RANGE / LIST / HASH / KEY）；
- 视图、触发器、UDF、Sequence、LIMIT。

#### 9.1.2 Oracle 模式

- 兼容 PL/SQL 子集、`DUAL`、`ROWNUM`、`CONNECT BY`、`DECODE` 等；
- 用于金融行业从 Oracle 迁移；
- 通过租户的 `compatible_mode` 开关开启。

### 9.2 优化器

OB 优化器是多代进化：

- v1：基于规则的 RBO；
- v2.x：CBO + Cascade 风格；
- v3.x：CBO + Cost Based Transformation；
- v4.x：完整 Cascades + Dynamic Query Rewrite + Plan Cache；
- 表 / 列的统计信息由 `DBMS_STATS` 风格接口维护（用户主动收集）。

源码：`src/sql/optimizer/ob_optimizer.cpp`、`src/sql/rewrite`。

### 9.3 执行模型

- **Volcano 风格**：每个算子是 pull-based；
- **矢量化**：表扫、project、filter 算子按 Batch（KV cache line 友好）批量；
- **并行执行**：单 query 内并行（intra-query parallelism）v3+；
- **DAS（Direct Access Service）**：绕过 SQL 层直接访问 partition（连接 SINK 用）；
- **DistSQL**：跨机器执行的分布式 SQL，例如分区表 JOIN。

源码：`src/sql/executor`、`src/sql/dtl`、`src/sql/das`。

### 9.4 接口与生态

- **MySQL 客户端**全部可用：mysql CLI、JDBC、PHP PDO、Python mysqlclient 等；
- **DBeaver / DataGrip / TablePlus**：GUI（仅 MySQL 模式可用）；
- **OMS（OceanBase Migration Service）**：从 MySQL / Oracle / DB2 / PG 迁移工具；
- **OBKV**：OB 提供的 KV/HBase 兼容的独立接口（`oceanbase/obkv`）；
- **OBProxy / ODP**：连接代理 / 智能路由；
- **OB-Operator / OCP**：Kubernetes / 独立运维平台；
- **备份恢复**：OB 的 "物理备份" 直接传 Macro Block 到 OSS / NFS；
- **CDC**：OB CDC Studio + 自带 binlog（兼容 MySQL binlog）。

## 十、事务与一致性

### 10.1 隔离级别

| 隔离级别 | 是否支持 | 说明 |
|----------|----------|------|
| READ UNCOMMITTED | 是 | - |
| READ COMMITTED | **默认**，金融场景首选 | 允许幻读，但提供 consistent snapshot |
| REPEATABLE READ | 是 | snapshot |
| SERIALIZABLE | 是（`SET SESSION ISOLATION LEVEL = SERIALIZABLE`） | 通过 SSI 实现 |

OB 默认是 **READ COMMITTED**，与传统 MySQL 默认 RR 不同。

### 10.2 分布式事务：Multi-Paxos + 2PC

OB 自研 **Paxos 协议**（实现在 `src/logservice/palf/`）：

```
client → ob-server SQL 层
  ├── BeginTxn: 拿事务 ID + read snapshot
  ├── DML: 各 LogStream 上：
  │     • 写 Memtable（带 rowkey_lock）
  │     • Redo log 通过 Palf 走 Paxos（多数派 ack）
  ├── Commit:
  │     • 2PC 阶段：所有 involved LogStream 同步 commit point
  │     • 通过全局 commit_version (Paxos log position)
  └── Fail: 任一 Paxos 失败 → 回滚 rowkey_lock
```

要点：

- **Paxos（每个 LogStream）**：保证 redo log 的多数派 ack；
- **多 Paxos 组协调**：跨 LS 事务由 SQL 层协调（OB Optimizer + 2PC 协同）；
- **强一致：RPO=0**：仅当所有副本永久丢失，才丢最后一条 commit；
- **RTO < 8s**：单副本故障切换靠自动 leader 选举，一般 1-3 s；
- **快照读**：based on `snapshot_version`，扫对应版本序的 redo log。

源码：`src/logservice/palf/palf_handle_impl.cpp`、`src/storage/transaction/ob_trans_ctx.cpp`、`src/sql/ob_end_trans_callback.cpp`。

### 10.3 共识协议（Paxos in OB）

- **Paxos（经典）**：Lamport 1989，少数派存活即可；
- **Multi-Paxos**：OB 在经典 Paxos 基础上做的优化（稳定 leader lease + log batching）；
- **Learner**：OB 4.x 支持 Async Learner（仅投票不参与活）；
- **配置变更**：Joint Paxos-like 协议，避免切换期间双主；
- **强同步**：`FORCE_TRANSFER` 用于运维强制角色切换。

源码：`src/logservice/palf/palf_options.h`、`src/logservice/palf/palf_kv_cb_engine.cpp`、`src/logservice/palf/palf_meta_file.cpp`。

### 10.4 全局时间戳 / 快照管理

- 通过 Paxos log position 实现"快照版本"——任何 redo log 都带 monotonic 序号；
- 不依赖外部时钟，"逻辑时钟 + 物理时钟"混用；
- 跨 LS 事务通过 SQL 层记录 `cluster_snapshot_version`。

源码：`src/logservice/palf/palf_log_group_entry.cpp`、`src/storage/ob_storage_io_util.cpp`。

## 十一、复制与高可用（深度扩展）

### 11.1 多副本：Multi-Paxos per LogStream

每个 LogStream 是一组独立 Paxos 实例：

- **副本数**：1/3/5（生产场景 3 起步，anytime 改）；
- **副本分布**：通过 `ZONE`、`PRIMARY_ZONE`、`LOCALITY` 标签；
- **副本类型**：F (Full) / R (ReadOnly 列存副本) / L (Log only)；由 RootService 控制；
- **Zone**：基本容灾单位，建议至少 3 个 zone，每 zone 不同可用区。

### 11.2 多区域容灾（Multi-Region / Multi-DC）

OB 支持**完整的多区多活方案**：

```
IDC-A:Zone1 + Zone2 + Zone3      IDC-B: Zone4 + Zone5 + Zone6
  OB-Servers (F 副本)               OB-Servers (F 副本)
同一集群, 跨 DC 通信:
  - 同城 latency < 1 ms (zone 间)
  - 跨城 latency 5-20 ms (DC 间)
通过 ALTER TENANT + PRIMARY_ZONE 控制读写就近策略
```

OB 4.x 也支持**两地三中心**：

```sql
ALTER TENANT my_tenant PRIMARY_ZONE = 'zone1;zone2,zone3';
```

可选 [OB 三地五中心](https://www.oceanbase.com/docs/community/observer-3rd-0000000002918153) 解决方案。

OBProxy 端到端切换时通过 `SET GLOBAL ODP_TARGET_CLUSTER` 重定向。

### 11.3 主动 / 被动 / 多主 / 双活

- **同步主备**：通过 Standby Replica 实时同步（4.x GA）；
- **跨 IDC DR**：阿里云容灾方案 `OceanBase DR`；
- **多主**：v3.x 早期实验，4.x GA（基于 Leader+Follower 的强一致多写）；
- **双活**：通过 ODTS（基于 redo log 的在线传输服务）。

### 11.4 故障自愈

- **Leader Lease 心跳**：1-3s 间隔，掉线触发选举；
- **自动 Rebalance**：RootService 持续调度副本分布、负载均匀；
- **OBAgent 报告**：集群内 `__all_virtual_server_stat` 等 SQL 接口可查；
- **OB 备份恢复器**：物理备份 + redo log 重放；

源码：`src/observer/ob_heartbeat.cpp`、`src/rootserver/ob_all_server_checker.cpp`。

## 十二、扩展性（深度扩展）

### 12.1 分片策略

OB 不像 CockroachDB 自动 Range Split / Merge；分片主要靠：

- **手工 Partition**：`PARTITION BY HASH(col) PARTITIONS 64`；
- **自创** Range / List / Hash / Key 分区；
- **Secondary Split**（OGP）：通过迁移动作把 partition 切成两半（4.1+）；
- **Reorg Backup**：`ALTER TABLE ... REORGANIZE PARTITION`。

> **注意**：OB 早期不自动扩缩容是金融场景可接受的——业务方对"扩容"有计划、可控。4.x 起动态 partition 增强。

### 12.2 水平扩容流程

```
1. 新 ob-server 加入（线上和已有 zookeeper / etcd 风格的元数据中心同步）
2. 调整 tenant 的 UNIT_NUM / UNIT_CONFIG
3. RootService 自动迁移 / 新增 partition 副本到新节点
4. (可选) tenant 切主 zone / 重分布
```

### 12.3 在线 Schema Change

OB 所有 DDL 走**异步作业**：

```
DDL Job (mysql.__all_ddl_operation)
  → 进入 queue
  → 多个 epoch 状态机:
     Check → Lock → Schema version update → Build index → Replace
  → 状态 schema_version++ 推动所有 OB Server 切版本（轻量级 cache ref）
```

源码：`src/storage/ddl/ob_ddl_service.cpp`、`src/rootserver/ddl_task/`、`src/sql/ob_ddl_service.cpp`。

### 12.4 多租户（Multi-Tenant）

OB 4.x 重新设计为 **多租户共享集群**：

- **Cluster**：物理资源（节点、内存、磁盘）；
- **Tenant**：逻辑单位（数据库集合）；通过 resource unit 分配 CPU / Memory；
- **Resource Unit**：1 个 Unit = 若干 Cpu + 若干 Mem + 1 个 LogDisk；
- **Tenant 隔离**：cgroup + Unit 配额。

一个 DB 用户可看到"我在哪个 Tenant"：

```sql
SELECT t.tenant_name FROM oceanbase.DBA_OB_TENANTS t;
```

源码：`src/rootserver/ob_tenant_service.cpp`、`src/sql/engine/cmd/ob_tenant_cmd_executor.cpp`。

### 12.5 Primary Zone / Region

```sql
ALTER TENANT my_tenant PRIMARY_ZONE = 'z1;z2,z3';
ALTER TENANT my_tenant DEFAULT TABLEGROUP = 'tg1';
```

源码：`src/rootserver/ob_ddl_service.cpp`、`src/observer/ob_zone_service.cpp`。

## 十三、性能与基准

### 13.1 TPC-C

- 2019-09：6,070 万 tpmC（30 节点 OB 2.2）；
- 2020-09：7.07 亿 tpmC（约 1500+ 节点 OB 2.0 / OB 3.0）；
- TPC-H 同期世界纪录：1.526 亿 QphH @30TB（OB 4.0）。
- 持续 8 年（2019-2026）刷新性能纪录，反映其工程深度。

### 13.2 SysBench OLTP

- 单 ob-server：oltp_point_select 20k+ QPS；
- 集群横向扩展到 100+ 节点；
- 写多读少：与 CockroachDB / TiDB 同档，吞吐与延迟关键看 partition 分布。

### 13.3 延迟分布

- 同 IDC 同 zone P99：2-10 ms（ob-server）；
- 同 IDC 跨 zone P99：10-30 ms；
- 跨城 P99（300 km）：30-100 ms；
- 列存副本 HTAP P99：100-500 ms（基于 redo log 重放延迟）。

### 13.4 TPC-H 与 HTAP

- TPC-H 30TB：OB 4.0 在 200+ 节点上刷新纪录（1.526 亿 QphH）；
- 列存副本的引入使得 OB 在保持事务能力的同时提供 10x OLAP 加速。

## 十四、运维与可观测性

### 14.1 OCP（OceanBase Cloud Platform / Enterprise 自带控制台）

OCP 提供：

- **总览**：集群健康（CPU / IO / 网络 / 延迟）；
- **租户列表**：每个 tenant 状态、资源、慢 SQL；
- **SQL 性能分析**：执行计划、慢查询、TopN；
- **备份管理**：全量 + 增量 + PITR；
- **告警与变更**；
- **部署模式**：单节点 / 多副本 / 高可用。

OCP 源码部分开源（`oceanbase/oceanbase` 子仓）。

### 14.2 OB 内置诊断视图

```sql
SELECT * FROM oceanbase.DBA_OB_SERVERS;
SELECT * FROM oceanbase.DBA_OB_TENANTS;
SELECT * FROM oceanbase.DBA_OB_LOG_STREAMS;
SELECT * FROM oceanbase.GV$OB_SERVER_STAT;
SELECT * FROM oceanbase.GV$SQL_AUDIT;
```

源码：`src/sql/engine/builtins/cmd/ob_admin_*.cpp`。

### 14.3 备份与恢复

- **物理备份**：通过 backup 直接拷贝 SST + redo log：
  - `ALTER SYSTEM BACKUP DATABASE ... TO 'oss://...';`
  - 由 Backup/Recovery Service（`src/rootserver/backup/`）协调；
- **增量** / **日志归档** / **PITR**：内建；
- **OSS / NFS / COS**：存储后端支持。

源码：`src/rootserver/backup/ob_backup_service.cpp`、`src/storage/backup/ob_backup_restore_proxy.cpp`。

### 14.4 OMS（数据迁移服务 / 商业）

- **OMS**：MySQL / Oracle / DB2 / Postgres → OB 实时增量 + 全量；
- **OB-DRDS / DRDS / 上下游融合**；
- **OBKV**：KV 接口；
- **OB MySQL Binlog** 兼容（binlog_format = ROW）。

### 14.5 Prometheus / Grafana

OB 4.x 自带 `_metrics` Prometheus endpoint（基于 service 暴露）；

- 官方 Grafana JSON：`docs.yugabyte.com`（旧名 ob-doc）+ `open-ob/` 仓库 sample。

## 十五、生态与集成

### 15.1 客户端 SDK

| 语言 | 驱动 | 备注 |
|------|------|------|
| Go | `github.com/go-sql-driver/mysql` | 标准 |
| Java | MySQL Connector/J | 5.1.x + 8.0.x |
| Python | `mysqlclient` | 推荐 |
| Node.js | `mysql2` | 标准 |
| Rust | `sqlx-mysql` | 异步 |
| PHP | PDO_MySQL | 标准 |
| OBKV | `java/com.alipay.oceanbase` | OB 自己的 KV 接口 |

### 15.2 数据迁移工具

- **OMS（商业）**：实时迁移 + 全量同步；
- **OB-DRC**：数据复制服务（`oceanbase/ob-drc` 仓库）；
- **mysqldump / pg_dump → OMS Convertor**：从 MySQL/PG 数据源；
- **DataX**：阿里巴巴开源数据迁移，OB 已有 reader / writer 插件；
- **OBLOADER / OBDUMPER**：用于替代 mysqldump 兼容。

### 15.3 Kubernetes Operator

- `oceanbase/ob-operator`（K8s CRD）；
- 提供 `OBCluster` / `OBTenant` / `OBProxy` 等 CRD；
- 集成备份、恢复、自动 rebalance。

### 15.4 周边生态

- **DBeaver / DataGrip / TablePlus / Navicat**：标准 MySQL GUI；
- **Spark / Flink**：通过 connector；
- **BI**：Tableau / Power BI 直连；
- **DataX / Canal**：阿里巴巴生态适配；
- **OCEA 联盟**：OB 社区贡献插件、工具。

## 十六、核心论文与算法原理

- **Paxos 论文**：Lamport, "The Part-Time Parliament", 1998；
- **Lamport "Paxos Made Simple"** 2001：OB 自研 Palf 来源；
- **Spanner 论文**（OSDI 2012）：OB 的设计哲学对照（OB 走 Paxos 而非 Spanner 自研协议）；
- **LSM-Tree 论文**（1996）：OB 的存储引擎基线；
- **Raft 论文**（2014）：对比（Paxos 实现更精细）；
- **Multi-Paxos** 相关多项工程论文；
- **Cascades 论文**：OB 4.x 优化器实现参考；
- **Snapshot Isolation 论文**（Berenson et al., 1995）：事务隔离级别依据；
- **Oracle 兼容性**：受 Oracle 数据库白皮书启发。

## 十七、源码学习路径

### 17.1 推荐阅读顺序

1. **README.md + `docs/wiki/`** （仓库根 / wiki）；
2. **`src/observer/main.cpp`** — Observer 进程入口与启动流程；
3. **`src/observer/ob_heartbeat.cpp`** — 心跳与节点注册；
4. **`src/logservice/palf/palf_handle_impl.cpp`** — Paxos 副本核心；
5. **`src/storage/ls/ob_ls.cpp`** — LogStream 状态机；
6. **`src/storage/blocksstable/ob_block_manager.cpp`** — Macro Block 管理；
7. **`src/storage/memtable/ob_memtable.cpp`** — MemTable；
8. **`src/storage/transaction/ob_trans_ctx.cpp`** — 事务上下文；
9. **`src/sql/optimizer/ob_optimizer.cpp`** + **`src/sql/executor/`** — SQL 层；
10. **`src/rootserver/ob_root_service.cpp`** — RootService 总控；

### 17.2 关键文件清单

- `src/observer/main.cpp:1`
- `src/observer/ob_heartbeat.cpp:1`
- `src/observer/ob_server_main.cpp:1`
- `src/logservice/palf/palf_handle_impl.cpp:1`
- `src/logservice/palf/palf_options.h:1`
- `src/storage/ls/ob_ls.cpp:1`
- `src/storage/ls/ob_ls_meta.cpp:1`
- `src/storage/blocksstable/ob_block_manager.cpp:1`
- `src/storage/blocksstable/ob_macro_block_writer.cpp:1`
- `src/storage/blocksstable/cs_encoding/`
- `src/storage/memtable/ob_memtable.cpp:1`
- `src/storage/transaction/ob_trans_ctx.cpp:1`
- `src/sql/optimizer/ob_optimizer.cpp:1`
- `src/sql/parser/parse_node.cpp:1`
- `src/sql/executor/execute.cpp`
- `src/sql/engine/exec/ob_engine_executor.cpp`
- `src/sql/ob_end_trans_callback.cpp:1`
- `src/rootserver/ob_root_service.cpp:1`
- `src/rootserver/ob_ddl_service.cpp:1`
- `src/share/ob_schema_proxy.cpp`

### 17.3 关键测试文件

- `unittest/storage/`（大量 LSM-Tree 单元测试）；
- `unittest/logservice/palf/`（Paxos 单测）；
- `unittest/sql/optimizer/`；
- `unittest/share/`（系统表 schema 测试）；
- `integration_test/`：集群级别回归。

## 十八、源码规模与开发语言

- **语言**：C++ 为主（`src/*`），少量 C（GCC for hot path），C 兼容头；Java（OMS、OCP 等子仓）；
- **代码规模**：master 分支约 350 万行（含 src/*、unittest、cmake_modules）；
- **构建系统**：CMake + 自定义脚本（`build.sh`、`deps.sh`）；
- **依赖管理**：CMake `find_package`、conan-like 内部工具；
- **格式化**：clang-format 强制（仓库根 `.clang-format`）；
- **跨平台**：Linux x86_64 主流；macOS / ARM 适配；Windows 不主推。

## 十九、学习资源

- **官方文档**：`https://en.oceanbase.com/docs/` 与 `https://www.oceanbase.com/docs/`（中文）；
- **中文社区文档**：`https://www.oceanbase.com/docs/community/observer-doc/`；
- **OceanBase 官方公众号**、知乎专栏、GitBook 教程；
- **GitHub Discussions / Issues**：`oceanbase/oceanbase`;
- **OB 训练营**：OCA / OCP / OCE 三级认证（培训合作伙伴）；
- **OB 大赛**：年度开发者大赛（双年大事件）；
- **Conference Talks**：SIGMOD、VLDB、ICDE、CSDN Live、KCon、ArchSummit；
- **博客与论文**：OB 团队官方 release blog、ACM SIGMOD 论文 `OceanBase: A 707 Million tpmC Distributed Relational Database`、`OceanBase's HTAP`;
- **培训视频**：B 站"蚂蚁课堂" OB 系列。

## 二十、可移植性与部署

- **二进制**：`ob-server` 静态/动态链接；
- **平台**：Linux x86_64、arm64（部分主推），其他平台少；
- **Docker**：`oceanbase/oceanbase`：全套镜像；
- **Kubernetes**：`oceanbase/ob-operator`；
- **OAT（Open-Source Admin Toolkit）**：基于 Docker-Compose 单机体验集群；
- **云**：阿里云 OceanBase Cloud、AWS / Azure / 华为云合作；
- **多语言集群**：OB Server / OBProxy 缺一不可；OCP / OMS 商业组件可替换为开源版。

## 二十一、变体与衍生项目

- **OceanBase CE**（社区版）：核心全开放，部署门槛低；
- **OceanBase EE**（商业版）：OMS、OCP、ODP、Profiler；
- **OceanBase Cloud**：阿里云服务（China / Global）；
- **OB-bench-repo**：基准测试脚本；
- **OB-DRC**：`oceanbase/ob-drc` 数据库复制服务；
- **OBPlugin (OAS)**：面向 MySQL 兼容性 API 的"伪装"接口；
- **OBKV**：独立 KV API（兼容 HBase / Redis 协议）。

## 二十二、风险与陷阱

| 风险 | 缓解 |
|------|------|
| **默认 READ COMMITTED**：金融场景误用 | 显式 `SET ISOLATION = SERIALIZABLE`；并发回滚靠应用 |
| **MySQL 兼容不完整**：DDL/DML 部分语法未支持 | 参考 `docs/community/compat-list`；事先 mock 测试 |
| **Partition 设计不当**：分区不均匀导致热点 | 推荐 HASH 分区 + 周期评估 partition size |
| **大 DDL 阻塞事务**：长时间主 DDL 会长时间取锁 | 用 `ALTER TABLE ... ALGORITHM = INSTANT`（4.x 部分支持） |
| **Paxos commit latency**：跨 IDC 写入 P99 较高 | 业务就近写；可考虑双主读多写 |
| **租户资源竞争**：多租户共享集群 OOM | 用 `ALTER TENANT UNIT_NUM` 调配额；监控 |
| **备份链长**：PITR 与 log_archive 长期留存复杂 | 监控 `backup_set_list`；清理过期 backup； |
| **Schema 演进**：ClogPB schema_version 切换期间 IO 抖动 | 低峰期切换；监控 `schema_op` |
| **资源配额**：4.x 多租户资源池化配置复杂 | 单元化培训；通过 OCP UI 调 |
| **OBProxy 单点**：OBProxy 是无状态，可水平扩展 | 至少 2 节点 OBProxy + 一致性 hash |

## 二十三、与本项目的关联

- **存储引擎**：`engineering/src/db/storage/` 中的 heap + btree；OB 的 LSM-Tree + Macro Block 是工业级实现，可作 maxite-level 学习对照（但 OB 太工业，入门推荐从 CockroachDB / RocksDB 开始）。
- **共识协议**：`engineering/src/dist/` 中已实现的 raft.h/c + 2PC demo（与 OB 的 Paxos 路径不同，可作对比参考）。
- **SQL 解析器**：`engineering/src/sql/sql_lex.c` 手写；OB 沿用 MySQL 兼容的 yacc/lex（`src/sql/parser/`）。
- **事务**：`engineering/src/db/tx.h` 简单事务 vs OB 的多分区 2PC + 强一致快照。
- **学习轨参考**：`learning/notes/` 中已含 NewSQL、Paxos、HLC 相关章节；OB 的具体 API 可作为产业落地案例补充。
- **分布式存储参考**：`learning/scaffold/cpp/dist_kv/`（如存在）可与 OB 的 LogStream 对照看。

## 二十五、Paxos 状态机与 Leader Lease 协议（算法深度）

§十.3 已概述 Paxos / Multi-Paxos / Learner / 配置变更的角色分工；本节聚焦**算法伪代码与状态转移**，补足实现细节。所有源码路径均基于公开 OB 4.x master 头部位置（`reference/distributed-sql/oceanbase/` 子模块**未 init**，下列行号均为 approx 估算，仅作入口参考）。

### 25.1 Multi-Paxos 角色与状态机

OB 的 Palf（Parallel Log Framework）将"一条 LogStream = 一组 Paxos 实例"实现为**稳定 Leader + log batching**：

```
角色：
  - Proposer  = 当前 LogStream 的 Leader replica
  - Acceptor  = 全部 N 个 Paxos 副本（含 Leader 自身）
  - Learner   = 所有副本（含 ReadOnly Columnar Replica）

单 Paxos 实例（instance i）状态：
  state.proposal_id  ← 单调递增 (leader_id 嵌入高 32 位，本地 ballot 嵌入低 32 位)
  state.accepted_id  ← 最近一次 Promise 的 proposal_id
  state.accepted_val ← 最近一次 Accepted 的 log entry

Multi-Paxos 优化：
  - Stable Leader：leader 任期内的 Prepare/Promise 只需一次；log entry 可跳过
    Prepare 阶段（仅记录当前 leader ballot，replicas 已认可）
  - Log Batching：leader 把多条 redo log 聚合成一个 slab 走一次 Accept，
    减少 RPC 往返
```

### 25.2 Multi-Paxos Accept 路径伪代码

```
function LEADER_ACCEPT(entry: LogEntry):
    # 本轮 leader 任期，proposal_id 由 (term, slot) 推得
    pid = (current_term, monotonically_increasing_local_ballot)

    # 单副本内：本地写日志 + 同步给 followers
    LDR_localAppend(entry, pid)
    if quorum_send(entry, pid) == MAJORITY:  # N/2+1 acks
        commit_index = max(commit_index, entry.lsn)
        # leader 把 commit point 推给 followers（async）
        broadcast_commit(commit_index)
        return COMMITTED
    else:
        return REJECTED  # 触发重新 Prepare 或切主
```

复杂度：
- **单次 Accept RPC 延迟**：1 × RTT（leader → majority followers）；
- **失败重试**：O(retries) × RTT，重试期间 leadership 变化需重新 Prepare（额外 1 RTT）。

源码入口：`src/logservice/palf/palf_handle_impl.cpp` (approx, 模块未 init，行号需校对)；`src/logservice/palf/palf_kv_cb_engine.cpp`。

### 25.3 Leader Lease 与弱读（Weakly Consistent Read）

OB 4.x 在 Paxos 之上叠加 **leader-based lease** 以支持**弱一致性读**（无 redo log 走读路径，直接读 follower）：

```
参数：weak_read_lease_duration_ms（默认 5000 ms）
状态：lease_start_ts, lease_end_ts（follower 侧记录）

Leader 续约（每 lease/3 时间间隔）：
  function LEADER_RENEW_LEASE():
      now = monotonic_clock()
      if now >= lease_end_ts - lease_duration/3:
          lease_end_ts = now + weak_read_lease_duration_ms
          # 把 lease 走 Paxos Accept 同步到 majority
          ACCEPT_LOG(LeaseRecord{renew_ts=now, expire_ts=lease_end_ts})

Follower 弱读：
  function FOLLOWER_WEAK_READ(snapshot_ts):
      if lease_start_ts <= snapshot_ts <= lease_end_ts - now():
          # 在 lease 窗口内，leader 不会切主 → 可线性读
          return local_snapshot_read(snapshot_ts)
      else:
          # 退化为强读：转 leader 走 read index 或 round-robin
          return STRONG_READ(snapshot_ts)
```

要点：
- **强读路径**仍走 Paxos log position（§10.2 提到的 `read snapshot_version`）；
- **弱读路径**仅在 lease 窗口内安全，避免 follower 读旧 leader 的脏数据；
- 与 Spanner 的 TrueTime 不同，OB 不依赖硬件时钟同步，而是依赖 **Paxos lease 上的单调时间**。

源码入口：`src/logservice/palf/palf_handle_impl.cpp` 中的 lease 续约循环（approx, 模块未 init，行号需校对）。

### 25.4 配置变更：Joint Consensus 简化版

OB 在副本增删时使用**类 Joint Paxos**两阶段切换，避免双主：

```
function CHANGE_REPLICAS(zone_list: list):
    Phase 1 (Prepare):
      1. 旧 leader 把 C_old 配置封入 log entry（仍走 Quorum_C_old）
      2. 一旦 Quorum_C_old ack，进入 Phase 2
    Phase 2 (Commit):
      3. 旧 leader 把 C_new 配置封入 log entry
      4. Quorum_{C_old ∪ C_new} ack 后，C_new 生效，新 leader 由 C_new 选出
    期间读写由 C_old 继续服务；不存在双 leader 同时接受写的窗口。
```

源码入口：`src/logservice/palf/ob_palf_replica_config_handler.cpp` (approx, 模块未 init，行号需校对)。

---

## 二十六、LSM-Tree 合并策略与全局时间戳分配

§七.1 给出 LSM-Tree + Macro Block 的概念图，§10.4 仅 3 行提及全局时间戳；本节深挖**合并策略选择算法、Macro Block cost 估算、TSO 中央分配器**，补足实现算法。

### 26.1 合并层级与触发条件

OB 的合并按影响范围分三类：

| 合并类型 | 范围 | 触发条件 | 阻塞写入 |
|----------|------|----------|----------|
| **minor freeze** | MemTable → 0 层 SST | MemTable 内存超阈值（`freeze_trigger_percentage`，默认 50%） | 仅冻结的 MemTable |
| **major freeze** | 全部 SST 全量合并为快照 | 显式 `ALTER SYSTEM MAJOR FREEZE` 或调度策略 | 是，影响所有 SST |
| **daily merge** | 多层 SST 选择性合并 | 后台调度 + 成本阈值 | 否，后台任务 |

### 26.2 Minor / Major Freeze 时序伪代码

```
function MAYBE_TRIGGER_FREEZE(ls: LogStream):
    mem_used = sum(memtables[].approx_size())
    mem_cap  = ls.memory_limit_bytes

    if mem_used >= mem_cap * freeze_trigger_percentage / 100:
        # minor freeze: 把 active memtable 切为 immutable，flush 为 0 层 SST
        immutable = memtables[active].freeze()
        flush_immutable_to_sst_0(immutable)   # 异步磁盘写

    if scheduler_major_freeze_window():
        # major freeze: 沿 schema_version 链全量合并
        snapshot = pick_snapshot_version_for_major()
        parallel_for_each(sst_level in 0..N):
            merge_into_new_sst(sst_level, snapshot)

    if cluster_2pc_coordinator_signals_merge():
        # 分布式协调器（RootService）下达的合并指令
        execute_scheduled_merge(task_descriptor)
```

### 26.3 合并 Cost 估算

OB 合并选择器为每个候选 SST 集合估算 cost（读取放大 + 写入放大 + 空间放大），选 cost 最低者：

```
cost(L_selected) = w_r · read_amp(L_selected)         # 读放大
               + w_w · write_amp(L_selected)          # 写放大
               + w_s · space_amp(L_selected)          # 空间放大

其中：
  read_amp = sum(sst.selected[i].bloom_size + index_size) / row_count
  write_amp = sum(rewrite_bytes(sst.selected)) / total_bytes
  space_amp = total_size_with_overlap / total_unique_size

权重 w_r, w_w, w_s 默认 1:1:1（可通过 GUC 微调）
```

启发式：OB 不像 RocksDB 那样追求严格的 level-tiered，而是 **"按 range 选择 + 周期性 major"**——通过 major freeze 兜底，避免 read amp 无限增长。

源码入口：`src/storage/compaction/ob_compaction_choice_provider.cpp`、`src/storage/compaction/ob_merge_task.cpp` (approx, 模块未 init，行号需校对)。

### 26.4 全局时间戳分配（TSO / GTS）

OB 不依赖外部时钟，而用 **Paxos log position 作为逻辑时钟**；同时存在一个**单机版 Timestamp Oracle**（`GTS`）用于单分区事务。

```
中央分配器（单分区内）：
  function GTS_NEXT(slots=200):
      # slots 是批量分配数量，避免单点 RPC
      atime = monotonic_clock_ns()
      ret = atomic_fetch_add(global_ts_counter, slots)
      return (atime << 20) | ret   # 高 44 位物理时间，低 20 位逻辑计数

事务层调用：
  function TRANS_BEGIN():
      ts_read  = GTS_NEXT()         # 读快照版本
      ts_write = GTS_NEXT()         # 写入 commit 版本
      txn = TxContext(read_ts=ts_read, write_ts=ts_write)
      return txn

  function TRANS_COMMIT(txn):
      # write_ts 必须比所有冲突事务的 read_ts 都大
      assert(ts_write > max(conflict_read_ts))
      APPLY_VIA_PAXOS(txn)         # 走 §10.2 描述的 2PC + Multi-Paxos
```

注：
- GTS 是 **per-zone** 中央分配（并不是全局唯一），跨 zone 事务还需 Paxos log position 兜底；
- 这与 Spanner TrueTime 不同——OB 牺牲"全局同步时钟"换取"协议实现纯粹度"；
- 事务 commit 时如果 `ts_write` 已被其他事务占用，需 retry（serializable isolation 下）。

源码入口：`src/storage/transaction/ob_trans_ctx.cpp`、`src/observer/ob_observer_func.cpp`（含 GTS 启动逻辑）(approx, 模块未 init，行号需校对)。

### 26.5 读路径与 schema_version 协调

OB 的合并强 schema 约束——读路径需找到对应的 schema_version：

```
function POINT_GET(key, schema_version):
    # schema_version 由 schema service 提供；通常表结构变更后 +1
    sst_idx = memtable_binary_search(key, ts=read_snapshot)

    # 0 层 SST 是按 schema_version 分桶的
    sst = sst_layer_0.lookup(key, schema_version=schema_version)
    if sst is None:
        sst = sst_layer_1plus.lookup(key, schema_version)

    if sst.column_meta[schema_version] exists:
        return decode_row(sst, key, columns_requested)
    else:
        # schema 已演进到新版本，需要 column mapping
        return materialize_via_column_mapping(sst, key, old_to_new_schema)
```

要点：
- ClogPB 列存天然支持 schema 演进（每列独立写新 SST，旧 SST 可读旧 schema）；
- 合并时不同 schema_version 的 SST 不会相互覆盖，避免跨版本 IO 抖动。

源码入口：`src/storage/blocksstable/ob_block_manager.cpp`、`src/storage/blocksstable/cs_encoding/ob_column_oriented_encoding.cpp`、`src/share/schema/ob_column_schema.cpp` (approx, 模块未 init，行号需校对)。

---

## 二十四、参考资料

1. OceanBase 官方文档（中文）：https://www.oceanbase.com/docs/
2. OceanBase 官方文档（英文）：https://en.oceanbase.com/docs/
3. OceanBase 社区 wiki：https://www.oceanbase.com/docs/community/
4. The Part-Time Parliament (Lamport, 1998)：https://lamport.azurewebsites.net/pubs/lamport-paxos.pdf
5. Paxos Made Simple (Lamport, 2001)：https://lamport.azurewebsites.net/pubs/paxos-simple.pdf
6. Spanner (OSDI 2012)：https://research.google/pubs/spanner-googles-globally-distributed-database/
7. Raft (USENIX ATC 2014)：https://raft.github.io/raft.pdf
8. LSM-Tree (O'Neil et al., 1996)：http://www.cs.berkeley.edu/~brewer/cs262/LCS.pdf
9. OceanBase GitHub：https://github.com/oceanbase/oceanbase
10. OceanBase TPC-C / TPC-H 历史纪录：https://www.oceanbase.com/about/news/ (搜索 "tpmC" / "tpc-h")。
