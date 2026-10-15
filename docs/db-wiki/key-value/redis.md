# Redis Wiki（内存键值数据库 / 多数据结构服务器）

> 参考版本：Redis 仓库当前 commit `f306bc31a93033551d29190ac6247f71f824cf3d`（7.x 系列，主要 API 与数据布局在 6.x 之后保持稳定；7.x 引入了 Redis Functions、ACL v2、Sharded Pub/Sub 等增强）。

## 一、项目定位

Redis（REmote DIctionary Server）是一款**基于内存、支持多种数据结构的键值数据库**，对外提供字符串、哈希、列表、集合、有序集合、位图、HyperLogLog、Stream、地理空间等丰富的数据结构。核心定位是**高性能缓存 + 多数据结构内存数据库**，近年通过 Redis Stack（RediSearch / RedisJSON / RedisGraph / RedisTimeSeries / RedisBloom）向多模态数据库方向扩展。

- **痛点**：
  - 传统 Memcached 仅支持简单 KV，无法表达业务复杂结构（排行榜、计数器队列、共同好友等）；
  - 关系数据库在大规模 KV 缓存场景下 QPS 上限受限于磁盘 IO 与事务开销；
  - 业务需要"延迟极低 + 数据结构丰富 + 原子操作 + 持久化可选"的全能型内存引擎；
  - 互联网场景（社交 feed、推荐、限流、分布式锁、会话、消息队列）需要一个统一低延迟后端。
- **典型用户**：Twitter（Timeline 缓存）、GitHub（Session / Queue）、StackOverflow（Page Cache）、Snapchat、Uber、阿里巴巴（双 11 大促库存前置缓存）、美团（外卖订单状态）、腾讯（QQ 在线状态）。

## 二、发展历史与社区

- **起源**：2009 年意大利开发者 Salvatore Sanfilippo（antirez）在创业公司 LLOOGG 做实时网站分析时，因 MySQL 性能不足而开发 Redis；最初定位是"日志型实时分析器"的内存后端。
- **里程碑**：
  - 2009 年开源（BSD），最初仅支持字符串 + 列表；
  - 2010 年 VMware 赞助 antirez 全职开发；引入哈希、集合、有序集合；
  - 2013 年 Redis Cluster 推出；2014 年 Sentinel 高可用方案 GA；
  - 2015 年 3.0（Cluster GA）、2017 年 4.0（模块系统、PSYNC2 增强）；
  - 2018 年 5.0（Stream 数据结构）、2020 年 6.0（多线程 IO、ACL、客户端缓存）；
  - 2021 年 6.2（Sharded Pub/Sub、Redis Functions 早期）；
  - 2022 年 7.0（Redis Functions GA、ACL v2、Sharded Pub/Sub GA、多部分 AOF）；
  - 2023 年 7.2（Sharded Pub/Sub 完善、命令细粒度）；
  - 2024-2025 年 7.4 / 8.0（Redis Stack 多模态融合，向量化检索登场）。
- **维护状态**：活跃；Redis 8.x 起改为每年一个大版本；2024 年起 Redis 协议与公司（Redis Inc.）保持"双许可证"（RSALv2/SSPLv2 与 AGPLv3 并行）。
- **社区**：GitHub Discussions、Discord、Reddit r/redis、Redis University（免费认证）、RedisConf 年度大会。

## 三、许可与商业限制

- **核心代码**：自 2024 年 4 月起切换为 **RSALv2（Redis Source Available License v2）** 或 **SSPLv2（Server Side Public License v2）** 双许可证；AGPLv3 仍为开源社区选项之一。
  - 关键条款：禁止以"数据库即服务"对外提供托管 Redis 业务（与 MongoDB SSPL 类似），但自用、生产环境使用、嵌入应用分发均允许。
- **Redis 商业产品 / 服务**：
  - **Redis Enterprise**：闭源扩展（在仓的 `enterprise/` 目录），提供多区 Active-Active CRDT（Active-Active Geo Replication）、RDB+AOF+磁盘存储混合、多租户 Proxy；
  - **Redis Cloud**：全托管 DBaaS（AWS / GCP / Azure / AliCloud），提供 Essentials（小型）与 Pro（生产级）两档；
  - **Redis Stack**：开源模块化套件（RediSearch 向量+全文检索、RedisJSON、RedisGraph、RedisTimeSeries、RedisBloom、RedisGears）。
