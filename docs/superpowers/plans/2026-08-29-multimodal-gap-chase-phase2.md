# 多模态数据库追赶计划 - Phase 2 实现方案

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 8 个已注册模态按代码质量排序追赶，每个模态达到 7+ 分，API + 性能双达标 90%+

**Architecture:** 按代码质量从好到差排序，逐模态追赶。每模态修复：并发安全 → 崩溃恢复 → 核心算法 → 测试覆盖

**Tech Stack:** C 语言、CMake、cmocka 测试框架

## Global Constraints

- 全程简体中文；commit message 用中文
- 禁止修改无关文件
- 测试驱动开发（TDD）
- 环境：Windows 11 + Git Bash；仓库根 `D:\code\book`

---

## Phase 2: 8 个模态追赶顺序

| 顺序 | 模态 | 当前分 | 目标分 | 核心工作 |
|------|------|--------|--------|---------|
| 1 | Tree/Yang (T21) | 4.2 | 7+ | XML parser 升级、NETCONF 完善、持久化 |
| 2 | KV (T22) | 4.2 | 7+ | 并发完善、CAS/Watch/Multi、fsync |
| 3 | Spatial (T23) | 4.2 | 7+ | R-Tree 加锁、ST_* 扩展 |
| 4 | Timeseries (T24) | 4.2 | 7+ | 热路径压缩、连续聚合完善 |
| 5 | Document (T25) | 4.3 | 7+ | 聚合管道完善、同义词增强 |
| 6 | Vector (T26) | 4.3 | 7+ | UAF 修复、WAL fsync、索引完善 |
| 7 | Graph (T27) | 4.5 | 7+ | CSR 加锁、算法库扩充、Cypher 测试 |
| 8 | Relational (T28) | 3.8 | 7+ | TID + WAL + MVCC 集成、优化器完善 |

---

### Task 21: Tree/Yang 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/yang/yang_model.c`
- Modify: `engineering/src/db/storage/yang/yang_engine.c`
- Modify: `engineering/src/db/storage/yang/netconf_server.c`
- Create: `engineering/test/db/storage/yang/yang_test.c`

**关键改进：**
1. XML parser 升级（支持属性/命名空间）
2. NETCONF 1.1 支持
3. 持久化增强
4. 并发安全（rwlock）

---

### Task 22: KV 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/kv/kv.c`
- Create: `engineering/test/db/storage/kv/kv_test.c`

**关键改进：**
1. CAS 命令实现
2. Watch 命令实现
3. Multi 命令实现
4. WAL fsync 验证

---

### Task 23: Spatial 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/spatial/rtree.c`
- Modify: `engineering/src/db/storage/spatial/spatial_engine.c`
- Create: `engineering/test/db/storage/spatial/spatial_test.c`

**关键改进：**
1. R-Tree 加锁完善
2. ST_* 函数实现（PostGIS 兼容）
3. 时空索引支持

---

### Task 24: Timeseries 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/ts/ts_engine.c`
- Create: `engineering/test/db/storage/ts/ts_test.c`

**关键改进：**
1. 热路径增量压缩
2. 连续聚合完善
3. 降采样支持

---

### Task 25: Document 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/doc/doc_engine.c`
- Modify: `engineering/src/db/storage/doc/doc_pipeline.c`
- Create: `engineering/test/db/storage/doc/doc_test.c`

**关键改进：**
1. 聚合管道完善
2. 同义词增强
3. JSONPath 完整支持

---

### Task 26: Vector 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_engine.c`
- Modify: `engineering/src/db/index/vector_index/faiss_hnsw.c`
- Create: `engineering/test/db/storage/vector/vector_test.c`

**关键改进：**
1. faiss_hnsw UAF 修复（RCU/快照）
2. WAL fsync 验证
3. 21+ 索引完善

---

### Task 27: Graph 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/graph/graph_engine.c`
- Modify: `engineering/src/db/storage/graph/graph_algorithms.c`
- Modify: `engineering/src/db/storage/graph/csr.c`
- Create: `engineering/test/db/storage/graph/graph_test.c`

**关键改进：**
1. CSR 加锁（读写锁 + 快照）
2. 图算法扩充（betweenness/closeness/Louvain）
3. PageRank 悬挂节点处理
4. Cypher 测试覆盖

---

### Task 28: Relational 模态追赶

**Files:**
- Modify: `engineering/src/db/storage/rel/rel_engine.c`
- Modify: `engineering/src/db/storage/rel/heapam.c`
- Modify: `engineering/src/db/storage/rel/ltree.c`
- Create: `engineering/test/db/storage/rel/rel_test.c`

**关键改进：**
1. MVCC 集成到执行路径
2. B+Tree 页式化
3. 优化器 join 顺序动态规划
4. TID 管道完善

---

## 执行策略

1. 每个 Task 派发一个子代理
2. 子代理完成后提交 git
3. 每完成 2-3 个 Task 做一次编译验证
4. 遇到编译错误立即修复

## 验收标准

- [ ] 8 个模态总分 ≥ 7
- [ ] 核心 API 覆盖率 ≥ 90%
- [ ] 召回率/QPS 达到标杆 90%
- [ ] 无 P0/P1 正确性风险
- [ ] 测试覆盖 ≥ 80%
