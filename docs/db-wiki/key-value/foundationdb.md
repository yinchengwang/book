# FoundationDB Wiki（分布式强一致键值数据库）

> 参考版本：FoundationDB 仓库当前 commit `b7bb49457086606ab382b0b6f7355d78a2faf05f`（v7.x 系列；2024 年起由 Apple 开源维护）。

## 一、项目定位

FoundationDB（简称 FDB）是一款**支持跨键 ACID 事务、强一致、分布式、可线性扩展的开源键值数据库**。它由 Apple 在 2015 年收购后于 2018 年开源（GPLv3 + Commercial 双许可），定位是**"在 KV 接口下提供关系数据库般的强一致事务能力"**，并通过 deterministic simulation 测试保证极高的可靠性。它采用独特的"分层协调 + 单库进程 + 多模态层"架构，对外是 KV 接口，对上可构建 SQL、文档、图等层次（"Record Layer"）。

- **痛点**：
  - 传统分布式 KV（如 Dynamo、Cassandra）只提供最终一致，跨 key 事务弱；
  - 关系数据库（如 MySQL）难以横向扩展，事务模型依赖集中式锁；
  - 大规模云原生时代需要"既可扩展又强一致"的引擎；
  - 强一致系统测试极难，deterministic simulation 是工业级的解决思路；
  - 需要一个可以在 KV 上构建 SQL / 文档 / 图等多模态的统一底层。
- **典型用户**：Apple（icloud、apple.com 后端多个服务）；Snowflake（早期架构参考）；Wavefront；社区案例如 FoundationDB-Record-Layer（SQL/索引层）、FoundationDB-Net（分布对象）、DocLayer（文档）、Theo Graph（图的页面）。

## 二、发展历史与社区

- **起源**：2009 年 Dave Rosenthal、Nick Lavezzo、Nathan Ho（创始人）在加州创立 FoundationDB；最初设计为"scale-out 关系数据库"，后转向分布式 KV；
- **里程碑**：
  - 2010-2012 年产品化与种子轮融资；
  - 2013 年发布 1.0，提供 strict serializability + 跨 key ACID；
  - 2015 年被 Apple 收购，转为闭源；
  - 2018 年 4 月重新开源（GPLv3 + Commercial）；
  - 2019 年 Record Layer（SQL/索引层）开源；
  - 2020-2021 年 SimulatedFS、Configurable Databases 增强；
  - 2022 年 v7.x（Massively Parallel Reads、BackupEngine 增强等）；
  - 2023-2024 年社区维护活跃，新增多模态支持；
- **维护状态**：Apple 团队核心维护 + 社区贡献；近 3 年活跃度高；
- **社区**：FoundationDB Forum、Slack、GitHub Discussions、Apple OSS 邮件列表。

## 三、许可与商业限制

- **许可证**：**GPLv3**（AGPLv3 类似）+ **Commercial License** 双许可；
  - 来源：`foundationdb/LICENSE`；
  - 关键约束：开源使用 GPLv3；商用（含 SaaS 化分发）需要 Apple 商业许可；
- **商业版本**：Apple 自营（icloud 内部使用），社区版功能与商用版差异较小；Apple 提供商业技术支持；
- **Record Layer 兼容**：与 FDB Core 同样 GPLv3 + Commercial 双许可；
- **生态影响**：GPLv3 影响二次分发；许多用户选择静态链接到自己应用中（不触发分发条款）。

## 四、业界主要使用方与案例

- **Apple**：icloud、Apple Maps、apple.com、Siri 部分服务、Apple Music 部分子系统；
- **Snowflake**：早期架构重要参考（但实际未使用 FDB）；
- **Wavefront**：时序数据库后端（Wavefront by VMware）；
- **社区案例**：
  - **FoundationDB-Record-Layer**：SQL/索引层（社区开发，类似 MySQL 表结构 + 索引）；
  - **DocLayer**：文档存储（MongoDB-like）；
  - **Theo Graph**：图数据库层（Neo4j-like）；
  - **FoundationDB-Net**：分布对象（Redis-like 集合）；
  - **SQLFauna**：SQL 兼容层；
- **学术**：CMU 6.824、Cornell、UC Berkeley 分布式系统课程参考。

## 五、一句话对比定位

> FoundationDB 是**"分布式 KV + 跨键 ACID + 严格可串行化 + Deterministic Simulation"**的独特引擎；与 **TiKV（分布式 KV + RocksDB + Raft，事务模型较弱）**、**CockroachDB（PostgreSQL 兼容 + Raft + Pebble）**、**FaunaDB（基于 FDB 的 Serverless DB）** 形成**强一致分布式 KV 阵营**。

## 六、整体架构

