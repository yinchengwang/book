# NATS Wiki（云原生轻量级消息与流平台）

> 参考版本：NATS Server 2.10+（Go 1.22+，Apache 2.0 许可）。本 wiki 重点对比 Kafka / Pulsar 介绍 NATS 的轻量级架构、Subject-based Routing、JetStream（流存储）、RAFT-native 元数据共识、Leaf Node / Gateway 联邦。

## 一、项目定位

NATS 是由 **Derek Collison**（原 Cloud Foundry / Apcera 创始人）于 2014 年开源的 **云原生消息系统**，定位为「always-on, lightweight, secure messaging for cloud-native architectures」，提供三种模式：**Core NATS**（无状态 publish/subscribe）、**JetStream**（持久化流存储）、**Leaf Node / Gateway**（边缘节点与多集群联邦）。以**单二进制（nats-server）、极小内存占用（< 10MB）、Subject-based Routing（非 Topic）**为核心差异化特性，被誉为「消息中间件的 Unix 哲学」。

- **痛点**：
  - Kafka / Pulsar 运维复杂（JVM 调优、集群配置、ZK）；
  - 边缘计算（IoT、CDN、Mesh）需要低资源占用的消息系统；
  - 微服务间通信需要 HTTP 替代品；
  - 大规模 Fan-out（IoT 数百万设备）需要轻量级协议；
  - 多集群联邦比 MirrorMaker 简单；
- **典型用户**：Cloud Foundry（起源）、Salesforce、PingCAP、TigerGraph、MasterCard、Honda、Siemens、Splunk、Datadog、字节跳动（部分团队）、AMD、Tesla（车队遥测）；
- **核心口号**：**"Always On. Lightweight. Secure."** + **"The connective tissue for cloud-native"** + **"Simple, secure, scalable messaging"**。

## 二、发展历史与社区

- **起源**：2014 年由 Derek Collison 开发（来自 Cloud Foundry 的 NATS 项目，2014 年独立为 NATS.io），最初是 Cloud Foundry 内部组件；
- **里程碑版本**：
  - 2014：NATS 0.1 上市（Cloud Foundry 内部）；
  - 2016：NATS 0.9（生产可用）；
  - 2017：NATS 1.0、Synadia 公司成立（商业支持）；
  - 2018：NATS 2.0 + JetStream GA（持久化流存储）、RAFT 共识；
  - 2019：NATS 2.1 Leaf Node + Cluster Gateway（联邦）；
  - 2020：NATS 2.2 Streams Manager、Multi-tenancy；
  - 2021：NATS 2.6（性能改进、Streams 镜像）；
  - 2022：NATS 2.9 JetStream 性能优化；
  - 2023：NATS 2.10 KV/Object Store 优化、Leaf Node Quota；
  - 2024：NATS 2.11（Leaf Node TLS、WebSocket 改进）；
  - 2025：NATS 2.12（进行中）；
- **维护状态**：活跃，6 周 minor 节奏；
- **社区**：GitHub 16k+ star、Slack（最活跃社区）、NATS Summit 年度大会、NATS 认证专家；
- **生态**：Synadia（商业化公司，提供 NGS - NATS Global 托管）、NATS Helm Operator（NATS 官方）、NATS 微服务框架（micro框架）；

## 四、许可与商业限制

- **核心代码**：**Apache License 2.0**（宽松商用）；
- **生态关系**：
  - **Synadia** 是主要商业公司，提供 **NGS（NATS Global Services）** 托管服务（全球区域）、**Synadia Pro**（企业特性）；
  - **NATS 客户端多语言**：Go（核心）、Java、Python、Node.js、Ruby、Rust、C#、C/C++、Elixir；
  - **JetStream KV Store**：内置 KV 数据库（类似 Redis）；
  - **Object Store**：内置对象存储（类似 S3）；
  - **MQTT 桥**：NATS-MQTT Bridge（连接 IoT）；
  - **Kafka 桥**：nats-kafka（双向 Kafka 集成）；
  - **CloudEvents**：原生 CloudEvents 规范支持。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Salesforce**：CRM 微服务通信；
  - **PingCAP**：TiDB 内部通信（TiKV → TiDB）；
  - **MasterCard**：支付网络边缘通信；
  - **Honda / Toyota**：车联网遥测；
  - **Siemens**：工业 IoT；
  - **AMD**：分布式集群管理；
  - **Tesla**：自动驾驶车队遥测（百万设备）；
  - **Cloud Foundry**：PaaS 平台内部通信；
  - **TigerGraph**：图数据库分布式通信；
  - **字节跳动**：部分团队微服务；
- **典型场景**：
  - **微服务消息总线**：服务发现 + RPC 替代（`nats request/reply`）；
  - **IoT 边缘**：Leaf Node 部署在边缘网关，中心集群聚合；
  - **CDN 边缘同步**：NATS Leaf Node + Object Store；
  - **Fan-out 通知**：数百万设备广播事件；
  - **Event Mesh**：跨云 / 跨集群事件联邦；
  - **Service Mesh 补充**：Istio / Linkerd 内部消息（NATS 已集成）；

## 五、一句话对比定位

