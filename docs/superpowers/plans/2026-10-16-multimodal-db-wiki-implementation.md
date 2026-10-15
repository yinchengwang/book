# 多模态数据库 & AI 框架 Wiki 库 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `docs/db-wiki/`、`docs/rag-wiki/`、`docs/agent-wiki/` 下生成 94 篇深度中文 wiki（68 篇数据库 + 11 篇 RAG + 13 篇 Agent + 2 篇商业背景），并把 24 个 RAG/Agent 项目 clone 到 `reference/ai/rag/` 与 `reference/ai/agent/`。

**Architecture:** 按 15 个子任务（13 个数据库分类 + RAG + Agent + 索引/验收）分类批量串行执行；每个子任务内部完成 clone → 读源码 → 写 wiki → 提交推送的完整闭环；阶段化推进 P0→P5。

**Tech Stack:**
- 文档：Markdown（中文为主，专有名词保留英文）
- 源码：参考 `reference/<分类>/<项目>/`（已通过 git submodule 注册 73 个项目）
- Clone 工具：`git clone --depth 1`（节省磁盘与带宽）
- 元信息：`reference/catalog.yml`（增量登记 AI 项目）
- Submodule 注册：根目录 `.gitmodules`

## Global Constraints

以下约束来自 `CLAUDE.md` 与设计文档，所有 task 一律适用：

1. **语言规范**：所有对话、解释、建议必须简体中文；代码注释中文；commit message 中文。
2. **保留专业术语**：API、SDK、HNSW、Raft 等专业英文术语保留不译。
3. **提交推送纪律**：每次 commit 后立即 `git push origin main`（不允许累积）。
4. **OPSX 变更操作纪律**：回退只动当前变更相关代码；提交只提当前变更相关代码；归档只提交当前变更相关代码。
5. **根目录准入规则**：禁止在根目录新增任何 `.md` 报告文件（设计文档与 plan 已落 `docs/superpowers/`）。
6. **设计文档基线**：所有 wiki 内容必须满足 10+ 章节、1+ ASCII 架构图、5+ 源码路径引用、3+ 外部引用、含"风险与陷阱"段、含"学习路径"段。
7. **文档骨架灵活性**：基础模板 24 个维度是参考，每个项目按类型动态调整；RAG/Agent 框架裁剪不适用章节，扩展特有维度。
8. **不修改范围外代码**：禁止修改 `engineering/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`。
10. **目录创建规则**：`docs/db-wiki/` 与 `reference/ai/` 全部新建，必须先建目录再写文件。
11. **catalog 同步**：每次新增/clone 项目必须同步更新 `reference/catalog.yml`，保证 git submodule 状态与 catalog 一致。
12. **commit message 格式**：`<type>: <中文描述>`，类型包括 `docs:`、`chore:`、`feat:`。

---

## 工作流模板（每个分类任务的通用流程）

> **本节为所有 Task 3-17 的公共模板。** 每个分类任务执行时，按本模板的 7 步走；wiki 写作章节在 docs/superpowers/specs/2026-10-16-multimodal-db-wiki-design.md 第 4 节「文档骨架」中。

### 模板步骤

```bash
# 步骤 1：clone（如未 clone）
if [ ! -d "reference/<分类>/<项目>" ]; then
  git clone --depth 1 <repo-url> reference/<分类>/<项目>
fi

# 步骤 2：抓 README + docs + 顶层结构
ls -la reference/<分类>/<项目>/
cat reference/<分类>/<项目>/README.md | head -200
find reference/<分类>/<项目>/docs -maxdepth 2 -type f 2>/dev/null | head -50

# 步骤 3：抓源码骨架（top-level 模块、核心数据结构）
ls reference/<分类>/<项目>/src 2>/dev/null || ls reference/<分类>/<项目>/<main-dir> 2>/dev/null
# 项目类型特定入口：
#   - 数据库：storage/ engine/ index/ executor/ repl/
#   - RAG：chains/ agents/ retrievers/ document_loaders/ embeddings/
#   - Agent：agents/ tools/ memory/

# 步骤 4：写 wiki（按设计文档第 4 节骨架）
# 落盘路径：docs/db-wiki/<分类>/<项目>.md  或  docs/rag-wiki/<项目>.md  或  docs/agent-wiki/<项目>.md
# 必须满足 10+ 章节、1+ ASCII 架构图、5+ 源码路径引用、3+ 外部引用

# 步骤 5：增量更新索引页（如本任务完成后生成/更新索引）
# 索引页：docs/db-wiki/README.md / docs/rag-wiki/README.md / docs/agent-wiki/README.md

# 步骤 6：提交（按 CLAUDE.md 每次立即 push）
git add docs/<wiki>/<分类>/<项目>.md reference/<分类>/<项目>/ reference/catalog.yml
git commit -m "docs: 新增<分类>/<项目> wiki"
git push origin main

# 步骤 7：循环下一个项目（同一分类内）
```

### wiki 章节选择规则

| 项目类型 | 必须覆盖维度 | 裁剪维度 | 扩展维度 |
|----------|--------------|----------|----------|
| 关系型数据库（如 Postgres、MySQL） | 1-24 全 | 无 | 无（可加深事务/复制章节） |
| 分布式 SQL（如 TiDB、CockroachDB） | 1-24 全 | 无 | 加深 11（多区域容灾）、12（分片） |
| KV（如 Redis、RocksDB） | 1, 6-10, 13-17, 19-22 | 10 简化（Redis 无完整事务） | 加深 7（数据结构：SDS/跳表/LSM） |
| 文档（如 MongoDB、FerretDB） | 1-9, 11-22 | 10（多数文档库无强 ACID） | 扩展「文档模型」「聚合管道」 |
| 列式（如 ClickHouse、Doris） | 1-7, 9-22 | 8（倒排/向量化索引无典型列存） | 加深 7（列存压缩）、12（MPP） |
| 时序（如 InfluxDB、TDengine） | 1-9, 11-22 | 10（多数时序库简化） | 扩展「降采样」「保留策略」 |
| 搜索（如 Elasticsearch、Meilisearch） | 1-9, 11-22 | 10 | 加深 8（倒排）、9（DSL） |
| 向量（如 Milvus、Qdrant） | 1-9, 11-22 | 10（部分无） | 扩展「向量索引」「量化」「混合检索」 |
| 图（如 Neo4j、Kuzu） | 1-9, 11-22 | 10 | 加深 7（图存储 + 图算法） |
| 流（如 Kafka、Pulsar） | 1-9, 11-22 | 10（部分无） | 加深 7（Log + 分区） |
| 嵌入式（如 DuckDB、SQLite） | 1-9, 11-22 | 10, 11（无复制） | 加深 7（mmap / 文件 I/O）、20（嵌入式） |
| 扩展（如 pgvector、Arrow） | 1-22 | 按类型裁剪 | 「与主项目集成」 |
| RAG（如 LangChain、Llama-Index） | 1-5, 7, 9, 13-22 | 8, 10, 11, 12（多数无） | 4 个扩展维度（嵌入/解析/检索/后端） |
| Agent（如 LangGraph、AutoGen） | 1-5, 7, 9, 13-22 | 8, 10, 11, 12（多数无） | 4 个扩展维度（角色/记忆/工具/人机） |

---

## Task 1: 初始化文档骨架与目录结构

