# 多模态 wiki 库差距分析 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于 94 篇 wiki 库 + 自研多模态数据库 + multimodal_rag 应用，输出 1 篇总报告 + 13 篇子报告 + 补齐项下月计划，帮助个人研发者看清自研与业界的针对性差距与下一步动作。

**Architecture:** 新建顶层目录 `docs/gap-analysis/`，按 12 数据模型 + RAG + Agent 拆 13 篇子报告，每篇子报告统一 7 节模板（自研现状 / 业界主流 / 24 维度差距矩阵 / Top-5 差距 / Top-3 优势 / 学习路径 / 补齐项），最后汇总成 1 篇总报告。

**Tech Stack:** Markdown, Git, 中文 commit message, 24 维度对照矩阵（与 wiki 设计文档 §4.2 同源）。

## Global Constraints

1. **目标读者**：个人研发者（学习决策导向），不做对外宣传、不做团队协作
2. **对照范围**：94 篇 wiki 全谱对位（68 数据库 + 11 RAG + 13 Agent + 2 商业）
3. **产出形式**：1 总报告 + 13 子报告 + 补齐项下月计划
4. **执行模式**：分类批量串行；不使用 subagent（避免 100+ subagent 成本）；每天 2-3 篇
5. **每篇单独 commit + push**（commit message 中文 `docs: 新增 gap-analysis/<name> 子报告`）
6. **禁止 force-push on main**（wiki 任务链 Task 12 教训）
7. **不修改** 根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`、`engineering/`、`learning/`
8. **不修改** wiki 三目录（`docs/db-wiki/`、`docs/rag-wiki/`、`docs/agent-wiki/`）—仅在末尾追加「差距分析」直达链接
9. **不修改** wiki 已写过的内容—只引用，不复制
10. **每篇验收标准**（来自 spec §6.3）：
    - 24 维度对照矩阵完整
    - Top-5 差距每条有具体代码路径 / wiki 引用
    - 补齐项下月计划至少 3 条 P0/P1
    - 至少 5 处 `engineering/` 真实路径引用
    - 至少 2 个 wiki 链接引用

---

## Task 1: 探索工程轨自研现状

**Files:**
- Read: `engineering/CMakeLists.txt`（确认工程轨总入口）
- Read: `engineering/include/db/`（核心 DB 头文件）
- Read: `engineering/src/db/`（核心 DB 源码目录）
- Read: `engineering/src/db/index/`（索引模块）
- Read: `engineering/src/db/executor/`（执行器）
- Read: `engineering/apps/multimodal_rag/`（多模态 RAG 应用 14 子模块）
- Output: `.superpowers/sdd/gap-task-1-inventory.md`（自研现状清单，供后续子报告引用）

**Interfaces:**
- Consumes: `engineering/` 全部源码目录
- Produces: 一份自研现状清单（按 13 子报告对应的模块拆分），每条含：模块名、关键文件、覆盖度评估

- [ ] **Step 1: 列 engineering/ 顶层结构**

```bash
cd C:/code/book
ls engineering/ 2>&1 | head -30
ls engineering/include/db/ 2>&1 | head -30
ls engineering/src/db/ 2>&1 | head -30
```

记录输出。

- [ ] **Step 2: 列工程轨核心模块**

```bash
cd C:/code/book
ls engineering/src/db/index/ 2>&1
ls engineering/src/db/executor/ 2>&1
ls engineering/src/db/engine/ 2>&1
ls engineering/src/db/storage/ 2>&1
ls engineering/src/db/distributed/ 2>&1
```

记录每个目录的关键模块。

- [ ] **Step 3: 列 multimodal_rag 应用子模块**

```bash
cd C:/code/book
ls engineering/apps/multimodal_rag/ 2>&1
ls engineering/apps/multimodal_rag/src/ 2>&1
ls engineering/apps/multimodal_rag/include/ 2>&1
ls engineering/apps/multimodal_rag/config/ 2>&1
```

记录 14 个子模块的覆盖情况。

- [ ] **Step 4: 写自研现状清单**

写入 `.superpowers/sdd/gap-task-1-inventory.md`（被 .gitignore 忽略，不提交），结构：

```markdown
# 自研现状清单（13 子报告映射）

