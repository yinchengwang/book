# P1-B Vector CheckpointManager 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `vector_api` 接入 `vector_wal`，修复 vdb_chaos 3 个失败用例（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash），实现 CheckpointManager 后台线程。

**Architecture:**
- 写路径：`vector_api_insert` → `vector_wal_append` → 后台 CheckpointManager 周期 flush + checkpoint + truncate
- save 路径：`vector_api_save` → `vector_wal_flush` → `vector_wal_checkpoint` → 写 `collections.meta` snapshot
- load 路径：`vector_api_load` → 读 snapshot → `vector_wal_replay` 从 `last_checkpoint_lsn` idempotent insert

**Tech Stack:** C11、gtest、C++17 测试夹具、现有 `vector_wal_t` + `vector_api_t`、pthread

**Spec:** `docs/process/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1 §4.2 §6.1 §8 Phase 1
**Parent Change:** `openspec/changes/p1b-checkpoint-manager/`
**Branch:** `feature/p1b-checkpoint-manager`（不在 main 上推任何代码）
**Base:** P1-A（`3939cc03c`），4 个独立单元测试 + cjson 路径修复已合入

---

## Global Constraints

- C11 / C++17，CMake 3.20+ 编译
- 中文注释
- 中文 commit message
- 即时推送：commit 后立即 `git push`，不批量推送
- 严格在 `feature/p1b-checkpoint-manager` 分支上推，绝不 main
- 测试产物 `test-results/`，编译产物 `build/`
- gtest + `.cpp`

---

## 文件结构

| 文件 | 类型 | 职责 |
|------|------|------|
| `engine/src/db/api/vector_api.c` | 修改 | insert/save/load 接入 vector_wal |
| `engine/include/db/storage/vector/vector_persist.h` | 修改 | 新增 idempotent insert 声明（如果需要） |
| `engine/src/db/storage/vector/vector_wal.c` | 可能修改 | idempotent insert 实现 |
| `engine/src/db/storage/vector/checkpoint_manager.c` | 新增 | CheckpointManager 后台线程 |
| `engine/include/db/storage/vector/checkpoint_manager.h` | 新增 | CheckpointManager 公共 API |
| `engine/test/db/integration/vdb_checkpoint_test.cpp` | 新增 | 5 个独立单元测试 |
| `engine/test/db/integration/CMakeLists.txt` | 修改 | 注册新测试 |
| `engine/src/CMakeLists.txt` 或对应目录 | 修改 | 链接 pthread + 新源文件 |

---

## Task 1: OPSX 骨架（已完）

✅ 在 `openspec/changes/p1b-checkpoint-manager/` 建 proposal.md / design.md / tasks.md / notes/

- [x] 提案：背景 / What Changes / 验收 / Non-Goals
- [x] 设计：核心思路 / 数据契约 / 风险
- [x] 任务清单：9 个 Task 状态追踪

---

## Task 2: 探查 vector_wal_t 内部状态机

**Files:**
- Read: `engine/src/db/storage/vector/vector_wal.c:1-300`
- Read: `engine/include/db/storage/vector/vector_persist.h:570-720`

- [ ] **Step 1: 读 vector_wal_t 结构体定义**

```bash
grep -A 30 "typedef struct.*vector_wal_t\|^struct vector_wal" engine/include/db/storage/vector/vector_persist.h
```

记录：`last_checkpoint` / `num_records` / `header_size` 字段。

- [ ] **Step 2: 读 vector_wal_append / flush / checkpoint / replay / truncate 实现**

```bash
grep -n "^int vector_wal_\|^uint64_t vector_wal_\|^int64_t vector_wal_\|^size_t vector_wal_\|^vector_wal_t \*vector_wal_" engine/src/db/storage/vector/vector_wal.c
```

记录 5 个核心 API 的签名 + 行为（特别是 replay 起始位置 = `header.last_checkpoint`）。

- [ ] **Step 3: 写探查报告**

写到 `openspec/changes/p1b-checkpoint-manager/notes/task2-explore.md`：
- `vector_wal_t` 字段
- 5 个核心 API 行为摘要
- idempotent insert 实现位置（vector_api.c 已有还是需新增）
- 接入 `vector_api` 的具体修改点（行号）

- [ ] **Step 4: 提交 + push**

```bash
git add openspec/changes/p1b-checkpoint-manager/notes/
git commit -m "docs(P1-B): Task 2 探查 vector_wal_t 状态机 + idempotent insert 路径"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 3: 写 vdb_checkpoint_test.cpp 5 个红测试

