# C0-1 统一并发原语推广 Proposal

## Why

差距分析（`docs/multimodal-gap-analysis-2026/README.md` §3.1）发现 5 个模态（Vector/Timeseries/Document）各自复刻同一份有竞态窗口与写者饥饿缺陷的自旋读写锁（`vector_engine.c:1557`、`ts_engine.c:752`、`doc_engine.c:757`），Graph 与 Spatial 甚至零锁；且 `use_lock=false` 是默认值。而仓库已有正确的跨平台锁 wrapper `include/sdk/impl/mmdb_lock.h`（P5 X2 落地，SRWLOCK/pthread_rwlock）只在 SDK 层使用——正确组件已存在但未推广。

## What Changes

- 将 `mmdb_lock.h` 从 `include/sdk/impl/` 提升到 `include/db/`（SDK 层 include 路径保持兼容）
- 删除三个模态的 `simple_rwlock_t`/`ts_rwlock_t`/`doc_rwlock_t` 复刻实现，统一替换为 `mmdb_rwlock_t`
- Graph（graph_csr/graph_engine）与 Spatial（rtree/spatial_engine）接入 `mmdb_rwlock_t`（从零到有）
- 锁默认值反转：`use_lock` 默认 `true`，新增 `mm_disable_lock()` 显式关闭（仅 benchmark 场景）
- Graph CSR 双视图基础：查询走旧 CSR、写入走 COO、compact 后原子切换（为 C2-3 铺路）

## Capabilities

| 能力 | 交付 |
|------|------|
| 统一锁原语 | `grep -r simple_rwlock\|ts_rwlock\|doc_rwlock engineering/src/db` 零命中 |
| 默认并发安全 | 5 模态默认加锁，关闭需显式 API |
| 并发正确性 | 每模态一个并发回归测试（双线程读写混合，无死锁无崩溃） |

## Impact

- 修改文件：vector_engine.c / ts_engine.c / doc_engine.c / graph_csr.c / graph_engine.c / rtree.c / spatial_engine.c
- 新增文件：`include/db/mmdb_lock.h`（迁移）、每模态并发测试
- 预计 5-7 个 commit
