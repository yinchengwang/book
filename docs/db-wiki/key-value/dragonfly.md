# Dragonfly Wiki（现代多线程 Redis 兼容内存数据库）

> 参考版本：Dragonfly 仓库当前 commit `dafb825b0e5c6f0d8ce2836e1252a568fee639ff`（v1.x 系列；Dragonfly 1.0 GA 2023 年）。

## 一、项目定位

Dragonfly 是一款由 Roman Gushchin（前 Facebook 工程师，Linux 内核 cgroup v1 作者）开发的**Redis 兼容、现代多线程、内存键值数据库**。它在保持 Redis 协议（RESP）兼容性的同时，通过**共享无锁架构 + 无 sharding 设计**实现单实例可处理数百万 QPS，远超传统 Redis 单实例的 10W~20W 限制。它的设计目标是"成为 Redis 的天然替代品，在不修改应用代码的前提下提供大幅性能提升"。

- **痛点**：
  - 传统 Redis 单线程命令执行，多核利用率低（受单核瓶颈限制）；
  - Redis Cluster 通过 sharding 解决扩展性，但增加运维复杂度（16384 槽 + 跨槽事务不支持）；
  - 多线程 Redis 兼容 fork（如 KeyDB）实现复杂，架构不够干净；
  - Redis 6.x 的多线程 IO 仅加速解析不拖动解析命令执行；
  - 云原生时代需要"更高 QPS per instance"降低基础设施成本；
- **典型用户**：Meituan（美团）、Lazada、Alibaba（部分内部缓存）、DuckDuckGo（部分服务）、Microsoft（GitHub Codespaces 部分场景）；社区 case 涵盖社交、推荐、广告、缓存、限流、分布式锁。

## 二、发展历史与社区

- **起源**：2022 年 5 月 Roman Gushchin（前 Linux 内核工程师、cgroup v1 作者）宣布 Dragonfly；初始目标是为 Meta / Facebook 的场景提供一个"Redis-like 但支持多核"的高性能 KV；
- **里程碑**：
  - 2022 年 5 月开源（初始版本，名称来自 DragonflyDB）；
  - 2022 年 8 月 0.10 版本（性能稳定）；
  - 2022 年 11 月 0.14（ACL、Snapshot 优化）；
  - 2023 年 1 月 0.16（Lua 引擎优化）；
  - 2023 年 4 月 1.0 GA（生产就绪）；
  - 2023 年 7 月 Beta Support；
  - 2023 年 10 月正式商用版（Dragonfly Enterprise）发布；
  - 2024 年持续优化 + 新数据结构支持；
- **维护状态**：活跃（每月发布小版本）；
- **社区**：GitHub Discussions、Dragonfly Labs 官方博客、Reddit r/dragonflydb、Discord 频道。

## 三、许可与商业限制

- **许可证**：**Business Source License (BSL) 1.1**（非 OSI 认证的开源许可证，类似 MariaDB、MongoDB 早期）；
  - 来源：`dragonfly/LICENSE`；
  - 关键条款：
    - 允许**非生产使用**（开发、测试、学习）；
    - 允许**生产使用**当不作为"数据库即服务"对外提供（除非获得 Dragonfly Labs 商业许可）；
    - 4 年后自动转换为 Apache 2.0；
- **商业版本**：Dragonfly Enterprise（含管理控制台、企业级监控、自动扩缩容）；
- **生态关系**：完全 Redis 协议兼容（已有大量 Redis 客户端可直接使用）。

## 四、业界主要使用方与案例

- **Meituan（美团）**：外卖订单状态缓存，部分场景替代 Redis；
- **Lazada**：电商场景缓存；
- **DuckDuckGo**：部分服务缓存；
- **Microsoft**：GitHub Codespaces 部分场景；
- **Alibaba**：部分内部 KV 场景；
- **Shopify**：缓存层；
- **社区案例**：社交 feed、广告索引、限流、分布式锁、会话存储、计数器；

## 五、一句话对比定位

> Dragonfly 是**"Redis 协议 + 现代多线程 + 共享无锁架构 + 无 sharding"**的下一代内存 KV；与 **Redis（单线程经典）**、**KeyDB（多线程 fork）**、**Memcached（纯 KV、无持久化）**、**Valkey（Linux Foundation 下的 Redis fork）** 一起构成**内存 KV 阵营**。Dragonfly 的差异化是**无 sharding + 单实例百万 QPS**。

## 六、整体架构

Dragonfly 采用**共享无锁架构（Shared-Nothing）**：

1. **Main Thread**：网络监听、HTTP 管理；
2. **Proactor Threads**：网络 IO（多线程）；
3. **Shard（dfly::Db）**：多个 sharded-by，每个独立 dict + 无锁互斥；
4. **Transaction Queue**：事务排队；
5. **Background Fibers**：Compact / Defrag / Garbage Collect。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│            用户 API (RESP 协议 / Redis 客户端)                         │
│   - redis-cli / jedis / redis-py / go-redis / ioredis 直接兼容       │
│   - HTTP 管理端口 (默认 9090)                                          │
│   - TLS 支持（社区版）                                                │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ RESP2 / RESP3
┌─────────────────────▼────────────────────────────────────────────────┐
│                Proactor Pool (server/proactor.h)                       │
│   - N 个 Proactor 线程（默认 = 核数）                                  │
│   - 每个 Proactor 持有一个 main reactor（io_uring/epoll 异步 IO）      │
│   - 接受连接 + 解析 RESP + 路由到对应 Shard                          │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                Connection (server/connection.h)                        │
│   - 每个客户端连接一个 Connection 对象                                  │
│   - 绑定到一个 Proactor 线程（不跨线程）                              │
│   - 提供 RESP 解析、命令序列化                                          │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                Dispatcher (server/dispatcher)                          │
│   - 关键创新：共享无锁（每个 shard 独立，无跨 shard lock）             │
│   - 按 key hash（CRC16 类似）路由到固定 shard                          │
│   - 单 key 操作：直接本地处理                                          │
│   - 多 key / Lua 脚本：触发 Fiber 切换到 global queue                  │
│   - Cluster 模式：类似 Redis Cluster 路由（127.0）                    │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                Shard / dfly::Db (server/db_singleton.h)               │
│   - N 个独立 shard（默认 = 核数 / 2）                                  │
│   - 每个 shard 持有：dict、TTL、streams（独立、零冲突）                │
│   - 单线程 EventLoop（同一时刻一个 shard 仅一个线程操作）              │
│   - Fiber-based 协程（无线程切换开销）                                 │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                数据结构层 (src/server/)                                 │
│   - String / List / Hash / Set / ZSet / Stream (Redis 同款)            │
│   - CompactString：定制字符串（节省内存）                              │
│   - QuickList + Listpack（紧凑存储）                                  │
│   - ZSet：dict + skiplist                                            │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                持久化层 (server/snapshot.cc + server/rdb_extensions.cc)│
│   - Snapshot (RDB 风格二进制)                                         │
│   - AOF (Append-Only File, 8.0+)                                     │
│   - Delta Snapshot (增量快照)                                          │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                集群 / HA (native/cluster/ + server/replica.cc)        │
│   - Cluster 模式（127.0 / Dragonfly Cluster 协议）                   │
│   - 主从复制（PSYNC，部分兼容 Redis）                                │
│   - 内存分片（仅 Enterprise）                                         │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- 服务端启动：`src/server/main.cc`
- DFly 主入口：`src/server/server.h:ServerFamily`
- Dispatcher：`src/server/dispatcher.cc`
- DB：`src/server/db_singleton.h`

### 6.2 进程模型

Dragonfly 采用**多线程 + 协程（Fiber）**：

- **Main Thread**：监听 + 接收 + 启动 Proactor；
- **Proactor Threads**（默认 = CPU 数）：每个线程一个 main loop + 多个 fibers；
- **Connection Fiber**：每个客户端连接一个协程（在某个 Proactor 线程）；
- **Command Fiber**：每个命令一个协程（短暂）；
- **Transaction Fiber**：跨 key 命令专用的协程；
- **Thread per shard**：每个 shard 单线程（避免数据竞争）；
- **Fiber 调度**：基于 `boost::context` 或 `ucontext`，比 goroutine 更轻；
- **优势**：多核并行 + 协程轻量（每个连接 < 1KB stack）；

### 6.3 一条 SET 命令的端到端路径

1. **客户端连接**：TCP 连接到 Dragonfly 端口（默认 6379）；
2. **Proactor 接受**：新连接被某个 Proactor 线程接受；
3. **RESP 解析**：Proactor 协程读取 RESP 请求；
4. **命令分派**：
   - 单 key 命令（SET/GET/DEL）：根据 key hash 路由到目标 shard；
   - 多 key 命令（MSET/MGET/Lua 脚本）：进入 global queue，所有 shard 同步参与；
5. **执行命令**：在对应 shard 上同步执行；
6. **持久化**：可选 append AOF / mark dirty；
7. **响应客户端**：通过 Proactor 协程写响应。

### 6.4 一条 GET 命令的端到端路径

1. **接受连接 + 解析**（Proactor 协程）；
2. **路由 key 到 shard**（hash mod N）；
3. **在 shard dict 中查找**（O(1) 哈希）；
4. **返回 value**。

## 七、存储引擎（KV 类型核心库深度展开）

Dragonfly 的核心是**共享无锁分片架构 + 定制紧凑数据结构**。

### 7.1 共享无锁架构（核心创新）

`src/server/shard_set.h` + `src/server/db_singleton.h`:

```
┌──────────────────────────────────────────────────────────────┐
│                    Dragonfly 内存布局                          │
│                                                              │
│   Shard 0          Shard 1          Shard 2      Shard N    │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌────────┐  │
│  │ dict     │    │ dict     │    │ dict     │    │ dict   │  │
│  │ TTL      │    │ TTL      │    │ TTL      │    │ TTL    │  │
│  │ Streams  │    │ Streams  │    │ Streams  │    │Streams │  │
│  │ Pub/Sub  │    │ Pub/Sub  │    │ Pub/Sub  │    │ Pub/Sub│  │
│  └──────────┘    └──────────┘    └──────────┘    └────────┘  │
│   Thread 0       Thread 1       Thread 2      Thread N       │
│                                                              │
│   - 单线程 EventLoop（每时刻 1 个 thread 操作 1 个 shard）    │
│   - 无跨 shard mutex（key 路由 = shard 隔离）                  │
│   - 多 key 命令：所有相关 shard 同步进入 transaction mode   │
└──────────────────────────────────────────────────────────────┘
```

要点：
- **每个 shard 独立 dict**：避免锁竞争；
- **按 key hash 路由**：CRC32(key) mod N → shard N；
- **多 key 命令**：触发"barrier transaction"，所有 shard 暂停服务直到事务完成（短暂）；
- **Lua 脚本**：在同一个 transaction 内执行（避免冲突）；

### 7.2 CompactString（定制字符串）

`src/server/compact_string.h`:

```cpp
// src/server/compact_string.h:14
class CompactString {
 private:
  std::string_view sv_;
  std::unique_ptr<std::string> heap_str_;
  // 紧凑编码（small/embstr/int）
};
```

