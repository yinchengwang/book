# 领导者选举 学习笔记

## 核心概念

- **Raft**: 以领导者为中心的共识算法，分为选举/日志复制/安全性三组件
- **任期 (Term)**: 每次选举递增的逻辑时钟，防止过期投票
- **多数派 (Quorum)**: N/2+1 节点确认才能做出决策
- **Paxos**: 两阶段确认（Prepare/Accept），比 Raft 更抽象
- **租约 (Lease)**: 领导者周期性续租，过期自动降级
- **脑裂**: 网络分区导致多个领导者——多数派原则天然防裂

## Raft vs Paxos

| 特性 | Raft | Paxos |
|------|------|-------|
| 理解成本 | 低（拆分问题） | 高（抽象层次高） |
| 选举机制 | 随机超时触发 | 无显式选举 |
| 日志复制 | 领导者单点入 | 任意节点可提案 |
| 工程采用 | etcd/Consul/TiKV | Zookeeper/Chubby |

## 工程对照

`engineering/src/db/dist/raft.c` 是本项目中 Raft 算法的工程实现。`raft_node_t` 结构体记录了节点的当前任期（`current_term`）、投票状态（`voted_for`）和日志条目（`entries`）。`raft_hold_election()` 函数实现了完整的选举流程——节点超时后转为 Candidate，递增任期，发起 RequestVote RPC。`raft_append_entries()` 实现领导者日志复制（对应 Raft 论文的 AppendEntries RPC）。`raft_become_leader()` 在获得多数票后切换状态。`engineering/include/db/raft.h` 中定义了 Raft 对外接口，包括 `raft_init()`、`raft_tick()`（驱动超时逻辑）、`raft_step()`（处理 RPC）。`engineering/src/db/dist/coordinator.c` 中的协调器利用 Raft 管理集群状态，确保多节点下的强一致性。`engineering/src/db/txn/txn.c` 中的分布式事务实现依赖 Raft 的日志复制来保证跨节点的原子性。

## 面试要点

1. Raft 的核心在于"可理解性"——拆分为相对独立的子问题
2. 选举超时应引入随机化，避免多节点同时发起选举
3. 领导者故障时的不可用窗口 = 选举超时时间