- **生态对比**：社区版（BSD/AGPL）与商业版的差异主要在 Active-Active 跨区复制、磁盘分层存储、Enterprise 管理工具等。

## 四、业界主要使用方与案例

- **互联网**：Twitter（Timeline / User Cache）、GitHub（Job Queue / Session）、StackOverflow（全站 Page Cache）、Pinterest、Instagram、Snapchat、Craigslist；
- **出行**：Uber（Dispatch 路由缓存）、Lyft、滴滴（订单实时状态）；
- **电商**：Alibaba（双 11 商品详情页缓存 + 库存前置）、JD、eBay；
- **金融**：Stripe（Rate Limiter）、Square、Robinhood；
- **游戏**：Zynga、暴雪（部分场景）；
- **国内**：美团、字节跳动、腾讯、网易、小红书、知乎、bilibili。

## 五、一句话对比定位

> Redis 是**"多数据结构 + 内存 + 丰富原子操作 + 可选持久化"**的内存数据库标杆；与 **Memcached（纯 KV、LRU、无持久化）**、**KeyDB（多线程 Redis 兼容 fork）**、**Dragonfly（Redis 兼容 + 多线程、共享无锁）** 一起构成**内存 KV 四强**。

## 六、整体架构

Redis 采用**单进程（主线程）+ 后台线程（IO、关闭、AOF 刷盘）+ 多客户端 IO 复用**的架构。Redis 7.x 后引入多线程 IO（`io-threads` 配置），但命令执行仍然是单线程。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│                  客户端 (RESP2/RESP3 / hiredis / jedis / redis-py)    │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ TCP (默认 6379) / RESP (REdis Serialization Proto)
┌─────────────────────▼────────────────────────────────────────────────┐
│                网络层 (networking.c, networking.c)                     │
│   - anet.c / anet.h : TCP 封装（bind、listen、non-blocking 接受）      │
│   - conn.c          : 客户端连接抽象（CT_Socket/CT_TLS）              │
│   - networking.c    : readQueryFromClient / addReply*                 │
│   - 多线程 IO：主线程读 + N 个 IO 线程解析 + 主线程命令执行          │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ 解析后的命令对象 (robj* + argv)
┌─────────────────────▼────────────────────────────────────────────────┐
│                命令层 (server.c, server.h)                            │
│   - server.c       : 命令表 redisCommandTable / 命令分派             │
│   - call()         : 命令执行（含 ACL、cluster 路由、慢日志、监视器）│
│   - db.c           : 16 个库的字典 + 键空间管理                       │
│   - expire.c       : 过期键删除（惰性 + 周期）                        │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ robj* + dict* + zskiplist*
┌─────────────────────▼────────────────────────────────────────────────┐
│                数据结构层 (object.c, dict.c, t_string.c, t_list.c, ...)│
│   - object.c    : robj 抽象（type + encoding + ptr）                 │
│   - dict.c      : 哈希表 + 渐进式 rehash（核心 KV 容器）              │
│   - sds.c       : 简单动态字符串（embstr/raw/编码共享）               │
│   - t_string.c, t_list.c, t_hash.c, t_set.c, t_zset.c : 6 大数据类型 │
│   - t_stream.c  : Stream（5.0+，基于 listpack + radix tree）          │
│   - quicklist.c, listpack.c, ziplist.c : 紧凑型容器                  │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ 分配/释放
┌─────────────────────▼────────────────────────────────────────────────┐
│                持久化层 (rdb.c, aof.c, bio.c, lazyfree.c)             │
│   - RDB  : 快照（SAVE/BGSAVE）→ dump.rdb                              │
│   - AOF  : 追加写命令（appendfsync everysec/always/no）→ appendonly.aof│
│   - 混合 : 7.x 默认 RDB-AOF 混合持久化                                │
│   - BIO  : 后台线程做异步刷盘、关闭 fd、lazy free                     │
└─────────────────────┬────────────────────────────────────────────────┘
                      │ fork / IO
