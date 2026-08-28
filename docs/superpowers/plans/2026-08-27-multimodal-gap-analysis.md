# 多模态数据库差距深度分析 执行计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 产出 22 领域（8 深挖模态 + 9 未注册引擎 + 5 缺失模态）的实现质量级差距分析报告。

**Architecture:** 每个深挖模态派一个 general-purpose 子代理直接撰写报告分卷；主线程用 grep 校验证据密度（每维度 ≥3 个 `file:line`）后提交；最后汇总写 README。不改任何代码。

**Tech Stack:** 无代码变更；工具为 Glob/Grep/Read/Agent(general-purpose)/Bash(git)。业界数据来源为 `reference/` 子模块源码、`docs/multimodal-db-comparison-2026.md` 及公开资料（子代理可用 WebSearch）。

**设计文档:** `docs/superpowers/specs/2026-08-27-multimodal-gap-analysis-design.md`

## Global Constraints

- 全程简体中文；commit message 用中文；只提交当前任务相关文件
- **禁止修改 `engineering/`、`learning/` 下任何文件**（纯分析任务）
- 不运行新的性能基准；已有基准数据直接引用并注明出处（如"158K vec/s，见 memory/sdk-insert-speed-opt 与旧对比文档"）
- 报告目录：`docs/multimodal-gap-analysis-2026/`；禁止在仓库根目录新增文件
- 证据规则：每个结论必须有 `file:line` 代码证据或文档/基准出处，禁止"应该是"式判断
- Gap 二分：每个差距必须标注「功能缺失」或「实现质量缺陷」
- 评分口径：0-10 分（10=标杆水平，5=可用但明显落后，≤2=缺失或有正确性风险）
- 结论区分「确认」（代码可证）与「疑似」（需运行验证才能定论）
- 环境：Windows 11 + Git Bash；仓库根 `D:\code\book`

---

## 深挖子代理共用提示词模板（Task 2-9 引用）

每个深挖任务派发的子代理提示词 = 「模态专属段」+ 以下「共用标准段」（必须完整拼入提示词，不得省略）：

```text
【共用标准段】

你是资深数据库内核工程师，对 D:\code\book 仓库中一个自研模态做代码级质量审查。
仓库背景：C/C++ 多模态数据库，源码在 engineering/src/db/ 与 engineering/include/db/，
测试在 engineering/test/db/，参考源码（只读子模块）在 reference/。

六维质量审查标准（每维度至少 3 个带 file:line 的证据）：
1. 并发正确性：多线程读写路径是否有数据竞争；锁粒度是否合理；原子操作/内存屏障是否缺失
2. 崩溃恢复：WAL 是否覆盖所有写路径；checkpoint 后能否正确恢复；torn page 处理
3. 内存安全：泄漏/UAF/越界；错误路径的清理是否完整
4. 错误处理：错误码传播一致性；失败路径是否留下半写状态
5. 算法实现质量：核心算法与业界标杆实现的偏差（具体到算法步骤级别的差异）
6. API 设计：接口一致性、可组合性、与 mm_storage 抽象（engineering/include/db/mm_storage.h）的契合度

输出规则：
- 直接撰写报告文件（路径见模态专属段），使用简体中文
- 每个 Gap 标注「功能缺失」或「实现质量缺陷」之一
- 每个结论必须有 file:line 代码证据或文档/基准出处（引用 docs/multimodal-db-comparison-2026.md 的数据需注明）
- 结论标注「确认」（代码可证）或「疑似」（需运行验证）
- 评分 0-10：10=标杆水平，5=可用但明显落后，≤2=缺失或有正确性风险
- 只读分析：禁止修改 engineering/ 与 learning/ 下任何文件；禁止运行性能基准

报告文件必须使用以下骨架（五个二级标题）：
# <模态名>模态差距深度分析
## 1. 实现现状盘点
   （模块清单、代码规模——用 Glob/Grep 统计文件数与行数量级、测试覆盖——列出 engineering/test/db/ 下相关测试文件及断言数量级）
## 2. 代码级质量审查
   （六个三级小节：2.1 并发正确性 / 2.2 崩溃恢复 / 2.3 内存安全 / 2.4 错误处理 / 2.5 算法实现质量 / 2.6 API 设计；每小节≥3 个 file:line 证据）
## 3. 业界标杆对比
   （对比表：自实现 vs 各标杆，逐维度）
## 4. 差距矩阵
   （0-10 评分表，每行一个维度，附证据链接 file:line 或出处）
## 5. 改进优先级
   （正确性风险的缺陷排最前，其次按投入产出排序；每项标注分类与工作量估计 S/M/L）

你的最终文本回复只需返回：报告文件路径 + 六维证据计数 + 发现的「实现质量缺陷」数量。
```