**Files:**
- Create: `docs/db-wiki/README.md`（占位）
- Create: `docs/db-wiki/relational/.gitkeep` 等 12 个分类子目录占位
- Create: `docs/rag-wiki/README.md`（占位）
- Create: `docs/agent-wiki/README.md`（占位）
- Modify: `reference/README.md`（增加 ai/rag/ 与 ai/agent/ 条目说明）

**Interfaces:**
- Consumes: docs/superpowers/specs/2026-10-16-multimodal-db-wiki-design.md 的文档位置与命名规范
- Produces: 后续所有 Task 落盘的目录结构、索引页初始模板

- [ ] **Step 1: 建 docs/db-wiki 子目录**

```bash
mkdir -p docs/db-wiki/{relational,distributed-sql,key-value,document,columnar,time-series,search,vector,graph,stream,embedded,extension}
ls docs/db-wiki/
```

期望：12 个分类子目录创建成功。

- [ ] **Step 2: 建 docs/rag-wiki 与 docs/agent-wiki**

```bash
mkdir -p docs/rag-wiki docs/agent-wiki
ls docs/
```

期望：docs/ 下多出 rag-wiki 与 agent-wiki 目录。

- [ ] **Step 3: 创建 docs/db-wiki/README.md（占位）**

内容：

```markdown
# 多模态数据库 Wiki 库

> 本目录收录 `reference/` 下 13 个数据模型分类中 68 个开源数据库项目的深度中文介绍。

## 分类索引

| 分类 | 项目数 | 项目清单 |
|------|--------|----------|
| [relational](./relational/) | 7 | postgres、mysql、sqlite3、openGauss、mariadb、firebird、h2 |
| [distributed-sql](./distributed-sql/) | 4 | cockroachdb、tidb、yugabytedb、oceanbase |
| [key-value](./key-value/) | 7 | redis、rocksdb、leveldb、foundationdb、badger、pebble、dragonfly |
| [document](./document/) | 5 | mongodb、couchdb、couchbase、ravendb、ferretdb |
| [columnar](./columnar/) | 5 | clickhouse、doris、starrocks、pinot、druid |
| [time-series](./time-series/) | 5 | influxdb、victoriametrics、questdb、tdengine、iotdb |
| [search](./search/) | 6 | elasticsearch、opensearch、meilisearch、typesense、tantivy、quickwit |
| [vector](./vector/) | 9 | chroma、milvus、faiss、qdrant、weaviate、vespa、lancedb、vald、usearch |
| [graph](./graph/) | 6 | neo4j、arangodb、janusgraph、nebula、memgraph、kuzu |
| [stream](./stream/) | 4 | kafka、redpanda、pulsar、nats |
| [embedded](./embedded/) | 4 | duckdb、lmdb、boltdb、pklite |
| [extension](./extension/) | 6 | arrow、pgvector、datafusion、velox、lance、hnswlib |

## 写作规范

按 `docs/superpowers/specs/2026-10-16-multimodal-db-wiki-design.md` 第 4 节文档骨架执行。每个 wiki 必须满足：

- 至少 10 个章节（深度扩展版基线）
- 至少 1 张 ASCII 架构图或关键流程图
- 至少 5 处源码路径引用
- 至少 3 个外部引用（论文/官方文档/benchmark）
- 中文为主，专有名词保留英文
```

- [ ] **Step 4: 创建 docs/rag-wiki/README.md（占位）**

```markdown
# RAG 框架 Wiki 库

> 本目录收录业界主流的 11 个开源 RAG 框架深度中文介绍，以及 1 篇商业产品背景。

## 项目清单

| 项目 | 定位 | 链接 |
|------|------|------|
| langchain | LLM 应用编排框架 | [查看](./langchain.md) |
| llama-index | RAG 数据框架 | [查看](./llama-index.md) |
| haystack-ai | NLP 管道框架 | [查看](./haystack-ai.md) |
| txtai | 语义搜索与工作流 | [查看](./txtai.md) |
| embedchain | RAG 应用框架 | [查看](./embedchain.md) |
| verba | Weaviate 出品的 RAG UI | [查看](./verba.md) |
| cognita | TrueFoundry 出品的 RAG | [查看](./cognita.md) |
| r2r | SciPhi 出品的 RAG 框架 | [查看](./r2r.md) |
| quivr | 个人知识库 RAG | [查看](./quivr.md) |
| private-gpt | 本地私有 GPT | [查看](./private-gpt.md) |
| qanything | 网易出品的 RAG | [查看](./qanything.md) |
| 商业产品背景 | Notion AI、ChatGPT Enterprise 等 | [查看](./commercial-rag-products.md) |

## 写作规范

RAG 框架 wiki 必须覆盖以下扩展维度：嵌入模型适配、文档解析器、检索器/重排器、向量库后端。详见设计文档 4.3 节。
```

- [ ] **Step 5: 创建 docs/agent-wiki/README.md（占位）**

```markdown
# Agent 框架 Wiki 库

> 本目录收录业界主流的 13 个开源 Agent 框架深度中文介绍，以及 1 篇商业产品背景。

## 项目清单

| 项目 | 定位 | 链接 |
|------|------|------|
| langgraph | LangChain 出品的图编排 Agent | [查看](./langgraph.md) |
| crewai | 多 Agent 协作框架 | [查看](./crewai.md) |
| autogen | Microsoft 出品的多 Agent | [查看](./autogen.md) |
| metagpt | 多 Agent 软件公司 | [查看](./metagpt.md) |
| openai-agents | OpenAI 官方 Agent SDK | [查看](./openai-agents.md) |
| chatdev | 软件开发多 Agent | [查看](./chatdev.md) |
| agentverse | 研究型多 Agent 平台 | [查看](./agentverse.md) |
| agixt | 自动化 Agent 框架 | [查看](./agixt.md) |
| open-interpreter | 代码解释器 Agent | [查看](./open-interpreter.md) |
| agentscope | 阿里出品的多 Agent | [查看](./agentscope.md) |
| letta | 长期记忆 Agent | [查看](./letta.md) |
| swarm | OpenAI 教学版多 Agent | [查看](./swarm.md) |
| agents | Microsoft 新版 Agent | [查看](./agents.md) |
| 商业产品背景 | Devin、Cursor、Manus 等 | [查看](./commercial-agent-products.md) |

## 写作规范

Agent 框架 wiki 必须覆盖以下扩展维度：角色协作模式、记忆系统、工具调用协议、人机协作。详见设计文档 4.3 节。
```

- [ ] **Step 6: 更新 reference/README.md 增加 ai 条目**

在 `reference/README.md` 的目录结构说明中追加：

```markdown
- `ai/rag/`：开源 RAG 框架源码（用于 AI 应用层学习）。
- `ai/agent/`：开源 Agent 框架源码（用于 AI 应用层学习）。
```

- [ ] **Step 7: 提交并推送**

```bash
git add docs/db-wiki docs/rag-wiki docs/agent-wiki reference/README.md
git commit -m "docs: 初始化多模态 wiki 文档骨架与索引页"
git push origin main
```

期望：commit 成功并推送至 origin/main。

---

## Task 2: P0 — Clone 24 个 RAG/Agent 项目