┌─────────────────────▼────────────────────────────────────────────────┐
│                高可用 & 集群 (replication.c, cluster.c, sentinel.c)   │
│   - 主从复制（PSYNC2、offset、断点续传）                               │
│   - Sentinel（raft-like 多数派） → Master 选举                        │
│   - Cluster：16384 槽 + gossip + ASK/MOVED 重定向                     │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- 服务端启动：`src/server.c:5370`（`main()`）
- 命令表：`src/server.c:133`（`redisCommandTable[]`）
- 网络事件循环：`src/ae.c`（epoll/kqueue 封装）
- 数据库入口：`src/db.c:21`（`redisDb` 结构）

### 6.2 单进程 + 多线程模型

- **主线程**：唯一的命令执行线程，所有命令串行处理（保证原子性，无需加锁）；
- **IO 线程（7.x 增强）**：`io-threads` 配置支持 N 个线程并行**读网络 + 解析命令**，但命令执行仍回主线程（避免数据结构竞争）；
- **后台 BIO 线程**：处理 AOF 刷盘、关闭大文件 fd、lazy free 大对象（避免主线程阻塞）；
- **模块动态库**：Redis Modules（`module.c`）可加载第三方模块如 RediSearch、RedisGraph，独立线程池。

### 6.3 一条 SET 命令的端到端路径

1. **客户端连接** → `src/networking.c:readQueryFromClient()`；
2. **协议解析** → `src/networking.c:processInputBuffer()` 解析 RESP；
3. **命令查找** → `src/server.c:lookupCommand()` 在 `redisCommandTable` 中匹配；
4. **ACL 校验** → `src/acl.c` 检查用户权限；
5. **Cluster 路由** → `src/cluster.c:clusterRedirectIfNeeded()` 处理 MOVED/ASK；
6. **执行命令** → `src/t_string.c:setCommand()`；
7. **写入 dict** → `src/db.c:dbAdd()` / `src/dict.c:dictAdd()`；
8. **AOF 追加** → `src/aof.c:feedAppendOnlyFile()`；
9. **复制传播** → `src/replication.c:replicationFeedSlaves()`；
10. **响应客户端** → `src/networking.c:addReply()`。

## 七、存储引擎（KV 类型核心库深度展开）

Redis 的存储引擎是其灵魂，由**多层级数据结构**组成，每种类型对应多种编码，根据数据规模自动切换。

### 7.1 总体数据结构体系

```
┌──────────────────────────────────────────────────────────────────────┐
│                      Redis 键空间 (redisDb)                            │
│   - dict* dict    : 哈希表，键 → robj*                                │
│   - dict* expires : 过期字典（key → expire timestamp ms）             │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                    robj 对象层 (object.c)                              │
│   typedef struct redisObject {                                         │
│       unsigned type:4;       // OBJ_STRING / OBJ_LIST / OBJ_HASH ...  │
│       unsigned encoding:4;   // OBJ_ENCODING_RAW/EMBSTR/INT/HT/...   │
│       unsigned lru:24;       // LRU 时间戳（LRU/LFU 淘汰用）           │
│       int refcount;                                                 │
│       void *ptr;             // 指向具体数据结构                       │
│   } robj;                                                              │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┬───────────────┬──────────────────┐
        │             │             │               │                  │
   ┌────▼────┐   ┌────▼────┐   ┌────▼────┐   ┌────▼────┐        ┌────▼────┐
   │  String │   │   List  │   │   Hash  │   │   Set   │        │ZSet/Skiplist│
   │ sds/emb │   │quicklist│   │ listpack│   │ intset/ │        │ zskiplist │
   │ str/int │   │ / ziplist│  │  / ht   │   │   ht    │        │ + dict   │
   └─────────┘   └─────────┘   └─────────┘   └─────────┘        └──────────┘
                                                                       Stream
                                                                       listpack+rax
```

### 7.2 SDS（Simple Dynamic String）

`sds.c` / `sds.h`：Redis 自定义字符串，**二进制安全 + O(1) 长度 + 预分配 + 惰性空间释放**。

```c
// src/sds.h:39
struct __attribute__ ((__packed__)) sdshdr5 { ... };
struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len;        // 已使用长度
    uint8_t alloc;      // 已分配长度（不含 header）
    unsigned char flags;// 低 3 位表示 hdr 类型
    char buf[];         // 数据
};
struct __attribute__ ((__packed__)) sdshdr16 { uint16_t len; uint16_t alloc; ... };
struct __attribute__ ((__packed__)) sdshdr32 { uint32_t len; uint32_t alloc; ... };
struct __attribute__ ((__packed__)) sdshdr64 { uint64_t len; uint64_t alloc; ... };
```

