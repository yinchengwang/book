# P1-B 任务清单

> 详细步骤见 `docs/superpowers/plans/2026-10-16-p1b-checkpoint-manager.md`（待建）
> 每个 task 实施时独立 commit + push 到 `feature/p1b-checkpoint-manager` 分支

## 任务列表

- [ ] **Task 1**：建 OPSX 变更骨架（proposal.md / design.md / tasks.md / notes/）
- [ ] **Task 2**：探查 `vector_wal_t` 内部状态机 + 确认 idempotent insert 路径
- [ ] **Task 3**：写失败的 `vdb_checkpoint_test.cpp`（5 用例红）
- [ ] **Task 4**：实现 `vector_api_insert` 接入 `vector_wal_append`
- [ ] **Task 5**：实现 `vector_api_save` 加 `vector_wal_flush` + `vector_wal_checkpoint`
- [ ] **Task 6**：实现 `vector_api_load` 加 `vector_wal_replay`（idempotent）
- [ ] **Task 7**：实现 CheckpointManager 后台线程（周期 checkpoint + truncate）
- [ ] **Task 8**：跑 vdb_chaos 3 用例 + vdb_checkpoint 5 用例 + 回归
- [ ] **Task 9**：收口 + 归档

## 完成状态

- [x] Task 1（OPSX 骨架 proposal.md / design.md / tasks.md）

## 验收结论

（待 Task 9 完成后填写）

## 关联 OPSX 变更

- **已完成：** `p1a-vector-wal-complete`（main `3939cc03c`）
- **本变更：** `p1b-checkpoint-manager`（修复 vdb_chaos 3 失败）
- **后续候选：** `p1c-chaos-recovery`（chaos 套件加固 + e2e 扩展）
- **追溯：** `gap-analysis-roadmap` OPSX Task 2.2.1
