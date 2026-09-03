# C0-1 统一并发原语推广 设计文档

## 设计目标

消除 5 个模态各自复刻的 buggy 自旋读写锁，将已存在的 `mmdb_lock.h` 从 SDK 层提升到 `db/` 公共层，让所有存储模态共享同一份正确的跨平台锁实现。

## 现有正确组件

`engineering/include/sdk/impl/mmdb_lock.h`：
- Windows：`SRWLOCK`（Slim Reader/Writer Lock，内核态原语，正确）
- POSIX：`pthread_rwlock_t`（POSIX 标准原语，正确）
- 提供 5 个 API：`mmdb_rwlock_init/rdlock/wrlock/unlock/destroy`

这是**已验证正确**的跨平台锁原语，但仅被 SDK 层使用。

## Buggy 复刻（要替换的）

5 个模态各自复刻的 `simple_rwlock_t`/`ts_rwlock_t`/`doc_rwlock_t`，共同缺陷：
- 用 `__sync_fetch_and_add`/`__sync_bool_compare_and_swap` 手工构造
- 读者/写者非原子竞态窗口（详见 `vector_engine.c:1565-1598`）
- 写者饥饿（read_lock 不看 `writers_waiting`）
- `timeout_ms` 参数被忽略
- `use_lock=false` 默认（调用方可选，但默认不安全）

## 设计方案

### 1. 头文件提升

| 路径 | 角色 |
|------|------|
| `engineering/include/db/mmdb_lock.h` | **canonical 位置**（db 公共层） |
| `engineering/include/sdk/impl/mmdb_lock.h` | shim，1 行 `#include "db/mmdb_lock.h"`（SDK 兼容） |

### 2. 实现文件移动

| 原位置 | 新位置 |
|--------|--------|
| `engineering/src/sdk/core/mmdb_lock.c` | `engineering/src/db/concurrency/mmdb_lock.c` |

新文件更新 `#include` 路径指向 `db/mmdb_lock.h`。

### 3. CMake 注册

`engineering/src/db/concurrency/CMakeLists.txt` 新增（如果不存在则创建）：
```cmake
add_project_library(mmdb_lock
    mmdb_lock.c
)
```
原 `sdk/core/mmdb_lock.c` 从 SDK 编译中移除（不再有副本）。

### 4. 模态侧替换

每个模态（vector/ts/doc/graph/spatial）：
- 删除本地 buggy rwlock 结构定义与函数实现
- `db->rwlock` 字段类型改为 `mmdb_rwlock_t`
- `use_lock` 默认值反转（`false` → `true`）
- `enable_lock()` 调用 `mmdb_rwlock_init`
- 读路径 `mmdb_rwlock_rdlock`，写路径 `mmdb_rwlock_wrlock`，解锁 `mmdb_rwlock_unlock(lock, is_wrlock)`
- 新增 `mm_disable_lock()` 公开 API（仅 benchmark 场景使用）

### 5. 不变项

- mmdb_rwlock_t 的内部 typedef 不暴露 ABI 变化（Windows SRWLOCK 与 POSIX pthread_rwlock_t 都是不透明类型）
- 函数签名不变
- `use_lock` 字段名不变
- 仅行为变化：默认开启锁

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 锁默认开启后并发性能回归 | benchmark 套件验证；性能退化超 5% 退回单独评估 |
| Graph/Spatial 加锁后遍历性能差 | 后续 C2-3 引入 COW/RCU；本变更仅替换错误实现 |
| 现有 SDK 调用方路径中断 | shim 兼容保留 |