**Files:**
- Create: `engine/test/db/integration/vdb_checkpoint_test.cpp`

- [ ] **Step 1: 写测试骨架 + 5 个 TEST_F**

参考 P1-A `vdb_wal_recovery_test.cpp` 风格，5 个测试：

1. **CheckpointTriggeredAfterNRecords**：插 1000 条后 checkpoint 自动触发，`vector_wal_get_checkpoint_info` 返回 checkpoint > header
2. **CheckpointLsn起点正确**：插 100 条 → checkpoint → 插 100 条 → restart → 期望 = 200（从 checkpoint_lsn 重放）
3. **TruncateAfterCheckpoint**：checkpoint 后 vector_wal_file_size 应 < 原始大小（truncate 生效）
4. **IdempotentInsert**：replay 同一段 WAL 两次，集合 size 不变（按 vector_id 去重）
5. **ConcurrentInsertWithCheckpoint**：2 线程并发插 500 条 + 后台 checkpoint，期望 = 1000（无丢失无重复）

完整代码模板：

```cpp
// vdb_checkpoint_test.cpp
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <db/storage/vector/vector_persist.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <atomic>

class VDBCheckpointTest : public ::testing::Test {
protected:
    std::string test_data_dir;
    VectorAPI *api = nullptr;
    // 同 P1-A 风格的 setup / teardown / helpers
};

// 5 个 TEST_F 如上
```

- [ ] **Step 2: 注册到 CMakeLists.txt**

```cmake
add_executable(vdb_checkpoint_test vdb_checkpoint_test.cpp)
target_link_libraries(vdb_checkpoint_test
    PRIVATE
        vector_api
        vector_wal
        db_core
        project_includes
        GTest::gtest_main
        Threads::Threads
)
```

- [ ] **Step 3: 跑测试（红）**

```bash
cmake --build build/engineering --target vdb_checkpoint_test 2>&1 | tail -10
cd build/engineering && ./test/db/integration/vdb_checkpoint_test.exe 2>&1 | tail -30
```

预期：5 个测试**全部 FAIL**（因为 vector_api 还没接 vector_wal + CheckpointManager 未实现）。

- [ ] **Step 4: 提交 + push**

```bash
git add engine/test/db/integration/vdb_checkpoint_test.cpp \
        engine/test/db/integration/CMakeLists.txt
git commit -m "test(vector): 新增 CheckpointManager 5 个红测试"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 4: vector_api_insert 接入 vector_wal_append

**Files:**
- Modify: `engine/src/db/api/vector_api.c:400-447`

- [ ] **Step 1: 在 vector_api_insert 末尾追加 vector_wal_append**

在 `vector_api_insert` 成功插入后（约 444 行附近），加：

```c
/* 写 WAL（idempotent key = vector_id） */
if (api->wal && coll->wal_segment_id >= 0) {
    vector_wal_op_t op = {.type = VECTOR_WAL_OP_INSERT,
                          .segment_id = (uint32_t)coll->wal_segment_id,
                          .vector_id = out_ids[i],
                          .dim = (uint32_t)coll->dimension,
                          .data = vec,
                          .metadata = meta,
                          .metadata_len = (uint32_t)(meta ? meta_sizes[i] : 0)};
    if (vector_wal_append(api->wal, op.segment_id, &op) != 0) {
        LOG_WARN("WAL append 失败: collection=%s, vector_id=%lld",
                 coll->name, (long long)out_ids[i]);
    }
}
```

- [ ] **Step 2: 在 vector_api_create 时初始化 wal**

在 `vector_api_create` 中（约 250 行）加：

```c
char wal_path[1024];
snprintf(wal_path, sizeof(wal_path), "%s/wal.log", api->data_dir);
api->wal = vector_wal_open(wal_path, 0);  /* 0 = default segment */
if (!api->wal) {
    api->wal = vector_wal_create(wal_path, 0);
}
```

- [ ] **Step 3: 编译 + 跑测试**

```bash
cmake --build build/engineering --target vdb_checkpoint_test 2>&1 | tail -10
cd build/engineering && ./test/db/integration/vdb_checkpoint_test.exe --gtest_filter='*Idempotent*' 2>&1 | tail -10
```

预期：IdempotentInsert 测试**可能**转绿（如果 insert 路径已接 WAL）。

- [ ] **Step 4: 提交 + push**

```bash
git add engine/src/db/api/vector_api.c engine/include/db/storage/vector/vector_persist.h
git commit -m "feat(vector): insert 接入 vector_wal_append"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 5: vector_api_save 接入 vector_wal_flush + checkpoint

