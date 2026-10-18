# wiki 库算法深化 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 基于 94 篇现有 wiki（68 数据库 + 12 RAG + 14 Agent），按需为每篇新增 1-3 节算法专节（章名按内容自定，不叫"小特性"），使总行数从 52126 提升至 70000-80000 行（+50%）。

**Architecture:** 4 Phase × N Batch × 1 wiki/task 结构。每篇 wiki 1 task，按 db-wiki(68) / rag-wiki(12) / agent-wiki(14) 分 Phase。Phase 1 拆 6 个 Batch（按 12 类聚合），Phase 2 拆 2 个 Batch（11 RAG + 1 商业），Phase 3 拆 2 个 Batch（13 Agent + 1 商业），Phase 4 全量自检 + 进度更新。

**Tech Stack:** Markdown, Git, 中文 commit message, 5 条验收 PASS（per-wiki）。

## Global Constraints

1. **范围**：仅 94 篇 wiki（不含 3 个 README 索引页） + progress.md 更新
2. **现有结构不破坏**：每篇 17-25 节不动，编号体系不跳
3. **章名自定**：不叫"小特性""小特效""补充"等通用名，要反映实际算法内容
4. **不加节判据**：现有章节已涵盖算法细节则不加；只加 1-3 节
5. **行号宽容**：`reference/*` 子模块未 init 时用「公开 canonical」+ 标注「approx」
6. **每篇单独 commit + push**（中文 commit message `docs: 深化 docs/{db,rag,agent}-wiki/<name>.md 算法细节`）
7. **禁止 force-push on main**
8. **不修改** `engineering/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`
9. **不修改** wiki 已写过的内容 — 只追加，不覆盖
10. **每篇验收 5 条**：
    - 至少 1 个伪代码 / 公式 / 复杂度分析
    - 至少 1 处真实源码路径（reference/ 或 engineering/）+ 行号
    - 现有 17-25 节编号未跳过/未重排
    - 不冗余（不重复现有章节）
    - 中文 + 术语保留英文

---

## Task 模板（每篇 wiki 用此模板）

每篇 wiki 1 task，按以下 5 步执行。Task 编号 `WD-N`（WD = Wiki Deepen）。

### Task WD-N: 深化 `<wiki-file>.md` 算法细节

**Files:**
- Modify: `docs/{db,rag,agent}-wiki/<分类>/<name>.md`（追加 1-3 节算法专节，不动现有内容）

**Interfaces:**
- Consumes: `docs/superpowers/specs/2026-10-17-wiki-deepen-design.md` §3 命名规则 + §2 算法范畴
- Produces: 1 个 commit，1 file changed，N lines added

- [ ] **Step 1: 读 wiki 全文 + 决定是否需要新节**

```bash
# Read 工具读完整 wiki（可能 500-800 行）
# 重点 audit：
# - §3 算法范畴 8 大类中本项目涉及哪些
# - 现有章节是否已涵盖（按 §3.3 判据）
# - 决定加 0 节 / 1 节 / 2 节 / 3 节
```

记录 audit 结果 + 决定加几节 + 章名。

- [ ] **Step 2: 生成 brief + 派发 implementer**

```bash
cd C:/code/book
bash .claude/skills/subagent-driven-development/scripts/task-brief docs/superpowers/plans/2026-10-17-wiki-deepen-implementation.md WD-N
# 派发 subagent（sonnet），给 brief 路径 + 报告路径
```

implementer 必须：
1. Read 现有 wiki 全文（500-800 行）
2. 按 audit 结果在合适位置插入 1-3 节
3. 每节 ≥ 1 个伪代码 / 公式 / 复杂度
4. 每节 ≥ 1 处 `reference/<submodule>/path:NNN` 或 `engineering/.../path:NNN` 真实路径
5. 报告 5 条 self-check

- [ ] **Step 3: 验证 5 条 self-check**

- [ ] 算法细节覆盖：YES/NO
- [ ] 工程落地：YES/NO
- [ ] 不破坏骨架：YES/NO
- [ ] 不冗余：YES/NO
- [ ] 中文 + 术语：YES/NO

- [ ] **Step 4: 提交并推送**

```bash
cd C:/code/book
git checkout main
git branch --show-current  # 确认 main
git add docs/{db,rag,agent}-wiki/<分类>/<name>.md
git commit -m "docs: 深化 docs/<dir>/<name>.md 算法细节"
git push origin main
```

Expected: 1 commit 推送成功。git log --oneline -1 验证 HEAD 推进。

