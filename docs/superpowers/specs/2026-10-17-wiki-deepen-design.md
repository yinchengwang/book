# wiki 库「算法相关小特性」深化 — 设计文档

| 字段 | 值 |
|------|-----|
| 文档版本 | v1.0 |
| 创建日期 | 2026-10-17 |
| 状态 | 设计已通过，待用户审核 spec |
| 范围 | 94 篇 wiki 全部（68 数据库 + 12 RAG + 14 Agent） |
| 文档存放 | `docs/superpowers/specs/` |
| 读者 | 个人研发者（学习决策导向） |
| 产出形式 | 94 篇 wiki 各加 1-3 节算法专节，总行数 +50% |
| 执行模式 | Subagent-Driven（沿用 gap-analysis 模式） |
| 排期 | 6-12 个工作日，按节奏推进 |

---

## 1. 背景与目标

### 1.1 背景

经过 18 个任务的多模态 wiki 库建设（`docs/db-wiki/`、`docs/rag-wiki/`、`docs/agent-wiki/`，共 94 篇 wiki + 3 个索引页），现已完成 24 维骨架的「广度」覆盖。每篇 wiki 17-25 节、500-700 行，已包含定位、架构、存储、索引、事务、复制、扩展、运维、生态、论文、源码路径、风险等结构化内容。

但**在算法细节深度上存在明显不足**：
- 部分 wiki 对核心算法**只提名字没细节**（如 Faiss 提到 PQ 但不展开训练过程）
- 部分 wiki 对**多个核心算法并列存在却只讲 1 个**（如 Milvus 提到 IVF/HNSW/DiskANN 三种但只展开 HNSW）
- 部分 wiki 缺**伪代码/公式/复杂度**等深度讲解
- progress.md 留待 Task 19+ 处理的 15 Minor 瑕疵（错路径/typo/章节漂移）顺带修复

### 1.2 目标

让 94 篇 wiki 在「算法相关的小特性」维度上达到深度讲解 — 每篇在保持现有 17-25 节骨架基础上，**按需新增 1-3 个专节**，章节名按内容自定。

**核心原则**：
- 不叫「小特性」「小特效」等通用名 — 章名要反映实际内容
- 不需要的就不加（如某 wiki 现有章节已涵盖算法细节，就不冗余加节）
- 现有 24 维骨架**不破坏** — 编号体系不动，新章节插入到合适位置
- 现有内容**不改** — 只在合适位置**追加**（按 OPSX 纪律）

### 1.3 不做的事

- 不重写已有章节内容（只追加不覆盖）
- 不修改 wiki 已写过的内容
- 不强制要求 `reference/*` 子模块 init（行号用「公开 canonical」+ 标注「approx」）
- 不修改 `engineering/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`
- 不引入新依赖
- 不做外部 benchmark 实测（仅引用 wiki/wiki 已引用的公开数字）

---

## 2. 算法相关范畴

每篇 wiki 按以下清单扫描是否需要新增/深化算法章节：

| 类别 | 典型算法 |
|------|---------|
| 存储引擎 | B+Tree 节点分裂、LSM Compaction、WAL 编码、MVCC 快照回收、Free Page 管理 |
| 索引算法 | B-Tree / HASH / GIN / GIST / SP-GiST / BRIN / Bloom / SkipList / ART / Radix Tree |
| **向量算法（独立大项）** | **ANN：HNSW / IVF / NSG / SSG / Vamana (DiskANN) / ScaNN / SPANN / CAGRA**；**量化：PQ / LVQ / SQ / BQ / OPQ / RQ / PRQ / RaBitQ / TurboQuant / LSQ / QAT / ITQ**；**距离：L2 / Inner Product / Cosine / Hamming**；**聚类：k-means / Hierarchical k-means / IVF 训练**；**图构建：邻居启发式剪枝、entry point、level generation (mL)、beam search**；**混合检索：BM25+向量 RRF、payload filter 集成**；**图/邻居压缩：邻居量化、距离表预计算**；**GPU 加速：FastScan、TurboQuant SIMD** |
| 查询优化 | Cascades 优化器、Volcano / Velocity 执行、向量化 SIMD、Join 算法、统计信息 |
| 事务并发 | MVCC 可见性、2PL / OCC / Percolator / Calvin、SI 隔离级别、Deadlock 检测 |
| 分布式 | Raft / Paxos 选举、Log Replication、Sharding 路由、TSO 分配、Linearizable Read |
| 编码压缩 | Snappy / Zstd / LZ4、Dedup、Delta Encoding、Dictionary、Paxos / RAID |
| 特定领域 | RAG: chunking 算法 / reranking / hybrid retrieval；Agent: graph traversal / tool selection / planning |

