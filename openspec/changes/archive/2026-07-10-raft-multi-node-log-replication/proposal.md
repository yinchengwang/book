# P1 — Raft Multi-Node + Log Replication

## What Changes

Phase11 单节点实现。P1 扩展为多节点 + 日志复制：

1. **In-process transport**：`raft_transport_t` 抽象（loop 数组发送）
2. **RequestVote RPC**：candidate → 收 RequestVote → 投票
3. **AppendEntries RPC**：leader → 复制日志到 follower
4. **Log Replication**：leader 发 AppendEntries，follower 持久化，复制过半 commit
5. **Cluster**：`raft_cluster_t` = `RaftServer_t[]`（带 transport 互联）

## Why

**α 价值**：
- 真正的 Raft 共识能多节点运行（不仅单节点）
- 工程层 db/ 模块分布式特性基础

**前置依赖**：
- phase11 单节点已实现 + 6 ctest pass

## Scope

**包含**：
- `raft_transport.h/c`：in-process transport
- `raft_cluster.h/c`：集群管理
- `raft_vote.c`：RequestVote 处理
- `raft_append.c`：AppendEntries 处理
- `raft_log_replication_test.cpp`：3 节点场景
- 验证 V1-V3

**不包含**：
- ❌ Log persistence（仍 in-memory）
- ❌ Snapshot
- ❌ Joint Consensus
- ❌ 网络传输（仅 loop）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 锁死锁（lock 嵌套） | 中 | 单 mutex per server + transport 异步 |
| 选举死循环 | 中 | election timeout 随机化 |
| 测试复杂度 | 中 | 用 scripted scenarios 而非随机化 |
