# C0-2 共享 WAL 统一覆盖 设计文档

## 设计目标

消除 WAL 覆盖严重不对称的问题：
- 仅 KV 接入共享 WAL（kv.c:443-447）
- Vector 有独立 vector_wal.c，但 SYNC 模式只 fflush 无 fsync（vector_wal.c:282-289）
- Relational/Timeseries/Spatial/Tree 四模态写路径零 redo log
- WAL 失败被静默吞（vector_engine.c:377-379 LOG_WARN 继续；kv.c:445,459 忽略 wal_write_* 返回值）

## 设计方案

### 1. 刷盘策略

新增 `wal_flush_policy_t` 四级（参考 PostgreSQL `synchronous_commit`）：
- `WAL_FLUSH_NONE`：仅应用层缓存，不强制
- `WAL_FLUSH_OS`：依赖 OS 刷盘
- `WAL_FLUSH_FSYNC`：强制 fsync / FlushFileBuffers（默认）
- `WAL_FLUSH_BATCH`：累积 N 条或 T 毫秒后 fsync

通过 GUC `wal_sync_mode` 可调（默认 FSYNC）。

新增 `db_fsync(int fd)` 跨平台封装：POSIX→`fsync`，Windows→`FlushFileBuffers`。

### 2. 共享 WAL 记录类型扩展

在 `storage/wal/wal.c` 现有 `wal_log_type_t` 枚举新增：
```c
WAL_LOG_HEAP_INSERT       = 20,
WAL_LOG_HEAP_DELETE       = 21,
WAL_LOG_HEAP_UPDATE       = 22,
WAL_LOG_TS_APPEND         = 23,
WAL_LOG_SPATIAL_UPSERT    = 24,
WAL_LOG_YANG_DATASTORE_WR = 25,
```
（具体枚举值由 T2 实施时确认无冲突）

每种记录配套 `wal_<type>_apply(ctx, record)` 回调。

### 3. 模态接入

**Relational（heapam.c）**：insert/delete/update 之前 `db_wal_log(HEAP_*, tuple_meta)`，失败返回 DBERR_WAL_FAILED。`heap_insert` 内部加入 WAL-first 调用。

**Timeseries**：ts_engine_insert 之前 WAL 记录（segment_id + ts + value）。

**Spatial**：spatial_engine_insert/update 之前 WAL 记录（geom_id + bbox）。

**Tree**：yang datastore 写之前 WAL（依赖 C2-5 datastore 落地，本任务作为 T6 标记后置）。

### 4. 失败中止语义

WAL-first 铁律：任何 WAL 写失败必须中止当次 DML 并返回错误，不允许"先写主存再补 WAL"。本变更完成后：
- vector_wal_append 失败 → vector_engine_insert 返回 -1
- wal_write_* 失败 → KV put 返回 -1
- heap_insert 失败 → ModifyTable 错误传播（与 C0-3 错误码扩展对齐）

### 5. Vector 独立 WAL 修复（T8）

vector_wal.c 已知缺陷：
- VLA 栈溢出（:261 `record[HEADER + dims*sizeof(float)]`）
- 异步模式 memcpy 越界（:292-298）

本变更同步修复：将 dims 限制（dataspace 验证）或固定堆缓冲分配。考虑最小改动，本期对 dims > 64K 拒绝（绝大多数场景 dims ≤ 4096）。

### 6. 统一恢复入口

新增 `db_startup_recover(data_dir)`：
- 按模态注册 apply 回调
- 启动时统一重放 WAL
- KV 的 `kv_wal_apply` 迁入此框架（KV 模块仍可保留自有函数，由注册器调用）

### 7. 不变项

- `wal_log_type_t` 已有枚举值不变（向后兼容）
- 各模态对外 API 不变（仅错误码变严格）
- 不动 vector_wal.c 的对外函数签名（vector_engine.c 仍调 vector_wal_*）

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 刷盘策略引入性能回退 | 默认 FSYNC 不变；BENCH 验证 |
| 多模态 WAL 接入扩散面广 | T3-T6 每个模态单独提交，单独回归 |
| vector_wal.c 修复风险 | 修 VLA + memcpy 边界两处局部问题，行为对齐原 SYNC |
| T6 Yang 依赖 C2-5 | 标记 T6 后置，单独任务跟踪 |
