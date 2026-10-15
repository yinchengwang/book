# Task 2 探查报告：vector_api_save / vector_api_load 现状

> 探查日期：2026-10-16
> 探查者：C/C++ 工程实施 subagent
> 探查范围：`vector_api_save` / `vector_api_load` 与 `vector_wal_t` 的集成现状
> 目的：为 P1-A 后续 Task（Task 3+）定位修复切入点

---

## 1. 文件实际位置（订正 plan 中过期路径）

| 文件 | 实际路径 | plan 写的路径 | 备注 |
|------|----------|----------------|------|
| 实现 | `engineering/src/db/api/vector_api.c` | `engineering/src/db/storage/vector/vector_api.c` | plan 路径已过期 |
| 头文件 | `engineering/include/db/api/vector_api.h` | — | plan 未提及 |
| WAL 实现 | `engineering/src/db/storage/vector/vector_wal.c` | — | 唯一正确 |
| WAL 头文件 | `engineering/include/db/storage/vector/vector_persist.h` | `engineering/include/db/vector_wal.h` | 头文件实际叫 `vector_persist.h`，**没有 `vector_wal.h`** |

---

## 2. `vector_api_save` 现状

**位置**：`engineering/src/db/api/vector_api.c:606-665`

**当前逻辑摘要**：

1. 校验 `api` 非空，否则返回 `VECTOR_API_ERR_INVALID_PARAM`。
2. 用 `_mkdir(data_dir)`（Windows）或 `mkdir(data_dir, 0755)`（POSIX）保证目录存在。
3. 打开 `{data_dir}/collections.meta`（`"wb"` 模式，覆盖写）。
4. 写 `api->n_collections`（`int32_t`）。
5. 逐个集合序列化元数据 + 向量数据 + id 数组 + metadata：
   - 集合名（`sizeof(coll->name)`，定长字段）
   - `dimension`、`size`、`index_type`、`metric_type`、`created_at`、`updated_at`、`next_id`
   - `vectors_size = size * dimension * sizeof(float)`，再写 `size` 个 float
   - `ids[]`（`int64_t[size]`）
   - 每条 metadata 的 `metadata_sizes[j]` + 二进制 payload
6. 关闭文件，记 `LOG_INFO`，返回 `VECTOR_API_OK`。

**关键观察**：
- **完全没用 `vector_wal_*` 系列 API**——所有写操作都在写出自定义二进制格式的 `collections.meta`。
- 没有调用 `vector_wal_append` / `vector_wal_flush` / `vector_wal_checkpoint`。
- 因此：从 `vector_api_insert` 到 `vector_api_save` 之间没有累积任何 WAL 记录；崩溃后没有任何恢复机制。

---

## 3. `vector_api_load` 现状

**位置**：`engineering/src/db/api/vector_api.c:667-745`

**当前逻辑摘要**：

1. 打开 `{data_dir}/collections.meta`（`"rb"` 模式）。
2. **文件不存在不视为错误**——直接 `LOG_INFO` 后返回 `VECTOR_API_OK`（冷启动场景）。
3. 读 `n_collections`，逐个 `calloc` 出 `VectorCollectionImpl`，按 `save` 的逆序读回各字段。
4. 调用 `vector_query_plan_create()` 为每个集合创建查询计划。
5. 累积到 `api->collections[api->n_collections++]`，关文件，返回 `VECTOR_API_OK`。

**关键观察**：
- 不调用 `vector_wal_replay` / `vector_index_recover`，因此崩溃后 "save 之后、load 之前" 的插入会全部丢失。
- 容量分配：`capacity = vectors_size / (dim * sizeof(float))`；空集合默认分配 1024。

---

## 4. `vector_wal_t` 可用 API

API 表面来自 `engineering/include/db/storage/vector/vector_persist.h`（**注意：`vector_wal.h` 不存在，所有 WAL 接口都在 `vector_persist.h`**）。

