# Redpanda Wiki（C++ 重写的高性能 Kafka 兼容流平台）

> 参考版本：Redpanda 24.x+（C++20，单二进制 RAFT-native 架构，BSL 1.1 许可）。本 wiki 重点对比 Kafka 介绍 C++ 重新设计的存储引擎、线程模型、IO 路径与 Tiered Storage。

## 一、项目定位

Redpanda 是由 **Vectorized**（创始人 Alexander Galitski 与 Maya Ber Libchuk，2019 年）开发的 **C++ 重写版 Kafka 兼容事件流平台**，定位为「drop-in Kafka replacement with 10x lower latency & simpler operations」，以 **单二进制 RAFT-native（无 ZooKeeper）**、**线程核心亲和（thread-per-core）**、**io_uring 异步 I/O** 为差异化特性，提供 100% Kafka 协议兼容（含 Kafka 3.x 客户端），被称为「**the Kafka you always wanted**」。

- **痛点**：
  - Kafka JVM GC 停顿导致延迟不稳定（p99 偶发几十~几百 ms）；
  - Kafka ZooKeeper 依赖复杂运维（KRaft 3.0+ 才移除）；
  - Kafka Page Cache 命中率依赖 OS 文件系统调优；
  - 运维复杂：JVM 调优 + ZK 集群 + 多组件（Broker、Schema Registry、ksqlDB）；
  - Tiered Storage 起步晚（3.6 GA），IO 路径优化空间大；
- **典型用户**：Datadog、Confluent 兼容迁移、Akamai、Tencent Cloud、Cisco、Binance 衍生品、LiveRamp、DataOps；
- **核心口号**：**"10x faster. 1/10 the complexity."** + **"Simple, fast, Kafka-compatible streaming"** + **"Built for the AI era"**。

## 二、发展历史与社区

- **起源**：2019 年 Vectorized 公司成立（前身是 C++ 量化交易团队），目标是「Kafka 的兼容替代品，更简单更快」；
- **里程碑版本**：
  - 2019：Redpanda 1.0，基础 RAFT + Kafka 协议；
  - 2020：WASM 脚本引擎（rpk WASM），但后续移除；
  - 2021：Tiered Storage（早期 S3 后端）；
  - 2022：v22.x，Topic Deletion、ACLs、Iceberg 集成（RPK Enterprise）；
  - 2023：v23.x，Iceberg GA、Data Transforms（SQL 转换）、Cloud Topics（实验）；
  - 2024：v24.x，Cloud Topics GA（自管理对象存储层）、Datalake（Iceberg 自动 Sink）、Cluster Linking（跨集群异步复制）；
  - 2025：v25.x（进行中），更多 WASM 替代方案（Materialized Views）、Enterprise Security；
- **维护状态**：活跃，每 2-4 周发布 minor；
- **社区**：GitHub 9k+ star、Slack 社区、Redpanda Forum（社区会议）、rpk CLI 工具成熟；
- **生态**：与 Kafka Confluent 协议兼容（含 Schema Registry、MirrorMaker 部分兼容）、支持 Iceberg / Delta Lake 表格式、Redpanda Console（基于 Kowl）。

## 三、许可与商业限制

- **核心代码**：**BSL 1.1（Business Source License）**——可自由使用（含商业用途），但**不允许将 Redpanda 作为托管服务再销售**（避免与 Redpanda Cloud 竞争）；
- **生效期**：每个版本发布后 4 年自动转为 Apache 2.0；
- **生态关系**：
  - **Redpanda Cloud**：官方托管（AWS / GCP / Azure 三云）；
  - **Redpanda Enterprise**（商业版）：含 Iceberg 自动 Sink、Datalake、Cluster Linking、Data Transforms、SAML/LDAP；
  - **Confluent Cloud 兼容性**：客户端直接对接，无需修改代码；
  - **AWS MSK**、**Confluent Cloud** 等其他 Kafka 兼容服务互斥（定位不同）。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Datadog**：内部日志管道（百万级事件/秒）；
  - **Akamai**：CDN 边缘事件流；
  - **LiveRamp**：广告投放实时归因；
  - **Hudson River Trading**：高频交易信号分发（量化）；
  - **Binance / OKX 衍生品**：订单流处理；
  - **Cisco**：IoT 事件聚合；
  - **Tencent Cloud**：游戏日志流；
  - **Confluent 兼容性**：已通过 Kafka Compatibility Test Suite（部分 client 兼容性）；