- [ ] **Step 5: 追加 progress.md**

在 `.superpowers/sdd/progress.md` 末尾「wiki-deepen 进度」段追加 1 行：

```markdown
- Task WD-N: complete (commit <sha7>, <N> 新增行, 5 条 self-check 全 PASS, <章名>)
```

**注意**：`progress.md` 在 `.superpowers/sdd/.gitignore` 内，仅本地保留。

---

## Phase 1：db-wiki（68 篇，拆 6 个 Batch）

### Batch 1.1：relational(7) + distributed-sql(4) = 11 篇

**Files:**
- `docs/db-wiki/relational/postgres.md`
- `docs/db-wiki/relational/mysql.md`
- `docs/db-wiki/relational/sqlite3.md`
- `docs/db-wiki/relational/openGauss.md`
- `docs/db-wiki/relational/mariadb.md`
- `docs/db-wiki/relational/firebird.md`
- `docs/db-wiki/relational/h2.md`
- `docs/db-wiki/distributed-sql/cockroachdb.md`
- `docs/db-wiki/distributed-sql/tidb.md`
- `docs/db-wiki/distributed-sql/yugabytedb.md`
- `docs/db-wiki/distributed-sql/oceanbase.md`

**重点 audit 维度**（按 spec §2）：
- relational：B+Tree 节点分裂、MVCC 可见性、查询优化器（Cascades/Volcano）、Join 算法、统计信息、WAL 编码
- distributed-sql：Raft 优化、Percolator 分布式事务、TSO 分配、Linearizable Read、Sharding 路由

- [ ] **Task WD-1: 深化 postgres.md**
- [ ] **Task WD-2: 深化 mysql.md**
- [ ] **Task WD-3: 深化 sqlite3.md**
- [ ] **Task WD-4: 深化 openGauss.md**
- [ ] **Task WD-5: 深化 mariadb.md**
- [ ] **Task WD-6: 深化 firebird.md**
- [ ] **Task WD-7: 深化 h2.md**
- [ ] **Task WD-8: 深化 cockroachdb.md**
- [ ] **Task WD-9: 深化 tidb.md**
- [ ] **Task WD-10: 深化 yugabytedb.md**
- [ ] **Task WD-11: 深化 oceanbase.md**

每 task 按 §Task 模板执行。**Batch 1 完成后做小自检**：11 篇 PASS 5 条，行数 52126 → +2500-5500。

### Batch 1.2：key-value(7) + document(5) = 12 篇

**Files:**
- `docs/db-wiki/key-value/redis.md`、`rocksdb.md`、`leveldb.md`、`foundationdb.md`、`badger.md`、`pebble.md`、`dragonfly.md`
- `docs/db-wiki/document/mongodb.md`、`couchdb.md`、`couchbase.md`、`ravendb.md`、`ferretdb.md`

**重点 audit 维度**：
- KV：LSM Compaction 策略、Block Cache 与 Bloom Filter、Prefix Scan、MVCC 事务、WAL 压缩
- document：BSON 编码、文档模型、聚合管道、索引（多键/文本/地理）、WiredTiger 引擎

- [ ] **Task WD-12: 深化 redis.md**
- [ ] **Task WD-13: 深化 rocksdb.md**
- [ ] **Task WD-14: 深化 leveldb.md**
- [ ] **Task WD-15: 深化 foundationdb.md**
- [ ] **Task WD-16: 深化 badger.md**
- [ ] **Task WD-17: 深化 pebble.md**
- [ ] **Task WD-18: 深化 dragonfly.md**
- [ ] **Task WD-19: 深化 mongodb.md**
- [ ] **Task WD-20: 深化 couchdb.md**
- [ ] **Task WD-21: 深化 couchbase.md**
- [ ] **Task WD-22: 深化 ravendb.md**
- [ ] **Task WD-23: 深化 ferretdb.md**

### Batch 1.3：columnar(5) + time-series(5) = 10 篇

**Files:**
- `docs/db-wiki/columnar/clickhouse.md`、`doris.md`、`starrocks.md`、`pinot.md`、`druid.md`
- `docs/db-wiki/time-series/influxdb.md`、`victoriametrics.md`、`questdb.md`、`tdengine.md`、`iotdb.md`

**重点 audit 维度**：
- columnar：MergeTree 家族、列式压缩（LZ4/Zstd/Dict/Delta）、向量化执行、MaterializedView、CBO
- time-series：TSM/LSM 变体、downsampling、retention policy、out-of-order writes、连续查询

