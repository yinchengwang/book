# P1-C Chaos Recovery 加固 — 实施计划

> **追溯：** `openspec/changes/p1c-chaos-recovery/proposal.md` §验收
> **关联 spec：** `docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1 §4.2 §6.1 §8 Phase 1 P1-C
> **关联 roadmap：** `openspec/changes/gap-analysis-roadmap/tasks.md` §2.2.2
> **执行分支：** `feature/p1c-chaos-recovery`（基于 main HEAD `03eb3f91e`）

## 全局约束

1. **chaos_e2e 必须 5/5 全绿**（kill-during-checkpoint / kill-during-replay / partial-write-recovery / short-interval-stress / concurrent-save-crash）
2. **vdb_chaos 必须 8/8 全绿**（原 6 + 新增 2-3 短 interval chaos 子用例）
3. **回归无破坏**（vdb_checkpoint 6/6 + vdb_wal_recovery 4/4 + vdb_e2e 11/13 + vdb_stress 5/5 = 26 关键路径不变）
4. **CheckpointManager 默认间隔 5000ms**（保持 P1-B 行为，chaos 测试用 100ms 显式传参）
5. **不在 main 分支执行任何代码改动**（CLAUDE.md 铁律）
6. **WAL 文件后缀 `.log`**（`wal_make_path` 实际拼 `.log`，不是 `.wal`，P1-B Task 3 教训）
7. **commit 即 push，不批量推送**（CLAUDE.md 提交推送纪律）
8. **测试 fixture 用 std::filesystem 跨平台**（Windows cmd.exe 不支持 `mkdir -p`）
9. **代码注释用中文**（CLAUDE.md 项目语言规范）
10. **只提交变更相关代码**（OPSX 操作纪律）

## Task 列表

### Task 1：OPSX 骨架 + plan + 分支
- 建 `openspec/changes/p1c-chaos-recovery/{proposal,design,tasks}.md` + notes/
- 写本 plan
- `git checkout -b feature/p1c-chaos-recovery` 基于 main HEAD `03eb3f91e`

### Task 2：探查 chaos 现状 + 间隔参数化路径
- 读 `vdb_chaos_test.cpp` 6 用例
- 读 `vector_api.c` create + destroy 路径（约 240-300 行 + 285-291）
- 读 `checkpoint_manager.c` 线程主循环
- 读 `vector_api.h` 公共 API
- 输出 `notes/task2-explore.md` ≥ 150 行（API 行为表 + 间隔参数化方案 + destroy 顺序 + 新加 mutex 位置）

### Task 3：写 5 个 chaos_e2e 红测试
- `engineering/test/db/integration/chaos_e2e_test.cpp`（5 个 TEST_F）
- `engineering/test/db/integration/CMakeLists.txt` 注册 target
- 5/5 红（编译可能因 interval_ms 缺失而红）

### Task 4：CheckpointManager 间隔参数化
- `vector_api.h` 加 `vector_api_create_with_interval(data_dir, interval_ms)`
- `vector_api.c` 改 `vector_api_create` 委托 `_with_interval(data_dir, 5000)`
- `vector_api.c` 实现 `vector_api_set_checkpoint_interval()`（mutex 保护 stop+create）
- `VectorAPI_s` 加 `checkpoint_interval_ms` 字段

### Task 5：实现 chaos_e2e 5 + vdb_chaos 扩展
- chaos_e2e 5 用例全绿
- vdb_chaos 加 2-3 个短 interval chaos 子用例

### Task 6：跑全套 + 回归 + 收口 + 合并
- 跑 vdb_chaos 8 + chaos_e2e 5 + vdb_checkpoint 6 + vdb_wal_recovery 4 + vdb_e2e 2 + vdb_stress 5
- 写 result.md + 更新 gap-analysis-roadmap + ff-merge main + push

## 任务依赖

```
Task 1 ──> Task 2 ──> Task 3 ──> Task 4 ──> Task 5 ──> Task 6
   │          │          │          │          │          │
   ↓          ↓          ↓          ↓          ↓          ↓
 OPSX     探查报告   5 红测试   间隔参数化  实现全绿   合并 main
 骨架                CMakeLists          + vdb_chaos
                                          扩展
```

## 验收清单

- [ ] chaos_e2e 5/5 全绿
- [ ] vdb_chaos 8/8 全绿
- [ ] vdb_checkpoint 6/6 全绿
- [ ] vdb_wal_recovery 4/4 全绿
- [ ] vdb_e2e 11/13（pre-existing 2 不变）
- [ ] vdb_stress 5/5 全绿
- [ ] result.md 完整
- [ ] gap-analysis-roadmap Task 2.2.2 标 [x]
- [ ] feature/p1c-chaos-recovery ff-merge 到 main 并 push
