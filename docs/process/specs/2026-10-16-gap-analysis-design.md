# 自研 vs 业界多模态数据库 / RAG 差距分析 — 设计文档

| 字段 | 值 |
|------|-----|
| 文档版本 | v1.0 |
| 创建日期 | 2026-10-16 |
| 状态 | 设计已通过，待用户审核 spec |
| 范围 | 自研多模态数据库（engine/）+ 自研多模态 RAG（multimodal_rag/）vs 业界 94 篇 wiki |
| 文档存放 | `docs/analysis/` |
| 读者 | 个人研发者（学习决策导向） |
| 对照范围 | 全谱对位（94 篇 wiki 全部参与对照） |
| 产出形式 | 总报告 + 13 篇子报告 + 补齐项下月计划 |

---

## 1. 背景与目标

经过 18 个任务的多模态 wiki 库建设（`docs/db-wiki/`、`docs/rag-wiki/`、`docs/agent-wiki/`，共 94 篇 wiki + 3 个索引页），现在已对业界 13 类数据模型 + 11 RAG 框架 + 13 Agent 框架 + 2 篇商业背景有了系统化认知。

**本设计要解决的问题**：

1. 把 wiki 库的认知反向应用于**自研项目**——基于 wiki 找出「自研覆盖度」「针对性差距」「业界主流方案」
2. 输出**总报告**（一眼看出整体优劣）+ **13 篇子报告**（按 12 模态 + RAG + Agent 拆分）+ **补齐项下月计划**（行动型）
3. 避免"以为短板其实不是"——引入风险与陷阱段，给出**认知偏差**的提醒

**不做的事**：
- 不重写 wiki（已 100% 完成）
- 不修改自研代码（仅分析 + 路线图）
- 不修改 `engine/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`
- 不引入新依赖

---

## 2. 范围

### 2.1 自研对象

- **多模态数据库**：`engine/src/db/`、`engine/include/db/`
  - 存储引擎：catalog/buf/heapam/btreeam/wal/disk/page
  - 索引：B+Tree、Hash、GIN/GiST、BRIN、Hilbert、ART、vector_index（faiss_hnsw / dist_hnsw / faiss_ivf_compat）
  - 执行器：executor/sql、executor/graph、executor/rag、executor/parallel、executor/operators、executor/framework
  - 多模型：KV/关系/文档/时序/图/向量 6 大模态

- **多模态 RAG**：`engine/apps/multimodal_rag/`
  - 14 个子模块：agent / core / embed / engine / eval / graph / index / llm / observability / pipeline / retrieval / server / util
  - 含 main.cpp（推测为 C++ 入口）+ Python scripts（gguf_to_fp32.py）

### 2.2 业界对照范围

- 数据库 wiki：68 篇（`docs/db-wiki/` 12 类）
- RAG wiki：11 篇 + 1 商业背景（`docs/rag-wiki/`）
- Agent wiki：13 篇 + 1 商业背景（`docs/agent-wiki/`）
- 合计 94 篇 + 2 商业 = 96 个对照点

### 2.3 不在范围内

- 重写自研代码
- 重新 clone wiki 已覆盖项目
- benchmark 实测（仅引用 wiki 中提到的数字）
- 商业产品技术细节推测（仅引用公开内容）

---

## 3. 文档位置与命名

```
docs/
└── gap-analysis/                          # 新建顶层目录
    ├── README.md                          # 总报告（1 篇，600-900 行）
    ├── relational.md                      # 子报告 1/13
    ├── key-value.md                       # 子报告 2/13
    ├── document.md                        # 子报告 3/13
    ├── columnar.md                        # 子报告 4/13
    ├── time-series.md                     # 子报告 5/13
    ├── search.md                          # 子报告 6/13
    ├── vector.md                          # 子报告 7/13
    ├── graph.md                           # 子报告 8/13
    ├── stream.md                          # 子报告 9/13
    ├── embedded.md                        # 子报告 10/13
    ├── extension.md                       # 子报告 11/13
    ├── rag.md                             # 子报告 12/13
    └── agent.md                           # 子报告 13/13
```

合计 14 个 .md 文件（1 总 + 13 子）。**`docs/analysis/` 顶层新建**，不污染 wiki 库三目录。

---

## 4. 报告结构

### 4.1 总报告 `docs/analysis/README.md`

