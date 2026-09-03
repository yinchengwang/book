# WAL 刷盘与覆盖规范（新增）

## 目的

为自研多模态数据库提供统一的 WAL 刷盘策略与跨模态覆盖范围，消除"仅 KV 接入"、"失败静默吞"、"无 fsync"等系统性缺陷。

## 要求

### REQ-1：统一刷盘策略

共享 WAL（`storage/wal/`）必须支持四级刷盘策略：

| 策略 | 含义 |
|------|------|
| `WAL_FLUSH_NONE` | 仅应用缓存，不强制落盘 |
| `WAL_FLUSH_OS` | 依赖 OS 刷盘 |
| `WAL_FLUSH_FSYNC` | 强制 `fsync` / `FlushFileBuffers`（**默认**） |
| `WAL_FLUSH_BATCH` | 累积 N 条或 T 毫秒后 fsync |

通过 GUC `wal_sync_mode` 可调，默认 FSYNC。

### REQ-2：跨平台 fsync 封装

`db_fsync(int fd)` 跨平台封装：
- POSIX：`fsync(fd)`
- Windows：`FlushFileBuffers(fd)`

### REQ-3：失败中止语义

WAL-first 铁律：**任何 WAL 写失败必须中止当次 DML** 并返回 `DBERR_WAL_FAILED`，不允许"先写主存后补 WAL"的语义。

### REQ-4：跨模态覆盖

以下模态的写路径必须产生 redo 记录：
- KV（已覆盖，本变更补强失败中止）
- Vector（已有独立 vector_wal，本变更修复 fsync 与 VLA 缺陷）
- Relational / Timeseries / Spatial（**本变更接入**）
- Yang（依赖 C2-5 datastore 落地）

### REQ-5：统一恢复入口

`db_startup_recover(const char *data_dir)` 启动时统一重放：
- 各模态注册 apply 回调
- KV 的 `kv_wal_apply` 迁入此框架
- 失败时记录但继续（不阻塞启动）

## 实现文件

- `engineering/include/db/wal_flush.h`（新增）
- `engineering/src/db/storage/wal/wal_flush.c`（新增）
- `engineering/src/db/storage/wal/wal.c`（记录类型扩展）
- `engineering/src/db/storage/wal/wal_recover.c`（新增）
- 各模态 WAL 接入（heapam.c / ts_engine.c / spatial_engine.c / vector_wal.c）
