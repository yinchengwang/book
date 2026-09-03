# R16 DB Redis 存储栈 — 规格定义

## 概述

R16 DB Redis 存储栈，涵盖 Redis 核心架构的 6 张学习卡。

## 能力规格

### 1. redis_event（事件循环）

| 规格 | 说明 |
|------|------|
| Reactor 模式 | 单线程事件分发器 |
| I/O 多路复用 | select/poll/epoll 对比 |
| 定时器管理 | 最小堆算法 |
| 工程对照 | `engineering/src/redis/event.c` |

### 2. redis_resp（RESP 协议）

| 规格 | 说明 |
|------|------|
| RESP 类型 | + - : $ * 五种类型 |
| 命令编码 | RESP 格式命令封装 |
| 解析器 | 状态机解析 RESP |
| 管道化 | 批量命令优化 |
| 工程对照 | `engineering/src/redis/resp.c` |

### 3. redis_object（对象系统）

| 规格 | 说明 |
|------|------|
| 对象类型 | String/List/Set/ZSet/Hash |
| 编码转换 | int/embstr/raw, ziplist/hashtable |
| 过期删除 | 惰性删除 + 定时删除 |
| 引用计数 | 对象共享与释放 |
| 工程对照 | `engineering/src/redis/object.c` |

### 4. redis_persist（持久化）

| 规格 | 说明 |
|------|------|
| RDB 持久化 | 二进制快照，fork COW |
| AOF 持久化 | 命令日志，appendfsync |
| 混合持久化 | RDB 前缀 + AOF 增量 |
| 重写机制 | AOF bgrewriteaof |
| 工程对照 | `engineering/src/redis/persist.c` |

### 5. redis_replica（主从复制）

| 规格 | 说明 |
|------|------|
| PSYNC 同步 | 全量/增量同步 |
| 复制积压缓冲区 | 环形缓冲区 |
| Replication Buffer | 从节点缓冲 |
| Sentinel | 故障检测与切换 |
| 工程对照 | `engineering/src/redis/replica.c` |

### 6. redis_cluster（集群架构）

| 规格 | 说明 |
|------|------|
| Hash Slot | 16384 槽分配 |
| MOVED 重定向 | 槽迁移处理 |
| ASK 重定向 | 迁移中临时访问 |
| Gossip 协议 | 节点发现与故障检测 |
| 工程对照 | `engineering/src/redis/cluster.c` |

## 文件清单

```
learning/scaffold/db/
├── redis-event/     # 事件循环 (~200 行)
├── redis-resp/      # RESP 协议 (~300 行)
├── redis-object/    # 对象系统 (~250 行)
├── redis-persist/   # 持久化 (~200 行)
├── redis-replica/   # 主从复制 (~200 行)
└── redis-cluster/   # 集群架构 (~250 行)

每卡包含: main.c, Makefile, README.md, NOTES.md
```

## 完成标准

- [x] 6 张卡 scaffold 创建完成
- [x] 编译通过 (gcc -std=c11)
- [x] 运行输出正常
- [x] statuses.json 更新为 done
- [x] NOTES.md 工程对照 ≥100 字