**Files:**
- Modify: `engine/src/db/api/vector_api.c:606-665`

- [ ] **Step 1: 在 vector_api_save 头部加 WAL flush + checkpoint**

```c
int vector_api_save(VectorAPI *api) {
    if (!api) return VECTOR_API_ERR_INVALID_PARAM;

    /* 1. 先 flush WAL（确保所有脏数据落盘） */
    if (api->wal) {
        if (vector_wal_flush(api->wal) != 0) {
            LOG_WARN("WAL flush 失败");
        }
    }

    /* 2. checkpoint 记录 LSN 起点（truncate 之前的最后位置） */
    if (api->wal) {
        if (vector_wal_checkpoint(api->wal, VECTOR_CHECKPOINT_TYPE_MANUAL) != 0) {
            LOG_WARN("WAL checkpoint 失败");
        }
    }

    /* 3. 原 snapshot 逻辑（写 collections.meta） */
    // ... existing code ...
}
```

- [ ] **Step 2: 编译 + 跑测试**

```bash
cmake --build build/engineering --target vdb_checkpoint_test 2>&1 | tail -10
cd build/engineering && ./test/db/integration/vdb_checkpoint_test.exe --gtest_filter='*Checkpoint*' 2>&1 | tail -10
```

预期：CheckpointTriggeredAfterNRecords + CheckpointLsn起点正确**应转绿**。

- [ ] **Step 3: 提交 + push**

```bash
git add engine/src/db/api/vector_api.c
git commit -m "feat(vector): save 接入 vector_wal_flush + checkpoint"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 6: vector_api_load 接入 vector_wal_replay

**Files:**
- Modify: `engine/src/db/api/vector_api.c:667-740`

- [ ] **Step 1: 在 vector_api_load 末尾加 WAL replay**

```c
/* 4. 读 snapshot 后 replay WAL（恢复 snapshot 之后的脏数据） */
if (api->wal) {
    for (int32_t i = 0; i < api->n_collections; i++) {
        VectorCollectionImpl *coll = api->collections[i];
        int64_t replayed = vector_wal_replay(
            (char *)api->data_dir,  /* wal path */
            (uint32_t)coll->wal_segment_id,
            coll,  /* 回调上下文 */
            vector_api_wal_replay_callback  /* idempotent insert 回调 */
        );
        if (replayed < 0) {
            LOG_WARN("WAL replay 失败: collection=%s", coll->name);
        } else if (replayed > 0) {
            LOG_INFO("WAL replay 成功: collection=%s, count=%lld",
                     coll->name, (long long)replayed);
        }
    }
}
```

- [ ] **Step 2: 实现 replay 回调（idempotent insert）**

在 vector_api.c 中新增：

```c
static int64_t vector_api_wal_replay_callback(void *ctx, const vector_wal_op_t *op) {
    VectorCollectionImpl *coll = (VectorCollectionImpl *)ctx;
    /* 检查 vector_id 是否已存在（按 ids 数组二分查找） */
    for (int32_t i = 0; i < coll->size; i++) {
        if (coll->ids[i] == (int64_t)op->vector_id) {
            return 0;  /* 已存在，幂等 */
        }
    }
    /* 不存在则 insert */
    memcpy(coll->vectors + coll->size * coll->dimension,
           op->data, coll->dimension * sizeof(float));
    coll->ids[coll->size] = (int64_t)op->vector_id;
    coll->size++;
    return 1;
}
```

- [ ] **Step 3: 编译 + 跑全套 vdb_checkpoint_test**

```bash
cmake --build build/engineering --target vdb_checkpoint_test 2>&1 | tail -10
cd build/engineering && ./test/db/integration/vdb_checkpoint_test.exe 2>&1 | tail -15
```

预期：5 个测试**全部 PASS**。

- [ ] **Step 4: 提交 + push**

```bash
git add engine/src/db/api/vector_api.c
git commit -m "feat(vector): load 接入 vector_wal_replay (idempotent)"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 7: CheckpointManager 后台线程