**Files:**
- Create: `reference/ai/rag/{langchain,llama-index,haystack-ai,txtai,embedchain,verba,cognita,r2r,quivr,private-gpt,qanything}/`
- Create: `reference/ai/agent/{langgraph,crewai,autogen,metagpt,openai-agents,chatdev,agentverse,agixt,open-interpreter,agentscope,letta,swarm,agents}/`
- Modify: `reference/catalog.yml`（增量登记 24 项）
- Create: `reference/ai/README.md`（AI 子目录总览）
- Create: `reference/ai/rag/README.md`（RAG 克隆清单）
- Create: `reference/ai/agent/README.md`（Agent 克隆清单）

**Interfaces:**
- Consumes: Task 1 已建 `reference/ai/` 目录约定（虽然目录实际在 Task 2创建，与 Task 1 同 commit 也可；这里独立 Task 是为了 stage 清晰）
- Produces: 24 个本地 git 仓库；catalog.yml 新增 24 项；3 个 README 说明

- [ ] **Step 1: 创建 reference/ai 目录结构**

```bash
mkdir -p reference/ai/rag reference/ai/agent
```

- [ ] **Step 2: Clone 11 个 RAG 项目（并发或分批）**

```bash
# 主流 3 个
git clone --depth 1 https://github.com/langchain-ai/langchain.git reference/ai/rag/langchain &
git clone --depth 1 https://github.com/run-llama/llama_index.git reference/ai/rag/llama-index &
git clone --depth 1 https://github.com/deepset-ai/haystack.git reference/ai/rag/haystack-ai &
wait

# 轻量级 3 个
git clone --depth 1 https://github.com/neuml/txtai.git reference/ai/rag/txtai &
git clone --depth 1 https://github.com/embedchain/embedchain.git reference/ai/rag/embedchain &
git clone --depth 1 https://github.com/zylon-ai/private-gpt.git reference/ai/rag/private-gpt &
wait

# 平台型 3 个
git clone --depth 1 https://github.com/weaviate/verba.git reference/ai/rag/verba &
git clone --depth 1 https://github.com/truefoundry/cognita.git reference/ai/rag/cognita &
git clone --depth 1 https://github.com/QuivrHQ/quivr.git reference/ai/rag/quivr &
wait

# 新锐 2 个
git clone --depth 1 https://github.com/SciPhi-AI/R2R.git reference/ai/rag/r2r &
git clone --depth 1 https://github.com/netease-youdao/QAnything.git reference/ai/rag/qanything &
wait

ls reference/ai/rag/
```

期望：11 个目录全部创建成功。

- [ ] **Step 3: Clone 13 个 Agent 项目（并发或分批）**

```bash
# 主流 4 个
git clone --depth 1 https://github.com/langchain-ai/langgraph.git reference/ai/agent/langgraph &
git clone --depth 1 https://github.com/crewAIInc/crewAI.git reference/ai/agent/crewai &
git clone --depth 1 https://github.com/microsoft/autogen.git reference/ai/agent/autogen &
git clone --depth 1 https://github.com/geekan/MetaGPT.git reference/ai/agent/metagpt &
wait

# 官方/SDK 3 个
git clone --depth 1 https://github.com/openai/openai-agents-python.git reference/ai/agent/openai-agents &
git clone --depth 1 https://github.com/openai/swarm.git reference/ai/agent/swarm &
git clone --depth 1 https://github.com/microsoft/agents.git reference/ai/agent/agents &
wait

# 教学/研究 2 个
git clone --depth 1 https://github.com/OpenBMB/ChatDev.git reference/ai/agent/chatdev &
git clone --depth 1 https://github.com/OpenBMB/AgentVerse.git reference/ai/agent/agentverse &
wait

# 工具型 2 个
git clone --depth 1 https://github.com/AGiXT/AGiXT.git reference/ai/agent/agixt &
git clone --depth 1 https://github.com/OpenInterpreter/open-interpreter.git reference/ai/agent/open-interpreter &
wait

# 国产 2 个
git clone --depth 1 https://github.com/modelscope/agentscope.git reference/ai/agent/agentscope &
git clone --depth 1 https://github.com/letta-ai/letta.git reference/ai/agent/letta &
wait

ls reference/ai/agent/
```

期望：13 个目录全部创建成功。

- [ ] **Step 4: 更新 reference/catalog.yml 增量登记 24 项**

在 `reference/catalog.yml` 末尾 `projects:` 数组中追加 24 个 AI 项目（model: `ai/rag` 或 `ai/agent`，tier 按主流/代表/创新划分）。模板：

```yaml
  - name: langchain
    model: ai/rag
    tier: core
    repository: https://github.com/langchain-ai/langchain.git
    mirror: https://github.com/langchain-ai/langchain.git
    license: MIT
    status: active
    notes:
      - LLM 应用编排框架主流实现
```

（其余 23 个按相同模板填写）

- [ ] **Step 5: 创建 reference/ai/README.md**

```markdown
# reference/ai/ 目录说明

本目录收录开源 RAG 与 Agent 框架源码，作为 AI 应用层学习参考。不参与工程构建。

## 子目录

- `rag/`：11 个开源 RAG 框架（详见 rag/README.md）。
- `agent/`：13 个开源 Agent 框架（详见 agent/README.md）。
```

- [ ] **Step 6: 创建 reference/ai/rag/README.md**

```markdown
# RAG 框架克隆清单

| 项目 | GitHub | 本地路径 | 备注 |
|------|--------|----------|------|
| langchain | langchain-ai/langchain | rag/langchain/ | LLM 应用编排主流 |
| llama-index | run-llama/llama_index | rag/llama-index/ | RAG 数据框架 |
| haystack-ai | deepset-ai/haystack | rag/haystack-ai/ | NLP 管道框架 |
| txtai | neuml/txtai | rag/txtai/ | 语义搜索 |
| embedchain | embedchain/embedchain | rag/embedchain/ | 快速构建 RAG |
| verba | weaviate/verba | rag/verba/ | Weaviate 出品 |
| cognita | truefoundry/cognita | rag/cognita/ | TrueFoundry 出品 |
| r2r | SciPhi-AI/R2R | rag/r2r/ | SciPhi 出品 |
| quivr | QuivrHQ/quivr | rag/quivr/ | 个人知识库 |
| private-gpt | zylon-ai/private-gpt | rag/private-gpt/ | 本地私有 GPT |
| qanything | netease-youdao/QAnything | rag/qanything/ | 网易出品 |

## 更新方式

```bash
cd reference/ai/rag/<project>
git fetch --depth 1 origin
git reset --hard origin/main
```
```

- [ ] **Step 7: 创建 reference/ai/agent/README.md**

```markdown
# Agent 框架克隆清单

| 项目 | GitHub | 本地路径 | 备注 |
|------|--------|----------|------|
| langgraph | langchain-ai/langgraph | agent/langgraph/ | 图编排 Agent |
| crewai | crewAIInc/crewAI | agent/crewai/ | 多 Agent 协作 |
| autogen | microsoft/autogen | agent/autogen/ | Microsoft 出品 |
| metagpt | geekan/MetaGPT | agent/metagpt/ | 软件公司多 Agent |
| openai-agents | openai/openai-agents-python | agent/openai-agents/ | OpenAI 官方 SDK |
| swarm | openai/swarm | agent/swarm/ | OpenAI 教学版 |
| agents | microsoft/agents | agent/agents/ | Microsoft 新版 |
| chatdev | OpenBMB/ChatDev | agent/chatdev/ | 教学型 |
| agentverse | OpenBMB/AgentVerse | agent/agentverse/ | 研究型 |
| agixt | AGiXT/AGiXT | agent/agixt/ | 自动化框架 |
| open-interpreter | OpenInterpreter/open-interpreter | agent/open-interpreter/ | 代码解释器 |
| agentscope | modelscope/agentscope | agent/agentscope/ | 阿里出品 |
| letta | letta-ai/letta | agent/letta/ | 长期记忆 |

## 更新方式

```bash
cd reference/ai/agent/<project>
git fetch --depth 1 origin
git reset --hard origin/main
```
```

