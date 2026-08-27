# C3-5 各模态功能面补齐 任务清单

> 本变更规模较大，建议执行时按模态再拆分为 C3-5a (Relational+KV+Tree)、C3-5b (Graph+Timeseries+Document)、C3-5c (Spatial+宽表)。此处按主题分组列出。

## 任务列表（Relational）

- [ ] **T1** B+Tree 页式化（页面复用 Buffer Pool + 叶子链）
- [ ] **T2** Hash 索引（等值）+ BRIN 索引（时序大表）
- [ ] **T3** TOAST 大元组外存（对接 C3-1 Blob）
- [ ] **T4** FSM 空闲空间映射
- [ ] **T5** Extended Query 协议（pgwire）
- [ ] **T6** Window Function 增强（更多 frame types）

## 任务列表（Graph）

- [ ] **T7** Cypher 变长路径 `*1..n` 实现
- [ ] **T8** Cypher OPTIONAL MATCH + UNWIND
- [ ] **T9** 属性 B-Tree 二级索引（复用 index/btree）
- [ ] **T10** MODEL_RDF 注册到 DataModel 枚举
- [ ] **T11** MODEL_SPATIOTEMPORAL 注册到 DataModel 枚举

## 任务列表（Timeseries）

- [ ] **T12** 标签倒排索引（复用 doc_inverted）
- [ ] **T13** 基数上限保护（阈值 GUC + 降级全扫）
- [ ] **T14** 窗口级增量聚合刷新

## 任务列表（Document）

- [ ] **T15** B-Tree 二级索引（复用 index/btree）
- [ ] **T16** TTL 索引（复用 kv_ttl）
- [ ] **T17** 聚合管道 +$lookup/$unwind/$facet/$bucket

## 任务列表（Spatial）

- [ ] **T18** R*Tree 分裂策略（重插入降低重叠）
- [ ] **T19** +MultiPoint/MultiLineString/MultiPolygon/GeometryCollection

## 任务列表（KV）

- [x] **T20** kv_cas（compare-and-swap）
- [ ] **T21** kv_txn（begin/put/commit 乐观事务）
- [x] **T22** kv_comparator_t 注入点
- [ ] **T23** 跨 CF 原子写（WriteBatch）

## 任务列表（Tree）

- [ ] **T24** PostgreSQL ltree 兼容层（路径列 + GiST 索引 + 操作符）

## 任务列表（宽表第一期）

- [ ] **T25** wide_row_get/put/范围扫描（row_key + column + ts 版本）
- [ ] **T26** 聚簇索引（row 内列有序）+ 二级索引（复用 B-Tree）

## 收尾

- [ ] **T27** 各模态功能集成测试
- [x] **T28** Verify + Archive（T20+T22 实装，其他推迟）
