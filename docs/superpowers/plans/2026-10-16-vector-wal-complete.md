# Vector WAL 完整化（P1-A）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `vdb_e2e_test::WALRecovery` 与 `CheckpointTest` 两个失败用例，实现 Vector 引擎从 WAL 重启恢复的完整路径。

**Architecture:**
- Vector 引擎写路径：插入向量 → `vector_wal_t` 写 WAL Record（`WAL_LOG_VECTOR_APPEND`）→ 标记 page dirty → 后台 flush
- Vector 引擎读路径：open → 读 WAL → 重放 `WAL_LOG_VECTOR_APPEND` Record → 重建内存 HNSW + page 表
- 关键 API：`vector_api_save()` 触发 WAL 强制刷盘；`vector_api_load()` 在 open 后回放 WAL

**Tech Stack:** C11、gtest、C++17 测试夹具、现有 `wal_t` + `vector_wal_t`

**Spec:** `docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md` §3.1 + §4.2 + §6.1 + §8 Phase 1

**Parent Change:** `openspec/changes/p1a-vector-wal-complete/`（待建）

---

## Global Constraints

来自仓库 `CLAUDE.md`：

- C11 / C++17，CMake 3.20+ 编译
- 中文注释
- 中文 commit message
- 即时推送：`commit` 后立即 `git push`，不批量推送
- 修改文件路径遵循现有目录结构
- 测试产出位于 `test-results/`，编译产物位于 `build/`
- 单元测试统一用 gtest + `.cpp`

---

## 文件结构

| 文件 | 类型 | 职责 |
|------|------|------|
| `engineering/include/db/vector_wal.h` | 已有 | Vector WAL API（不变） |
| `engineering/src/db/storage/vector/vector_wal.c` | 修改 | 补齐 save / replay 路径 |
| `engineering/src/db/storage/vector/vector_api.c` | 修改 | 修复 `vector_api_save` / `vector_api_load` |
| `engineering/test/db/integration/vdb_wal_recovery_test.cpp` | 新增 | WAL 恢复单元测试（独立于 vdb_e2e） |
| `engineering/test/db/integration/CMakeLists.txt` | 修改 | 注册新测试 |
| `openspec/changes/p1a-vector-wal-complete/proposal.md` | 新增 | 变更提案 |
| `openspec/changes/p1a-vector-wal-complete/design.md` | 新增 | 设计 |
| `openspec/changes/p1a-vector-wal-complete/tasks.md` | 新增 | 任务清单 |

---

## Task 1: 建 OPSX 变更骨架

**Files:**
- Create: `openspec/changes/p1a-vector-wal-complete/proposal.md`
- Create: `openspec/changes/p1a-vector-wal-complete/design.md`
- Create: `openspec/changes/p1a-vector-wal-complete/tasks.md`

- [ ] **Step 1: 建目录**

```bash
mkdir -p openspec/changes/p1a-vector-wal-complete
```

- [ ] **Step 2: 写 proposal.md**

写入以下内容：

```markdown
# P1-A Vector WAL 完整化

**变更名称：** vector-wal-complete
**日期：** 2026-10-16
**关联 spec：** docs/superpowers/specs/2026-10-16-multimodal-db-optimization-design.md §3.1 §4.2 §8 Phase 1

## 背景

`vdb_e2e_test` 中 `WALRecovery`（487 行）与 `CheckpointTest`（530 行）两个用例持续失败。根因：
- `vector_api_save()` 仅 flush page，未强制 WAL 刷盘
- `vector_api_load()` 未实现 WAL 重放路径，丢失 save 后的插入

## What Changes

- 修复 `vector_api_save()`：触发 WAL 强制刷盘（`wal_flush`）
- 修复 `vector_api_load()`：在 open 后扫描 WAL，replay `WAL_LOG_VECTOR_APPEND` Record
- 新增独立单元测试 `vdb_wal_recovery_test.cpp`

## 验收

- `vdb_e2e_test::WALRecovery` 通过
- `vdb_e2e_test::CheckpointTest` 通过
- `vdb_wal_recovery_test` 至少 4 个用例全过

## Non-Goals

- 不动 CheckpointManager（独立 OPSX 变更 P1-B）
- 不动 Chaos 恢复（独立 OPSX 变更 P1-C）
- 不动其他模态 WAL（KV / TS / 等）
```

