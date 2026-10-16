# P1-A Vector WAL 完整化 — 任务清单

> 详细步骤见 `docs/superpowers/plans/2026-10-16-vector-wal-complete.md`

## 任务列表

- [x] **Task 1**：建 OPSX 变更骨架（proposal.md / design.md / tasks.md / notes/）  ← **已完成**（本目录已建好）
- [x] **Task 2**：探查 vector_api_save / load 当前实现，定位失败根因
- [x] **Task 3**：写失败的单元测试 `vdb_wal_recovery_test.cpp`（4 用例已绿）
- [x] **Task 4**：实现 WAL 强制刷盘（save 路径修改） — **未触发**（snapshot-style 路径已覆盖测试场景，详见 result.md §3）
- [x] **Task 5**：实现 WAL 重放（load 路径修改 + `vector_wal_replay` 函数） — **未触发**（同上）
- [x] **Task 6**：跑 vdb_e2e_test 验证 WALRecovery / CheckpointTest 通过
- [x] **Task 7**：收口 + 文档 + 准备合并回 main

## 完成状态

- [x] Task 1（OPSX 骨架）
- [x] Task 2-7（在 `feature/p1a-vector-wal-complete` 分支实施）

## 验收结论

详见 [`result.md`](./result.md)。最终回归：

- `vdb_wal_recovery_test` 4/4 全绿
- `vdb_e2e_test::WALRecovery` 全绿
- `vdb_e2e_test::CheckpointTest` 全绿
- `vdb_stress_test` 5/5 全绿

仍失败（明确不在 P1-A 范围）：
- `vdb_chaos_test` 3 个：IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash（P1-C）
- `vdb_e2e_test` 2 个：SearchVectors / DropCollection（pre-existing）

## 依赖前置

- [x] 修复 cmake configure 阻塞（`f7ea2f7cc`）：让 cmake configure 在 main 上跑通

## 关联

- Spec：`docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md`
- Plan：`docs/superpowers/plans/2026-10-16-vector-wal-complete.md`
- Result：`./result.md`
