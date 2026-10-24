# Vector 与多模态核心生产化规范（新增）

## 目的

完成批次 A：faiss_hnsw COW 段 + 多模态检索完整路径。

## 要求

### REQ-1：faiss_hnsw COW 段

- `faiss_hnsw_segment_t` 不可变 segment（含 vectors/levels/offsets/nbs + start_id/end_id）
- `faiss_hnsw_t` 持有 `active_segment *` + `segments[]` 数组
- add 路径：mmdb_rwlock_wrlock 包裹 → 累积到 immutable_buffer → 超阈值触发 compact
- search 路径：mmdb_rwlock_rdlock 包裹 → 遍历所有 segments 合并 top-k

### REQ-2：多模态检索

`mm_multimodal_search_v2(query, top_k, filter_json, out_results)`：
- per-named-vector 串行搜索（每空间 top-k 候选）
- RRF 融合（mm_rrf_score）
- 对候选对象应用 metadata filter
- 返回整体 top-k

### REQ-3：cross_modal_search

通过 vtable 接入 faiss_hnsw 索引族，文本 query → 指定 modal 空间 top-k。

## 实现文件

- `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_segment.c`（新增）
- `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_v2.c`（faiss_hnsw 重写为段化）
- `engineering/src/db/storage/multimodal/multimodal_search_v2.c`（新增）
- `engineering/test/db/storage/cross_modal_e2e_test.cpp`（升级）
