# Pulsar Wiki（云原生的分层消息流平台）

> 参考版本：Apache Pulsar 3.x（Java 11+，Apache 2.0 许可）。本 wiki 重点对比 Kafka 介绍分层架构（Broker / BookKeeper / ZooKeeper 三层）、Segment / ManagedLedger、Cursor 偏移量模型、Pulsar Functions、计算存储分离。

## 一、项目定位

Apache Pulsar 是由 **Yahoo!**（2016 年开源，后捐赠给 Apache 顶级项目，2018 年毕业）开发的 **云原生分层消息流平台**，定位为「下一代分布式消息系统」，以 **计算与存储分离（Broker 无状态 + BookKeeper 有状态）**、**Segment 级存储**、**多租户（tenant/namespace/topic 三级）**、**内置 Functions / IO Connectors** 为差异化特性，提供 Kafka 协议兼容（Pulsar 自带 Kafka 兼容适配器）。

- **痛点**：
  - Kafka 计算与存储耦合，broker 扩容需迁移数据；
  - Kafka 多租户能力弱（无原生租户隔离）；
  - Kafka 长期保留（retention months+）成本高（必须 Tiered Storage）；
  - 云原生场景（K8s 弹性扩缩）需要无状态 broker；
  - 缺乏内置的轻量级流处理（Kafka Streams 仍需独立库）；
- **典型用户**：Yahoo!（Pulsar 起源）、Tencent（QQ 邮箱 / 微信）、Splunk、Twitter、Verizon Media、Tencent Cloud、华为云、阿里云消息队列、Apache Pinot（与 Pulsar 集成做实时分析）；
- **核心口号**：**"Cloud-native, distributed messaging and streaming"** + **"Flexible Pub/Sub + Unified Messaging Queue + Serverless Functions"** + **"Built for the cloud era"**。

## 二、发展历史与社区

- **起源**：2012 年 Yahoo! 内部开发，最初叫 **PubSub**（开源时改名 Pulsar，意为「脉冲星」）；
- **里程碑版本**：
  - 2016：开源，进入 Apache 孵化器；
  - 2018：成为 Apache 顶级项目；
  - 2.0（2018）：Schema Registry、跨地域复制；
  - 2.5（2019）：事务支持（EOS）、Kafka 兼容协议适配器；
  - 2.7（2020）：Pulsar Functions（生产可用）、TableView；
  - 2.8（2021）：分层存储 GA（Tiered Storage）、Topic Compaction；
  - 2.10（2022）：Pulsar SQL（基于 Trino/Presto）；
  - 3.0（2023）：Kop 1.0 GA（Kafka 协议兼容 GA）、Offload 优化、Functions 改进；
  - 3.2（2024）：多集群发现改进、Auto Recovery；
  - 3.3（2025）：Serverless Functions（无状态 + 自动扩缩）；
- **维护状态**：活跃，每月 minor；
- **社区**：GitHub 14k+ star、Slack、Pulsar Summit 全球大会、Apache Committers 群体（中国贡献者占比高）；
- **生态**：StreamNative（商业公司，提供 Pulsar Cloud / StreamNative Platform）、Apache Pinot（实时分析）、Apache Flink（一等公民集成）。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（宽松商用）；
- **生态关系**：
  - **StreamNative** 是主要商业化公司，提供 StreamNative Cloud（托管）、StreamNative Platform（企业特性：多集群联邦、RBAC、Auto-balancer、StreamNative Kafka Connect）；
  - **Apache BookKeeper**：Pulsar 内置依赖的存储层（也是独立 Apache 项目）；
  - **Kafka 兼容**：通过内置 **KOP（Kafka on Pulsar）** 协议适配器，Kafka 客户端无需改；
  - **AWS 托管**：Amazon MQ for Apache Pulsar（AWS 托管服务）；
  - **国产云**：阿里云消息队列 RocketMQ 5.x（融合了部分 Pulsar 思想）、华为云 DMS for Pulsar、腾讯云 TDMQ for Pulsar。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Yahoo!**：Pulsar 起源公司，PB 级邮件事件流；
  - **Tencent**：QQ 邮箱通知流（10+ 万 topics）、微信支付事件流；
  - **Splunk**：日志采集管道（替代 Kafka）；
  - **Twitter**：Timeline 事件分发；
  - **Verizon Media**：广告投放实时数据；
  - **Huawei Cloud**：DMS 消息服务；
  - **Aliyun**：MQ for Apache Pulsar 商业化；
  - **Bilibili**：直播弹幕事件流；
  - **JD.com**：电商订单事件流；
  - **Appen**：AI 数据标注任务分发；
