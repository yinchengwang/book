# C0-2 共享 WAL 统一覆盖 Proposal

## Why

差距分析（README §3.2 + 各深挖卷 2.2）发现：仅 KV 接入共享 WAL；Vector 有独立 WAL 但 SYNC 模式无 fsync（`vector_wal.c:282-289`）且记录用 VLA 栈分配（`:261`）与异步 memcpy 越界（`:292-298`）；Relational/Timeseries/Spatial/Tree 四模态写路径零 redo log；KV 忽略 `wal_write_*` 返回值（`kv.c:445,459`）；Vector WAL 失败仅 LOG_WARN 继续写主存（`vector_engine.c:377-379`）。

## What Changes

- 共享 WAL（storage/wal/）扩展记录类型：heap insert/delete/update、ts append、spatial upsert、yang datastore 写
- 统一刷盘策略 `wal_flush_policy_t`（NONE/OS/BUFFER/FSYNC 四级），默认 FSYNC，GUC `wal_sync_mode` 可调
- WAL-first 硬约束：WAL 写失败 → DML 返回 DBERR_WAL_FAILED 并中止（页面写入移到 WAL 成功后）
- KV `wal_write_*` 返回值检查 + 失败中止
- vector_wal.c 修复：VLA 改堆缓冲、异步 memcpy 边界检查、SYNC 模式接统一 fsync 策略
- 统一启动恢复 `db_startup_recover()`：按模态注册 apply 回调，启动时统一重放（KV 的 kv_wal_apply 迁入）

## Capabilities

| 能力 | 交付 |
|------|------|
| WAL 覆盖 | 5 模态写路径全部产生 redo 记录 |
| 刷盘语义 | FSYNC 默认；`wal_sync_mode` GUC 分级可调 |
| 失败语义 | WAL 失败中止 DML，无静默继续 |
| 崩溃恢复 | kill -9 后重启，五模态数据完整（集成测试） |

## Impact

- 修改：storage/wal/、storage/kv/kv.c、storage/vector/vector_wal.c、storage/vector/vector_engine.c、heapam.c、ts_engine.c、spatial_engine.c、yang_engine.c
- 新增：db_startup_recover.c、崩溃恢复集成测试
- 预计 8-10 个 commit