要点：
- **5 种 header**（sdshdr5/8/16/32/64）按字符串长度自适应，节省小串内存；
- **预分配**：SDS 增长时若新长 < 1MB 则分配 2 倍空间，> 1MB 则每次多分配 1MB；
- **惰性释放**：`sdsfree()` 不立即 `free`，而是把空间留给后续 `sdscat` 复用；
- **二进制安全**：`len` 字段而非 `\0` 终止，可以保存任意字节（含 `\0`）。

### 7.3 dict（哈希表 + 渐进式 rehash）

`dict.c` / `dict.h`：Redis 全局键空间（`redisDb.dict`）和哈希类型的底层容器。

```c
// src/dict.h:73
typedef struct dict {
    dictType *type;       // 多态回调（hash/keyDup/valDup/...）
    void *privdata;
    dictht ht[2];         // 两个 hash table，rehash 时交替使用
    long rehashidx;       // -1 表示未 rehash；>=0 表示渐进 rehash 进度
    unsigned long iterators;  // 安全迭代器计数
} dict;
```

**渐进式 rehash**（`dict.c:dictRehash()`）：
- 扩容条件：`ht[0].used / ht[0].size >= 1 + force_resize_ratio` 或 `used > 5` 且负载因子达阈值；
- **不一次性迁移**：每次 `dictAdd / dictFind / dictDelete / dictGetRandomKey` 时迁移 `rehashidx` 桶的 1 个槽，跨多次操作完成；
- **查询同时遍历两张表**：rehash 期间先在 `ht[0]` 查，miss 再在 `ht[1]` 查；
- **避免大 key rehash 阻塞**：单次 rehash 最多 N 个桶（受 `activerehashing` 配置限制），同时使用 lazy rehashing。

### 7.4 zskiplist（跳表）

`t_zset.c` / `zskiplist.c` / `zskiplist.h`：有序集合（Sorted Set）的核心数据结构，提供 O(log N) 范围查询与排名。

```c
// src/zskiplist.h:36
typedef struct zskiplistNode {
    sds ele;                        // 元素（SDS 字符串）
    double score;                   // 排序分
    struct zskiplistNode *backward;// 后向指针
    struct zskiplistLevel {
        struct zskiplistNode *forward;  // 层内前向
        unsigned long span;              // 层间跨度（用于 ZRANK）
    } level[];
} zskiplistNode;

typedef struct zskiplist {
    struct zskiplistNode *header, *tail;
    unsigned long length;
    int level;   // 当前最大层数
} zskiplist;
```

要点：
- **每节点层数随机**：P=0.25，期望层数 1.33，平均指针数 1/(1-P)=4/3；
- **跨度 span**：每层记录"该层 forward 跳过了多少节点"，用于 O(log N) 算 `ZRANK`；
- **后向指针 backward**：仅最低层有，用于 `ZREVRANGE` 反向遍历；
- **两种编码**：元素 < 128 且 `server.zset_max_ziplist_entries`（默认 128）时使用 `ziplist`/`listpack`，超过则升级为 `zskiplist + dict`（O(1) 按 member 取 score）。

### 7.5 quicklist（列表底层）

`quicklist.c` / `quicklist.h`：List 数据结构的底层（替代旧版 `ziplist + linkedlist`）。

```
quicklist
├─ quicklistNode (ziplist / listpack)
│   ├─ prev / next 双向链表
│   ├─ zl 指针（紧凑型节点）
│   └─ sz, count, encoding
└─ fill 因子（list-max-listpack-size）
    - 正数：每个 ziplist 最多 fill 个 entry
    - 负数：每个 ziplist 字节上限（-5=64KB）
```

要点：
- **双向链表 + 节点内 ziplist/listpack**：兼顾双向遍历效率与紧凑性；
- **LZF 压缩**：`list-compress-depth` 配置控制中间节点压缩（节省内存）；
- **push/pop 在两端**：O(1) 修改头尾节点的 ziplist，满了才新建节点。

### 7.6 listpack（紧凑字符串容器）