---

### Task 1: 提交设计文档并创建报告目录骨架

**Files:**
- Create: `docs/multimodal-gap-analysis-2026/README.md`（占位骨架，Task 12 填充）

**Interfaces:**
- Produces: 目录 `docs/multimodal-gap-analysis-2026/` 存在；设计文档已入库（后续任务引用它）

- [ ] **Step 1: 提交设计文档（若尚未提交）**

```bash
cd D:/code/book && git status --short docs/superpowers/specs/
# 若 2026-08-27-multimodal-gap-analysis-design.md 未提交：
git add docs/superpowers/specs/2026-08-27-multimodal-gap-analysis-design.md
git commit -m "docs: 添加多模态数据库差距深度分析设计文档"
```

预期：输出含 `1 file changed`

- [ ] **Step 2: 创建占位 README**

写入 `docs/multimodal-gap-analysis-2026/README.md`：

```markdown
# 多模态数据库差距深度分析（2026-08）

> 实现质量级差距分析。设计文档：`../superpowers/specs/2026-08-27-multimodal-gap-analysis-design.md`
> 本文件为总览，内容在全部分卷完成后填充（见 Task 12）。

## 分卷目录

| 卷 | 领域 | 状态 |
|----|------|------|
| 01 | Vector 深挖 | 待完成 |
| 02 | Relational 深挖 | 待完成 |
| 03 | Graph 深挖 | 待完成 |
| 04 | KV 深挖 | 待完成 |
| 05 | Timeseries 深挖 | 待完成 |
| 06 | Document 深挖 | 待完成 |
| 07 | Spatial 深挖 | 待完成 |
| 08 | Tree 深挖 | 待完成 |
| 09 | 未注册引擎核对（9 个） | 待完成 |
| 10 | 缺失模态调研（5 个） | 待完成 |
```

- [ ] **Step 3: 提交**

```bash
cd D:/code/book && git add docs/multimodal-gap-analysis-2026/README.md
git commit -m "docs: 创建多模态差距分析报告目录骨架"
```

---

### Task 2: Vector 模态深挖（01-vector-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026/01-vector-gap.md`（由子代理写入）

**Interfaces:**
- Consumes: 共用标准段（本计划上文）、Task 1 的目录
- Produces: 分卷文件，供 Task 12 汇总引用

- [ ] **Step 1: 派发深挖子代理**

用 Agent 工具（subagent_type: general-purpose）派发，提示词 = 共用标准段 + 以下模态专属段：

```text
【模态专属段：Vector】

审查对象：
- 源码：engineering/src/db/storage/vector/（含 faiss_hnsw/ 等子模块，先 Glob 列出全部）
- 头文件：engineering/include/db/（vector_engine.h、sparse_vector.h、bm25_index.h、hybrid_retrieval.h 及 include/db/index/ 相关）
- 测试：engineering/test/db/ 下向量相关测试
- 已知索引：21+ 种（HNSW/IVF-Flat/IVF-PQ/NSW/LSH/ScaNN/Annoy/KD-Tree/Ball-Tree/GPU-HNSW/GPU-IVF/GPU-IVF-PQ 等），memory 记录：100K/1M Recall@10=0.99，插入 ~158K vec/s，ef=k*5+50
- 注意区分三套路径：旧 hnsw/faiss_hnsw.h（仍被 24 处引用）、新 faiss_hnsw/ 内存版、hnsw/ 持久化版

业界标杆（对比时逐一对照）：Milvus、Qdrant、FAISS（reference/vector/faiss 可读源码）、pgvector（reference/extension/pgvector 可读源码）。
算法质量审查重点：HNSW 层级分配/搜索终止条件/删除处理 vs FAISS HNSW；IVF 的 nlist 探测策略；
PQ 编码器训练；量化与过滤的组合正确性。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\01-vector-gap.md
```