- [ ] **Task WD-24: 深化 clickhouse.md**
- [ ] **Task WD-25: 深化 doris.md**
- [ ] **Task WD-26: 深化 starrocks.md**
- [ ] **Task WD-27: 深化 pinot.md**
- [ ] **Task WD-28: 深化 druid.md**
- [ ] **Task WD-29: 深化 influxdb.md**
- [ ] **Task WD-30: 深化 victoriametrics.md**
- [ ] **Task WD-31: 深化 questdb.md**
- [ ] **Task WD-32: 深化 tdengine.md**
- [ ] **Task WD-33: 深化 iotdb.md**

### Batch 1.4：search(6) + vector(9) = 15 篇

**Files:**
- `docs/db-wiki/search/elasticsearch.md`、`opensearch.md`、`meilisearch.md`、`typesense.md`、`tantivy.md`、`quickwit.md`
- `docs/db-wiki/vector/chroma.md`、`milvus.md`、`faiss.md`、`qdrant.md`、`weaviate.md`、`vespa.md`、`lancedb.md`、`vald.md`、`usearch.md`

**重点 audit 维度**（spec §2 向量算法独立大项）：
- search：倒排索引 posting list 合并、FST 编码、BM25 评分、Roaring Bitmap、Aggregations
- vector：**ANN 搜索算法**（HNSW/IVF/NSG/SSG/Vamana/ScaNN/CAGRA）；**量化**（PQ/SQ/BQ/OPQ/RQ/RaBitQ/TurboQuant）；**距离**（L2/Inner Product/Cosine/Hamming）；**聚类**（k-means/Hierarchical）；**图构建**（邻居剪枝、entry point、level generation）；**混合检索**（BM25+RRF、payload filter）；**GPU 加速**（FastScan、TurboQuant SIMD）

- [ ] **Task WD-34: 深化 elasticsearch.md**
- [ ] **Task WD-35: 深化 opensearch.md**
- [ ] **Task WD-36: 深化 meilisearch.md**
- [ ] **Task WD-37: 深化 typesense.md**
- [ ] **Task WD-38: 深化 tantivy.md**
- [ ] **Task WD-39: 深化 quickwit.md**
- [ ] **Task WD-40: 深化 chroma.md**
- [ ] **Task WD-41: 深化 milvus.md**（vector 旗舰）
- [ ] **Task WD-42: 深化 faiss.md**（vector 旗舰）
- [ ] **Task WD-43: 深化 qdrant.md**
- [ ] **Task WD-44: 深化 weaviate.md**
- [ ] **Task WD-45: 深化 vespa.md**
- [ ] **Task WD-46: 深化 lancedb.md**
- [ ] **Task WD-47: 深化 vald.md**
- [ ] **Task WD-48: 深化 usearch.md**

### Batch 1.5：graph(6) + stream(4) = 10 篇

**Files:**
- `docs/db-wiki/graph/neo4j.md`、`arangodb.md`、`janusgraph.md`、`nebula.md`、`memgraph.md`、`kuzu.md`
- `docs/db-wiki/stream/kafka.md`、`redpanda.md`、`pulsar.md`、`nats.md`

**重点 audit 维度**：
- graph：Cypher 查询优化、CSR 邻接表、图遍历（BFS/DFS/Pregel）、RDF/SPARQL、ltree
- stream：Log 结构、分区策略、ISR、Exactly-Once Semantics、Schema 演进

- [ ] **Task WD-49: 深化 neo4j.md**（graph 旗舰）
- [ ] **Task WD-50: 深化 arangodb.md**
- [ ] **Task WD-51: 深化 janusgraph.md**
- [ ] **Task WD-52: 深化 nebula.md**
- [ ] **Task WD-53: 深化 memgraph.md**
- [ ] **Task WD-54: 深化 kuzu.md**
- [ ] **Task WD-55: 深化 kafka.md**（stream 旗舰）
- [ ] **Task WD-56: 深化 redpanda.md**
- [ ] **Task WD-57: 深化 pulsar.md**
- [ ] **Task WD-58: 深化 nats.md**

### Batch 1.6：embedded(4) + extension(6) = 10 篇

**Files:**
- `docs/db-wiki/embedded/duckdb.md`、`lmdb.md`、`boltdb.md`、`pklite.md`
- `docs/db-wiki/extension/arrow.md`、`pgvector.md`、`datafusion.md`、`velox.md`、`lance.md`、`hnswlib.md`