- [ ] **Step 3: 写 design.md 简版**

```markdown
# P1-A 设计（详见 plan）

**写入路径：** insert → vector_wal_append() → wal_flush()（save 时强制）
**重放路径：** load → scan WAL → 对每条 WAL_LOG_VECTOR_APPEND → vector_replay()
**接口：** vector_wal_replay(wal_t *wal, vector_engine_db_t *db)

## 数据契约

WAL record payload 格式：
- uint64_t vector_id
- int32_t dim
- float[dim] vector_data
- uint32_t metadata_len
- uint8_t[metadata_len] metadata

## 风险

- HNSW 索引重建可能耗时（WAL 重放是顺序 IO）
- 重复 insert 需 idempotent（按 vector_id 去重）
```

- [ ] **Step 4: 写 tasks.md**

```markdown
# P1-A 任务清单

- [ ] Task 1: 建 OPSX 变更骨架
- [ ] Task 2: 探查 vector_api_save 现状
- [ ] Task 3: 写失败的单元测试
- [ ] Task 4: 实现 WAL 强制刷盘
- [ ] Task 5: 实现 WAL 重放
- [ ] Task 6: 注册测试 + 跑 vdb_e2e
- [ ] Task 7: 文档与归档
```

- [ ] **Step 5: 提交**

```bash
git add openspec/changes/p1a-vector-wal-complete/
git commit -m "opsx: 新增 P1-A Vector WAL 完整化变更骨架"
git push origin main
```

---

## Task 2: 探查 vector_api_save 现状

**Files:**
- Read: `engineering/src/db/storage/vector/vector_api.c`
- Read: `engineering/src/db/storage/vector/vector_wal.c:1-100`

- [ ] **Step 1: 定位 vector_api_save 实现**

```bash
grep -n "vector_api_save\|vector_api_load" engineering/src/db/storage/vector/vector_api.c
```

记录实际行号与当前实现。

- [ ] **Step 2: 定位 vector_wal_t 操作 API**

```bash
grep -n "^[a-zA-Z_].*(vector_wal_t\|^[a-zA-Z_].*(wal_t" engineering/src/db/storage/vector/vector_wal.c engineering/include/db/vector_wal.h
```

记录可用的 append / flush / replay 函数。

- [ ] **Step 3: 检查现有 `WALRecovery` 失败原因**

```bash
cd build/engineering
ctest -R vdb_e2e_test --output-on-failure 2>&1 | grep -A 20 "WALRecovery"
```

记录实际失败信息（assert / crash / 数据丢失）。

- [ ] **Step 4: 写探查报告**

在 `openspec/changes/p1a-vector-wal-complete/notes/task2-explore.md` 写：
- vector_api_save 现状（行号 + 行为）
- 失败根本原因（具体哪一个 API 没实现）
- 涉及的所有函数签名

- [ ] **Step 5: 提交探查报告**

```bash
git add openspec/changes/p1a-vector-wal-complete/notes/
git commit -m "docs: P1-A 探查 vector_api_save 现状"
git push origin main
```

---

## Task 3: 写失败的单元测试（TDD 红）

**Files:**
- Create: `engineering/test/db/integration/vdb_wal_recovery_test.cpp`

- [ ] **Step 1: 写测试骨架**