- [ ] **Step 8: 提交并推送**

```bash
git add reference/ai/ reference/catalog.yml
git commit -m "chore: clone 24 个 RAG/Agent 开源项目并登记 catalog"
git push origin main
```

期望：commit 成功并推送至 origin/main。如果网络失败导致部分仓库未 clone，按"工作流模板"中的"失败项目降级"原则处理。

---

## Task 3: P1.1 — 写 7 篇 relational wiki

**Files:**
- Create: `docs/db-wiki/relational/{postgres,mysql,sqlite3,openGauss,mariadb,firebird,h2}.md`

**Interfaces:**
- Consumes: Task 1 已建目录；`reference/relational/<项目>/` 中的源码
- Produces: 7 篇 wiki（每篇 10+ 章节、1+ 架构图、5+ 源码路径、3+ 外部引用）

参考"工作流模板"7 步走，每个项目（postgres、mysql、sqlite3、openGauss、mariadb、firebird、h2）循环一次：

- [ ] **Step 1: postgres.md（首篇，作为模板参考）**

按"工作流模板"读 `reference/relational/postgres/`：

- README.md / INSTALL / 顶层目录结构
- `src/backend/access/heap/`（堆表访问方法）
- `src/backend/storage/buffer/`（Buffer Pool）
- `src/backend/executor/`（执行器）
- `src/backend/replication/`（流复制）
- 论文：PostgreSQL 历史论文、MVC、MVCC 论文

写 `docs/db-wiki/relational/postgres.md`，覆盖 24 个维度（关系型数据库类型 = 全维度覆盖），包含至少 5 处 `src/backend/.../*.c:行号` 形式的源码路径引用、1+ ASCII 架构图、3+ 论文/官方文档 URL。

```bash
git add docs/db-wiki/relational/postgres.md
git commit -m "docs: 新增 relational/postgres wiki"
git push origin main
```

- [ ] **Step 2: mysql.md**

按"工作流模板"读 `reference/relational/mysql/`，重点 `sql/`、`storage/innobase/`、`mysys/`。写 `docs/db-wiki/relational/mysql.md`，提交推送。

- [ ] **Step 3: sqlite3.md**

按"工作流模板"读 `reference/relational/sqlite3/`，重点 `src/btree.c`、`src/vdbe.c`、`src/parse.c`。写 `docs/db-wiki/relational/sqlite3.md`，提交推送。

- [ ] **Step 4: openGauss.md**

按"工作流模板"读 `reference/relational/openGauss/`，重点 openGauss 是 PostgreSQL 衍生。写 `docs/db-wiki/relational/openGauss.md`，提交推送。

- [ ] **Step 5: mariadb.md**

按"工作流模板"读 `reference/relational/mariadb/`，写 `docs/db-wiki/relational/mariadb.md`，提交推送。

- [ ] **Step 6: firebird.md**

按"工作流模板"读 `reference/relational/firebird/`，写 `docs/db-wiki/relational/firebird.md`，提交推送。

- [ ] **Step 7: h2.md**

按"工作流模板"读 `reference/relational/h2/`，写 `docs/db-wiki/relational/h2.md`，提交推送。

- [ ] **Step 8: 自检 7 篇 wiki 全部满足设计文档验收标准**

```bash
for f in docs/db-wiki/relational/*.md; do
  echo "=== $f ==="
  echo "章节数（## 数）：$(grep -c '^## ' $f)"
  echo "源码引用数：$(grep -cE 'src/|sql/|\.c:|\.h:' $f)"
done
```

期望：每个文件 ≥10 章节、≥5 源码引用。

---

## Task 4: P1.2 — 写 4 篇 distributed-sql wiki

**Files:**
- Create: `docs/db-wiki/distributed-sql/{cockroachdb,tidb,yugabytedb,oceanbase}.md`

**Interfaces:**
- Consumes: `reference/distributed-sql/<项目>/` 中的源码
- Produces: 4 篇 wiki（分布式 SQL 重点加深 11 多区域容灾、12 分片维度）

按"工作流模板"7 步走，每个项目循环一次：

- [ ] **Step 1: cockroachdb.md**

读 `reference/distributed-sql/cockroachdb/`，重点 `pkg/sql/`、`pkg/storage/`、`pkg/kv/`、`pkg/raft/`。写 `docs/db-wiki/distributed-sql/cockroachdb.md`，提交推送。

- [ ] **Step 2: tidb.md**

读 `reference/distributed-sql/tidb/`，重点 `pkg/sql/`、`pkg/store/`、`pkg/raftstore/`。写 `docs/db-wiki/distributed-sql/tidb.md`，提交推送。

- [ ] **Step 3: yugabytedb.md**

读 `reference/distributed-sql/yugabytedb/`，重点 `src/yb/`。写 `docs/db-wiki/distributed-sql/yugabytedb.md`，提交推送。

- [ ] **Step 4: oceanbase.md**

读 `reference/distributed-sql/oceanbase/`，重点 `src/share/`、`src/storage/`、`src/sql/`。写 `docs/db-wiki/distributed-sql/oceanbase.md`，提交推送。

- [ ] **Step 5: 自检 4 篇 wiki 满足验收标准**

```bash
for f in docs/db-wiki/distributed-sql/*.md; do
  echo "=== $f ==="
  echo "章节数：$(grep -c '^## ' $f)"
done
```

---

## Task 5: P1.3 — 写 7 篇 key-value wiki

**Files:**
- Create: `docs/db-wiki/key-value/{redis,rocksdb,leveldb,foundationdb,badger,pebble,dragonfly}.md`

**Interfaces:**
- Consumes: `reference/key-value/<项目>/` 中的源码
- Produces: 7 篇 wiki（KV 类型重点加深 7 数据结构章节：SDS/跳表/LSM；可简化 10 事务章节）

按"工作流模板"7 步走，每个项目循环一次：

- [ ] **Step 1: redis.md**

读 `reference/key-value/redis/`，重点 `src/server.c`、`src/db.c`、`src/dict.c`、`src/sds.c`、`src/t_zset.c`（跳表）。写 `docs/db-wiki/key-value/redis.md`，提交推送。

- [ ] **Step 2: rocksdb.md**

读 `reference/key-value/rocksdb/`，重点 `db/`、`table/`、`memtable/`。写 `docs/db-wiki/key-value/rocksdb.md`，提交推送。

- [ ] **Step 3: leveldb.md**

读 `reference/key-value/leveldb/`，重点 `db/`、`table/`、`util/`。写 `docs/db-wiki/key-value/leveldb.md`，提交推送。

- [ ] **Step 4: foundationdb.md**

读 `reference/key-value/foundationdb/`，重点 `fdbserver/`、`fdbclient/`、`flow/`。写 `docs/db-wiki/key-value/foundationdb.md`，提交推送。