**重点 audit 维度**：
- embedded：mmap、文件 I/O、多线程安全、崩溃恢复、MVCC
- extension：列式内存格式（Arrow IPC）、向量化执行、跨语言 FFI、UDF、SIMD 优化

- [ ] **Task WD-59: 深化 duckdb.md**（embedded 旗舰）
- [ ] **Task WD-60: 深化 lmdb.md**
- [ ] **Task WD-61: 深化 boltdb.md**
- [ ] **Task WD-62: 深化 pklite.md**
- [ ] **Task WD-63: 深化 arrow.md**（extension 旗舰）
- [ ] **Task WD-64: 深化 pgvector.md**
- [ ] **Task WD-65: 深化 datafusion.md**
- [ ] **Task WD-66: 深化 velox.md**
- [ ] **Task WD-67: 深化 lance.md**
- [ ] **Task WD-68: 深化 hnswlib.md**

---

## Phase 2：rag-wiki（12 篇，拆 2 个 Batch）

### Batch 2.1：核心 RAG 框架（11 篇）

**Files:**
- `docs/rag-wiki/langchain.md`、`llama-index.md`、`haystack-ai.md`、`txtai.md`、`embedchain.md`、`quivr.md`、`private-gpt.md`、`verba.md`、`cognita.md`、`qanything.md`、`r2r.md`

**重点 audit 维度**（spec §2 RAG/Agent 特定领域）：
- 嵌入模型适配（batching / 缓存 / 多模态）
- 文档解析器（PDF/HTML/Code/Markdown chunking 算法）
- 检索器重排（BM25 / 向量 / RRF / Cohere Rerank / Cross-encoder）
- 向量库后端（Chroma / Milvus / Weaviate / Faiss / LanceDB）
- 评估观测（RAGAS / TruLens / LangSmith / Phoenix）

- [ ] **Task WD-69: 深化 langchain.md**（RAG 旗舰）
- [ ] **Task WD-70: 深化 llama-index.md**（RAG 旗舰）
- [ ] **Task WD-71: 深化 haystack-ai.md**
- [ ] **Task WD-72: 深化 txtai.md**
- [ ] **Task WD-73: 深化 embedchain.md**
- [ ] **Task WD-74: 深化 quivr.md**
- [ ] **Task WD-75: 深化 private-gpt.md**
- [ ] **Task WD-76: 深化 verba.md**
- [ ] **Task WD-77: 深化 cognita.md**
- [ ] **Task WD-78: 深化 qanything.md**
- [ ] **Task WD-79: 深化 r2r.md**

### Batch 2.2：商业背景（1 篇）

**Files:**
- `docs/rag-wiki/commercial-rag-products.md`

- [ ] **Task WD-80: 深化 commercial-rag-products.md**

---

## Phase 3：agent-wiki（14 篇，拆 2 个 Batch）

### Batch 3.1：核心 Agent 框架（13 篇）

**Files:**
- `docs/agent-wiki/langgraph.md`、`crewai.md`、`autogen.md`、`agentscope.md`、`agentverse.md`、`agixt.md`、`openai-agents.md`、`swarm.md`、`chatdev.md`、`metagpt.md`、`open-interpreter.md`、`letta.md`、`agents.md`

**重点 audit 维度**（spec §2 Agent 特定领域）：
- 角色协作（角色定义 / 任务分配 / 协同协议）
- 记忆系统（短期 / 长期 / 向量化记忆 / MemGPT）
- 工具调用协议（Function Calling / OpenAI Tools / ReAct / Toolformer）
- 人机协作（Human-in-the-loop / Interrupt / Resume）
- 图执行引擎（StateGraph / Pregel BSP / Checkpoint）
- 状态管理（持久化 / 序列化 / 时间旅行）
- 多 Agent 通信（Pub/Sub / Direct Message / Shared State）

- [ ] **Task WD-81: 深化 langgraph.md**（Agent 旗舰）
- [ ] **Task WD-82: 深化 crewai.md**（Agent 旗舰）
- [ ] **Task WD-83: 深化 autogen.md**（Agent 旗舰）
- [ ] **Task WD-84: 深化 agentscope.md**
- [ ] **Task WD-85: 深化 agentverse.md**
- [ ] **Task WD-86: 深化 agixt.md**
- [ ] **Task WD-87: 深化 openai-agents.md**
- [ ] **Task WD-88: 深化 swarm.md**
- [ ] **Task WD-89: 深化 chatdev.md**
- [ ] **Task WD-90: 深化 metagpt.md**
- [ ] **Task WD-91: 深化 open-interpreter.md**
- [ ] **Task WD-92: 深化 letta.md**
- [ ] **Task WD-93: 深化 agents.md**

