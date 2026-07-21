# 多模态数据库 Wiki 深度补全设计

## 1. 概述

在 `docs/db_wiki/11_multi_model/` 下完成多模态数据库分类的深度补全，包括 SurrealDB 现有文档的扩展和 8 个新多模态库的添加，以及 README 分类概述的全面更新。

## 2. 范围

### A 部分 — SurrealDB 扩展

| 文件 | 内容要点 |
|------|---------|
| 01_architecture.md | 三层架构（SurrealQL → 查询引擎 → 存储后端），多模态融合设计（文档+图+KV 统一存储） |
| 06_features.md | SurrealQL 语法特性、实时订阅(LIVE SELECT)、权限模型(SCOPE/DEFINE)、嵌入式部署、Schema-less、ACID 事务 |
| 07_use_cases.md | 创业 MVP 快速原型、边缘计算(嵌入式+离线优先)、实时协作应用(WebSocket 订阅)、知识图谱 |
| 08_experiments.md | Docker 部署、CRUD 操作、图遍历(RELATE)、实时订阅、权限测试 |
| 09_resources.md | 源码目录结构(SDK/查询/引擎/存储)、核心文件、学习路径、社区资源 |
| 10_project_connection.md | 与项目 mm_storage.h 多模态引擎的架构对比，借鉴 SurrealDB 的权限模型和实时订阅设计 |

### B 部分 — 新增 8 库

每个库标准 7 文件：00_overview + 01_architecture + 06_features + 07_use_cases + 08_experiments + 09_resources + 10_project_connection

| 库 | 核心定位 | 数据模型 | 查询语言 | 部署方式 |
|-----|----------|---------|---------|---------|
| ArangoDB | 多模型先驱 | 文档+图+KV | AQL | 单机/集群/DC2DC |
| Azure CosmosDB | 云原生多模型 | 文档/图/列族/KV/时序 | SQL/Gremlin/API | 云原生(全球分发) |
| OrientDB | 图+文档经典 | 图+文档+KV | SQL + Gremlin | 单机/分布式 |
| FaunaDB | Serverless 多模型 | 文档+图+关系 | FQL | Serverless(云) |
| FoundationDB | 可伸缩多模型 | 排序键值+多模型层 | FoundationDB CLI | 分布式(严格排序) |
| EdgeDB | 图-关系混合 | 对象-关系+图 | EdgeQL | 单机/分布式 |
| Couchbase | 文档+KV | 文档+KV+SQL++ | N1QL/Full-text | 单机/分布式/云 |
| InterSystems IRIS | 企业级多模型 | 文档/图/KV/SQL/向量 | SQL+ObjectScript | 企业部署 |

### C 部分 — README 更新

更新 `docs/db_wiki/11_multi_model/README.md`，新增内容：

1. **多模态数据库分类定义** — 区分"多模型"与"多模态"的概念差异
2. **演进历史** — 单模型（1970s-2000s）→ 多模型（2010s）→ 多模态融合（2020s）
3. **核心技术** — 查询语言统一、存储后端选择、数据模型映射、ACID 跨模型事务
4. **对比表** — 9 个库 × 15+ 维度（数据模型、查询语言、一致性、部署方式、扩展性、生态等）
5. **选型指南** — 按场景（云原生/嵌入式/Serverless/企业级）给出推荐
6. **学习路径** — 分 4 阶段：概念理解 → 选型决策 → 实践验证 → 架构设计

## 3. 结构

```
docs/db_wiki/11_multi_model/
├── README.md                          # 更新：分类概述 + 对比表 + 选型指南
├── surrealdb/                         # 已存在，扩展 6 文件
│   ├── 00_overview.md                 # 已有
│   ├── 01_architecture.md             # 新增
│   ├── 06_features.md                 # 新增
│   ├── 07_use_cases.md                # 新增
│   ├── 08_experiments.md              # 新增
│   ├── 09_resources.md                # 新增
│   └── 10_project_connection.md       # 新增
├── arangodb/                          # 新增 7 文件
├── azure_cosmosdb/                    # 新增 7 文件
├── orientdb/                          # 新增 7 文件
├── faunadb/                           # 新增 7 文件
├── foundationdb/                      # 新增 7 文件
├── edgedb/                            # 新增 7 文件
├── couchbase/                         # 新增 7 文件
└── iris/                              # 新增 7 文件
    （InterSystems IRIS）
```

## 4. 模板参考

每个文件格式遵循 `docs/db_wiki/00_template/01_relational/sample_db/` 模板：
- 学习目标（3-4 条）
- 核心概念（Mermaid 图）
- 主要内容（代码示例、架构图、对比表格）
- 要点总结（6-10 条）
- 思考题（3-5 道）

## 5. 工作量估算

| 模块 | 文件数 | 预估行数 |
|------|--------|---------|
| SurrealDB 扩展 | 6 | ~3000 |
| 8 个新库 × 7 文件 | 56 | ~28000 |
| README 更新 | 1 | ~500 |
| **合计** | **63** | **~31500** |

## 6. 实施顺序

1. 先更新 README.md（分类概述，作为整体框架）
2. 然后 ArangoDB（最成熟，覆盖多模型所有模式）
3. 然后 SurrealDB 扩展（已有基础，快速产出）
4. 然后 FoundationDB / EdgeDB（苹果/社区新锐）
5. 然后 OrientDB / Couchbase（经典，文档丰富）
6. 最后 Azure CosmosDB / FaunaDB / IRIS（云原生/企业级，部分需参考官网）

## 7. 后续建议

- 与项目 mm_storage.h 模块建立代码级交叉引用
- 涉及分布式事务的库（FoundationDB、CosmosDB）可与项目 dist_txn/及 Raft 模块对照