- [ ] **Step 2: 校验证据密度**

```bash
cd D:/code/book && grep -oE '[a-z_0-9]+\.(c|h|cpp):[0-9]+' docs/multimodal-gap-analysis-2026/01-vector-gap.md | wc -l
```

预期：≥ 54（六维 × ≥3 × 三套路径规模；至少 ≥ 18）

- [ ] **Step 3: 校验骨架完整**

```bash
cd D:/code/book && grep -c '^## ' docs/multimodal-gap-analysis-2026/01-vector-gap.md && grep -c '^### 2\.' docs/multimodal-gap-analysis-2026/01-vector-gap.md
```

预期：`5` 和 `6`（五个二级标题 + 2.x 六个小节）

- [ ] **Step 4: 校验 Gap 二分标注**

```bash
cd D:/code/book && grep -cE '功能缺失|实现质量缺陷' docs/multimodal-gap-analysis-2026/01-vector-gap.md
```

预期：≥ 10

- [ ] **Step 5: 修正并提交**

若 Step 2-4 任一不达标：让子代理（SendMessage 续问或重派）补证据/补小节，重新校验。达标后：

```bash
cd D:/code/book && git add docs/multimodal-gap-analysis-2026/01-vector-gap.md
git commit -m "docs: 完成 Vector 模态差距深度分析分卷"
```

---

### Task 3: Relational 模态深挖（02-relational-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026/02-relational-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：Relational】

审查对象：
- 存储：engineering/src/db/storage/rel/、heapam/btreeam（rel.c/heapam.c/btreeam.c）
- 执行器：engineering/src/db/executor/ 与 sql/（Volcano 迭代器 + 8 物理算子：SeqScan/IndexScan/NestLoop/HashJoin/HashAgg/Sort/Limit/Gather）
- 优化器：engineering/src/db/optimizer/、rewrite/（代价模型 + 规则优化）
- 解析器：engineering/src/db/parser/
- 事务：engineering/src/db/txn/、storage/txn/、include/db/index_mvcc.h、mvcc_hot.h（注意 memory 提示：SQL exec P1-5 已完成 ~11.6K LOC、184+ 测试；旧对比文档称"无 MVCC"可能已过时，需核实 txn/ 与 index_mvcc.h 的实际能力与覆盖范围）
- Buffer Pool/WAL/Catalog 共享层：engineering/src/db/storage/buffer/、wal/、catalog/
- 测试：engineering/test/db/ 下 sql/executor/optimizer 相关

业界标杆（逐一对照）：PostgreSQL（reference/relational/postgres 可读源码）、DuckDB、SQLite（reference/relational/sqlite3 可读源码）。
算法质量审查重点：优化器代价模型 vs PostgreSQL（动态规划 join 顺序、选择率估计）；HashJoin 构建/探测阶段溢写处理；
BTree 页分裂/删除 vs SQLite；MVCC 版本链与快照可见性判断 vs PostgreSQL heap 元组头。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\02-relational-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `02-relational-gap.md`，commit message：`docs: 完成 Relational 模态差距深度分析分卷`

---

### Task 4: Graph 模态深挖（03-graph-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026/03-graph-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：Graph】

审查对象：
- 源码：engineering/src/db/storage/graph/、engineering/src/db/graph/（先 Glob 列出，区分存储层与查询层）
- Cypher：近期提交 a37a6e396 修复过 Cypher 相关文件，含 parser/executor（在 engineering/src/db/ 下 Glob *cypher*）
- 图算法库：P1-4 已完成（commit 0b4ff2090），核实算法数量与实现质量
- 存储：CSR + COO 混合（旧对比文档）；核实增量更新如何处理 CSR 重构
- 测试：engineering/test/db/ 下 graph/cypher 相关