FoundationDB 采用**分层 + 单库进程 + 多模态层**架构。核心架构特点：

1. **Cluster Controller（CC）+ Master**：单库单 Master（可 failover），不依赖外部协调；
2. **Proxy**：无状态代理，转发读写请求到对应的 Storage Server；
3. **Storage Server**：存 B-Tree 数据（基于 SQLite 引擎）；
4. **Resolver**：负责跨 shard 事务冲突检测（维护 write conflict window）；
5. **Record Layer**：上层多模态（SQL、Document、Graph）；
6. **Deterministic Simulation**：核心测试方法，重放 actor 行为并保证可重现。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│         应用层 (Application / Record Layer / SQL Layer)               │
│   - Record Layer: SQL/索引/文档语义（fdb-record-layer/）             │
│   - 应用可基于 FDBClient 直接开发                                    │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ FDB Client API（基于 Actor + Flow）
                      │ 事务：begin / get / set / commit（pure 5ms）
┌─────────────────────▼────────────────────────────────────────────────┐
│         FDBClient 库 (fdbclient/)                                     │
│   - FDBClient.actor.h：FDB API 的 Actor 实现                         │
│   - NativeAPI.actor.h：与 FDBServer 通信的 .actor                    │
│   - Transaction.actor.h：事务管理（read version + commit version）   │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ RPC (Flow/Actor) → Coordination 层
┌─────────────────────▼────────────────────────────────────────────────┐
│         Coordination 层 (fdbserver/ + fdbclient/)                     │
│   - ClusterController（CC）：配置管理、Master 选举                   │
│   - Master：DDL 流程、Recovery、Resolver 协调、Shards Rebalancing    │
│   - Resolver：跨 shard 事务冲突检测、write conflict window           │
│   - Proxy：无状态路由（按 key range 路由到对应 SS）                  │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ 内部 RPC (GenericActivePacket / Buggify)
┌─────────────────────▼────────────────────────────────────────────────┐
│         Storage 层 (fdbserver/ + fdbserver/storage*)                  │
│   - StorageServer（SS）：存一个 shard (B-Tree on top of SQLite)       │
│   - TLogServer（TL）：Write-Ahead Log（保证 durability）             │
│   - DataDistributor：shard 重新分布                                   │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│         本地引擎 (storage/)                                            │
│   - B-Tree on top of SQLite（fdbserver/include/storage/SQLite3）   │
│   - Prefix-based 内存索引                                            │
│   - LRU Cache for pages                                               │
│   - DiskFile (.fdb 二进制文件 + per-shard SQLite db)                 │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ Disk I/O
┌─────────────────────▼────────────────────────────────────────────────┐
│         操作系统 / 磁盘 (Linux/SSD/HHD)                                │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- 客户端入口：`fdbclient/include/fdbclient/FDBOptions.h`
- 服务端入口：`fdbserver/ServerLoop.actor.cpp:ServerLoop()`
- Master：`fdbserver/MasterServer.actor.cpp`
- Storage Server：`fdbserver/StorageServer.actor.cpp`

### 6.2 进程模型

FDB Cluster 包含**多种进程角色**，每个进程多线程（基于 Flow Actor）：

- **fdbserver 进程**：可担任任意角色（CC/Master/Proxy/Resolver/SS/TLog）；
- **单库多角色**：每进程多线程，每个线程跑一个 Actor（基于 `flow/` Actor 框架）；
- **Actor 模型**：`flow/flow.h` 实现 Actor 通信（无锁消息队列 + 单线程事件循环）；
- **线程隔离**：每个 Actor 跑在自己的线程（避免共享状态）；通过消息 + future 通信；
- **fdbmonitor**：监控并按需重启 fdbserver 进程；
- **fdbcli**：命令行管理工具（status / configure / exclude / include）。

### 6.3 一条 BEGIN + PUT + COMMIT 命令的端到端路径

1. **应用调用**：`FDB::Transaction tr(db); tr.set(key, value); auto v = await tr.commit();`
2. **Begin Transaction**：从 Proxy 获取 read version（`m_version`）和 commit version（暂未分配）；
3. **Get**：从 Proxy 路由到目标 SS，SS 查询 B-Tree（SQLite）；
4. **Set**：暂存本地 mutations（`tr.mutations`），未发送给 SS；
5. **Commit**：
   - 检查冲突：本地 mutations 与 read version 之后的事务是否有冲突（通过 Proxy 向 Resolver 询问）；
   - 如无冲突 → 分配 commit version；
   - 发送 mutation log 到 TLogServer（write ahead log）；
   - Proxy 通知 SS 写入数据 + 更新 TLog 指针；
   - 响应 client commit version。
