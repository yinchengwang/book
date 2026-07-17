# S10 — 设计文档（OpenSpec 全量架构整理）

## 1. 新分类结构

```
openspec/specs/
├── README.md           (新：说明规格组织策略)
├── engineering/        (新：工程层规格——59 个)
│   ├── apps-2048/
│   ├── apps-common-menu/
│   ├── apps-common-terminal/
│   ├── apps-games-mini-program/
│   ├── apps-games-snake/
│   ├── apps-games-sudoku/
│   ├── apps-web-knowledge-hub/
│   ├── apps-wechat-mini-program/
│   ├── backup-recovery/
│   ├── cluster-coordinator/
│   ├── columnar-storage/
│   ├── cross-model-query/
│   ├── data-item-registry/
│   ├── db-distributed-sqlite/
│   ├── db-distributed-vector/
│   ├── db-index-query/
│   ├── db-redis/
│   ├── db-storage-engine/
│   ├── db-transaction-recovery/
│   ├── db-vdb-stack-split/
│   ├── distributed-transaction/
│   ├── doc-aggregations/
│   ├── doc-nested/
│   ├── doc-vector-search/
│   ├── docker-deployment/
│   ├── engine-concurrency/
│   ├── engine-concurrency/
│   ├── graph-algorithms/
│   ├── graph-property/
│   ├── graph-query/
│   ├── health-check/
│   ├── ivf-pq-engine/
│   ├── jdbc-driver/
│   ├── kv-sorted-set/
│   ├── mm-pool-api/
│   ├── monitoring-logging/
│   ├── mvcc/
│   ├── performance-benchmark/
│   ├── pgwire-protocol/
│   ├── python-sdk/
│   ├── raft-ha/
│   ├── rag-integration/
│   ├── rest-api/
│   ├── row-level-lock/
│   ├── sharding/
│   ├── spatial-functions/
│   ├── sql-executor/
│   ├── sql-parser/
│   ├── sql-planner/
│   ├── tree-hierarchy/
│   ├── ts-columnar/
│   ├── ts-continuous-agg/
│   ├── ts-functions/
│   ├── vdb-core-index-algorithms/
│   ├── vdb-engineering/
│   ├── vdb-quantization/
│   ├── vector-database-illustrate/
│   ├── vector-diskann/
│   ├── vector-hybrid-search/
│   └── vectorized-execution/
└── learning/        (新：学习层规格——35 个)
    ├── adaptive-learning-roadmap/
    ├── database-content-organization/
    ├── drawio-diagrams/
    ├── five-year-learning-layer/
    ├── five-year-plan-integration/
    ├── h5-vite-build/
    ├── illustrate-series/
    ├── illustrated-articles/
    ├── illustration-enhancement/
    ├── items-db-vdb-migration/
    ├── kanban-product-grouping/
    ├── kanban-render-merge/
    ├── learn-content-page/
    ├── learning-page-redesign/
    ├── linux-kernel-subsystems/
    ├── linux-networking/
    ├── linux-observability/
    ├── linux-quiz-content/
    ├── linux-storage-filesystem/
    ├── linux-tech-stack/
    ├── markdown-content-rendering/
    ├── mermaid2excalidraw-cli/
    ├── mermaid2excalidraw-generator/
    ├── mermaid2excalidraw-layout/
    ├── mermaid2excalidraw-parser/
    ├── multi-platform/
    ├── quiz-bank-expansion/
    ├── quiz-file-by-quadrant/
    ├── quiz-file-quadrant-split/
    ├── quiz-product-file-org/
    ├── quiz-quadrant-field-fix/
    ├── quiz-system-navigation/
    ├── quiz-system-split/
    ├── readme-update/
    └── unified-navigation/
```

## 2. 分类规则

每个 spec 归属下列其一：

**engineering/**：核心特征是规格描述 `engineering/src/db/...` 或 `engineering/apps/...` 的具体工程模块
- 数据库子系统（db-）
- 向量索引（vector-*、vdb-*、mm-*、pgwire-protocol、ivf-pq-engine）
- 并发/事务（mvcc、raft-ha、sharding、distributed-transaction 等）
- SQL 执行器/解析器/规划器
- 独立 apps

**learning/**：核心特征是规格描述 `learning/notes/...` 或 `learning/code-solutions/...` 或 `apps/web/knowledge_hub/...`
- 学习路线图
- Linux 知识栈
- Quiz 测验系统
- Mermaid2Excalidraw 转换器
- Drawio 图表
- 笔记渲染
- 看板与导航

**deleted/**：完全废弃（已迁入根目录或与代码脱节）

## 3. 删除清单 (Phase 1)

```
build-my-db-art-index      (历史 mini-DB 项目)
build-my-db-kv-api
build-my-db-storage-engine
build-my-db-transaction-wal
dl-content-classification  (与代码架构脱节)
dl-content-generation
dl-content-integration
practice-curriculum        (被 S2/S3 取代)
```

## 4. README.md 内容草稿

```markdown
# OpenSpec 规格组织

## 子目录结构

- `engineering/`：工程层（PG 风格数据库、MiniVecDB、独立 apps、CLI 工具、API 服务器）
- `learning/`：学习层（学习路线图、Linux 知识栈、Quiz 系统、Mermaid 转换、笔记渲染、双端导航）

## 命名约定

- 与 src/ 目录树同构：`db-storage-engine` 对应 `engineering/src/db/storage_engine/`
- 每个 spec 必须有 1 个 spec.md（OpenSpec 声明式格式）
- 每个 spec 至少 30 行（少于 30 行视为空泛，需要 content 填充）

## 添加新 spec 流程

1. 在 `openspec/changes/<change-name>/specs/<capability>/spec.md` 写新规格
2. 归档时移到 `openspec/specs/{engineering,learning}/<capability>/spec.md`
```

## 5. 风险

| 风险 | 概率 | 缓解 |
|---|---|---|
| 删除 spec 在 change archive 中被引用 | 低 | grep change 文件 |
| 子目录 git 移动破坏 Windows 路径 | 低 | `git mv` 处理；README 提供 fallback 路径 |
| 用户偏好把 spec 分类按其他维度 | 中 | 主分类（双轨），子目录可后续微调 |