> NATS 是 **"单二进制 Go 进程 + Subject 路由（非 Topic）+ RAFT 共识 + JetStream（持久化 KV + Stream）+ Leaf Node 边缘 + Gateway 集群联邦 + Apache 2.0"** 的云原生轻量消息流平台；与 Kafka（生态最广 + 重）、Pulsar（云原生多租户 + 重）、Redis Streams（KV + 轻）、RabbitMQ（队列 + 老牌）构成消息生态。NATS 的差异化是 **"极致轻量 + 边缘友好 + 联邦原生 + 多模型（pub/sub + queue + KV + object）"**。

## 六、整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                        NATS Client（多语言 SDK）                       │
│   nats.go（Go 官方）/nats.java / nats.go (Python) / nats.c             │
│   ├─ Conn（连接到 Server + 自动重连）                                  │
│   ├─ Subscription（订阅 subject）                                      │
│   ├─ Request/Reply（一对一 RPC）                                       │
│   ├─ Queue Group（负载均衡订阅组）                                     │
│   └─ JetStream API（KV / Object / Stream）                            │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ NATS Protocol (TCP/TLS/WebSocket)
┌─────────────────────▼────────────────────────────────────────────────┐
│                          nats-server 进程                             │
│  server/server.go                                                     │
│  ├─ Server（主循环：Accept + Auth + Parse + Route）                     │
│  ├─ Accounts（多租户隔离：内存/CPU/连接数/操作）                        │
│  ├─ Route（cluster 内 peer 路由，gossip 协议）                          │
│  ├─ LeafNode（边缘节点，单向/双向连接到 hub）                           │
│  ├─ Gateway（跨集群同步 + 远程 subject 路由）                           │
│  ├─ Client（TCP/TLS/WebSocket 客户端管理）                             │
│  └─ Subscription（subject 路由表 + queue group 负载均衡）               │
├──────────────────────────────────────────────────────────────────────┤
│                    JetStream 持久化层（可选）                          │
│  server/jetstream.go                                                  │
│  ├─ Stream（持久化消息流，可 ACK/replay）                              │
│  ├─ Consumer（消费位置 + ack + 过滤）                                  │
│  ├─ KV Store（基于 Stream 的 KV 存储）                                 │
│  ├─ Object Store（基于 Stream 的对象存储）                             │
│  └─ Meta Cluster（RAFT 集群元数据）                                    │
├─────────────────────┬────────────────────────────────────────────────┤
│                    Stream 存储实现                                      │
│  server/filestore.go（默认文件存储）                                   │
│  ├─ Block（512KB-4MB 数据块）                                          │
│  ├─ Index（每条消息 offset 索引）                                      │
│  ├─ Compaction（基于 Stream 的 KV compaction）                          │
│  └─ Snapshot（快照，用于 HA 与恢复）                                    │
├─────────────────────┬────────────────────────────────────────────────┤
│                    RAFT 共识层（自研）                                  │
│  server/raft.go                                                       │
│  ├─ Node（RAFT state machine）                                         │
│  ├─ Group（meta group / stream group）                                │
│  ├─ Log（RAFT log entry + snapshot）                                  │
│  └─ Transport（gossip + RPC）                                          │
└──────────────────────────────────────────────────────────────────────┘
```

### 6.1 进程与协议模型

- **单进程多 goroutine**：
  - Accept Loop：接收新连接；
  - 每个 Client 一个读 goroutine（解析 NATS 协议）；
  - 共享的 readloop 写 pool（处理 send queue）；
  - Router 协程：subject 路由表分发消息；
- **NATS 协议**：基于文本/二进制的轻量协议（10+ 年未变），消息头最少 8 字节；
  - 常用命令：`PUB`, `SUB`, `UNSUB`, `MSG`, `HMSG`, `INFO`, `PING`, `PONG`, `+OK`, `-ERR`；
- **单二进制部署**：`nats-server` 一个文件即可运行（含 server + JetStream + Leaf + Gateway）；

### 6.2 Subject-based Routing（与 Topic 区别）

```
NATS Subject 层级：
  orders.us.east.created.   ← 点分隔层级（支持通配符）
  orders.*.east.created.     ← * 匹配一段
  orders.>.                  ← > 匹配多段
  orders.us.*.>             ← 混合