6. **异常处理**：
   - Conflict（事务竞争）：返回 `transaction_conflict`，应用需重试；
   - Process Failure（proxy、SS、TL）：client 自动重试其他 proxy。

### 6.4 Deterministic Simulation（核心创新）

`fdbserver/simulation/`:

- **目标**：在测试环境复现整个 cluster 行为，每个 actor 决策由随机种子决定；
- **测试时序控制**：用虚拟时钟跑大量不同随机种子的 simulation；
- **故障注入**：network partition、machine failure、disk corruption 都能精确注入；
- **代码不变**：生产代码与 simulation 代码完全一致；
- **收益**：发现传统测试难以发现的并发 bug；保证 fault tolerance 路径正确。

## 七、存储引擎（KV 类型核心库深度展开）

FoundationDB 的存储引擎独特之处：**B-Tree on top of SQLite**，而非 RocksDB 之类的 LSM-Tree。

### 7.1 总体引擎架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                  FDB Storage Server 引擎                               │
│   ┌──────────────────────────────────────────────────────────────┐   │
│   │  Write-Ahead Log (fdbserver/storage/)                        │   │
│   │  - TLogServer 维护：append-only mutations + undo log          │   │
│   │  - Snapshot：旧版本（用于 read version 历史读）               │   │
│   │  - 清理：超过 versionBytes 自动 tlog pop                      │   │
│   └──────────────────────────────────────────────────────────────┘   │
│   ┌──────────────────────────────────────────────────────────────┐   │
│   │  Memory Index (fdbserver/include/storage/Memory/)             │   │
│   │  - Page Cache：最近访问的 data page（LRU）                    │   │
│   │  - Versioned BTree（in-memory）                               │   │
│   │  - MemTable：写入批次（commit 后 flush 到 SQLite B-Tree）     │   │
│   └──────────────────────────────────────────────────────────────┘   │
│   ┌──────────────────────────────────────────────────────────────┐   │
│   │  Persistent Storage (fdbserver/storage/SqliteDB)              │   │
│   │  - 每个 shard 一个 SQLite database                            │   │
│   │  - B-Tree 数据结构（sqlite3 内置）                            │   │
│   │  - KV 接口：key/value 都以 binary blob 存                     │   │
│   │  - Native Engine：未来可替换 SQLite（foundationdb 6.x+）      │   │
│   └──────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

### 7.2 SQLite as B-Tree Engine

`fdbserver/storage/SqliteDB.{h,cpp}`:

要点：
- **每个 shard 一个 SQLite db**：fdbserver 用 SQLite API 而非自定义 B-Tree；
- **Schema 简单**：`keyval(8 bytes), versionstamp(10 bytes), data BLOB`；
- **MVCC 实现**：每个 key 多个版本通过 versionstamp 区分；
- **优势**：稳定性高（SQLite 久经验证），B-Tree 实现成熟；
- **劣势**：写吞吐受限（SQLite 写串行化）；FDB 用 commit pipeline + 多 SS 缓解。

### 7.3 Native Engine（v7+）

`fdbserver/include/storage/Native`:

- **New B-Tree Engine**：原生 C++ 实现替换 SQLite；
- **优点**：性能更高（多线程读写）、更可控；
- **架构**：类似 SQLite 的 B-Tree，但用 sharded cache + custom WAL；
- **状态**：v7 默认启用（v6.x 实验性）；未来可能完全替代 SQLite。

### 7.4 DataDistributor & Shard Rekeying

`fdbserver/DataDistributor.actor.cpp`:

- **Shard 概念**：基于 key range 的逻辑分片（默认 100MB / shard）；
- **数据分布**：每个 SS 负责若干 shard；TLogServer 保存多个 SS 的 mutation log；
- **Rebalance**：DataDistributor 按 SS 容量触发 rebalance（搬迁 shard）；
- **Re-keying**：shard 拆分（当 shard > 100MB）；
- **Granularity**：granule 概念（v7+）提供更高粒度（10MB 块）的负载均衡。

### 7.5 TLog（Write-Ahead Log）

`fdbserver/TLogServer.actor.cpp`:

- **持久化 mutation log**：所有 commit 的 mutation 都写到 TLogServer；
- **Backup**：TLogServer 定期生成 snapshot + log snapshot；
- **Pop**：旧 TLog 数据在 SS 持久化后清理；
- **Sequence**：TLog 也按 shard 分组，每个 shard 一个 TLogServer；
- **TlogSpill**：TLog 满时 spill 到磁盘。

### 7.6 Resolver（冲突检测）

`fdbserver/Resolver.actor.cpp`:

- **写冲突窗口**：维护 read version 之后的所有 write；
- **冲突检测**：transaction commit 时检查 read keys 是否被其他事务修改；
- **MVCC 基础**：基于 read version + commit version 实现 strict serializability；
- **扩容**：Resolver 自身是分布式的（按 key range 分片）。