### Batch 3.2：商业背景（1 篇）

**Files:**
- `docs/agent-wiki/commercial-agent-products.md`

- [ ] **Task WD-94: 深化 commercial-agent-products.md**

---

## Phase 4：全量自检 + 进度更新（4 task）

### Task WD-95: db-wiki 全量自检（68 篇）

**Files:** 不修改文件，只验收。

```bash
cd C:/code/book
# 68 篇全部存在
ls docs/db-wiki/relational/*.md docs/db-wiki/distributed-sql/*.md \
   docs/db-wiki/key-value/*.md docs/db-wiki/document/*.md \
   docs/db-wiki/columnar/*.md docs/db-wiki/time-series/*.md \
   docs/db-wiki/search/*.md docs/db-wiki/vector/*.md \
   docs/db-wiki/graph/*.md docs/db-wiki/stream/*.md \
   docs/db-wiki/embedded/*.md docs/db-wiki/extension/*.md | wc -l
# 期望：68

# 每篇 ≥ 18 章节（深化后）
for f in $(find docs/db-wiki -name '*.md' ! -name 'README.md'); do
  cnt=$(grep -c '^## ' "$f")
  if [ "$cnt" -lt 18 ]; then
    echo "WARN: $f 只有 $cnt 章节（深化后应 ≥18）"
  fi
done

# 每篇 ≥ 5 处伪代码/公式
for f in $(find docs/db-wiki -name '*.md' ! -name 'README.md'); do
  cnt=$(grep -cE '^```$|^\\\[|^```[a-z]+$|复杂度' "$f")
  if [ "$cnt" -lt 5 ]; then
    echo "WARN: $f 只有 $cnt 伪代码/公式"
  fi
done
```

- [ ] **Step 1: 跑上述自检命令**
- [ ] **Step 2: 任何 WARN 在 progress.md 留待清单追加**
- [ ] **Step 3: 追加 progress.md 1 行**

### Task WD-96: rag-wiki 全量自检（12 篇）

按 Task WD-95 同样模式，scope 限定 `docs/rag-wiki/`。

- [ ] **Step 1-3: 同 Task WD-95**

### Task WD-97: agent-wiki 全量自检（14 篇）

按 Task WD-95 同样模式，scope 限定 `docs/agent-wiki/`。

- [ ] **Step 1-3: 同 Task WD-95**

### Task WD-98: 续写 progress.md wiki-deepen 收尾段

**Files:**
- Modify: `.superpowers/sdd/progress.md`（追加 wiki-deepen 收尾段）

- [ ] **Step 1: 续写 progress.md 末尾**

```markdown

## wiki-deepen 收尾

- 全任务完成：94/94 篇 wiki 全部深化
- commit 总览：9474e4d56..<head7>（base 9474e4d56 — wiki-deepen 计划起点；head <head7> — Phase 4 完成）
- 关键数字：
  - 94 篇 wiki 平均 +N 行（深度扩展后）
  - 总行数 52126 → 70000-80000 行
  - 5 条验收全 PASS 比例
- API 中断透明化：5 个 task 重试成功（按实际补充）
- 未修改范围：engineering/、learning/、根 CMakeLists.txt、CLAUDE.md、AGENTS.md、3 个 README 索引页
```

- [ ] **Step 2: 检查 .superpowers/sdd/.gitignore**

```bash
cd C:/code/book
cat .superpowers/sdd/.gitignore
# 期望：*（整个目录忽略）
```

- [ ] **Step 3: 仅本地保留，不 commit**

按 sdd 协作惯例，progress.md 在 .gitignore 内不提交。如不在 .gitignore 才 commit。

---

## 验收清单

- [ ] 94 篇 wiki 全部深化完成（WD-1 到 WD-94）
- [ ] 4 个 Phase 自检 task 完成（WD-95 到 WD-98）
- [ ] 总行数 52126 → 70000-80000 行
- [ ] 每篇 PASS 5 条验收（伪代码/公式 + 工程落地 + 骨架不破 + 不冗余 + 中文术语）
- [ ] 所有 commit 已 push origin main
- [ ] 无 force-push on main
- [ ] 不修改 `engineering/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`
- [ ] 不修改 3 个 wiki README 索引页
- [ ] 不修改 wiki 已写过的内容
- [ ] progress.md 含 wiki-deepen 收尾段（本地）
