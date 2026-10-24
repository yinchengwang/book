# C1-2 faiss_hnsw 并发安全与度量修正 Proposal

## Why

差距分析 01 卷发现：`faiss_hnsw_index_add` 在容量不足时 realloc `idx->vectors/levels/offsets/neighbors`（`faiss_hnsw_stubs.c:211-241`），而搜索路径直接解引用（`faiss_hnsw_search.c:26`、`faiss_hnsw_search_filtered.c` 无锁）——并发插入触发扩容 + 并发搜索 = 释放后使用。另有 IP（内积）度量静默退化为 L2²（`faiss_hnsw_search.c:46-52`），语义错误且无告警。

## What Changes

- **COW 批量段**：add 攒批到 immutable buffer（阈值或显式 flush），搜索持旧快照原子切换（借鉴 Qdrant segment 设计）
- IP 度量修正：独立比较器（距离 = -inner_product）或建索引时显式拒绝并返回 DBERR_INVALID_METRIC
- WAL ID 与存储 ID 一致性：插入路径统一 ID 分配（`vector_engine.c:376` 与 `:384` 错位修正）
- 复现测试先行：并发插入 + 搜索 UAF 压力测试（TSAN 或 Debug 双分配器）

## Capabilities

| 能力 | 交付 |
|------|------|
| 并发安全 | 并发插入搜索压力测试无 UAF 无崩溃 |
| IP 度量 | 内积查询结果语义正确（对拍暴力扫描） |
| ID 一致性 | WAL 恢复后向量 ID 与插入时一致 |

## Impact

- 修改：faiss_hnsw_stubs.c、faiss_hnsw_search.c、faiss_hnsw_internal.h、vector_engine.c
- 新增：并发压力测试、IP 度量对拍测试
- 预计 4-5 个 commit
- 依赖：C0-1