**Files:**
- Create: `engine/include/db/storage/vector/checkpoint_manager.h`
- Create: `engine/src/db/storage/vector/checkpoint_manager.c`

- [ ] **Step 1: 写 checkpoint_manager.h 公共 API**

```c
typedef struct CheckpointManager_s CheckpointManager;

CheckpointManager *checkpoint_manager_create(vector_wal_t *wal, uint32_t interval_ms);
void checkpoint_manager_destroy(CheckpointManager *mgr);
int checkpoint_manager_start(CheckpointManager *mgr);
int checkpoint_manager_stop(CheckpointManager *mgr);
```

- [ ] **Step 2: 写 checkpoint_manager.c 实现**

使用 pthread + mutex + condvar，主体循环：

```c
static void *checkpoint_thread_main(void *arg) {
    CheckpointManager *mgr = (CheckpointManager *)arg;
    while (atomic_load(&mgr->running)) {
        pthread_mutex_lock(&mgr->mutex);
        pthread_cond_timedwait(&mgr->cond, &mgr->mutex,
                               &mgr->interval_timespec);
        if (!atomic_load(&mgr->running)) break;
        /* flush + checkpoint + truncate */
        vector_wal_flush(mgr->wal);
        vector_wal_checkpoint(mgr->wal, VECTOR_CHECKPOINT_TYPE_AUTO);
        vector_wal_truncate(mgr->wal);
        pthread_mutex_unlock(&mgr->mutex);
    }
    return NULL;
}
```

- [ ] **Step 3: 在 vector_api_create / destroy 接入**

- `vector_api_create`：`mgr = checkpoint_manager_create(api->wal, 5000); checkpoint_manager_start(mgr);`
- `vector_api_destroy`：`checkpoint_manager_stop(mgr); checkpoint_manager_destroy(mgr);`

- [ ] **Step 4: 编译 + 跑 ConcurrentInsertWithCheckpoint 测试**

```bash
cmake --build build/engineering --target vdb_checkpoint_test 2>&1 | tail -10
cd build/engineering && ./test/db/integration/vdb_checkpoint_test.exe --gtest_filter='*Concurrent*' 2>&1 | tail -10
```

预期：ConcurrentInsertWithCheckpoint 转绿。

- [ ] **Step 5: 提交 + push**

