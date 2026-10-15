# 多模态数据库 & AI 框架 Wiki 库 — 设计文档

| 字段 | 值 |
|------|-----|
| 文档版本 | v1.0 |
| 创建日期 | 2026-10-16 |
| 状态 | 设计已通过，待用户审核 |
| 范围 | 13 个数据库分类 + AI 框架（RAG/Agent） |
| 文档存放 | `docs/db-wiki/`、`docs/rag-wiki/`、`docs/agent-wiki/` |
| 源码存放 | `reference/<分类>/<项目>/`（已存在）+ `reference/ai/rag/`、`reference/ai/agent/`（新建） |

---

## 1. 背景与目标

`reference/` 目录下以只读参考源码的形式托管了大量开源数据库项目，按 13 个数据模型分类（relational / distributed-sql / key-value / document / columnar / time-series / search / vector / graph / stream / embedded / extension / benchmark），并在 `catalog.yml` 中维护了约 73 个项目的元信息。

随着 RAG 与 Agent 范式兴起，业界出现了一大批 GitHub 上的 RAG / Agent 框架，亟需补充到 `reference/` 作为学习参考。

**本设计要解决的问题**：

1. 把 `reference/` 下 70+ 个开源数据库项目，每个生成一份**深度扩展版**的中文 wiki，按 10+ 个特性维度展开
2. 把业界主流的 RAG 与 Agent 项目 clone 到本地（`reference/ai/rag/` 与 `reference/ai/agent/`），并同样生成 wiki
3. 把业界主流的商业 RAG / Agent 产品（不开源）作为单独背景 wiki，介绍其特性、定位、典型客户
4. 在 `docs/` 下形成完整中文 wiki 体系，用作长期知识沉淀，方便后续在 `engineering/`、`learning/` 中复用其设计思想

---

## 2. 范围

### 2.1 数据库 wiki 范围

覆盖 `catalog.yml` 中 13 个数据模型分类下的 68 个开源项目，每个项目一篇 wiki。`benchmark/` 下的 5 个项目（ann-benchmarks、vectordbbench、tpch、tpcds、db-bench）作为**参考资料**在 wiki 引用部分提及，不单独写 wiki。

| 分类 | 项目数 | wiki 数 |
|------|--------|---------|
| relational | 7 | 7 |
| distributed-sql | 4 | 4 |
| key-value | 7 | 7 |
| document | 5 | 5 |
| columnar | 5 | 5 |
| time-series | 5 | 5 |
| search | 6 | 6 |
| vector | 9 | 9 |
| graph | 6 | 6 |
| stream | 4 | 4 |
| embedded | 4 | 4 |
| extension | 6 | 6 |
| benchmark | 5 | 0（仅引用） |

### 2.2 RAG 范围

**11 个开源项目**（全部 clone 到 `reference/ai/rag/`）：
- 主流：langchain、llama-index、haystack-ai
- 轻量级：txtai、embedchain、private-gpt
- 平台型：verba（Weaviate 出品）、cognita（TrueFoundry 出品）、quivr
- 新锐：r2r（SciPhi 出品）、qanything（网易）

**商业项目背景**（1 篇 wiki，介绍但不开源）：
- Notion AI Q&A、ChatGPT Enterprise、Microsoft Copilot for Search、Glean、Hebbia、Amazon Kendra、Azure AI Search、Google Vertex AI Search、阿里云智能客服

### 2.3 Agent 范围

**13 个开源项目**（全部 clone 到 `reference/ai/agent/`）：
- 主流框架：langgraph、crewai、autogen、metagpt
- 官方/SDK：openai-agents、swarm、agents（Microsoft 新版）
- 教学/研究：chatdev、agentverse
- 工具型：agixt、open-interpreter
- 国产：agentscope（阿里）

**商业项目背景**（1 篇 wiki，介绍但不开源）：
- AutoGPT（早期）、Devin、Cursor、Replit Agent、Sourcegraph Cody、GitHub Copilot Workspace、Claude Computer Use、Manus、阿里通义晓蜜

### 2.4 不在范围内

- 项目重构、二次开发、移植到工程轨
- benchmark 项目独立 wiki（仅作引用）
- 商业产品源码 clone（仅 wiki 介绍）
- 对现有 `engineering/` 与 `learning/` 内容的修改

---

## 3. 文档位置与命名

### 3.1 wiki 文档位置

