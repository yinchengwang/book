# S10 — OpenSpec 全量架构整理

## What Changes

`openspec/specs/` 当前 **102 个独立 spec 目录**，平均 78 行/个，总 8048 行文档。这些 spec 是 OpenSpec 治理的基础设施，但当前**没有按双轨架构（learning/engineering）分类**，且混含**已被取代的旧 change 残留**（build-my-db-*, dl-content-*）。

按架构重新组织：

**Phase 1 — 删 8 个完全废弃的 spec**

| Spec | 行数 | 原因 |
|---|---|---|
| build-my-db-art-index | 82 | S1 之前的 mini-DB 项目已迁移为 db/storage/ 真实代码，"build-my-db" 路径语义不存在 |
| build-my-db-kv-api | 119 | 同上 |
| build-my-db-storage-engine | 95 | 同上 |
| build-my-db-transaction-wal | 122 | 同上 |
| dl-content-classification | 47 | 是 `notes/learning-roadmap.md` 的 OpenSpec 误迁移版本，应是文档而非规格 |
| dl-content-generation | 67 | 同上 |
| dl-content-integration | 59 | 同上 |
| practice-curriculum | 89 | 与代码架构脱节，已被 S2/S3 取代 |

**Phase 2 — 重命名 + 子目录分类**

把所有 spec 按双轨重新分到子目录：

- `openspec/specs/engineering/`（工程层规格，72 个）
  - `db/`（数据库内核）
  - `vector_index/`（向量索引）
  - `algo/`（algo-prod 工程算法）
  - `storage/`（存储层）
  - `apps/`（独立应用）
- `openspec/specs/learning/`（学习层规格，22 个）
  - `ds/`（数据结构）
  - `algo/`（学习算法）
  - `notes/`（学习笔记规格）
  - `quiz/`（测验系统）
- `openspec/specs/cross-cutting/`（跨轨规格，0 个，因为双轨严格隔离）

**Phase 3 — 删除 < 40 行的低价值 spec**（review 状态）

筛选出 50 行以下、内容空泛、与当前代码架构无关的 spec：
- 大量 quiz-* / mermaid2excalidraw-* / linux-quiz / linux-storage 等

## Why

**符合 CLAUDE.md OpenSpec 铁律**：
- tasks.md 是任务源
- 提案/设计/规格必须同步
- **规格(specs)必须反映当前代码状态**

**α 价值（工程作品集）**：
- 外部 review 看到 102 个 spec，但 8 个已废弃、数十个与代码无关——降低项目的可信度
- 整理后 spec 数 = 工程层实际模块数 ≈ 文档一致性

**β 价值（学习日志）**：
- 学习层 spec 单独分类后，能更好管理"学习轨迹"指标

**前置依赖**：
- S1-S9 已让工程层/学习层独立可编译 + 测试
- 双轨纪律确立（learning/engineering 互不引用）

## Scope

**包含**：
- 删除 8 个完全废弃 spec（Phase 1）
- 删除 ~12 个低价值 + 与当前架构脱节的 spec（Phase 3）
- Phase 2 重命名 + 移动到 2 个子目录（engineering/, learning/）
- 创建一个 openspec/specs/README.md 说明新的分类策略

**不包含**：
- ❌ 重写 spec 内容（仅删除/移动）
- ❌ 把 spec 加入 OpenSpec change 工作流（spec 是 should-be，change 才是 will-be）
- ❌ 引入新能力规格
- ❌ 把笔记 `notes/*` 迁到 OpenSpec

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 删除被某 change 引用 | 低 | grep change 文档确认无引用 |
| 重命名破坏外部链接 | 中 | grep commit history + 创建 README 映射 |
| 误删有效 spec | 中 | Phase 1 仅删 8 个已知废弃；Phase 3 限定低价值阈值 |

## 不做（明确范围外）

- ❌ 不动 OpenSpec change/archive 目录
- ❌ 不统一规格格式（保持现有声明式 Markdown）
- ❌ 不让规格与测试自动同步（保留 OpenSpec 半自动流程）
