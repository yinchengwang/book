# Kafka Wiki（事件流平台的事实标准）

> 参考版本：Apache Kafka 3.7+（Scala 2.13 / Java 17，KRaft 共识模式默认开启，Apache 2.0 许可）。本 wiki 重点覆盖流类型专项章节（Log 结构 / 分区模型 / ISR 一致性 / EOS / Schema 演进 / Tiered Storage）。

## 一、项目定位

Apache Kafka 是由 **LinkedIn** 起源（2010 年 Jay Kreps 等人开源）、2011 年捐赠给 **Apache 基金会**的事件流平台（Event Streaming Platform），定位为「分布式、持久化、高吞吐的发布-订阅消息系统」，以 **append-only log（顺序写盘）** 为存储原语、以 **partition + ISR（In-Sync Replicas）** 为扩展与一致性基石，提供 Producer/Consumer、Connect、Streams、KSQL/ksqlDB 完整生态，是事实标准的事件流中间件。

- **痛点**：
  - 传统消息中间件（ActiveMQ / RabbitMQ）面向请求-响应，单 broker 难以承载 TB 级日志；
  - 日志聚合场景需要**保留重放**（retention + replay），而非消息被消费即删；
  - 流式 ETL 需要**有状态**计算（窗口、聚合、Join），传统消息中间件只能做无状态转发；
  - 数据集成（DB CDC、日志、IoT）需要统一的 Source/Sink 抽象；
  - 微服务 / 事件驱动架构需要**解耦**与**回溯**能力（Event Sourcing / CQRS）；
- **典型用户**：LinkedIn（万亿级消息/天）、Uber、Netflix、Airbnb、PayPal、字节跳动、美团、阿里、平安银行等几乎所有大型互联网与金融企业；
- **核心口号**：**"The world's most popular event streaming platform"** + **"Publish & subscribe | Store & process | Stream | Connect"**。

## 二、发展历史与社区

- **起源**：2010 年 LinkedIn 内部为解决日志聚合与数据集成问题而开发；2011 年开源并进入 Apache 孵化器；2012 年成为 Apache 顶级项目；
- **里程碑版本**：
  - 0.8（2013）：Replication（副本机制）首次引入；
  - 0.9（2015）：Kafka Connect（Source/Sink 框架）发布；
  - 0.10（2016）：Kafka Streams（流处理库）发布；
  - 0.11（2017）：Exactly-Once Semantics（EOS）首次实现（幂等 Producer + 事务）；
  - 1.0（2018）：幂等 Producer 默认开启、Streams 改进；
  - 2.0（2020）：KRaft（基于 Raft 的 Controller 共识）实验性引入；
  - 3.0（2021）：KIP-500 默认 KRaft（替代 ZooKeeper）；
  - 3.3（2022）：Self-Balancing、Consumer Group 重平衡改进；
  - 3.6（2023）：**Tiered Storage（分层存储，对象存储后端）GA**；
  - 3.7（2024）：KIP-848 新一代 Consumer Group 协议（**Consumer Group Protocol: Consumer**）取代旧的 group coordinator；
  - 4.0（2025）：Java 17 默认、移除 ZooKeeper 完全路径、新一代 broker 协议、Cloud Events 默认；
- **维护状态**：极度活跃（Apache 最活跃项目之一），每月发布 minor 版本；
- **社区**：GitHub 30k+ star、Confluent 商业支持、80+ Confluent 认证工程师、每年 Kafka Summit 全球大会；
- **生态**：Confluent Schema Registry、Confluent Connectors Hub、ksqlDB、MirrorMaker（跨集群复制）、CMAK / AKHQ / UI for Apache Kafka 管理面板。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（宽松商用，无 SSPL/BSL 限制）；
- **生态关系**：
  - **Confluent Inc.** 是 Kafka 主要商业化公司，提供 Confluent Cloud（托管）、Confluent Platform（商业发行版，含 Schema Registry、Connect 商业版、ksqlDB、REST Proxy、Auto Data Balancer）、Confluent Enterprise 安全特性（RBAC、SASL、审计）；
  - **AWS MSK**（Managed Streaming for Apache Kafka）提供托管服务；
  - **Azure Event Hubs Kafka-compatible**、**Google Pub/Sub Lite**、**Aliyun Message Queue for Apache Kafka** 兼容 Kafka 协议；
  - **Redpanda**（本 wiki 后续文章）以 C++ 重写 Kafka 协议、号称 10x 性能；
  - **Apache Pulsar**（本 wiki 后续文章）走 BookKeeper + 计算存储分离架构；
  - **NATS**（本 wiki 后续文章）面向云原生的轻量级消息系统。

## 四、业界主要使用方与案例

- **大型用户**（公开案例）：
  - **LinkedIn**：Kafka 起源公司，10+ 万 broker，日均万亿级消息；
  - **Uber**：实时定价、行程追踪、欺诈检测；
  - **Netflix**：用户行为事件流、视频元数据；
  - **Airbnb**：房源变更事件、订单管道；
  - **PayPal**：金融交易实时反欺诈；
  - **Walmart**：库存实时同步；
  - **字节跳动**：广告投放、用户画像、AB 实验；
  - **美团**：订单状态机、骑手实时位置、配送 ETA；
  - **阿里**：双 11 订单洪峰、双链路同步；
  - **平安银行**：交易流水、监管报送；
