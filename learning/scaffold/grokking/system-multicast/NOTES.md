# Gossip 多播协议 学习笔记

## 核心概念

- **Gossip 传播**: 每个已感染节点随机选择 k 个对等点传播
- **收敛性**: O(log N) 轮后全网感染（k=fanout）
- **Push 模型**: 主动推送给对等点（适合少量节点）
- **Pull 模型**: 定期拉取比较（适合大量节点）
- **Anti-Entropy**: 定期全量比对修复差异

## Gossip 变体

| 模式 | 描述 | 带宽 | 收敛速度 |
|------|------|------|----------|
| Push | 感染者推送给随机节点 | 高 | 快 |
| Pull | 未感染者从随机节点拉取 | 低 | 慢 |
| Push-Pull | 双向交换 | 中 | 最快 |
| 反熵 | 定期全量比对数 | 高 | 最可靠 |

## 工程对照

`engineering/src/db/dist/raft.c` 中的 Raft 协议使用**点对点 RPC**（RequestVote、AppendEntries）而非 Gossip 传播——每次选举结果和日志条目通过直接 RPC 而非流言传播。这突出了两种协议的设计差异：Raft 追求强一致性（领导者为中心），Gossip 追求最终一致性（去中心化）。`engineering/include/db/raft.h` 的 `raft_entry_log` 结构用于按序复制日志，而 Gossip 协议如果用于日志同步则需要版本向量（Version Vector）检测冲突。`engineering/include/db/dist_txn.h` 的分布式事务实现中，如果最终一致性要求（如只读副本同步），可以使用 Gossip 协议传播事务状态变更——这与 Raft 的同步复制形成对比。`engineering/src/db/rel/rel.c` 中的 Relation 扫描机制通过文件系统接口读取数据页，对应 Gossip Anti-Entropy 的"全量比对"概念——都是通过比较底层数据来修复差异。

## 面试要点

1. Gossip 是去中心化的，没有单点故障
2. O(log N) 的收敛速度保证了可扩展性
3. 反熵修复要配合 Merkle Tree 减少传输量
4. Gossip 适合"成员管理"和"告警传播"场景
