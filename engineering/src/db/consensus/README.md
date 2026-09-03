# Raft 共识模块 — 工程层 Phase11

公共 ABI 见 `engineering/include/db/consensus/raft.h`。
实现：
- `raft.c` —— 单节点状态管理 + tick 选举推进 + leader 接受日志

## 限制

Phase11 是**最小可用版本**（P0 smoke test）：
- 单节点当选（cluster_size == 1）
- 多节点不投票选举（Phase11 不实现通信）
- 仅 in-memory 日志

## P1+ 续做（不在 Phase11 范围）

- 集群网络通信（loopback transport）
- Joint Consensus / Snapshot
- Log Compaction
- Log Persistence

构建：`cmake --build engineering/build` 自动纳入 `db_consensus` 库。
