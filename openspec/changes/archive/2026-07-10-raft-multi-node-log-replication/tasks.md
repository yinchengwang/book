# P1 — Tasks（Raft Multi-Node + Log Replication）

> **目标**：phase11 单节点 → 多节点 + 日志复制。

## 1.1 调研

- [x] 1.1.1 已查：phase11 raft.h/c 已含基础 API
- [x] 1.1.2 决策：in-process transport（避免网络）

## 1.2 实现

- [ ] 1.2.1 `engineering/include/db/consensus/raft_transport.h`
- [ ] 1.2.2 `engineering/include/db/consensus/raft_cluster.h`
- [ ] 1.2.3 `raft_transport.c` / `raft_vote.c` / `raft_append.c` / `raft_cluster.c`
- [ ] 1.2.4 `raft_log_replication_test.cpp`
- [ ] 1.2.5 `db/consensus/CMakeLists.txt` 加新源
- [ ] 1.2.6 `test/db/consensus/CMakeLists.txt` 加新测试

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: `cmake --build engineering/build` 全部成功
- [ ] 1.3.2 V2: `raft_log_rep_test` 列出 ≥ 3 tests
- [ ] 1.3.3 V3: 所有 ctest 通过

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/raft-multi-node-log-replication/ engineering/include/db/consensus/ engineering/src/db/consensus/ engineering/test/db/consensus/`
- [ ] 1.4.2 `git commit -m "feat(raft): P1 多节点 + 日志复制"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-raft-multi-node-log-replication/`
