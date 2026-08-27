# C1-2 faiss_hnsw 并发安全与度量修正 任务清单

## 任务列表

- [ ] **T1** 复现测试：并发插入+搜索 UAF 压力（Debug 双分配器，先 FAIL）
- [ ] **T2** IP 度量对拍测试（暴力扫描 vs 索引，先 FAIL）
- [ ] **T3** COW 批量段实现（immutable buffer + 原子指针切换）
- [ ] **T4** IP 度量修正（独立比较器或显式拒绝）
- [ ] **T5** WAL/存储 ID 一致性修正（vector_engine.c:376 vs :384）
- [ ] **T6** 测试转 PASS + Recall 回归（100K ≥0.99 不回退）
- [ ] **T7** Verify + Archive