- **典型场景**：
  - **多租户 SaaS**：原生租户隔离（与 Kafka 商业版多租户方案对比）；
  - **长期保留**：retention 可设置数月/数年（BookKeeper 滚动 + Tiered Storage）；
  - **多协议统一**：MQTT / Kafka / AMQP / WebSocket 同一集群；
  - **流式 AI 数据准备**：实时特征分发；
  - **跨地域复制**：内置跨 cluster geo-replication；

## 五、一句话对比定位

> Pulsar 是 **"计算与存储分离（Broker 无状态 + BookKeeper 有状态）+ Segment 滚动 + 多租户（tenant/namespace/topic）+ 内置 Functions/IO + Kafka 协议兼容（KOP）+ Tiered Storage + Apache 2.0"** 的云原生流平台；与 Kafka（生态最广）、Redpanda（C++ 重写性能优先）、NATS JetStream（轻量云原生边缘）构成事件流生态。Pulsar 的差异化是 **"云原生架构 + 多租户 + 长期保留 + 多协议"**。

## 六、整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                     Producer 客户端                                    │
│  pulsar-client/src/main/java/org/apache/pulsar/client/               │
│  ├─ PulsarClient（连接多 broker + 自动发现）                            │
│  ├─ Producer / Consumer / Reader / ReaderBuilder                      │
│  └─ Schema（内置 Avro/Protobuf/JSON Schema/KeyValue）                  │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ Pulsar Binary Protocol (TCP) + Optional TLS
┌─────────────────────▼────────────────────────────────────────────────┐
│                     Broker 集群（无状态计算层）                          │
│  pulsar-broker/src/main/java/org/apache/pulsar/broker/                │
│  ├─ BrokerService（Topic / Subscription / Dispatcher）                 │
│  ├─ service/Topic / Subtopic / Dispatcher                             │
│  ├─ service.PersistentTopic / PersistentSubscription                  │
│  ├─ managed-ledger/ManagedLedgerImpl（BookKeeper 客户端抽象）            │
│  ├─ loadbalance/（bundle 负载均衡）                                     │
│  ├─ service.Producer / Consumer                                       │
│  ├─ service.schema（Schema Registry 内置）                              │
│  └─ service.transaction（EOS 事务协调）                                  │
├─────────────────────┬──────────────────┬─────────────────────────────┤
│                    Tiered Storage     │   Pulsar Functions / IO       │
│  pulsar/tiered-storage/               │   pulsar-functions/           │
│  ├─ jcloud/（S3/GCS/Azure Blob）     │   ├─ runtime/                  │
│  ├─ file-system/（HDFS/NFS）         │   ├─ worker/                   │
│  └─ offload policies                  │   └─ instance/                 │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              Apache BookKeeper 集群（有状态存储层）                      │
│  external/bookkeeper/（独立 Apache 项目）                                │
│  ├─ Bookie Server（持久化 Ledger Entry 到本地磁盘）                      │
│  ├─ Journal / Entry Log / Index / Ledger Cache                       │
│  └─ AutoRecovery / Auditor                                           │
├─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│          元数据与协调层（ZooKeeper / etcd / RocksDB）                    │
│  pulsar-broker/src/main/java/org/apache/pulsar/broker/                │
│  ├─ metadata/（topic 元数据 + bundle 分配）                             │
│  ├─ service.zookeeper/（ZK 会话）                                      │
│  └─ ManagedLedgerFactory（管理 BookKeeper ledger）                      │
└──────────────────────────────────────────────────────────────────────┘
```

### 6.1 三层架构详解

| 层 | 组件 | 职责 | 状态性 |
|----|------|------|--------|
| **计算层** | Broker | 协议处理、Topic 调度、Dispatcher、Schema 协调 | **无状态**（重启可接管任何 bundle） |
| **存储层** | BookKeeper | Segment 持久化、副本同步、读写路径 | **有状态**（每个 bookie 持有 ledger entry） |
| **协调层** | ZooKeeper | 集群元数据、Broker 注册、Topic 配置 | **强一致** |

- **优势对比 Kafka：
  - Broker 故障 → 另一 broker 直接接管 topic（ZK 选择，毫秒级）；
  - 存储层可独立扩容（加 bookie 不影响 broker）；
  - 长期保留数据可放在 Tiered Storage（S3/HDFS），bookie 只持热数据；
- **代价**：
  - 三层运维复杂度高；
  - BookKeeper 副本传输开销；
  - ZooKeeper 依赖（3.x 可选 etcd）；

### 6.2 多租户模型

Pulsar 的多租户是**原生一等公民**：

```
Tenant (组织)
 └── Namespace (业务域)
       ├── Topic (消息流)
       └── Topic (另一消息流)