### 7.7 Master（协调）

`fdbserver/MasterServer.actor.cpp`:

- **DDL**：createDatabase / changeConfig 等；
- **Resolvers 协调**：管理 resolver 注册；
- **Backup / Restore 协调**；
- **Ratekeeper**：动态限制 client transaction rate（避免 load 过大导致冲突风暴）；
- **DataDistributor 触发**：rebalance / re-shard 触发；
- **故障恢复**：cluster recovery（init / recover / alive）。

## 八、索引机制

FDB 本身**不内置二级索引**，但提供以下机制：

- **主索引**：B-Tree on SQLite（自然有序范围扫描）；
- **Directory Layer**（`fdbclient/include/fdbclient/Directory.h`）：逻辑目录抽象（前缀 + 元数据）；
- **Watch**（`FDB::Transaction::watch()`）：监听 key 变化，类似 Redis keyspace notification；
- **Conflict Range**：事务 setWatch + commit 时若 key 变化则回滚；
- **Record Layer**（上层）：SQL 索引、二级索引、全文索引等。

## 九、查询/计算/接口层

- **客户端 API**：C/C++/Java/Go/Python/Ruby 多语言绑定；
- **事务 API**：基于 5ms 事务 timeout（可配 `transaction_timeout`）；
- **范围查询**：`Transaction::getRange(begin, end, limit)`，流式返回；
- **CRDT**：`Atomic<int>` / `Stack<T>` / `Set<T>` / `IndexSet<T>` 等高级类型；
- **Tuple 层**：`Tuple` 类型支持嵌套 + 二进制编码（高效序列化）；
- **Record Layer**（SQL）：完整 SQL 支持 + 索引；
- **执行模型**：纯 Actor 异步（基于 Flow future + promise）。

## 十、事务与一致性（KV 类型相对较强）

FDB 提供**业界最强的一致性保证**之一：

- **ACID 跨 key 事务**：单事务可修改任意多个 key；
- **Strict Serializability**：可串行化 + 实时性（外存顺序 = 实时顺序）；
- **MVCC 实现**：基于 read version + commit version；
- **冲突处理**：
  - **Write Conflict**：事务冲突由 Resolver 检测；
  - **Process Failure**：client 自动重试（transparent retry）；
  - **Read Conflict**：默认 snapshot isolation；可配 `read_conflict_retry`；
- **隔离级别**：Strict Serializable（最强）；
- **持久化一致性**：依赖 TLogServer 的 fsync（默认 `DurabilityVersion` 保证）；
- **Two-Phase Commit**：内部 SS + TLog 协调，非用户层 2PC；
- **事务 timeout**：默认 5 秒；超时返回 `transaction_timed_out`。

## 十一、复制与高可用

FDB 集群默认采用**多副本容错 + 多数派协议**：

- **副本数**：可配 1 / 2 / 3 / `tLog_replicas` / `storage_replicas`；
- **Single 模式**：单副本（开发）；
- **Double 模式**：双副本（容忍一节点故障）；
- **Triple 模式**：三副本（容忍两节点故障）；
- **故障检测**：基于 gRPC + heartbeat；
- **Master Failover**：主 master 故障时新 master 自动选主；
- **SS/TL Failover**：状态从 TLog 恢复；
- **Process Recovery**：进程崩溃后重启并从 disk + TLog 重建状态；
- **Cluster Recovery**：集群崩溃后 init + recover；
- **Network Partition**：FDB 设计假设 partition 时少数派可用，多数派继续服务；
- **Backup & Restore**：TLog + Snapshot 备份到 S3 / 文件系统。

## 十二、扩展性

- **横向扩展**：增加 SS/TLogServer 节点即可扩容；
- **自动分片**：DataDistributor 自动 rebalance shard；
- **Re-keying**：shard 自动拆分；
- **Granule**（v7+）：更高粒度（10MB）负载均衡；
- **Read Snapshot**：可读历史版本（基于 TLog snapshot）；
- **Read Version**：client 可指定 read version（time travel）；
- **动态扩容**：可通过 `configure` 命令加节点。

## 十三、性能与基准

- **Apple 生产数据**：单 cluster 可达百万级 QPS（带事务）；
- **Sequential Read**（范围扫描）：可超过 100K ops/s per SS；
- **Random Point Get**：~50-100K ops/s per SS（取决于 value size）；
- **事务延迟**：默认 5ms timeout 内的成功事务；
- **冲突代价**：高冲突率下 throughput 急剧下降（需 ratekeeper 调节）；
- **YCSB**：FDB 在 conflict-free 场景下表现良好；
- **关键指标**：每次 commit 至少 1 个 TLog fsync + N 个 SS 写（强一致代价）。