- **典型场景**：
  - **低延迟交易撮合**：延迟敏感（p99 < 10ms）；
  - **日志聚合**：替代 ELK 中的 Kafka；
  - **流式 ETL**：Iceberg 自动 Sink 到数据湖；
  - **AI 实时数据**：RAG 流式更新、特征存储 CDC；
  - **游戏服务器**：实时排行榜、跨服事件。

## 五、一句话对比定位

> Redpanda 是 **"C++20 + thread-per-core + io_uring + 简单部署（单二进制）+ RAFT-native（无 ZK）+ Kafka 兼容 + BSL 许可 + Tiered Storage + Iceberg 自动 Sink"** 的现代化重写版 Kafka；与 **Kafka（生态最广）**、**Pulsar（计算存储分离）**、**NATS（轻量云原生）** 构成流生态。Redpanda 的差异化是 **"性能优先 + 运维简化 + BSL 防竞品 + 数据湖原生（Iceberg）"**。

## 六、整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                          Kafka Client（任意兼容）                       │
│   - KafkaProducer/Consumer（confluent-kafka-python / librdkafka）       │
│   - kafka-clients.jar / kafkacat / rpk                               │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ Kafka Wire Protocol (TCP, 100% 兼容)
┌─────────────────────▼────────────────────────────────────────────────┐
│                       Redpanda 单二进制进程                            │
│  src/v/redpanda/application.cc                                       │
│  ├─ main.cc（启动入口）                                                │
│  ├─ application.cc（生命周期 / config）                                 │
│  └─ admin/（HTTP 管理接口）                                           │
├──────────────────────────────────────────────────────────────────────┤
│                    Kafka 协议兼容层                                     │
│  src/v/kafka/server/                                                 │
│  ├─ app.cc（broker 主循环）                                            │
│  ├─ fetch_session_cache.cc / fetch_read_coalescer.cc                 │
│  ├─ group.cc（Consumer Group 实现）                                    │
│  └─ protocol/（Kafka 请求/响应 schema）                                │
├──────────────────────────────────────────────────────────────────────┤
│                    集群协调（Cluster / RAFT）                          │
│  src/v/cluster/                                                       │
│  ├─ metadata_cache.cc / partition_balancer.cc                        │
│  ├─ archival/（Tiered Storage 控制器）                                 │
│  └─ cluster_link/（跨集群同步）                                        │
├─────────────────────┬────────────────────────────────────────────────┤
│                    RAFT 共识层（自研）                                  │
│  src/v/raft/                                                          │
│  ├─ consensus.cc（raft 状态机，per-partition）                        │
│  ├─ group_manager.cc / group_configuration.cc                        │
│  └─ recovery_throttle.cc（恢复限流）                                   │
├─────────────────────┬────────────────────────────────────────────────┤
│                    存储引擎（Log + LSM 可选）                           │
│  src/v/storage/                                                       │
│  ├─ disk_log_impl.cc（主存储路径，sequential append + segment）       │
│  ├─ batch_cache.cc / chunk_cache.cc（读缓存）                          │
│  ├─ compaction/（key_offset_map + filter + reducer）                  │
│  └─ lsm/（LSM Tree 二级索引，可选）                                    │
├─────────────────────┬────────────────────────────────────────────────┤
│                    对象存储 / Iceberg 集成                             │
│  src/v/cloud_storage/ + src/v/datalake/ + src/v/iceberg/              │
│  ├─ partition_manifest.cc（清单管理）                                │
│  ├─ offset_index.cc / offset_translation_layer.cc                   │
│  └─ datalake_table_creator.cc（Iceberg Sink）                          │
└──────────────────────────────────────────────────────────────────────┘
```

### 6.1 Thread-per-Core 模型

Redpanda 抛弃了 Kafka 的「IO 线程池 + 共享线程池」模型，采用**每个核心绑定一个 reactor**（thread-per-core）：

- **sharded service**：`ss::sharded<T>`（Seastar 框架）模式：每个 shard 一个 T 实例，跨 shard 用 `ss::future<T>` 通过 `invoke_on` 投递；
- **CPU 亲和性**：`taskset` 或 `numactl` 绑核，避免上下文切换；
- **优势**：
  - 无锁（shard 内单线程访问）；
  - 缓存友好（L1/L2 全部归属该核）；
  - 延迟稳定（无 GC，无线程切换抖动）；
- **代价**：
  - 核心数需要足够（生产 ≥ 16 核）；
  - 跨 partition 操作复杂（需路由到对应 shard）；

### 6.2 异步 I/O（io_uring / Seastar）

- **底层**：基于 [Seastar](https://github.com/scylladb/seastar) 框架（ScyllaDB 同款） + Linux io_uring；
- **优势**：单次系统调用提交多个 I/O 请求（io_uring SQ/CQ 模式），降低 syscall 开销；
- **Linux 版本**：≥ 5.10（io_uring 必需），生产用 6.x LTS；
- **平台限制**：**仅 Linux**（无 macOS / Windows 生产支持），macOS 仅开发测试；

## 七、存储引擎（Log 结构）

Redpanda 的存储与 Kafka 类似为 **partitioned log + segment**，但底层 IO 与缓存机制不同。

### 7.1 物理文件结构

```
data_dir/
├── kafka/
│   └── <internal-topic>/
│       └── <partition-id>/
│           ├── 0_0.log           # segment 文件（追加写）
│           ├── 0_0.index         # offset → file position
│           ├── 0_0.txn           # 控制批次（transaction / abort）
│           └── ...
└── controller/
    └── ...