```

- **Tenant**：组织（如 Yahoo / Tencent），可设配额、ACL、隔离策略；
- **Namespace**：业务域（多环境、多集群），可设 retention、bookie affinity；
- **Topic**：消息流单元，可跨 namespace 引用；
- **Bundle**：namespace 内 topic 的逻辑分组（一致性 hash 分配到 broker）；

## 七、存储引擎（Log 结构）

Pulsar 的存储由 **ManagedLedger → Ledger → Entry** 三层抽象。

### 7.1 ManagedLedger 抽象

`pulsar-broker/src/main/java/org/apache/pulsar/broker/ManagedLedger/ManagedLedgerImpl.java`：

```
ManagedLedger (Topic 一条流)
   ├── Ledger 0  [Entry_0, Entry_1, ..., Entry_999]
   ├── Ledger 1  [Entry_1000, ..., Entry_1999]
   ├── Ledger 2  (active, 正在追加)
   └── Ledger N
```

- **Ledger**：BookKeeper 的一次写入单元（默认 5,000 entries / 100MB 滚动）；
- **Entry**：单条消息（含 ledgerId + entryId + payload）；
- **ManagedLedger**：topic 的逻辑视图（持久 + active ledgers）；
- **ManagedCursor**：订阅偏移量（每个订阅一个 cursor，独立维护位置）；

### 7.2 BookKeeper Ledger（底层）

`external/bookkeeper/bookkeeper-server/src/main/java/org/apache/bookkeeper/bookie/`：

```
Bookie 本地目录结构：
  journals/         # write-ahead log（顺序写，重启时 replay）
    └── journal.current
  ledgers/           # 已完成 ledger
    └── <ledger-id>/
        ├── log     # entry 顺序追加
        └── idx     # entryId → file offset
  index/             # last entry 索引
    └── ...
```

- **Journal**：所有写操作的 WAL（顺序写，单文件，回滚机制）；
- **Entry Log**：已落盘的 entry 顺序追加（达到阈值后切新文件）；
- **Ledger Cache**：缓存最近访问的 ledger 数据（写读加速）；
- **Index**：内存 + 持久化（LDB 或 RocksDB）；
- **副本机制**：Ledger 写入时复制到 ensemble（默认 3 个 bookie）；
- **优势**：
  - 一致性 hash 散列 entry 到 bookie；
  - Ledger 是 atomic unit（写入完成即一致）；
  - Bookie 故障不影响其他 bookie 上的 ledger；

### 7.3 消息格式

Pulsar 的消息格式（`pulsar-client-api/src/main/proto/PulsarApi.proto`）：

```
┌──────────────────────────────────────────────────────────┐
│ Magic Number (0x0e01)                                     │
│ Total Length                                              │
│ Command Type / Checksum / MessageID                     │
│ Single Message Batch Metadata (if batch)                │
│ Single Message [replication] [Checking] [Key] [OrderKey]│
│                 [Properties] [Payload]                    │
└──────────────────────────────────────────────────────────┘
```

- **支持 4 种订阅模式**：
  - **Exclusive**：单消费者独占；
  - **Shared**：负载均衡（每条消息只被一个消费者处理）；
  - **Failover**：多消费者但只有 1 个 active（备份）；
  - **Key_Shared**：按 key 哈希分配（同一 key 到同一消费者）；

## 八、索引机制（流类型专项展开）

### 8.1 Log 结构（Append-only + Ledger 滚动 + Index）

- **Append-only Ledger**：BookKeeper ledger 是顺序追加 entry，不可修改；
- **Ledger 滚动**：`ManagedLedgerConfig` 控制（默认 `maxEntriesPerLedger=5000`、`maxSizePerLedgerMB=100`）；
- **Entry Index**：
  - 每个 ledger 在 bookie 上有 `.idx` 文件，记录 entryId → file offset；
  - 内存中维护 last entry 索引（LDB / RocksDB）；
  - 支持二分查找（按 entryId 定位）；
- **Time Index**：Pulsar broker 维护时间索引（基于 broker 接收时间，用于按时间回溯）；
- **Subscription Cursor**：每个订阅维护一个 cursor（独立于其他订阅），记录消费位置；

### 8.2 分区模型（Topic Bundle / 分片 vs 分区）

```
Topic (my-topic, 16 partitions)
   ├── Topic-Partition-0  → ManagedLedger-0 → Ledger 0, 1, 2, ...
   ├── Topic-Partition-1  → ManagedLedger-1 → ...
   └── Topic-Partition-15 → ManagedLedger-15 → ...
