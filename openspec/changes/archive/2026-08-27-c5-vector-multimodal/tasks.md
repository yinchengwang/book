# C5 Vector 与多模态核心生产化 任务清单

## 任务列表

### faiss_hnsw COW 段（C5.1-C5.3）
- [ ] **C5.1** faiss_hnsw_segment 结构定义 + faiss_hnsw_segment_create/destroy/free
- [ ] **C5.2** immutable_buffer 累积逻辑（add → buffer → 超阈值触发 compact）
- [ ] **C5.3** faiss_hnsw 改为 segments[] 数组（写锁包裹 add，rdlock 包裹 search 多段归并）
- [ ] **C5.4** mmdb_rwlock 接入 + 跨段 top-k 合并

### 多模态检索完整路径（C5.5-C5.7）
- [ ] **C5.5** mm_multimodal_search_v2：per-named-vector 串行 + RRF 融合
- [ ] **C5.6** RRF 接入 faiss_hnsw search 返回（mm_rrf_score 复用）
- [ ] **C5.7** cross_modal_search 接入 vtable + blob + metadata filter 完整

### 测试与文档（C5.8-C5.10）
- [ ] **C5.8** cross_modal_e2e_test 升级到真实路径（multi-vector search）
- [ ] **C5.9** multi_modal DESIGN.md 更新 + Commit 全览 + Whole-batch review
- [ ] **C5.10** Verify + Archive