```

- **与 Kafka 格式兼容**：`rpk topic produce` 写入的 segment 可被 Kafka 工具读取；
- **每段大小**：`log_segment_size`（默认 128 MB，相比 Kafka 1 GB 略小）；
- **读路径**：
  - **batch_cache**（`batch_cache.cc`）：缓存最近读取的 batch（内存热路径）；
  - **chunk_cache**（`chunk_cache.cc`）：缓存解压后的 batch 数据；
  - **disk**：直接顺序读（mmap 或 pread）；
- **写路径**：
  - Producer batch → in-memory 缓冲 → 后台 flush 到 disk（`log_flush_interval_ms`）；
  - fdatasync 保证 durability；

### 7.2 LSM Tree 二级索引（可选）

`src/v/lsm/` 提供 LSM Tree 风格的二级索引（24.x GA）：

- **适用场景**：key-value 查询（如 RocksDB-style `rpk topic consume --key`）；
- **数据流**：Kafka batch → LSM Memtable → SSTable（后台 compaction）；
- **优势**：避免每次查询都扫 segment index；
- **配置**：`enable_lsm_compaction=true`（实验性 → GA 演进中）；

### 7.3 Compaction（Key Offset Map）

`src/v/compaction/`：

- **key_offset_map.cc**：记录每个 key 的最新 offset；
- **filter.cc**：布隆过滤器加速 key 查询；
- **reducer.cc**：合并 segment 时按 key 保留最新；
- **触发**：`compaction_interval_ms` 或 segment 数量阈值；

## 八、索引机制（流类型专项展开）

### 8.1 Log 结构

- 与 Kafka 高度类似：append-only segment + offset index + tx index；
- **关键差异**：
  - **无 sparse index 的「larger page cache」开销**：每个 segment 整体 cache（chunk_cache）+ 内存 batch cache；
  - **读取按 batch**：consumer fetch 一次拉一个 batch（约 1-16 MB），batch cache 命中率高；

### 8.2 分区模型

- 与 Kafka **完全兼容**：同一 topic 多个 partition，key hash 路由；
- **改进**：
  - **Controller Group**：所有 partition leader 调度由 controller 集中处理（cluster/metadata_cache.cc）；
  - **动态均衡**：partition_balancer.cc 自动检测 broker 负载；
  - **Tiered Storage 友好**：partition 可独立配置 remote read priority；

### 8.3 副本与一致性（RAFT-Native）

```
        Producer (acks=all)
            │
            ▼
   ┌─── Leader (Partition 0) ───┐  ◀─── src/v/raft/consensus.cc
        │ writes to local log
        ▼
   ┌─ Follower 1 ─┐  ┌─ Follower 2 ─┐
   │  RAFT replica │  │  RAFT replica │
   └───────────────┘  └───────────────┘