业界标杆（逐一对照）：Neo4j（reference/graph/neo4j 可读源码）、NebulaGraph、Memgraph。
算法质量审查重点：CSR 增量写放大 vs Neo4j PropertyStore；最短路径/PageRank 与 GDS 的正确性边界
（有向/无向、权重为负、自环）；Cypher 子集覆盖（MATCH/WHERE/RETURN/路径模式）vs openCypher。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\03-graph-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `03-graph-gap.md`，commit message：`docs: 完成 Graph 模态差距深度分析分卷`

---

### Task 5: KV 模态深挖（04-kv-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026/04-kv-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：KV】

审查对象：
- 源码：engineering/src/db/storage/kv/、engineering/include/db/kv.h、kv_engine.h
- Column Family：engineering/src/db/cf/（P3-4 已完成，commit bb2c77fbb）
- TTL/游标扫描/有序存储：在 kv 源码中核实实现机制
- WAL 集成：kv 写路径是否经过 storage/wal/
- 测试：engineering/test/db/ 下 kv/cf 相关

业界标杆（逐一对照）：Redis（reference/key-value/redis 可读源码）、RocksDB。
算法质量审查重点：页面布局 vs RocksDB LSM（写放大/空间放大/读放大三者权衡）；
过期清理是惰性还是主动；大 Value 的处理（TOAST 类机制）；Column Family 与 RocksDB CF 的语义对齐。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\04-kv-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `04-kv-gap.md`，commit message：`docs: 完成 KV 模态差距深度分析分卷`

---

### Task 6: Timeseries 模态深挖（05-timeseries-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026\05-timeseries-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：Timeseries】

审查对象：
- 源码：engineering/src/db/storage/ts/、engineering/include/db/（时序相关头文件，先 Glob）
- Segment 索引、Gorilla 压缩、滑动窗口聚合、降采样：逐一核实实现（旧对比文档称写入 ~50-100K 点/秒）
- 乱序写入处理：时间戳乱序时的行为
- 测试：engineering/test/db/ 下 ts/timeseries 相关

业界标杆（逐一对照）：InfluxDB 3.0、TimescaleDB、TDengine、QuestDB。
算法质量审查重点：Gorilla XOR 编码的 delta-of-delta 头部与精度（float 尾数位处理）；
乱序数据 vs TimescaleDB 的 hypertable 分区路由；连续查询/物化视图（旧文档称缺失，核实是否仍缺失）；
Segment 大小选择与冷热分层。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\05-timeseries-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `05-timeseries-gap.md`，commit message：`docs: 完成 Timeseries 模态差距深度分析分卷`

---

### Task 7: Document 模态深挖（06-document-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026\06-document-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：Document】

审查对象：
- 源码：engineering/src/db/storage/doc/、engineering/include/db/doc_engine.h
- JSONPath 查询、倒排索引、BM25：逐一核实（注意 sparse/ 下的 bm25_index.c 属未注册引擎，本卷只引用不深挖）
- 文档大小上限（旧对比文档称受 VecPage 16KB 限制，核实是否已解决）
- 聚合管道：旧文档称缺失，核实近期是否已实现
- 测试：engineering/test/db/ 下 doc 相关

业界标杆（逐一对照）：MongoDB、Elasticsearch（reference/search/elasticsearch 可读源码）、CouchDB。
算法质量审查重点：JSONPath 语义覆盖（过滤器/切片/递归下降）vs MongoDB 查询；
BM25 公式变体（k1/b 参数）与 Lucene 实现；嵌套文档索引；更新时倒排索引的一致性维护。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\06-document-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `06-document-gap.md`，commit message：`docs: 完成 Document 模态差距深度分析分卷`

---

### Task 8: Spatial 模态深挖（07-spatial-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026\07-spatial-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：Spatial】

