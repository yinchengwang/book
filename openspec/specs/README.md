# OpenSpec 规格组织

> **本目录是 OpenSpec 治理的根**：所有归档后的规格（spec）按双轨架构分类。
>
> **变更中的规格**：见 `../../changes/<change-name>/specs/<capability>/spec.md`

## 子目录结构

- **`engineering/`**（59 个）：工程层规格——描述 `engineering/src/<子系统>/` 或 `engineering/apps/<模块>/` 的能力
  - 数据库子系统：`db-storage-engine/`, `db-redis/`, `db-index-query/` 等
  - 向量索引：`vector-diskann/`, `vector-hybrid-search/`, `vdb-core-index-algorithms/` 等
  - 并发/事务：`mvcc/`, `raft-ha/`, `sharding/`, `distributed-transaction/` 等
  - SQL 子系统：`sql-parser/`, `sql-planner/`, `sql-executor/`
  - 独立应用：`apps-games-2048/`, `apps-common-menu/` 等
  - CLI/API：`rest-api/`, `python-sdk/`, `jdbc-driver/`

- **`learning/`**（35 个）：学习层规格——描述 `learning/notes/<主题>/`、`learning/code-solutions/<模块>/`、`apps/web/knowledge_hub/<功能>/`
  - 学习路线图：`adaptive-learning-roadmap/`, `five-year-plan-integration/`
  - Linux 知识栈：`linux-kernel-subsystems/`, `linux-networking/`, `linux-observability/` 等
  - Quiz 测验系统：`quiz-system-navigation/`, `quiz-bank-expansion/` 等
  - Mermaid2Excalidraw：`mermaid2excalidraw-cli/`、`generator/`、`layout/`、`parser/`
  - 笔记渲染与展示：`markdown-content-rendering/`, `drawio-diagrams/`

## 命名约定

- spec 名称 kebab-case，与 `src/` 目录树同构：`db-storage-engine` 对应 `engineering/src/db/storage/`
- 每个 spec 含 1 个 `spec.md`（OpenSpec 声明式格式：Purpose + Requirements + Scenarios）
- 单个 spec.md 不少于 30 行（少于视为空泛，应通过 change 流程丰富）

## 添加新规格流程

1. 在 `openspec/changes/<change-name>/specs/<capability>/spec.md` 起草新规格
2. 归档时（`openspec/changes/archive/<date>-<change>/`）由 Claude 自动移到 `openspec/specs/{engineering,learning}/<capability>/spec.md`
3. 使用 OpenSpec 模板：Purpose / Requirements / Scenario 三段式

## 删除规格流程

1. 在 change/archive 中标记为 REMOVED
2. `git rm -r openspec/specs/<capability>/`
3. 在本 README 末尾 ARCHIVED 段添加删除原因

## ARCHIVED（已移除）

| 规格 | 移除时间 | 原因 |
|---|---|---|
| `build-my-db-art-index` | 2026-07-10 | S1 之前的 mini-DB 项目已迁为 `engineering/src/db/storage/art.c`，原路径已废弃 |
| `build-my-db-kv-api` | 2026-07-10 | 同上 |
| `build-my-db-storage-engine` | 2026-07-10 | 同上 |
| `build-my-db-transaction-wal` | 2026-07-10 | 同上 |
| `dl-content-classification` | 2026-07-10 | 与代码架构脱节，应为笔记而非 OpenSpec 规格 |
| `dl-content-generation` | 2026-07-10 | 同上 |
| `dl-content-integration` | 2026-07-10 | 同上 |
| `practice-curriculum` | 2026-07-10 | 被 S2/S3 (学习层建设) 取代 |

## 统计

- 工程层规格：59 个
- 学习层规格：35 个
- 已归档：8 个
- 当前总共可见：**94 个**