```

- **通配符订阅**：
  - `*`：匹配单段（如 `orders.*.created` 匹配 `orders.us.created`、`orders.cn.created`，不匹配 `orders.us.east.created`）；
  - `>`：匹配多段（必须最后）；
- **路由优势**：
  - 无须预先创建 subject（无 schema 概念）；
  - 订阅者与发布者解耦（发布者只发，订阅者只听）；
  - 大量通配符订阅高效（subject trie 树）；

## 七、存储引擎（Core NATS vs JetStream）

### 7.1 Core NATS（无持久化）

- **内存路由**：`server/sublist.go`（subject trie 树）；
- **无盘存储**：消息不进磁盘（除非启用 JetStream）；
- **Fan-out 队列**：Queue Group 内存负载均衡；
- **Fire-and-forget**：默认 at-most-once（`PUB` 命令）；

### 7.2 JetStream（持久化流存储）

JetStream 是 NATS 的**持久化层**（R1.x 起），基于 **Stream（流）** 与 **Consumer（消费者）** 模型。

#### 7.2.1 物理文件结构（FileStore 默认）

```
jetstream/
├── streams/
│   └── <account-hash>/
│       └── <stream-name>/
│           ├── meta.inf      # Stream 元数据
│           ├── snapshot      # 快照（周期生成）
│           ├── 1.blk          # Block 1（512KB-4MB）
│           ├── 1.idx          # Block 1 索引
│           ├── 2.blk          # Block 2
│           ├── 2.idx
│           └── ...
└── ...
```

- **Block 文件**：每条消息按写入顺序追加到 block，达到阈值后创建新 block；
- **Index**：记录每条消息在 block 中的 offset（用于快速定位）；
- **Snapshot**：周期生成（默认 64MB 或 1 分钟），用于快速重启；

#### 7.2.2 消息格式

- **消息头**（`nats.Header`）：键值对（`Nats-Msg-Subject` 等）；
- **消息体**：任意二进制；
- **Batch**：NATS 支持 Message Batching（一次发送多条）；

#### 7.2.3 刷盘与保留

- **刷盘策略**：
  - 内存队列 → 当达到 `max_block_size` 时 fsync；
  - 也可配置 `sync_interval` 强制刷盘；
- **保留策略**：
  - **Limits**：达到大小/时间/消息数限制后丢弃（默认）；
  - **Interest**：所有 consumer 都消费后丢弃；
  - **WorkQueue**：只允许一个 consumer 接收（单消费者）；
  - **Compact**：保留每个 key 最新消息（KV/compact 模式）；

## 八、索引机制（流类型专项展开）

### 8.1 Stream 结构（Append-only Block + Index + Snapshot）

- **Block**：连续追加的 N 条消息，达到 `max_block_size`（512KB-4MB 默认 2MB）后切新 block；
- **Index**：每条消息的 `seq → block_offset` 映射（`server/filestore.go`）；
- **Snapshot**：周期性全量快照（meta + seq 状态）；
- **恢复流程**：
  1. 加载 snapshot；
  2. replay snapshot 之后的 block；
  3. 重建内存索引；

### 8.2 Subject 路由（不是 Partition）

NATS 没有 Kafka 的 partition 概念，但有类似的并行度：

- **Stream 内部按 subject 分发**（一个 Stream 可绑定多个 subject pattern）；
- **Consumer group**：NATS Queue Group（与 Kafka consumer group 类似，按 hash 分配到一组 subscriber）；
- **Replicas**（副本）：
  - **R = 1**：无副本（开发用）；
  - **R = 3**：3 副本（容忍 1 故障，生产默认）；
  - **R = 5**：5 副本（容忍 2 故障）；
- **Placement**：可指定副本分布在哪些节点（如机房拓扑）；

### 8.3 副本与一致性（RAFT for Meta + RAFT for Streams）

```
        Publisher
            │
            ▼
   ┌─── Leader (Stream "ORDERS", R=3) ───┐
   │  RAFT Group "ORDERS"               │
   │  writes to local filestore         │
   └──────┬────────────────┬─────────────┘
          │ replicate      │
          ▼               ▼
   ┌─ Follower 1 ─┐ ┌─ Follower 2 ─┐
   │  RAFT replica │ │  RAFT replica │
   │  Filestore    │ │  Filestore    │
   └───────────────┘ └───────────────┘