- **典型场景**：
  - **消息中间件**：服务解耦、削峰填谷；
  - **日志聚合**：应用日志、审计事件；
  - **流式 ETL**：CDC（Change Data Capture，从 MySQL/Postgres 通过 Debezium 同步到 Kafka，再到下游）；
  - **事件溯源（Event Sourcing）**：订单状态、账户余额；
  - **实时分析**：点击流、IoT 传感数据 → Kafka → Flink/Spark；
  - **AI 数据准备**：大规模数据集的实时分发（LLM 训练数据流、向量库 CDC）。

## 五、一句话对比定位

> Kafka 是 **"Log 存储（partition + segment）+ 分区复制（ISR）+ 顺序保证 + 完整流生态（Connect/Streams/Schema Registry/ksqlDB）+ KRaft 共识（无 ZK）+ Tiered Storage（对象存储分层）"** 的事实标准；与 **Redpanda（C++ 重写，10x 延迟优化）**、**Pulsar（BookKeeper 分层，云原生友好）**、**NATS JetStream（轻量，云原生边缘）**、**AWS Kinesis（托管）**、**Google Pub/Sub（托管）** 构成事件流生态。Kafka 的差异化是 **"最早开源 + 生态最广 + 工程最成熟 + 厂商主导（Confluent）"**。

## 六、整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                            Producer 客户端                              │
│  clients/src/main/java/org/apache/kafka/clients/producer/             │
│  ├─ KafkaProducer（批处理、压缩、partition 路由）                       │
│  ├─ Partitioner（DefaultPartitioner / RoundRobinPartitioner）          │
│  ├─ RecordAccumulator（按 topic-partition 聚合 batch）                  │
│  └─ Sender I/O 线程（Selector 复用 NIO）                              │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ TCP (Kafka Wire Protocol)
┌─────────────────────▼────────────────────────────────────────────────┐
│                            Broker 集群                                │
│  core/src/main/scala/kafka/server/                                    │
│  ├─ KafkaServer / KafkaBroker（broker 进程入口）                       │
│  ├─ KafkaApis（请求处理入口：PRODUCE / FETCH / METADATA 等）            │
│  ├─ ReplicaManager（副本管理、ISR、HW、log end offset）                 │
│  ├─ LogManager（segment 管理、retention、compaction）                  │
│  ├─ GroupCoordinator（__consumer_offsets 内部 topic）                   │
│  ├─ TransactionCoordinator（__transaction_state 内部 topic）           │
│  ├─ ControllerServer / KafkaRaftServer（KRaft 共识控制器）             │
│  └─ TieredStorageManager（3.6+，对象存储 HDFS/S3）                     │
└──────┬─────────────────────────────────────────────┬─────────────────┘
       │                                              │