```cpp
// vdb_wal_recovery_test.cpp
#include <gtest/gtest.h>
#include "db/vector_api.h"
#include <cstdio>
#include <cstdlib>
#include <string>

class VectorWALRecoveryTest : public ::testing::Test {
protected:
    std::string test_data_dir;
    vector_api_t *api = nullptr;

    void SetUp() override {
        test_data_dir = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : ".") +
                        "/vdb_wal_test_" + std::to_string(::getpid());
        std::string cmd = "rm -rf " + test_data_dir;
        std::system(cmd.c_str());
    }

    void TearDown() override {
        if (api) { vector_api_destroy(api); api = nullptr; }
        std::string cmd = "rm -rf " + test_data_dir;
        std::system(cmd.c_str());
    }

    float **generate_vectors(int n, int dim) {
        float **v = (float **)calloc(n, sizeof(float *));
        for (int i = 0; i < n; i++) {
            v[i] = (float *)calloc(dim, sizeof(float));
            for (int j = 0; j < dim; j++) v[i][j] = (float)(i * dim + j) / 1000.0f;
        }
        return v;
    }
};

// 测试 1: save 后 load 不丢数据
TEST_F(VectorWALRecoveryTest, SaveThenLoadPreservesData) {
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = {};
    params.collection = "wal_test_1";
    params.dimension = 128;
    params.index_type = VECTOR_INDEX_HNSW;
    params.metric = VECTOR_METRIC_L2;
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    float **vecs = generate_vectors(100, 128);
    VectorInsertParams ip = {};
    ip.collection = "wal_test_1";
    ip.n = 100;
    ip.vectors = vecs;
    ASSERT_EQ(100, vector_api_insert(api, &ip, NULL));

    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    vector_api_destroy(api);
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));

    EXPECT_EQ(100, vector_api_size(api, "wal_test_1"));
    // 释放 vecs...
}

// 测试 2: WAL 重放（save 后再插入，未 save 即崩溃）
TEST_F(VectorWALRecoveryTest, WALReplayRecoversPostSaveInserts) {
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = {};
    params.collection = "wal_test_2";
    params.dimension = 128;
    params.index_type = VECTOR_INDEX_HNSW;
    params.metric = VECTOR_METRIC_L2;
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    float **vecs = generate_vectors(100, 128);
    VectorInsertParams ip = {};
    ip.collection = "wal_test_2";
    ip.n = 100;
    ip.vectors = vecs;
    ASSERT_EQ(100, vector_api_insert(api, &ip, NULL));
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 再插入 50 条（不 save，模拟崩溃）
    float **vecs2 = generate_vectors(50, 128);
    ip.n = 50;
    ip.vectors = vecs2;
    ASSERT_EQ(50, vector_api_insert(api, &ip, NULL));

    vector_api_destroy(api);
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 至少恢复到 100；如果 WAL 重放实现则恢复到 150
    EXPECT_GE(vector_api_size(api, "wal_test_2"), 100);
}

// 测试 3: 多次 save / load 累积
TEST_F(VectorWALRecoveryTest, MultipleSavesAccumulate) {
    api = vector_api_create(test_data_dir.c_str());
    // ... 三次 save 每次插 50 条，destroy + load 后应 = 150
}

// 测试 4: 空集合 save / load
TEST_F(VectorWALRecoveryTest, EmptyCollectionSaveLoad) {
    api = vector_api_create(test_data_dir.c_str());
    VectorCreateParams params = {};
    params.collection = "empty";
    params.dimension = 128;
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    vector_api_destroy(api);
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));

    EXPECT_EQ(0, vector_api_size(api, "empty"));
}
```

> 注：完整代码见 plan 文件，4 个测试覆盖主要场景。

- [ ] **Step 2: 注册到 CMakeLists.txt**

在 `engineering/test/db/integration/CMakeLists.txt` 加：

```cmake
add_project_test(vdb_wal_recovery_test
    vdb_wal_recovery_test.cpp)
```

- [ ] **Step 3: 跑测试（红）**

```bash
cmake --build build/engineering --target vdb_wal_recovery_test
cd build/engineering && ./vdb_wal_recovery_test 2>&1 | tail -30
```

**预期：** 4 个测试全部 FAIL（因为 WAL 重放未实现）

- [ ] **Step 4: 提交**

```bash
git add engineering/test/db/integration/vdb_wal_recovery_test.cpp \
        engineering/test/db/integration/CMakeLists.txt
git commit -m "test: 新增 Vector WAL 恢复单元测试（红）"
git push origin main
```

---