- [ ] **Step 5: badger.md**

读 `reference/key-value/badger/`（Go），重点 `db.go`、`table/`、`levels.go`。写 `docs/db-wiki/key-value/badger.md`，提交推送。

- [ ] **Step 6: pebble.md**

读 `reference/key-value/pebble/`（Go），重点 `db.go`、`sstable/`、`batch.go`。写 `docs/db-wiki/key-value/pebble.md`，提交推送。

- [ ] **Step 7: dragonfly.md**

读 `reference/key-value/dragonfly/`，重点 `src/server/`、`src/facade/`。写 `docs/db-wiki/key-value/dragonfly.md`，提交推送。

- [ ] **Step 8: 自检 7 篇 wiki 满足验收标准**

---

## Task 6: P2.1 — 写 5 篇 document wiki

**Files:**
- Create: `docs/db-wiki/document/{mongodb,couchdb,couchbase,ravendb,ferretdb}.md`

**Interfaces:**
- Consumes: `reference/document/<项目>/` 中的源码
- Produces: 5 篇 wiki（文档类型裁剪 10 事务，扩展「文档模型」「聚合管道」维度）

- [ ] **Step 1: mongodb.md**

读 `reference/document/mongodb/`，重点 `src/mongo/db/`、`src/mongo/db/exec/`。写 `docs/db-wiki/document/mongodb.md`，提交推送。

- [ ] **Step 2: couchdb.md**

读 `reference/document/couchdb/`，重点 `src/couchdb/`、`src/couch/`，扩展「B-tree 存储引擎」「MVCC」。写 `docs/db-wiki/document/couchdb.md`，提交推送。

- [ ] **Step 3: couchbase.md**

读 `reference/document/couchbase/`，重点 C++ 嵌入式引擎 LiteCore。写 `docs/db-wiki/document/couchbase.md`，提交推送。

- [ ] **Step 4: ravendb.md**

读 `reference/document/ravendb/`（.NET），重点 `src/Raven.Server/`。写 `docs/db-wiki/document/ravendb.md`，提交推送。

- [ ] **Step 5: ferretdb.md**

读 `reference/document/ferretdb/`（Go），重点 `internal/`、`cmd/ferretdb/`。扩展「PostgreSQL 兼容代理」。写 `docs/db-wiki/document/ferretdb.md`，提交推送。

- [ ] **Step 6: 自检 5 篇 wiki 满足验收标准**

---

## Task 7: P2.2 — 写 5 篇 columnar wiki

**Files:**
- Create: `docs/db-wiki/columnar/{clickhouse,doris,starrocks,pinot,druid}.md`

**Interfaces:**
- Consumes: `reference/columnar/<项目>/` 中的源码
- Produces: 5 篇 wiki（列式类型加深 7 列存压缩、12 MPP 章节）

- [ ] **Step 1: clickhouse.md**

读 `reference/columnar/clickhouse/`，重点 `src/Storages/MergeTree/`、`src/Interpreters/`。写 `docs/db-wiki/columnar/clickhouse.md`，提交推送。

- [ ] **Step 2: doris.md**

读 `reference/columnar/doris/`，重点 `fe/`、`be/src/`。写 `docs/db-wiki/columnar/doris.md`，提交推送。

- [ ] **Step 3: starrocks.md**

读 `reference/columnar/starrocks/`，重点 `fe/`、`be/src/`。写 `docs/db-wiki/columnar/starrocks.md`，提交推送。

- [ ] **Step 4: pinot.md**

读 `reference/columnar/pinot/`，重点 `pinot-controller/`、`pinot-server/`、`pinot-common/`。写 `docs/db-wiki/columnar/pinot.md`，提交推送。

- [ ] **Step 5: druid.md**

读 `reference/columnar/druid/`，重点 `server/src/main/java/`。写 `docs/db-wiki/columnar/druid.md`，提交推送。

- [ ] **Step 6: 自检 5 篇 wiki 满足验收标准**

---

## Task 8: P2.3 — 写 5 篇 time-series wiki

**Files:**
- Create: `docs/db-wiki/time-series/{influxdb,victoriametrics,questdb,tdengine,iotdb}.md`

**Interfaces:**
- Consumes: `reference/time-series/<项目>/` 中的源码
- Produces: 5 篇 wiki（时序类型扩展「降采样」「保留策略」「TSM/TSI 索引」）

- [ ] **Step 1: influxdb.md**

读 `reference/time-series/influxdb/`，重点 `influxdb2/`、`storage/`、`tsdb/`。扩展 TSM 存储引擎章节。写 `docs/db-wiki/time-series/influxdb.md`，提交推送。

- [ ] **Step 2: victoriametrics.md**

读 `reference/time-series/victoriametrics/`，重点 `app/vmstorage/`、`app/vmselect/`。写 `docs/db-wiki/time-series/victoriametrics.md`，提交推送。

- [ ] **Step 3: questdb.md**

读 `reference/time-series/questdb/`，重点 `core/src/main/java/`。写 `docs/db-wiki/time-series/questdb.md`，提交推送。

- [ ] **Step 4: tdengine.md**

读 `reference/time-series/tdengine/`，重点 `source/libs/`、`source/client/`、`source/dnode/`。扩展 TSDB 双层架构。写 `docs/db-wiki/time-series/tdengine.md`，提交推送。

- [ ] **Step 5: iotdb.md**

读 `reference/time-series/iotdb/`，重点 `iotdb-core/`、`tsfile/`。写 `docs/db-wiki/time-series/iotdb.md`，提交推送。

- [ ] **Step 6: 自检 5 篇 wiki 满足验收标准**

---

## Task 9: P2.4 — 写 6 篇 search wiki

**Files:**
- Create: `docs/db-wiki/search/{elasticsearch,opensearch,meilisearch,typesense,tantivy,quickwit}.md`

**Interfaces:**
- Consumes: `reference/search/<项目>/` 中的源码
- Produces: 6 篇 wiki（搜索类型加深 8 倒排索引、9 DSL 章节）

- [ ] **Step 1: elasticsearch.md**

读 `reference/search/elasticsearch/`，重点 `server/src/main/java/`、`x-pack/`。写 `docs/db-wiki/search/elasticsearch.md`，提交推送。

- [ ] **Step 2: opensearch.md**

读 `reference/search/opensearch/`，重点 `server/src/main/java/`。写 `docs/db-wiki/search/opensearch.md`，提交推送。

- [ ] **Step 3: meilisearch.md**

读 `reference/search/meilisearch/`（Rust），重点 `crates/`、`meilisearch-lib/`。写 `docs/db-wiki/search/meilisearch.md`，提交推送。

- [ ] **Step 4: typesense.md**

读 `reference/search/typesense/`（C++），重点 `src/`。写 `docs/db-wiki/search/typesense.md`，提交推送。

- [ ] **Step 5: tantivy.md**

读 `reference/search/tantivy/`（Rust），重点 `src/`、`columnar/`。写 `docs/db-wiki/search/tantivy.md`，提交推送。

- [ ] **Step 6: quickwit.md**

读 `reference/search/quickwit/`（Rust），重点 `quickwit-search/`、`quickwit-indexing/`。写 `docs/db-wiki/search/quickwit.md`，提交推送。

- [ ] **Step 7: 自检 6 篇 wiki 满足验收标准**

---

## Task 10: P3.1 — 写 9 篇 vector wiki

