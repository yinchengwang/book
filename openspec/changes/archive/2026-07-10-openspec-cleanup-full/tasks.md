# S10 — Tasks (OpenSpec 全量架构整理)

> **目标**：把 `openspec/specs/` 从 102 个散乱目录重组为按双轨分类的子目录架构，删除完全废弃的 spec。

## 1.1 调研 + OpenSpec 提案

- [x] 1.1.1 已查：102 specs，8048 行总文档，平均 78 行/个
- [x] 1.1.2 已查：8 个 spec 完全废弃（4 个 build-my-db-* + 4 个 dl-* + practice-curriculum）
- [x] 1.1.3 已查：specs 与代码架构脱节（无 learning/engineering 分类）
- [x] 1.1.4 用 AskUserQuestion 用户选重量：按架构整理重全量清

## 1.2 Phase 1: 删除 8 个完全废弃 spec

- [ ] 1.2.1 `git rm -r openspec/specs/build-my-db-art-index/`  (82 行)
- [ ] 1.2.2 `git rm -r openspec/specs/build-my-db-kv-api/`  (119 行)
- [ ] 1.2.3 `git rm -r openspec/specs/build-my-db-storage-engine/`  (95 行)
- [ ] 1.2.4 `git rm -r openspec/specs/build-my-db-transaction-wal/`  (122 行)
- [ ] 1.2.5 `git rm -r openspec/specs/dl-content-classification/`  (47 行)
- [ ] 1.2.6 `git rm -r openspec/specs/dl-content-generation/`  (67 行)
- [ ] 1.2.7 `git rm -r openspec/specs/dl-content-integration/`  (59 行)
- [ ] 1.2.8 `git rm -r openspec/specs/practice-curriculum/`  (89 行)

## 1.3 Phase 2: 重命名 + 子目录分类

- [ ] 1.3.1 创建 `openspec/specs/engineering/` 目录
- [ ] 1.3.2 创建 `openspec/specs/learning/` 目录
- [ ] 1.3.3 把 59 个工程层 spec 移到 `engineering/`：
  - db-* (12)、vdb-* (6)、vector-* (3)、vectorized-execution (1)
  - mm-* (1)、pgwire-protocol (1)、ivf-pq-engine (1)
  - mvcc (1)、row-level-lock (1)、sql-* (3)
  - cluster-coordinator (1)、distributed-transaction (1)、raft-ha (1)、sharding (1)
  - engine-concurrency (1)、rest-api (1)、jdbc-driver (1)
  - ts-* (3)、columnar-storage (1)、spatial-functions (1)
  - python-sdk (1)、cross-model-query (1)、tree-hierarchy (1)
  - doc-* (3)、backup-recovery (1)、monitoring-logging (1)
  - performance-benchmark (1)、health-check (1)、docker-deployment (1)
  - rag-integration (1)、graph-* (3)、kv-sorted-set (1)
  - data-item-registry (1)、apps-* (8)
  - 共 59 个
- [ ] 1.3.4 把 35 个学习层 spec 移到 `learning/`：
  - linux-* (6)、quiz-* (7)、mermaid2excalidraw-* (4)
  - illustrated-articles、illustrate-series、illustration-enhancement (3)
  - five-year-learning-layer、five-year-plan-integration (2)
  - learn-content-page、learning-page-redesign (2)
  - drawio-diagrams、database-content-organization (2)
  - markdown-content-rendering、readme-update (2)
  - adaptive-learning-roadmap、unified-navigation (2)
  - kanban-product-grouping、kanban-render-merge、items-db-vdb-migration (3)
  - h5-vite-build、multi-platform (2)
  - 共 35 个
- [ ] 1.3.5 创建 `openspec/specs/README.md`：说明新结构（engineering/learning 子目录）

## 1.4 验证 V1-V4

- [ ] 1.4.1 V1: `ls openspec/specs/` 输出 2 个目录（engineering/, learning/）
- [ ] 1.4.2 V2: 旧 spec 路径不再存在（grep 测试用例）
- [ ] 1.4.3 V3: `find openspec/specs -name spec.md | wc -l` = 94（102-8）
- [ ] 1.4.4 V4: 新路径有正确文件

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/specs/ openspec/changes/openspec-cleanup-full/`
- [ ] 1.5.2 `git commit -m "chore(openspec): 全量架构整理——删 8 废弃 spec，工程/学习子目录分类"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-openspec-cleanup-full/`
