# Task 2 探查报告 — vector_wal_t 状态机 + idempotent insert 路径

> 探查者：P1-B Task 2 子代理
> 范围：定位后续 Task 3-7 修复 `vdb_chaos_test` 3 个失败用例所需的全部事实
> 根因：`engineering/src/db/api/vector_api.c` 完全未集成 `vector_wal_*` API

---

## 1. vector_wal_t 结构体定义

**位置**：`engineering/include/db/storage/vector/vector_persist.h:564-576`

```c
typedef struct vector_wal_s {
    char    path[256];           /**< WAL 文件路径 */
    FILE   *fp;                  /**< WAL 文件指针 */
    uint8_t *buffer;             /**< 写缓冲区 */
    size_t  buffer_size;         /**< 缓冲区大小 (=VECTOR_WAL_BUFFER_SIZE=64KB) */
    size_t  buffer_used;         /**< 缓冲区已用大小 */
    uint64_t num_records;        /**< 记录计数（append+delete 总数） */
    uint64_t num_vectors;        /**< 向量计数（仅 append 累加） */
    uint64_t last_checkpoint;    /**< 上次检查点位置（文件偏移，初始 = sizeof(header)） */
    vector_wal_mode_t mode;      /**< WAL 模式 (SYNC / ASYNC) */
    bool    is_open;             /**< 是否打开 */
    pthread_mutex_t mutex;       /**< 互斥锁（所有写操作均加锁） */
} vector_wal_t;
```

关键字段：

- **`last_checkpoint`**（u64）— 既是运行期检查点偏移量，也是持久化字段（在 `vector_wal_file_header_t.last_checkpoint` 中）。`vector_wal_create` 初始化为 `sizeof(header)` = `VECTOR_WAL_HEADER_SIZE` = **128**；`vector_wal_open` 从 header 恢复。
- **`num_records`**（u64）— 仅在内存，`vector_wal_close` 时回写 header。
- **`header_size`** — 不在结构体内，定义为 `VECTOR_WAL_HEADER_SIZE = 128`（vector_persist.h:511）。
- **`buffer_size` / `buffer_used`** — 异步模式缓存；同步模式 `vector_wal_append` 直接 fwrite+ffsync。
- **`mode`** — `VECTOR_WAL_SYNC`（默认，直接刷盘）/ `VECTOR_WAL_ASYNC`（buffer 缓冲）。

---

## 2. 5 个核心 API 行为摘要

> 5 个核心 API：`vector_wal_append` / `vector_wal_flush` / `vector_wal_checkpoint` / `vector_wal_replay` / `vector_wal_truncate`
> 位置：`engineering/src/db/storage/vector/vector_wal.c`

### 2.1 `vector_wal_append`（vector_wal.c:267-352）

```c
int vector_wal_append(vector_wal_t *wal, uint32_t segment_id,
                      int32_t vector_id, int32_t dims, const float *vector);
```

**行为**：

1. 加锁 → 参数校验（`dims ∈ (0, 65536]`，C0-2 T8 修复）
2. 构造堆缓冲 `record[HEADER+data+crc]`，op=`VECTOR_WAL_APPEND`(=1)
3. `mode == SYNC`：直接 `fwrite + fflush + fsync`（C0-2 T8 走 `wal_flush_get_policy()`）
4. `mode == ASYNC`：写入 `wal->buffer`，满时先刷盘
5. 成功时 `num_records++` 且 `num_vectors++`

**关键**：`vector_id` 由调用方传入——意味着 **vector_api.c 必须在 insert 前分配/查询 ID**。

### 2.2 `vector_wal_flush`（vector_wal.c:407-435）

```c
int vector_wal_flush(vector_wal_t *wal);
```

**行为**：仅在 ASYNC 模式有意义——把 `wal->buffer[0..buffer_used]` 写盘+fsync，`buffer_used=0`。

### 2.3 `vector_wal_checkpoint`（vector_wal.c:444-491）

```c
int vector_wal_checkpoint(vector_wal_t *wal, vector_checkpoint_type_t type);
```

**行为**：

1. 加锁 → flush buffer → fflush → `ftell` 取当前位置
2. `wal->last_checkpoint = pos`
3. 回写 header：`last_checkpoint / num_records / num_vectors`（fseek 至 0）
4. fseek 回 SEEK_END → 解锁