**Files:**
- Create: `docs/db-wiki/vector/{chroma,milvus,faiss,qdrant,weaviate,vespa,lancedb,vald,usearch}.md`

**Interfaces:**
- Consumes: `reference/vector/<项目>/` 中的源码
- Produces: 9 篇 wiki（向量类型扩展「向量索引」「量化」「混合检索」章节）

- [ ] **Step 1: chroma.md**

读 `reference/vector/chroma/`，重点 `chromadb/`、`rust/`。扩展「HNSW」「量化」。写 `docs/db-wiki/vector/chroma.md`，提交推送。

- [ ] **Step 2: milvus.md**

读 `reference/vector/milvus/`（Go + C++），重点 `internal/core/`、`internal/util/`。扩展「IVF」「HNSW」「DiskANN」「量化」。写 `docs/db-wiki/vector/milvus.md`，提交推送。

- [ ] **Step 3: faiss.md**

读 `reference/vector/faiss/`，重点 `faiss/`、`faiss/indexes/`。扩展「向量索引算法全景」「GPU 加速」。写 `docs/db-wiki/vector/faiss.md`，提交推送。

- [ ] **Step 4: qdrant.md**

读 `reference/vector/qdrant/`（Rust），重点 `lib/`、`src/`。扩展「HNSW + Payload 过滤」。写 `docs/db-wiki/vector/qdrant.md`，提交推送。

- [ ] **Step 5: weaviate.md**

读 `reference/vector/weaviate/`（Go），重点 `adapters/`、`usecases/`。扩展「倒排 + 向量混合」。写 `docs/db-wiki/vector/weaviate.md`，提交推送。

- [ ] **Step 6: vespa.md**

读 `reference/vector/vespa/`（Java + C++），重点 `vespaclient-core/`、`searchcore/`。扩展「文档式向量平台」。写 `docs/db-wiki/vector/vespa.md`，提交推送。

- [ ] **Step 7: lancedb.md**

读 `reference/vector/lancedb/`（Rust + Python），重点 `rust/`、`python/`。扩展「Lance 列式格式」。写 `docs/db-wiki/vector/lancedb.md`，提交推送。

- [ ] **Step 8: vald.md**

读 `reference/vector/vald/`（Go），重点 `internal/`、`cmd/`。扩展「分布式向量检索」。写 `docs/db-wiki/vector/vald.md`，提交推送。

- [ ] **Step 9: usearch.md**

读 `reference/vector/usearch/`（C++），重点 `include/`、`src/`。扩展「单文件向量库」。写 `docs/db-wiki/vector/usearch.md`，提交推送。

- [ ] **Step 10: 自检 9 篇 wiki 满足验收标准**

---

## Task 11: P3.2 — 写 6 篇 graph wiki

**Files:**
- Create: `docs/db-wiki/graph/{neo4j,arangodb,janusgraph,nebula,memgraph,kuzu}.md`

**Interfaces:**
- Consumes: `reference/graph/<项目>/` 中的源码
- Produces: 6 篇 wiki（图类型加深 7 图存储 + 图算法）

- [ ] **Step 1: neo4j.md**

读 `reference/graph/neo4j/`（Java + Scala），重点 `community/kernel/`、`community/cypher/`。写 `docs/db-wiki/graph/neo4j.md`，提交推送。

- [ ] **Step 2: arangodb.md**

读 `reference/arangodb/`（C++），重点 `arangod/`、`arangosh/`。扩展「多模型（文档+图+KV）」。写 `docs/db-wiki/graph/arangodb.md`，提交推送。

- [ ] **Step 3: janusgraph.md**

读 `reference/janusgraph/`（Java），重点 `janusgraph-core/`。写 `docs/db-wiki/graph/janusgraph.md`，提交推送。

- [ ] **Step 4: nebula.md**

读 `reference/nebula/`（C++/Go），重点 `src/storage/`、`src/meta/`、`src/graph/`。写 `docs/db-wiki/graph/nebula.md`，提交推送。

- [ ] **Step 5: memgraph.md**

读 `reference/memgraph/`（C++），重点 `src/`、`storage/`。写 `docs/db-wiki/graph/memgraph.md`，提交推送。

- [ ] **Step 6: kuzu.md**

读 `reference/kuzu/`（C++），重点 `src/main/`、`src/storage/`。扩展「嵌入式图数据库」。写 `docs/db-wiki/graph/kuzu.md`，提交推送。

- [ ] **Step 7: 自检 6 篇 wiki 满足验收标准**

---

## Task 12: P3.3 — 写 4 篇 stream wiki

**Files:**
- Create: `docs/db-wiki/stream/{kafka,redpanda,pulsar,nats}.md`

**Interfaces:**
- Consumes: `reference/stream/<项目>/` 中的源码
- Produces: 4 篇 wiki（流类型加深 7 Log + 分区）

- [ ] **Step 1: kafka.md**

读 `reference/stream/kafka/`（Java + Scala），重点 `core/src/main/scala/`、`clients/`。写 `docs/db-wiki/stream/kafka.md`，提交推送。

- [ ] **Step 2: redpanda.md**

读 `reference/stream/redpanda/`（C++），重点 `src/v/`、`src/v/storage/`。扩展「C++ 重写 Kafka」。写 `docs/db-wiki/stream/redpanda.md`，提交推送。

- [ ] **Step 3: pulsar.md**

读 `reference/stream/pulsar/`（Java），重点 `pulsar-broker/`、`pulsar-client/`、`managed-ledger/`。扩展「BookKeeper 分层存储」。写 `docs/db-wiki/stream/pulsar.md`，提交推送。

- [ ] **Step 4: nats.md**

读 `reference/stream/nats/`（Go），重点 `server/`。写 `docs/db-wiki/stream/nats.md`，提交推送。

- [ ] **Step 5: 自检 4 篇 wiki 满足验收标准**

---

## Task 13: P3.4 — 写 4 篇 embedded wiki

**Files:**
- Create: `docs/db-wiki/embedded/{duckdb,lmdb,boltdb,pklite}.md`

**Interfaces:**
- Consumes: `reference/embedded/<项目>/` 中的源码
- Produces: 4 篇 wiki（嵌入式类型加深 7 mmap / 文件 I/O、20 嵌入式；裁剪 11 代理）

- [ ] **Step 1: duckdb.md**

读 `reference/embedded/duckdb/`（C++），重点 `src/storage/`、`src/execution/`、`src/common/`。扩展「OLAP 嵌入式」。写 `docs/db-wiki/embedded/duckdb.md`，提交推送。

- [ ] **Step 2: lmdb.md**

读 `reference/embedded/lmdb/`（C），重点 `lmdb/`、`mdb.c`。扩展「B+tree mmap 极致轻量」。写 `docs/db-wiki/embedded/lmdb.md`，提交推送。

- [ ] **Step 3: boltdb.md**

读 `reference/embedded/boltdb/`（Go），重点 `db.go`、`bucket.go`、`page.go`。扩展「B+tree 嵌入式 KV」。写 `docs/db-wiki/embedded/boltdb.md`，提交推送。

- [ ] **Step 4: pklite.md**

读 `reference/embedded/pklite/`。扩展「轻量嵌入式数据库」。写 `docs/db-wiki/embedded/pklite.md`，提交推送。

- [ ] **Step 5: 自检 4 篇 wiki 满足验收标准**