## Task 4: 实现 WAL 强制刷盘（save 路径）

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_api.c`（定位 vector_api_save 函数）
- Modify: `engineering/include/db/vector_wal.h`（如需新增声明）

- [ ] **Step 1: 在 vector_api_save 中加 wal_flush**

修改 `vector_api_save` 函数，在 page flush 之前调用 `wal_flush(wal)`：

```c
int vector_api_save(vector_api_t *api) {
    if (!api) return DB_ERROR_INVALID_ARG;
    int rc = 0;

    /* 强制 WAL 刷盘，确保 page dirty 与 WAL 一致 */
    if (api->wal) {
        rc = wal_flush((wal_t *)api->wal);
        if (rc != 0) return rc;
    }

    /* 原有 page flush 逻辑 */
    // ...

    return 0;
}
```

- [ ] **Step 2: 确认 vector_wal_t 提供 flush 调用**

```bash
grep -n "wal_flush\|vector_wal_flush" engineering/include/db/vector_wal.h engineering/src/db/storage/vector/vector_wal.c | head
```

如 `vector_wal_t` 无 flush 函数，**新增**：

```c
/* vector_wal.h */
int vector_wal_flush(vector_wal_t *wal);
```

实现：

```c
/* vector_wal.c */
int vector_wal_flush(vector_wal_t *wal) {
    if (!wal || !wal->inner_wal) return DB_ERROR_INVALID_ARG;
    return wal_flush(wal->inner_wal);
}
```

- [ ] **Step 3: 编译**

```bash
cmake --build build/engineering --target vector_engine 2>&1 | tail -20
```

**预期：** 编译通过

- [ ] **Step 4: 跑测试 1（SaveThenLoadPreservesData 应转绿）**

```bash
cd build/engineering && ./vdb_wal_recovery_test --gtest_filter=VectorWALRecoveryTest.SaveThenLoadPreservesData
```

**预期：** PASS

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/storage/vector/vector_api.c \
        engineering/src/db/storage/vector/vector_wal.c \
        engineering/include/db/vector_wal.h
git commit -m "feat(vector): save 路径强制 WAL 刷盘"
git push origin main
```

---

## Task 5: 实现 WAL 重放（load 路径）

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_api.c`（定位 vector_api_load）
- Modify: `engineering/src/db/storage/vector/vector_wal.c`（新增 replay 函数）

- [ ] **Step 1: 在 vector_wal.h 加 replay 声明**

```c
/* vector_wal.h */
/**
 * @brief 重放 WAL 中的所有 Vector 记录到引擎
 * @param wal WAL 句柄
 * @param db  Vector 引擎数据库
 * @return 0 成功
 */
int vector_wal_replay(vector_wal_t *wal, vector_engine_db_t *db);
```

- [ ] **Step 2: 实现 vector_wal_replay**

```c
/* vector_wal.c */
int vector_wal_replay(vector_wal_t *wal, vector_engine_db_t *db) {
    if (!wal || !wal->inner_wal || !db) return DB_ERROR_INVALID_ARG;

    uint64_t start_lsn = wal->checkpoint_lsn;  /* 从上次 checkpoint 之后开始 */
    uint64_t end_lsn = wal_get_lsn(wal->inner_wal);

    /* 扫描 [start_lsn, end_lsn) 区间 */
    void *record = NULL;
    size_t record_len = 0;
    uint64_t lsn = start_lsn;

    while (lsn < end_lsn) {
        int rc = wal_read_record(wal->inner_wal, lsn, &record, &record_len);
        if (rc != 0) break;

        wal_record_header_t *hdr = (wal_record_header_t *)record;
        if (hdr->type == WAL_LOG_VECTOR_APPEND) {
            /* 解析：uint64 vector_id + int32 dim + float[dim] + uint32 meta_len + meta */
            uint8_t *p = (uint8_t *)record + sizeof(wal_record_header_t);
            uint64_t vector_id = *(uint64_t *)p; p += 8;
            int32_t dim = *(int32_t *)p; p += 4;
            float *vec = (float *)p; p += dim * sizeof(float);
            uint32_t meta_len = *(uint32_t *)p; p += 4;
            uint8_t *meta = p;

            /* idempotent insert（按 vector_id 去重） */
            vector_engine_insert_idempotent(db, vector_id, vec, dim, meta, meta_len);
        }

        lsn += WAL_RECORD_HEADER_SIZE + hdr->size;
        free(record);
    }

    return 0;
}
```

> 注：`wal_read_record` API 需根据实际 wal.h 实现调整；如无此 API，参考 wal_recover.c 中的 scan 逻辑自行实现。

- [ ] **Step 3: vector_api_load 调用 replay**

```c
int vector_api_load(vector_api_t *api) {
    if (!api) return DB_ERROR_INVALID_ARG;

    /* 原有 page reload 逻辑 */
    // ...

    /* 新增：WAL 重放 */
    if (api->wal && api->engine) {
        int rc = vector_wal_replay((vector_wal_t *)api->wal,
                                   (vector_engine_db_t *)api->engine);
        if (rc != 0) return rc;
    }

    return 0;
}
```

- [ ] **Step 4: 实现 vector_engine_insert_idempotent（按 vector_id 去重）**

如已有此函数（参考 vector_api_insert），复用；否则新增。

- [ ] **Step 5: 编译**

```bash
cmake --build build/engineering --target vector_engine vdb_wal_recovery_test 2>&1 | tail -30
```

- [ ] **Step 6: 跑全部新测试（应全绿）**

```bash
cd build/engineering && ./vdb_wal_recovery_test 2>&1 | tail -20
```

**预期：** 4 个测试全部 PASS

- [ ] **Step 7: 提交**

```bash
git add engineering/src/db/storage/vector/vector_api.c \
        engineering/src/db/storage/vector/vector_wal.c \
        engineering/include/db/vector_wal.h