## 十四、运维与可观测性

- **监控指标**：`fdbcli status` + `/status/json` 完整状态；
- **状态指标**：role / fault state / data distribution / durability / qos；
- **慢事务**：未明确暴露，但可通过 trace 收集；
- **日志**：每个进程独立的 trace event 文件（fdbserver.log）；
- **Backup**：TLog + snapshot 周期备份（`backup_agent` 进程）；
- **Restore**：`fdbcli restore` 命令；
- **Prometheus 集成**：官方 `fdb-prometheus-exporter`；
- **tracing**：基于 Flow trace + actor events。

## 十五、生态与集成

- **客户端 SDK**：C++（`libfdb_c`）、Java（`foundationdb-clients`）、Go（`foundationdb-go`）、Python（`python-fdb`）、Ruby（`fdb-ruby`）、Node.js（`foundationdb` npm）；
- **Record Layer**：Java SQL + 索引层（`foundationdb-record-layer`）；
- **多模态层**：
  - **DocLayer**：文档；
  - **Theo Graph**：图；
  - **FoundationDB-Net**：Redis-like 集合；
  - **DB Comparer**：迁移工具；
- **消息队列**：可基于 FDB 实现 Kafka-like（Kadho DB 等）；
- **容器化**：官方 Docker（`foundationdb/foundationdb`）、Kubernetes Operator（community）；
- **BI 工具**：通过 Record Layer JDBC 集成（受限）。

## 十六、变体与衍生项目

- **Record Layer**：SQL + 索引层（社区主导）；
- **DocLayer**：文档存储层；
- **Theo Graph**：图数据库层；
- **FoundationDB-Net**：分布对象抽象；
- **FaunaDB**：基于 FDB 的 Serverless 数据库（Fauna Inc.）；
- **CockroachDB / TiKV**：受到 FDB 启发但采用不同实现；
- **Eraser Coding / PolarFS**：Apple 自研文件层（PolarFS 是社区版）；

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `flow/Actor.h`：Actor 模型 + future + promise；
  - 阅读 `fdbclient/include/fdbclient/FDBOptions.h`：配置；
  - 阅读 `fdbclient/include/fdbclient/NativeAPI.actor.h`：API 定义；
  - 阅读 `fdbserver/ServerLoop.actor.cpp`：服务端入口；
  - 阅读 `fdbserver/ClusterController.actor.cpp`：集群控制。
- **进阶（2-3 周）**：
  - 阅读 `fdbserver/MasterServer.actor.cpp`：Master 协调；
  - 阅读 `fdbserver/TLogServer.actor.cpp`：TLog 实现；
  - 阅读 `fdbserver/StorageServer.actor.cpp`：Storage Server；
  - 阅读 `fdbserver/Resolver.actor.cpp`：冲突检测；
  - 阅读 `fdbserver/include/storage/SqliteDB.h`：B-Tree on SQLite。
- **高级（持续）**：
  - 阅读 `fdbserver/simulation/`：deterministic simulation 框架；
  - 阅读 `fdbserver/workloads/`：测试 workload；
  - 阅读 `fdbserver/RecoveryStore.actor.cpp`：cluster recovery；
  - 阅读 `fdbserver/DataDistributor.actor.cpp`：数据分布；
  - 阅读 `flow/Net2.actor.cpp`：网络层。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| 客户端 API | `fdbclient/include/fdbclient/NativeAPI.actor.h` |
| 服务端入口 | `fdbserver/ServerLoop.actor.cpp` |
| 集群控制 | `fdbserver/ClusterController.actor.cpp` |
| Master | `fdbserver/MasterServer.actor.cpp` |
| TLog | `fdbserver/TLogServer.actor.cpp` |
| Storage Server | `fdbserver/StorageServer.actor.cpp` |
| Resolver | `fdbserver/Resolver.actor.cpp` |
| DataDistributor | `fdbserver/DataDistributor.actor.cpp` |
| Backup Agent | `fdbserver/BackupAgent.actor.cpp` |
| Watch | `fdbclient/include/fdbclient/Range.h` |
| Directory | `fdbclient/include/fdbclient/Directory.h` |
| CRDT | `fdbclient/include/fdbclient/Map.h` |
| Tuple | `fdbclient/include/fdbclient/Tuple.h` |
| Flow Actor | `flow/Actor.h` |
| Network | `flow/Net2.actor.cpp` |
| Simulation | `fdbserver/simulation/` |
| Record Layer | `fdb-record-layer/` |

## 二十、源码规模与开发语言