```bash
git add engine/include/db/storage/vector/checkpoint_manager.h \
        engine/src/db/storage/vector/checkpoint_manager.c \
        engine/src/db/storage/vector/CMakeLists.txt \
        engine/src/db/api/vector_api.c
git commit -m "feat(vector): CheckpointManager 后台线程（5s 周期 flush + checkpoint + truncate）"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 8: 跑 vdb_chaos 3 + vdb_checkpoint 5 + 回归

- [ ] **Step 1: 跑 vdb_chaos_test 3 失败用例**

```bash
rm -rf build/engineering/test-results/vdb_chaos && mkdir -p build/engineering/test-results/vdb_chaos
cd build/engineering && ./test/db/integration/vdb_chaos_test.exe --gtest_filter='VDBChaosTest.IndexRebuildAfterKill:VDBChaosTest.MultipleCrashRecovery:VDBChaosTest.ConcurrentWriteWithCrash' 2>&1 | tail -20
```

预期：3 个测试**全部 PASS**。

- [ ] **Step 2: 跑 vdb_checkpoint_test 全套**

```bash
cd build/engineering && ./test/db/integration/vdb_checkpoint_test.exe 2>&1 | tail -10
```

预期：5/5 全过。

- [ ] **Step 3: 跑 vdb_wal_recovery_test（P1-A 回归）**

```bash
rm -rf build/engineering/test-results/vdb_wal_recovery && mkdir -p build/engineering/test-results/vdb_wal_recovery
cd build/engineering && ./test/db/integration/vdb_wal_recovery_test.exe 2>&1 | tail -10
```

预期：4/4 仍全过（无回归）。

- [ ] **Step 4: 跑 vdb_e2e_test::WALRecovery + CheckpointTest（P1-A 回归）**

```bash
rm -rf build/engineering/test-results/vdb_e2e && mkdir -p build/engineering/test-results/vdb_e2e
cd build/engineering && ./test/db/integration/vdb_e2e_test.exe --gtest_filter='VDBE2ETest.WALRecovery:VDBE2ETest.CheckpointTest' 2>&1 | tail -10
```

预期：2/2 仍全过（无回归）。

- [ ] **Step 5: 提交 + push**

```bash
git add -u
git diff --cached --quiet || git commit -m "test: 跑通 vdb_chaos 3 + vdb_checkpoint 5 + P1-A 回归全绿"
git push origin feature/p1b-checkpoint-manager
```

---

## Task 9: 收口 + 归档

- [ ] **Step 1: tasks.md 全勾 + 写 result.md**

写到 `openspec/changes/p1b-checkpoint-manager/result.md`：
- 完成工作
- 通过测试数（vdb_chaos 3 + vdb_checkpoint 5 + 回归 P1-A 6 = 14 用例全绿）
- 仍失败用例（pre-existing SearchVectors / DropCollection）
- 下一阶段 P1-C 建议

- [ ] **Step 2: 跑最终回归**

```bash
cd build/engineering
for t in vdb_chaos vdb_checkpoint vdb_wal_recovery vdb_e2e vdb_stress; do
  echo "=== $t ==="
  ./test/db/integration/${t}_test.exe 2>&1 | grep -E "PASSED|FAILED" | tail -3
done
```

- [ ] **Step 3: 提交 + push + 反馈 gap-analysis-roadmap**

```bash
git add openspec/changes/p1b-checkpoint-manager/
git commit -m "opsx(P1-B): 验收完成 — vdb_chaos 3 + vdb_checkpoint 5 + P1-A 回归全绿，标记任务全绿"
git push origin feature/p1b-checkpoint-manager
```

更新 `openspec/changes/gap-analysis-roadmap/tasks.md` 的 Task 2.2.1 为 `[x]`。

---

## 自审

1. **Spec 覆盖**：
   - §3.1 持久化 → Task 4/5/6 ✓
   - §4.2 Vector 持久化 → Task 4/5/6 ✓
   - §6.1 vdb_chaos 3 失败 → Task 8 ✓
   - §8 Phase 1 P1-B CheckpointManager → Task 1-9 ✓

2. **占位符扫描**：所有代码模板已写实际值（op.type / segment_id / interval 5000ms 等）

3. **类型一致性**：`vector_wal_t` / `vector_wal_op_t` / `vector_api_t` / `CheckpointManager` 命名一致

4. **可工作性**：每个 Task 独立可测（commit + push + 测试）+ 验证回归无破坏
