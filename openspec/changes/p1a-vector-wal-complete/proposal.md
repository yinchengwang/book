# P1-A Vector WAL 完整化 — 变更提案

**变更 ID：** p1a-vector-wal-complete
**日期：** 2026-10-16
**关联 plan：** `docs/superpowers/plans/2026-10-16-vector-wal-complete.md`
**关联 spec：** `docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1 §4.2 §8 Phase 1

---

## 背景

`vdb_e2e_test` 中两个用例持续失败：

- `WALRecovery`（487 行）：模拟崩溃后无法从 WAL 恢复数据
- `CheckpointTest`（530 行）：checkpoint 后 destroy + load 数据丢失

`vector_api_save()` 仅触发 page flush，未强制 WAL 刷盘；`vector_api_load()` 未实现 WAL 重放路径。

## What Changes

### 必做项
1. `vector_api_save()` 调用 `wal_flush()`，确保 WAL 强制刷盘
2. `vector_api_load()` 调用 `vector_wal_replay()`，重放 `WAL_LOG_VECTOR_APPEND` Record
3. 新增 `vector_wal_replay()` 函数（`vector_wal.h/c`）
4. 新增单元测试 `vdb_wal_recovery_test.cpp`（4 个用例）
5. CMakeLists.txt 注册新测试

### 必做的依赖前置（已在 feature 分支完成）
- 修复 cmake configure 阻塞（`f7ea2f7cc`）：test_memctx 重复 / GNUInstallDirs / cpp-httplib 跳过 / cmocka 源不全跳过 / test_canonical_parser 死代码

## 验收

- `vdb_wal_recovery_test` 4/4 通过
- `vdb_e2e_test::WALRecovery` 通过
- `vdb_e2e_test::CheckpointTest` 通过
- 全部 commit 在 `feature/p1a-vector-wal-complete` 分支

## Non-Goals

- ❌ CheckpointManager（独立 P1-B OPSX 变更）
- ❌ Chaos 恢复路径（独立 P1-C OPSX 变更）
- ❌ 其他模态 WAL（KV / TS / 等）

## 风险

| 风险 | 应对 |
|------|------|
| HNSW 索引重建耗时 | 增量 replay + 按 vector_id 去重 |
| WAL record payload schema 未文档化 | 在 design.md 中固定 |
| checkpoint_lsn 起点不正确 | 测试覆盖 save / load / save 三态 |