```

- **Topic Partition**：与 Kafka 类似的概念（topic 内并行度单位）；
- **Bundle**：namespace 内 topic 的逻辑分组（一致性 hash 分配到 broker）；
  - 每个 bundle 包含多个 topic partitions，由一个 broker 负责；
  - 动态负载均衡（broker 启动或负载高时拆分 bundle）；
- **与 Kafka 对比**：
  | 概念 | Kafka | Pulsar |
  |------|-------|-------|
  | 并行度 | Partition | Topic Partition |
  | 分配策略 | Hash 路由到 broker | Bundle 一致性 hash + Broker 负载均衡 |
  | 存储粒度 | Partition log（broker 本地） | Ledger（bookie 三副本） |
  | 扩容数据迁移 | 重平衡 partition | 仅调整 bundle 分配，ledger 不动 |

### 8.3 副本与一致性（BookKeeper Ensemble / Ack Quorum / Write Quorum）

```
        Producer (Send)
            │
            ▼
   ┌─── Broker (Topic A) ───┐
   │  ManagedLedger 创建 ledger │
   │  Ensemble = [B1, B2, B3] │
   │  Write Quorum (Qw) = 3   │
   │  Ack Quorum (Qa) = 2     │
   └──────┬──────┬──────┬────┘
          ▼      ▼      ▼
   ┌─ Bookie B1 ┐ ┌─ Bookie B2 ┐ ┌─ Bookie B3 ┐
   │   Journal  │ │   Journal   │ │   Journal  │
   │   Entries  │ │   Entries   │ │   Entries  │
   └─────────────┘ └─────────────┘ └─────────────┘
