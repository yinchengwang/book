# 并发控制规范（新增）

## 目的

为自研多模态数据库的所有存储模态提供统一的、经过验证的跨平台读写锁原语，消除各模态自行复刻的有缺陷的自旋读写锁实现。

## 要求

### REQ-1：统一锁原语

所有存储模态（Vector / Timeseries / Document / Graph / Spatial / KV）的内部并发保护**必须**使用 `mmdb_rwlock_t`（定义在 `engineering/include/db/mmdb_lock.h`），**禁止**自行实现读写锁或复用其他模态的私有实现。

### REQ-2：默认并发安全

任何使用 `mmdb_rwlock_t` 的数据结构，**默认** 必须启用锁保护（即 `use_lock = true`）。只有在显式调用 `mm_disable_lock()` 后才能关闭锁（仅限 benchmark/单线程调试场景）。

### REQ-3：跨平台实现

`mmdb_rwlock_t` 底层实现：
- **Windows**：`SRWLOCK`（Slim Reader/Writer Lock，Win API 原语）
- **POSIX**：`pthread_rwlock_t`（POSIX 1003.1 标准原语）

接口必须正确处理这两种后端的差异（如 SRWLOCK 不支持 `pthread_rwlockattr_t`）。

### REQ-4：现有正确实现迁移

将已存在的、经过验证的 `engineering/include/sdk/impl/mmdb_lock.h` 提升为 `engineering/include/db/mmdb_lock.h`，原 SDK 路径保留为兼容 shim。

## 实现文件

- `engineering/include/db/mmdb_lock.h` — canonical 头
- `engineering/include/sdk/impl/mmdb_lock.h` — 兼容 shim（`#include "db/mmdb_lock.h"`）
- `engineering/src/db/concurrency/mmdb_lock.c` — 跨平台实现
- `engineering/src/sdk/core/mmdb_lock.c` — 删除（原位置）
