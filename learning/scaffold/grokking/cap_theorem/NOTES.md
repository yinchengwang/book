# CAP 定理 · 学习笔记

## 核心概念

1. **C（Consistency）**：所有节点在同一时刻看到相同的数据
2. **A（Availability）**：每个请求都能获得非错误的响应（但不保证数据是最新的）
3. **P（Partition Tolerance）**：系统在节点间网络分区时仍能正常运作
4. **CAP 不可能三角**：分布式系统最多同时满足 2 个

## 系统分类

| 系统 | 选择 | 理由 |
|------|------|------|
| HBase / ZK | CP | 强一致性优先，分区时不可用 |
| Cassandra / DynamoDB | AP | 始终可用，分区时可能读到旧数据 |
| 单机 MySQL | CA | 无分区场景，保证 ACID |

## 工程对照

分布式理论直接影响数据库实现。本项目的 `engineering/src/db/shard/shard.c`
实现了分片路由和 2PC 协调器，底层采用了 Raft 共识算法
（`engineering/src/db/raft/raft.c`）来达成数据一致性。Raft 本质上是一个
CP 系统：Leader 保证日志的强一致性，Follower 的网络分区中不可用。
与本文的 Python 类模拟不同，Raft 通过 Leader 选举和日志复制来实现
严格的一致性语义，并周期性心跳维持集群视图。