```

- **Ensemble (E)**：ledger 的 bookie 集合（如 [B1, B2, B3]）；
- **Write Quorum (Qw)**：每条 entry 写入的 bookie 数（默认 3）；
- **Ack Quorum (Qa)**：broker 等待 Qa 个 bookie ack 才返回（默认 2）；
- **配置**：`E=3, Qw=3, Qa=2` 是 Pulsar 默认（容忍 1 bookie 故障）；
- **与 Kafka ISR 对比**：
  - Kafka ISR 概念：与 leader 同步的副本集合；
  - Pulsar BookKeeper：每个 ledger 独立指定 ensemble（首次写入后基本固定）；
  - Pulsar 不依赖 ISR 选举 leader（无 leader 概念）；
- **Erasure Coding（ZK-based）**：可选节省存储（类似 Reed-Solomon），但实现复杂且默认未启用；

### 8.4 偏移量管理（Subscription Cursor / Position / Ack）

- **Subscription Cursor**（`ManagedCursorImpl.java`）：
  - 每个 subscription（消费者组）独立维护 cursor；
  - 记录消费到的 Position（`PositionImpl(ledgerId, entryId)`）；
  - 持久化到 BookKeeper（与 ledger 分开）；
- **Ack 模式**：
  - **Individual Ack**：消费者逐条 ack；
  - **Cumulative Ack**：消费者一次 ack 多条；
  - **Key Shared 模式**：按 key 哈希路由消息到同一 consumer，cursor 跨 partition；
- **Backlog 统计**：cursor 到 ledger end 之间的未读 entry；
- **Backlog Quota**：broker 强制 backlog 上限（`backlogQuotaMap` 配置）；

### 8.5 存储分层（Tiered Storage + Offload）

`pulsar/tiered-storage/`：

- **Offload**：active ledger 关闭后异步上传到 Tiered Storage（默认 S3 / GCS / Azure Blob）；
- **Ledger Offload 流程**：
  1. Broker 关闭 ledger（达到阈值）；
  3. Offloader Worker 读取 ledger 条目；
  3. 序列化 entry + 压缩（可选 ZSTD/LZ4）；
  4. 上传到对象存储（带元数据索引）；
  5. 删除本地 bookie ledger（节省磁盘）；
- **读路径**：consumer 请求远端 ledger → broker 从 Tiered Storage 下载 → 返回；
- **配置**：
  ```properties
  managedLedgerOffloadDriver=S3
  managedLedgerOffloadBucket=pulsar-offload
  managedLedgerOffloadThresholdSize=10M
  ```
- **优势**：
  - 长期 retention 成本降低（对象存储 ¥0.023/GB/月 vs bookie NVMe ¥3/TB/月）；
  - bookie 磁盘占用稳定；
  - 历史回溯任意长时间；

## 九、查询/计算/接口层

### 9.1 生产/消费 API

- **PulsarClient**（`pulsar-client/src/main/java/org/apache/pulsar/client/`）：
  - **Producer**：异步发送、回调、批量、压缩、路由（Hash/RoundRobin/Custom）；
  - **Consumer**：订阅、ack、nack、seek、pause/resume；
  - **Reader**：单条消息级别读取（类似 Kafka Reader API）；
  - **Schema**：内置 Avro / Protobuf / JSON Schema / String / KeyValue；
- **多协议支持**：
  - **Pulsar Binary Protocol**（默认）：自定义二进制协议；
  - **Kafka Protocol**（KOP，3.0 GA）：Kafka 客户端无需改代码；
  - **MQTT**（PulsarMQTT 适配器）；
  - **AMQP**（部分支持）；
  - **WebSocket**：浏览器订阅；
- **Reader API**：精确读取（从某个位置开始读，无需 commit）；

### 9.2 消息语义（at-most-once / at-least-once / exactly-once）

- **at-most-once**：Producer 同步发送不重试 + Consumer 不 ack；
- **at-least-once**：Producer 异步重试 + Consumer ack；
- **exactly-once**（Pulsar 2.5+）：
  - **幂等 Producer**：基于 `producerName` + sequence ID；
  - **事务**：
    - API：`pulsarClient.newTransaction()` + `producer.newMessage().sendAsync()`；
    - 实现：bookie catalog（`__transaction_coordinator`）+ 2PC 协议；
    - 与 Kafka 事务类似，跨多 topic + offset commit 原子；
- **EOS 限制**：
  - 仅支持 partitioned topic；
  - transaction timeout 默认 60s；
  - 事务性能略低于非事务（~20%）；

### 9.3 流处理引擎（Pulsar Functions）

`pulsar-functions/`：

- **Pulsar Functions**：轻量级流处理（嵌入 broker runtime）；
- **三种部署模式**：
  - **Local Mode**：单进程运行（开发）；
  - **Cluster Mode**：broker 内的 worker 调度（生产）；
  - **Serverless Mode**（3.x 新）：无状态函数 + 自动扩缩；
- **支持语言**：Java / Python / Go（通过 Function Worker 容器化）；
- **示例**：
  ```java
  public class ExclamationFunction implements Function<String, String> {
      @Override
      public String apply(String input) { return input + "!"; }
  }
  ```
- **Source / Sink / Function 三层 API**：
  - **Source**：从外部（DB / HTTP / file）拉取 → 输出 topic；
  - **Function**：从输入 topic 处理 → 输出 topic；
  - **Sink**：从输入 topic 消费 → 写入外部；
- **Pulsar IO**：内置 30+ Source/Sink Connector（JDBC、Elasticsearch、Iceberg、Kafka 等）；
- **状态管理**：
  - **StateStore**：基于 BookKeeper 的 KV 存储；
  - **Windowing**：滚动 / 滑动 / 会话窗口；
- **对比 Kafka Streams**：
  | 维度 | Kafka Streams | Pulsar Functions |
  |------|---------------|------------------|
  | 部署 | 嵌入式 Java 库 | 独立 runtime（broker 内/独立） |
  | 状态存储 | RocksDB | BookKeeper StateStore |
  | 多语言 | Java / Scala | Java / Python / Go |
  | 自动扩缩 | 无（依赖应用进程数） | Serverless 模式支持 |

### 9.4 Schema 演进（内置 Schema Registry）

- **Pulsar Schema Registry**（**内置**，无需单独部署）：
  - 每个 namespace 自动启用；
  - 持久化在 `__transaction_coordinator` 或独立 topic（`persistent://<tenant>/<namespace>/__schema_registry`）；
  - HTTP API：GET/PUT/DELETE schema；
- **Schema 类型**：String / JSON / Avro / Protobuf / KeyValue；
- **兼容性策略**：
  - **BACKWARD**：新 schema 可读旧数据；
  - **FORWARD**：旧 schema 可读新数据；
  - **FULL**：双向兼容；
  - **NONE**：不检查；
- **演进**：通过 `Schema.define()` 编码 + `Producer<MyClass>` 反序列化自动校验；

## 十一、复制与高可用

### 11.1 集群内复制

- BookKeeper 已实现（见 8.3 节）；
- Broker 高可用：ZK 自动 leader 选举 + bundle 重分配；

### 11.2 跨集群复制（Geo-Replication）

- **内置 Geo-Replication**：
  - 多 cluster 配置（cluster-id、cluster-url、web-service-url）；
  - 配置 namespace 的 replication clusters 列表；
  - broker 自动同步消息到其他 cluster；
- **实现**：
  - `pulsar-broker/src/main/java/org/apache/pulsar/broker/service/AbstractReplicator.java`；
  - 内置复制策略（按 topic/namespace 维度）；
  - 异步复制（最终一致）；
- **使用场景**：跨地域容灾、就近访问；
- **限制**：replication 是单方向的（多向需 StreamNative Enterprise）；

### 11.3 灾难恢复