```
docs/
├── db-wiki/
│   ├── README.md                  # 数据库 wiki 总索引页
│   ├── relational/                # 7 篇
│   ├── distributed-sql/           # 4 篇
│   ├── key-value/                 # 7 篇
│   ├── document/                  # 5 篇
│   ├── columnar/                  # 5 篇
│   ├── time-series/               # 5 篇
│   ├── search/                    # 6 篇
│   ├── vector/                    # 9 篇
│   ├── graph/                     # 6 篇
│   ├── stream/                    # 4 篇
│   ├── embedded/                  # 4 篇
│   └── extension/                 # 6 篇
├── rag-wiki/
│   ├── README.md                  # RAG wiki 总索引页
│   ├── langchain.md
│   ├── llama-index.md
│   ├── haystack-ai.md
│   ├── txtai.md
│   ├── embedchain.md
│   ├── verba.md
│   ├── cognita.md
│   ├── r2r.md
│   ├── quivr.md
│   ├── private-gpt.md
│   ├── qanything.md
│   └── commercial-rag-products.md
└── agent-wiki/
    ├── README.md                  # Agent wiki 总索引页
    ├── langgraph.md
    ├── crewai.md
    ├── autogen.md
    ├── metagpt.md
    ├── openai-agents.md
    ├── chatdev.md
    ├── agentverse.md
    ├── agixt.md
    ├── open-interpreter.md
    ├── agentscope.md
    ├── letta.md
    ├── swarm.md
    ├── agents.md
    └── commercial-agent-products.md
```

**合计 94 篇 wiki（68 数据库 + 11 RAG + 13 Agent + 2 商业背景）+ 3 个索引页 = 97 个 markdown 文件。**

### 3.2 源码 clone 位置

```
reference/
├── ai/                            # 新建一级目录
│   ├── rag/
│   │   ├── langchain/
│   │   ├── llama-index/
│   │   ├── haystack-ai/
│   │   ├── txtai/
│   │   ├── embedchain/
│   │   ├── verba/
│   │   ├── cognita/
│   │   ├── r2r/
│   │   ├── quivr/
│   │   ├── private-gpt/
│   │   └── qanything/
│   └── agent/
│       ├── langgraph/
│       ├── crewai/
│       ├── autogen/
│       ├── metagpt/
│       ├── openai-agents/
│       ├── chatdev/
│       ├── agentverse/
│       ├── agixt/
│       ├── open-interpreter/
│       ├── agentscope/
│       ├── letta/
│       ├── swarm/
│       └── agents/
```

数据库项目源码位置**保持现有结构**（`reference/<分类>/<项目>/`），不动。

### 3.3 命名规范

- 项目目录名（reference 内）保持与上游 GitHub 一致（langchain、llama-index）
- wiki 文件名小写、连字符（`langchain.md`、`llama-index.md`、`open-interpreter.md`），与目录名严格对应
- 商业项目 wiki 文件名加 `-products` 后缀（`commercial-rag-products.md`、`commercial-agent-products.md`），不开新仓

### 3.4 目录归档规范

- 同步更新 `reference/catalog.yml`：在 `model` 字段中为 AI 类项目填 `ai/rag` 或 `ai/agent`，tier 字段按主流/代表/创新 划分
- 新建 `reference/ai/rag/README.md` 与 `reference/ai/agent/README.md` 作为克隆清单
- 更新 `reference/README.md` 增加 `ai/rag/` 与 `ai/agent/` 条目说明

---

## 4. 文档骨架（深度扩展版）

### 4.1 设计原则

- 提供**基础模板作为参考**，但**不强求统一骨架**
- 每个项目的 wiki **根据项目类型与实际情况动态调整章节**
- 数据库内核项目展开全部维度；RAG / Agent 框架裁剪掉不适用章节（如分布式事务），扩展特有维度
- 字数无硬性上限，按需展开，质量优先

### 4.2 基础模板（24 个候选维度）

**一、基础信息（5 项）**
1. 项目定位：一句话核心定位、解决的痛点、典型用户
2. 发展历史与社区：起源、版本演进、维护状态、社区活跃度
3. 许可与商业限制：开源许可证、商业版本差异、SSPL/BSL/AGPL 影响
4. 业界主要使用方与案例：头部公司、生产案例、典型业务场景
5. 一句话对比定位：与同类项目的关键差异点

**二、架构与实现（5 项）**
6. 整体架构：进程模型、模块划分、核心数据结构（必须有 ASCII 架构图）
7. 存储引擎：底层数据结构（LSM-Tree / B+Tree / Columnar / Graph Storage / Append-only / WORM）、刷盘策略、压缩
8. 索引机制：支持的索引类型（B+Tree / Hash / HNSW / IVF / 跳表 / 倒排 / 列存索引等）、索引实现位置、索引代价
9. 查询/计算/接口层：查询语言（SQL / Cypher / GQL / DSL / PromQL）、API（HTTP / gRPC / SDK）、执行模型（Volcano / 矢量化 / 流式）
10. 事务与一致性：ACID、隔离级别、MVCC 实现、共识协议（Raft / Paxos / EPaxos）

