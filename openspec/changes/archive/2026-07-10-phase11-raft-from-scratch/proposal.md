# Phase11 — Engineering Raft Module 从零实现

## What Changes

工程层 `engineering/src/db/consensus/` + `engineering/include/db/consensus/` **从零创建**：

1. **`raft.h`**：Raft 公共 ABI（~600 行）
2. **`raft.c`**：选举 + 日志复制 + 心跳（最小可工作）
3. **`raft_test.cpp`**：smoke 测试
4. **CMakeLists.txt**：注册到 `db` 子目录
5. **ctest 集成**：`engineering/test/db/consensus/`

## Why

**α 价值**：
- 工程层当前无 raft 模块——phase9 memory 文档与实际代码不符
- 构建真正可用的 Raft 共识是分布式数据库基础

**前置依赖**：
- S1-S27 已让工程层基础架构稳定
- 工程层 cmake 与 ctest 测试框架已就绪

## Scope

**包含**：
- `engineering/include/db/consensus/raft.h`：公共 ABI
- `engineering/src/db/consensus/raft.c`：实现
- `engineering/test/db/consensus/raft_test.cpp`：smoke
- CMakeLists.txt 注册
- 验证 V1-V3 (编译 + test 跑通)

**不包含**：
- ❌ 集群网络通信（仅本机多节点 loop）
- ❌ 持久化（log_inmemory 实现）
- ❌ Joint Consensus / Snapshot 完整功能（V1 仅核心路径）
- ❌ 学习层副本（phase10+ 重做）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| Raft 算法实现 bug | 中 | V3 必须通过测试 |
| 并发安全 bug | 中 | 单线程最简实现 + lock |
| 编译警告 | 中 | -Wno-unused-parameter |

## 不做（明确范围外）

- ❌ 完整 Raft 实现（Joint Consensus、Snapshots、PreVote 等）
- ❌ 网络层实现（loopback transport 占位）
- ❌ Batching 优化
- ❌ Log Compaction