```

- **两层 RAFT**：
  - **Meta RAFT Group**：管理集群元数据（accounts、streams 配置）；
  - **Stream RAFT Group**：每个 Stream 一个 RAFT Group（独立副本）；
- **RAFT 自研**（`server/raft.go`）：
  - 与 etcd-raft 类似的设计；
  - **Leader Election**：基于 RAFT term + 随机超时；
  - **Snapshot**：每个 Stream 独立 snapshot；
  - **Catchup**：落后 follower 通过 RAFT snapshot + log catchup 同步；
- **副本副本数**：
  - `R=1`：开发用；
  - `R=3`：生产默认（容忍 1 故障）；
  - **R 必须为奇数**（避免脑裂）；
- **一致性保证**：
  - 写入必须 RAFT 多数派 ack；
  - Consumer 可读取 Linearizable（leader）或 Lease（任意副本）；

### 8.4 偏移量管理（Consumer + Ack + Sequence）

- **Consumer**（`server/consumer.go`）：
  - 每个 Consumer 独立维护 `consumer_seq`（消费位置）；
  - **Ack Mode**：
    - **Explicit Ack**：消费者必须手动 ack（`jsMsg.Ack()`）；
    - **No Ack**：不持久化消费位置；
    - **All / None Ack**：根据 ack 行为区分；
  - **Replay Policy**：
    - **Instant**：从当前序列开始（默认）；
    - **Original**：从消息原始时间开始；
    - **Sequence**：从指定 seq 开始；
    - **Time**：从指定时间开始；
- **Persistent Consumer**：消费位置存进 RAFT log，consumer 重启可恢复；
- **Max Ack Pending**：限制每条 consumer 的未 ack 消息数（背压）；

### 8.5 存储分层（NATS Stream + KV Store + Object Store）

- **NATS Stream**：持久化消息流（标准流模式）；
- **KV Store**（`server/jetstream_api.go`）：
  - 基于 Stream 的 KV 存储；
  - **Watch**：订阅 key 变更事件；
  - **TTL**：支持 key 过期；
  - **History**：每个 key 保留 N 个历史版本（默认 1）；
- **Object Store**（`server/jetstream_api.go`）：
  - 基于 Stream 的大对象存储（类似 S3）；
  - **分块**：大文件自动分块（512KB-4MB）；
  - **Watch**：对象变更事件；
  - **Link**：跨账户/集群对象链接；

### 8.6 联邦架构（Leaf Node + Gateway + Cluster）

- **Cluster（`cluster/`）**：节点间 gossip + RAFT 元数据；
- **Gateway（`server/gateway.go`）**：跨集群同步 + subject 路由；
- **Leaf Node（`server/leafnode.go`）**：
  - 边缘节点（轻量客户端，仅订阅 / 转发）；
  - 适合 IoT、CDN、车联网；
  - **Leaf Node Quota**：限制每个 leaf 的资源使用；
- **Account**：多租户隔离（subject + connection + memory 限制）；

## 九、查询/计算/接口层

### 9.1 生产/消费 API

- **Core NATS 客户端**（`nats.go`）：
  - `nc.Publish(subject, data)`：发布；
  - `nc.Subscribe(subject, handler)`：订阅；
  - `nc.Request(subject, data, timeout)`：RPC（等待响应）；
  - `nc.QueueSubscribe(subject, queue, handler)`：Queue Group 负载均衡；
- **JetStream API**（`nats.go/jetstream`）：
  - `js.Publish(subject, msg)`：流发布；
  - `js.PullSubscribe(name, "ORDERS")`：拉取消费；
  - `js.Subscribe(name, handler)`：回调消费；
  - `js.StreamInfo(name)`：流元信息；
  - `js.ConsumerInfo()`：消费者信息；
- **多语言 SDK**（`nats.go`、`nats.java`、`nats.go` (Python)、`nats.c`、`nats.rs` 等）；
- **Wire Protocol**：基于 TCP / TLS / HTTP/2（WebSocket）的轻量协议；HTTP/3 实验中；

### 9.2 消息语义（at-most-once / at-least-once / exactly-once）

- **at-most-once**：Core NATS 默认（`PUB`）；
- **at-least-once**：JetStream 默认（消费者 ack）；
- **exactly-once**（JetStream 2.10+）：
  - **幂等 Publisher**：`js.Publish()` 通过 dedup window 去重；
  - **Deduplication Window**：基于消息 ID 去重窗口；
  - **源 subject 校验**：通过 message header 校验；
  - **限制**：需客户端主动使用 idempotent API；
- **消息事务**：JetStream 支持 `js.PublishAsync()` + ack 等待，类似 Kafka EOS；

### 9.3 流处理引擎（JetStream + Stream Processing）

- **JetStream + Services 模式**：
  - **微服务响应式**：`nc.Subscribe()` + 内部处理 + 发布结果；
  - **无内置流处理 DSL**：与 Kafka Streams、Pulsar Functions 不同；
- **外部流处理引擎**：
  - **Apache Flink**：NATS Source/Sink（Flink Connector）；
  - **Materialize**：NATS Source；
  - **Spark Streaming**：NATS Adapter；
- **NATS 微服务框架（micro）**：
  - 服务注册 / 请求-响应 / 监控；
  - 类似 RPC 框架（gRPC 替代）；
- **对比**：
  | 引擎 | 部署 | 优势 |
  |------|------|------|
  | NATS micro | 嵌入应用 | 极轻量、零依赖 |
  | Pulsar Functions | 嵌入 broker | Serverless |
  | Kafka Streams | 嵌入应用 | 复杂 DSL |
  | Apache Flink | 独立集群 | 大规模 |

### 9.4 Schema 演进（轻量级 / JSON Schema）

- **NATS Schema Registry**（社区项目，非官方）：
  - 基于 NATS KV 的 Schema 存储；
  - 客户端主动拉取；
- **官方推荐**：
  - **消息头 + Protobuf / Avro**：在 NATS 消息中嵌入 schema ID；
  - **CloudEvents**：原生支持 CloudEvents 规范；
  - **Subject 类型**：通过 subject 命名约定；
- **兼容性**：依赖应用层（无官方强制）；

## 十一、复制与高可用

- **Stream 副本**：R 参数（默认 3，容忍 1 故障）；
- **Cluster**：3 节点起（RAFT 多数派）；
- **跨集群 Gateway**：跨数据中心异步同步；
- **Leaf Node HA**：Leaf 自动重连到 hub（多 hub 负载均衡）；
- **Snapshot + Restore**：
  - `nats stream snapshot` / `nats stream restore`；
  - 用于灾备与跨集群迁移；

## 十二、扩展性

- **水平扩展**：加 server 节点 → cluster 自动发现 + RAFT 自动重平衡；
- **JetStream Stream 拆分**：将不同业务拆为多个 Stream（独立扩展）；
- **联邦**：Gateway（跨集群同步）+ Leaf Node（边缘聚合）；
- **限制**：
  - 单集群节点数推荐 < 50（超过性能下降）；
  - 单 Stream 副本数（建议 3-5）；
  - 单 Node 连接数（建议 < 50K，因文件描述符限制）；
  - Leaf Node 与 Hub 之间受网络延迟影响；

## 十三、性能与基准

- **NATS 官方基准**：
  - **单节点 Core NATS**：~10M msg/sec（小消息 64B）；
  - **JetStream**：~200K-500K msg/sec（持久化路径）；
  - **延迟**：< 1ms（同数据中心）；
- **CNCF TAG 报告**：
  - NATS 是最快消息系统之一；
  - 与 Kafka 对比：NATS 资源占用 < 10%，但功能不及 Kafka；
- **优化要点**：
  - **Subject 路由**：避免太深 subject（影响 trie 性能）；
  - **Batch**：大批量消息减少 RPC 次数；
  - **Leaf Node**：边缘流先在 leaf 聚合，避免网络抖动；

## 十四、运维与可观测性

- **指标导出**：Prometheus（端口 8222，`/metrics`）；
- **关键指标**：
  - `gnatsd_varz_connections`（连接数）；
  - `gnatsd_varz_subscriptions`（订阅数）；
  - `gnatsd_jetstream_memory_used` / `gnatsd_jetstream_storage_used`；
  - `gnatsd_jetstream_streams_bytes_total`（按 stream）；
- **日志**：结构化 JSON（默认 stderr）；
- **管理工具**：
  - `nats` CLI（Go 实现，跨平台）；
  - `nats-box`（容器化 NATS）；
  - **NATS Survey**（Web UI，社区）；
- **健康检查**：`nats server check connection` / `nats server check stream`；
- **备份恢复**：
  - Stream Snapshot/Restore（标准方式）；
  - 跨集群 Gateway 异步同步；

## 十五、生态与集成

- **客户端 SDK**：Go（核心）、Java、Python、Node.js、Ruby、Rust、C#、C/C++、Elixir、Erlang、PHP；
- **CDC 集成**：Debezium NATS Outbox Connector（实验）；
- **Schema 管理**：社区 Schema Registry 项目；
- **流处理**：Flink Connector、Materialize Adapter、Spark Adapter；
- **IoT 桥**：NATS-MQTT Bridge、NATS-WebSocket；
- **Kafka 桥**：nats-kafka（双向同步）；
- **Kubernetes**：
  - **NATS Helm Chart**（官方）；
  - **NATS Operator**（社区）；
- **服务网格**：Linkerd 集成、Consul 直接绑定；
- **可观测性**：OpenTelemetry 集成、Prometheus；

## 十六、核心论文与算法原理

- **NATS 设计哲学**：Derek Collison 博客 "Subject-Based Messaging"；
- **RAFT 论文**（Ongaro）：NATS 自研 RAFT 实现；
- **CloudEvents CNCF 规范**：NATS 原生支持；
- **MQTT 对比**：MQTT 5 vs NATS（功能类似，NATS 性能更强）；
- **gRPC vs NATS Request/Reply**：性能相当但 NATS 更轻；
- **JetStream 设计文档**：[NATS Docs](https://docs.nats.io/nats-concepts/jetstream)；
- **轻量协议设计**：NATS 协议 10+ 年未变（仅增加 HMSG 与 headers）；

## 十七、源码学习路径

### 17.1 必读模块清单（按入门到进阶）

| 阶段 | 模块 | 路径 | 说明 |
|------|------|------|------|
| 入门 | 启动 | `server/server.go` | 主入口 + Accept loop |
| 入门 | 路由 | `server/sublist.go` | Subject trie 路由 |
| 入门 | 账户 | `server/accounts.go` | 多租户隔离 |
| 进阶 | JetStream | `server/jetstream.go`、`jetstream_api.go` | 流管理 |
| 进阶 | FileStore | `server/filestore.go` | 持久化 |
| 进阶 | RAFT | `server/raft.go`、`raft_transport.go` | 共识 |
| 高级 | Gateway | `server/gateway.go` | 跨集群 |
| 高级 | Leaf Node | `server/leafnode.go` | 边缘节点 |

### 17.2 推荐阅读顺序

1. **第 1 周**：NATS 官方文档 + `server.go` 启动；
2. **第 2 周**：subject 路由（sublist）+ 订阅实现；
3. **第 3 周**：JetStream Stream + Consumer；
4. **第 4 周**：FileStore + Block/Index；
5. **第 5 周**：RAFT 共识（meta + stream group）；
6. **第 6 周**：Leaf Node / Gateway 联邦；

## 十八、源码规模与开发语言

- **规模**：NATS Server 2.10 约 **30 万行** Go；
- **语言**：Go（单一语言）；
- **客户端**：Go（核心）、Java、Python、Node.js、Ruby、Rust 等多语言；
- **构建系统**：Go modules；
- **平台**：Linux / macOS / Windows / FreeBSD / ARM / x86 / ARM64；
- **测试**：Go testing + 集成测试（`tests/`）；
- **CI**：GitHub Actions；

## 十九、学习资源

- **官方文档**：[NATS Documentation](https://docs.nats.io/)；
- **书籍**：
  - "NATS messaging"（社区文档）；
  - 官方培训（Synadia）；
- **视频课程**：Synadia Academy（官方培训，免费）；
- **社区**：
  - [NATS Slack](https://slack.nats.io/)（最活跃）；
  - [NATS Survey](https://github.com/nats-io/nats-surveyor)（UI 工具）；
  - NATS Summit 大会；
- **中文资源**：GitHub `nats-io/nats.go` 中文注释、`CloudEvents` 中文规范；
- **Reddit r/NATS**（社区讨论）；

## 二十、可移植性与部署

- **平台**：Linux / macOS / Windows / FreeBSD（生产主流 Linux）；
- **架构**：x86_64 / ARM64 / RISC-V（官方支持）；
- **容器化**：官方 Docker 镜像 `nats`；
- **Kubernetes**：
  - **NATS Helm Chart**（官方）；
  - **NATS Operator**（社区）；
- **裸机**：单二进制 `nats-server`；
- **硬件**：极低（1 vCPU + 256MB 内存可启动）；
- **嵌入式**：可嵌入 Go 应用（轻量模式）；

## 二十一、变体与衍生项目

- **NGS（NATS Global Services）**：Synadia 官方全球托管；
- **NATS Edge**：轻量级边缘版本（资源更低）；
- **NATS CloudEvents**：CloudEvents 集成；
- **对比项目**：
  - **Apache Kafka**：本 wiki kafka.md；
  - **Redpanda**：本 wiki redpanda.md；
  - **Apache Pulsar**：本 wiki pulsar.md；
  - **Redis Streams**：KV + 轻量流；
  - **RabbitMQ**：老牌队列；
  - **MQTT**：IoT 协议（NATS 兼容）；

## 二十二、风险与陷阱

### 22.1 常见误用

- **Subject 设计不当**：
  - 太深的 subject 树（如 `a.b.c.d.e.f`）→ 影响 trie 性能；
  - 通配符 `>` 使用过多 → 路由表大；
- **Core NATS 当消息队列**：Core NATS 没有持久化，**重启丢消息**；
- **JetStream Stream 数量过多**：每个 Stream 占用一个 RAFT group，过多增加元数据负担；
- **KV Watch 频繁创建**：每次 Watch 走 RAFT，过多影响性能；
- **Leaf Node 节点过多**：单 hub 节点连接数限制（~50K）；
- **Gateway 配置循环**：跨集群同步配置不当 → 消息循环；

### 22.2 性能陷阱

- **小消息高频发送**：NATS 协议高效但消息过小（< 10B）反而成为瓶颈；
- **Subject 频繁订阅/取消订阅**：订阅表操作开销；
- **Queue Group 不均衡**：hash 分布不均 → 部分 subscriber 空闲；
- **JetStream Max Ack Pending 过高**：consumer 积压影响 replay；
- **Snapshot 频繁**：snapshot 频率过高影响 IO；
- **磁盘配置**：FileStore 推荐专用磁盘（避免与系统盘竞争）；

### 22.3 迁移痛点

- **从 Kafka 迁移**：
  - NATS subject 是动态的（无需预先创建）；
  - 但 Kafka topic 必须预先创建；
  - 应用代码改写量大；
- **从 Core NATS 升级到 JetStream**：协议兼容但消费模式变化；
- **多语言客户端兼容性**：C/C++ 客户端更新慢（部分新特性延迟支持）；

### 22.4 运维雷区

- **JetStream 文件系统满**：磁盘满 stream 停止接受写入；
- **RAFT 多数派不可用**：3 节点集群 2 故障 → cluster 不可用；
- **Leaf Node 认证失效**：Token 过期导致 leaf 断开；
- **Gateway 跨区域延迟高**：跨洋同步延迟 > 200ms 时业务不可接受；
- **Account 资源耗尽**：单账户 CPU/连接超额被限流；
- **Snapshot 损坏**：手动 restore 困难，建议备份配置；

## 二十三、与本项目的关联

- **可借鉴的设计**：
  - **单进程轻量化设计**：本项目 `learning/playground/` 演示项目可参考 NATS 的极简部署；
  - **Subject-based 路由**：本项目如有消息路由需求可参考（基于 trie 的动态路由）；
  - **Leaf Node 模式**：本项目如需边缘部署，可参考分层架构；
  - **Go 语言实战**：本项目 C/C++ 主导，但 `learning/` 可引入 Go 学习 NATS 源码；
- **不适合借鉴**：
  - **RAFT 自研**：本项目当前无分布式共识需求；
  - **JetStream FileStore**：本项目已有自己的存储引擎；
- **集成参考**：
  - 本项目若需轻量消息集成（IoT、边缘），NATS 比 Kafka 更适合；
  - 本项目 RPC 通信：NATS Request/Reply 比 HTTP REST 更快；

## 二十四、FileStore 的Block 切分、写入缓存与每 Subject 索引

> 本节源码路径：`server/filestore.go`、`server/stree/`。§8.1 用一句话描述了「Append-only Block + Index + Snapshot」，§17.1 把 `server/filestore.go` 列为进阶阅读材料，但**都没有展开三个决定 FileStore 行为的内部机制**：Block 大小如何按配额反推、写入如何通过 cache + flush 落盘、每 Subject 状态（`fss`）如何维护。本节补齐这三处。

### 24.1 Block 大小反推：`dynBlkSize` 的 25% 开销与三段夹逼

NATS JetStream 的 Stream 只需给定**总配额** `maxBytes`，Block（对应 Kafka 的 segment）大小由它反推。`dynBlkSize`（`server/filestore.go:840`）是唯一的计算入口：

```
dynBlkSize(retention, maxBytes, encrypted):# server/filestore.go:840
    if maxBytes > 0:
        blkSize = (maxBytes / 4) + 1                # ① 25% 开销，server/filestore.go:842
        if blkSize % 100 != 0:                # ② 向上取整到 100 的倍数
            blkSize += 100 - (blkSize % 100)
        if   blkSize <= FileStoreMinBlkSize: blkSize = FileStoreMinBlkSize      # ③ 下界
        elif blkSize >= FileStoreMaxBlkSize: blkSize = FileStoreMaxBlkSize      #    上界
        else:                                blkSize = defaultMediumBlockSize#    中段
        if encrypted and blkSize > maximumEncryptedBlockSize:                # ④ 加密上限
            blkSize = maximumEncryptedBlockSize
        return blkSize
    # maxBytes <= 0 时走另一分支（按 retention 推算）