`listpack.c` / `listpack.h`：7.0 起替代 ziplist 的下一代紧凑容器（ziplist 在大列表时 cascade update 性能差）。

```c
// src/listpack.h:33
struct listpackEntry {
    long long val;        // 整数值（INT 编码）
    sds ele;               // 字符串
    unsigned char *p;     // 原始字节
    unsigned int slen;    // 字符串长度
    int valen;            // 整数值编码长度
};
```

要点：
- **单字节 header**：每个 entry 含 `<encoding-type><element-data><element-tot-len>`；
- **避免 cascade update**：每个 entry 记录自己的总长度，不依赖后续 entry 的 prevlen；
- **更安全的大列表**：相比 ziplist 的 prevlen 链式更新，listpack 在增删改时的连锁更新概率显著降低。

### 7.7 Stream（5.0+）

`t_stream.c`：消息流数据结构，用于消息队列、时序事件。

内部组成：
- **radix tree（rax）** 存消息 ID → 消息体（listpack 数组）；
- **listpack 节点** 单个消息（`[id, [field1, val1, field2, val2, ...]]`）；
- **消费组（CG）**：基于 PEL（Pending Entries List）+ last_delivered_id 实现"消费者组 + ACK"语义，类似 Kafka Consumer Group。

### 7.8 编码自适应（encoding 切换）

Redis 对每种数据类型都设计了**"小规模紧凑型 + 大规模 hash 表/跳表"**的双编码策略，根据数据规模自动切换：

| 类型 | 小规模编码 | 大规模编码 | 阈值 |
|------|-----------|-----------|------|
| String | int / embstr (≤44B) | raw (sds) | 44B |
| Hash | listpack | dict | list-max-listpack-entries 默认 128 |
| List | quicklist (内部 listpack) | quicklist (无升级) | list-max-listpack-size 默认 -2（8KB） |
| Set | intset (全整数) | dict | set-max-intset-entries 默认 512 |
| ZSet | listpack | zskiplist + dict | zset-max-listpack-entries 默认 128 |

切换由 `object.c:tryObjectEncoding()` 在每次写入时检查；目的是**小数据节省内存 + 大数据保持性能**。

## 八、索引机制

Redis 数据结构自身的"索引"与数据库索引概念不同，但通过以下方式支持高效查询：

- **dict**（O(1) 哈希查）：所有类型的成员到 value 的映射；
- **zskiplist**（O(log N) 范围查）：有序集合 ZRANGEBYSCORE / ZRANK；
- **rax**（radix tree，O(k) 前缀查）：Stream 按消息 ID 前缀查询；
- **geohash**（`geo.c`）：地理空间位置编码到 sorted set 分数位，按 ZRANGE 邻近查询；
- **bitmap / HyperLogLog / Bloom**（Redis Stack 模块）：位图基数 / 概率基数 / 布隆过滤器。

## 九、查询/计算/接口层

- **接口协议**：RESP（REdis Serialization Protocol）v2/v3，文本行 + 二进制混合；
- **API 风格**：单条命令式 API（`SET key val`、`ZADD zset score member`），通过 Lua 脚本扩展事务与复杂逻辑；
- **执行模型**：所有命令在主线程顺序执行（无并发），通过 IO 复用（epoll/kqueue）单线程处理 10W+ QPS；
- **脚本系统**：Lua 脚本（`EVAL` / `EVALSHA`），基于 Redis 自身 Lua 5.1 引擎；保证脚本内命令原子；
- **模块系统**：Redis Modules API（`module.c`）允许加载 `.so`，实现自定义命令与数据类型。

## 十、事务与一致性（KV 类型简化版）

Redis 事务模型较为朴素：

- **MULTI / EXEC**：`src/multi.c`，将多条命令打包串行执行，但**不支持回滚**（部分命令失败不影响后续命令）；
- **WATCH / CAS**：`src/multi.c:watchCommand()`，乐观锁机制（被 WATCH 的 key 在 EXEC 前被修改则放弃事务）；
- **Lua 脚本原子性**：`EVAL` / `EVALSHA` 中的所有命令整体原子执行；
- **持久化一致性**：AOF `appendfsync=always` 提供强一致；RDB 快照丢失窗口期数据（默认 5 分钟 BGSAVE）；
- **主从复制**：异步复制（`src/replication.c`），存在主从切换丢数据风险；7.x 起引入 `WAIT` 命令可等待 N 副本确认；
- **集群一致性**：Cluster 不保证强一致，副本复制为异步，`WAIT` 可强制 N 副本。

