# P1-C Chaos Recovery 加固

**关联设计：** `design.md`
**关联任务：** `tasks.md`
**追溯：** `openspec/changes/gap-analysis-roadmap/tasks.md` §2.2.2

## 背景

P1-B（`p1b-checkpoint-manager`）已把 `vdb_chaos` 3 失败用例（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash）转绿，但 chaos 覆盖范围仍是 6 个测试，写法停留在"kill → 重新加载 → 验证 size"的初级层次：

| 现状 | 不足 |
|------|------|
| `vdb_chaos` 6 用例写完就 kill | 未覆盖 kill-during-checkpoint（正在 flush/checkpoint/truncate 中途被杀） |
| CheckpointManager 周期硬编码 5s | 未覆盖短 interval（100ms）的 stress 场景，stress 概率提不上去 |
| 测试 fixture 用 128 维 1000 向量 | 未覆盖 partial write 场景（WAL 末尾字节被破坏） |
| 未测 replay 过程中被杀 | 未测 replay 进行中崩溃的幂等性 |
| 并发写后 kill | 未测并发写 + save 同时进行时 kill 的恢复 |

## 修改点

| 区域 | 当前 | 目标 |
|------|------|------|
| `vector_api_create` | 仅 `data_dir` | 加可选 `interval_ms`（默认 5000），传入 CheckpointManager |
| `VectorAPI_s` | 无 checkpoint_interval 字段 | 加 `uint32_t checkpoint_interval_ms` |
| `VectorAPI_s` | 5s 硬编码 | 暴露 `vector_api_set_checkpoint_interval()` setter（运行时调） |
| `engineering/test/db/integration/` | 仅 `vdb_chaos_test.cpp` | 新增 `chaos_e2e_test.cpp`（5 用例：kill-during-checkpoint / kill-during-replay / partial-write-recovery / short-interval-stress / concurrent-save-crash） |
| `engineering/test/db/integration/vdb_chaos_test.cpp` | 6 用例 | 加 2-3 个短 interval chaos 子用例 |

## 验收

1. **新增 `chaos_e2e` 套件 5/5 全绿**（短 interval + 多种 kill 模式 + partial write）
2. **`vdb_chaos` 套件 8/8 全绿**（原 6 + 新增 2-3 短 interval 子用例）
3. **回归无破坏**：
   - `vdb_checkpoint` 6/6
   - `vdb_wal_recovery` 4/4
   - `vdb_e2e` 11/13（pre-existing 2 失败不变）
   - `vdb_stress` 5/5
4. **手动 chaos e2e**：用 100ms 间隔跑 100 次 insert→kill→recover，零数据丢失

## Non-Goals

- **不**重构 `vector_wal_replay`（P1-D 技术债）
- **不**实现 WAL 段切换 / 滚动（multi-segment 留作未来工作）
- **不**实现分布式 chaos（节点间网络分区）
- **不**修复 pre-existing vdb_e2e SearchVectors / DropCollection（Task 2.2.3 独立 OPSX）
- **不**改 vdb_chaos 已有 6 用例的实现（仅追加新用例）

## 关联 spec

- `docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1（持久化）、§4.2（Vector 持久化）、§6.1（chaos 失败）、§8 Phase 1 P1-C
- `docs/gap-analysis/vector-sub-report.md` §7（chaos 相关 P0 项）

## 估计

- 投入：~3 周（按 gap-analysis 路线图）
- 代码量：~600 行（5 测试 + 1-3 扩展 + 间隔参数化 + 设计文档）
- 任务数：6 Task TDD（1 OPSX + 1 探查 + 1 红测试 + 1 间隔参数化 + 1 实现 + 1 收口）