- **LOC 数量**：约 40 万行 C++（核心）+ 一些 Java（bindings + Record Layer）+ Python bindings；
- **主要语言**：C++（核心）、Java（bindings / Record Layer）、Python（bindings）、Ruby（bindings）、Go（community bindings）；
- **构建系统**：CMake（`CMakeLists.txt`）+ Actor 编译（自定义 `actorcompiler.py`）；
- **测试**：deterministic simulation + 单元测试 + Integration test（`tests/`）；
- **CI**：GitHub Actions + Apple 内部 CI；
- **特殊工具**：`actorcompiler.py` 把 `.actor.h` 转换为 `.h`（Actor 编译期生成）；

## 二十一、学习资源

- **官方文档**：[https://apple.github.io/foundationdb/](https://apple.github.io/foundationdb/)；
- **官方教程**：[https://apple.github.io/foundationdb/getting-started.html](https://apple.github.io/foundationdb/getting-started.html)；
- **书籍**：《FoundationDB 实战》（社区）；
- **Apple 工程博客**：FDB 相关文章（Apple Open Source）；
- **论文参考**：
  - **FoundationDB: A Distributed Unbundled Transactional Database**（SIGMOD 2021 paper）；
  - **Actor 模型论文**：Hewitt 1973；
  - **Spanner / Calvin / FaunaDB 论文**：分布式事务理论；
- **课程**：CMU 15-721、Stanford CS346、UC Berkeley；
- **YouTube**：Apple WWDC FoundationDB 演讲、RISE Lab 演讲；
- **Reddit / HN**：r/foundationdb、news.ycombinator.com。

## 二十二、可移植性与部署

- **支持平台**：Linux（x86_64 / aarch64）、macOS；
- **依赖**：GCC 7+ / Clang 6+、OpenSSL、Python 3；
- **容器化**：官方 Docker（`foundationdb/foundationdb:latest`）；
- **Kubernetes Operator**：社区（`foundationdb/fdb-kubernetes-operator`）；
- **部署模式**：单进程多角色 / 多进程多角色；
- **嵌入式部署**：不支持（必须是 server + client 模式）；
- **配置管理**：通过 `fdbcli configure` 动态改 cluster config。

## 二十三、风险与陷阱

- **5 秒事务超时**：长事务会失败，需业务拆分；
- **Process Failure 频繁**：网络抖动 + failover 频繁发生，需客户端 retry 友好；
- **冲突率高时吞吐下降**：strict serializable 代价；ratekeeper 控制并发事务数；
- **单库总容量限制**：单 FDB cluster 不超 1PB（社区经验值）；超大规模需分区（不同 database）；
- **TLog 满时阻塞**：TLog spill + rebalance 是后台异步，但极端情况会阻塞 commit；
- **Backup 影响性能**：TLog 备份开 IO；
- **deterministic simulation 复杂性**：测试基础设施复杂；
- **GPLv3 影响**：商业闭源产品需商业许可；
- **生态较小**：相比 Redis / RocksDB 客户端绑定略少；
- **初次部署学习曲线陡**：Master / SS / TLog / Resolver / CC 角色多；
- **磁盘 IO**：FDB 是 IO bound（每个 commit 至少 1 TLog fsync）。

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/db/` 中的 storage 引擎（如实现分布式 KV）可参考 FDB 的 B-Tree + MVCC 设计；
  - `engineering/src/db/wal.c` 中的 WAL 可参考 TLog 设计；
  - `engineering/src/db/` 的事务模型可参考 strict serializable 的实现思路；
  - **Record Layer 思路**值得借鉴：在 KV 上构建 SQL/文档/图层；
- **学习复用价值**：
  - **Deterministic Simulation** 是分布式系统测试的工业级范式；
  - **Actor 模型**作为异步并发框架非常值得学习；
  - **FDB 严格的分层 + 角色分工**是分布式系统设计的范本；
  - **shard + TLog + Resolver** 三件套是强一致 KV 的标准架构。

## 二十六、MVCC 版本区间与读写冲突检测

FoundationDB 的事务模型在并发控制层面与 §十「事务与一致性」所述「strict serializable」承诺互补，但**真正落地的算法**是**版本区间（version interval）+ 读写冲突范围（read/write conflict range）**机制。这套机制由 commit proxy（`committer`）批量执行，与 Resolver 协作完成。

### 26.1 版本区间语义

每个事务在 `begin` 时从 Proxy 获取一个**读版本（read version）** `m_version`，事务内所有 `get` 操作读 `m_version` 时刻的快照；提交时分配一个**提交版本（commit version）** `c_version > m_version`。事务对外语义是：在 `[m_version, c_version)` 这个**版本区间**内执行了一次原子写入。

```
read_version  ← commit_proxy.getReadVersion()
loop:
    get(key)            → 读 [read_version, +∞) 内的最近可见版本
    set(key, value)     → 暂存本地 mutation，未下发
commit_version ← commit_proxy.commit(mutations, read_conflict_ranges, write_conflict_ranges)
```

关键不变量：**strict serializability** ⇒ 任意事务的 `c_version` 顺序必须与实时顺序一致（external consistency）。

### 26.2 冲突范围（Conflict Range）

事务提交时向 commit proxy 上报两组**冲突范围**（即「哪些 key 区间影响事务结果」）：

- **read conflict range**：事务内所有 `get` 的 key 区间；
- **write conflict range**：事务内所有 `set` 的 key 区间（含 `clear`）。

伪代码：

```
commit(read_version, mutations, reads, writes):
    // 1. Resolver 阶段：read conflict 检测
    for r in reads:
        if r 区间在 [read_version, now) 之间被其他事务写过:
            return CONFLICT  // 触发 client 透明重试
    // 2. 分配 commit_version
    commit_version ← resolver.commit(read_version + 1)
    // 3. 写 mutation log 到 TLog
    tlog.append(commit_version, mutations)
    // 4. 通知 SS 应用 mutations
    ss.apply(mutations, commit_version)
    return commit_version
```

冲突检测由 **Resolver**（`fdbserver/Resolver.actor.cpp`，approx，模块未 init 行号需校对）维护的**写冲突窗口（write conflict window）**索引实现：在 `read_version` 之后任何对 `writes` 区间的写入都会导致该事务返回 `transaction_conflict`，client 自动透明重试。

### 26.3 复杂度与权衡

- **冲突检测复杂度**：单事务 commit 在 Resolver 端是 O(K log M)，K = conflict range 个数，M = 该 range 上最近 K 版本数；纯只读事务 O(1) 不走 Resolver；
- **写放大**：每次 commit 至少 1 个 TLog fsync + N 个 SS 应用，串行化代价是 FDB 的核心权衡；
- **retry 风险**：高冲突率场景下客户端会经历指数退避，ratekeeper（§7.7）动态限制 commit rate 抑制冲突风暴。

> 源码：`fdbserver/CommitProxyServer.actor.cpp`（approx，模块未 init，行号需校对）`committer` actor；`fdbclient/include/fdbclient/FDBTypes.h`（approx）`Version` 类型定义；冲突范围管理 `fdbserver/Resolver.actor.cpp`（approx）。

## 二十七、确定性模拟与不变量检查

§6.4 已概述 Deterministic Simulation，本节展开其**算法实现**：如何让整个集群在内存中按种子完全可重现地运行，并通过**不变量检查（invariant checking）**捕获分布式系统 bug。

### 27.1 抽象层

模拟的关键是把所有非确定性来源**注入到抽象层**而非业务逻辑：

| 非确定性来源 | 抽象类 | 路径（canonical + approx） |
|--------------|--------|---------------------------|
| 时钟 | `SimulatedClock` | `fdbclient/SimulatedClock.actor.cpp`（approx） |
| 网络 | `SimulatedNetwork` | `fdbclient/SimulationNetwork.actor.cpp`（approx） |
| 进程 | `SimulatedProcess` | `fdbclient/include/fdbclient/SimulatedProcess.h`（approx） |
| Actor 调度 | `Sim2_scheduler` | `flow/Sim2.actor.cpp`（approx） |

业务代码（`MasterServer`、`StorageServer`）**完全不知道自己在 simulation 模式**——它们调用 `now()`、`send()`、`recv()` 时实际命中的是上述抽象类，由抽象类按**随机种子 + bugify 选项**返回结果。

### 27.2 模拟运行伪代码

```
run_simulation(seed, workload, duration):
    rng ← new Random(seed)
    installAbstractClock(rng)        // 时钟推进 = 虚拟时间
    installAbstractNetwork(rng)      // 丢包/延迟/分区由 rng 决定
    installAbstractProcesses(rng)    // 进程崩溃/重启由 rng 决定

    cluster ← bootCC(clusterFile)    // 与生产代码同一份
    workload.run(cluster)            // 跑 CPython 测试脚本

    while virtual_time < duration:
        scheduler.tick()              // 只调度 ready 的 actor
        if invariant_violated():
            fail(seed, trace)         // 输出最小重现

    report(seed, metrics, invariants_passed)
```

### 27.3 Invariant 检查机制

FDB 在模拟中跑两类**不变量**：

1. **结构不变量**：每次 commit 后 B+Tree 仍然是一棵合法树（页指针合法、版本链完整）；
2. **语义不变量**：对任意 key 的 `get(k) @ version=v` 必须等于当时写入的值；这条由**read-your-writes** + **因果版本检查**保证。

实现路径：`fdbserver/Invariant.h`（approx，模块未 init，行号需校对）声明 invariant 框架；`checkWitness` / `CHECK` 宏在关键路径埋点。违反时输出 trace event + 该 seed 的复现脚本。

### 27.4 工程意义

- **同一份代码**：production 与 simulation 共享 fdbserver 二进制，仅替换抽象实现；
- **CI 必备**：每次 PR 跑数千个 seed，发现回归立刻定位；
- **复杂度**：单次模拟可压缩到秒级，但代价是必须用 Actor 模型（共享内存通信的非确定性不可控）。

> 源码：`fdbclient/Simulation.actor.cpp`（approx）`Simulation::run`；测试脚本 `tests/simulation/`（approx，模块未 init）。

## 二十八、B+Tree 存储与版本链

FDB 与 LSM 系（LevelDB / RocksDB / Pebble）的关键差异：**存储引擎是 B+Tree + 版本链**（不是 SSTable + compaction）。本节展开 B+Tree 上的 MVCC 实现细节，补充 §七「存储引擎」中未展开的算法层面。

### 28.1 版本链（Version Chain）

每个 key 在 B+Tree 叶子节点维护一个**版本链**（`LinkedPageMap` 或 `BTree` 上的 versioned leaf）：

```
leaf[key=K] → [(ver=v1, val=A), (ver=v2, val=B), (ver=v3, val=C), ...]
              └── oldest ─────────── newest ──┘
```

读 `get(K) @ version=v` ⇒ 在 leaf 上二分查找**最大的 ≤ v 的版本** ⇒ 时间复杂度 O(log V)，V = 该 key 历史版本数。

写 `set(K, val)` ⇒ 在当前 head 追加 `(commit_version, val)`，**不覆盖**历史。

### 28.2 节点分裂 / 合并伪代码

```
split(node):
    if node.size < MAX_KEYS:
        return
    mid ← node.size / 2
    left  ← node.copy(keys[0..mid], children[0..mid+1])
    right ← node.copy(keys[mid..], children[mid..])
    parent.insert_split_key(mid_key, left, right)
    if parent.overflows: split(parent)   // 向上递归

merge(node):
    if node.size >= MIN_KEYS:
        return
    sibling ← node.find_sibling()
    if sibling.size + node.size <= MAX_KEYS:
        parent.merge_into_one(node, sibling)
        parent.delete_split_key()
```

### 28.3 SQLite vs Native Engine

- **v6.x 默认**：`SqliteDB`（`fdbserver/storage/SqliteDB.cpp`，approx）—— 每 shard 一个 SQLite db，B+Tree + MVCC 版本链由 `keyval` + `versionstamp` 实现；
- **v7+ 默认**：`Native`（`fdbserver/include/storage/Native`，approx）—— 自研 C++ B+Tree，多线程读写 + sharded page cache + 自定义 WAL；
- **淘汰机制**：旧版本随 `tlog_pop` 阈值（`versionBytes`）由后台 compaction 清理；保留窗口由 `read_consistency` 配置决定。

### 28.4 复杂度

- **点查**：`get(K) @ version=v` 时间复杂度 O(log N + log V)，N = 总 key 数，V = 单 key 历史版本数；
- **commit 写**：O(log N) 找到 leaf + O(1) 追加版本链 head；
- **范围扫描**：`get_range` 在指定 `version=v` 下沿 leaf 顺序读，每节点先二分定位 ≤ v 的版本再向前。

> 源码：`fdbserver/BTree.cpp`（approx，模块未 init，行号需校对）`BTree::insert` / `BTree::find`；`fdbserver/include/storage/SqliteDB.h`（approx）`SqliteDB::commit`。

## 二十九、参考资料

- [FoundationDB 官方文档](https://apple.github.io/foundationdb/)
- [FoundationDB GitHub](https://github.com/apple/foundationdb)
- [FoundationDB SIGMOD 2021 论文](https://sigmod.org/wp-content/uploads/2021/04/p175-zhou.pdf)
- [FoundationDB 入门教程](https://apple.github.io/foundationdb/getting-started.html)
- [Record Layer 文档](https://github.com/FoundationDB/fdb-record-layer)
- [FoundationDB 论文与演讲集锦](https://apple.github.io/foundationdb/papers.html)
- [Deterministic Simulation 测试方法](https://apple.github.io/foundationdb/testing.html)
- [Flow Actor 框架文档](https://apple.github.io/foundationdb/flow.html)
- [FoundationDB Community Forum](https://forums.foundationdb.org/)
- [FoundationDB 实战教程（社区）](https://github.com/FoundationDB/fdb-screenshots-etc)