```

四个常量的实际取值（`server/filestore.go:375`、`server/filestore.go:383`、`server/filestore.go:387`、`server/filestore.go:391`）：

| 常量 | 值 | 作用 |
|------|-----|------|
| `defaultMediumBlockSize` | 4 MB | 中段区间的取值 |
| `maximumEncryptedBlockSize` | 2 MB | 加密流量的硬上限 |
| `FileStoreMinBlkSize` | 32 KB（`32 * 1000`） | 下界 |
| `FileStoreMaxBlkSize` = `maxBlockSize` | = `defaultLargeBlockSize` | 上界 |

三点值得注意：

- **`+ 1` 是为了让 `maxBytes/4` 整除时不落到区间外的下界**。`maxBytes = 4` 时 `blkSize = 2`，若无 `+1` 则为 1，会被下界夹到 32KB；这里 1 让极小配额也能得到一个非零值。
- **向上取整到 100 的倍数**（`server/filestore.go:844`）纯粹为了可读性与日志对齐，代价是最多 99 字节的浪费。
- **`else` 分支（中段）直接返回 `defaultMediumBlockSize` 而非 `blkSize`**（`server/filestore.go:851`）。也就是说：只要计算值落在 (32 KB, MaxBlkSize) 区间内，**就一律取 4 MB，配额再小也不用计算值**。这是刻意的产物收敛——Block 数量直接决定元数据开销与查找开销，让中等配额都统一为 4 MB 可以避免 Block 碎片化。副作用是：小配额场景下 4 MB 的 Block 意味着**极少几个 Block 就会写满**（例如配额 16 MB 只切出 4 个 Block），配额越大 Block 才逐渐逼近上界。

**为什么加密场景要额外压到 2 MB**：`server/filestore.go:383` 的注释与 `dynBlkSize` 的第④ 步说明，加密是**每个 Block 独立**做的（chacha20-poly1305 之类），Block 越大意味着单个密文块越大，而密钥轮换以 Block 为单位会放大重写代价；压到 2 MB 是在加密强度与重写粒度之间的取舍。

> **工程含义**：调`--max-bytes` 时 Block 数量并非线性增长，而是"阶梯式"的。若观察到 Block 数远少于预期（元数据偏少、单Block 偏大），通常就是踩中了中段统一 4 MB 的规则，而不是配置没生效。

### 24.2 写入路径：cache 先行、flush 后落

`writeMsgRecordLocked`（`server/filestore.go:7598`）是每条消息写入的入口，它的执行顺序体现了几层独立的考虑：

```
writeMsgRecordLocked(rl, seq, subj, mhdr, msg, ts, flush, kick):   # server/filestore.go:7598
    if mb.werr != nil: return mb.werr                    # ① 粘滞错误：一次失败后永久拒绝写入
    defer: 若出错且非读错误 → mb.werr = rerr             # ② 区分「写失败」与「读旧数据失败」
    if mb.mfd == nil: mb.enableForWriting(...)            # ③ 惰性打开文件描述符
    if len(subj) > 0 and not mb.noTrack:                  # ④ 每 Subject 状态先于 cache 变更
        mb.ensurePerSubjectInfoLoaded()
        if ss, ok := mb.fss.Find(subj); ok: ss.Msgs++; ss.Last = seq
        else: mb.fss.Insert(subj, SimpleState{1, seq, seq})
    mb.setupWriteCache(nil)                # ⑤ 准备写缓存（可能已被上一步带入）
    mb.ecache.Strengthen()                            # ⑥ 强化弹性引用，阻止 GC 抢走未落盘数据
    ...追加到 cache.buf / cache.idx...