- **节省内存**：相比 Redis 的 robj + sds，CompactString 在小字符串时内存占用更低（10-30% 节省）；
- **多种编码**：small (< 64B 嵌在 CompactString 内)、int (整数值)、heap (堆分配)；
- **原子操作**：INCR/DECR 直接操作整数值（无字符串解析开销）；

### 7.3 QuickList + Listpack

`src/server/quick_list.h`:

要点：
- **QuickList**：与 Redis quicklist 类似（双向链表 + 节点内紧凑容器）；
- **Listpack**：默认紧凑字符串替代 v2;
- **小对象**：小列表 listpack 编码节省内存；

### 7.4 ZSet（dict + skiplist）

`src/server/zset_family.cc`:

要点：
- **双结构**：dict（member → score）+ skiplist（score 排序）；
- **小 ZSet**：单 dict 或 single-skiplist 编码；
- **大 ZSet**：双结构（与 Redis 一致）；

### 7.5 Snapshot（持久化）

`src/server/snapshot.cc`:

```cpp
// src/server/snapshot.cc:14
class RdbSaver {
 public:
  Status Save(io::Sink* sink, const SaveContext& ctx);
  // 按 shard 并行 save
};
```

要点：
- **按 shard 并行 Snapshot**：多线程并发导出各 shard 数据；
- **RDB 兼容格式**：与 Redis RDB 格式兼容（部分支持）；
- **Delta Snapshot**：增量（自上次 snapshot 后的变更）；
- **Cron 触发**：可设置定时自动 snapshot；

### 7.6 AOF（8.0+ 新增）

`src/server/aof.cc`:

要点：
- **Append-only File**：类似 Redis AOF；
- **每 shard 独立 AOF**：避免跨 shard 锁；
- **Replay**：启动时按 shard 顺序 replay AOF；
- **fsync 策略**：everysec / always / no；

### 7.7 Pub/Sub（多 shard）

`src/server/pubsub_family.cc`:

要点：
- **Pub/Sub 全 shard 索引**：每个 shard 维护自己的 subscriber；
- **Publish**：向所有相关 shard 广播；
- **Pattern subscribe**：支持 PSUBSCRIBE `*` 模式匹配；
- **Sharded Pub/Sub**（7.x+）：按 channel 路由到固定 shard 减少跨 shard 通信；

### 7.8 Stream（消息流）

`src/server/stream_family.cc`:

要点：
- **Stream 数据结构**：与 Redis Stream 类似；
- **Consumer Group**：支持多消费者 + PEL（Pending Entries List）；
- **内部加速**：因 Dragonfly 单 shard 内 Stream 操作更快；

### 7.9 Compact / Defrag

`src/server/compact_object.cc`:

要点：
- **后台自动 Compact**：定期触发（小对象压缩、回收空闲内存）；
- **Defrag**：防止内存碎片（类似 `activedefrag`）；
- **不影响前端**：在低优先级协程中执行；

### 7.10 ACL（访问控制）

`src/server/acl_family.cc`:

要点：
- **ACL 用户**：密码 + 权限 + key 模式（与 Redis ACL 兼容）；
- **持久化**：用户列表持久化到文件；
- **ACL LOG**：记录拒绝的命令便于审计；

### 7.11 Scripting（Lua）

`src/server/scripting.cc`:

要点：
- **Lua 5.1**：与 Redis Lua 兼容；
- **Fast Path**：单 key 脚本可在 single-shot 模式执行（无 transaction overhead）；
- **脚本缓存**：`EVALSHA` 支持；
- **Script Kill / Flush**：`SCRIPT KILL` 停止长运行脚本；

### 7.12 主从复制

`src/server/replica.cc`:

要点：
- **PSYNC 兼容**：与 Redis 主从协议兼容（部分）；
- **Full Sync**：发送完整 RDB snapshot；
- **Partial Sync**：偏移量续传（有限）；
- **Failover**：Dragonfly Cluster 内自动选主（基于 Dragonfly Cluster 协议，不是 Raft）；

### 7.13 Cluster 模式（127.0）

`src/server/cluster/`:

要点：
- **127.0 Cluster 模式**：与 Redis Cluster 协议兼容；
- **Gossip**：节点间通过 gossip 交换状态；
- **Slots**：16384 槽 + 客户端路由；
- **Failover**：基于 Dragonfly 内部协议（与 Redis Sentinel 不同）；

## 八、索引机制

Dragonfly 数据结构自身的"索引"：

- **dict**（O(1) 哈希）：所有类型；
- **skiplist**（O(log N)）：ZSet 排序；
- **rax**（radix tree）：Stream 消息索引；
- **geohash**：GEO 类型（与 Redis 一致）；
- **bitmap / HyperLogLog**：Redis 同款；
- **Bloom Filter**：通过 RedisBloom 模块。

## 九、查询/计算/接口层

- **协议**：RESP2 / RESP3（与 Redis 完全兼容）；
- **API 风格**：所有 Redis 命令支持；
- **执行模型**：多线程 Proactor + 单 shard 单线程 + Fiber 协程；
- **脚本**：Lua 5.1 + LuaJIT 实验性；
- **模块系统**：Redis Modules 部分兼容（实验性）；
- **HTTP 管理**：9090 端口 + Prometheus 指标 + JSON API；
- **JSON 数据结构**：RedisJSON-like（v1.13+）；

## 十、事务与一致性（KV 类型简化版）

- **MULTI / EXEC**：与 Redis 兼容（事务内所有命令原子执行）；
- **WATCH / CAS**：与 Redis 兼容；
- **Lua 脚本**：原子执行；
- **持久化一致性**：
  - Snapshot 异步（非强一致）；
  - AOF `everysec` 最多丢 1 秒；
  - AOF `always` 强一致（性能代价）；
- **主从复制**：异步复制，存在主从切换丢数据风险；
- **Cluster 一致性**：异步复制，与 Redis Cluster 类似；
- **shard 内 transaction**：所有跨 shard 命令同步执行（barrier transaction）；

## 十一、复制与高可用

- **主从复制**（`src/server/replica.cc`）：
  - 部分兼容 Redis 主从协议（PSYNC）；
  - Full Sync（snapshot） + Partial Sync（offset）；
  - 主从心跳；
- **Dragonfly Cluster**（127.0）：
  - 与 Redis Cluster 协议兼容（部分）；
  - Gossip + 槽路由；
  - 自动 failover（基于 Dragonfly Cluster 协议）；
- **Enterprise HA**：含 Sentinel-like 监控、Multi-AZ Replication、Active-Active（企业版）；
- **客户端库**：redis-cli / jedis / redis-py / go-redis / ioredis 直接兼容；

## 十二、扩展性

- **水平扩展**：Dragonfly Cluster（127.0 协议）支持横向扩展；
- **垂直扩展**：单实例可支撑 1M+ QPS（多线程 Proactor + 共享无锁）；
- **内存扩展**：单实例可利用 100GB+ 内存（多 shard dict + 紧凑数据结构）；
- **多租户**：通过 ACL（用户 + 密码 + 权限）；
- **动态参数**：部分 Options 可运行时调整（如 num_shards 不可改，maxmemory 可改）；
- **冷热分层**：Enterprise 版支持 RAM + SSD 磁盘分层；
- **跨数据中心**：Enterprise 版支持 Active-Active CRDT（类似 Redis Enterprise）；

## 十三、性能与基准

- **官方基准**（`dragonfly/bench/`）：
  - 单实例 GET/SET：1M+ QPS（典型硬件）；
  - 与 Redis 对比：相同硬件下 5-10 倍吞吐；
  - Pipeline：可突破 10M QPS；
- **YCSB**：Dragonfly 在 YCSB 工作负载下领先 Redis；
- **关键指标**：
  - **延迟**：P99 < 1ms（典型 GET/SET）；
  - **吞吐**：单实例百万级 QPS；
  - **内存效率**：紧凑编码（CompactString）节省 10-30% 内存；
- **限制**：
  - Lua 脚本（跨 shard）：所有 shard barrier，性能下降；
  - 大 key：单 shard 单线程，大 key 是瓶颈；
  - Cluster 模式：跨槽命令性能下降；

## 十四、运维与可观测性

- **监控指标**：HTTP 9090 端口 + Prometheus exporter；
- **INFO 命令**：完全 Redis 兼容；
- **SLOWLOG**：慢查询日志；
- **MEMORY**：内存使用统计；
- **CLIENT**：客户端连接信息；
- **CLUSTER**：集群信息；
- **日志**：标准 glog 风格（自创 logger）；
- **Tracing**：通过 OpenTelemetry 集成；
- **慢命令**：自动识别 + 记录；
- **Backup**：通过 RDB snapshot + AOF；

## 十五、生态与集成

- **客户端 SDK**：所有 Redis 客户端直接兼容（无需修改）；
- **ORM**：Spring Data Redis、Redisson、redis-py 等；
- **消息队列**：Stream + Lua 实现 Kafka-like；
- **缓存框架**：Spring Cache、Guava Cache、Apache Ignite；
- **容器化**：官方 Docker（`docker.io/dragonflydb/dragonfly`）、Kubernetes 部署；
- **Kubernetes Operator**：社区（`dragonfly-operator`）；
- **BI 工具**：RedisInsight、TablePlus、Another Redis Desktop Manager（兼容）；
- **云服务**：AWS、GCP、Azure 上可部署；

## 十六、变体与衍生项目

- **KeyDB**：Snapchat 出品的多线程 Redis fork（共享内存 + 多线程命令执行，2019 起）；
- **Valkey**：Linux Foundation 旗下 Redis 兼容 fork（2024 年成立）；
- **Microsoft Garnet**：.NET 实现的下一代 KV 服务器（与 Dragonfly 思路相似）；
- **Redis Cluster**：Redis 自带 Cluster 模式（sharding）；
- **Redis Enterprise**：Redis Inc 商业版；
- **AWS ElastiCache for Redis**：AWS 托管 Redis；
- **Azure Cache for Redis**：Azure 托管 Redis；
- **Memcached**：纯 KV 内存缓存（无持久化、无数据类型）；

## 十七、核心论文与算法原理

- **Shared-Nothing Architecture**：分布式系统经典设计哲学（Stonebraker, 1986）；
- **Skip Lists**（Pugh, 1990）：跳表原始论文；
- **io_uring Linux 内核异步 IO**（Linux 5.1+）：Dragonfly 使用 io_uring 替代 epoll；
- **Fibers / Coroutines**：轻量级协程设计；
- **Redis Design Choices**：单线程命令执行的优劣分析；
- **Dragonfly 官方博客**：Roman Gushchin 关于 Dragonfly 设计的深度文章（2022-2024 多篇）；
- **Coroutine vs Thread**：协程与线程的性能分析（社区）；

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `src/server/main.cc`：服务端启动；
  - 阅读 `src/server/server_family.cc`：ServerFamily 入口；
  - 阅读 `src/server/dflycmd.cc`：命令分发；
  - 阅读 `src/server/dispatcher.cc`：Dispatcher 路由；
  - 阅读 `src/server/db_singleton.h`：DB 单例；
- **进阶（2-3 周）**：
  - 阅读 `src/server/shard_set.h`：多 shard 管理；
  - 阅读 `src/server/connection.h` + `src/server/connection.cc`：连接管理；
  - 阅读 `src/server/proactor.h`：Proactor 线程池；
  - 阅读 `src/server/fiber.*`：Fiber 协程实现；
  - 阅读 `src/server/string_family.cc`：String 命令实现；