| 函数 | 签名 | 行号（`.c`） | 用途 |
|------|------|-------------|------|
| 创建 | `vector_wal_t *vector_wal_create(const char *path, uint32_t segment_id)` | 61 | 新建 WAL 文件 |
| 打开 | `vector_wal_t *vector_wal_open(const char *path, uint32_t segment_id)` | 127 | 打开已有 WAL（不存在则自动创建） |
| 关闭 | `int vector_wal_close(vector_wal_t *wal)` | 203 | 关闭 & 释放 |
| 删除 | `int vector_wal_drop(const char *path, uint32_t segment_id)` | 249 | 删除 WAL 文件 |
| 追加向量 | `int vector_wal_append(vector_wal_t *wal, uint32_t segment_id, int32_t vector_id, int32_t dims, const float *vector)` | 267 | 写追加日志 |
| 删除向量 | `int vector_wal_delete(vector_wal_t *wal, uint32_t segment_id, int32_t vector_id)` | 357 | 写删除日志 |
| 刷盘 | `int vector_wal_flush(vector_wal_t *wal)` | 407 | buffer 落盘 |
| 检查点 | `int vector_wal_checkpoint(vector_wal_t *wal, vector_checkpoint_type_t type)` | 444 | 持久化 header & 推进检查点 |
| 截断 | `int vector_wal_truncate(vector_wal_t *wal)` | 496 | 截断到 checkpoint |
| 记录数 | `uint64_t vector_wal_num_records(const vector_wal_t *wal)` | 559 | 查询 |
| 文件大小 | `uint64_t vector_wal_file_size(const vector_wal_t *wal)` | 566 | 查询 |
| **重放** | `int64_t vector_wal_replay(const char *wal_path, uint32_t segment_id, vector_segment_t *segment, bool (*cb)(int64_t, int64_t, void *), void *)` | 583 | 关键！回放 |
| 完整恢复 | `vector_recovery_result_t *vector_index_recover(const char *data_dir, uint32_t segment_id)` | 739 | 完整崩溃恢复流程 |
| 检查点信息 | `int vector_wal_get_checkpoint_info(...)` | — | 查询 |
| 序列化/反序列化 | `vector_wal_serialize`/`vector_wal_deserialize` | 913/944 | 兼容旧 API |

**`vector_wal_t` 结构体关键字段**（`vector_persist.h:564-576`）：
- `path[256]`、`FILE *fp`、`buffer`、`buffer_used`
- `num_records`、`num_vectors`、`last_checkpoint`
- `mode`（`VECTOR_WAL_SYNC` / `VECTOR_WAL_ASYNC` / `VECTOR_WAL_NONE`）
- `is_open`、`pthread_mutex_t mutex`

**常量**（`vector_persist.h:505-520`）：
- `VECTOR_WAL_MAGIC = 0x5657414C`
- `VECTOR_WAL_HEADER_SIZE = 128`
- `VECTOR_WAL_BUFFER_SIZE = 64 * 1024`
- `VECTOR_WAL_RECORD_HEADER_SIZE = 17`

---

## 5. CMake build 结果

### 5.1 `vector_api` 静态库

```
cmake --build build/engineering --target vector_api
→ [0/2] Re-checking globbed directories...
→ ninja: no work to do.
```

**结论**：库已编译，`vector_api.c` 自身可过编。

### 5.2 `vdb_e2e_test` 可执行

```
cmake --build build/engineering --target vdb_e2e_test
→ [58/63] Building C object .../vector_api.c.obj
→ [61/63] Building CXX object .../vdb_e2e_test.cpp.obj
→ [62/63] Linking CXX executable engineering\test\db\integration\vdb_e2e_test.exe
→ FAILED: cannot find -lcjson: No such file or directory
```

**编译产物成功**（含 `vdb_e2e_test.cpp.obj`），**仅链接失败**：
- `collect2.exe: error: ld returned 1 exit status`
- 触发于 `cannot find -lcjson`
- 链路：`vdb_e2e_test → libvector_api.a → db_core`（链路在 `engineering/src/db/api/CMakeLists.txt:24` 显式 link `cjson`）

**根因排查**：
- `engineering/CMakeLists.txt:114-115` 用 `add_subdirectory(../third_part/cjson ${CMAKE_BINARY_DIR}/third_part/cjson_build)` 构建 cjson。
- `build/engineering/third_part/cjson_build/` 目录**不存在**——cjson 子项目根本没被纳入当前 build 配置（可能因为 cmake reconfigure 时该子目录被跳过、或 `ENGINEERING_BUILD=ON` 时未触发 add_subdirectory）。
- 与 P1-A / `vector_api_save` 逻辑**无关**，是预先存在的 build 环境问题。

**结论**：**编译验证通过，链接验证因环境缺 cjson 失败——非 Task 2 关心的代码层面 bug**。

> 建议：Task 3+ 在做 ctest 时可临时改用 `ninja -C build/engineering vector_api`（库级测试），或预先 `cmake --build build/engineering --target cjson` 解决依赖；或先在工程维度补全 cjson 子项目构建（不在 P1-A 范围内）。

---

## 6. 失败根本原因分析

### 6.1 WALRecovery 测试期望 vs 现实

**测试位置**：`engineering/test/db/integration/vdb_e2e_test.cpp:487-525`

**测试步骤**：
1. 插入 100 个向量 → `vector_api_save(api)`（刷盘）。
2. 继续插入 50 个 → 跳过 save，直接 `vector_api_destroy(api)`（模拟崩溃，`pre_crash_size = 150`）。
3. 重新 `vector_api_create(test_data_dir)` + `vector_api_load(api)`。
4. **断言 `EXPECT_GE(recovered_size, 100)`**。

