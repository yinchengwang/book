# P1-B Vector CheckpointManager 完善 — 变更提案

**变更 ID：** p1b-checkpoint-manager
**日期：** 2026-10-16
**关联 plan：** `docs/superpowers/plans/2026-10-16-p1b-checkpoint-manager.md`（待建）
**关联 spec：** `docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1 §4.2 §8 Phase 1
**关联 gap-analysis：** `docs/gap-analysis/vector.md` §7 补齐项下月计划
**关联 OPSX：** `gap-analysis-roadmap`（追溯表 Task 2.2.1）

---

## 背景

`vdb_chaos_test` 3 个用例持续失败：
- `IndexRebuildAfterKill`（line 238）
- `MultipleCrashRecovery`（line 357）
- `ConcurrentWriteWithCrash`（line 411）

这 3 个测试模拟"进程级 kill -9"——不调 `vector_api_save` 直接 destroy，期望 vector_api 从 WAL/Checkpoint 恢复数据。

### 根因（探查结论）

`engineering/src/db/api/vector_api.c` 整体**没有集成** `vector_wal_*` API：

| 函数 | 行号 | 现状 | 缺口 |
|------|------|------|------|
| `vector_api_insert` | 400 | 写内存 + 标 dirty | **不调** `vector_wal_append` |
| `vector_api_save` | 606 | 写 `collections.meta` snapshot | **不调** `vector_wal_flush` / `vector_wal_checkpoint` |
| `vector_api_load` | 667 | 读 `collections.meta` | **不调** `vector_wal_replay` |

`engineering/src/db/storage/vector/vector_wal.c` 自身 API 完整（`vector_wal_append` / `vector_wal_flush` / `vector_wal_checkpoint` / `vector_wal_replay` / `vector_wal_truncate` 全部存在），但**无人调用**。

### P1-A 验收误判

P1-A result.md 报告 "vdb_wal_recovery_test 4/4 + vdb_e2e WALRecovery/CheckpointTest 全绿"。这两个测试的共同点：
- 都**先调** `vector_api_save` 再 destroy
- `vector_api_save` 写完整 `collections.meta` snapshot，足够覆盖场景
- `vector_api_load` 读 snapshot 即可恢复

但 vdb_chaos **不调 save**——这才是真实"进程级 kill -9"场景，**暴露 vector_api 没接 WAL** 的真实缺口。

## What Changes

### 必做项
1. **`vector_api_insert` 接入 WAL**：每次 insert 调 `vector_wal_append`（按 segment + vector_id 记录）
2. **`vector_api_save` 加 WAL 刷盘**：save 时先调 `vector_wal_flush` 再写 snapshot，调 `vector_wal_checkpoint` 记录 checkpoint LSN
3. **`vector_api_load` 加 WAL 重放**：load 时先读 snapshot，再调 `vector_wal_replay` 从 `last_checkpoint_lsn` 重放（idempotent insert）
4. **CheckpointManager 后台线程**：周期触发 `vector_wal_checkpoint` + `vector_wal_truncate`（避免 WAL 无限增长）
5. **修复 vdb_chaos 3 个失败用例**：IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash
6. **新增独立单元测试** `vdb_checkpoint_test.cpp`（5 个用例覆盖：周期触发 / checkpoint LSN 起点 / truncate 后 replay / 并发安全 / 幂等 insert）

### 必做的依赖前置
- 沿用 P1-A 的 `9e3ae438d` 4 个独立单元测试（`vdb_wal_recovery_test.cpp`）作为基础
- 沿用 P1-A 的基础设施修复（`f7ea2f7cc` + `cb70175cc` 等）

## 验收

- [ ] `vdb_chaos_test::IndexRebuildAfterKill` 通过
- [ ] `vdb_chaos_test::MultipleCrashRecovery` 通过
- [ ] `vdb_chaos_test::ConcurrentWriteWithCrash` 通过
- [ ] `vdb_checkpoint_test` 5/5 用例全过
- [ ] `vdb_wal_recovery_test`（P1-A）4/4 仍全过（无回归）
- [ ] `vdb_e2e_test::WALRecovery` / `CheckpointTest`（P1-A）仍全过（无回归）

## Non-Goals

- 不动 `vector_wal.c` 内部 API（已完整）
- 不动 CheckpointManager 的多模态扩展（独立 OPSX 变更 P1-D）
- 不修复 pre-existing 的 vdb_e2e SearchVectors / DropCollection 失败
- 不改 `vector_persist.h` 公共接口

## 路线图关联

`gap-analysis-roadmap` OPSX 变更 Task 2.2.1 高优先级 P0 候选。Phase 1 路线图第 1 月基础设施 P0 集中。