- **备份**：
  - topic metadata 备份（`bin/pulsar-admin topics backup`）；
  - BookKeeper 自动管理（无需 backup）；
  - Tiered Storage 天然异地持久化；
- **恢复**：
  - `pulsar-admin topics restore <topic>`；
  - 跨集群恢复通过 Geo-Replication 异步同步；

## 十二、扩展性

- **计算层扩展**：加 broker → 自动负载均衡（bundle 重分配）；
- **存储层扩展**：加 bookie → 新 entry 一致性 hash 分配到新 bookie；
- **协调层**：ZK 集群（3-5 节点）或 etcd（3.x 可选）；
- **Tiered Storage**：独立扩展（加对象存储容量即可）；
- **集群联邦**：跨集群 Geo-Replication（无限扩展）；
- **限制**：
  - 单 bookie 建议 < 10TB 容量（受索引计算开销影响）；
  - 单 topic partition 推荐 < 50 MB/s（避免 bookie 热）；
  - ZK 集群通常 < 100 节点；

## 十三、性能与基准

- **OpenMessaging Benchmark**：
  - Pulsar 在 partition 数百以下时，吞吐与 Kafka 相当；
  - 大集群（Pulsar 集群 > 100 broker）吞吐优势明显；
  - p99 延迟 Pulsar 略高于 Kafka（多一跳 bookie）；
- **StreamNative 官方基准**：
  - 100 个 topic + 100 万 producer → 千万级消息/秒；
  - BookKeeper 端到端延迟 p99 ~ 5ms；
- **优化要点**：
  - **bookie 数量**：>= 3（容忍 1 故障）；
  - **journal 磁盘**：专用 SSD（避免与 entry log 竞争）；
  - **Tiered Storage offload threshold**：不要太小（避免频繁 offload）；

## 十四、运维与可观测性

- **指标导出**：Prometheus（默认端口 8080）；
- **关键指标**：
  - `pulsar_broker_publish_rate_limit`（限流）；
  - `pulsar_broker_storage_size`（broker 磁盘占用）；
  - `pulsar_bookie_journal_queue_size`（journal 积压）；
  - `pulsar_broker_subscription_back_log`（消费积压）；
- **日志**：Log4j2（broker）+ SLF4J（client）；
- **管理工具**：
  - `pulsar-admin`（CLI 工具，主题/命名空间/集群管理）；
  - `pulsar-client`（CLI 生产/消费）；
  - **Pulsar Manager**（Web UI，由 StreamNative 维护）；
- **健康检查**：`bin/pulsar-perf` / `bin/pulsar-test-client`；
- **备份恢复**：Tiered Storage 自动异地持久化；

## 十五、生态与集成

- **客户端 SDK**：
  - **官方**：Java（核心）、Go（`pulsar-client-go`）、Python（`pulsar-client-python`）、C++（`pulsar-client-cpp`）、Node.js；
  - **社区**：Rust（`pulsar-rs`）、.NET、WebSocket、Function Mesh；
- **CDC 集成**：Debezium Pulsar Connector（替代 Kafka 模式）；
- **Schema 管理**：内置 Schema Registry；
- **流处理引擎**：Pulsar Functions（官方）、Apache Flink（Pulsar Source/Sink）、Apache Spark（Pulsar Adapter）、Apache Pinot（实时 OLAP）；
- **数据湖**：Pulsar Iceberg Sink / Delta Lake Sink（StreamNative 提供）；
- **消息队列协议**：Pulsar Binary、Kafka 协议（KOP）、MQTT、AMQP（部分）、WebSocket；
- **Kubernetes**：
  - **Pulsar Operator**（Apache）；
  - **StreamNative Helm Chart**（生产级）；
  - **KOP Helm Chart**（Kafka 兼容模式）；
- **多云**：AWS（Amazon MQ for Apache Pulsar）、GCP（Pub/Sub Lite）、Azure（部分兼容）；

## 十六、核心论文与算法原理

- **Pulsar 原始论文**："Pulsar: Distributed Pub-Sub Messaging System"（Yahoo Research 2015）；
- **BookKeeper 论文**："BookKeeper: Scalable and Fault Tolerant Logging"（Yahoo Research 2008）；
- **RAFT 论文**（Ongaro）：Pulsar broker 选举（2.10+ 可选 RAFT，避免 ZK）；
- **Geo-Replication 设计**：Yahoo 2018 论文 "Pulsar: Scalable Geo-Replication";
- **Apache HTrace 集成**：分布式 tracing；
- **Consistent Hashing**：bundle 分配策略；
- **Quorum 协议**：BookKeeper Write/Ack Quorum（类似 Paxos 派生）；

## 十七、源码学习路径

### 17.1 必读模块清单（按入门到进阶）