---

## 3. 章节命名规则

### 3.1 现有结构兼容

- 每篇 wiki 现有 17-25 节不动
- 编号体系不破坏（如「十二、扩展性」不能跳过变「十三、小特性」）
- 新章节插入到与内容相关的现有节**之后**（如向量算法放「八、索引机制」之后、「九、查询/计算/接口层」之前）

### 3.2 章节命名规则

| 内容类型 | 推荐章名 | 例 |
|---------|---------|---|
| 多个核心算法并列 | 「N. 关键算法与实现细节」 | Faiss: 「八.5 量化与训练算法（PQ / OPQ / k-means）」 |
| 单个深度算法 | 「N. 某某算法深度」 | RocksDB: 「七.5 Block Cache 与 Bloom Filter 选型」 |
| 编码/格式 | 「N. 编码与压缩算法」 | Cassandra: 「七.5 SSTable 编码与压缩」 |
| 分布式协议 | 「N. 共识与复制算法」 | CockroachDB: 「十一.5 Raft 优化与 Leaseholder」 |
| 优化器/规划器 | 「N. 查询优化算法」 | Postgres: 「九.5 规划器与代价模型」 |
| 事务并发 | 「N. 并发控制算法」 | TiDB: 「十.5 Percolator 分布式事务」 |
| 向量 | 「N. ANN 搜索与量化算法」 | Milvus: 「八.5 索引算法与量化（IVF / HNSW / DiskANN / PQ）」 |

### 3.3 不加节的判据

如果 wiki 现有章节**已经覆盖**算法细节（如「八、索引机制」已含 HNSW/IVF 公式和复杂度），就不加新节。仅在以下情况加节：
1. 现有章节**只提名字没细节**（如只说「使用 HNSW」没说图构建/选层/剪枝）
2. 现有章节**算法广度不够**（只讲 1 个，实际有 3-5 个）
3. 现有章节**没有伪代码 / 复杂度 / 工程考量**

### 3.4 命名禁忌

- ❌ 「小特性」「小特效」「小知识」「补充」等通用名
- ❌ 「N.5 杂项」等偷懒编号
- ✅ 章名要让人一眼看出**是什么算法**（如「HNSW 图构建与搜索」「PQ 编码与 k-means 训练」）

---

## 4. 验收标准

每篇 wiki 深化后必须满足 5 条 PASS：

| # | 验收项 | 通过标准 |
|---|--------|---------|
| 1 | **算法细节覆盖** | 新增/深化章节内至少 1 个**伪代码 / 公式 / 复杂度分析**（任一种） |
| 2 | **工程落地** | 至少 1 处**真实源码路径 + 行号**引用（如 `reference/<submodule>/src/...:NNN`）或本地 `engineering/` 路径对照 |
| 3 | **不破坏骨架** | 现有 17-25 节编号体系**未跳过/未重排**；新章节插入位置合理 |
| 4 | **不冗余** | 现有章节**已涵盖的算法不重复加节**；新内容确实是新加的 |
| 5 | **中文 + 术语** | 中文为主，专有名词保留英文（HNSW / PQ / Cascades 等不翻译） |

### 4.1 全量自检

- 94 篇 wiki 全部 PASS 5 条
- 总章节数 17-25（深化前）→ 18-30（深化后），平均每篇 +1-3 节
- 总体行数从 52126 → 70000-80000 行（每篇 +200-500 行）
- 现有瑕疵（progress.md 留待 Task 19+ 的 15 Minor）顺带修复

---

## 5. 执行模式与任务切片

### 5.1 执行模式：Subagent-Driven

| 维度 | Subagent-Driven | Inline |
|------|----------------|--------|
| 上下文隔离 | ✅ 每篇 implementer 独立上下文 | ❌ 上下文会爆炸 |
| API 错误重试 | ✅ 上次 5 个中断都成功重试 | ❌ 中断后整个 plan 中断 |
| 速度 | 中（每篇 2-5 min dispatch） | 慢（同一上下文会越跑越慢） |
| 质量 | ✅ 独立 implementer + 自我 review | ⚠️ 我自己会疲 |