- **高级（持续）**：
  - 阅读 `src/server/cluster/`: 集群实现；
  - 阅读 `src/server/snapshot.cc`：持久化；
  - 阅读 `src/server/replica.cc`：主从复制；
  - 阅读 `src/server/scripting.cc`：Lua 引擎集成；
  - 阅读 `src/server/compact_object.cc`：Compact + Defrag。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| 服务端启动 | `src/server/main.cc` |
| ServerFamily | `src/server/server_family.cc` |
| Dispatcher | `src/server/dispatcher.cc` |
| ShardSet | `src/server/shard_set.h` |
| DB | `src/server/db_singleton.h` |
| Connection | `src/server/connection.h` |
| Proactor | `src/server/proactor.h` |
| Fiber | `src/server/fiber.h` + `src/server/fiber.cc` |
| String | `src/server/string_family.cc` |
| List | `src/server/list_family.cc` |
| Hash | `src/server/hash_family.cc` |
| Set | `src/server/set_family.cc` |
| ZSet | `src/server/zset_family.cc` |
| Stream | `src/server/stream_family.cc` |
| CompactString | `src/server/compact_string.h` |
| QuickList | `src/server/quick_list.h` |
| Snapshot | `src/server/snapshot.cc` |
| AOF | `src/server/aof.cc` |
| Replica | `src/server/replica.cc` |
| Cluster | `src/server/cluster/` |
| Scripting | `src/server/scripting.cc` |
| ACL | `src/server/acl_family.cc` |
| Pub/Sub | `src/server/pubsub_family.cc` |
| 持久化 RDB | `src/server/rdb_extensions.cc` |
| HTTP 管理 | `src/server/http/` |
| Lua | `src/server/lua/` |

## 二十、源码规模与开发语言

- **LOC 数量**：约 30 万行 C++（核心 `src/server/` + `src/facade/` + 第三方模块）；
- **主要语言**：C++17（核心）、C（少量）、Bash（基准）、Lua（脚本测试）；
- **依赖**：
  - **boost::fiber**（协程库）；
  - **abseil**（Google 工具库）；
  - **io_uring**（Linux 内核异步 IO）；
  - **openssl**（TLS）；
- **构建系统**：CMake（`CMakeLists.txt`）+ Bazel 部分子项目；
- **测试**：标准 C++ testing + 集成测试 + Lua 脚本测试；
- **CI**：GitHub Actions + Docker 构建；

## 二十一、学习资源