1. **摘要**：自研覆盖度统计（12 模态 × 24 维度的覆盖率矩阵）
2. **整体差距诊断**：5-7 条关键短板（基于子报告 Top-5 提炼）
3. **优势亮点**：3-5 条自研独有的设计选择（不可被业界产品替代的）
4. **RAG/Agent 专题结论**：从 rag.md / agent.md 子报告提炼
5. **6 个月路线图**：12 个子报告各 1 行的补齐项（按 P0/P1/P2 排序）
6. **风险与陷阱**：5-7 条认知偏差（"以为短板其实不是"）

**长度**：600-900 行 markdown

### 4.2 子报告统一模板（13 篇通用）

每篇子报告按以下 7 节组织：

1. **自研现状**：5-10 条（技术决策 / 关键路径 / 覆盖度）
2. **业界主流做法**：2-3 个对照项目的核心做法
3. **针对性差距矩阵**：按 24 维度逐条对比（与 wiki 设计文档第 4 节一致）
4. **关键差距 Top-5**：具体到代码路径 / wiki 引用
5. **优势 Top-3**：自研独有的设计
6. **学习路径**：推荐精读 2-3 篇 wiki
7. **补齐项下月计划**：3-5 个具体动作（见 §5）

**每篇长度**：300-500 行 markdown

### 4.3 24 维度对照矩阵（继承 wiki 设计）

来自 `docs/process/specs/2026-10-16-multimodal-db-wiki-design.md` §4.2：

| 维度类别 | 维度 |
|---------|------|
| 基础信息 | 1. 项目定位、2. 发展历史、3. 许可限制、4. 使用方案例、5. 对比定位 |
| 架构实现 | 6. 整体架构、7. 存储引擎、8. 索引机制、9. 查询/计算/接口层、10. 事务一致性 |
| 运维生态 | 11. 复制高可用、12. 扩展性、13. 性能基准、14. 运维可观测性、15. 生态集成 |
| 学习路径 | 16. 核心论文、17. 源码路径、18. 源码规模、19. 学习资源、20. 可移植性 |
| 补充 | 21. 变体衍生、22. 风险陷阱、23. 与本项目关联、24. 参考资料 |

**对照矩阵列**：自研现状 / 业界 1 / 业界 2 / 差距等级（A=领先 / B=持平 / C=落后 / D=缺失）

### 4.4 13 篇子报告与对照项目映射

| 子报告 | 自研模块 | 主要对照 wiki（2-3 个） |
|--------|---------|----------------------|
| relational | `db/storage`, `db/heapam`, `db/btreeam`, `db/sql` | postgres, mysql, tidb |
| key-value | `db/storage`, `db/wal`, `db/buf` | rocksdb, leveldb, redis |
| document | `db/storage`, `db/rel`, `db/heap` | mongodb, ferretdb |
| columnar | `db/columnar_engine` (推测), `db/cf` | clickhouse, doris, duckdb |
| time-series | `db/ts` (推测), `index/btree` | influxdb, tdengine, questdb |
| search | `index/fulltext`, `index/gin` | elasticsearch, tantivy, meilisearch |
| vector | `index/vector_index/*`, `db/executor/operators` | milvus, faiss, qdrant |
| graph | `db/executor/graph`, `db/rel` | neo4j, kuzu, nebula |
| stream | (尚未在 db 中找到) | kafka, pulsar, nats |
| embedded | (尚未在 db 中找到) | duckdb, lmdb, boltdb |
| extension | `db/executor/framework`, `db/api` | arrow, datafusion, velox |
| rag | `apps/multimodal_rag/*` (14 子模块) | langchain, llama-index, haystack |
| agent | `apps/multimodal_rag/agent` (推测) | langgraph, crewai, autogen |

---

## 5. 补齐项下月计划

### 5.1 统一字段

每条补齐项包含 5 字段：

| 字段 | 说明 |
|------|------|
| 动作 | 1 行描述（动词开头） |
| 投入 | 半天 / 1 天 / 2-3 天 / 1 周 |
| 学习资源 | 1-2 个 wiki 链接 / 论文 / 官方文档 |
| 验收标准 | 1 行可验证标准（grep / build / test） |
| 优先级 | P0（必做）/ P1（应做）/ P2（待议） |

### 5.2 优先级评分原则