**当前实际行为**（按代码静态推理）：
- `vector_api_save` 写 `collections.meta`，100 条持久化。
- 后 50 条只在内存，**没有 WAL 写入**（`vector_api_insert` 不调 `vector_wal_append`）。
- `vector_api_destroy` 释放内存，后 50 条丢失。
- `vector_api_load` 只读 `collections.meta`，恢复 100 条。
- `EXPECT_GE(100, 100) == pass` → **测试是"侥幸通过"，不是真正 WAL 恢复**。

### 6.2 根因汇总

| 编号 | 位置 | 当前状态 | 影响 |
|------|------|----------|------|
| **R1** | `engineering/src/db/api/vector_api.c:7-15` 头文件 | 已 `#include <db/storage/vector/vector_persist.h>`（具备调用 WAL 能力） | — |
| **R2** | `vector_api_save` (line 606) | **完全没有调用 WAL API**——只写 `collections.meta` | 崩溃后丢失未 save 的改动 |
| **R3** | `vector_api_load` (line 667) | **没有 `vector_wal_replay` / `vector_index_recover`**——只读 `collections.meta` | 无法从 WAL 恢复 |
| **R4** | `vector_api_insert` 路径（需进一步探查） | 没有追到 `vector_wal_append` 调用——需在 Task 3+ 用探查脚本确认 | 如果 R4 成立，R2/R3 是同根问题 |
| **R5** | `vector_cli.c:476, 490` 等 CLI 层 | 仅包装 `vector_api_save/load`，未直接驱动 WAL | CLI 层也无 WAL 入口 |
| **R6** | `vector_api.h` | 公共头文件未暴露任何 WAL 类型与函数 | 上层无 API 用 |

### 6.3 结论

**`vector_api` 是 WAL 旁路实现**：当前 `vector_api_save` / `vector_api_load` 用私有 `collections.meta` 做快照式持久化，**与 `vector_wal_t` 完全解耦**。`vector_wal_*` 是 "VectorSegment" 级别的细粒度机制（每个段一个 WAL 文件），`vector_api` 是 "Collection" 级别的高层抽象——**两层之间没有集成层**。

P1-A 的目标（plan 推断）是：让 `vector_api` 借助 WAL 实现真正的无数据丢失恢复（崩溃后 `EXPECT_EQ(recovered_size, pre_crash_size)` 而非 `>= 100`）。这需要在 `vector_api` 内部新增一个"per-collection WAL 写入 + 启动 replay"层。

---

## 7. 修复策略建议（供 Task 3-7 参考）

> 本节是 Task 3 开始的探路建议，不在 Task 2 范围内实施。

### 策略 A（在 `vector_api` 内部集成 WAL，推荐）

1. **新增 per-collection WAL**：`{data_dir}/{coll_name}/wal.log` 或 `{data_dir}/wal/{coll_name}_{id}.log`。
2. **在 `vector_api_insert` / `vector_api_delete` 等修改路径**插入 `vector_wal_append` / `vector_wal_delete` 调用（按 `collection_id + vector_id` 维度）。
3. **改造 `vector_api_save`**：
   - 写入 `collections.meta` 后调 `vector_wal_checkpoint(...)`。
   - 视情况 `vector_wal_truncate(...)`。
4. **改造 `vector_api_load`**：
   - 读 `collections.meta` 基础数据。
   - 对每个 collection，扫描其 WAL 目录，调用 `vector_wal_replay` 把未提交改动重放进内存。

### 策略 B（双轨制，保守）

保留 `collections.meta` 作为"完整快照"，新增 `wal.log` 作为"增量日志"：
- `save` = snapshot + WAL checkpoint + truncate。
- `load` = 读 snapshot → replay WAL → 内存合并。
- 优点：snapshot 仍然自包含，可独立 inspect；缺点：写放大。

### 兼容性

- 老 `collections.meta` 文件无法被新代码辨认（没有 WAL 头部）；需要在 load 时检测 WAL 存在与否决定走哪条路径。
- 已有的 `vdb_chaos_test.cpp` 等大量测试依赖 `vector_api_save/load` 行为，**任何改动都必须先运行现有测试**，确认基线不掉。

---

## 8. 关键文件清单（绝对路径）

实现 / 头文件：
- `C:\code\book\engineering\src\db\api\vector_api.c`
- `C:\code\book\engineering\include\db\api\vector_api.h`
- `C:\code\book\engineering\src\db\storage\vector\vector_wal.c`
- `C:\code\book\engineering\include\db\storage\vector\vector_persist.h`

测试：
- `C:\code\book\engineering\test\db\integration\vdb_e2e_test.cpp`（WALRecovery @ 487）
- `C:\code\book\engineering\test\db\integration\vdb_chaos_test.cpp`
- `C:\code\book\engineering\test\db\api\vector_api_test.cpp`
- `C:\code\book\engineering\test\db\storage\vector_wal_test.cpp`

调用点：
- `C:\code\book\engineering\src\db\api\vdb_api.c:152, 180, 476, 481`
- `C:\code\book\engineering\src\db\cli\vector_cli.c:476, 490`