**三、运维与生态（5 项）**
11. 复制与高可用：主从/多主/无主、分片策略、跨区域容灾
12. 扩展性：水平/垂直扩展、分区分片、动态扩缩容、冷热分层
13. 性能与基准：内部 benchmark、第三方基准（ann-benchmarks、TPC-H、YCSB）、典型性能数字
14. 运维与可观测性：监控指标、慢查询日志、备份恢复、Prometheus 集成、tracing
15. 生态与集成：客户端 SDK（Python/Go/Java/Rust/JS）、ORM、BI 工具、消息队列、容器化

**四、学习路径（5 项）**
16. 核心论文与算法原理：Raft 论文、B+Tree 论文、HNSW 论文、LSM 论文等引用
17. 源码学习路径：必读模块清单，标注路径（如 `src/storage/.../btree.cpp:1280`）
18. 源码规模与开发语言：LOC 数量、主要语言占比、构建系统（CMake/Bazel/Maven/Go modules）
19. 学习资源：官方文档、推荐书、博客、YouTube 频道、会议演讲、Reddit/HN 讨论
20. 可移植性与部署：支持的平台、容器化、Helm Chart、Operator、嵌入式部署可行性

**五、补充（2+ 项）**
21. 变体与衍生项目：Cloud 版本、商业版（YugaByte、Aurora、TiDB Cloud）、兼容接口（FerretDB/PgCat）
22. 风险与陷阱：常见误用、性能陷阱、迁移痛点、运维雷区
23. 与本项目的关联：`engineering/` 中是否有对应实现（如 `engineering/include/db/` 中是否有相同模式的存储引擎）
24. 参考资料：所有引用的 URL 集中列在文末

### 4.3 项目类型专项维度

**RAG 框架**额外覆盖：
- 嵌入模型适配（OpenAI / HuggingFace / Cohere / 本地）
- 文档解析器（PDF / HTML / Markdown / Docx / 数据库）
- 检索器/重排器（BM25 / 向量 / Hybrid / Cross-encoder）
- 向量库后端（Chroma / Milvus / pgvector / Pinecone / Weaviate）

**Agent 框架**额外覆盖：
- 角色协作模式（中心化 / 对等 / 分层）
- 记忆系统（短期 / 长期 / 向量记忆 / 文件记忆）
- 工具调用协议（Function Calling / ReAct / CodeAct）
- 人机协作（人在回路 / 中断恢复 / 审批流）

**嵌入式引擎**（duckdb、lmdb、boltdb、sqlite、leveldb）额外覆盖：
- 进程内嵌模式 vs 服务器模式
- mmap / 文件 I/O 策略
- 多线程 / 多进程安全

### 4.4 商业项目 wiki 维度

商业项目（不开源）只覆盖：
- 1-5（基础信息）
- 11-15（运维与生态）
- 19（学习资源）
- 23（与本项目关联）

剩余维度明确标注"商业闭源，未公开"。

---

## 5. 执行计划

### 5.1 任务切片（15 个子任务）

| # | 子任务 | 项目数 | wiki 数 | 预计工作量 |
|---|--------|--------|---------|------------|
| 1 | relational | 7 | 7 | 大型项目多，约 7-10 天 |
| 2 | distributed-sql | 4 | 4 | 约 4-5 天 |
| 3 | key-value | 7 | 7 | 约 7-10 天 |
| 4 | document | 5 | 5 | 约 5-7 天 |
| 5 | columnar | 5 | 5 | 约 5-7 天 |
| 6 | time-series | 5 | 5 | 约 5-7 天 |
| 7 | search | 6 | 6 | 约 6-8 天 |
| 8 | vector | 9 | 9 | 约 9-12 天 |
| 9 | graph | 6 | 6 | 约 6-8 天 |
| 10 | stream | 4 | 4 | 约 4-5 天 |
| 11 | embedded | 4 | 4 | 约 4-5 天 |
| 12 | extension | 6 | 6 | 约 6-8 天 |
| 13 | rag（含 clone） | 11 | 11 + 1 商业 | 约 8-10 天 |
| 14 | agent（含 clone） | 13 | 13 + 1 商业 | 约 10-14 天 |
| 15 | 3 个索引页 + 总体验收 | — | 3 | 约 2 天 |

### 5.2 执行模式：分类批量串行

