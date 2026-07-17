# S22 — Phase10 Raft Redsign 续做

## What Changes

学习层 `learning/code-solutions/distributed/{raft,shard,dist_txn}.c` 与同名 .h 不兼容（S14 已确认）。本 S22 **重写 raft 学习层副本**使其能编译：

1. **重写 `learning/code-solutions/distributed/raft.c`**：让所有 .h 声明的函数与 .c 实现匹配
2. **重写 `learning/code-solutions/distributed/dist_txn.c`**：用 `TwoPCoordinator`
3. **重写 `learning/code-solutions/distributed/shard.c`**：返回 `ShardRouting*`
4. **CMakeLists.txt** 移除 EXCLUDE 排除
5. **验证 V1-V3**：4 个 .c 全部进 libdistributed-solutions.a

## Why

**β 价值（学习日志）**：
- phase9 业务逻辑真实可学可编译
- 学习者 clone 单文件即读懂 Raft 共识

**前置依赖**：
- S11 已让 coordinator.c 编译
- S14 已确认不兼容点

## Scope

**包含**：
- 3 个 .c 重写
- CMakeLists.txt 修复

**不包含**：
- ❌ 改 .h（公共 ABI）
- ❌ 改工程层 `engineering/src/db/core/raft.c`（生产实现在那里）
- ❌ 加 ctest 给学习层副本

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 重写丢失 phase9 业务语义 | 中 | 学习层副本仅参考用，工程层真产品不受影响 |
| 仍编译失败 | 中 | V3 验证 |