┌──────▼─────────────────┐               ┌─────────────▼───────────────┐
│     Consumer 客户端    │               │    Streams / ksqlDB          │
│  clients/.../consumer/ │               │  streams/src/main/...       │
│  ├─ KafkaConsumer      │               │  KStream / KTable / DSL     │
│  ├─ FetchSessionHandler│               │  Topology / Processor      │
│  └─ ConsumerCoordinator│               │  State Store (RocksDB)     │
└────────────────────────┘               └─────────────────────────────┘
```

### 6.1 进程与线程模型

- **Broker 进程**：单进程多线程模型；一个 broker 既是 partition leader 也是 follower；
  - **IO 线程池**（`RequestHandlerPool`）：网络请求入口；
  - **ReplicaFetcher 线程**：从其他 broker 拉取数据（`core/src/main/scala/kafka/server/ReplicaFetcherThread.scala`）；
  - **Log 刷盘线程**（`Log.Flusher`）：按 `log.flush.interval.ms` 触发 flush；
  - **Compaction 线程**（`LogCleaner`）：后台清理 compaction topic；
- **Controller 节点**：KRaft 模式下由集群自动选出，使用 Raft 协议同步元数据；老版本需要 ZooKeeper 集群；
- **Client 端**：Producer 的 `Sender` 线程负责实际网络 I/O；Consumer 的 `HeartbeatThread`（KIP-848 后改为服务器侧心跳）；

### 6.2 关键模块路径

| 模块 | 路径 |
|------|------|
| Kafka 主入口 | `core/src/main/scala/kafka/Kafka.scala` |
| Broker 生命周期 | `core/src/main/scala/kafka/server/KafkaServer.scala`、`KafkaBroker.scala` |
| 元数据管理 | `core/src/main/scala/kafka/server/metadata/` |
| 副本管理 | `core/src/main/scala/kafka/server/ReplicaManager.scala` |
| Log 存储 | `core/src/main/scala/kafka/log/LogManager.scala`、`Log.scala` |
| Raft 共识 | `core/src/main/scala/kafka/raft/KafkaRaftManager.scala` |
| Group 协调 | `core/src/main/scala/kafka/coordinator/group/GroupCoordinator.scala` |
| 事务协调 | `core/src/main/scala/kafka/coordinator/transaction/TransactionCoordinator.scala` |
| Producer | `clients/src/main/java/org/apache/kafka/clients/producer/KafkaProducer.java` |
| Consumer | `clients/src/main/java/org/apache/kafka/clients/consumer/KafkaConsumer.java` |
| Streams | `streams/src/main/java/org/apache/kafka/streams/KafkaStreams.java` |

## 七、存储引擎（Log 结构）

Kafka 的核心抽象是 **Partitioned Log（分区日志）**。每个 topic 由 N 个 partition 组成，每个 partition 是一个**严格 append-only 的顺序写日志**。

### 7.1 物理文件结构

```
data_dir/
├── __consumer_offsets-0/        # 内部 topic（consumer offset）
│   └── 00000000000000000000.log
├── my-topic-0/                  # 用户 topic 第 0 个 partition
│   ├── 00000000000000000000.log       # segment 文件（追加写）
│   ├── 00000000000000000000.index      # offset → file position 索引
│   ├── 00000000000000000000.timeindex  # timestamp → offset 索引
│   ├── 00000000000000000000.txnindex   # 事务索引（EOS）
│   ├── 00000000000500000000.log              # 新 segment（5GB 默认）
│   ├── 00000000000500000000.index
│   └── ...
```

- **Segment 文件**：达到 `segment.bytes`（默认 1GB）或 `segment.ms`（默认 7 天）触发滚动；
- **稀疏索引**：`.index` 是 offset → file position 的稀疏索引（每写入一定字节后插一项）；
- **Time Index**：`.timeindex` 是 timestamp → offset 的稀疏索引，用于按时间查询（`offsetsForTimes` API）；
- **零拷贝**：Consumer 拉取时使用 `FileChannel.transferTo` 走 `sendfile()` 系统调用，避免用户态拷贝；
- **页缓存友好**：所有读写优先使用 OS 页缓存，broker 进程崩溃不丢数据（依赖副本而非本地持久化保证）。

### 7.2 消息格式

- **RecordBatch**（`org.apache.kafka.common.record.RecordBatch`）：
  - 一个 batch 内含多条 record（`linger.ms` 或 `batch.size` 控制），启用 LZ4 / ZSTD / Snappy 压缩时整批压缩；
  - **Magic V2**（Kafka 0.11+）：batch 内含 producer ID、sequence、transaction marker，支持幂等与事务；
  - **Compact Format**（KIP-98，Kafka 1.0+）：用 varint / zig-zag 编码整数与长度，省空间；
- **Record**（`org.apache.kafka.common.record.Record`）：
  - length / attributes / timestampDelta / offsetDelta / keyLen / key / valueLen / value / headers。

### 7.3 刷盘与压缩

- **刷盘策略**：默认异步刷盘（依赖 OS 页缓存 + 副本），可配 `log.flush.interval.ms`（默认空，即不主动刷）；
- **数据可靠性**：副本 ISR 内 ack 即认为写入成功（`acks=all` + `min.insync.replicas>=2`）；
- **Compaction**：对 `cleanup.policy=compact` 的 topic，**后台 LogCleaner** 线程定期合并 segment，每个 key 仅保留最新 value（实现 changelog / CDC 场景）；
- **Tiered Storage**（3.6+）：`remote.log.storage.volumes` + S3/HDFS，segment 异步上传对象存储，本地仅保留热数据（`local.retention.ms`），冷数据按需从对象存储按时间窗口拉取。

## 八、索引机制（流类型专项展开）

### 8.1 Log 结构（Append-only Log + Segment + Index file + Time index）

- **Append-only Log**：每条消息按 offset 单调追加写入，**不可修改、不可删除**（仅过期删除或 compaction 覆盖）；
- **Segment**：partition 的物理分片，达到大小或时间阈值后滚动关闭，开启新 segment；
- **Index file**：稀疏 offset → file position 索引（`Kafka 3.7` 默认 `index.interval.bytes=4096`），用于**二分查找**快速定位 offset 对应的 file position；
- **Time index**：稀疏 timestamp → offset 索引，用于 `offsetsForTimes` API（按时间戳定位 offset）；
- **Txn index**：事务 commit/abort marker 索引（KIP-98），用于 EOS 读取时过滤未提交事务的 record。

### 8.2 分区模型（Partition vs Shard / 路由策略）

- **Partition（Kafka 概念）**：topic 内的并行度单位；同一 partition 内有序，跨 partition 无序；
- **Partition vs Shard**：
  - Kafka 的 partition 类似 **shard**（一个分区是数据子集的一个独立副本集）；
  - 但 Kafka 把 partition 当作**消费顺序保证单位**（同一 key 路由到同一 partition 即顺序保证）；
  - Pulsar 的 shard（segment）由 BookKeeper bookie 均匀承载，Kafka 的 partition 由 leader broker 集中承载；
- **分区策略**（`Partitioner.java`）：
  - **Hash Partitioner**（默认）：`murmur2(key) % numPartitions`（启用 key 时）；
  - **RoundRobin**：无 key 时轮询分布；
  - **Custom Partitioner**：实现 `Partitioner` 接口注入（如按业务字段 sticky）；
  - **Sticky Partitioner**（2.4+）：无 key 时尽可能批次内集中到同一 partition，提高 batch 命中率（吞吐量提升 30%+）；
- **动态分区数变更**：`kafka-topics.sh --alter --partitions N` 增加 partition 数；**减少 partition 数不支持**（破坏分布均衡）。

### 8.3 副本与一致性（ISR / Leader-Follower / acks / min.insync.replicas）

```
        Producer (acks=all)
            │
            ▼
   ┌─── Leader Broker (Partition 0) ───┐
   │   LEO = 100, HW = 95              │
   │   writes to local log             │
   └──────┬────────────────┬───────────┘
          │ replicate      │          │
          ▼               ▼          ▼
   ┌─Follower 1─┐  ┌─Follower 2─┐ ┌─Follower 3─┐ (offline)
   │  LEO=100   │  │  LEO=99    │ │             │
   │  ISR=yes   │  │  ISR=yes   │ │  ISR=no     │
   └────────────┘  └────────────┘ └─────────────┘
