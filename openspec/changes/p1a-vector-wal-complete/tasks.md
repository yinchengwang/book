# P1-A Vector WAL 完整化 — 任务清单

> 详细步骤见 `docs/superpowers/plans/2026-10-16-vector-wal-complete.md`

## 任务列表

- [ ] **Task 1**：建 OPSX 变更骨架（proposal.md / design.md / tasks.md / notes/）  ← **已完成**（本目录已建好）
- [ ] **Task 2**：探查 vector_api_save / load 当前实现，定位失败根因
- [ ] **Task 3**：写失败的单元测试 `vdb_wal_recovery_test.cpp`（红）
- [ ] **Task 4**：实现 WAL 强制刷盘（save 路径修改）
- [ ] **Task 5**：实现 WAL 重放（load 路径修改 + `vector_wal_replay` 函数）
- [ ] **Task 6**：跑 vdb_e2e_test 验证 WALRecovery / CheckpointTest 通过
- [ ] **Task 7**：收口 + 文档 + 准备合并回 main

## 完成状态

- [x] Task 1（OPSX 骨架）
- [ ] Task 2-7（在 `feature/p1a-vector-wal-complete` 分支实施）

## 依赖前置

- [x] 修复 cmake configure 阻塞（`f7ea2f7cc`）：让 cmake configure 在 main 上跑通

## 关联

- Spec：`docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md`
- Plan：`docs/superpowers/plans/2026-10-16-vector-wal-complete.md`