```

三个设计要点：

- **①/② 的错误分类很有讲究**。`server/filestore.go:7605` 起的 `defer` 块用 `isReadErr(rerr)` 把错误分成两类：**读旧数据出错**（如某个历史 Block 损坏）只`rateLimitWarn` 后继续，**不置 `werr`**；只有真正的写入失败才设置 `mb.werr`。而 `werr` 一旦非nil，**后续所有写入立即返回该错误**（`server/filestore.go:7600`）。这意味着一个历史 Block 的局部损坏不会让整个 Stream 停止服务——这是个相当克制的数据可用性设计。
- **④ 的顺序是刻意的**：`ensurePerSubjectInfoLoaded` 可能触发 cache 的换入/重建，若放在 cache 变更之后就会踩到源码注释指出的问题——"Do this before changing the cache that would trigger a flush pending msgs call if we needed to regenerate the per subject info"（`server/filestore.go:7637`）。
- **⑥ `ecache.Strengthen()` 是 JetStream 的生命周期基石**：缓存块在未被 flush 前以**强引用**挂住，防止 Go 的 GC 判定它可回收导致未落盘数据丢失；只有当 flusher 决定写入完成后才"弱化"。这是典型的「用引用语义兜住持久化窗口」。

落盘的收口在 `flushPendingMsgs`（`server/filestore.go:8623`）：

```
flushPendingMsgs():                                   # server/filestore.go:8623
    mb.mu.Lock(); fsLostData, err = mb.flushPendingMsgsLocked(); mb.mu.Unlock()
    if fsLostData != nil and fs != nil:
        fs.rebuildState(fsLostData)                   # 丢数据时重建 Stream 级状态
