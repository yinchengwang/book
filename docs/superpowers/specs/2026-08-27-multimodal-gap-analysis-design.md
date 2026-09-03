# 多模态数据库差距深度分析 — 设计文档

> 日期：2026-08-27
> 状态：待用户审阅
> 前置文档：`docs/multimodal-db-comparison-2026.md`（功能级对比，2026-08-25）、`docs/multimodal-gap-fill-plan-2026.md`（补齐方案）

## 一、背景与目标

2026-08-25 的功能级对比文档已过时：其后 P1-P2 Gap Fill、P3-3 Yang/NETCONF、P3-4 Column Family、P1-4 图分析算法、P2-1 GPU 加速、Cypher 修复等大量 Gap 已落地，文档中列出的多项"差距"实际已关闭。

本任务产出一份**实现质量级**的深度差距分析报告：

- 不只对比功能清单，而是深入到**实现质量层面**——并发正确性、崩溃恢复、内存安全、错误处理、算法实现质量、API 设计
- 覆盖自研库**实际存在**的全部引擎（包括未注册进 `DataModel` 枚举的 9 个）
- 调研业界存在但自研库**完全没有**的 5 类模态，作为扩展方向建议

**非目标**：不改任何代码；不运行新的基准测试（业界对比引用 `reference/` 子模块源码与既有基准数据）。

## 二、范围（22 个领域）

| 分类 | 数量 | 领域 |
|------|------|------|
| 深挖（代码级六维审查） | 8 | Vector / Relational / Graph / KV / Timeseries / Document / Spatial / Tree |
| 未注册引擎核对（轻量） | 9 | RDF(SPARQL) / 稀疏向量+BM25 混合检索 / 流引擎 / 列存引擎 / Column Family / st 引擎 / mmview / ledger 账本 / CDC 订阅 |
| 完全缺失模态调研 | 5 | 全文搜索引擎 / 宽表(Wide-Column) / 对象 Blob 存储 / 可观测日志 / 多模态 AI 原生存储 |

代码位置：

- 已注册模态：`engineering/src/db/storage/{vector,rel,graph,kv,ts,doc,spatial,yang}/`、`engineering/include/db/`
- 未注册引擎：`engineering/src/db/storage/{rdf,sparse,stream,columnar,st,mmview}/`、`engineering/src/db/{cf,ledger,subscription}/`

业界对比基准固定：

| 模态 | 标杆 |
|------|------|
| Vector | Milvus / Qdrant / FAISS / pgvector（`reference/vector/`） |
| Relational | PostgreSQL / DuckDB / SQLite（`reference/relational/`） |
| Graph | Neo4j / NebulaGraph / Memgraph（`reference/graph/`） |
| KV | Redis / RocksDB（`reference/key-value/`） |
| Timeseries | InfluxDB 3.0 / TimescaleDB / TDengine / QuestDB |
| Document | MongoDB / Elasticsearch / CouchDB（`reference/search/`） |
| Spatial | PostGIS / DuckDB Spatial / SpatiaLite / H3 / S2 |
| Tree | MarkLogic / BaseX / libyang / PostgreSQL ltree |
| 缺失模态 | Elasticsearch/Lucene、Cassandra/HBase、S3/MinIO、Loki/ClickHouse Observability、Voxel51/Chroma |

## 三、交付物结构

```
docs/multimodal-gap-analysis-2026/
├── README.md                 # 执行摘要：22 领域差距总矩阵 + 跨模态共性 Gap + 优先级路线图
├── 01-vector-gap.md          # 深挖八卷，统一骨架（见下）
├── 02-relational-gap.md
├── 03-graph-gap.md
├── 04-kv-gap.md
├── 05-timeseries-gap.md
├── 06-document-gap.md
├── 07-spatial-gap.md
├── 08-tree-gap.md
├── 09-unregistered-engines.md   # 9 个未注册引擎，各一节轻量核对
└── 10-missing-modalities.md     # 5 个缺失模态调研，各一节
```

## 四、深挖章节统一骨架（8 卷一致）

1. **实现现状盘点**：模块清单、代码规模、测试覆盖（`engineering/test/db/`）
2. **代码级质量审查**（六维，每维度给出合格线定义）：
   - **并发正确性**：多线程读写路径是否有数据竞争；锁粒度是否合理
   - **崩溃恢复**：WAL 覆盖是否完整；checkpoint 后能否正确恢复；torn page 处理
   - **内存安全**：泄漏 / UAF / 越界；错误路径的清理是否完整
   - **错误处理**：错误码传播一致性；失败路径是否留下半写状态
   - **算法实现质量**：核心算法与标杆实现的偏差（如 HNSW vs FAISS、优化器 vs PostgreSQL）
   - **API 设计**：接口一致性、可组合性、与 `mm_storage` 抽象的契合度
3. **业界标杆对比**：引用 `reference/` 子模块源码与 `docs/multimodal-db-comparison-2026.md` 既有数据
4. **差距矩阵**：量化评分（0-10）+ 具体证据（`file:line`）
5. **改进优先级**：按正确性风险 > 投入产出排序

## 五、关键设计决策

1. **差距结论强制二分**：每个 Gap 必须标注「功能缺失」（工程量问题）或「实现质量缺陷」（正确性风险），处置策略不同。
2. **证据强制**：每个结论要么有代码证据（`file:line`），要么有文档/基准出处；禁止"应该是"式判断。
3. **评分口径统一**：0-10 分锚定业界标杆（10 = 标杆水平，5 = 可用但明显落后，2 以下 = 缺失或有正确性风险）。
4. **旧文档处置**：`multimodal-db-comparison-2026.md` 保留不删，README 中注明本报告取代其"差距分析"部分。

## 六、执行流程

```
阶段 1：并行深挖（8 个子代理，每模态一个）
  产出结构化中间产物：现状 / 六维审查 / 对比 / 差距矩阵 / 优先级

阶段 2：轻量调研（2 个子代理）
  Agent-U：9 个未注册引擎核对（现状 + 完成度 + 注册进 DataModel 的建议）
  Agent-M：5 个缺失模态调研（业界格局 / 主流产品 / 与自研架构契合度 / 建议）

阶段 3：汇总与成文（主线程）
  核对各代理结论 → 消除矛盾（冲突时读代码仲裁，仲裁不了的并列标注争议）
  → 提炼跨模态共性 Gap（如分布式、事务、并发）
  → 写 README 总览 + 分卷终稿 → 提交 git
```

## 七、验收标准

- [ ] 每个深挖模态的每个质量维度 ≥3 个带 `file:line` 的具体证据
- [ ] 每个 Gap 均标注「功能缺失 / 实现质量缺陷」分类
- [ ] README 含 22 领域差距总矩阵、共性 Gap 分析、按投入产出排序的路线图
- [ ] 分卷均按统一骨架组织，量化评分口径一致
- [ ] 报告全部结论可溯源（代码行号 / 文档出处 / 基准数据）

## 八、错误处理

- 子代理结论冲突 → 主线程读代码仲裁；无法仲裁的并列双方观点并标注争议
- 单模态代码量超出单代理处理范围 → 拆分多个代理分区审查后合并
- 子代理失败/超时 → 重试一次，仍失败则该模态降级为轻量核对并在 README 标注

## 九、风险

- 静态审查可能漏判运行时才暴露的问题（并发竞争、崩溃恢复缺陷）→ 结论区分"确认"与"疑似"
- 22 领域篇幅庞大 → 分卷控制每份 ~500 行，README 控制在 300 行内
