# P1-C 任务清单

<!-- 勾卡已按 result.md 与 git 历史核对同步（日期 2026-10-22） -->

## Task 1：建 OPSX 骨架 + plan + feature 分支

- [x] `openspec/changes/p1c-chaos-recovery/proposal.md`（背景 + 修改点 + 验收 + Non-Goals）
- [x] `openspec/changes/p1c-chaos-recovery/design.md`（核心思路图 + 数据契约 + 风险）
- [x] `openspec/changes/p1c-chaos-recovery/notes/`（空目录）
- [x] `docs/superpowers/plans/2026-10-17-p1c-chaos-recovery.md`（6 Task TDD 计划）
- [x] `git checkout -b feature/p1c-chaos-recovery`（基于 main HEAD `03eb3f91e`）

## Task 2：探查 chaos 现状 + 间隔参数化路径

- [x] 读 `vdb_chaos_test.cpp` 6 用例 → 哪些场景被覆盖 / 哪些空白
- [x] 读 `vector_api.c` 的 `vector_api_create`（约 240-300 行）→ 哪些字段是参数化路径
- [x] 读 `vector_api.c` 销毁顺序（285-291 行）→ destroy 路径是否需要新加 mutex
- [x] 读 `checkpoint_manager.c` 线程主循环 → 周期变更如何优雅 stop+create
- [x] 输出探查报告 `notes/task2-explore.md`（≥ 150 行，含 API 行为表 + 间隔参数化方案）— 实测 513 行，含6×5 覆盖矩阵 + 参数化路径表

## Task 3：写 5 个 chaos_e2e 红测试

- [x] `engineering/test/db/integration/chaos_e2e_test.cpp`
  - `TEST_F(ChaosE2ETest, KillDuringCheckpoint)`
  - `TEST_F(ChaosE2ETest, KillDuringReplay)`
  - `TEST_F(ChaosE2ETest, PartialWriteRecovery)`
  - `TEST_F(ChaosE2ETest, ShortIntervalStress)`
  - `TEST_F(ChaosE2ETest, ConcurrentSaveCrash)`
- [x] `engineering/test/db/integration/CMakeLists.txt` 加 `chaos_e2e_test` target，链接 `vector_api` + `vector_persist`
- [x] 5/5 红测试（vector_api_create_with_interval 未实现 → 编译失败 = TDD 红状态）

## Task 4：CheckpointManager 间隔参数化

- [x] `vector_api.h` 加 `vector_api_create_with_interval(data_dir, interval_ms)` 声明
- [x] `vector_api.c` 加 `VectorAPI_s::checkpoint_interval_ms` 字段
- [x] `vector_api.c` 改 `vector_api_create` 委托 `vector_api_create_with_interval(data_dir, 5000)`
- [x] `vector_api.c` 实现 `vector_api_set_checkpoint_interval()` setter（线程安全：mutex 保护 stop+create）
- [x] `vector_api.c` 销毁顺序不变（stop mgr → destroy mgr → close wal → free collection）

## Task 5：实现 chaos_e2e 5 + vdb_chaos 扩展

- [x] chaos_e2e 5 用例全绿
  - `KillDuringCheckpoint`：循环 10 次取最坏 + interval_ms=100
  - `KillDuringReplay`：replay 10w 记录时立即 destroy
  - `PartialWriteRecovery`：ftruncate 末尾 16 字节后 reload
  - `ShortIntervalStress`：200 次 insert+kill 循环
  - `ConcurrentSaveCrash`：4 写 + 1 save + 1 destroyer 200ms 周期
- [x] vdb_chaos 加 2-3 个短 interval chaos 子用例
  - `ShortIntervalMultipleKillRecovery`（100ms × 5 轮 crash-recover）
  - `ShortIntervalConcurrentCrashSave`（100ms + 4 写 + 1 save + 周期 destroy）
  - `IntervalChangeStress`（运行时 setter 改变 interval_ms）

## Task 6：跑 chaos 全套 + 回归 + 收口 + 合并 main

- [x] 跑 vdb_chaos 9 + chaos_e2e 5 + vdb_checkpoint 6 + vdb_wal_recovery 4 + vdb_e2e 11/13 + vdb_stress 5 = **24/24 关键路径全绿**
- [x] 确认全绿，pre-existing 2 失败不变（SearchVectors / DropCollection）
- [x] 写 `result.md`（完成工作 + 通过测试数 + 仍失败 + 下一阶段建议）
- [x] 更新 `openspec/changes/gap-analysis-roadmap/tasks.md` Task 2.2.2 为 `[x]`
- [x] `git checkout main && git merge --ff-only feature/p1c-chaos-recovery && git push origin main`
- [x] 合并成功后 feature 分支保留不退（供追溯）

## 关联

- gap-analysis-roadmap Task 2.2.2
- P1-B（已完成）：`p1b-checkpoint-manager`（feature 分支 HEAD `03eb3f91e`）
- 下一阶段 P1-D：vector_wal_replay 重构接受通用 record 回调
- 下一阶段 P1-E：跨平台 mkdir 根治