## 十一、复制与高可用

- **主从复制**（`src/replication.c`）：
  - 全量 SYNC → PSYNC2（`src/replication.c:masterTryPartialResynchronization`）支持断点续传；
  - 复制偏移量（replication offset）+ 复制积压缓冲区（repl_backlog）；
  - 主从心跳 PING / REPLCONF ACK。
- **Sentinel**（`src/sentinel.c`）：raft-like 多数派选举，自动 failover；客户端通过 Sentinel 订阅发现 Master；
- **Redis Cluster**（`src/cluster.c`）：
  - 16384 哈希槽（CRC16(key) mod 16384）；
  - Gossip 协议（meet / ping / pong / fail / publish）；
  - 槽迁移：`src/cluster.c:MIGRATE` 命令 + `redis-trib.rb`；
  - 智能客户端：MOVED（永久重定向）/ ASK（迁移中临时）；
  - 副本选举：基于 `cluster-replica-validity-factor` + Rank。
- **Active-Active CRDT**（Enterprise 闭源）：跨区多活 + CRDT 解决冲突。

## 十二、扩展性

- **垂直扩展**：单实例内存上限受限于硬件（典型 256GB ~ 1TB）；Cluster 模式横向扩展；
- **水平扩展**：Cluster 分片（16384 槽）+ 客户端路由或 Proxy（Twemproxy / Codis / Redis Proxy）；
- **冷热分层**：Enterprise 版支持 RAM + SSD 磁盘分层（将冷 value 卸载到 SSD）；
- **多数据库**：默认 16 个 `redisDb`（`SELECT 0..15`），仅做命名空间隔离，不跨库事务；
- **多租户**：通过 ACL（`src/acl.c`）实现用户/密码/命令/key 模式细粒度权限（7.x ACL v2）。

## 十三、性能与基准

- **官方基准**（`redis/redis` 仓 README）：
  - 单实例 GET/SET：`P99 < 1ms`，典型 QPS 10W ~ 20W（取决于硬件）；
  - Pipeline 模式：可突破 100W QPS；
  - 内存带宽限制：哈希、压缩、加密命令成为瓶颈；
- **YCSB 基准**：在 `ycsb` 工作负载下，Redis Cluster 吞吐量与延迟均领先多数 KV 引擎；
- **延迟分解**：网络 IO（epoll）+ 解析（RESP）+ 查找（dict 哈希）+ 复制（replicationFeedSlaves）；
- **内存效率**：
  - 紧凑编码（embstr/listpack/intset）显著降内存；
  - `zmalloc` 用 `__attribute__((packed))` 避免对齐浪费；
  - 内存碎片：`activedefrag yes` 自动整理。

## 十四、运维与可观测性

- **监控指标**：`INFO`（server / clients / memory / stats / replication / keyspace / commandstats / cluster）+ `LATENCY SUBSCRIBE`；
- **慢查询日志**：`slowlog-log-slower-than` 配置；
- **客户端监视器**：`MONITOR` 命令（生产慎用，影响性能）；
- **备份恢复**：RDB（`BGSAVE` / `SAVE`）+ AOF（`BGREWRITEAOF`）；启动时优先 AOF 恢复；
- **可观测生态**：Prometheus `redis_exporter`、Datadog / New Relic / Grafana Cloud Redis 集成；
- **tracing**：OpenTelemetry 集成（Redis 7.x 官方支持）。

## 十五、生态与集成

- **客户端 SDK**：官方支持 C（hiredis）、Python（redis-py）、Java（Jedis/Lettuce/Redisson）、Go（go-redis）、Node（ioredis）、Rust（redis-rs）、C++（cpp_redis）；
- **ORM**：Spring Data Redis、Redisson（分布式锁 / 集合 / 队列抽象）；
- **消息队列**：Redis Streams 替代轻量级 Kafka；Pub/Sub 用于实时通知；
- **缓存框架**：Spring Cache + Redis、Hibernate L2 Cache；
- **容器化**：官方 Docker 镜像、`redis-stack-server`；Helm Chart（Bitnami）；Kubernetes Operator（OT-CONTAINER-KIT）；
- **BI 工具**：RedisInsight 官方 GUI；TablePlus、Another Redis Desktop Manager（ARDM）。