---

## Task 14: P3.5 — 写 6 篇 extension wiki

**Files:**
- Create: `docs/db-wiki/extension/{arrow,pgvector,datafusion,velox,lance,hnswlib}.md`

**Interfaces:**
- Consumes: `reference/extension/<项目>/` 中的源码
- Produces: 6 篇 wiki（扩展类型重点「与主项目集成」维度）

- [ ] **Step 1: arrow.md**

读 `reference/extension/arrow/`，重点 `cpp/src/arrow/`、`python/`、`rust/`。扩展「列式内存格式跨语言」。写 `docs/db-wiki/extension/arrow.md`，提交推送。

- [ ] **Step 2: pgvector.md**

读 `reference/extension/pgvector/`，重点 `src/`、`vector.c`。扩展「PostgreSQL 向量扩展」。写 `docs/db-wiki/extension/pgvector.md`，提交推送。

- [ ] **Step 3: datafusion.md**

读 `reference/extension/datafusion/`（Rust），重点 `datafusion/`、`datafusion-expr/`。扩展「Rust SQL 执行引擎」。写 `docs/db-wiki/extension/datafusion.md`，提交推送。

- [ ] **Step 4: velox.md**

读 `reference/extension/velox/`（C++），重点 `velox/`、`velox/exec/`。扩展「Meta 向量化执行引擎」。写 `docs/db-wiki/extension/velox.md`，提交推送。

- [ ] **Step 5: lance.md**

读 `reference/extension/lance/`（Rust），重点 `src/`。扩展「Lance 列式向量格式」。写 `docs/db-wiki/extension/lance.md`，提交推送。

- [ ] **Step 6: hnswlib.md**

读 `reference/extension/hnswlib/`（C++ + Python 绑定），重点 `hnswlib/`、`python_bindings/`。扩展「HNSW 索引库」。写 `docs/db-wiki/extension/hnswlib.md`，提交推送。

- [ ] **Step 7: 自检 6 篇 wiki 满足验收标准**

---

## Task 15: P4.1 — 写 11 篇 RAG wiki

**Files:**
- Create: `docs/rag-wiki/{langchain,llama-index,haystack-ai,txtai,embedchain,verba,cognita,r2r,quivr,private-gpt,qanything}.md`

**Interfaces:**
- Consumes: `reference/ai/rag/<项目>/`（Task 2 已 clone）
- Produces: 11 篇 wiki（RAG 类型扩展 4 个维度：嵌入模型适配、文档解析器、检索器/重排器、向量库后端）

- [ ] **Step 1: langchain.md**

读 `reference/ai/rag/langchain/`，重点 `libs/langchain/langchain/chains/`、`libs/langchain/langchain/agents/`。扩展四个 RAG 维度。写 `docs/rag-wiki/langchain.md`，提交推送。

- [ ] **Step 2: llama-index.md**

读 `reference/ai/rag/llama-index/`，重点 `llama-index-core/`、`llama-index-retrievers/`。扩展四个 RAG 维度。写 `docs/rag-wiki/llama-index.md`，提交推送。

- [ ] **Step 3: haystack-ai.md**

读 `reference/ai/rag/haystack-ai/`，重点 `haystack/components/`、`haystack/pipelines/`。扩展四个 RAG 维度。写 `docs/rag-wiki/haystack-ai.md`，提交推送。

- [ ] **Step 4: txtai.md**

读 `reference/ai/rag/txtai/`，重点 `src/python/txtai/`。扩展四个 RAG 维度。写 `docs/rag-wiki/txtai.md`，提交推送。

- [ ] **Step 5: embedchain.md**

读 `reference/ai/rag/embedchain/`，重点 `embedchain/`。扩展四个 RAG 维度。写 `docs/rag-wiki/embedchain.md`，提交推送。

- [ ] **Step 6: verba.md**

读 `reference/ai/rag/verba/`，重点前端 + 后端。扩展四个 RAG 维度。写 `docs/rag-wiki/verba.md`，提交推送。

- [ ] **Step 7: cognita.md**

读 `reference/ai/rag/cognita/`，重点 `backend/`。扩展四个 RAG 维度。写 `docs/rag-wiki/cognita.md`，提交推送。

- [ ] **Step 8: r2r.md**

读 `reference/ai/rag/r2r/`，重点 `r2r/`。扩展四个 RAG 维度。写 `docs/rag-wiki/r2r.md`，提交推送。

- [ ] **Step 9: quivr.md**

读 `reference/ai/rag/quivr/`，重点 `frontend/`、`backend/`。扩展四个 RAG 维度。写 `docs/rag-wiki/quivr.md`，提交推送。

- [ ] **Step 10: private-gpt.md**

读 `reference/ai/rag/private-gpt/`，重点 `privategpt/`。扩展四个 RAG 维度（强调本地/隐私）。写 `docs/rag-wiki/private-gpt.md`，提交推送。

- [ ] **Step 11: qanything.md**

读 `reference/ai/rag/qanything/`，重点 `qanything_server/`。扩展四个 RAG 维度。写 `docs/rag-wiki/qanything.md`，提交推送。

- [ ] **Step 12: 自检 11 篇 wiki 满足验收标准**

---

## Task 16: P4.2 — 写 13 篇 Agent wiki

**Files:**
- Create: `docs/agent-wiki/{langgraph,crewai,autogen,metagpt,openai-agents,chatdev,agentverse,agixt,open-interpreter,agentscope,letta,swarm,agents}.md`

**Interfaces:**
- Consumes: `reference/ai/agent/<项目>/`（Task 2 已 clone）
- Produces: 13 篇 wiki（Agent 类型扩展 4 个维度：角色协作模式、记忆系统、工具调用协议、人机协作）

- [ ] **Step 1: langgraph.md**

读 `reference/ai/agent/langgraph/`，重点 `libs/langgraph/langgraph/`、`libs/langgraph/langgraph/graph/`。扩展四个 Agent 维度。写 `docs/agent-wiki/langgraph.md`，提交推送。

- [ ] **Step 2: crewai.md**

读 `reference/ai/agent/crewai/`，重点 `crewai/`、`crewai/agents/`、`crewai/tasks/`。扩展四个 Agent 维度。写 `docs/agent-wiki/crewai.md`，提交推送。

- [ ] **Step 3: autogen.md**

读 `reference/ai/agent/autogen/`，重点 `autogen/`、`autogen/agentchat/`。扩展四个 Agent 维度。写 `docs/agent-wiki/autogen.md`，提交推送。

- [ ] **Step 4: metagpt.md**

读 `reference/ai/agent/metagpt/`，重点 `metagpt/`、`metagpt/roles/`、`metagpt/software_company.py`。扩展四个 Agent 维度。写 `docs/agent-wiki/metagpt.md`，提交推送。

- [ ] **Step 5: openai-agents.md**

读 `reference/ai/agent/openai-agents/`，重点 `src/agents/`。扩展四个 Agent 维度。写 `docs/agent-wiki/openai-agents.md`，提交推送。

- [ ] **Step 6: chatdev.md**

读 `reference/ai/agent/chatdev/`，重点 `chatdev/`。扩展四个 Agent 维度。写 `docs/agent-wiki/chatdev.md`，提交推送。

- [ ] **Step 7: agentverse.md**

