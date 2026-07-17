# S14 — Tasks (Phase9 Raft/Shard Header Align) - CANCELED

> **最终状态**：S14 实施中遇到 phase9 学习层副本（raft.c、dist_txn.c、shard.c）与 .h 头文件签名大量不匹配，最严重的：
>
> - `raft_handle_append_entries`：.h 7 参（缺 leader_id 或 leader_term），.c 9 参；逻辑也互相对不上
> - `raft_handle_request_vote`：参数顺序 .c 与 .h 不一致
> - `dist_txn.c::TwoPCCoordinator` 与 .h 的 `TwoPCoordinator` 名称不一致
> - `shard.c::shard_route_query` 返回类型 `ShardRouting*` vs .h `ShardRouting**`
>
> 修复需要**重写 phase9 业务逻辑**——超出 S14 范围（学习层副本仅作参考代码，工程层 `engineering/src/db/core/raft.c` 才是生产实现）。

## 1.1 调研

- [x] 1.1.1 已查：3 个 .c 文件签名 / 类型不匹配（具体见上）
- [x] 1.1.2 决策：S14 范围限定为"头对齐"，不重写业务逻辑
- [x] 1.1.3 决策：保守方案——保留 EXCLUDE 排除，不让 coordinator.c 之外的代码进 libdistributed-solutions.a

## 1.2 实施结论

- [x] 1.2.1 决策：S14 取消（S11 已让 coordinator 进入学习层库）
- [x] 1.2.2 决策：把"phase9 业务逻辑续做"标记为后续 change（phase10+）
- [x] 1.2.3 维持 CMakeLists.txt 的 EXCLUDE REGEX 不变

## 1.3 验证

- [x] 1.3.1 验证：build-learning 仍 158 leetcode 通过 + libdistributed-solutions.a 含 coordinator 部分
- [x] 1.3.2 libdistributed-solutions.a 仍 57 符号（coordinator）

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/phase9-raft-shard-header-align/`
- [ ] 1.4.2 `git commit -m "chore(phase9): S14 关闭——phase9 业务逻辑续做移到 phase10+"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-phase9-raft-shard-header-align/`