## 十六、变体与衍生项目

- **KeyDB**：Snapchat 开源的多线程 Redis 兼容 fork（共享内存 + 多线程命令执行）；
- **Dragonfly**：现代 C++ 重写的 Redis 兼容服务器，共享无锁架构（详见 dragonfly.md）；
- **Valkey**：Linux Foundation 旗下 Redis 兼容分支（2024 年成立，Redis 7.2 之后 fork）；
- **Microsoft Garnet**：.NET 实现的下一代 KV 服务器，兼容 RESP 协议（高性能多线程）；
- **Redis Stack**：官方模块套件（RediSearch 向量+全文检索、RedisJSON、RedisGraph、RedisTimeSeries、RedisBloom）。

## 十七、核心论文与算法原理

- **Antirez 的早期博客**（http://antirez.com/）：Redis 设计哲学与数据结构演进；
- **Skip Lists: A Probabilistic Alternative to Balanced Trees**（Pugh, 1990）：跳表原始论文；
- **AOF 设计与混合持久化**：Redis 官方文档，RDB-AOF 混合（7.0+）；
- **PSYNC2 实现细节**：`replication.c` 中 `masterTryPartialResynchronization`；
- **CRC16-CCITT 哈希算法**：Redis Cluster 槽计算（`crc16.c`）。

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `src/server.h` 与 `src/server.c` 顶层结构（命令表、redisDb、客户端）；
  - 阅读 `src/dict.c`（渐进式 rehash 是 Redis 设计精髓）；
  - 阅读 `src/sds.c`（理解 Redis 自定义字符串的巧妙）；
  - 阅读 `src/object.c`（robj 抽象 + 编码自适应）。
- **进阶（2-3 周）**：
  - 阅读 `src/t_zset.c` 与 `src/zskiplist.c`（跳表实现与 ZRANGE 算法）；
  - 阅读 `src/cluster.c`（槽路由、Gossip、MOVED/ASK 协议）；
  - 阅读 `src/replication.c`（PSYNC2、replication offset）；
  - 阅读 `src/aof.c` 与 `src/rdb.c`（持久化与混合）。
- **高级（持续）**：
  - 阅读 `src/module.c`（Redis Modules API 设计）；
  - 阅读 `src/t_stream.c`（Consumer Group 设计，对标 Kafka）；
  - 阅读 `src/lazyfree.c`（BIO 异步释放，避免大 key 阻塞）；
  - 阅读 `src/expire.c`（惰性删除 + 周期删除 + LFU 近似算法）。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| 服务端启动 | `src/server.c:5370 main()` |
| 命令表 | `src/server.c:133 redisCommandTable[]` |
| 哈希表 | `src/dict.c:dictRehash()` |
| SDS | `src/sds.c:sdsNew() / sdscat()` |
| 跳表 | `src/zskiplist.c:zslInsert` |
| 列表 | `src/quicklist.c` |
| 紧凑容器 | `src/listpack.c` |
| 持久化 | `src/rdb.c` `src/aof.c` |
| 复制 | `src/replication.c` |
| Sentinel | `src/sentinel.c` |
| Cluster | `src/cluster.c` |
| 模块 API | `src/module.c` |

## 二十、源码规模与开发语言

- **LOC 数量**：约 18 万行 C（核心 `src/`）+ 1 万行 Tcl 测试；
- **主要语言**：C（核心）、Lua（脚本扩展）、Tcl（集成测试）、少量 Python 与 Bash；
- **构建系统**：简单 Makefile（`src/Makefile`）；无 CMake/Bazel；
- **测试**：Tcl 集成测试 + `tests/unit/type/listpack.tcl` 单测 + Redis Labs 内存测试套件；
- **CI**：GitHub Actions（Linux/macOS）+ 自有 Drone CI（多架构交叉编译）。

## 二十一、学习资源