## 1. relational
- 模块: engineering/src/db/storage, engineering/src/db/heapam, engineering/src/db/btreeam, engineering/src/db/sql
- 关键文件: <列出 3-5 个核心 .c/.h>
- 覆盖度: A/B/C/D

## 2. key-value
...

## 3. document
...

## 4. columnar
...

## 5. time-series
...

## 6. search
...

## 7. vector
- 模块: engineering/src/db/index/vector_index/*
- 关键文件:
  - engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw.h
  - engineering/src/db/index/vector_index/dist_hnsw/dist_hnsw.h
  - engineering/src/db/index/vector_index/faiss_ivf_compat/
  - engineering/src/db/access/vector_am.c
- 覆盖度: A/B/C/D

## 8. graph
...

## 9. stream
- 模块: <未发现/部分发现/已实现>
- 覆盖度: D（很可能缺失）

## 10. embedded
- 模块: <未发现>
- 覆盖度: D

## 11. extension
...

## 12. rag
- 模块: engineering/apps/multimodal_rag/* (14 子模块)
- 关键文件: <按子模块列出>
- 覆盖度: A/B/C/D

## 13. agent
- 模块: engineering/apps/multimodal_rag/agent/
- 覆盖度: A/B/C/D
```

每条覆盖度评估基于实际文件存在性 + 是否含完整实现（如是否有 .c + .h + CMakeLists）。

- [ ] **Step 5: 自检**

打开 `.superpowers/sdd/gap-task-1-inventory.md`，确认 13 个分类都有条目、每条都有「关键文件 + 覆盖度」。

Expected: 13 条全齐，关键文件路径真实存在于 `engineering/`，覆盖度评估有依据。

**不要 commit**。此文件是内部参考，不提交。

---

## Task 2: 创建 gap-analysis 目录与 README 占位

**Files:**
- Create: `docs/gap-analysis/.gitkeep`（让 git 跟踪空目录）

- [ ] **Step 1: 创建目录**

```bash
cd C:/code/book
mkdir -p docs/gap-analysis
touch docs/gap-analysis/.gitkeep
```

- [ ] **Step 2: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current  # 确认 main
git add docs/gap-analysis/.gitkeep
git commit -m "docs: 建 gap-analysis 顶层目录"
git push origin main
```

Expected: 1 commit 推送成功。

---

## Task 3-15: 写 13 篇子报告（每篇 1 task）

每篇子报告结构统一（来自 spec §4.2）：

1. **自研现状**（5-10 条：技术决策 / 关键路径 / 覆盖度）— 引用 Task 1 的 inventory
2. **业界主流做法**（2-3 个对照项目）— 引用 `docs/db-wiki/<分类>/<项目>.md`
3. **针对性差距矩阵**（24 维度逐条对比）— A/B/C/D 评级
4. **关键差距 Top-5**（具体到代码路径 / wiki 引用）
5. **优势 Top-3**（自研独有的设计）
6. **学习路径**（推荐精读 2-3 篇 wiki）
7. **补齐项下月计划**（3-5 个 P0/P1/P2 动作）

每篇 300-500 行 markdown。

### Task 3: relational 子报告

**Files:**
- Create: `docs/gap-analysis/relational.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §1 relational。

- [ ] **Step 2: 读 3 篇对照 wiki**

```bash
cd C:/code/book
# 用 Read 工具读以下 3 个文件
# docs/db-wiki/relational/postgres.md
# docs/db-wiki/relational/mysql.md
# docs/db-wiki/distributed-sql/tidb.md
```

重点抽取：存储引擎、索引机制、事务一致性、复制高可用 4 个维度的关键内容。

- [ ] **Step 3: 写 7 节内容**

按统一模板写 `docs/gap-analysis/relational.md`：
- 1. 自研现状（5-10 条，引用 engineering/src/db/storage/, heapam.c, btreeam.c）
- 2. 业界主流（postgres 的 MVCC + heap-only tuple、tidb 的 Percolator 事务、mysql 的 InnoDB）
- 3. 24 维度对照矩阵（Markdown 表格，列：自研 / postgres / tidb / 等级）
- 4. 关键差距 Top-5（每条带 engineering/ 路径 + wiki 路径）
- 5. 优势 Top-3（如 PG 风格的多模态复用）
- 6. 学习路径（2-3 篇 wiki 链接）
- 7. 补齐项下月计划（3-5 个 P0/P1/P2 动作）

- [ ] **Step 4: 自检**

按 5 条验收标准核对：
- [ ] 24 维度对照矩阵完整
- [ ] Top-5 差距每条有具体代码路径 / wiki 引用
- [ ] 补齐项下月计划至少 3 条 P0/P1
- [ ] 至少 5 处 `engineering/` 真实路径引用
- [ ] 至少 2 个 wiki 链接引用

- [ ] **Step 5: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/gap-analysis/relational.md
git commit -m "docs: 新增 gap-analysis/relational 子报告"
git push origin main
```

Expected: 1 commit 推送成功。

### Task 4: key-value 子报告

**Files:**
- Create: `docs/gap-analysis/key-value.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §2 key-value。

- [ ] **Step 2: 读 3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/key-value/rocksdb.md
# docs/db-wiki/key-value/leveldb.md
# docs/db-wiki/key-value/redis.md
```

重点：LSM-Tree、WAL、Compaction、MemTable、SkipList。

- [ ] **Step 3: 写 7 节内容**

按统一模板（参考 Task 3）。

- [ ] **Step 4: 自检**

按 5 条验收标准核对。

- [ ] **Step 5: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/gap-analysis/key-value.md
git commit -m "docs: 新增 gap-analysis/key-value 子报告"
git push origin main
```

### Task 5: document 子报告

**Files:**
- Create: `docs/gap-analysis/document.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §3 document。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/document/mongodb.md
# docs/db-wiki/document/ferretdb.md
# docs/db-wiki/document/couchdb.md
```

重点：BSON / 文档模型 / 索引机制 / 复制协议。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 6: columnar 子报告

**Files:**
- Create: `docs/gap-analysis/columnar.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §4 columnar。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/columnar/clickhouse.md
# docs/db-wiki/columnar/doris.md
# docs/db-wiki/columnar/starrocks.md
```

重点：列存 / 压缩 / 向量化执行 / 物化视图。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 7: time-series 子报告

**Files:**
- Create: `docs/gap-analysis/time-series.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §5 time-series。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/time-series/influxdb.md
# docs/db-wiki/time-series/tdengine.md
# docs/db-wiki/time-series/questdb.md
```

重点：TSM / downsampling / retention / out-of-order writes。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 8: search 子报告

**Files:**
- Create: `docs/gap-analysis/search.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §6 search。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/search/elasticsearch.md
# docs/db-wiki/search/tantivy.md
# docs/db-wiki/search/meilisearch.md
```

重点：倒排索引 / FST / BM25 / DSL / 聚合。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 9: vector 子报告（最关键）

**Files:**
- Create: `docs/gap-analysis/vector.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §7 vector。**重点关注**：
- `engineering/src/db/index/vector_index/faiss_hnsw/`（自研 HNSW 移植 faiss）
- `engineering/src/db/index/vector_index/dist_hnsw/`（分布式 HNSW）
- `engineering/src/db/index/vector_index/faiss_ivf_compat/`（IVF 兼容层）
- `engineering/src/db/access/vector_am.c`（访问方法）
- `engineering/src/db/api/vector_api.c`（API 层）

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/vector/milvus.md
# docs/db-wiki/vector/faiss.md
# docs/db-wiki/vector/qdrant.md
```

重点：HNSW / IVF / DiskANN / ScaNN / 量化（PQ/LVQ/SQ/RaBitQ）/ GPU 加速 / hybrid retrieval / payload 过滤。

- [ ] **Step 3: 写 7 节内容**

特别在第 3 节差距矩阵中重点对比：
- 24 维度中的第 7（存储引擎）、8（索引机制）、13（性能基准）三行
- Top-5 差距应该 2-3 条与量化 / DiskANN / GPU 相关

- [ ] **Step 4: 自检**

按 5 条验收标准核对 + 特别检查量化章节是否覆盖 PQ/LVQ/SQ/RaBitQ。

- [ ] **Step 5: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/gap-analysis/vector.md
git commit -m "docs: 新增 gap-analysis/vector 子报告"
git push origin main
```

### Task 10: graph 子报告

**Files:**
- Create: `docs/gap-analysis/graph.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §8 graph。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/graph/neo4j.md
# docs/db-wiki/graph/kuzu.md
# docs/db-wiki/graph/nebula.md
```

重点：原生图存储 / CSR / Cypher / GQL / Pregel。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 11: stream 子报告

**Files:**
- Create: `docs/gap-analysis/stream.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §9 stream。**预期发现**：自研在 `engineering/src/db/` 中可能没有流式子系统。如果确认缺失，第 3 节差距矩阵中第 6/7/8/9 维度应标 D。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/stream/kafka.md
# docs/db-wiki/stream/pulsar.md
# docs/db-wiki/stream/redpanda.md
```

重点：Log 结构 / 分区 / ISR / EOS / Schema 演进。

- [ ] **Step 3-5: 同 Task 3 模板**

如自研缺失，第 7 节补齐项至少 1 个 P0 写「启动流式子系统」。

### Task 12: embedded 子报告

**Files:**
- Create: `docs/gap-analysis/embedded.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §10 embedded。**预期发现**：自研在 db 中可能没有嵌入式子系统。`engineering/src/db/storage/cold.c` 可能是最接近的（冷热分层）。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/embedded/duckdb.md
# docs/db-wiki/embedded/lmdb.md
# docs/db-wiki/embedded/boltdb.md
```

重点：mmap / 文件 I/O / 多线程安全 / 崩溃恢复。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 13: extension 子报告

**Files:**
- Create: `docs/gap-analysis/extension.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §11 extension。

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/db-wiki/extension/arrow.md
# docs/db-wiki/extension/datafusion.md
# docs/db-wiki/extension/velox.md
```

重点：列式内存格式 / 向量化执行 / 跨语言互操作 / UDF。

- [ ] **Step 3-5: 同 Task 3 模板**

### Task 14: rag 子报告（与 agent 并列第二重要）

**Files:**
- Create: `docs/gap-analysis/rag.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §12 rag。**重点关注**：
- `engineering/apps/multimodal_rag/src/agent/`（agent 模块）
- `engineering/apps/multimodal_rag/src/core/`（核心逻辑）
- `engineering/apps/multimodal_rag/src/embed/`（嵌入）
- `engineering/apps/multimodal_rag/src/engine/`（引擎）
- `engineering/apps/multimodal_rag/src/eval/`（评估）
- `engineering/apps/multimodal_rag/src/index/`（索引）
- `engineering/apps/multimodal_rag/src/llm/`（LLM 集成）
- `engineering/apps/multimodal_rag/src/retrieval/`（检索）
- `engineering/apps/multimodal_rag/src/pipeline/`（管道）
- `engineering/apps/multimodal_rag/config/default.yaml`（默认配置）

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/rag-wiki/langchain.md
# docs/rag-wiki/llama-index.md
# docs/rag-wiki/haystack-ai.md
```

重点：4 个 RAG 维度（嵌入模型适配 / 文档解析器 / 检索器重排 / 向量库后端）+ 评估观测 / 生产部署。

- [ ] **Step 3: 写 7 节内容**

特别在第 3 节差距矩阵中重点对比：
- 24 维度的 6（整体架构）、9（查询/计算/接口层）、14（运维可观测性）、15（生态集成）
- 自研 multimodal_rag 的 14 个子模块映射到对照框架的对应功能

- [ ] **Step 4: 自检**

按 5 条验收标准核对 + 特别检查 14 个子模块是否都有覆盖度评估。

- [ ] **Step 5: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/gap-analysis/rag.md
git commit -m "docs: 新增 gap-analysis/rag 子报告"
git push origin main
```

### Task 15: agent 子报告

**Files:**
- Create: `docs/gap-analysis/agent.md`

- [ ] **Step 1: 读自研现状**

读 Task 1 inventory 的 §13 agent。**重点关注**：
- `engineering/apps/multimodal_rag/src/agent/`（agent 模块）

- [ ] **Step 2: 读 2-3 篇对照 wiki**

```bash
cd C:/code/book
# Read 工具读
# docs/agent-wiki/langgraph.md
# docs/agent-wiki/crewai.md
# docs/agent-wiki/autogen.md
```

重点：4 个 Agent 维度（角色协作 / 记忆系统 / 工具调用协议 / 人机协作）+ 图执行引擎 / 状态管理 / 多 Agent 通信。

- [ ] **Step 3-5: 同 Task 3 模板**

---

## Task 16: 写总报告 README.md

**Files:**
- Create: `docs/gap-analysis/README.md`（600-900 行）
- Create: `.superpowers/sdd/gap-task-16-summary.md`（内部数据收集）

- [ ] **Step 1: 收集子报告 Top-5 差距**

读 13 篇子报告的「关键差距 Top-5」节，整理成 1 张总表：

```markdown
| 子报告 | 差距 #1 | 差距 #2 | 差距 #3 | 差距 #4 | 差距 #5 |
|--------|---------|---------|---------|---------|---------|
| relational | ... | ... | ... | ... | ... |
| key-value | ... | ... | ... | ... | ... |
| ... |
```

写到 `.superpowers/sdd/gap-task-16-summary.md`。

- [ ] **Step 2: 收集子报告 Top-3 优势**

同上。

- [ ] **Step 3: 收集补齐项下月计划**

13 篇子报告的「补齐项下月计划」节合并去重，按 P0 优先排序。

- [ ] **Step 4: 写总报告 6 节内容**

写 `docs/gap-analysis/README.md`：

1. **摘要**：自研覆盖度统计（12 模态 × 24 维度的覆盖率矩阵）
2. **整体差距诊断**：5-7 条关键短板（从子报告 Top-5 提炼）
3. **优势亮点**：3-5 条自研独有的设计选择
4. **RAG/Agent 专题结论**：从 rag.md / agent.md 子报告提炼
5. **6 个月路线图**：12 个子报告各 1 行的补齐项（按 P0/P1/P2 排序）
6. **风险与陷阱**：5-7 条认知偏差

- [ ] **Step 5: 自检**

按 §7.1 总报告验收核对：
- [ ] 12 模态覆盖度统计完整
- [ ] 5-7 条整体差距诊断具体到子报告
- [ ] 3-5 条优势亮点有自研独有证据
- [ ] 6 个月路线图按 P0/P1/P2 排序
- [ ] 风险与陷阱段 5-7 条

- [ ] **Step 6: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/gap-analysis/README.md
git commit -m "docs: 新增 gap-analysis 总报告（13 子报告汇总 + 6 个月路线图）"
git push origin main
```

---

## Task 17: 更新 wiki 三个 README 加直达链接

**Files:**
- Modify: `docs/db-wiki/README.md`（在末尾追加「差距分析」段）
- Modify: `docs/rag-wiki/README.md`（在末尾追加「差距分析」段）
- Modify: `docs/agent-wiki/README.md`（在末尾追加「差距分析」段）

- [ ] **Step 1: 更新 docs/db-wiki/README.md**

在末尾追加（**不要覆盖原有内容**）：

```markdown

---

## 差距分析

基于本 wiki 库的「自研 vs 业界」分析已发布，详见：

- 总报告：[docs/gap-analysis/README.md](../gap-analysis/README.md)
- 子报告：[docs/gap-analysis/relational.md](../gap-analysis/relational.md)、[key-value.md](../gap-analysis/key-value.md)、[document.md](../gap-analysis/document.md)、[columnar.md](../gap-analysis/columnar.md)、[time-series.md](../gap-analysis/time-series.md)、[search.md](../gap-analysis/search.md)、[vector.md](../gap-analysis/vector.md)、[graph.md](../gap-analysis/graph.md)、[stream.md](../gap-analysis/stream.md)、[embedded.md](../gap-analysis/embedded.md)、[extension.md](../gap-analysis/extension.md)

最后更新：2026-10-16
```

- [ ] **Step 2: 更新 docs/rag-wiki/README.md**

在末尾追加（**不要覆盖原有内容**）：

```markdown

---

## 差距分析

基于本 wiki 库的「自研 vs 业界」RAG 专题分析已发布，详见：

- 总报告：[docs/gap-analysis/README.md](../gap-analysis/README.md)
- RAG 子报告：[docs/gap-analysis/rag.md](../gap-analysis/rag.md)

最后更新：2026-10-16
```

- [ ] **Step 3: 更新 docs/agent-wiki/README.md**

在末尾追加（**不要覆盖原有内容**）：

```markdown

---

## 差距分析

基于本 wiki 库的「自研 vs 业界」Agent 专题分析已发布，详见：

- 总报告：[docs/gap-analysis/README.md](../gap-analysis/README.md)
- Agent 子报告：[docs/gap-analysis/agent.md](../gap-analysis/agent.md)

最后更新：2026-10-16
```

- [ ] **Step 4: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/db-wiki/README.md docs/rag-wiki/README.md docs/agent-wiki/README.md
git commit -m "docs: 三个 wiki README 末尾追加 gap-analysis 直达链接"
git push origin main
```

Expected: 1 commit 推送成功。

---

## Task 18: 全量自检 + 清理 .gitkeep

**Files:**
- Modify: `docs/gap-analysis/.gitkeep`（删除，已被 README.md 替代）

- [ ] **Step 1: 全量自检**

```bash
cd C:/code/book
# 检查 14 个 .md 文件全部存在
test -f docs/gap-analysis/README.md && echo "总报告 OK"
for f in docs/gap-analysis/{relational,key-value,document,columnar,time-series,search,vector,graph,stream,embedded,extension,rag,agent}.md; do
  test -f "$f" && echo "$f OK" || echo "$f MISSING"
done

# 检查每篇至少 24 维度对照矩阵（grep 期望见到 24 行 "|"）
for f in docs/gap-analysis/*.md; do
  echo "=== $f ==="
  grep -c "^|" "$f" 2>&1 | head -1
done

# 检查每篇至少 5 处 engineering/ 路径引用
for f in docs/gap-analysis/*.md; do
  cnt=$(grep -c "engineering/" "$f")
  if [ "$cnt" -lt 5 ]; then
    echo "WARN: $f 只有 $cnt 处 engineering/ 引用"
  fi
done
```

Expected: 14 个文件全 OK，无 MISSING，无 WARN。

- [ ] **Step 2: 删 .gitkeep**

```bash
cd C:/code/book
rm docs/gap-analysis/.gitkeep
```

- [ ] **Step 3: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add docs/gap-analysis/.gitkeep
git commit -m "chore: 移除 gap-analysis 目录 .gitkeep 占位文件"
git push origin main
```

Expected: 1 commit 推送成功。

---

## Task 19: 续写 progress.md

**Files:**
- Modify: `.superpowers/sdd/progress.md`（追加 gap-analysis 段）

- [ ] **Step 1: 续写 progress.md**

在末尾追加：

```markdown

## gap-analysis 进度

- 计划：`docs/superpowers/plans/2026-10-16-gap-analysis-implementation.md`
- 设计：`docs/superpowers/specs/2026-10-16-gap-analysis-design.md`

### Task 状态

- Task 1: complete (engineering/ 自研现状清单)
- Task 2: complete (建 gap-analysis 目录)
- Task 3-15: 13 篇子报告 — 按完成顺序追加
- Task 16: 总报告
- Task 17: 三个 wiki README 加链接
- Task 18: 全量自检 + 清道夫
- Task 19: 续写 progress.md（本任务）
```

实际填写时按 commit 顺序追加 1 行每个 Task。

- [ ] **Step 2: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current
git add .superpowers/sdd/progress.md
# 注意：此文件在 .gitignore 内？检查
test -f .gitignore && grep -q "^\.superpowers" .gitignore && echo "在 .gitignore 内，仅本地不提交" || {
  git commit -m "docs: 续写 progress.md gap-analysis 进度"
  git push origin main
}
```

如果 `.superpowers/` 在 .gitignore 内，跳过 commit（保留本地）。

---

## 验收清单

- [ ] 14 个 .md 文件（1 总 + 13 子）全部存在
- [ ] 每篇子报告 24 维度对照矩阵完整
- [ ] 每篇子报告 Top-5 差距有具体代码路径 / wiki 引用
- [ ] 每篇子报告补齐项下月计划至少 3 条 P0/P1
- [ ] 每篇子报告至少 5 处 `engineering/` 路径引用
- [ ] 总报告 6 节完整（摘要 / 差距 / 优势 / RAG-Agent / 路线图 / 风险）
- [ ] 三个 wiki README 已追加直达链接
- [ ] 所有 commit 已 push origin main
- [ ] 无 force-push on main
- [ ] 不修改 `engineering/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`
- [ ] 不修改 wiki 已写过的内容