审查对象：
- 源码：engineering/src/db/storage/spatial/、空间相关头文件（先 Glob）
- R-Tree、Hilbert 曲线、几何类型（Point/LineString/Polygon）、bbox 查询：逐一核实
- P2-3 空间分析 OpenSpec 文档已完成（commit 719c06d9c），核实其中承诺的空间分析函数落地情况
- 平面/球面坐标：核实是否仍仅支持平面欧氏
- 测试：engineering/test/db/ 下 spatial 相关（含 docs/diagrams/level3-vector/L3-010-rtree-structure.md 可参考）

业界标杆（逐一对照）：PostGIS、DuckDB Spatial、SpatiaLite、H3、S2。
算法质量审查重点：R-Tree 分裂策略（线性/二次/R*）vs PostGIS GiST；Hilbert 编码精度与边界；
多边形相交判定算法（射线法/轴投影）的鲁棒性；z-order 与 R-Tree 的查询计划选择。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\07-spatial-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `07-spatial-gap.md`，commit message：`docs: 完成 Spatial 模态差距深度分析分卷`

---

### Task 9: Tree 模态深挖（08-tree-gap.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026\08-tree-gap.md`（由子代理写入）

**Interfaces:** 同 Task 2 模式

- [ ] **Step 1: 派发深挖子代理**

模态专属段：

```text
【模态专属段：Tree（Yang/层次树）】

审查对象：
- 源码：engineering/src/db/storage/yang/、engineering/src/db/yang/、netconf/
- P3-3 Yang/NETCONF 模块已注册（commit beac0aee1），核实 RFC 6020/7950/6241 合规程度（YANG 模型解析、XPath、datastore、RPC）
- 层次存储、节点类型、父子关系、路径查询：逐一核实
- 测试：engineering/test/db/ 下 yang/netconf 相关

业界标杆（逐一对照）：libyang、sysrepo、PostgreSQL ltree、BaseX。
算法质量审查重点：YANG schema 解析的模块/子模块/导入解析 vs libyang；
XPath 子集覆盖；datastore 事务（candidate/running/startup）语义 vs sysrepo；
路径查询的物化路径 vs ltree 的 GiST 索引优化。

报告输出文件：D:\code\book\docs\multimodal-gap-analysis-2026\08-tree-gap.md
```

- [ ] **Step 2-5: 同 Task 2 的校验与提交流程**

校验对象改为 `08-tree-gap.md`，commit message：`docs: 完成 Tree 模态差距深度分析分卷`

---

### Task 10: 未注册引擎核对（09-unregistered-engines.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026\09-unregistered-engines.md`（由子代理写入）

**Interfaces:**
- Produces: 9 个引擎的核对结论，供 Task 12 汇总

- [ ] **Step 1: 派发轻量核对子代理**

用 Agent 工具（general-purpose）派发，提示词：

```text
你是资深数据库内核工程师，对 D:\code\book 仓库中 9 个"存在但未注册进 DataModel 枚举
（engineering/include/db/storage_engine.h）"的引擎做轻量核对。全程简体中文，只读分析，
禁止修改任何文件，禁止运行性能基准。

9 个引擎及代码位置（先 Glob 确认实际文件，再读源码）：
1. RDF/SPARQL：engineering/src/db/storage/rdf/（rdf_engine.c、rdf_index.c、sparql_parser.c）
2. 稀疏向量+BM25 混合检索：engineering/src/db/storage/sparse/（sparse_vector.c、bm25_index.c、hybrid_retrieval.c）
3. 流引擎：engineering/src/db/storage/stream/stream_engine.c
4. 列存引擎：engineering/src/db/storage/columnar/columnar_engine.c
5. Column Family：engineering/src/db/cf/（注意：P3-4 已完成；核实它属于 KV 模态扩展还是独立引擎，
   与 DataModel 枚举的关系）
6. st 引擎：engineering/src/db/storage/st/st_engine.c（先搞清楚它是什么：读文件头注释与 CMakeLists）
7. mmview：engineering/src/db/storage/mmview/
8. ledger 账本：engineering/src/db/ledger/ledger.c
9. CDC 订阅：engineering/src/db/subscription/（cdc.c、cdc_wal.c）

对每个引擎输出一节（三级标题）：
### N. <引擎名>
- **定位**：一句话说明它是什么、服务什么场景
- **完成度**：核心 API 清单（读头文件/源码提取）、已实现 vs 半成品 vs 占位
- **与业界对标**：同类标杆（如 RDF→Jena/Virtuoso/Blazegraph；CDC→Debezium；流→Kafka Streams）
  的关键差距 2-3 条，每条标注「功能缺失」或「实现质量缺陷」并附 file:line 证据
- **注册建议**：是否应加入 DataModel 枚举（MODEL_RDF=10 等）并接入 mm_storage 抽象，理由

报告文件末尾附汇总表：引擎 | 定位 | 完成度(0-10) | 是否建议注册。
直接写入：D:\code\book\docs\multimodal-gap-analysis-2026\09-unregistered-engines.md
最终回复只返回：报告路径 + 9 引擎完成度一览。
```

