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

## 二十五、参考资料

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