**关键**：`last_checkpoint` 等于文件大小，replay 从此偏移继续。

### 2.4 `vector_wal_replay`（vector_wal.c:583-669）

```c
int64_t vector_wal_replay(const char *wal_path, uint32_t segment_id,
                          vector_segment_t *segment,
                          bool (*callback)(int64_t, int64_t, void*), void *user_data);
```

**行为（重点 — 起始位置 = `header.last_checkpoint`）**：

```c
/* 跳到检查点位置 */
if (header.last_checkpoint > VECTOR_WAL_HEADER_SIZE) {
    fseek(fp, header.last_checkpoint, SEEK_SET);
}
```

1. 打开文件 → 读 header → 验证魔数
2. **fseek 到 `header.last_checkpoint`**（这是核心契约）
3. 循环读 record：解析 `op / seg_id / vec_id / dims / data_size` → 验证 crc32
4. `op == APPEND`：`vector_segment_append(segment, vec)`；`op == DELETE`：`vector_segment_delete(segment, vec_id)`
5. 返回恢复记录数

**关键**：replay 跳过 checkpoint 之前的所有 record。所以 checkpoint 之后到 crash 之间的 record 必须能被 replay 完整重建——这正是 idempotent insert 的需求。

### 2.5 `vector_wal_truncate`（vector_wal.c:496-550）

```c
int vector_wal_truncate(vector_wal_t *wal);
```

**行为**：

1. 加锁
2. 若 `last_checkpoint > HEADER_SIZE`：复制 `[0, last_checkpoint)` 到 `*.tmp` → rename 替换
3. 重置 `num_records=0 / num_vectors=0 / buffer_used=0`
4. **注意**：truncate 后 `last_checkpoint` 不重置（仍指向旧偏移）；下次 checkpoint 会更新

---

## 3. idempotent insert 实现位置 — **需新增**

**结论**：`vector_api.c` 中 **不存在 idempotent / upsert / dedup 逻辑**，vector_api_insert 完全无 WAL 调用，也无 ID 查重。

### 当前 vector_api_insert（vector_api.c:400-446）

```c
for (int32_t i = 0; i < params->n; i++) {
    if (collection_expand(coll) != 0) break;
    int64_t id = params->ids ? params->ids[i] : coll->next_id++;
    ...
    coll->ids[coll->size] = id;
    memcpy(coll->vectors + coll->size * coll->dimension, params->vectors[i], ...);
    ...
    coll->size++;
    inserted++;
}
```

- 调用方提供 `params->ids` → 数组直接用
- 未提供 → 累加 `next_id`
- **无任何 "id 是否已存在" 检查**，也无 WAL 写入

### vdb_chaos_test 期望（test/db/integration/vdb_chaos_test.cpp:169-219 WriteKillRecovery）

测试期望：**插入 → 不 save → 销毁 → 重建 → load → 数据完整**。

当前现状：`vector_api_save/load` 把所有数据 fwrite 到 `collections.meta` 单文件（vector_api.c:606-665）。**如果未调用 save 就销毁**，load 时只能恢复"最后一次 save"的数据；崩溃前新插入的 50 条会丢失。

### P1-B 修复路径

后续 Task 3-7 需在 `vector_api.c` 增加：

1. **idempotent insert 检查**（核心修复点）：
   - 在 `vector_api_insert` 拿 `coll->lock` 后，遍历 `coll->ids[0..size)`，若 `params->ids[i]` 已存在 → **跳过/覆盖**（覆盖写是 idempotent 的标准语义）
   - 配合：当前 `coll->size++` 在重复 ID 时会污染数组；必须先扫描去重

2. **WAL 集成点**：
   - `vector_api_insert` 末尾（`coll->size++` 之后）调用 `vector_wal_append(wal, seg_id, vec_id, dims, vec)`
   - `vector_api_delete` 末尾调用 `vector_wal_delete(wal, seg_id, vec_id)`
   - `vector_api_save` 末尾调用 `vector_wal_checkpoint(wal, MANUAL) + vector_wal_truncate(wal)`
   - `vector_api_load` 开头（创建 segment 后）调用 `vector_wal_replay(wal_path, seg_id, segment, NULL, NULL)` 把 checkpoint 后未持久化的记录应用回内存
   - `vector_api_create` 内增加 `vector_wal_open(data_dir, 0)`

