# 多模态数据库 Wiki 深度补全 — 实现计划

## 概述

在 `docs/db_wiki/11_multi_model/` 下完成多模态数据库分类的深度补全，共 63 个文件。

## 任务列表

### Task 1：更新 README.md（1 文件）

更新 `docs/db_wiki/11_multi_model/README.md`：
- [ ] 多模态数据库分类定义
- [ ] 演进历史（单模型 → 多模型 → 多模态融合）
- [ ] 核心技术（查询语言统一、存储后端、数据模型映射、ACID 跨模型事务）
- [ ] 对比表（9 库 × 15+ 维度）
- [ ] 选型指南（云原生/嵌入式/Serverless/企业级）
- [ ] 学习路径（4 阶段）

### Task 2：SurrealDB 扩展（6 文件）

- [ ] `docs/db_wiki/11_multi_model/surrealdb/01_architecture.md`
- [ ] `docs/db_wiki/11_multi_model/surrealdb/06_features.md`
- [ ] `docs/db_wiki/11_multi_model/surrealdb/07_use_cases.md`
- [ ] `docs/db_wiki/11_multi_model/surrealdb/08_experiments.md`
- [ ] `docs/db_wiki/11_multi_model/surrealdb/09_resources.md`
- [ ] `docs/db_wiki/11_multi_model/surrealdb/10_project_connection.md`

### Task 3：ArangoDB（7 文件）

- [ ] `docs/db_wiki/11_multi_model/arangodb/00_overview.md`
- [ ] `docs/db_wiki/11_multi_model/arangodb/01_architecture.md`
- [ ] `docs/db_wiki/11_multi_model/arangodb/06_features.md`
- [ ] `docs/db_wiki/11_multi_model/arangodb/07_use_cases.md`
- [ ] `docs/db_wiki/11_multi_model/arangodb/08_experiments.md`
- [ ] `docs/db_wiki/11_multi_model/arangodb/09_resources.md`
- [ ] `docs/db_wiki/11_multi_model/arangodb/10_project_connection.md`

### Task 4：FoundationDB + EdgeDB（14 文件）

- [ ] FoundationDB 7 文件
- [ ] EdgeDB 7 文件

### Task 5：OrientDB + Couchbase（14 文件）

- [ ] OrientDB 7 文件
- [ ] Couchbase 7 文件

### Task 6：Azure CosmosDB + FaunaDB + IRIS（21 文件）

- [ ] Azure CosmosDB 7 文件
- [ ] FaunaDB 7 文件
- [ ] InterSystems IRIS 7 文件

## 实施顺序

1. **Task 1** → README（框架先行）
2. **Task 2** → SurrealDB（已有基础，快速产出）
3. **Task 3** → ArangoDB（最成熟，覆盖面广）
4. **Task 4** → FoundationDB + EdgeDB（苹果/社区新锐，可并行）
5. **Task 5** → OrientDB + Couchbase（经典，可并行）
6. **Task 6** → CosmosDB + FaunaDB + IRIS（云原生/企业级，可并行）
7. **提交推送** — 统一 git add、commit、push

## 文件模板

每个文件遵循以下结构：
- 学习目标（3-4 条）
- 核心概念（Mermaid 图）
- 主要内容（代码示例、架构图、对比表格）
- 要点总结（6-10 条）
- 思考题（3-5 道）

## 工作量估算

| Task | 文件数 | 执行方式 |
|------|--------|---------|
| Task 1: README | 1 | 直接写入 |
| Task 2: SurrealDB | 6 | 单 agent |
| Task 3: ArangoDB | 7 | 单 agent |
| Task 4: FoundationDB+EdgeDB | 14 | 并行 2 agent |
| Task 5: OrientDB+Couchbase | 14 | 并行 2 agent |
| Task 6: CosmosDB+FaunaDB+IRIS | 21 | 并行 3 agent |
| 提交推送 | - | 手动 |
| **合计** | **63** | |

## 验收标准

- [ ] 63 个文件全部创建
- [ ] README.md 包含完整的分类定义、对比表、选型指南
- [ ] 每个库文件包含 Mermaid 图
- [ ] 每个库 10_project_connection.md 与项目 mm_storage.h/index/storage/algo 模块关联
- [ ] 所有文件已提交并推送