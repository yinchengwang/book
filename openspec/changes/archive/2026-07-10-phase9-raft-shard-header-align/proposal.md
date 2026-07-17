# S14 — Phase9 Raft/Shard/DistTxn 头文件对齐

## What Changes

`learning/code-solutions/distributed/{raft,shard,dist_txn}.c` 与同名 .h 不兼容（自 S5 起被排除编译）。具体不兼容点：

- `raft.c::raft_handle_append_entries` 调用 9 参，`.h` 声明 7 参
- `raft.c::raft_handle_request_vote` 类似
- `shard.c::shard_route_query` 返回 `ShardRouting*`，`.h` 声明返回 `ShardRouting**`
- `dist_txn.c` 使用 `TwoPCCoordinator`，`.h` 用 `TwoPCoordinator`

**S14 目标**：把这些学习层副本文件**对齐**——保持 .h 声明不变（这些函数在工程层可能仍被引用），改 .c 实现内部调用以匹配。

具体：
- `learning/code-solutions/distributed/raft.h` 中函数签名 = **生产相**（contract 是 7 参）
- `raft.c` 内部调用站点需调整参数列表

实际上，相对于"重写业务逻辑"，**更务实的方案是**：
1. 删除 raft.c、dist_txn.c、shard.c 中的"残缺调用"（如 raft.c 中调用 `raft_handle_append_entries` 时多传 2 个参数——去掉即可）
2. dist_txn.c 中 `TwoPCCoordinator` → `TwoPCoordinator`
3. shard.c 中 `ShardRouting*` 改成 `ShardRouting**`

**变更内容**：

1. **raft.c**：调整 `raft_handle_append_entries` / `raft_handle_request_vote` 调用站点（去冗余参数）
2. **dist_txn.c**：替换 `TwoPCCoordinator` → `TwoPCoordinator`
3. **shard.c**：调整 `shard_route_query` 等函数返回类型
4. **CMakeLists.txt**：移除 EXCLUDE REGEX，让 4 文件全编译
5. **验证 V1-V3**：cmake build-learning 全部成功

## Why

**α 价值（学习日志）**：
- 学习层副本代码不被编译 = 学习材料不完整
- 让 phase9 真实可编译 = 学习追溯时能跟踪

**前置依赖**：
- S11 已让 coordinator.c 部分编译
- S12 CI 已验证 build-learning

## Scope

**包含**：
- 4 个 .c 文件改为与 .h 兼容
- CMakeLists.txt 移除 EXCLUDE
- 验证 `libdistributed-solutions.a` 包含 4 个 .c 的所有符号

**不包含**：
- ❌ 修复功能语义（如返回类型变更可能改变算法语义）
- ❌ 改 raft.h 等头文件（保持 ABI 兼容）
- ❌ 给学习层副本加 ctest（仍为学习材料而非可测产品）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 改 .c 调用站点改错导致逻辑 bug | 中 | 仅学习层副本，不影响工程层产品 |
| 部分函数签名差太多，无法对齐 | 中 | 若难对齐，把那部分函数 wrap 成 static helper |