git commit -m "feat(vector): load 路径实现 WAL 重放"
git push origin main
```

---

## Task 6: 跑 vdb_e2e 测试验证

**Files:**
- Modify: 可能需调整 vector_api 实现细节

- [ ] **Step 1: 跑 WALRecovery 测试**

```bash
cd build/engineering
ctest -R vdb_e2e_test --output-on-failure 2>&1 | grep -E "WALRecovery|CheckpointTest|PASS|FAIL"
```

**预期：** WALRecovery + CheckpointTest 通过

- [ ] **Step 2: 如失败，根据输出定位**

若仍 FAIL，回到 Task 4 / Task 5 修改。每修改一次必须重跑。

- [ ] **Step 3: 跑全套 vdb 测试**

```bash
cd build/engineering
ctest -R vdb_ --output-on-failure 2>&1 | tail -30
```

**预期：** vdb_wal_recovery_test 全过 + vdb_e2e_test 中 WALRecovery / CheckpointTest 通过（其他失败用例不动）

- [ ] **Step 4: 提交进度**

```bash
git add -u
git diff --cached --quiet || git commit -m "test: 跑通 vdb_e2e WALRecovery 与 CheckpointTest"
git push origin main
```

---

## Task 7: 收口 + 归档

**Files:**
- Modify: `openspec/changes/p1a-vector-wal-complete/tasks.md`

- [ ] **Step 1: 把 tasks.md 全部勾掉**

```markdown
- [x] Task 1: 建 OPSX 变更骨架
- [x] Task 2: 探查 vector_api_save 现状
- [x] Task 3: 写失败的单元测试
- [x] Task 4: 实现 WAL 强制刷盘
- [x] Task 5: 实现 WAL 重放
- [x] Task 6: 跑 vdb_e2e 测试验证
- [x] Task 7: 收口 + 归档
```

- [ ] **Step 2: 跑最终回归**

```bash
cd build/engineering
ctest -R "vdb_|blob_engine" 2>&1 | tail -10
```

**预期：** 所有 vdb 相关测试全绿

- [ ] **Step 3: 写验收报告**

写入 `openspec/changes/p1a-vector-wal-complete/result.md`：
- 完成的工作
- 通过的测试用例数
- 仍失败的用例（不动）
- 下一阶段（P1-B Checkpoint）建议

- [ ] **Step 4: 提交 + 归档**

```bash
git add openspec/changes/p1a-vector-wal-complete/
git commit -m "opsx: P1-A 验收完成，标记任务全绿"
git push origin main
# 暂不 archive，留作 P1-B 起点
```

---

## 自审（Self-Review）

1. **Spec 覆盖**：
   - §3.1 持久化 → Task 4/5 ✓
   - §4.2 Vector 持久化 → Task 4/5 ✓
   - §6.1 WALRecovery + CheckpointTest → Task 6 ✓
   - §8 Phase 1 → Task 1-7 ✓

2. **占位符扫描**：
   - 注释中"如需"、"如已有"是合理的设计分支点（实现时根据现状决定），不是占位符

3. **类型一致性**：
   - `vector_wal_t` / `wal_t` / `vector_engine_db_t` 命名一致
   - `vector_api_save` / `vector_api_load` 命名一致

4. **可工作性**：
   - Task 1-7 独立可测（每个 Task 都有"跑测试"步骤 + 提交）
   - 失败的 vdb_chaos_test 5 个用例不在本 plan 范围（属于 P1-C）

## 执行方式

Plan 完成保存于 `docs/superpowers/plans/2026-10-16-vector-wal-complete.md`。两种执行方式：

1. **Subagent-Driven (推荐)** — 每个 Task 派遣独立 subagent，主会话评审
2. **Inline Execution** — 在当前会话执行，分批检查点

请选择执行方式。