- **官方文档**：[https://redis.io/docs/](https://redis.io/docs/)（最新文档）、[https://redis.io/commands/](https://redis.io/commands/)（命令字典）；
- **书籍**：《Redis 设计与实现》（黄健宏）、Redis in Action（Josiah Carlson）、《Redis 实战》；
- **博客**：antirez 个人博客（http://antirez.com/）、Redis Labs 官方博客；
- **YouTube 频道**：Redis、Redis University、Database Conference 演讲；
- **课程**：Redis University 认证课程（RU101 / RU102 / RU202 / RU301 / RU302）；
- **Reddit / HN**：r/redis、news.ycombinator.com。

## 二十二、可移植性与部署

- **支持平台**：Linux（全架构 x86_64 / aarch64 / arm / ppc64le）、macOS、FreeBSD、Windows（实验性 WSL 推荐）；
- **容器化**：官方 Docker（`redis:7.4-alpine`、`redis/redis-stack`）、Helm Chart；
- **Kubernetes Operator**：OT-CONTAINER-KIT / Spotahome Redis Operator；
- **嵌入式部署**：不推荐（Redis 设计为服务端，但有 `redis-cell` 风格的内嵌 SDK）；
- **可执行二进制**：`redis-server` + `redis-cli` + `redis-benchmark` + `redis-check-rdb` + `redis-sentinel`。

## 二十三、风险与陷阱

- **大 key 阻塞**：单 key > 100KB（如 HGETALL 大 hash、`KEYS *` 扫全库）会阻塞主线程数十毫秒；用 `SCAN` 替代 `KEYS`、`UNLINK` 替代 `DEL` 异步删除；
- **内存碎片与 OOM**：高频改写的紧凑型容器会产生碎片；`INFO memory` 的 `mem_fragmentation_ratio > 1.5` 需关注；`maxmemory` 达到后按策略淘汰；
- **持久化丢数据**：RDB 周期默认 `save 3600 1`，可能丢 1 小时数据；AOF `everysec` 最多丢 1 秒；强一致需求用 `WAIT N 0`；
- **主从切换丢数据**：异步复制 + `min-replicas-to-write 1` 才保证至少 1 副本写入；
- **Cluster 槽迁移**：迁移中客户端需支持 ASK 重定向；批量命令（如 MGET）需 `hash tag` 确保同槽；
- **慢命令**：KEYS / FLUSHDB / FLUSHALL / SORT / LREM 大列表 / ZUNIONSTORE 大集合；
- **Lua 脚本耗时**：脚本超时会阻塞主线程；用 `--lua-script-limit` 控制；
- **监控不当**：`MONITOR` 命令本身极耗性能，生产严禁开启；
- **客户端连接池**：默认 `maxclients 10000`；连接泄漏会耗尽 `tcp-backlog`。

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/algo/` 或 `engineering/include/algo/` 中若实现 SDS 抽象可参考 `sds.c`；
  - `engineering/include/db/` 中已有 KV 引擎（`kv_engine`），但与 Redis 的内存字典风格差异较大，Redis 的"数据结构即数据库"思想值得借鉴；
  - 渐进式 rehash 思想可应用于 `engineering/src/db/buf.c` 等；
- **学习复用价值**：
  - SDS 设计（紧凑 header + 预分配 + 惰性释放）可直接移植到 C/C++ 项目；
  - 编码自适应策略（小数据紧凑、大数据 hash）对内存受限场景极有价值；
  - 跳表实现可作为 `learning/ds-c/orig/` 教学数据结构补充。

## 二十五、参考资料

- [Redis 官方文档](https://redis.io/docs/)
- [Redis 7.0 发布说明](https://github.com/redis/redis/releases/tag/7.0.0)
- [RESP 协议规范](https://redis.io/docs/reference/protocol-spec/)
- [Redis 设计与实现（黄健宏）](https://redisbook.com/)
- [Redis Persistence（RDB + AOF 详解）](https://redis.io/docs/management/persistence/)
- [Redis Cluster Specification](https://redis.io/docs/reference/cluster-spec/)
- [Redis 源码阅读笔记](https://github.com/redis/redis/wiki)
- [Skip Lists 原始论文（Pugh 1990）](https://15721.codetables.org/slides/2023/07_pugh_1990.pdf)
- [antirez 个人博客](http://antirez.com/)
- [Redis Labs 工程博客](https://redis.com/blog/engineering/)
- [RedisConf 大会演讲](https://www.youtube.com/results?search_query=redisconf)