3. **VectorAPI_s 结构体扩展**（vector_api.c:53-59）：
   - 增加 `vector_wal_t *wal` 字段
   - 现有字段无 WAL 句柄

---

## 4. 接入 vector_api 的具体修改点（行号）

| 位置 | 函数 | 行号 | 修改 |
|------|------|------|------|
| `engineering/src/db/api/vector_api.c:53-59` | `struct VectorAPI_s` | 53-59 | 增加 `vector_wal_t *wal` 字段 |
| `engineering/src/db/api/vector_api.c:237-260` | `vector_api_create` | 237 | 末尾 `vector_wal_open(api->data_dir, 0)` |
| `engineering/src/db/api/vector_api.c:261-273` | `vector_api_destroy` | 261 | 开头 `vector_wal_close(api->wal)` |
| `engineering/src/db/api/vector_api.c:400-446` | `vector_api_insert` | 400 | (a) 加 ID 查重扫描 (b) 末尾调 `vector_wal_append` |
| `engineering/src/db/api/vector_api.c:540-582` | `vector_api_delete` | 540 | 末尾调 `vector_wal_delete` |
| `engineering/src/db/api/vector_api.c:606-665` | `vector_api_save` | 606 | 末尾 `vector_wal_checkpoint(MANUAL)` |
| `engineering/src/db/api/vector_api.c:667-744` | `vector_api_load` | 667 | 开头 `vector_wal_replay(...)` 补 checkpoint 后数据 |

---

## 5. 失败测试用例 & 期望行为

| 测试 | 失败原因 | 修复目标 |
|------|---------|---------|
| `WriteKillRecovery` (line 169) | 崩溃前插入未 save，load 后只恢复 save 时数据 | load 时 replay WAL 补全 |
| `IndexRebuildAfterKill` (line 238) | 同上，1000 条分 10 批插入只有 save 时持久化 | 同上 + save 触发 checkpoint |
| `MultipleCrashRecovery` (line 357) | 多次崩溃-重启-再插入，最后 load 只看到最后一次 save | 配合 idempotent insert 避免 ID 冲突 |
| `ConcurrentWriteWithCrash` (line 411) | 并发写入 + 崩溃 + 恢复可能插入重复 ID | idempotent insert + WAL 顺序回放 |
| `SaveLoadCorrectness` (line 474) | save/load 路径未带 WAL | save 后 checkpoint / load 前 replay |
| `DiskFullSimulation` (line 301) | 边界场景，预计 save 触发 checkpoint 后正常 | save 路径补 checkpoint 即可 |

---

## 6. 后续 Task 3-7 实施清单（建议）

- **Task 3**：扩展 `struct VectorAPI_s` + `vector_api_create/destroy` 接入 `vector_wal_open/close`
- **Task 4**：`vector_api_insert` 加 ID 查重（idempotent），尾部追加 `vector_wal_append`
- **Task 5**：`vector_api_delete` 尾部追加 `vector_wal_delete`
- **Task 6**：`vector_api_save` 末尾加 `vector_wal_checkpoint` + `vector_wal_truncate`；`vector_api_load` 开头 `vector_wal_replay`
- **Task 7**：跑 `vdb_chaos_test` 3 个失败用例转绿

---

## 7. 关键代码契约（备忘）

1. **WAL 起始 replay 偏移** = `header.last_checkpoint`（vector_wal.c:613-614）
2. **Checkpoint 偏移量** = checkpoint 调用时刻的 `ftell(wal->fp)`（vector_wal.c:462-468）
3. **`last_checkpoint` 持久化字段** = `vector_wal_file_header_t.last_checkpoint`（vector_persist.h:560-572）
4. **同步模式**（默认）：append 后立即 fsync；**异步模式**：攒 buffer 到 64KB
5. **record 格式**：`[op:1][seg_id:4][vec_id:4][dims:4][data_size:4][data:?][crc32:4]`，header_size=17
6. **WAL 路径**：`{data_dir}/vector_wal_{segment_id}.log`
7. **VectorAPI 当前无任何 vector_wal_* 引用** — `grep -n "vector_wal_" vector_api.c` 返回 0 行