```
分类 1 → 分类 2 → ... → 分类 15
（分类间串行；分类内可并行处理多个项目）

每个分类内部：
1. clone（如未 clone）
2. 读 README + docs + 关键目录结构
3. 抓源码骨架（top-level 模块、核心数据结构）
4. 按项目类型套用基础模板，裁剪章节
5. 写 wiki .md → 落盘 docs/<wiki>/<分类>/<项目>.md
6. 增量更新索引页
7. 提交 + 推送（按 CLAUDE.md 每次 commit 立即 push 到 origin main）
```

### 5.3 验收标准

每篇 wiki 必须满足：

- [ ] 至少 10 个章节（深度扩展版基线，可按项目类型裁剪）
- [ ] 至少有 1 张 ASCII 架构图或关键流程图
- [ ] 至少有 5 处源码路径引用（如 `src/storage/.../btree.cpp:1280`）
- [ ] 至少有 3 个外部引用（论文 / 官方文档 / benchmark）
- [ ] 有"风险与陷阱"或"易踩坑"段落
- [ ] 有"学习路径"段落，标注入门到进阶
- [ ] 中文为主，专有名词保留英文
- [ ] 文件大小无硬性下限（按需）

### 5.4 风险与缓解

| 风险 | 缓解 |
|------|------|
| RAG/Agent 项目 disk space 紧张（合计可能 ~10GB） | 分批 clone，clone 一个删一个临时缓存 |
| 部分仓库 clone 失败（gitee 镜像限制） | 用 GitHub 原始仓库重试；失败项目可降级为「仅 wiki 介绍」 |
| 质量参差 | 每批写完后随机抽查 3 篇 review |
| 大型项目（Postgres、Milvus、Elasticsearch）wiki 体量失控 | 允许拆分子任务（如 Postgres 拆为「查询引擎」「存储引擎」「复制」三篇） |
| 双轨纪律：每周交付 1 批的时序紧张 | 子任务间允许灵活调整；按学习闭环节奏推进 |

### 5.5 与现有系统的衔接

- 在 `docs/db-wiki/README.md` 与 `docs/rag-wiki/README.md`、`docs/agent-wiki/README.md` 提供直达链接
- 在 `reference/README.md` 增加 `ai/rag/` 与 `ai/agent/` 条目说明
- 在 `reference/catalog.yml` 增量登记新项目（`model: ai/rag`、`model: ai/agent`），保持与 git submodule 状态一致
- 在 `reference/ai/rag/README.md` 与 `reference/ai/agent/README.md` 提供克隆清单与版本快照
- 在 `reference/ai/rag/.gitmodules` 与 `reference/ai/agent/.gitmodules` 注册 submodule（如走 submodule 方式）或在主仓 `/.gitmodules` 中注册

---

## 6. 阶段化推进建议

| 阶段 | 内容 | 验收 |
|------|------|------|
| P0 | clone 全部 RAG/Agent 项目到 `reference/ai/rag/` 与 `reference/ai/agent/`，更新 `catalog.yml` | 24 个新仓全部 clone 成功，git submodule 注册 |
| P1 | 完成 relational + distributed-sql + key-value 三类 wiki（共 18 篇） | 18 篇 wiki 落盘 + 提交 |
| P2 | 完成 document + columnar + time-series + search（共 21 篇） | 21 篇 wiki 落盘 + 提交 |
| P3 | 完成 vector + graph + stream + embedded + extension（共 29 篇） | 29 篇 wiki 落盘 + 提交 |
| P4 | 完成 RAG（11 篇）+ Agent（13 篇）+ 商业背景（2 篇） | 26 篇 wiki 落盘 + 提交 |
| P5 | 索引页（3 个 README）+ 总体验收 + 全量 review | 94 篇 wiki 全部索引可点 |

---

## 7. 不做（明确排除）

- 不修改 `engineering/`、`learning/` 任何代码
- 不引入新依赖、不修改根 `CMakeLists.txt`
- 不创建新的 `reference/<一级分类>`（除 `ai/`）
- 不为 benchmark 项目独立写 wiki
- 不为商业产品 clone 源码
- 不修改 CLAUDE.md / AGENTS.md 中的纪律条款

---

## 8. 参考

- `reference/catalog.yml`：完整项目清单
- `reference/README.md`：目录组织说明
- `docs/multimodal-db-comparison-2026.md`：已有的多模态数据库对比（提供写作参考）
- `docs/superpowers/specs/`：已有 30+ 个设计文档，作为格式参考
- `CLAUDE.md`：项目语言规范、双轨纪律、提交推送纪律