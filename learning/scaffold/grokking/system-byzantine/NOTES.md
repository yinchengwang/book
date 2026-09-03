# 拜占庭容错 学习笔记

## 核心概念

- **拜占庭故障**: 节点可能发送任意错误消息（包括恶意攻击）
- **PBFT**: 实用拜占庭容错算法，三阶段达成共识
- **3f+1=N**: 容忍 f 个拜占庭节点需要至少 3f+1 个总节点
- **视图 (View)**: 主节点轮换机制，故障时触发视图变更

## PBFT 三阶段

| 阶段 | 动作 | 目的 |
|------|------|------|
| Pre-Prepare | 主节点广播请求 | 初始提案 |
| Prepare | 所有节点广播准备 | 确认收到提案 |
| Commit | 收到 2f+1 Prepare 后广播 Commit | 最终确认 |
| 执行 | 收到 2f+1 Commit 后执行 | 应用状态变更 |

## 工程对照

`engineering/src/db/dist/raft.c` 中的 Raft 实现是非拜占庭容错的（容忍 Crash 故障而非恶意故障），这对应 PBFT 工程化时的参照基线。`engineering/include/db/dist_txn.h` 中定义的分布式事务接口（`dist_txn_t`、`dist_txn_id`）处理跨节点的数据一致性，其实现使用的两阶段提交（2PC）与 PBFT 的 Phase 1/2 有类似的"准备-确认"模式——2PC 的 Prepare → Commit 对应 PBFT 的 Pre-Prepare → Prepare → Commit。`engineering/src/db/dist/coordinator.c` 中的集群协调器在 Raft 之上管理成员关系和健康检测，成员变更（节点上线/下线）对应 PBFT 的视图变更（View Change）。`engineering/src/db/wal/wal.c` 的 WAL 日志持久化保证了节点崩溃后的数据恢复，这是 PBFT 中"检查点"（Checkpoint）的工程等价物——定期截断日志并生成状态快照。

## 面试要点

1. PBFT 通信复杂度 O(n²)——不适用于大规模网络
2. 联盟链场景（有限节点）是 PBFT 的主要应用场景
3. 非拜占庭场景（Crash Crash）用 Raft，拜占庭场景用 PBFT