- [ ] **Step 2: 校验覆盖与证据**

```bash
cd D:/code/book && grep -c '^### ' docs/multimodal-gap-analysis-2026/09-unregistered-engines.md && grep -oE '[a-z_0-9]+\.(c|h|cpp):[0-9]+' docs/multimodal-gap-analysis-2026/09-unregistered-engines.md | wc -l
```

预期：`9`（九个小节）和 `≥ 27`（每引擎 ≥3 处证据）

- [ ] **Step 3: 修正并提交**

不达标则续问子代理补齐。达标后：

```bash
cd D:/code/book && git add docs/multimodal-gap-analysis-2026/09-unregistered-engines.md
git commit -m "docs: 完成未注册引擎核对分卷"
```

---

### Task 11: 缺失模态调研（10-missing-modalities.md）

**Files:**
- Create: `docs/multimodal-gap-analysis-2026\10-missing-modalities.md`（由子代理写入）

**Interfaces:**
- Produces: 5 个缺失模态的调研结论与建议，供 Task 12 汇总

- [ ] **Step 1: 派发调研子代理**

用 Agent 工具（general-purpose，允许 WebSearch/WebFetch）派发，提示词：

```text
你是资深数据库架构师，调研 5 类"自研多模态数据库（D:\code\book，已有
Relational/KV/Graph/Vector/Timeseries/Document/Spatial/Tree/Stream/Columnar 十类模型）
目前完全没有"的模态，评估补入价值。全程简体中文。

5 个调研对象：
1. 全文搜索引擎（Elasticsearch/Lucene 级；注意自研已有 Document 模态 BM25 + sparse/ 混合检索，
   需论证"专用搜索引擎"与现有能力的真实边界，避免重复建设）
2. 宽表 / Wide-Column（Cassandra/HBase/ScyllaDB）
3. 对象 / Blob 存储（S3/MinIO；图像/音视频/模型文件原生存储）
4. 可观测 / 日志分析（Grafana Loki/ClickHouse Observability/VictoriaLogs）
5. 多模态 AI 原生存储（图像/音频/视频 blob + embedding 绑定；Voxel51/Chroma/LanceDB 方向；
   注意仓库已有 RAG 系统 engineering/rag/，需评估与 RAG 栈的协同）

调研方法：优先 WebSearch/WebFetch 获取各产品 2025-2026 现状（架构、核心能力、规模指标）；
也可读 reference/ 下相关子模块源码（如 reference/search/elasticsearch）。每条关键数据注明出处 URL。

对每个模态输出一节（三级标题）：
### N. <模态名>
- **业界格局**：主流产品对比表（架构/查询能力/规模/生态）
- **典型场景**：什么 workload 必须用它而非现有十类模型
- **与自研架构契合度**：接入点（复用 Buffer Pool/WAL/mm_storage 抽象的可行性）、
  与现有模态的重叠与边界
- **补入建议**：建议「自研内核扩展」/「集成开源库」/「暂不建设」三选一并给理由

报告文件末尾附汇总表：模态 | 业界代表 | 契合度(0-10) | 建议。
直接写入：D:\code\book\docs\multimodal-gap-analysis-2026\10-missing-modalities.md
最终回复只返回：报告路径 + 5 模态建议一览。
```

- [ ] **Step 2: 校验覆盖与出处**