读 `reference/ai/agent/agentverse/`，重点 `agentverse/`。扩展四个 Agent 维度。写 `docs/agent-wiki/agentverse.md`，提交推送。

- [ ] **Step 8: agixt.md**

读 `reference/ai/agent/agixt/`，重点 `agixt/`。扩展四个 Agent 维度。写 `docs/agent-wiki/agixt.md`，提交推送。

- [ ] **Step 9: open-interpreter.md**

读 `reference/ai/agent/open-interpreter/`，重点 `interpreter/`、`interpreter/core/`。扩展四个 Agent 维度。写 `docs/agent-wiki/open-interpreter.md`，提交推送。

- [ ] **Step 10: agentscope.md**

读 `reference/ai/agent/agentscope/`，重点 `src/agentscope/`。扩展四个 Agent 维度。写 `docs/agent-wiki/agentscope.md`，提交推送。

- [ ] **Step 11: letta.md**

读 `reference/ai/agent/letta/`，重点 `letta/`、`letta/agents/`。扩展四个 Agent 维度（强调长期记忆）。写 `docs/agent-wiki/letta.md`，提交推送。

- [ ] **Step 12: swarm.md**

读 `reference/ai/agent/swarm/`，重点 `swarm/`。扩展四个 Agent 维度。写 `docs/agent-wiki/swarm.md`，提交推送。

- [ ] **Step 13: agents.md**

读 `reference/ai/agent/agents/`，重点 `packages/agents/`。扩展四个 Agent 维度。写 `docs/agent-wiki/agents.md`，提交推送。

- [ ] **Step 14: 自检 13 篇 wiki 满足验收标准**

---

## Task 17: P4.3 — 写 2 篇商业背景 wiki

**Files:**
- Create: `docs/rag-wiki/commercial-rag-products.md`
- Create: `docs/agent-wiki/commercial-agent-products.md`

**Interfaces:**
- Consumes: 网络资料（官方页面、新闻稿、blog）、`docs/multimodal-db-comparison-2026.md` 中可能的相关对比
- Produces: 2 篇背景 wiki（覆盖设计文档 4.4 节规定的维度：1-5、11-15、19、23；剩余维度标注"商业闭源"）

- [ ] **Step 1: commercial-rag-products.md**

WebSearch 收集 Notion AI、ChatGPT Enterprise、Microsoft Copilot for Search、Glean、Hebbia、Amazon Kendra、Azure AI Search、Google Vertex AI Search、阿里云智能客服的信息。覆盖维度 1-5（基础信息）、11-15（运维与生态）、19（学习资源）、23（与本项目关联）。每个产品至少 3 段：定位/客户/集成方式。写 `docs/rag-wiki/commercial-rag-products.md`，提交推送。

- [ ] **Step 2: commercial-agent-products.md**

WebSearch 收集 AutoGPT（早期）、Devin、Cursor、Replit Agent、Sourcegraph Cody、GitHub Copilot Workspace、Claude Computer Use、Manus、阿里通义晓蜜的信息。覆盖维度 1-5、11-15、19、23。每个产品至少 3 段：定位/能力边界/商业模式。写 `docs/agent-wiki/commercial-agent-products.md`，提交推送。

- [ ] **Step 3: 自检 2 篇 wiki 满足验收标准**

---

## Task 18: P5 — 完善 3 个索引页 + 全量验收

**Files:**
- Modify: `docs/db-wiki/README.md`（填实项目状态）
- Modify: `docs/rag-wiki/README.md`（填实项目状态）
- Modify: `docs/agent-wiki/README.md`（填实项目状态）

**Interfaces:**
- Consumes: Task 3-17 全部产出
- Produces: 3 个 README.md 含全部项目链接、状态、最近更新日期

- [ ] **Step 1: 完善 docs/db-wiki/README.md**

追加每个分类下列表，包括已完成项目链接与状态。示例：

```markdown
| 分类 | 项目 | 状态 |
|------|------|------|
| relational | [postgres.md](./postgres.md) | ✅ |
| relational | [mysql.md](./mysql.md) | ✅ |
| ... |
```

对 68 个项目逐一标记。

- [ ] **Step 2: 完善 docs/rag-wiki/README.md**

追加每个项目的完成状态：

```markdown
| 项目 | 状态 |
|------|------|
| [langchain.md](./langchain.md) | ✅ |
| ... |
```

- [ ] **Step 3: 完善 docs/agent-wiki/README.md**

同 Step 2 模式。

- [ ] **Step 4: 全量自检**

```bash
# 检查 94 篇 wiki 全部存在
find docs/db-wiki docs/rag-wiki docs/agent-wiki -name '*.md' ! -name 'README.md' | wc -l
# 期望：94

# 检查每篇至少 10 章节
for f in $(find docs/db-wiki docs/rag-wiki docs/agent-wiki -name '*.md' ! -name 'README.md'); do
  cnt=$(grep -c '^## ' "$f")
  if [ "$cnt" -lt 10 ]; then
    echo "WARN: $f 只有 $cnt 章节"
  fi
done

# 检查每篇至少 5 源码路径引用
for f in $(find docs/db-wiki docs/rag-wiki docs/agent-wiki -name '*.md' ! -name 'README.md'); do
  cnt=$(grep -cE 'src/|sql/|\.c:|\.cpp:|\.go:|\.py:|\.rs:|\.java:' "$f")
  if [ "$cnt" -lt 5 ]; then
    echo "WARN: $f 只有 $cnt 源码引用"
  fi
done

# 检查 3 个索引页全部存在
test -f docs/db-wiki/README.md && echo "db-wiki/README.md OK"
test -f docs/rag-wiki/README.md && echo "rag-wiki/README.md OK"
test -f docs/agent-wiki/README.md && echo "agent-wiki/README.md OK"
```

期望：94 篇文件全部存在，无 WARN 警告，3 个索引页 OK。

- [ ] **Step 5: 提交并推送**

```bash
git add docs/db-wiki/README.md docs/rag-wiki/README.md docs/agent-wiki/README.md
git commit -m "docs: 完善三索引页与全量验收"
git push origin main
```

期望：commit 成功并推送至 origin/main。

---

## 自检报告（写作完成后核对）

- ✅ **范围覆盖**：设计文档第 2 节范围已在 Task 1-17 中映射（68 数据库 + 11 RAG + 13 Agent + 2 商业 = 94 篇）。
- ✅ **文档位置**：Task 1 显式规定 `docs/db-wiki/`、`docs/rag-wiki/`、`docs/agent-wiki/`。
- ✅ **文档骨架**：Task 3-17 引用设计文档第 4 节骨架；按项目类型动态裁剪（见"工作流模板"中表格）。
- ✅ **clone 位置**：Task 2 显式规定 `reference/ai/rag/` 与 `reference/ai/agent/`。
- ✅ **执行模式**：分类批量串行（Task 3-17 按数据库 13 类 + RAG + Agent 串行；每个 task 内部按项目循环）。
- ✅ **验收标准**：Task 3-18 结尾均带自检命令；Task 18 全量自检覆盖 94 篇。
- ✅ **commit 推送纪律**：每个 task 末尾命令均 `git commit` + `git push origin main`。
- ✅ **Type 命名**：commit message 全部中文；类型 `docs:`、`chore:`。
- ✅ **零 placeholder**：无 TBD/TODO；所有命令完整。
- ✅ **一方法一名**：每篇 wiki 文件名与目录名一致。