```

注意它返回的 `*LostStreamData` —— 若 flush 过程中发现已有数据不可恢复，**不是只返回错误，而是把丢失范围上报给 `fileStore` 触发 `rebuildState`**，让 Stream 的 `first/last seq` 等内存状态与磁盘重新对齐。这类"写路径发现损坏 → 反向驱动状态重建"的闭环，是单进程嵌入式存储能做到自愈的关键。

> **复杂度**：每条消息写入摊还 O(1)（追加到内存 buffer + 一次 `stree` 查找）；flush 是 O(pending)。`fss` 为 `stree.SubjectTree`（`server/filestore.go:249`），即 subject 字典树——按 subject 前缀共享路径，**`foo.a` 与 `foo.b` 的查找复杂度随公共前缀增加而下降**，且天然支持 `foo.>` 通配订阅的区间遍历。这比Go 原生 map 更适合 JetStream「大量 subject + 前缀订阅」的访问模式。

### 24.3 缓存一致性判据：`cacheAlreadyLoaded` 的重放校验

`cacheAlreadyLoaded`（`server/filestore.go:8783`）判断内存缓存是否与磁盘状态一致，判据不是简单的"cache 非空"，而是**重算应当存在的条目数并比对**：

```
cacheAlreadyLoaded():                                  # server/filestore.go:8783
    if mb.cache == nil: mb.cache = mb.ecache.Value()   # 尝试从弹性缓存取出
    if mb.cache == nil or mb.cache.fseq == 0 or len(mb.cache.buf) == 0:
        return false
    # 关键：重新推导「当前应有多少条」，与缓存实际条目数比对
    numEntries = mb.msgs + uint64(mb.dmap.Size()) + (atomic.LoadUint64(&mb.first.seq) - mb.cache.fseq)
    return numEntries == uint64(len(mb.cache.idx))