```bash
cd D:/code/book && grep -c '^### ' docs/multimodal-gap-analysis-2026/10-missing-modalities.md && grep -cE 'https?://' docs/multimodal-gap-analysis-2026/10-missing-modalities.md
```

预期：`5` 和 `≥ 10`（每模态 ≥2 个外部出处）

- [ ] **Step 3: 修正并提交**

不达标则续问子代理补齐。达标后：

```bash
cd D:/code/book && git add docs/multimodal-gap-analysis-2026/10-missing-modalities.md
git commit -m "docs: 完成缺失模态调研分卷"
```

---

### Task 12: README 总览汇总与终稿校验

**Files:**
- Modify: `docs/multimodal-gap-analysis-2026/README.md`（由主线程本人撰写，不派子代理）

**Interfaces:**
- Consumes: Task 2-11 的 10 个分卷全部内容

- [ ] **Step 1: 通读 10 个分卷，仲裁矛盾**

逐一 Read 全部分卷。发现分卷间结论冲突（如评分口径漂移、同一能力一处称已实现一处称缺失）时：读代码仲裁，无法仲裁的在 README 中并列双方观点并标注「争议」。修正结果直接改对应分卷。

- [ ] **Step 2: 重写 README 为终稿**

覆盖 `docs/multimodal-gap-analysis-2026/README.md`，结构：

```markdown
# 多模态数据库差距深度分析（2026-08）

> 实现质量级分析，取代 docs/multimodal-db-comparison-2026.md 的"差距分析"部分（该文档保留作功能级参考）。
> 设计文档：`../superpowers/specs/2026-08-27-multimodal-gap-analysis-design.md`

## 1. 执行摘要
（3-5 段：总体结论、最严重的正确性风险 Top 3、最高投入产出比的改进 Top 3）

## 2. 22 领域差距总矩阵
（一张表：领域 | 类型(深挖/核对/调研) | 总分 | 最大风险/差距 | 详见分卷链接）

## 3. 跨模态共性 Gap
（从各分卷提炼：哪些 Gap 出现在多个模态——如分布式、事务覆盖、并发控制、错误处理一致性；
每条列涉及的模态和证据出处）

## 4. 已关闭 Gap 备忘
（旧对比文档列出、经本次核实已关闭的能力清单——如 GPU 加速/图算法/Cypher/Column Family 等，逐条注明核实证据）

## 5. 优先级路线图
（正确性风险缺陷优先，其后按投入产出排序；每项标注 S/M/L 工作量与涉及模态）

## 6. 分卷目录
（11 个文件的链接表与一句话说明）
```

- [ ] **Step 3: 终稿校验**

```bash
cd D:/code/book && ls docs/multimodal-gap-analysis-2026/ && grep -c '^## ' docs/multimodal-gap-analysis-2026/README.md && wc -l docs/multimodal-gap-analysis-2026/*.md
```

预期：11 个文件齐全；README 二级标题 ≥ 6；README ≤ 300 行

- [ ] **Step 4: 提交**

```bash
cd D:/code/book && git add docs/multimodal-gap-analysis-2026/
git commit -m "docs: 完成多模态差距深度分析总览与终稿"
```

---

## Self-Review 记录

1. **规格覆盖**：设计文档 §二 的 22 领域 → Task 2-9（8 深挖）+ Task 10（9 核对）+ Task 11（5 调研）全覆盖；§四 骨架 → 共用标准段；§五 设计决策 → 共用标准段输出规则 + Task 12 Step 2 结构；§七 验收标准 → 各任务 Step 2-4 校验命令 + Task 12 Step 3；§八 错误处理 → 各任务"修正"步骤 + Task 12 Step 1 仲裁。无遗漏。
2. **占位符扫描**：所有子代理提示词完整可执行；校验命令有确切预期值。无 TBD。
3. **一致性**：分卷文件名 01-10 与 Task 1 占位 README 表格一致；commit message 中文统一；"共用标准段"在计划中定义一次、任务内显式引用并要求完整拼入（子代理驱动执行时由主线程拼接，符合"执行者只看自己任务"约束）。