```

- **ISR（In-Sync Replicas）**：与 leader 保持同步（`replica.lag.time.max.ms` 内有 fetch）的副本集合；
  - 写入需 `acks=all` 时，leader 等所有 ISR 副本 ack 才返回成功；
  - 选举 controller/leader 时只能从 ISR 选；
- **acks 语义**：
  - `acks=0`：不等任何副本 ack，最快但可能丢；
  - `acks=1`：仅 leader 写入即返回，leader 崩溃可能丢未复制的数据；
  - `acks=all`（推荐）：等待所有 ISR ack，最安全；
- **min.insync.replicas**：topic 级配置，要求 ISR 副本数 ≥ 该值才能写入；
  - 配合 `acks=all`，提供「ISR 不足时拒绝写入」的保护；
- **High Watermark (HW)**：所有 ISR 都已复制的最大 offset，Consumer 只能消费到 HW；
- **Leader Epoch (KIP-320)**：leader 选举会递增 epoch，取代旧版 HW 恢复逻辑（避免脑裂导致数据不一致）。

### 8.4 偏移量管理（Consumer Offset / __consumer_offsets / Commit 语义）

- **__consumer_offsets**：内部 topic，存储 `(group, topic, partition) → offset` 映射；
  - 默认 50 partition，副本因子 3；
  - 由 **GroupCoordinator**（`GroupCoordinator.scala`）管理；
- **Commit 语义**：
  - **Auto Commit**：`enable.auto.commit=true` + `auto.commit.interval.ms=5000`，后台定期 commit；
  - **Manual Commit**：`consumer.commitSync()` / `commitAsync()`，开发者控制；
  - **at-most-once**：先 commit 再处理 → 崩溃可能漏；
  - **at-least-once**：先处理再 commit → 崩溃可能重；
  - **exactly-once**：配合 EOS（幂等 Producer + 事务）→ 不重不漏；
- **KIP-848**（Kafka 3.7 GA）：**Consumer Group Protocol: Consumer**（新协议 "next generation"），将协调职责从 broker 移到客户端（服务器侧心跳），消除 stop-the-world rebalance。

### 8.5 存储分层（Tiered Storage / 冷热分层 / 对象存储后端）

- **Tiered Storage**（3.6 GA，KIP-405）：
  - **Hot tier**：本地磁盘，存放最近写入的 segment（`local.retention.ms`/`local.retention.bytes`）；
  - **Cold tier**：对象存储（S3 / Azure Blob / GCS / HDFS / MinIO / Ozone）；
  - 启动时**异步**把已完成 segment 上传到对象存储，consumer 按 offset 读取时按需从对象存储拉取；
- **意义**：
  - broker 本地磁盘不再需要无限增长（retention 不再被 jib 配额限制）；
  - 分区数量可大幅扩展（不受单 broker 磁盘限制）；
  - 集群重启恢复更快（本地只重放少量热数据）；
- **配置示例**：
  ```properties
  remote.log.storage.system.enabled=true
  remote.log.storage.manager.class.name=org.apache.kafka.server.log.remote.storage.RemoteTieredStorageManager
  remote.log.storage.volumes=...
  ```

## 九、查询/计算/接口层

### 9.1 生产/消费 API（Producer / Consumer / Streams / Connect / KSQL）

- **KafkaProducer**（`clients/src/main/java/org/apache/kafka/clients/producer/KafkaProducer.java`）：
  - 同步 `send().get()`、异步 `send(Callback)`、`flush()` 强制刷；
  - 关键配置：`acks`、`batch.size`、`linger.ms`、`compression.type`、`enable.idempotence`；
- **KafkaConsumer**（`clients/src/main/java/org/apache/kafka/clients/consumer/KafkaConsumer.java`）：
  - 订阅：`subscribe(topics)`（按 topic）/ `assign(partitions)`（精确控制）；
  - 拉取：`poll(Duration)` 阻塞拉取 batch；
  - 提交：`commitSync()` / `commitAsync()`；
- **Kafka Streams**（`streams/src/main/java/org/apache/kafka/streams/KafkaStreams.java`）：
  - DSL：`KStream`、`KTable`、`GlobalKTable`；
  - 算子：`map`、`filter`、`join`、`aggregate`、`window`；
  - 状态存储：RocksDB（默认）/ in-memory；
  - Exactly-Once：基于 `processing.guarantee=exactly_once`；
- **Kafka Connect**（`connect/runtime/src/main/java/org/apache/kafka/connect/runtime/`）：
  - Source Connector：从 DB（Debezium）/ 文件 / HTTP 拉取数据写入 Kafka；
  - Sink Connector：从 Kafka 消费写入 S3 / ES / DB；
  - 框架：REST API 管理 connector 生命周期；
- **ksqlDB / KSQL**：基于 Kafka Streams 的 SQL-like 引擎，对流做持续查询（Continuous Query）。

### 9.2 消息语义（at-most-once / at-least-once / exactly-once）

- **at-most-once**：Producer 不重试、Consumer 先 commit 后消费；可能丢；
- **at-least-once**：Producer 重试、Consumer 先消费后 commit；可能重；
- **exactly-once**（Kafka 0.11+）：
  - **幂等 Producer**（`enable.idempotence=true`）：每条消息带 `(producerId, sequenceNumber)`，broker 端去重；
  - **事务 Producer**（`transactional.id`）：跨多个 topic-partition + offset commit 原子提交；
    - API：`producer.beginTransaction(); ...; producer.commitTransaction();`
    - 内部用 **__transaction_state** topic 存事务状态；
    - 与幂等 Producer 组合实现 EOS；
  - **读会限制**：Consumer 配 `isolation.level=read_committed`，只读已提交事务的 record；
- **代价**：事务 Producer 性能损失 ~20%（额外协调开销）。

### 9.3 流处理引擎（Kafka Streams / ksqlDB / Pulsar Functions / NATS JetStream）

| 系统 | 计算模型 | 状态存储 | 部署 | 适用场景 |
|------|---------|---------|------|---------|
| **Kafka Streams** | 嵌入式 Java 库 / DSL | RocksDB | 与微服务同进程 | 微服务内流处理 |
| **ksqlDB** | 持续 SQL | RocksDB | 独立 server | 低代码流分析 |
| **Apache Flink** | Dataflow API | RocksDB / Heap | 独立集群 | 大规模流处理 |
| **Apache Spark Streaming** | Micro-batch | DStream | 独立集群 | 离线 + 近实时 |
| **Pulsar Functions** | 函数式 | BookKeeper | Broker 嵌入 | 轻量 ETL |
| **NATS JetStream** | 原生消息语义 | 文件 | 嵌入 server | 边缘云原生 |

- Kafka Streams 的优势：**库而非集群**（无需独立部署，资源随应用扩缩），但相应不能跨服务复用状态。

### 9.4 Schema 演进（Schema Registry / Avro / Protobuf / JSON Schema / 向后兼容）

- **Confluent Schema Registry**（独立服务，端口 8081）：
  - 集中管理 schema 版本（`/subjects/<topic>-value/versions`）；
  - 客户端启动时拉取最新 schema；
  - **兼容性策略**：`BACKWARD`（新 schema 可读旧数据）、`FORWARD`、`FULL`、`NONE`；
- **格式**：
  - **Avro**（默认）：schema + 二进制，compact 但需编译期生成代码；
  - **Protobuf**：IDL + 二进制，高性能；
  - **JSON Schema**：可读但大；
- **演进场景**：
  - 添加**可空字段**（向后兼容）；
  - 删除字段（标记 `@deprecated` + 不删）；
  - 修改字段名 → 不兼容（破坏消费者）；
- **参考**：`docs/superpowers/specs/` 中可对比 Postgres / ClickHouse 的 schema 演进。

## 十、事务与一致性

- **EOS 事务**（Kafka 0.11+）：
  - 内部用 **__transaction_state** topic 存事务状态（coordinator partition 选举）；
  - **两阶段提交**（WriteLog + CommitLog）：
    1. Producer 写入数据到 partition（标记未提交）；
    2. Producer 发 commit 到 transaction coordinator；
    3. Coordinator 写入 commit marker 到所有涉及 partition；
    4. Consumer 读 `read_committed` 模式看到 commit marker 后才消费；
- **幂等保证**：`enable.idempotence=true` 自动开启，要求 `acks=all`、`max.in.flight.requests.per.connection<=5`；
- **跨分区原子性**：事务可同时 commit多个 topic-partition 的写入 + offset commit（如 Streams 的 EOS state store）；
- **隔离级别**：
  - `read_uncommitted`：默认，读所有 record；
  - `read_committed`：只读已提交事务的 record；
- **与关系型 ACID 区别**：Kafka 事务是**逻辑事务**（跨 topic 原子写 + offset），不提供 read-modify-write 的行级锁。

## 十一、复制与高可用

- **Controller 选举（KRaft）**：
  - 集群自动从 broker 中选 controller（基于 Raft）；
  - Controller 维护 `__cluster_metadata` topic（partition = broker 数）；
  - 老版本基于 ZooKeeper，Kafka 3.x 完全移除 ZK 路径；
- **Leader 选举（partition 级别）**：
  - Leader 故障 → controller 从 ISR 选新 leader；
  - 选举延迟：通常 < 几秒（取决于 `unclean.leader.election.enable`）；
  - **`unclean.leader.election.enable=false`（默认）**：ISR 不足时**拒绝**选 leader，partition 不可写；
  - `unclean.leader.election.enable=true`：允许非 ISR 副本成为 leader，**有数据丢失风险**；
- **跨集群同步**：
  - **MirrorMaker 2**（`connect/mirror/`）：跨数据中心/区域异步复制；
  - **Confluent Cluster Linking**：商业版增强；
- **灾难恢复**：备份 `__cluster_metadata` topic + 各 partition 数据目录；Tiered Storage 简化冷备。

## 十二、扩展性

- **水平扩展**：增加 broker 后 partition rebalance（`kafka-reassign-partitions.sh`）；
- **分区扩展**：动态增加 partition 数（`--alter --partitions N`），但 key 的 partition 路由会改变（旧 key 可能到不同 partition，破坏顺序保证）；
- **集群联邦**：跨多个集群通过 MirrorMaker 同步；
- **Tiered Storage**：突破单机磁盘配额，分区数量可大幅提升；
- **限制**：
  - 单集群 partition 数建议 < 200k（受 controller 内存限制）；
  - 单 broker 连接数受文件描述符限制（建议 < 100k）；
  - KRaft 集群规模通常 < 5k broker。

## 十三、性能与基准

- **官方基准**（3.5+，官方参考链接）：
  - **单 partition 吞吐**：~900 MB/s（pipelined 批处理，LZ4 压缩）；
  - **端到端延迟**：p99 < 10ms（同区域）；
  - **扩展性**：broker 数线性扩展（每加 broker 吞吐近线性增长）；
- **OpenMessaging Benchmark**（社区标准）：
  - LinkedIn 测试：千万级消息/秒（双 12 集群）；
  - Confluent Cloud：百万级消息/秒单集群；
- **优化要点**：
  - **批处理**：`linger.ms=5~100` + `batch.size=64KB~1MB`；
  - **压缩**：LZ4 / ZSTD 通常节省 50%+ 网络；
  - **分区数**：单 partition 1MB/s 估算；
  - **消费者**：max.poll.records 调优避免频繁 rebalance。

## 十四、运维与可观测性

- **指标导出**：JMX 端口 9999 + Prometheus JMX Exporter；
- **关键指标**：
  - `kafka.server:type=BrokerTopicMetrics,name=MessagesInPerSec`（入站速率）；
  - `kafka.server:type=ReplicaManager,name=UnderReplicatedPartitions`（欠复制 partition）；
  - `kafka.controller:type=ControllerStats,name=ActiveControllerCount`（controller 状态）；
  - `kafka.log:type=LogFlushStats,name=LogFlushRateAndTimeMs`（刷盘延迟）；
- **客户端指标**：Producer/Consumer 自带 JMX metrics；
- **慢请求日志**：`kafka.request.logger.enabled=true`，记录超过阈值的请求；
- **管理工具**：
  - `kafka-topics.sh`、`kafka-consumer-groups.sh`、`kafka-reassign-partitions.sh`、`kafka-log-dirs.sh`；
  - **CMAK / AKHQ**（社区 UI）、**Confluent Control Center**（商业）；
- **健康检查**：`bin/kafka-broker-api-versions.sh --bootstrap-server` 验证 API 版本兼容；
- **备份恢复**：
  - 文件级备份：直接复制数据目录（需先停 broker）；
  - **MirrorMaker**（在线同步）；
  - **Tiered Storage**：天然支持快照恢复。

## 十五、生态与集成

- **客户端 SDK**：
  - **官方 Java/Scala**：Kafka 自身一部分；
  - **Confluent 官方**：Python（`confluent-kafka-python`，librdkafka 绑定）、Go（`confluent-kafka-go`）、C/C++（librdkafka）、.NET、Node.js；
  - **社区**：Rust（`rdkafka`、`kafka-rust`）、Ruby、Erlang；
- **CDC 集成**：
  - **Debezium**（基于 Connect）：MySQL / Postgres / MongoDB CDC 接入 Kafka；
  - **Maxwell**、**Flume**；
- **存储集成**：Kafka Connect Sink → S3 / HDFS / Elasticsearch / Cassandra / JDBC；
- **Schema 管理**：Confluent Schema Registry；
- **流处理引擎**：Kafka Streams（官方）、Apache Flink（Kafka source/sink 一等公民）、Apache Spark Streaming；
- **消息队列兼容**：AWS MSK、Azure Event Hubs、Confluent Cloud、阿里云 Kafka、华为云 Kafka；
- **Kubernetes 部署**：Strimzi Operator（生产级）、Confluent Operator（商业）。

## 十六、核心论文与算法原理

- **Kafka 原始论文**：Jay Kreps, "Kafka: a Distributed Messaging System for Log Processing"（NetDB 2011）；
- **Raft 共识论文**：Diego Ongaro & John Ousterhout, "In Search of an Understandable Consensus Algorithm"（USENIX ATC 2014）→ KRaft 实现基础；
- **EOS 设计文档**：KIP-98、Transaction Design（社区 wiki）；
- **Tiered Storage 设计**：KIP-405（远程日志存储）；
- **Consumer Group 协议演进**：KIP-848（新一代服务器侧心跳协议）；
- **零拷贝**：Mike Mellor, "Zero Copy I/O" 论文与 sendfile(2) man page；
- **稀疏索引设计**：Kafka Design 文档 "Efficient offset management"。

## 十七、源码学习路径

### 17.1 必读模块清单（按入门到进阶）

| 阶段 | 模块 | 路径 | 说明 |
|------|------|------|------|
| 入门 | Log | `core/src/main/scala/kafka/log/Log.scala`、`LogSegment.scala` | 顺序写、segment 滚动、稀疏索引 |
| 入门 | Producer | `clients/src/main/java/org/apache/kafka/clients/producer/KafkaProducer.java` | 批处理、RecordAccumulator、partition 路由 |
| 入门 | Consumer | `clients/src/main/java/org/apache/kafka/clients/consumer/KafkaConsumer.java` | 订阅、rebalance、fetch |
| 进阶 | ReplicaManager | `core/src/main/scala/kafka/server/ReplicaManager.scala` | ISR、HW、Leader Epoch |
| 进阶 | GroupCoordinator | `core/src/main/scala/kafka/coordinator/group/GroupCoordinator.scala` | Consumer group 状态机 |
| 进阶 | TransactionCoordinator | `core/src/main/scala/kafka/coordinator/transaction/TransactionCoordinator.scala` | EOS 事务两阶段提交 |
| 高级 | KRaft | `core/src/main/scala/kafka/raft/KafkaRaftManager.scala` | Raft 实现、Snapshot |
| 高级 | Tiered Storage | `core/src/main/scala/kafka/server/log/remote/` | 远程存储抽象与后端 |
| 高级 | Streams | `streams/src/main/java/org/apache/kafka/streams/processor/` | Topology、Task、State Store |

### 17.2 推荐阅读顺序

1. **第 1 周**：Kafka 官方文档 + `Log.scala`（理解顺序写 + segment）；
2. **第 2 周**：Producer / Consumer 源码（理解客户端协议）；
3. **第 3 周**：ReplicaManager + ISR 机制；
4. **第 4 周**：KRaft 共识（对照 Raft 论文）；
5. **第 5 周**：Streams 与 EOS 事务；
6. **第 6 周**：Connect 框架 + Tiered Storage 改造。

## 十八、源码规模与开发语言

- **规模**：Kafka 3.7 约 **80 万行** Scala + Java；
- **语言占比**：Scala ~70%（核心）、Java ~30%（clients / streams）；
- **构建系统**：Gradle（`build.gradle`、`gradlew`）；
- **JDK**：17（3.7+），11（3.0~3.6）；
- **测试**：JUnit 4 + ScalaTest + 集成测试集群（`tests/`）；
- **CI**：GitHub Actions + Apache Jenkins。

## 十九、学习资源

- **官方文档**：
  - [Apache Kafka 官方文档](https://kafka.apache.org/documentation/)
  - [Kafka Improvement Proposals (KIPs)](https://cwiki.apache.org/confluence/display/KAFKA/Kafka+Improvement+Proposals)
- **书籍**：
  - Neha Narkhede, Gwen Shapira, Todd Palino, "Kafka: The Definitive Guide"（O'Reilly）
  - Bill Bejeck, "Kafka Streams in Action"（Manning）
- **视频课程**：Confluent 官方课程、Udemy 上 Stephane Maarek 的 Apache Kafka 系列；
- **社区**：
  - **Confluent 博客**：design.kip 与 deep dive；
  - **Kafka Summit** 会议视频（YouTube）；
  - **Reddit r/apachekafka**、**Confluent Community Slack**；
- **中文资源**：GitHub 上的 `apachecn/kafka-doc-zh` 翻译项目、CSDN/掘金 Kafka 专栏。

## 二十、可移植性与部署

- **平台支持**：Linux（生产推荐，x86_64 / ARM64）、macOS / WSL2（开发）、Windows（不推荐生产）；
- **容器化**：Docker 镜像 `apache/kafka`（含 KRaft 模式）；
- **Kubernetes**：Strimzi Operator（生产级，含 Helm Chart 与 Operator）；
- **混合云**：本地 + 公有云通过 MirrorMaker 同步；
- **嵌入式**：Kafka 不适合嵌入（依赖 JVM + OS 文件系统）；
- **JVM 调优**：G1GC 推荐（`-Xmx6g -Xms6g -XX:+UseG1GC`），关注 GC 停顿时间；
- **磁盘**：推荐 **JBOD**（多盘）独立订阅，**避免 RAID 5/6**（写放大影响吞吐）。

## 二十一、变体与衍生项目

- **Confluent Platform**（商业发行版）：含 Schema Registry、Connect 商业版、ksqlDB、REST Proxy、Auto Data Balancer；
- **AWS MSK**（托管）：AWS 管理的 Kafka 集群；
- **Redpanda**（本 wiki 后续）：C++ 重写，10x 延迟优化，Kafka 协议兼容；
- **Apache Pulsar**（本 wiki 后续）：BookKeeper + 计算存储分离，云原生友好；
- **AutoMQ**：S3 原生 Kafka 替代（直接用对象存储）；
- **NATS JetStream**（本 wiki 后续）：轻量、云原生边缘场景；
- **Materialize**：基于 Kafka 的流式 SQL 数据库；
- **Benthos / Redpanda Connect**：现代 ETL 工具，多输入输出。

## 二十二、风险与陷阱

### 22.1 常见误用

- **Producer 丢失数据**：未配 `acks=all` + `enable.idempotence=true`；
- **Consumer 重复消费**：未使用事务，先 commit offset 后处理崩溃；
- **消息顺序错乱**：多 partition + 同一 key 但不同 partition；
- **分区数频繁变更**：破坏 key 顺序保证、rebalance 风暴；
- **Log 无限增长**：未配 `retention.ms` 或 `cleanup.policy=delete`；
- **大消息**：单条 > 1MB 严重影响吞吐（应拆分）；
- **ZooKeeper 残留**：KRaft 模式部署后未清理 ZK 配置；

### 22.2 性能陷阱

- **批处理过小**：`linger.ms=0` + `batch.size=1KB` 浪费 CPU 与网络；
- **压缩不当**：使用 `gzip` 但数据已是 JSON（压缩比低 + CPU 高）→ 改 LZ4 / ZSTD；
- **key 选择不当**：高基数 key 导致 partition 倾斜 → 改 sticky partitioner 或一致性 hash；
- **Consumer poll 间隔过短**：`max.poll.interval.ms` 过小导致频繁 rebalance；
- **Group rebalance 风暴**：consumer 数 > partition 数 → 半数成员离组触发 rebalance；
- **GC 停顿**：未调优 JVM 导致长 GC 停顿，触发 `session.timeout.ms`；

### 22.4 迁移痛点

- **从 ZK 升级 KRaft**：3.x 支持 in-place 迁移，但需先升级到 3.3+；
- **Confluent 商业特性迁移**：Schema Registry、REST Proxy 不能直接搬；
- **跨集群迁移**：MirrorMaker 2 配置复杂（offset 同步）；
- **Schema 不兼容**：升级 schema 时未做 `BACKWARD` 检查，下游消费失败；

### 22.5 运维雷区

- **磁盘满未监控**：broker 不可写 → 整 topic 不可用；
- **unclean leader election**：开启后错误负载下数据丢失难恢复；
- **Kafka Broker OOM**：JVM heap 过小，Segment 索引占用大；
- **网络分区**：min.insync.replicas 不足导致写入失败；
- **Tiered Storage 配置错误**：对象存储凭据过期 → segment 上传失败；
- **Controller 频繁切换**：网络抖动导致 controller 反复选举（KRaft 配置 `controller.quorum.voters`）。

## 二十三、与本项目的关联

- **可借鉴的设计**：
  - **append-only log + segment 文件**：本项目 `engineering/src/db/core/wal.c` 的 WAL 设计可参考 Kafka segment + 刷盘策略；
  - **ISR + ack 语义**：与 `db/wal.h` 的 WAL 协调器（`wal_buf.c`）思想一致；
  - **LSM-Tree Memtable + SSTable** vs Kafka Log：都是顺序写优先，但 Kafka 是不可变，LSM 是分层合并；
  - **EOS 事务**：与 Postgres 多版本事务对比学习（`docs/storage-architecture.md`）；
- **不适合借鉴**：
  - KRaft / Raft 共识：复杂度高，本项目目前用单节点 WAL；
  - Tiered Storage：依赖对象存储，本项目当前无对象存储后端；
- **流式 ETL 集成**：本项目无内置流接入能力；如需对接 Kafka，可参考 `engineering/apps/` 未来扩展。

## 二十四、参考资料

- [Apache Kafka 官方主页](https://kafka.apache.org/)
- [Apache Kafka 官方文档](https://kafka.apache.org/documentation/)
- [Kafka Improvement Proposals (KIPs)](https://cwiki.apache.org/confluence/display/KAFKA/Kafka+Improvement+Proposals)
- [KRaft 模式文档](https://kafka.apache.org/documentation/#kraft)
- [Tiered Storage (KIP-405)](https://cwiki.apache.org/confluence/display/KAFKA/KIP-405%3A+Tiered+Storage+for+Kafka)
- [Consumer Group Protocol: Consumer (KIP-848)](https://cwiki.apache.org/confluence/display/KAFKA/KIP-848%3A+The+New+Consumer+Group+Protocol+%28aka+Consumer+Group+Protocol+%27Consumer%27%29)
- [Confluent 官方文档](https://docs.confluent.io/)
- [OpenMessaging Benchmark](https://github.com/openmessaging/benchmark)
- [Strimzi Kafka Operator](https://strimzi.io/)
- [Redpanda 主页](https://redpanda.com/)（对比参考）
- [Apache Pulsar 主页](https://pulsar.apache.org/)（对比参考）
- [NATS 主页](https://nats.io/)（对比参考）
- [Kafka: The Definitive Guide（O'Reilly 书籍）](https://www.oreilly.com/library/view/kafka-the-definitive/9781491936153/)
- [Apache Kafka GitHub](https://github.com/apache/kafka)