```

三项相加的含义：

| 项 | 含义 |
|----|------|
| `mb.msgs` | 未删除（活跃）消息数 |
| `mb.dmap.Size()` | 删除标记（dmap / tombstones）条目数——**删除也是一条物理记录** |
| `first.seq - cache.fseq` | 缓存起点之前、已被淘汰出缓存但仍在 Block 内的消息数 |

`dmap` 的存在是这套设计的关键：NATS 的删除是**写一条墓碑记录**而非抹掉原记录（append-only 的必然要求），因此磁盘上「活跃消息」= `msgs`，而「索引条目」= `msgs +墓碑数 + 缓存外的历史消息`。若只用 `msgs` 比对，删除过的 Block 会永远判定为"缓存未加载"，退化成每次都从磁盘重放。

`atomic.LoadUint64(&mb.first.seq)` 用原子读而非直接取字段，是因为 `first.seq` 可能被并发的过期清理（expiration）线程推进；这行代码是该模式在本文件里的一处典型用法。

> **成本与收益**：这是一个 O(1) 校验（只有三个整型加法 + 一次长度比较），但换来的是**可以放心地在任意时刻丢弃并重建缓存**。`server/filestore.go:3209` 与 `server/filestore.go:5052` 两处都用到它；后者源码注释还特别说明了"**Not safe to use `mb.cacheAlreadyLoaded()` here as the heuristic**"（`server/filestore.go:5052`）——即某些路径不能依赖这个判据，说明该校验虽可靠但**不能推广为"缓存一定与磁盘一致"的全局不变式**。

> **工程含义**：JetStream 的缓存是**可丢弃的加速层**，不是权威数据。任何"重启后数据不丢"的保证都来自 append-only 的 Block 文件与 fsync，而不是缓存。把 Block 视为唯一真相、缓存视为纯派生状态——这个定位是 JetStream 能在崩溃后精确恢复的根本原因。

---

## 二十五、参考资料

- [NATS 官方主页](https://nats.io/)
- [NATS Documentation](https://docs.nats.io/)
- [NATS GitHub](https://github.com/nats-io/nats-server)
- [Synadia 官方主页](https://synadia.com/)（商业化公司）
- [JetStream Documentation](https://docs.nats.io/nats-concepts/jetstream)
- [NATS KV Store](https://docs.nats.io/nats-concepts/kv)
- [NATS Object Store](https://docs.nats.io/nats-concepts/object-store)
- [NATS Leaf Nodes](https://docs.nats.io/nats-concepts/leaf-nodes)
- [NATS Gateway](https://docs.nats.io/nats-concepts/gateway)
- [CloudEvents 规范](https://cloudevents.io/)
- [CNCF NATS 项目](https://www.cncf.io/projects/nats/)（云原生毕业项目）
- [Synadia Academy](https://academy.synadia.com/)（免费培训）
- [NATS Protocol Specification](https://github.com/nats-io/nats-architecture-and-design)
- [Apache Kafka 主页](https://kafka.apache.org/)（对比参考）
- [Apache Pulsar 主页](https://pulsar.apache.org/)（对比参考）
- [Redpanda 主页](https://redpanda.com/)（对比参考）