- **官方文档**：[https://www.dragonflydb.io/docs](https://www.dragonflydb.io/docs)；
- **官方博客**：[https://www.dragonflydb.io/blog](https://www.dragonflydb.io/blog)（Roman Gushchin 多篇技术博客）；
- **GitHub**：[https://github.com/dragonflydb/dragonfly](https://github.com/dragonflydb/dragonfly)；
- **论文参考**：
  - **io_uring 设计文档**（Linux 内核）；
  - **Boost.Fiber 文档**；
  - **Shared-Nothing Architecture**（Stonebraker）；
  - **Skip Lists**（Pugh）；
- **书籍**：《Redis 设计与实现》（对照参考）、《Linux 高性能服务器编程》（游双，含 io_uring）；
- **YouTube**：Dragonfly 官方频道、Roman Gushchin GopherCon / linux.conf.au 演讲；
- **课程**：CMU 15-721（Database Systems）、MIT 6.584；
- **Reddit / HN**：r/dragonflydb、news.ycombinator.com（"Dragonfly" 话题）。

## 二十二、可移植性与部署

- **支持平台**：Linux（x86_64 / aarch64，必须支持 io_uring）；
- **依赖**：Linux Kernel 5.10+（io_uring 完善）；
- **macOS**：实验性支持（io_uring 不支持，用 epoll 替代）；
- **Windows**：不支持；
- **容器化**：官方 Docker（`docker.io/dragonflydb/dragonfly`）、Helm Chart；
- **Kubernetes Operator**：社区（`dragonfly-operator`）；
- **嵌入式部署**：不支持（服务端，独立进程）；
- **云原生**：完整支持（可作为 StatefulSet 部署）；

## 二十三、风险与陷阱

- **BSL 许可证限制**：商业 SaaS 化分发需商业许可；
- **Linux Kernel 依赖**：必须 5.10+（io_uring 完善），老内核用 epoll fallback 性能下降；
- **多 key 命令延迟**：跨 shard 命令（MSET、MGET、Lua）会触发 barrier，所有 shard 暂停；
- **大 key 瓶颈**：单 shard 单线程，单个大 key 阻塞整个 shard；
- **Lua 脚本限制**：跨 shard Lua 必须 barrier，影响吞吐；
- **pub/sub 全 shard 索引**：大量 subscriber 占用多 shard 内存；
- **内存限制**：单实例内存上限受限于硬件（典型 100GB~1TB）；
- **持久化缺失（7.0 前）**：早期版本仅 Snapshot；AOF 在 8.0+；
- **主从复制丢数据**：异步复制存在主从切换丢数据风险；
- **shard 数量不可动态改**：`num_shards` 启动后固定；
- **小 key 反而不优**：CompactString 在 < 10B key 时优势不明显；
- **缺乏 OS 路径多样性**：仅 Linux + macOS，Windows 用户无法使用；
- **Community 模块缺失**：相比 Redis Modules 生态较小；
- **Cluster 协议兼容性**：与 Redis Cluster 不完全兼容（部分场景需调整）；

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/db/` 中 KV 引擎如需多线程扩展，Dragonfly 的 **shared-nothing + dispatch by hash** 设计值得借鉴；
  - `engineering/include/algo/` 中可参考 **io_uring** 异步 IO；
  - `engineering/src/algo/` 中可借鉴 **Fiber 协程**轻量级任务调度；
- **学习复用价值**：
  - **共享无锁架构**是分布式 KV + 多线程 KV 的经典模式；
  - **io_uring** 是 Linux 高性能 IO 的未来方向；
  - **Fiber-based 并发**比传统线程池更适合 IO 密集型应用；
  - **CompactString 等紧凑编码**对内存优化有借鉴意义；
  - **完全 Redis 兼容**思路（不破坏既有客户端）是兼容性设计的范例。

## 二十六、Shard 路由与 DenseSet：XXH64 取模 + 位标记开放寻址表

> 本节起的源码路径为 **canonical 路径 +（approx）标注**：本仓库 `reference/key-value/dragonfly/` 子模块未 init，无法读本地副本，因此符号位置以 Dragonfly 上游 HEAD 浅克隆逐符号 `grep` 核对（canonical 路径与符号名已确证），**行号需校对**。

### 26.1 key → shard 的路由函数

Dragonfly 不做一致性哈希，也不做 Redis Cluster 式的槽表维护。默认路径就是「一个固定种子的 64 位哈希 + 对 shard 数取模」：

```cpp
// src/server/sharding.cc（approx，模块未 init，行号需校对）dfly::Shard()
ShardId Shard(string_view v, ShardId shard_num) {
  if (IsClusterShardedBySlot())                    // 集群「按槽」模式
    return KeySlot(v) % shard_num;                 // src/server/cluster_support.h
  if (IsClusterShardedByTag())                     // 集群「按 tag」模式
    v = LockTagOptions::instance().Tag(v);         // 先剥出 {} tag 段

  XXH64_hash_t hash = XXH64(v.data(), v.size(), 120577240643ULL);
  if (RoundRobinSharder::IsEnabled()) { ... }      // 见 26.2，热点前缀旁路
  return hash % shard_num;
}
```

要点：

- **哈希函数是 XXH64，不是 CRC16/CRC32**。§7.1 写的 "CRC32(key) mod N" 与上游实现不符，以本节为准；
- 种子 `120577240643ULL` 是硬编码常量。三重作用：(a) 固定种子保证**所有 Dragonfly 实例对同一 key 得到同一相对位置**，跨实例路由可复现；(b) 非零种子使「用 `XXH64(..., 0)` 预计算的碰撞集」失效；(c) XXH64 的雪崩输出使取模后分布接近均匀；
- 复杂度：路由 **O(L)**，L 为 key 字节数（只读一遍 key 算哈希，无额外内存）；
- 源码注释记录了一个反直觉的取舍：**slot 模式的分布性反而更差**——3 个 shard 时 `'~'`（ASCII 最大值）在 slot 表里永远落不到 shard 2，32 个单字符 key 全挤在 shard 0。保留 slot 纯粹是为了配合 pipeline squashing（见 27.8），因为 squashing 依赖命令「必然是单分片的」。

### 26.2 RoundRobinSharder：热点前缀的旁路

可选开关 `--shard_round_robin_prefix`（源码已标注 deprecated）为一个 key 前缀启用 round-robin 分配，专治「少量超热 key」：

```cpp
// src/server/sharding.cc（approx，模块未 init，行号需校对）
constexpr size_t kRoundRobinSize = 100'003;               // 质数，≈200KB/线程

static optional<ShardId> TryGetShardId(string_view key, XXH64_hash_t key_hash) {
  if (!absl::StartsWith(key, round_robin_prefix_)) return nullopt;   // 非本前缀 → 走常规哈希
  size_t index = key_hash % round_robin_shards_tl_cache_.size();

  ShardId sid = round_robin_shards_tl_cache_[index];     // ① 读 thread_local 缓存（无锁）
  if (sid == kInvalidSid) {
    util::fb2::LockGuard guard(mutex_);                  // ② 未命中才加全局锁
    sid = round_robin_shards_[index];
    if (sid == kInvalidSid) {
      sid = next_shard_;
      round_robin_shards_[index] = sid;
      next_shard_ = (next_shard_ + 1) % shard_set_size_; // ③ 轮转分配
    }
    round_robin_shards_tl_cache_[index] = sid;           // ④ 回填本线程缓存
  }
  return sid;
}
```

这是标准的 **per-thread cache + 全局数组** 双层结构：热路径 O(1) 无锁，冷路径 O(1) + 一次 `Mutex`。源码注释给出的容量账：100 个热点 key 下碰撞概率 < 2.5%。内存代价是**每个线程固定 200KB**，所以只在显式开启时才分配。

### 26.3 DenseSet：把哈希表元数据压进指针的高位

DenseSet 是 Dragonfly 全部主表（string / hash / set / zset 的索引）的共同基类。它的桶不是「key 指针 + value 指针」的双字条目，而是**一个 8 字节的 `DensePtr`**，把链表标记、挤入标记、TTL 标记全部编码进**用户态地址空间的空闲高位**：

```
 bit 63                          56 55 54 53 52                              0
┌───────────────────────────────┬──┬──┬──┬─────────────────────────────────┐
│        未使用（12 bit）        │TT│DD│DL│   对象指针 or 链指针（52 bit）  │
└───────────────────────────────┴──┴──┴──┴─────────────────────────────────┘
                              Ttl DisplDir Displ Link
```

```cpp
// src/core/dense_set.h（approx，模块未 init，行号需校对）
static constexpr size_t kLinkBit             = 1ULL << 52;
static constexpr size_t kDisplaceBit         = 1ULL << 53;
static constexpr size_t kDisplaceDirectionBit = 1ULL << 54;
static constexpr size_t kTtlBit              = 1ULL << 55;
static constexpr size_t kTagMask             = 4095ULL << 52;   // 预留高 12 bit

static_assert(sizeof(DensePtr)      == sizeof(uintptr_t));      // 8 字节
static_assert(sizeof(DenseLinkKey)  == 2 * sizeof(uintptr_t));  // 16 字节
```

**52 位是安全的**：x86-64 / aarch64 的 canonical 地址最多用 48 位（aarch64 最多 52 位），高位天然空闲。源码注释直接引用了 arm64 memory model 文档作为依据。

发生冲突时，这个 8 字节槽被**原地升级**为 16 字节：

```cpp
struct DenseLinkKey : public DensePtr {   // 继承被 tag 过的对象指针，再加一个 next
  DensePtr next;                          // could be LinkKey* or Object*
};
```

于是「一个 8 字节槽」同时表达了 key 和链表指针两种语义：**无冲突的桶 8 字节，有冲突的桶在链上才 16 字节**，绝大多数情况下不需要为 key 单独分配 `dictEntry`。

`DenseSet` 是**模板基类**，具体类型通过 7 个纯虚函数注入身份与回收策略：

```cpp
virtual uint64_t Hash(const void* obj, uint32_t cookie) const = 0;
virtual bool     ObjEqual(const void* left, const void* right, uint32_t right_cookie) const = 0;
virtual size_t   ObjectAllocSize(const void* obj) const = 0;      // 用于 obj_malloc_used_ 统计
virtual uint32_t ObjExpireTime(const void* obj) const = 0;
virtual void     ObjDelete(void* obj) const = 0;
virtual void*    ObjectClone(const void* obj, bool has_ttl, bool add_ttl) const = 0;
```

### 26.4 桶定位：高位取模代替 `%`

```cpp
// src/core/dense_set.h（approx，模块未 init，行号需校对）
uint32_t BucketId(uint64_t hash) const {
  assert(capacity_log_ > 0);
  return hash >> (64 - capacity_log_);   // capacity_log_ = log2(entries_.size())
}
```

容量**恒为 2 的幂**：`Reserve()` 走 `absl::bit_ceil(sz)` 后设 `capacity_log_ = absl::bit_width(sz) - 1`；`AddUnique` 扩容时直接 `entries_.resize(prev_size * 2); ++capacity_log_;`。因此取模退化成**一次移位**，无除法指令、无分支。起始容量 `kMinSizeShift = 2` → 4 桶。

选高位而非低位，是因为 XXH64 的雪崩使**高位与低位质量相当**，而高位提取天然与「扩容后元素只会往高桶移动」配合（见 26.7）。

### 26.5 邻桶挤入（displacement）：把满载空槽率从 36% 压到 ~21%

```cpp
// src/core/dense_set.cc（approx，模块未 init，行号需校对）
constexpr bool kAllowDisplacements = true;

auto DenseSet::FindEmptyAround(uint32_t bid) -> ChainVectorIterator {
  ExpireIfNeeded(nullptr, &entries_[bid]);
  if (entries_[bid].IsEmpty()) return entries_.begin() + bid;      // ① 本桶空
  if (!kAllowDisplacements)      return entries_.end();

  if (bid + 1 < entries_.size()) {                                // ② 右邻空
    auto it = next(entries_.begin(), bid + 1);
    ExpireIfNeeded(nullptr, &(*it));
    if (it->IsEmpty()) return it;
  }
  if (bid) {                                                      // ③ 左邻空
    auto it = next(entries_.begin(), bid - 1);
    ExpireIfNeeded(nullptr, &(*it));
    if (it->IsEmpty()) return it;
  }
  return entries_.end();                                          // 都占 → 只能走链或扩容
}
```

三个约束缺一不可：

1. **只有 3 个探测位**（bid、bid+1、bid-1），不做更长的线性探测；
2. **邻桶仅当它是「空桶」而非「非空链」时才可挤入**——挤进一条链会让链变得更长，收益为负；
3. 挤入的元素打 `SetDisplaced(±1)` 标记（方向存在 `kDisplaceDirectionBit`），`Find` / `Grow` / `Shrink` 遇到 displaced 元素会把它**送回正确的桶**。

净效果：源码注释给出的对照是「满载时空槽率从 36% 降到 ~21%」。这是 DenseSet 内存优于 Redis dict 的**主要来源**（见 26.9）。

### 26.6 AddUnique：探测 → 扩容 → displaced 归位

```cpp
// src/core/dense_set.cc（approx，模块未 init，行号需校对）
void DenseSet::AddUnique(void* obj, bool has_ttl, uint64_t hashcode) {
  if (entries_.empty()) { capacity_log_ = kMinSizeShift; entries_.resize(kMinSize); }
  uint32_t bucket_id = BucketId(hashcode);

  for (unsigned j = 0; j < 2; ++j) {                     // ★ 最多扩一次容
    auto list = FindEmptyAround(bucket_id);
    if (list != entries_.end()) {
      obj_malloc_used_ += PushFront(list, obj, has_ttl);
      if (std::distance(entries_.begin(), list) != bucket_id)
        list->SetDisplaced(std::distance(entries_.begin() + bucket_id, list));   // ±1
      ++size_;
      return;
    }
    if (size_ < entries_.size()) break;                  // 表满但有空间 → 本轮只允许链式插入
    size_t prev_size = entries_.size();
    entries_.resize(prev_size * 2); ++capacity_log_;
    Grow(prev_size);
    bucket_id = BucketId(hashcode);
  }

  // 本桶非空：先把占位的 displaced 元素搬回它自己的桶，循环直到本桶是真链头。
  // 不必维护 displaced 栈——只需跟踪「当前这一个」 displaced 元素，
  // 若正确桶里又藏着 displaced 元素，摘出来继续处理。
  DensePtr to_insert(obj);
  if (has_ttl) { to_insert.SetTtl(true); expiration_used_ = true; }
  while (!entries_[bucket_id].IsEmpty() && entries_[bucket_id].IsDisplaced()) { ... }
  PushFront(entries_.begin() + bucket_id, to_insert, has_ttl);   // 升级为 16B 链节点
}
```

三个值得注意的设计：

- `for (j = 0; j < 2)` 的上界是 **2 而不是 while(true)**：最多扩一次容。若扩完仍探测不到空位，直接走链式插入——把最坏情况从「无限重试」钉死为「一次探测 + 一次链插入」，**摊还复杂度仍是 O(1)，但最坏延迟有硬上界**；
- displaced 元素的归位用**单变量循环**而非显式栈：算法上等价于把 displaced 元素在桶间的「置换链」解开，只需记住当前一个；
- `obj_malloc_used_` 在插入时就累加对象的 `ObjectAllocSize`，使 `MEMORY` / 统计不必遍历全表。

### 26.7 Grow：单向扫描的原地 rehash

```cpp
// src/core/dense_set.cc（approx，模块未 init，行号需校对）
void DenseSet::Grow(size_t prev_size) {
  DensePtr first;
  // 特例：bucket 0 的 displaced 元素会「向上」流到 bucket 1，而 bucket 1 的
  // displaced 元素又会「向下」流回 bucket 0，形成环。所以先把 bucket 0 的
  // displaced 元素暂存，最后再送回它真正的桶。
  if (entries_.front().IsDisplaced()) first = PopPtrFront(entries_.begin());

  for (long i = prev_size - 1; i >= 0; --i) {                    // ★ 从高桶扫到低桶
    DensePtr* curr = &entries_[i];  DensePtr* prev = nullptr;
    do {
      if (ExpireIfNeeded(prev, curr)) { if (prev && !prev->IsLink()) break; }
      if (curr->IsEmpty()) break;
      void*  ptr = curr->GetObject();
      uint32_t bid = BucketId(ptr, 0);

      if (bid == i) {                                            // 原地不动：清 displaced 标记
        curr->ClearDisplaced();
        prev = curr; curr = curr->Next(); if (!curr) break;
      } else {                                                   // 摘下 → 挂到正确的桶
        auto dest = entries_.begin() + bid;
        DensePtr dptr = *curr;
        // 若 prev 原是 link，顺带降级回裸 DensePtr 并 free 掉那个 link
        ...
      }
    } while (true);
  }
  if (!first.IsEmpty()) PushFront(entries_.begin() + BucketId(first.GetObject(), 0), first);
}
```

**扫描方向是算法正确性的核心，不是风格选择**：扩容后 `BucketId = hash >> (64 - capacity_log_)` 取的 bit 变少了，于是对任意元素必有 `bid_new >= bid_old`。从高到低扫时，一个元素被搬到 `bid_new` 落点后，该位置 `bid_new > i` **已经在本轮扫描范围之外**，因此不会被二次处理，也不会污染尚未扫描的低桶。只有 `bid == 0` 的 displaced 元素可能反向流回 bucket 0，所以开头特判暂存、结尾回填。

`Shrink` 反向印证了这个论证：

```cpp
// 源码注释原文：Process from low to high (opposite of Grow).
// This prevents double-processing: when moving elements from bucket i to bucket j < i,
// bucket j has already been processed, so the element won't be processed again.
```

**缩容从低到高、扩容从高到低**——两个方向刚好互补，都是为了让「元素只会单向流动」的不变量成立。`ShrinkBucket` 里还有一条值得学的细节：它**先无条件 free 掉 link**，再由 `PushFront` 按需重建，避免链节点在新旧布局间来回搬运。

复杂度：`Grow` 单次 O(entries_.size()) = O(N/α)，摊到 N 次插入上是 **O(1) 均摊**；空间 `entries_.size() * 8 + num_links_ * 16` 字节。`__builtin_prefetch(x, 0, 1)` 用于查找路径的访存预取。

### 26.8 StringMap / StringSet 的键值布局

两者都**直接继承 `DenseSet`**（`class StringMap : public DenseSet`、`class StringSet : public DenseSet`），共用 26.3~26.7 的桶协议。键值打包在一块 sds 内存里：

```cpp
// src/core/string_map.cc（approx，模块未 init，行号需校对）
constexpr uint64_t kValTtlBit = 1ULL << 63;

pair<sds, uint64_t> CreateEntry(string_view field, string_view value,
                                uint32_t time_now, uint32_t ttl_sec) {
  sds newkey;
  size_t meta_offset = field.size() + 1;
  if (ttl_sec == UINT32_MAX) {
    // 布局：field | '\0' | 8 字节 tagged value 指针
    newkey = AllocSdsWithSpace(field.size(), 8);
  } else {
    // 布局：field | '\0' | 8 字节 tagged value 指针 | 4 字节绝对过期时间
    newkey = AllocSdsWithSpace(field.size(), 8 + 4);
    uint32_t at = time_now + ttl_sec;
    absl::little_endian::Store32(newkey + meta_offset + 8, at);   // 跳过 value 指针
    sdsval_tag |= kValTtlBit;                                    // ← 复用第 63 位当 TTL 标志
  }
  absl::little_endian::Store64(newkey + meta_offset, sdsval_tag);
  return {newkey, sdsval_tag};
}
```

三个设计点：

1. **value 指针内嵌在 key 的 sds 尾部**，key 与其 value 一次分配 → 遍历 map 时天然按 key 聚簇，单节点的内存碎片显著更少；
2. **过期时间存绝对时间戳**（`time_now + ttl_sec`）而非相对偏移，读时判断只需 `expire <= time_now`，不依赖「谁在什么时候写的」；
3. **TTL 标志复用 tag 的第 63 位**（`kValTtlBit`）。注意它与 DensePtr 的 `kTtlBit`（第 55 位）**在不同的 bit 平面上**：一个标在桶指针上，一个标在 value 指针上，互不干扰。

> 需要澄清一个常见误解：Dragonfly 的 `StringMap` / `StringSet` **既不是 Robin Hood hashing，也不是独立的开放寻址表**。二者都直接 `public DenseSet`，完全复用上面的桶协议与 displacement 策略。对上游做 `grep -i "robin.hood"` 是**零命中**的。

### 26.9 内存账：为什么比 Redis dict 省

源码头部注释直接给出了对照（**不含 key 本身的空间**）：

| 结构 | 100% 装载 | 75% 装载 |
|---|---|---|
| Redis `dictEntry`（24B + 8B） | `N*24 + N*8` = **32N** 字节 | `N*1.33*8 + N*24` = **35N** 字节 |
| DenseSet | `N*8 + 0.2N*16` = **11.2N** 字节 | `N*1.33*8 + 0.12N*16` = **13N** 字节 |

差距来自三处：

- **(a) 条目大小**：无冲突桶只有 8B，而 `dictEntry` 是 24B 且按 8B 对齐后实际占 32B；Set 类型里 `dictEntry` 的 value 字段纯属浪费；
- **(b) 无二次分配**：Redis dict 需要为每次插入额外分配一个 `dictEntry`，DenseSet 直接把对象指针存进桶；
- **(c) 装载率**：displacement 把满载空槽率从 36% 压到 ~21%，所以 100% 装载只需 `0.2N` 个 link 而不是 `0.36N` 个——每条 link 是 16B，这就是 11.2N 与 13.6N 的差距来源。

从两行公式还能读出一个反直觉结论：**塞满（11.2N）比留 25% 空（13N）更省**。因为留空时桶数组本身被放大到 `N/0.75 = 1.33N` 个槽，多出的 `0.33N × 8B ≈ 2.7N` 是纯成本。所以「省内存」的正确姿势恰恰是**把装载率顶到接近 1**——这正是 displacement 存在的全部意义。

## 二十七、DASH 可扩展哈希表与事务 hop 调度

### 27.1 两种哈希表的分工

| | DenseSet | DASH |
|---|---|---|
| 路径 | `src/core/dense_set.h` | `src/core/dash.h` + `src/core/dash_internal.h` |
| 全称 | — | **D**ynamic **A**nd **S**calable **H**ashing |
| 结构 | 单桶开放寻址 + 短链 + 邻桶挤入 | 分段可扩展哈希（extendible hashing） |
| 并发 | 单线程（每 shard 独占一线程） | 支持多线程并发扩容 |
| 扩容 | 原地 `Grow` 全表 rehash，**阻塞整个 shard** | `Segment::Split`，只搬 ≤816 个 key |
| 用途 | 主表：string / hash / set / zset 的索引 | 冷数据索引：tiered storage 的 `EntryMap` 等 |

分工的理由很直接：主表是**单线程独占**的，可以放心做原地全表 rehash；而分层存储的外部条目索引需要**跨线程并发读写**，承担不起停表式扩容。

### 27.2 DASH 的分段结构与扩展

```cpp
// src/core/dash_internal.h（approx，模块未 init，行号需校对）
struct DefaultSegmentPolicy {
  static constexpr unsigned kSlotNum   = 12;      // 每个 bucket 12 个 slot
  static constexpr unsigned kBucketNum = 64;      // 每个 segment 64 个常规 bucket
  static constexpr bool      kUseVersion = true;  // VersionedBB：slot 带版本号做 CAS
};

class Segment {
  static constexpr unsigned kStashBucketNum = 4;                  // 另有 4 个 stash bucket
  static constexpr unsigned kTotalBuckets   = kBucketNum + kStashBucketNum;   // 68
  static constexpr size_t   kMaxSize        = kTotalBuckets * kSlotNum;      // 816

  Bucket  bucket_[kTotalBuckets];
  uint8_t local_depth_;      // 本段能区分多少 bit
  uint32_t segment_id_;
  uint32_t live_iterators_{0};   // 有多少迭代器持有本段指针 → 决定能否安全 realloc
  void Split(HashFn&& hfunc, Segment* dest, OnMoveCb&& on_move_cb);
};

uint32_t DashTableBase::SegmentId(size_t hash) const {
  return global_depth_ ? (hash >> (64 - global_depth_)) : 0;   // 高 gd bit 选段
}
```

**分段可扩展哈希的工作方式**：`global_depth_` 决定用 hash 的高 `gd` 位索引段。若某段的 `local_depth_ > global_depth_`，说明它与 `segment_id ^ (1 << (local_depth_ - global_depth_))` 处的兄弟段共享了同一段前缀前缀，键空间已无法区分——此时该段被标记为**待分裂**；插入触发 `Split`，把多出来的那个 bit 用上（`local_depth_++`，必要时 `global_depth_++`）。

表刚建时 `global_depth_ = 0`，所有 key 都落进唯一那一段；一直填到 `kMaxSize = 816` 才分裂并抬高 `global_depth_`。

**摊还复杂度**：可扩展哈希的摊还插入是 **O(1)**，且扩容是**增量**的——单次分裂只搬一个 segment 的至多 816 个 key，不影响其他段。对比 DenseSet 的 `Grow`：那是要停下来扫完整个 `entries_` 的 O(N) 操作，在百万 key 的 shard 上就是几十毫秒的停顿。

### 27.3 段税（tax）：stash bucket 的空间代价

```cpp
// src/core/dash_internal.h（approx，模块未 init，行号需校对）
static constexpr double kTaxSize = (double(sizeof(Segment)) / kMaxSize)
                                 - sizeof(Key_t) - sizeof(Value_t);
```

`kTaxSize` 就是**每个 key 平均被段级开销吃掉的字节数**：把 `Segment` 的全部字段（`local_depth_`、`segment_id_`、`mr_`、`live_iterators_`、每个 bucket 的 `SlotBitmap`）折算成每 key 摊销值，再扣掉 key/value 自身。

stash bucket 是为「长尾 key 全撞进同一个 bucket」准备的溢出区，但它要在**每个 segment 上都占 4 个 bucket**（`4/68 ≈ 5.9%` 的 bucket）。对短 key 小表，这笔税相当可观——所以 DASH 更适合条目较大、段数较少的场景。

### 27.4 SlotBitmap：把「占用 / 探测中 / 计数」压进一个 32 位字

这是 DASH 最精巧的一处。一个 bucket 需要同时维护三组信息：哪些 slot 被**占用**（owning，该 key 真正属于本 bucket）、哪些 slot 正在**探测**（probing，key 不属于本 bucket 但暂存于此、尚未决定去向）、以及占用总数。全部塞进 1~2 个 `uint32_t`：

```cpp
// src/core/dash_internal.h（approx，模块未 init，行号需校对）
template <unsigned NUM_SLOTS> class SlotBitmap {
  static_assert(NUM_SLOTS > 0 && NUM_SLOTS <= 28);
  static constexpr bool      SINGLE       = NUM_SLOTS <= 14;    // ≤14 用 1 个 uint32，否则 2 个
  static constexpr unsigned  kLen         = SINGLE ? 1 : 2;
  static constexpr unsigned  kAllocMask   = (1u << NUM_SLOTS) - 1;
  static constexpr unsigned  kBitmapLenMask = (1 << 4) - 1;    // 计数占低 4 bit（≤15）

  struct Unaligned { uint32_t d __attribute__((packed, aligned(1))); };
  Unaligned val_[kLen];
};
```

两种位布局：

```
// SINGLE（NUM_SLOTS ≤ 14），恰好用满 32 bit，无一 waste：
 bit 31 .............. 18   bit 17 ............. 4   bit 3 .......... 0
┌───────────────────────┬───────────────────────┬────────────────┐
│      busy     (14b)   │    probing   (14b)    │  count   (4b)   │
└───────────────────────┴───────────────────────┴────────────────┘

// kLen == 2（14 < NUM_SLOTS ≤ 28）：
 val_[0].d : bit 31 ............ 4 = busy(28b)，count = __builtin_popcount(val_[0].d)
 val_[1].d : bit 31 ............ 4 = probing(28b)
```

访问器把「双份数据」折叠成单次取值：

```cpp
uint32_t GetBusy() const { return SINGLE ? val_[0].d >> 18 : val_[0].d; }

uint32_t GetProbe(bool probe) const {
  if constexpr (SINGLE) return ((val_[0].d >> 4) & kAllocMask) ^ ((!probe) * kAllocMask);
  else                  return (val_[1].d & kAllocMask)         ^ ((!probe) * kAllocMask);
}

int FindEmptySlot() const {                 // 前提：确有空 slot
  uint32_t mask = ~(GetBusy());
  return __builtin_ctz(mask);               // 最低位的 0 = 第一个空 slot
}
unsigned Size() const { return SINGLE ? (val_[0].d & kBitmapLenMask)
                                      : __builtin_popcount(val_[0].d); }
```

四个值得学的技巧：

1. **`GetProbe(bool)` 用异或取补**：`probe` 为 true 时 `(!probe)*kAllocMask == 0`，`X ^ 0 == X`；为 false 时是全 1 掩码，`X ^ 0xFFFF == ~X`。一次 XOR 取代了 `probe ? probing : busy` 的分支与选择，且完全编译期消解；
2. **计数用位段而非独立字段**：单路布局下 4 bit 计数（上限 15，与 `kSlotNum ≤ 14` 自洽），双路布局下直接 `popcount(busy)`——「数量」本来就是「位图里 1 的个数」，单独存是冗余；
3. **`__builtin_ctz(~GetBusy())`**：找第一个空 slot 是单条 `tzcnt` 指令，O(1) 无分支；
4. **`__attribute__((packed, aligned(1)))` 强制非对齐 uint32**。源码注释说明这是为了「说服编译器生成非对齐访问指令」——否则编译器会假设 4 字节对齐，插入对齐检查甚至拆成多条指令。有了它，`VersionedBB<N>` 才能自由排布：`static_assert(sizeof(VersionedBB<12>) == 12 * 2 + 8)`（slot 本身 2 字节 + 每 bucket 一个 8 位计数器）。

### 27.5 stash bucket 的索引映射

```cpp
// src/core/dash_internal.h（approx，模块未 init，行号需校对）
for (unsigned i = 0; i < kStashBucketNum; ++i) {
  unsigned     stash_pos = (bid + i) % kStashBucketNum;    // ★ 用 bid 参与，不是裸 i
  PhysicalBid  stash_bid = kBucketNum + stash_pos;         // 映射到后 4 个物理桶
}
```

**用 `(bid + i) % 4` 而不是 `i`**：不同常规 bucket 的溢出会被打散到不同 stash 位，降低二次冲突率。

stash 区的容量硬上限写在源码注释里：`BucketBase::kStashFpLen = 4` 与 `kStashPresentBit = 1 << 4`——stash 区的指纹存在**一个字节**中，4 个 bucket × 2 bit 只能指向 4 个 stash 桶，所以「最多 4 个 stash 桶」不是随手取的常数，而是**被字节宽度的位预算反推出来的上限**（同理有 `static_assert(kBucketNum + kStashBucketNum < 255)`，因为 `PhysicalBid` 是 `uint8_t`）。

### 27.6 事务 hop：命令如何落到 shard 线程

Dragonfly 不做全局锁。每个命令被包成一个 `Transaction`，拆成若干 **hop**（回调），逐个派发到相关 shard 的 fiber 队列执行：

```cpp
// src/server/transaction.h（approx，模块未 init，行号需校对）
using RunnableType = absl::FunctionRef<RunnableResult(Transaction*, EngineShard*)>;

struct RunnableResult {
  enum Flag : uint8_t {
    // 单个 shard 回调可置位以「拒绝收尾」，让本次 hop 之后再跑一跳（阻塞命令的 fallback）
    AVOID_CONCLUDING = 1,
  };
  OpStatus status;  uint16_t flags;
};
static_assert(sizeof(RunnableResult) == 4);
```

回调的**两条硬性约定**（源码注释原话）：一是不允许持有指向 shard 内数据的指针——那些数据下一跳就可能失效，必须先拷贝出来；二是运行在不同线程上的回调**绝不能互相传消息或互相等待**，否则会阻塞其他事务的 hop。参与 hop 的 shard 集合由命令的 key 决定，global 事务则落到所有 shard。

派发原语：

```cpp
// src/server/engine_shard_set.h（approx，模块未 init，行号需校对）
template <typename F> auto Await(ShardId sid, F&& f) {     // 派发并等待结果
  return shards_[sid]->GetFiberQueue()->Await(std::forward<F>(f));
}
template <typename F> auto Add(ShardId sid, F&& f) {        // 入队，不等
  return shards_[sid]->GetFiberQueue()->Add(std::forward<F>(f));
}
// Runs a brief function on all shards. Waits for it to complete. `func` must not preempt.
template <typename U, typename P> void RunBriefInParallel(U&& func, P&& pred);
// Runs a possibly blocking function on all shards. Waits for it to complete.
template <typename U, typename P> void RunBlockingInParallel(U&& func, P&& pred);
```

**Brief 与 Blocking 的区分是 fiber 调度的硬约束，不是命名约定**：`RunBriefInParallel` 要求回调**全程不抢占**（不 `io_uring` 等待、不 fiber 切换），因此它实际只是一次串行的「遍历 shard 逐个调用」，并不真正并行；`RunBlockingInParallel` 才是 fan-out 后逐个 join。快照保存（28.5）用的正是后者——序列化每个 shard 要写磁盘，必须允许抢占。

### 27.7 队列与意图锁：TxQueue + IntentLock

```cpp
// src/core/tx_queue.h（approx，模块未 init，行号需校对）
// TxQueue 实现为循环双向链表（数组化存储）
using ValueType = std::variant<Transaction*, uint64_t>;
using Iterator  = uint32_t;
static constexpr Iterator kEnd = Iterator(-1);

Iterator      Insert(Transaction* t);
uint64_t      TailScore() const { return Rank(vec_[vec_[head_].prev]); }   // 队尾事务的时间戳
uint64_t      HeadScore() const { return Rank(vec_[head_]); }
```

队列按**事务开始时间**排序，队首是最老的等待者——这就是「先来先服务」的公平性来源，也是阻塞命令（WATCH / BLPOP）能挂住的前提。

每个 key 上挂一把 `IntentLock`：

```cpp
// src/core/intent_lock.h（approx，模块未 init，行号需校对）
enum Mode { SHARED = 0, EXCLUSIVE = 1 };

bool Acquire(Mode m) {                       // 返回是否真的拿到（无论如何都会记账）
  ++cnt_[m];
  if (cnt_[1 ^ int(m)]) return false;        // 对面模式已有人 → 拿不到
  return m == SHARED || cnt_[EXCLUSIVE] == 1;
}
bool Check(Mode m) const {
  unsigned s = cnt_[EXCLUSIVE];
  if (s) return false;
  return (m == SHARED) ? true : cnt_[SHARED] == 0;
}
unsigned ContentionScore() const { return cnt_[EXCLUSIVE] * 256 + cnt_[SHARED]; }
```

三点：

- **`ContentionScore` 用 `EXCLUSIVE << 8 | SHARED` 把两个计数器压成单个可比较的序数**。这是廉价但正确的优先级编码：EXCLUSIVE 的权重 256 严格大于 SHARED 的最大可能值 255，因此「任何有排他锁的情况分数 > 任何纯共享锁的情况」，排序语义无歧义；
- **只读命令拿 SHARED 锁，因此彼此完全不互斥**——这是 Dragonfly 「百万 QPS」的重要来源，Redis 单线程靠的是不并发，Dragonfly 靠的是读锁可共享；
- 源码注释点名了队列与锁的关系：「Transactions at the head of tx-queue are considered to be the ones that acquired the lock」——只有队首事务的锁申请才被放行，配合 `OUT_OF_ORDER` 标记支持部分乱序放行以提升并行度。

### 27.8 多命令 squash：把 N 条命令压成 1 个 hop

pipeline 里的一串单分片命令（`SET a 1 / SET b 2 / GET a`）本来要 3 个 hop，也就是 3 次跨线程调度 + 3 轮锁获取。**Squashing** 把它压成 1 个：

```cpp
// src/server/transaction.h（approx，模块未 init，行号需校对）
enum MultiMode : uint8_t { NOT_DETERMINED = 0, GLOBAL = 1, LOCK_AHEAD = 2, NON_ATOMIC = 3 };
enum MultiRole : uint8_t { DEFAULT = 0, SQUASHER = 1, SQUASHED_STUB = 2, SHARD_LOCAL = 3 };

// 源码注释给出的流程 ① 原子 squash（LOCK_AHEAD / GLOBAL 模式）：
for (cmd in single_shard_sequence) sharded[shard].push_back(cmd);
tx->PrepareSquashedMultiHop();
tx->ScheduleSingleHop({
    Transaction stub_tx{tx};                       // 每个 shard 一个「桩事务」
    for (cmd) { stub_tx.MultiSwitchCmd(cmd);        // 1. 换命令
                stub_tx.InitByArgs(args);           // 2. 重载参数
                cmd->Invoke(stub_tx); }             // 3. 直接执行，无调度
});

// 流程 ② 非原子 squash（NON_ATOMIC）：不压事务层。
// MultiCommandSquasher 把每条命令**各自**直投目标 shard 队列，
// 靠一个真实的、普通 SHARD_LOCAL 事务驱动——它全程钉在单 shard 上，
// 照常 schedule / 取锁，只是生命周期内只涉及一个 shard。
```

四个算法要点：

- **stub transaction 不做任何调度**，只是 mock 出真实事务的接口，让命令代码**零改动**地跑在单 shard 上。因此**只有已经证明是单分片的命令才能被 squash**——这正是 26.1 里保留槽模式的原因；
- `MultiRole::SHARD_LOCAL` 让 `shard_data_` 只占**一个格子**，而不是按 `ShardId` 索引的小 vector（源码注释："lets shard_data_ use a single cell"）。非原子 squash 的本质收益就在这里：省掉了每条命令一次的 hop 开销；
- `PrepareSquashedMultiHop` 只兼容「提前拿到全部锁」的模式（GLOBAL / LOCK_AHEAD），因为原子 squash 必须在一条 hop 内完成全部命令，中途不能被别的线程插入；
- `CancelScheduledTx()` 让这条路径具备**可撤销性**，见 27.9。

**摊还收益**：pipeline 深度 P 时，hop 数从 **P 降到 1**，跨线程唤醒从 P 次降到 1 次，锁获取从 `P × |keys|` 降到 `|shards|`。这是 Dragonfly 在 pipeline 场景下「10M QPS」的直接来源，也是 13 节性能数据的机制解释。

### 27.9 可撤销调度：CancelScheduledTx 的两阶段回退

```cpp
// src/server/transaction.h（approx，模块未 init，行号需校对）
// 尝试原子地 disarm 所有 active shard。
// 若全部成功 disarm → 事务取消，从各 shard 队列移除。
// 若任一 shard 已经执行（无法 disarm）→ 把先前 disarm 的重新 arm，事务照常跑完。
// 必须在 coordinator 线程、DispatchHop() 之后、run_barrier_.Wait() 之前调用。
bool CancelScheduledTx();
```

这是严格的**两阶段撤销**：结局只有「全部取消」或「全部回退」，没有中间态。每个 shard 上的位标记承担 arm/disarm 的原子性：

```cpp
enum LocalMask : uint8_t {
  ACTIVE               = 1,        // 本 shard 是否 active（要调度或执行 hop）
  OPTIMISTIC_EXECUTION = 1 << 1,   // 本 shard 是否已乐观执行（调度期间）
  OUT_OF_ORDER         = 1 << 2,   // 是否允许乱序（仅在 KEYLOCK_ACQUIRED 下有意义）
  KEYLOCK_ACQUIRED     = 1 << 3,   // key 锁是否已获取（global 命令永不清此位）
  WAS_SUSPENDED        = 1 << 4,   // 曾被 WatchInShard 挂起（粘性，一置永置）
  AWAKED_Q             = 1 << 5,   // 是否已被 NotifySuspended 唤醒
};
uint16_t DisarmInShard(ShardId sid);   // 已 arm 则返回 local mask（ACTIVE 恒置位），未 arm 返回 0
```

复杂度 O(#涉及 shard)，全程无锁（单个 `uint16_t` 的 CAS）。回退路径的存在保证了**取消失败也不会破坏正确性**——事务要么真的没跑，要么正常跑完，不存在「跑了一半」。

## 二十八、Journal 段式持久化与多流复制

### 28.1 一次写入的三份拷贝

理解 Dragonfly 持久化与复制的前提是看清「一份命令落到三处」：

```
        命令在 shard s 上执行
                 │
                 ├─► DenseSet 主表 ─────────────────────► 内存，权威数据
                 │
                 ├─► JournalSlice 环形缓冲 ────────────► 内存，每 shard 一个，LSN 单调递增
                 │        ├─ 消费者 1：AOF 写盘（aof_streamer）
                 │        ├─ 消费者 2：复制流（replica）
                 │        └─ 消费者 3：slot 迁移（cluster）
                 │
                 └─► 标记 dirty ──周期性──► .dfs 快照 ───► 磁盘，全量
```

**journal 优先于快照**。快照是「整表序列化」，journal 是「增量变更流」：**复制全程走 journal，快照只用于全量同步**。因此 Dragonfly 的持久化谱系不是「RDB + AOF 二选一」，而是「journal（增量/复制）+ 快照（基线）」的统一流，`.dfs` 只是这个流的冷启动基线。

### 28.2 JournalSlice：LSN 环形缓冲与两段式刷出

```cpp
// src/server/journal/journal_slice.h（approx，模块未 init，行号需校对）
class JournalSlice {
  boost::circular_buffer_space_optimized<JournalItem> ring_buffer_;  // 按【字节】而非条数计容
  LSN      lsn_ = 1;          // 恒为「下一条」的 LSN
  LSN      resume_bound_ = 1; // 最后一个用户消失时打的界；见 28.3
  uint32_t user_count_ = 0;
  mutable util::fb2::SharedMutex cb_mu_;   // 防止调用期间消费者被摘除
  std::list<std::pair<uint32_t, JournalConsumerInterface*>> journal_consumers_arr_;
  bool     enable_journal_flush_ = true;
  uint32_t max_age_ms_ = 0;    size_t max_bytes_ = 0;   // 时间 + 容量双阈值驱逐
  size_t   ring_buffer_bytes_ = 0;
};
```

两个细节：

- **`space_optimized` 按字节计容**。按条数计容时，一条 1KB 的命令和一条 100B 的命令占同样的槽位，内存上界完全不可控；按字节计容才能给出「ring buffer 最多吃 N MB」这种可推理的承诺；
- **LSN 是每 shard 独立的单调计数**，不是全局的。这不是实现偷懒，而是 28.6 多流复制能成立的前提——一条复制流只要负责固定的 shard 子集，它的字节序列就确定且自洽，`num_flows` 取多少都不影响语义。

**两段式刷出协议**是这里最精巧的部分：

```cpp
// src/server/journal/journal_slice.h（approx，模块未 init，行号需校对）注释原意
// SetFlushMode(allow_flush=false)：在随后的 AddLogRecord 调用中禁用抢占
// SetFlushMode(allow_flush=true) ：把自上次 false 以来聚合的全部日志记录一次性刷出，
//                                  此调用【可能】抢占
// 调用方必须保证两次调用之间不发生抢占。返回之前的模式以便恢复；重复设置当前模式是 no-op。
bool SetFlushMode(bool allow_flush);
```

配套的 RAII 包装：

```cpp
// src/server/journal/journal.h（approx，模块未 init，行号需校对）
class DisableFlushGuard {
 public:
  DisableFlushGuard() : prev_flush_(SetFlushMode(false)) {
    util::fb2::detail::EnterFiberAtomicSection();
  }
  ~DisableFlushGuard() {
    util::fb2::detail::LeaveFiberAtomicSection();
    SetFlushMode(prev_flush_);          // 嵌套时只有最外层真正重新开 flush
  }
 private:
  bool prev_flush_;   // 记住自己发现的模式 → 嵌套天然正确
};
```

**为什么非它不可**：一次事务要往 journal 写 N 条记录。如果每条写完都可能因消费者背压而抢占（fiber 让出线程），事务就会在中途被切走——此时它**已经改了内存主表却还没写完 journal**，一旦崩溃，内存与 journal 就不一致。`DisableFlushGuard` 把「改内存 + 写 journal」整段变成一个 **fiber 原子段**，全部完成后再在单点批量刷出、允许抢占。

`prev_flush_` 字段让**嵌套守卫自动正确**：内层析构时恢复的是外层设置的 `false`，只有最外层析构才真正重新开启 flush。

复杂度：`AddLogRecord` **O(1) 摊还**（环形缓冲 + space_optimized 的批量回收）；一次刷出 O(本批条数)，且最多抢占一次。

### 28.3 Journal 生命周期：引用计数而非常驻

```cpp
// src/server/journal/journal.h（approx，模块未 init，行号需校对）
// 以下函数必须在所属 shard 的上下文中调用
void AcquireUser(bool start_journal = true);   // 在复制 setup / slot 迁移 setup / 部分同步回放期间持有
void ReleaseUser();                            // 计数归零 → 打上 resume_bound_ → journal 可以停了
void StartInThreadAtLsn(LSN lsn);             // 从指定 LSN 启动，同时丢弃（重置）部分同步缓冲
void ClearBuffer();                            // 丢弃本 shard 的部分同步缓冲，并作废所有副本已观测的 LSN
```

Dragonfly 的 journal **不是常开的**：没有副本时 `user_count_ == 0`，journal 停止累积、几乎零开销。这与 Redis 的 AOF 有本质区别——Dragonfly 把 journal 当作「**为复制服务的、有生命周期的事件流**」，AOF 只是它注册的**一个消费者**，而不是「为持久化服务的、无条件写盘日志」。

停止判据是双条件（源码注释）：`resume_bound_` 之后的所有条目都已被消费完 **且** 当前没有用户，此时才允许停 journal——因为再没有副本能用得上它了。

### 28.4 AOF 段写：崩溃安全的发布协议

Dragonfly 的 AOF 是**分段（segment）**的，每段一个 `AofSegmentWriter`：

```cpp
// src/server/journal/aof_segment_writer.h（approx，模块未 init，行号需校对）
// 写一个 AOF 段。非线程安全：包括 Open 和 Shutdown 在内的所有调用，
// 必须在同一个 proactor 线程上执行。
class AofSegmentWriter {
  // 创建段 seq 并落一个 durable header：tmp → header → fdatasync → link → fsync dir（同步）
  std::error_code Open(uint64_t seq);

  void AddRecord(std::string_view record, uint64_t lsn);   // 非阻塞
  void Seal();                                            // 封段，禁止再写

  std::error_code Shutdown();   // 等待 in-flight I/O 并 fdatasync 该段；Open 失败时不可调用
  void WaitPending(size_t limit);  // 阻塞当前 fiber，直到 PendingBytes() <= limit

  uint64_t WrittenLsn() const;   // 最后一条【已写入】记录的 LSN（不一定持久）
  uint64_t DurableLsn() const;   // 最后一条被成功 fdatasync 覆盖的记录的 LSN
  std::error_code error() const; // 首次写或 fdatasync 失败；一旦出错 writer 永久停止

  size_t PendingBytes() const;   // 已打开的块负载 + 已 seal 但仍未写的块
  static std::string SegmentName(uint32_t shard_id, uint64_t seq);
};
```

**发布协议**是经典的 crash-safe rename 序列，源码用一行注释精确概括：

```
  ① 写 body 到 "seg-N.tmp"        —— 内容可随意，崩了也不影响，因为没有任何目录项指向它
  ② 写 header 到 "seg-N.tmp" 头部  —— CRC / segment 元信息
  ③ fdatasync("seg-N.tmp")         —— 数据块 + 必要的元数据落盘
  ④ link("seg-N.tmp", "seg-N")     —— 原子创建目录项（同目录内 rename），此刻段「存在」了
  ⑤ fsync(dir)                     —— 把目录项本身也落盘
```

`fdatasync` 刷数据块、`fsync(dir)` 刷目录项，两者都完成才算「段存在」。**任何一步之间崩溃，恢复时看到的要么是完整的 `seg-N`，要么什么都没有，绝无半截段。**（这是 POSIX 对 `link`/`rename` 原子性的直接利用，Dragonfly 只是把它用对了。）

**两级 LSN 是可观测性设计**：`WrittenLsn` 与 `DurableLsn` 的差值就是「已提交但尚未持久」的窗口。副本可用它衡量自己落后多少；运维可用它评估 `everysec` 模式的**真实风险敞口**——比笼统的「最多丢 1 秒」精确得多。

**背压**：`WaitPending(limit)` 在 pending 字节数超阈值时挂起当前 fiber，而不是无限缓冲。`PendingBytes()` 的定义是「已打开的块负载 + 已 seal 但还没写的块」，即**连排在未写块后面的那些已 seal 块也要算**——否则背压会被后续的 seal 绕过。这是 Dragonfly 上**唯一一处让命令主动等盘的地方**，也是 `AOF always` 模式写吞吐显著下降的直接原因。

**错误即终态**：`error()` 一旦非空，writer 永久停止接受新记录并停止 sync；恢复路径是 `Shutdown()` + 在**下一个 checkpoint** 开新 writer。宁可停在检查点，也不带着未知的部分状态继续写。

> 注：`uring_file.h`（AofSegmentWriter 依赖的 uring 文件封装）在 Dragonfly 主仓中并不存在，它由同作者的 **helio** 子模块提供——见 28.7。

### 28.5 .dfs 快照：按 shard 并行序列化（**无 fork，无 COW**）

一个常见误解需要澄清：**Dragonfly 的快照不使用 fork + 写时复制（COW）**。它走的是「每 shard 一个文件 + 一个 summary 文件」的显式并行：

```cpp
// src/server/detail/save_stages_controller.cc（approx，模块未 init，行号需校对）
void SaveStagesController::SaveDfs() {
  const string_view ext = snapshot_storage_->IsCloud() ? ".dfs" : ".dfs.tmp";
  ShardId sid = 0;
  for (auto& [_, filename] : snapshots_) {
    filename = full_path_;
    if (sid < shard_set->size()) ExtendDfsFilenameWithShard(sid++, ext, &filename);  // -0, -1, ...
    else                        SetExtension("summary", ext, &filename);           // -summary
  }
  absl::InsecureBitGen gen;
  std::string snapshot_id = GetRandomHex(gen, 32);      // 32 位随机十六进制

  SaveDfsSingle(nullptr, snapshot_id);                  // ① summary 先写

  trans_->ScheduleSingleHop([&](Transaction*, EngineShard* shard) {
    SaveDfsSingle(shard, snapshot_id);                  // ② 每个 shard 一个 hop，真正并行
    return OpStatus::OK;
  });
}

void SaveStagesController::WaitAllSnapshots() {
  if (use_dfs_format_) {
    shard_set->RunBlockingInParallel([&](EngineShard* shard) { WaitSnapshotInShard(shard); });
    SaveBody(shard_set->size());                        // 收尾：写完 summary 尾部
  } else {
    SaveBody(0);
  }
}
```

完整流程：

1. 文件名统一带 `-{sid}` 或 `-summary` 后缀，且**一律先写 `.dfs.tmp`，写完才改名**——与 AOF 段是同一套 rename 原子发布（写云端时直接 `.dfs`）；
2. `snapshot_id` 是 32 位随机十六进制，出现在**所有** shard 文件中，用于校验一组文件确实属于同一次快照；
3. **summary 先写**，它承载全局元信息（RDB 类型频次表、版本、辅助字段计数等）。对应 `SaveMode::SUMMARY`（`shard == nullptr` 时索引取 `shard_set->size()`）；
4. 每个 shard 用**一个 transaction hop** 启动自己的 `RdbSnapshot`（`SaveMode::SINGLE_SHARD`），所以 N 个 shard 真正并行；
5. 收尾用 `RunBlockingInParallel` fan-out 等待全部 shard 落盘（允许抢占，因为要写盘），再 `SaveBody` 收尾 summary。源码里 `RunBriefInParallel` 与 `RunBlockingInParallel` 的区分（27.6）在这里得到了印证——序列化必须用 Blocking 版本。

**与 fork+COW 的取舍对比**：

| | fork + COW | Dragonfly .dfs |
|---|---|---|
| 一致视图 | fork 瞬间的**全局一致**快照 | **没有**全局瞬时一致（各 shard 序列化时刻不同） |
| 前端影响 | 页表复制开销 + 写放大 + 数十~数百 ms 停顿 | **全程不阻塞**，可与写入并行 |
| 分发 | 单文件（往往需再分片） | 天然按 shard 分片，可直接并发上传对象存储 |
| 一致性兜底 | fork 本身 | `summary` 文件 + `snapshot_id` 校验 |

Dragonfly 放弃了「瞬时全局一致」，换取「不阻塞 + 天然分片 + 可直传对象存储」。这个取舍在**单实例持有 100GB+ 内存**、snapshot 动辄数十秒的场景下是明显划算的。

### 28.6 多流复制：num_flows 条 TCP 连接并行拉增量

Dragonfly 的复制是**多流（multi-flow）**的，这是它相对 Redis 单流 PSYNC 最大的结构性差异：

```cpp
// src/server/replica.h（approx，模块未 init，行号需校对）
struct MasterContext {
  std::string  master_repl_id;
  std::string  dfly_session_id;      // 本次同步会话的 id
  unsigned     num_flows = 0;        // 主库开的流数，合法范围 (0, 1024]
  DflyVersion  version = DflyVersion::VER1;
  std::string  lineage_id;
  std::optional<AnnouncedAddress> announced;
};

class Replica : ProtocolClient {
  // 状态机：R_ENABLED → R_TCP_CONNECTED → R_GREETED → R_SYNCING → R_SYNC_OK
  // R_SYNCING 与 R_SYNC_OK 互斥
  enum State : unsigned { R_ENABLED = 1, R_TCP_CONNECTED = 2,
                          R_GREETED = 4, R_SYNCING = 8, R_SYNC_OK = 0x10 };

  std::error_code StartStableSyncFlow(ExecutionState*);
  void StableSyncDflyReadFb(ExecutionState*);   // 读流 fiber
  void StableSyncDflyAcksFb(ExecutionState*);   // 回 ACK fiber
  std::array/vector<std::unique_ptr<DflyShardReplica>> shard_flows_;
};
```

流的合法性由握手阶段校验：`if (*num_flows <= 0 || *num_flows > 1024)` 直接报错丢弃。

流到 shard / 线程的分配：

```cpp
// src/server/replica.cc（approx，模块未 init，行号需校对）
// 把 flow 下标轮转铺到 shard_set 的 proactor 线程池上
vector<vector<unsigned>> Partition(unsigned num_flows) {
  vector<vector<unsigned>> partition(shard_set->pool()->size());
  for (unsigned i = 0; i < num_flows; ++i)
    partition[i % partition.size()].push_back(i);
  return partition;
}
```

**为什么这样做是可行的**：journal 的 LSN 是**每 shard 独立**的（28.2），因此一条流只要负责固定的 shard 子集，它的字节序列就是确定且自洽的，`num_flows` 取多少都不影响语义正确性。而 `Partition` 把流轮转铺到线程池上，保证**每条流由不同的 proactor 线程驱动**，否则一条慢流会阻塞同线程上的其他流。

**`num_flows` 直接决定增量同步的聚合带宽**：主库 journal 写入 1 GB/s 时，开 16 条流就能跑满 16 张网卡——这是 Redis 单流复制做不到的（单 TCP 流的 TCP 窗口会成为硬顶）。

各条流通过一个共享的 **`MultiShardExecution`**（`src/server/journal/tx_executor.h`）汇合：

```cpp
// src/server/journal/tx_executor.h（approx，模块未 init，行号需校对）
class MultiShardExecution {
 public:
  enum class Status : uint8_t {
    kOk,
    // 本轮命令未成功，各流一致。会话在运行前被取消（永久），或 apply 返回 false（仅本轮失败）。
    kFailed,
    // 各流到达了【不同的】全局命令 —— 这次汇合被永久取消。
    kOutOfOrder,
  };

  explicit MultiShardExecution(uint32_t num_flows);

  // 阻塞直到所有流都到达 txid，在其中【恰好一条】流里执行 apply，然后放行所有流。
  // apply 在 mu_ 保护下执行：其他流本来也过不去。它不得回调本对象。
  // 本轮命令一旦执行过，所有流都会拿到同一结果，即便期间被取消：
  // 因为每条流都从自己已应用的记录数继续部分同步，所以它们【必须】对结果达成一致。
  Status Execute(TxId txid, absl::FunctionRef<bool()> apply);
};
```

这是一个 **按 `TxId` 编组的屏障（barrier rendezvous）**，解决了「多流乱序到达」这个本质矛盾：

- 流之间**记录到达顺序不可比**，但同一笔事务（同一 `TxId`）的记录必须**恰好执行一次**且在所有流上看到**同一结果**；
- `Execute` 让所有流在 `txid` 处对齐，任选一条流执行 `apply`，然后同步放行全部流，从而**把一次全局事务的原子性跨流重建出来**；
- `kOutOfOrder` 是这套机制的失败模式：若各流在同一个位置看到的是**不同的全局命令**，说明流之间已经错位到无法对齐，此时汇合被**永久取消**而不是重试——因为重试没有正确性依据。

副本侧用 `JournalApplier`（`src/server/journal/journal_applier.h`）把记录写进内存表。

**部分同步续传**：每条流独立上报已执行的 LSN（`shard_flows_[i]->SetRecordsExecuted(partial_sync_lsn)` / `JournalExecutedCount()`），主库据此从 journal 环形缓冲的对应位置续发。若某条流要的 LSN 已**滚出环形缓冲**（受 `max_age_ms_` / `max_bytes_` 双阈值约束，28.2），该流退化为全量同步——发 `.dfs` 文件，用固定 40 字节的 `kRdbEofMarkSize` 作为 EOF 标记。

**稳定态同步（stable sync）**：`StableSyncDflyReadFb` + `StableSyncDflyAcksFb` 两个 fiber 组成读写闭环——副本周期性汇报自己 ack 到哪，主库据此推进 `resume_bound_` 并回收缓冲。所有流都越过 `resume_bound_` 且 `user_count_ == 0` 时，主库才允许停 journal（28.3）。

复杂度：

- 增量同步墙钟时间 **O(journal 字节数 / num_flows)**，内存 **O(num_flows × 每流缓冲)**；
- 全量同步 **O(总数据量 / shard 数)**，并行度 = min(shard 数, 流数)；
- 汇合屏障 **O(num_flows)** 一次 CAS 屏障，摊还 O(1)。

### 28.7 helio：真正承载 fiber / io_uring / uring_file 的仓库

一个必须点明的结构性事实：Dragonfly 主仓**没有** `src/fiber/`、**没有** `src/io/`，也没有 `util/fibers/uring_proactor.h`。这些全部来自同作者 Roman Gushchin 的 **helio** 子模块（`.gitmodules` 中 `url = https://github.com/romange/helio.git`，构建时 `include_directories(helio)`，CI 也把 `helio/**/*.cc` / `helio/**/*.h` 纳入检查）。

Dragonfly 侧对 helio 的典型依赖（canonical 路径在 helio 仓内，本仓库标注为 approx）：

| Dragonfly 中的 include | helio 提供的能力 |
|---|---|
| `util/fibers/uring_proactor.h` | **io_uring proactor**：提交/收割模型，替代 epoll；`src/io/uring.h` 在 helio |
| `util/fibers/synchronization.h` | `util::fb2::Mutex` / `SharedMutex` / `LockGuard` |
| `util/fibers/detail/fiber_interface.h` | `EnterFiberAtomicSection` / `LeaveFiberAtomicSection`（28.2 的原子段） |
| `util/fibers/fiberqueue_threadpool.h` | `FiberQueue`（27.6 的派发）、`FiberQueueThreadPool`（28.5） |
| `util/fibers/future.h` | `util::fb2::Future<io::Result<T>>`（分层存储异步回填，28.8） |
| `util/proactor_pool.h` | `util::ProactorPool`（28.6 的 `shard_set->pool()`） |
| `util/fiber_socket_base.h` | 协程 socket 基类（复制流 28.6 建立在它之上） |
| `io/io.h` / `io/io_buf.h` | `io::Sink` / `io::Result` / `io::Bytes`（快照与 AOF 的 I/O 抽象） |
| `util/fibers/uring_file.h` | uring 封装文件（`AofSegmentWriter` 的 I/O 后端，28.4） |

代码里统一用 **`util::fb2`** 这个命名空间前缀（`fb2` = fiber backend v2，对 boost::fibers 的封装）：

```cpp
// src/server/journal/journal_slice.h（approx）—— 来自 helio 的 boost::fibers 封装
mutable util::fb2::SharedMutex cb_mu_;
util::fb2::LockGuard guard(mutex_);
util::fb2::detail::EnterFiberAtomicSection();
// src/server/metrics.cc（approx）—— fiber 栈内存从 helio 查询
worker_fiber_stack_size = fb2::WorkerFibersStackSize();
```

**这一点对学习者很重要**：读 Dragonfly 的「fiber / io_uring 部分」时，光看 Dragonfly 主仓会一头雾水（因为这些符号根本不在仓里），必须同时打开 helio 仓。这也解释了为什么 §18.1 的「源码路径速查」里没有它们。

### 28.8 分层存储：把持久化谱系补到冷数据

作为最后一环，`src/server/tiered_storage.h` 把冷 value 卸载到磁盘：

```cpp
// src/server/tiered_storage.h（approx，模块未 init，行号需校对）
struct TieredStorageBase {
  // 独占一整页的 value 的最小尺寸
  const static size_t kMinOccupancySize = tiering::kPageSize / 2;   // kPageSize = 4_KB → 2KB
};

template <typename T> using TResult = util::fb2::Future<io::Result<T>>;
using BackPressureFuture = std::optional<util::fb2::Future<bool>>;

#ifdef WITH_TIERING
class TieredStorage : public TieredStorageBase {
  class ShardOpManager;      // 每 shard 的卸载/回填操作仲裁
  ...
  template <typename D, typename F>
  void Read(tiering::ReadId id, const tiering::DiskSegment& segment, const D& decoder, F&& f, ...)
  std::error_code Open(std::string_view path);   void Close();
};
#endif
```

三个算法要点：

1. **`kMinOccupancySize = kPageSize / 2 = 2KB`**（`kPageSize = 4_KB`，见 `src/server/tiering/common.h`）。小于半页的 value 卸载出去会浪费一半以上的磁盘块，所以**只搬大 value**——这是块设备「整块写」的物理约束直接推导出的阈值；
2. **卸载/回填全程异步**：接口返回 `Future<io::Result<T>>`，读命中磁盘时挂起当前 fiber（**不阻塞 proactor 线程**），`BackPressureFuture` 则用于在卸载过快时反压；
3. **外部条目的索引用 DASH 的 entry map**（`src/server/tiering/entry_map.h` 依赖 `core/dash_internal.h`）——正好印证了 27.1 的分工表：DenseSet 服务单线程主表，DASH 服务需要并发扩容的冷数据索引。

---

## 二十九、参考资料

- [Dragonfly 官方文档](https://www.dragonflydb.io/docs)
- [Dragonfly GitHub](https://github.com/dragonflydb/dragonfly)
- [Roman Gushchin 技术博客](https://www.dragonflydb.io/blog/author/roman-gushchin)
- [Dragonfly vs Redis 性能对比](https://www.dragonflydb.io/blog/benchmark)
- [Shared-Nothing Architecture 论文](https://en.wikipedia.org/wiki/Shared-nothing_architecture)
- [io_uring 内核文档](https://kernel.dk/io_uring.pdf)
- [Boost.Fiber 文档](https://www.boost.org/doc/libs/release/libs/fiber/)
- [Redis 设计与实现（对照参考）](https://redisbook.com/)
- [Linux 高性能服务器编程（游双）](https://book.douban.com/subject/24722611/)
- [DragonflyDB 官方 Reddit](https://www.reddit.com/r/dragonflydb/)