```

- **RAFT 自研实现**（`src/v/raft/consensus.cc`）：
  - **每个 partition 一个 RAFT group**（与 Kafka 的 ISR 概念不同）；
  - **无独立 Controller**：controller state 也存在 RAFT log（`__cluster_metadata`）；
  - **Leader Election**：基于 RAFT term，超时触发预选 + 投票；
  - **Log Entry**：每次写入是一条 RAFT log entry（含 producer batch + producer ID + epoch）；
- **与 Kafka ISR 对比**：
  | 维度 | Kafka ISR | Redpanda RAFT |
  |------|-----------|---------------|
  | 共识协议 | 自定义 ISR 集合 | 标准 RAFT |
  | Leader 选举 | Controller 触发 | RAFT 心跳超时 |
  | 故障切换延迟 | 几百 ms~几秒 | < 1s |
  | 数据一致性保证 | HW + Leader Epoch | RAFT log 严格一致 |
- **acks 语义**：
  - `acks=0`：仅写入 leader 内存（不保证）；
  - `acks=1`：写入 leader + 本地 fsync；
  - `acks=all`：等待 RAFT 多数派同步（默认且推荐）；
- **故障恢复**：
  - `recovery_throttle.cc`（`src/v/raft/`）：恢复限流，避免落后 replica 拖累集群；
  - **Snapshot + Catchup**：落后 replica 通过 RAFT snapshot 快速追上；

### 8.4 偏移量管理

- **__consumer_offsets**：内部 Kafka topic（同 Kafka）；
- **Group Coordinator**：
  - 实现位于 `src/v/kafka/server/group.cc`；
  - 完整支持 Kafka Consumer Group 协议（join/sync/heartbeat/commit）；
  - **KIP-848**（新一代协议）：Redpanda 24.x 支持部分客户端实验；
- **Commit 语义**：与 Kafka 完全兼容（at-most/at-least/exactly-once）；
- **EOS 实现**：基于 RAFT 的幂等 producer（`enable_idempotence=true`）+ 事务（实验）；

### 8.5 存储分层（Tiered Storage + Iceberg Sink）

`src/v/cloud_storage/` 是核心：

- **partition_manifest.cc**：记录每个 partition 的 segment 清单（含本地副本、远端副本）；
- **offset_index.cc / offset_translation_layer.cc**：
  - 解决 Kafka offset 与 Redpanda offset 在 Tiered Storage 后**不连续**的问题；
  - 提供 `offset_translator`：远端 offset → 真实 broker offset；
- **async_manifest_view.cc**：异步加载 manifest 缓存；
- **S3 后端**：默认 `cloud_storage_backend=s3`（也支持 Azure Blob / GCS / MinIO）；
- **Iceberg Datalake Sink**（`src/v/datalake/` + `src/v/iceberg/`）：
  - 直接把 partition data 转 Iceberg 表格式写入对象存储；
  - 集成 Spark / Trino / Snowflake / Athena 直接查询；
  - **不经过 Kafka 即传输层**：减少 Kafka Topic → 数据湖的中间环节；

### 8.6 Cluster Linking（跨集群同步）

`src/v/cluster/cluster_link/`（v24+）：

- **跨集群异步复制**：在 Redpanda Cloud 与自建集群间同步数据；
- **配置驱动**：通过 `rpk cluster config` 或 REST API 设置；
- **优于 MirrorMaker**：原生集成、支持 Schema 转换、支持双向复制；

## 九、查询/计算/接口层

### 9.1 生产/消费 API

- **100% Kafka 协议兼容**：confluent-kafka-go / confluent-kafka-python / kafka-clients.jar 直接对接；
- **rpk CLI**（`src/go/rpk/`）：
  - `rpk topic produce/consume`：命令行工具；
  - `rpk cluster info/health`：集群状态；
  - `rpk topic list/config`：topic 管理；
  - `rpk wasm`（早期，已弃用）：WASM 脚本引擎；

### 9.2 消息语义

- **At-most-once / At-least-once**：与 Kafka 相同；
- **Exactly-Once**：
  - **幂等 Producer**：基于 RAFT producer ID + sequence，去重；
  - **事务**：`transactional.id` + `initTransactions()` + `commitTransaction()`（部分兼容 Kafka 客户端）；
  - **读已提交**：`read_committed` isolation level 生效；
- **限制**：事务性能比非事务略低（RAFT 控制批次开销），但比 Kafka 事务快（无 JVM GC 抖动）；

### 9.3 流处理引擎

- **Data Transforms**（v23+）：基于 WebAssembly 的流转换（早期 rpk WASM 演化）；
- **Iceberg Sink**：替代部分 Kafka Connect 用途；
- **WASM Functions**：用户自定义函数处理每条消息（编译为 Wasm module）；
- **外部集成**：Flink / Spark / Materialize 同样兼容 Kafka 协议消费 Redpanda；
- **对比**：
  | 引擎 | 部署 | 优势 |
  |------|------|------|
  | Redpanda Wasm | 嵌入 broker | 低延迟无外部依赖 |
  | Apache Flink | 独立集群 | 大规模复杂计算 |
  | Materialize | 独立 | SQL 持续查询 |
  | Spark Streaming | 独立 | 微批处理 |

### 9.4 Schema 演进

- **Confluent Schema Registry 兼容**：通过 HTTP API 对接，无需修改；
- **格式**：Avro / Protobuf / JSON Schema；
- **演进策略**：BACKWARD / FORWARD / FULL；
- **Datalake Iceberg Schema**：自动从 Kafka topic schema 生成 Iceberg 表 schema；

## 十、事务与一致性

- **RAFT 强一致性**：每次写入必须 RAFT 多数派确认后才返回成功；
- **Producer ID 机制**：每个 producer session 分配一个 RAFT-tracked ID；
- **Sequence Number**：每条消息单调递增（partition 级别）；
- **幂等保证**：broker 端去重（同 producer ID + sequence）；
- **事务**：
  - **RecordBatch 标记**：事务 commit/abort marker 写入 RAFT log；
  - **Consumer 过滤**：事务内的 batch 标记未提交，consumer `isolation.level=read_committed` 过滤；
- **KV 隔离**：与 Kafka 事务类似，跨多 topic-partition 原子写 + offset commit；

## 十一、复制与高可用

- **RAFT 多数派**：默认 3 副本（容忍 1 故障），5 副本容忍 2 故障；
- **Controller 内嵌**：与 Kafka KRaft 类似，controller state 也走 RAFT；
- **故障切换**：RAFT 选举通常 < 1s 完成；
- **跨集群灾难恢复**：
  - **Cluster Linking**：异步复制到 DR 站点；
  - **S3 Tiered Storage**：天然异地热备（数据持久化在 S3）；
- **滚动升级**：`rpk cluster upgrade` 滚动重启 broker；
- **零停机升级**：配合 Tiered Storage + 滚动重启；

## 十二、扩展性

- **水平扩展**：增加 broker → partition 自动 rebalance（`partition_balancer.cc`）；
- **分区扩展**：动态增加 partition（同 Kafka 限制：减少不支持）；
- **Tiered Storage**：解除磁盘配额限制，分区数可大幅扩展；
- **限制**：
  - 单 broker CPU 核心数需 ≥ 8（thread-per-core 模型）；
  - 单 partition 仍受 broker 网络限制（建议 < 100 MB/s）；
  - 集群规模通常 < 100 broker（RAFT controller 性能）；

## 十三、性能与基准

- **官方基准**（Vectorized 团队）：
  - **p99 延迟**：< 5ms（vs Kafka ~20ms）；
  - **单 broker 吞吐**：~1 GB/s（p99 延迟可控）；
  - **10x 更快**：相比 Kafka 同等硬件；
- **第三方基准**（Confluent 2023 内部测试）：
  - Redpanda 在相同硬件下 p99 延迟 5-10x 优于 Kafka；
  - 吞吐基本持平（受网络限制）；
- **OpenMessaging Benchmark**：Vectorized 提供官方适配；
- **性能优势来源**：
  - 无 JVM GC 停顿；
  - thread-per-core 无锁；
  - io_uring 减少 syscall；
  - Seastar 零拷贝网络栈；
  - 智能批处理（`fetch_read_coalescer.cc`）；

## 十四、运维与可观测性

- **指标导出**：Prometheus 端口 9644（默认）；
- **关键指标**：
  - `vectorized_storage_log_log_flush_latency`（刷盘延迟）；
  - `vectorized_cluster_partition_under_replicated`（欠复制 partition）；
  - `vectorized_raft_leader_election_count`（leader 选举次数）；
  - `vectorized_kafka_consumer_group_lag`（消费延迟）；
- **日志**：Seastar logger（结构化 JSON 日志）；
- **管理工具**：
  - `rpk` CLI（Go 实现，跨平台）；
  - **Redpanda Console**（基于 Kowl，Web UI）；
  - **Admin API**（HTTP，端口 9644 `/admin/`）；
- **健康检查**：`rpk cluster health` / `rpk cluster info`；
- **备份恢复**：
  - 冷备（停 broker 后复制 `data_dir`）；
  - Tiered Storage 自动异地；
  - Cluster Linking 跨集群异步复制；
- **滚动升级**：`rpk cluster upgrade --filename <rpm>` 自动化；

## 十五、生态与集成

- **客户端 SDK**：**所有 Kafka 客户端均可**（Java、Python、Go、C/C++、.NET、Node.js）；
- **CDC 集成**：Debezium（Kafka Connect 形式）/ Airbyte（直接对接）；
- **Schema 管理**：Confluent Schema Registry 直接对接（HTTP）；
- **流处理**：Apache Flink / Materialize / Kafka Streams；
- **数据湖**：Apache Iceberg（自动 Sink）/ Delta Lake / Apache Hudi；
- **消息队列协议**：与 Kafka 协议兼容，**任何 Kafka producer / consumer 可对接**；
- **Kubernetes**：Redpanda Operator（官方 Helm Chart + Operator）、Strimzi 部分兼容；
- **多云**：Redpanda Cloud 支持 AWS / GCP / Azure 三个云；

## 十六、核心论文与算法原理

- **RAFT 共识**：Diego Ongaro, "Consensus: Bridging Theory and Practice"（PhD Thesis 2014）；
- **io_uring 论文**：Jens Axboe, "Efficient IO with io_uring"（Linux Kernel 文档）；
- **Seastar 框架**：ScyllaDB 团队的 reactor 模型；
- **LSM Tree**：O'Neil et al., "The Log-Structured Merge-Tree (LSM-Tree)"（Acta Informatica 1996）；
- **Iceberg 论文**：Ryan Blue, "Iceberg: A Fast Table Format for the Cloud"（SIGMOD 2024 Industry）；
- **Tiered Storage**：KIP-405（参考 Apache Kafka 同期设计）；
- **Redpanda 官方博客**：[vectorized.io/blog](https://vectorized.io/blog/) 大量深度文章；

## 十七、源码学习路径

### 17.1 必读模块清单（按入门到进阶）

| 阶段 | 模块 | 路径 | 说明 |
|------|------|------|------|
| 入门 | 启动入口 | `src/v/redpanda/application.cc`、`main.cc` | 单进程启动、Seastar 启动 |
| 入门 | Kafka 协议层 | `src/v/kafka/server/app.cc`、`protocol/` | 协议解析、请求路由 |
| 入门 | 存储 | `src/v/storage/disk_log_impl.cc` | segment、append、read |
| 进阶 | RAFT | `src/v/raft/consensus.cc` | raft 状态机、log replication |
| 进阶 | 集群协调 | `src/v/cluster/metadata_cache.cc`、`partition_balancer.cc` | partition 调度 |
| 进阶 | Tiered Storage | `src/v/cloud_storage/partition_manifest.cc`、`offset_translation_layer.cc` | 对象存储抽象 |
| 高级 | Iceberg Sink | `src/v/datalake/datalake_table_creator.cc` | Kafka → Iceberg 流式 |
| 高级 | Cluster Linking | `src/v/cluster/cluster_link/` | 跨集群复制 |

### 17.2 推荐阅读顺序

1. **第 1 周**：Redpanda 官方文档 + `application.cc` 启动流程；
2. **第 2 周**：`disk_log_impl.cc`（Kafka → Redpanda 存储差异）；
3. **第 3 周**：`raft/consensus.cc`（与 Kafka ISR 对比）；
4. **第 4 周**：Tiered Storage + Offset Translation；
5. **第 5 周**：Iceberg Datalake（数据湖集成）；
6. **第 6 周**：Cluster Linking 与多集群同步。

## 十八、源码规模与开发语言

- **规模**：Redpanda 24.x 约 **40 万行** C++（核心）+ 部分 Go（rpk CLI、Operator）+ Rust（少量）；
- **语言占比**：C++ ~80%、Go ~15%、Python / Rust / 其他 ~5%；
- **构建系统**：Bazel（`MODULE.bazel`、`bazel/`）；
- **编译器**：GCC 12+ 或 Clang 15+（C++20）；
- **平台**：**仅 Linux**（生产）、macOS（开发测试，io_uring 不可用）；
- **测试**：GTest + Seastar testing + Ducktape（集成测试框架）；
- **CI**：GitHub Actions + 内部 CI；

## 十九、学习资源

- **官方文档**：[Redpanda Documentation](https://docs.redpanda.com/)；
- **官方博客**：[Vectorized Blog](https://vectorized.io/blog/)（大量 RAFT / Seastar / 性能优化深度文章）；
- **视频**：Redpanda Forum 会议（YouTube）；
- **Reddit**：[r/redpanda](https://reddit.com/r/redpanda)；
- **Slack**：Redpanda Community Slack；
- **GitHub**：[redpanda-data/redpanda](https://github.com/redpanda-data/redpanda)；
- **中文资源**：相对较少，CSDN/掘金部分性能对比文章；

## 二十、可移植性与部署

- **平台**：**仅 Linux**（生产推荐 Ubuntu 22.04 LTS、RHEL 9）；
- **容器化**：官方 Docker 镜像 `redpandadata/redpanda`；
- **Kubernetes**：[Redpanda Operator](https://docs.redpanda.com/docs/operator/)（官方）+ Helm Chart；
- **混合云**：本地 + Redpanda Cloud 通过 Cluster Linking；
- **裸机部署**：`rpk cluster bootstrap` 一键初始化集群；
- **硬件建议**：
  - CPU：16+ 核心（thread-per-core）；
  - 内存：16-32 GB；
  - 磁盘：NVMe SSD（吞吐 + IOPS）；
  - 网络：≥ 10 GbE（高吞吐场景）；
- **监控**：Prometheus + Grafana 官方仪表盘；

## 二十一、变体与衍生项目

- **Redpanda Cloud**：AWS / GCP / Azure 三云托管；
- **Redpanda Enterprise**：含 Iceberg Sink、Cluster Linking、Data Transforms、SAML/LDAP；
- **Redpanda Edge**：轻量级，单节点部署（IoT、边缘场景）；
- **对比项目**：
  - **AutoMQ**：S3 原生 Kafka 替代品（Java + 对象存储）；
  - **Apache Kafka**：原版；
  - **Apache Pulsar**：本 wiki 后续文章；
  - **NATS JetStream**：本 wiki 后续文章；
  - **Warpstream**：又一个 Kafka 替代品（S3 原生）；

## 二十二、风险与陷阱

### 22.1 常见误用

- **macOS / Windows 生产部署**：Redpanda 仅在 Linux 下生产可用；
- **CPU 核心数不够**：thread-per-core 模型在 < 8 核下性能不如 Kafka；
- **BSL 许可误用**：不能将 Redpanda 作为托管服务对外销售；
- **数据迁移未测试**：从 Kafka 迁移需做兼容性测试（KIP-848 等新特性）；
- **WASM 函数 / Data Transforms 期望过高**：早期功能，不适合核心业务；

### 22.2 性能陷阱

- **不正确的 NUMA 绑核**：跨 NUMA 节点访问内存性能下降 30%+；
- **磁盘 IO 调度器**：NVMe 推荐 `none`（multi-gear 不再需要 I/O elevator）；
- **小 batch**：`batch_max_bytes` 过小导致 broker 写入放大的 RPC 次数过多；
- **Tiered Storage 配置错**：S3 凭据 / region 配置错 → segment 上传失败 → 日志不可恢复；
- **Iceberg Datalake Schema 变更**：下游查询侧必须同步更新（避免 schema 不兼容）；
- **网络吞吐饱和**：10 GbE 网络下，建议 broker CPU 足够（否则网络成为瓶颈）；

### 22.3 迁移痛点

- **从 Kafka 迁移**：
  - Topic 配置兼容：但部分新特性（KIP-848、集群链接）需双侧支持；
  - 客户端无需改代码（100% 兼容）；
  - **Connector 兼容**：部分 Kafka Connect connector 需调整（Sink/Source 目标一致）；
- **从 Redpanda 迁移回 Kafka**：
  - 数据可读（格式兼容）；
  - 但 Tiered Storage 部分数据**只有 Redpanda 能读**；
- **版本升级**：
  - 主版本升级（如 23.x → 24.x）需测试 Tiered Storage 兼容性；
  - BSL 4 年后自动转 Apache 2.0，社区版路线需关注；

### 22.4 运维雷区

- **broker 进程崩溃**：thread-per-core 下单个核 panic 可能影响整个 broker；
- **Seastar reactor hang**：需熟悉 Linux perf / bpftrace 排查；
- **Iceberg Sink 失败回滚**：需配置 DLQ（Dead Letter Queue）；
- **Cluster Linking 网络抖动**：跨区域同步延迟上升，注意批量推送窗口；
- **跨集群 schema 升级**：两边 schema 兼容性需同步；
- **裸金属 NUMA 不平衡**：多 socket 服务器需注意 CPU 分布；

## 二十三、与本项目的关联

- **可借鉴的设计**：
  - **thread-per-core 架构**：本项目 C++ 数据库引擎可参考单核绑定模式（避免锁竞争）；
  - **io_uring 异步 I/O**：与本项目 `engineering/src/db/core/disk.c` 同步阻塞 I/O 形成对比（可选升级路径）；
  - **Tiered Storage + Iceberg DAO**：本项目 `engineering/include/db/mm_storage.h` 已有部分多模态存储抽象；
  - **RAFT-native 共识**：本项目 `engineering/include/db/wal.h` 的 WAL 与 RAFT log entry 概念类似；
- **不适合借鉴**：
  - **C++20 模板元编程**：本项目偏 C11/C++17 工程实践；
  - **Seastar 框架依赖**：本项目无此依赖；
  - **BSL 许可**：与本项目 Apache 2.0 兼容优先原则不符；
- **流接入集成**：未来如需对接 Redpanda，可使用 Kafka 客户端 SDK 直接对接（无特殊集成成本）。

## 二十四、参考资料

- [Redpanda 官方主页](https://redpanda.com/)
- [Redpanda Documentation](https://docs.redpanda.com/)
- [Redpanda GitHub](https://github.com/redpanda-data/redpanda)
- [BSL 1.1 License](https://github.com/redpanda-data/redpanda/blob/dev/LICENSE.md)
- [Redpanda Architecture Overview](https://docs.redpanda.com/docs/architecture/architecture-overview/)
- [Tiered Storage Documentation](https://docs.redpanda.com/docs/data-management/tiered-storage/)
- [Iceberg Datalake](https://docs.redpanda.com/docs/data-management/datalake/)
- [Redpanda Forum Videos (YouTube)](https://www.youtube.com/@redpandadata)
- [Cluster Linking](https://docs.redpanda.com/docs/data-management/cluster-linking/)
- [Seastar 框架](https://github.com/scylladb/seastar)
- [RAFT 论文](https://raft.github.io/raft.pdf)
- [io_uring 文档](https://kernel.dk/io_uring.pdf)
- [Apache Kafka 主页](https://kafka.apache.org/)（对比参考）
- [Apache Pulsar 主页](https://pulsar.apache.org/)（对比参考）