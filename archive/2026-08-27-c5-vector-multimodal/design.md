# C5 Vector 与多模态核心生产化 设计文档

## 设计目标

完成批次 A 的核心实装：faiss_hnsw COW 段 + 多模态检索完整路径。

## 方案

### 1. faiss_hnsw COW 段（C5.1-C5.3）

**当前问题**：add 路径直接 realloc `idx->vectors/levels/offsets/nbs`，与搜索线程无锁共存会 UAF。

**实装**：
- 引入 immutable_buffer（容量阈值，默 1024 向量）
- add：累积到 buffer，超阈值后 compact → 新 immutable_segment
- segment 结构：`{vectors, levels, offsets, nbs, start_id, end_id}`
- idx 增加 `active_segment *` + `segments[]` 数组（mmdb_rwlock_wrlock 包裹 add；rdlock 包裹 search）
- 搜索：遍历所有 segments 合并 top-k（保证新数据可查）
- compact：从 buffer 构造新 segment，原 buffer 释放

### 2. 多模态检索完整路径（C5.4-C5.5）

`mm_multimodal_search_v2(query_obj, top_k, out)`：
- 提取 query_obj 中每个 named_vector
- 对每个向量空间查 faiss_hnsw（per-vector 并行 — 简单实现用串行）
- 收集各空间 top-k 结果 → RRF 融合 → 整体 top-k

### 3. cross_modal_search 接入索引族（C5.6）

通过 C4-2 新增的 `faiss_hnsw_vtable_t`，从 `faiss_hnsw_default_vtable()` 获取 search 函数指针，传入 mm_multimodal_search_v2。

### 4. blob + metadata filter 完整（C5.7）

mm_multimodal_search_v2 接受 `filter_json` 参数，对每个候选对象调用 `mm_multimodal_metadata_match`。

## 不变项

- faiss_hnsw_t 公开 API（add/search/...）签名不变
- mm_multimodal_object_t 公开字段不变
- vector_index_selector.c 索引选择流程不变

## 风险

| 风险 | 缓解 |
|------|------|
| COW 段引入额外遍历开销 | 段数通常 1-2，遍历开销可忽略 |
| mmdb_rwlock 在 COW 段路径下频繁 | add 写锁；search 短临界区读 segments 指针数组 |
