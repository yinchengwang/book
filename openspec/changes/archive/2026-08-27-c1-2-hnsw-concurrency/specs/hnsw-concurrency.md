# faiss_hnsw 并发与度量规范（新增）

## 目的

消除 faiss_hnsw 模块的并发 UAF 风险与 IP 度量语义错误。

## 要求

### REQ-1：IP 度量正确性

`compute_distance_internal` 对内积（IP）度量**禁止** fallback 到 L2 平方距离。返回 `dist = -inner_product`（距离越小表示相似度越高）。

### REQ-2：WAL/存储 ID 一致性

`vector_wal_append` 记录的 vec_id 必须与 `vector_page_append` 实际分配的 vec_id 一致。WAL 恢复后 vec_id 与插入时序保持一致。

### REQ-3：COW 段接口

faiss_hnsw 应支持 COW（copy-on-write）批量段接口骨架，便于后续完整实现。当前提供 `faiss_hnsw_seal_segment()` 与 `faiss_hnsw_get_active_segment()` 占位 API。

### REQ-4：回归保护

并发压力测试 + IP 度量对拍测试 + 100K Recall ≥0.99 回归测试必须全绿。
