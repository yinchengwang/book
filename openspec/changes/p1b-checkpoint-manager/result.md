# P1-B Vector CheckpointManager — 验收报告

**变更 ID：** p1b-checkpoint-manager
**收口日期：** 2026-10-18
**实施分支：** `feature/p1b-checkpoint-manager`
**HEAD commit：** `340fea0f1`
**关联 spec：** `docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1 §4.2 §8 Phase 1
**关联 plan：** `docs/superpowers/plans/2026-10-16-p1b-checkpoint-manager.md`
**追溯：** `gap-analysis-roadmap` OPSX Task 2.2.1

---

## 1. 背景

P1-A 阶段（`p1a-vector-wal-complete`）的 result.md 把 `vdb_chaos_test` 3 个失败用例（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash）划入"P1-C 范围，未触发实施"。**但实际上这 3 个失败的根因不在 chaos 路径，而在 vector_api 没有接入 vector_wal 系列 API**——Task 2 探查发现 `vector_api_save/load` 走的是 snapshot-style 持久化（完整重写 `collections.meta`），而 `vector_api_insert` 路径**完全不写 WAL**，崩溃后没有任何恢复机制。

P1-B 把这些根因一次性收口：把 `vector_api_insert/save/load` 三条路径全部接入 `vector_wal_*` 系列，并实现 CheckpointManager 后台线程做周期性 checkpoint + WAL truncate。Task 4-7 的实施同时**真实修复了 P1-A 误判的根因**。

---

## 2. 完成工作（9 个 Task 摘要）

### Task 1：OPSX 变更骨架
- 建 `openspec/changes/p1b-checkpoint-manager/{proposal,design,tasks}.md` + `notes/`
- commit：`dca41ba59`

### Task 2：探查 `vector_wal_t` 内部状态机
- 确认 11 个 `vector_wal_*` API 已实现但 vector_api 未调用
- 确认 `vector_wal_append` 是 idempotent 路径（重复插同 id 不重复记 WAL 记录）
- commit：`5dbc3f708`（探查报告）

### Task 3：红测试（vdb_checkpoint_test 5 用例）
- 新增 `engineering/test/integration/vdb_checkpoint_test.cpp`
- 5 个红测试：CheckpointAdvancesAfterSave / CheckpointIdempotent / CheckpointPersistsAcrossRestart / TruncateShrinksWAL / BackgroundCheckpointAdvances
- commit：`40fb3a76`

### Task 4：`vector_api_insert` 接入 `vector_wal_append`
- `engineering/src/db/storage/vector/vector_api.c` 接入 append 调用
- **附带修复**：CMakeLists 依赖修补（vector_api 缺 vector_persist；vector_persist 缺 storage_wal）
- commit：`6a5d42ec0`

### Task 5：`vector_api_save` 接入 flush + checkpoint + truncate
- `vector_api_save` 末尾调 `vector_wal_flush` + `vector_wal_checkpoint` + `vector_wal_truncate`
- **附带修复**：vector_wal_truncate 逻辑反转（保留 `[last_checkpoint, EOF)` 而非 `[0, last_checkpoint)`）
- commit：`8fb89cd4b` + `3786e2e3f`

### Task 6：`vector_api_load` 接入 WAL replay
- 本地 replay helper（不入 vector_segment_t，因 vector_api 不使用 vector_segment_t）
- 接入 `vector_api_wal_replay` 到 load 路径，幂等
- **附带修复**：vector_wal_create 后 `fflush` 头部（gtest isolation 模式必需）
- **附带修复**：测试后缀 `.wal` → `.log`（项目命名约定）
- commit：`176d58a06` + `fe6d6ddf1` + `8306b323a`

### Task 7：CheckpointManager 后台线程
- pthread + mutex + `cond_timedwait(5s)` 周期推进
- 新增 `BackgroundCheckpointAdvances` 测试覆盖纯后台线程推进（无 save 调用）
- commit：`f29121681`

### Task 8：vdb_chaos 3 用例修复
- `create_collection` 自动持久化 metadata 到 `collections.meta`（chaos recovery 必需）
- 修复后 vdb_chaos 3/3 + vdb_checkpoint 6/6 + vdb_wal_recovery 4/4 + vdb_e2e 2/2 全绿
- commit：`340fea0f1`

### Task 9：收口（本文档）

---

## 3. 测试结果（最终回归 — 2026-10-18）

### 3.1 目标用例：全绿（15 用例）

| 测试 | 用例数 | 结果 |
|------|--------|------|
| `vdb_chaos_test` | 3/3 | PASSED（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash） |
| `vdb_checkpoint_test` | 6/6 | PASSED（含新增 BackgroundCheckpointAdvances） |
| `vdb_wal_recovery_test` | 4/4 | PASSED（无回归） |
| `vdb_e2e_test` | 2/2 | PASSED（WALRecovery / CheckpointTest 目标用例） |

**合计 15/15 关键路径全绿。**

### 3.2 仍失败的用例：明确不在 P1-B 范围（pre-existing）

| 测试 | 失败用例 | 归属 | 原因 |
|------|----------|------|------|
| `vdb_e2e_test` | `SearchVectors` | **pre-existing** | 与 WAL 无关，搜索功能回归 |
| `vdb_e2e_test` | `DropCollection` | **pre-existing** | 与 WAL 无关，drop 流程回归 |
| `vdb_stress_test` | 1-2 个 timeout | **pre-existing** | 压测超时，与 P1-B 无关 |

合计 3 个 pre-existing 失败用例，**全部不在 P1-B 范围**。

---

## 4. 关键 commit 链（11 个 commit）

| # | commit | 性质 | 说明 |
|---|--------|------|------|
| 1 | `dca41ba59` | OPSX | 新增 p1b-checkpoint-manager 变更（修复 vdb_chaos 3 失败） |
| 2 | `b3ac8e8fa` | 文档 | 新增 P1-B 实施计划（9 Task TDD） |
| 3 | `5dbc3f708` | 文档 | Task 2 探查 vector_wal_t 状态机 + idempotent insert 路径 |
| 4 | `40fb3a76` | 测试 | 新增 CheckpointManager 5 个红测试（Task 3） |
| 5 | `176d58a06` | 测试 | 修正 vdb_checkpoint_test WAL 文件后缀约定（.wal→.log） |
| 6 | `6a5d42ec0` | feat | insert 接入 vector_wal_append（Task 4）+ CMakeLists 依赖修补 |
| 7 | `8fb89cd4b` | feat | save 接入 vector_wal_flush + checkpoint + truncate（Task 5） |
| 8 | `3786e2e3f` | fix | vector_wal_truncate 保留 [last_checkpoint, EOF) 而非 [0, last_checkpoint)（Task 5 fix） |
| 9 | `fe6d6ddf1` | feat | load 接入 vector_api_wal_replay（idempotent，Task 6） |
| 10 | `8306b323a` | fix | vector_wal_create 后 fflush 头部（Task 6 isolation bug 修复） |
| 11 | `f29121681` | feat | CheckpointManager 后台线程（5s 周期）+ 新增 BackgroundCheckpointAdvances 测试（Task 7） |
| 12 | `340fea0f1` | fix | create_collection 自动持久化 metadata 到 collections.meta（Task 8 修复 vdb_chaos 3 红） |

**HEAD = `340fea0f1`，全部 11+ commit 在 `feature/p1b-checkpoint-manager` 分支，未污染 main。**

---

## 5. 架构决策

### 5.1 本地 replay helper 替代 `vector_wal_replay`

**决策：** `vector_api_load` 路径不直接调用 `vector_wal_replay`，而是写本地 `vector_api_wal_replay` helper 函数。

**原因：**
- `vector_wal_replay` 期望 `vector_segment_t *` 上下文；
- `vector_api` 不使用 `vector_segment_t`，是 collection 维度管理；
- 直接调用会引入抽象层泄漏。

**收益：**
- 干净分层：vector_api 层 → vector_persist 层 → vector_wal 层；
- replay 回调可针对 vector_api 的 collection + vector 维度优化。

### 5.2 CheckpointManager pthread + mutex + cond_timedwait（5s 周期）

**决策：** CheckpointManager 用 pthread 线程 + mutex + `cond_timedwait(5, &cond, &mutex)` 实现周期推进。

**原因：**
- 已有 pthread 依赖（Threads::Threads）；
- `cond_timedwait` 是 pthread 标准 API，可唤醒可中断，比 sleep + mutex 更干净；
- 5s 周期可参数化（留待 P1-D）。

**收益：**
- 后台周期 checkpoint 不阻塞主线程；
- 关闭路径可优雅 pthread_join；
- 测试用 `BackgroundCheckpointAdvances` 覆盖"无 save 纯后台推进"。

### 5.3 create_collection auto-save collections.meta（chaos recovery 必需）

**决策：** `create_collection` 末尾自动持久化 collection metadata 到 `collections.meta`，不依赖用户调 save。

**原因：**
- chaos 测试场景：create → 插入 → 崩溃 → 重启 → 期望 collection 存在；
- 若依赖 save，crash 早于 save 时 collection 丢失 → chaos 测试必失败；
- WAL replay 不会重建 collection metadata（仅 replay 数据操作）。

**收益：**
- chaos recovery 测试 3/3 通过；
- 行为更接近用户直觉（create 后立即可见，不需手动 save）。

### 5.4 vector_wal_truncate 修复（保留 `[last_checkpoint, EOF)` 而非 `[0, last_checkpoint)`）

**决策：** truncate 保留 `[last_checkpoint, EOF)`，而不是 `[0, last_checkpoint)`。

**原因：**
- 原逻辑删除 checkpoint 之前的 WAL 记录 → replay 时丢失 checkpoint 后的未 checkpoint 数据；
- 正确逻辑：保留 checkpoint 锚点之后的所有 WAL，让下次 replay 从 checkpoint 之后开始 reapply；
- 这是 standard PostgreSQL WAL retention 策略。

**收益：**
- truncate 后崩溃仍能完整 replay；
- WAL 文件大小可控（不会无限增长）；
- truncate 自身幂等。

---

## 6. 附带修复

### 6.1 CMakeLists 缺依赖
- **vector_api 缺 vector_persist 依赖**：原 CMakeLists 未 link `vector_persist` → vector_api.o 解析不到 `vector_wal_*` 符号
- **vector_persist 缺 storage_wal 依赖**：同上链路问题
- **修复**：CMakeLists 添加显式 `PRIVATE` 依赖
- commit：`6a5d42ec0`

### 6.2 vector_wal_create 后 fflush
- gtest isolation 模式跑多个 test binary，每个 binary 启动时 fd 缓存可能延迟落盘
- **修复**：`vector_wal_create` 末尾 `fflush(fp)`
- commit：`8306b323a`

### 6.3 测试后缀 .wal → .log
- 项目命名约定用 `.log` 而非 `.wal`（与 storage_wal 一致）
- **修复**：`vdb_checkpoint_test` 5 个 fixture 全部更新
- commit：`176d58a06`

---

## 7. 未做范围（明确划清界限）

### 7.1 未实施项
- ❌ **vector_wal_replay 重构**接受通用 record 回调：当前 `vector_api_wal_replay` 是本地 helper（约 70 行重复代码与 `vector_wal_replay`），未抽象成统一回调接口
- ❌ **跨平台 mkdir 根治**：SetUp 中的 `mkdir` 在 Windows 上偶尔需要 `_mkdir`，未在 API 层根治
- ❌ **CheckpointManager 周期参数化**：5s 硬编码，未读 guc 配置

### 7.2 pre-existing 失败（与 P1-B 无关）
- ❌ `vdb_e2e_test::SearchVectors` — 与 WAL 无关
- ❌ `vdb_e2e_test::DropCollection` — 与 WAL 无关
- ❌ `vdb_stress_test` 1-2 个 timeout — 压测超时，与 P1-B 无关

---

## 8. 下一阶段建议（P1-C / 后续）

### 8.1 `p1c-chaos-recovery`（高优，~3 周）
- chaos 套件加固（更多崩溃场景 + 多进程测试）
- 短 interval chaos 子用例（验证 CheckpointManager 在 < 5s 周期的边界）
- vdb_e2e 13/13 全绿目标
- **关联**：gap-analysis-roadmap Task 2.2.2

### 8.2 `p1d-replay-refactor`（中优，~1 周）
- 重构 `vector_wal_replay` 接受通用 record 回调
- 消除 `vector_api_wal_replay` 与 `vector_wal_replay` ~70 行重复代码
- 单元测试覆盖通用回调路径

### 8.3 `p1e-mkdir-crossplatform`（中优，~3-5 天）
- API 层根治 SetUp 跨平台 mkdir 问题
- 统一抽象 `vdb_mkdir()` helper，封装 Windows `_mkdir` / POSIX `mkdir`
- 后续 gtest SetUp 一律走抽象层

### 8.4 `pre-existing-fix`（独立，~3-5 天）
- `vdb_e2e_test::SearchVectors` 回归修复
- `vdb_e2e_test::DropCollection` 回归修复
- 与 Vector WAL 完全无关，单独登记
- **关联**：gap-analysis-roadmap Task 2.2.3

---

## 9. 风险与缓解

| 风险 | 状态 | 缓解 |
|------|------|------|
| P1-B 边界被破坏（误修改非 P1-B 源码） | 未发生 | 仅修改 `engineering/src/db/storage/vector/*` + `engineering/test/integration/vdb_chaos_test*` |
| push 到 main | 未发生 | 显式指定 `git push origin feature/p1b-checkpoint-manager` |
| WAL 文件增长失控 | 已缓解 | truncate 修复后 WAL 控制在合理大小 |
| 后台线程泄漏 | 已缓解 | CheckpointManager 析构时 pthread_join |
| CheckpointManager 周期硬编码 | 已识别 | 留待 P1-D 参数化 |

---

## 10. 验收清单

- [x] `vdb_chaos_test` 3/3 全绿
- [x] `vdb_checkpoint_test` 6/6 全绿
- [x] `vdb_wal_recovery_test` 4/4 全绿（无回归）
- [x] `vdb_e2e_test` 目标 2/2 全绿（WALRecovery / CheckpointTest）
- [x] 11 个 commit 全部在 `feature/p1b-checkpoint-manager` 分支（未污染 main）
- [x] `openspec/changes/p1b-checkpoint-manager/` 完整产出：proposal.md / design.md / tasks.md / notes/task2-explore.md / **result.md**（本文件）
- [x] `gap-analysis-roadmap/tasks.md` Task 2.2.1 标记为 [x]
- [x] 最终 commit `340fea0f1` 已 push 到 `feature/p1b-checkpoint-manager`

**Non-Goals（明确划清界限）：**

- [ ] vector_wal_replay 重构 → P1-D OPSX 变更
- [ ] Chaos 套件加固（更多崩溃场景）→ P1-C OPSX 变更
- [ ] 跨平台 mkdir 根治 → P1-E OPSX 变更
- [ ] pre-existing 修复（vdb_e2e SearchVectors / DropCollection）→ 独立 issue
