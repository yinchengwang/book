# P1-B 任务清单

> 详细步骤见 `docs/superpowers/plans/2026-10-16-p1b-checkpoint-manager.md`（待建）
> 每个 task 实施时独立 commit + push 到 `feature/p1b-checkpoint-manager` 分支

## 任务列表

- [x] Task 1：建 OPSX 变更骨架（proposal.md / design.md / tasks.md / notes/）
- [x] Task 2：探查 `vector_wal_t` 内部状态机 + 确认 idempotent insert 路径
- [x] Task 3：写失败的 `vdb_checkpoint_test.cpp`（5 用例红）
- [x] Task 4：实现 `vector_api_insert` 接入 `vector_wal_append`（含 build 依赖修补）
- [x] Task 5：实现 `vector_api_save` 加 `vector_wal_flush` + `vector_wal_checkpoint` + truncate
- [x] Task 6：实现 `vector_api_load` 加 `vector_wal_replay`（idempotent + fflush + truncate 修复）
- [x] Task 7：实现 CheckpointManager 后台线程（周期 checkpoint + truncate + 新测试 BackgroundCheckpointAdvances）
- [x] Task 8：跑 vdb_chaos 3 用例 + vdb_checkpoint 用例 + 回归（修复 create_collection auto-save）
- [x] Task 9：收口 + 归档

## 完成状态

- [x] Task 1-9 全部完成
- [x] **P1-B 已收口**，commit hash = `340fea0f1`（HEAD），测试全绿（vdb_chaos 3/3 + vdb_checkpoint 6/6 + vdb_wal_recovery 4/4 + vdb_e2e 2/2）

## 验收结论

- [x] `vdb_chaos_test` 3/3 全绿（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash）
- [x] `vdb_checkpoint_test` 6/6 全绿（含新增 BackgroundCheckpointAdvances）
- [x] `vdb_wal_recovery_test` 4/4 全绿（无回归）
- [x] `vdb_e2e_test` 2/2 目标用例绿（WALRecovery / CheckpointTest）
- [x] 关键 commit 11 个，全部在 `feature/p1b-checkpoint-manager` 分支，未污染 main
- [x] `result.md` 完整交付报告已生成

**详见：** `openspec/changes/p1b-checkpoint-manager/result.md`

## 关联 OPSX 变更

- **已完成：** `p1a-vector-wal-complete`（main `3939cc03c`）
- **本变更：** `p1b-checkpoint-manager`（修复 vdb_chaos 3 失败）
- **后续候选：** `p1c-chaos-recovery`（chaos 套件加固 + e2e 扩展）
- **追溯：** `gap-analysis-roadmap` OPSX Task 2.2.1