### 5.2 任务切片

```
Phase 1（db-wiki 12 类 68 篇）：
  - Batch 1: relational(7) + distributed-sql(4) = 11 篇
  - Batch 2: key-value(7) + document(5) = 12 篇
  - Batch 3: columnar(5) + time-series(5) = 10 篇
  - Batch 4: search(6) + vector(9) = 15 篇
  - Batch 5: graph(6) + stream(4) = 10 篇
  - Batch 6: embedded(4) + extension(6) = 10 篇
Phase 2（rag-wiki 12 篇）
Phase 3（agent-wiki 14 篇）
Phase 4（全量自检 + 进度更新）
```

**总计 94 + 4 = 98 个 task**（每篇 1 task + 4 phase 自检）。

### 5.3 每篇 task 流程

1. **生成 brief**（`scripts/task-brief`）
2. **派发 implementer**（sonnet，机械修改 1 文件）
3. **self-check 5 条**
4. **commit + push**（中文 commit message，1 文件 1 commit）
5. **追加 progress.md**

---

## 6. 工作量与排期

### 6.1 工作量估算

| Phase | 内容 | 篇数 | 估算 |
|-------|------|------|------|
| Phase 1 | db-wiki 12 类 | 68 | 4-8 天 |
| Phase 2 | rag-wiki | 12 | 1-2 天 |
| Phase 3 | agent-wiki | 14 | 1-2 天 |
| Phase 4 | 全量自检 + 进度 | — | 0.5 天 |
| **合计** | | **94** | **6-12 天** |

每篇 30-60 min（audit 5-10 min + 写 1-3 节 20-40 min + commit + push 2 min）。

### 6.2 节奏建议

- 每个工作日跑 8-15 篇（2-3 个 batch）
- 每个 batch 结束做小自检（X 篇 PASS 5 条）
- 每周回顾 1 次：看 progress.md 调整后续 batch 优先级
- 不加班、不赶时间：按自然节奏推进，质量优先

---

## 7. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| API 中断 | 中 | 单篇失败 | 自动重试（gap-analysis 验证 5/5 成功） |
| 行号漂移（子模块未 init） | 高 | 引用不准 | 行号用「公开 canonical」+ 标注「approx」 |
| 重复添加相同内容 | 中 | 冗余 | 5 条验收含「不冗余」+ 控制器抽查 |
| 章名不一致 | 低 | 体验差 | 命名规则 §3.4 + 控制器抽查 |
| 94 篇疲劳 | 中 | 质量下滑 | 每天 8-15 篇上限 + 周末休息 |
| 与 wiki 既有内容矛盾 | 低 | 信任损失 | implementer 必须先 Read 全文再写 |

---

## 8. 验收与全量

### 8.1 总报告验收

- 94 篇 wiki 全部 PASS 5 条
- 总体行数从 52126 → 70000-80000 行
- 总体章节数从 ~2000 → ~2200-2400
- progress.md 含 wiki-deepen 段，按 task 完成顺序追加

### 8.2 进度跟踪

- 续写 `archive/.superpowers/sdd/progress.md` 加 wiki-deepen 段
- 每个 task 完成后追加一行（commits + self-check verdict）

---

## 9. 不做（明确排除）

- 不重写已有 wiki 内容
- 不修改 wiki 已写过的内容
- 不强制 reference 子模块 init
- 不修改 engineering/、learning/、根 CMakeLists.txt、CLAUDE.md、AGENTS.md
- 不引入新依赖
- 不做外部 benchmark 实测
- 不为商业产品推测内部技术细节

---

## 10. 参考

- `docs/superpowers/specs/2026-10-16-multimodal-db-wiki-design.md`：原 wiki 库设计（24 维骨架）
- `docs/superpowers/plans/2026-10-16-multimodal-db-wiki-implementation.md`：原 wiki 库实施计划
- `docs/superpowers/specs/2026-10-16-gap-analysis-design.md`：gap-analysis 设计
- `docs/superpowers/plans/2026-10-16-gap-analysis-implementation.md`：gap-analysis 实施计划
- `docs/db-wiki/README.md` / `docs/rag-wiki/README.md` / `docs/agent-wiki/README.md`：94 篇 wiki 入口
- `engineering/CLAUDE.md`：自研现状
- `CLAUDE.md`：项目语言规范、双轨纪律、提交推送纪律
