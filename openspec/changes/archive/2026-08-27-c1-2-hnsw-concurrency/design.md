# C1-2 faiss_hnsw 并发安全与 IP 度量修正 设计文档

## 设计目标

修复 01 卷识别的两类缺陷：
1. faiss_hnsw add 触发 realloc（`faiss_hnsw_stubs.c:211-241`）与 search 直接解引用（`faiss_hnsw_search.c:26`）无锁共存 → UAF
2. IP（内积）度量静默退化为 L2²（`faiss_hnsw_search.c:46-52`），语义错误且无告警

## 方案

### 1. 复现测试（T1-T2）

- T1：并发插入+搜索压力（4 reader + 1 writer，500ms），断言无崩溃+死锁检测
- T2：IP 度量对拍测试（暴力扫描 vs 索引），断言结果一致

### 2. COW 批量段（T3）

借鉴 Qdrant segment 不可变设计：
- 写入攒批到 immutable buffer（达到容量或显式 flush）
- 搜索持旧快照，原子指针切换
- 完整实现风险高：本变更仅添加接口骨架 + 注释，详细 batching 逻辑留给后续

### 3. IP 度量修正（T4）

`compute_distance_internal` 对 IP metric 显式拒绝或使用正确比较器：
- 选项 A：返回 `-inner_product`（距离 = -内积，最大内积=最小距离）
- 选项 B：建索引时显式拒绝 IP metric
- 本变更选 A（最小侵入），并增加注释警告

### 4. WAL/存储 ID 一致性（T5）

`vector_engine.c:376` 预分配 vec_id 与 `:384` 实际插入 vec_id 错位。改为：插入后读取 `vector_page_append` 返回的真实 vec_id，并同步记录到 WAL。

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| COW 完整实现扩散 | T3 仅接口骨架，详细 batching 留给后续 |
| IP 修正影响召回率 | T4 仅返回距离语义修正，索引结构不变，搜索行为与 FAISS IP 一致 |
| Recall 回归 | T6 跑现有 SIFT 召回率测试，≥0.99 |