- **P0**：自研完全缺失或严重过时，会卡住后续演进
- **P1**：业界主流，自研有但实现不够好
- **P2**：业界前沿，自研可暂缓

### 5.3 总报告中的 6 个月路线图

将 13 个子报告的 P0 项汇总为 24 周（约 6 个月）路线图：
- 每月 2-3 个 P0
- 每周 1 个可交付动作
- 不要求严格按月完成（重在持续推进）

---

## 6. 执行计划

### 6.1 任务切片（14 个子任务）

| # | 子任务 | 工作量 |
|---|--------|-------|
| 1 | 探索 engine/ + multimodal_rag/ 自研现状 | 半天 |
| 2 | 写总报告 README.md | 1-2 天 |
| 3-14 | 写 13 篇子报告（按 12 模态 + RAG + Agent） | 每篇 2-3 小时 |

### 6.2 执行模式：分类批量串行

- 不使用 subagent（避免 wiki 阶段 100+ subagent 调用的成本）
- 每天 2-3 篇（约 2-3 小时/篇）
- 5 个工作日完成

### 6.3 验证标准（每篇子报告）

- [ ] 24 维度对照矩阵完整（不能跳列）
- [ ] Top-5 差距每条有具体代码路径 / wiki 引用
- [ ] 补齐项下月计划至少 3 条 P0/P1
- [ ] 至少 5 处 `engine/` 真实路径引用
- [ ] 与 wiki 中对应项目的内容一致（不出现冲突）

### 6.4 风险

| 风险 | 缓解 |
|------|------|
| 13 篇产出过多（实际 1-2 周） | 按"在做/即将做"模块优先选 5-8 篇 |
| 自研模块文档不足 | 先回看 `engine/` 源码 + CLAUDE.md + 工程轨 docs |
| 主观偏差（"以为短板其实不是"） | 总报告第 6 节专项提醒，引用 wiki 中"风险与陷阱"段 |
| 容易和 wiki 内容冲突 | 严格按 24 维度对照，不引入新观点 |

---

## 7. 验收与全量

### 7.1 总报告验收

- [ ] 12 模态覆盖度统计完整
- [ ] 5-7 条整体差距诊断具体到子报告
- [ ] 3-5 条优势亮点有自研独有证据
- [ ] 6 个月路线图按 P0/P1/P2 排序
- [ ] 风险与陷阱段 5-7 条

### 7.2 子报告验收（每篇）

- [ ] 24 维度对照矩阵完整
- [ ] 自研现状 5-10 条
- [ ] 业界主流 2-3 个项目
- [ ] Top-5 差距具体
- [ ] Top-3 优势具体
- [ ] 学习路径 2-3 个 wiki
- [ ] 下月计划 3-5 条 P0/P1

### 7.3 全量

- [ ] 14 个 .md 文件全部存在
- [ ] 全部 commit + push 到 origin main
- [ ] 进度跟踪在 `docs/process/sdd/progress.md` 续写 gap-analysis 段

---

## 8. 与现有系统的衔接

- **新增** `docs/analysis/` 顶层目录（不污染 wiki 库三目录）
- **续写** `docs/process/sdd/progress.md` 加 gap-analysis 段
- **更新** `docs/db-wiki/README.md`、`docs/rag-wiki/README.md`、`docs/agent-wiki/README.md` 在末尾添加「差距分析」直达链接
- **不修改** `engine/`、`learning/`、根 `CMakeLists.txt`、`CLAUDE.md`、`AGENTS.md`

---

## 9. 不做（明确排除）

- 不重写 wiki
- 不修改自研代码
- 不修改 `engine/`、`learning/`、根 `CMakeLists.txt`
- 不修改 `CLAUDE.md` / `AGENTS.md` 中的纪律条款
- 不引入新依赖
- 不 benchmark 实测
- 不为商业产品推测内部技术细节

---

## 10. 参考

- `docs/process/specs/2026-10-16-multimodal-db-wiki-design.md`：wiki 库设计（24 维度骨架）
- `docs/process/plans/2026-10-16-multimodal-db-wiki-implementation.md`：wiki 库实施计划
- `docs/db-wiki/README.md` / `docs/rag-wiki/README.md` / `docs/agent-wiki/README.md`：94 篇 wiki 入口
- `engine/CLAUDE.md` / 工程轨 `README.md`：自研现状
- `CLAUDE.md`：项目语言规范、双轨纪律、提交推送纪律
