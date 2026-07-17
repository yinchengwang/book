# S14 Spec —— Phase9 学习层副本头对齐

## 1. 头文件契约

`learning/code-solutions/distributed/*.h` 是公共 ABI（可能由其他模块引用），**不应在 S14 修改**。

`.c` 文件实现内部应调整以匹配 `.h` 声明：

## 2. 已知不兼容点

### raft.c
- `raft_handle_append_entries` 调用：9 参 → 7 参
- `raft_handle_request_vote` 同样需调整

### dist_txn.c
- `TwoPCCoordinator` 全部改 `TwoPCoordinator`

### shard.c
- `shard_route_query` 返回 `ShardRouting **` 改为 `ShardRouting *`

## 3. 不做（明确范围外）

- ❌ 修改 .h 头文件
- ❌ 改工程层 `engineering/src/db/core/raft.c` 等
- ❌ 加 ctest 测试