| 阶段 | 模块 | 路径 | 说明 |
|------|------|------|------|
| 入门 | 启动 | `pulsar-broker/src/main/java/org/apache/pulsar/broker/PulsarService.java` | 启动流程 |
| 入门 | Broker 服务 | `pulsar-broker/src/main/java/org/apache/pulsar/broker/service/BrokerService.java` | Topic 管理 |
| 入门 | Topic 与订阅 | `pulsar-broker/src/main/java/org/apache/pulsar/broker/service/PersistentTopic.java`、`PersistentSubscription.java` | topic 生命周期 |
| 进阶 | ManagedLedger | `managed-ledger/src/main/java/org/apache/bookkeeper/mledger/impl/ManagedLedgerImpl.java` | ledger 管理 |
| 进阶 | BookKeeper 客户端 | `pulsar-broker/src/main/java/org/apache/pulsar/broker/BookKeeperClientFactoryImpl.java` | bookie 交互 |
| 进阶 | 事务协调 | `pulsar-broker/src/main/java/org/apache/pulsar/broker/transaction/` | EOS 实现 |
| 高级 | Functions | `pulsar-functions/runtime/src/main/java/org/apache/pulsar/functions/runtime/` | 函数调度 |
| 高级 | Tiered Storage | `tiered-storage/jcloud/src/main/java/org/apache/pulsar/managedledger/offload/jcloud/` | Offload 实现 |

### 17.2 推荐阅读顺序

1. **第 1 周**：Pulsar 官方文档 + `PulsarService.java` 启动流程；
2. **第 2 周**：Broker Service + Topic 生命周期；
3. **第 3 周**：ManagedLedger + BookKeeper 交互；
4. **第 4 周**：BookKeeper bookie 源码（外部项目）；
5. **第 5 周**：Functions 框架与 StateStore；
6. **第 6 周**：Tiered Storage 与 Geo-Replication；

## 十八、源码规模与开发语言

- **规模**：Pulsar 3.x 约 **150 万行**（Java 占 80%+）；
- **核心子模块**：
  - `pulsar-broker/`：约 40 万行；
  - `managed-ledger/`：约 10 万行；
  - `pulsar-functions/`：约 15 万行；
  - `external/bookkeeper/`：约 50 万行（独立 Apache 项目）；
- **语言占比**：Java ~80%、C++ ~10%（Client）、Python / Go / Rust ~10%；
- **构建系统**：Gradle（`build.gradle.kts`）；
- **JDK**：11（3.x），17（未来）；
- **测试**：TestNG + JUnit + 集成测试集群（`tests/`）；
- **CI**：GitHub Actions + Apache Jenkins；

## 十九、学习资源

- **官方文档**：
  - [Apache Pulsar Documentation](https://pulsar.apache.org/docs/)
  - [Pulsar Architecture Overview](https://pulsar.apache.org/docs/concepts-architecture-overview/)
- **书籍**：
  - "Apache Pulsar in Action"（Manning，2021）
  - O'Reilly 短篇 "Apache Pulsar: 概念与实践"（中文）；
- **视频课程**：StreamNative 培训、Pulsar Summit 会议（YouTube）；
- **中文资源**：GitHub `apachecn/pulsar-doc-zh`、StreamNative 中文博客；
- **社区**：[Pulsar Slack](https://apache-pulsar.slack.com/)、[Reddit r/apachepulsar](https://reddit.com/r/apachepulsar)、Pulsar Summit 大会；
- **微信公众号**：「StreamNative」「Pulsar Community」等；

## 二十、可移植性与部署

- **平台支持**：Linux（生产，x86_64 / ARM64）、macOS / WSL2（开发）、Windows（不推荐生产）；
- **容器化**：官方 Docker 镜像 `apachepulsar/pulsar`；
- **Kubernetes**：
  - **Pulsar Operator**（Apache）；
  - **StreamNative Helm Chart**（生产推荐）；
  - **KOP Helm Chart**（Kafka 兼容模式）；
- **混合云**：跨 cluster Geo-Replication；
- **裸机**：Compose multi-host cluster（`bin/pulsar` standalone / cluster）；
- **JVM 调优**：G1GC 推荐，避免 CMS（暂停时间长）；
- **BookKeeper 硬件**：
  - Journal：专用 NVMe（顺序写 100K+ IOPS）；
  - Entry Log：HDD 也可（顺序写）；
  - Index：内存足够（默认 RocksDB）；

## 二十一、变体与衍生项目

- **StreamNative Cloud**：托管 Pulsar 服务；
- **StreamNative Platform**：商业版（多集群联邦、Auto-balancer、企业安全）；
- **Apache RocketMQ 5.x**（阿里）：受 Pulsar 启发的国产消息队列，计算存储分离架构；
- **Amazon MQ for Apache Pulsar**：AWS 托管；
- **AutoMQ**：受 Pulsar 启发的 S3 原生 Kafka 替代；
- **对比项目**：
  - **Apache Kafka**：本 wiki kafka.md；
  - **Redpanda**：本 wiki redpanda.md；
  - **NATS JetStream**：本 wiki nats.md（轻量云原生边缘）；

## 二十二、风险与陷阱

### 22.1 常见误用

- **ZooKeeper 误用**：ZK 不能停机（broker leader 选举依赖）；
- **Tenant 隔离失效**：多租户共享 namespace 导致资源争抢；
- **Schema 不兼容**：未启用兼容性检查导致下游消费失败；
- **Backlog 无限制**：未配置 `backlogQuotaMap` 导致磁盘爆满；
- **Bookie 数量过少**：仅 1 bookie → 无副本保护；
- **Geo-Replication 循环**：集群 A→B→A 复制导致消息循环；

### 22.2 性能陷阱

- **Bookie Journal 慢**：journal 写入是写瓶颈，建议专用 NVMe；
- **Bundle 数量过少**：broker 负载不均（部分 broker 闲置）；
- **Bundle 数量过多**：ZK 元数据管理压力（> 500/namespace）；
- **Topic partition 数过多**：受 broker 调度开销影响；
- **Subscription 模式错选**：Shared 模式应避免顺序依赖场景；
- **Tiered Storage offload 频率过高**：S3 API 调用成本；

### 22.3 迁移痛点

- **从 Kafka 迁移**：
  - KOP 兼容模式客户端可平滑迁移；
  - 但 BookKeeper 数据不能直接读 Kafka（需重写数据）；
- **从 Pulsar 迁移到 Kafka**：
  - 数据格式不兼容；
  - 多租户结构需重新设计；
- **升级到 3.x**：从 ZK 升级到可选 etcd 需谨慎测试；

### 22.4 运维雷区

- **Bookie 磁盘满**：topic 无法写入（broker 限制）；
- **Bookie 恢复**：新增 bookie 后，BookKeeper 自动恢复旧 ledger，需监控 `available_bookies`；
- **ZooKeeper session 超时**：网络抖动导致 broker 频繁离组；
- **BookKeeper Journal 损坏**：journal 损坏可能丢数据；
- **Tiered Storage 凭据过期**：S3 凭据错误导致 offload 失败；
- **Functions 资源限制**：未配置 CPU/内存 → OOM；

## 二十三、与本项目的关联

- **可借鉴的设计**：
  - **计算与存储分离**：`engineering/src/db/` 当前的存储与查询耦合，可借鉴分层；
  - **Multi-Tenant 模型**：本项目目前无多租户，可参考 Pulsar 的 tenant/namespace 抽象；
  - **Segment 滚动存储**：与本项目 `engineering/src/db/core/wal.c` 的 WAL segment 概念类似；
  - **Pulsar Functions 轻量流处理**：可参考轻量级函数 runtime 集成到本项目 `learning/`；
- **不适合借鉴**：
  - **三层架构复杂依赖**：本项目单进程架构更适合；
  - **BookKeeper 独立项目**：本项目当前无分布式存储需求；
- **集成参考**：本项目如需消息中间件集成，可考虑 Pulsar（云原生）或 Kafka（生态广）；

## 二十四、参考资料

- [Apache Pulsar 官方主页](https://pulsar.apache.org/)
- [Apache Pulsar Documentation](https://pulsar.apache.org/docs/)
- [Pulsar Architecture](https://pulsar.apache.org/docs/concepts-architecture-overview/)
- [Apache BookKeeper](https://bookkeeper.apache.org/)（底层存储）
- [Pulsar GitHub](https://github.com/apache/pulsar)
- [Pulsar Concepts: Multi-tenancy](https://pulsar.apache.org/docs/concepts-multi-tenancy/)
- [Pulsar Tiered Storage](https://pulsar.apache.org/docs/concepts-tiered-storage/)
- [Pulsar Functions](https://pulsar.apache.org/docs/functions-overview/)
- [Pulsar Kafka Compatibility (KOP)](https://github.com/streamnative/kop)
- [StreamNative](https://streamnative.io/)（商业化公司）
- [Pulsar Summit](https://pulsar-summit.org/)（年度大会）
- [Apache Kafka 主页](https://kafka.apache.org/)（对比参考）
- [Redpanda 主页](https://redpanda.com/)（对比参考）
- [NATS 主页](https://nats.io/)（对比参考）
- [Yahoo Research: Pulsar Paper](https://research.yahoo.com/)