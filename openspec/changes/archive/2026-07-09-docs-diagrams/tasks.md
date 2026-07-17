# Tasks: 文档图表体系实施

## 1. 项目准备

- [x] 1.1 创建 `docs/diagrams/` 目录结构
- [x] 1.2 创建 level0-project/、level1-distributed/、level2-storage/、level3-vector/、level4-redis-algo/ 子目录
- [x] 1.3 创建 `_sketches/` 草图目录

## 2. Level 0: 项目级图表 (P0)

- [x] 2.1 创建 L0-001-project-overview.excalidraw.json（项目全景地图）
- [x] 2.2 创建 L0-002-module-dependency.excalidraw.json（模块依赖关系图）
- [x] 2.3 创建 L0-003-learning-path.excalidraw.json（学习路线图）

### Mermaid 源文件

- [x] 2.4 生成 level0-project/L0-001-project-overview.md（项目全景地图）
- [x] 2.5 生成 level0-project/L0-002-module-dependency.md（模块依赖关系图）
- [x] 2.6 生成 level0-project/L0-003-learning-path.md（学习路线图）

## 3. Level 1: Phase 9 分布式模块图表

### P0: 核心必做

- [x] 3.1 创建 L1-003-shard-routing-flow.excalidraw.json（分片路由流程图）
- [x] 3.2 创建 L1-004-raft-state-machine.excalidraw.json（Raft 状态机图）
- [x] 3.3 创建 L1-007-2pc-transaction-flow.excalidraw.json（2PC 事务流程图）

### P1: 重要支撑

- [x] 3.4 创建 L1-001-distributed-overview.excalidraw.json（分布式模块全景图）
- [x] 3.5 创建 L1-002-data-structures.excalidraw.json（核心数据结构关系图）
- [x] 3.6 创建 L1-005-raft-replication-seq.excalidraw.json（Raft 日志复制时序图）
- [x] 3.7 创建 L1-006-raft-election-seq.excalidraw.json（Raft 领导者选举时序图）
- [x] 3.8 创建 L1-008-2pc-recovery-flow.excalidraw.json（2PC 故障恢复流程图）
- [x] 3.9 创建 L1-009-coordinator-arch.excalidraw.json（节点协调器架构图）
- [x] 3.10 创建 L1-010-coordinator-seq.excalidraw.json（节点协调流程时序图）

### Mermaid 源文件

- [x] 3.11 生成 level1-distributed/L1-001-distributed-overview.md（分布式模块全景图）
- [x] 3.12 生成 level1-distributed/L1-002-data-structures.md（核心数据结构关系图）
- [x] 3.13 生成 level1-distributed/L1-003-shard-routing-flow.md（分片路由流程图）
- [x] 3.14 生成 level1-distributed/L1-004-raft-state-machine.md（Raft 状态机图）
- [x] 3.15 生成 level1-distributed/L1-005-raft-replication-seq.md（Raft 日志复制时序图）
- [x] 3.16 生成 level1-distributed/L1-006-raft-election-seq.md（Raft 领导者选举时序图）
- [x] 3.17 生成 level1-distributed/L1-007-2pc-transaction-flow.md（2PC 事务流程图）
- [x] 3.18 生成 level1-distributed/L1-008-2pc-recovery-flow.md（2PC 故障恢复流程图）
- [x] 3.19 生成 level1-distributed/L1-009-coordinator-arch.md（节点协调器架构图）
- [x] 3.20 生成 level1-distributed/L1-010-coordinator-seq.md（节点协调流程时序图）

## 4. Level 2: 存储引擎图表

### P0: 核心必做

- [x] 4.1 创建 L2-002-buffer-pool-flow.excalidraw.json（Buffer Pool 置换流程图）

### P1: 重要支撑

- [x] 4.2 创建 L2-001-storage-overview.excalidraw.json（存储引擎全景图）
- [x] 4.3 创建 L2-004-heap-page-structure.excalidraw.json（Heap Page 结构图）
- [x] 4.4 创建 L2-005-btree-split-flow.excalidraw.json（BTree 页面分裂流程图）
- [x] 4.5 创建 L2-006-wal-format.excalidraw.json（WAL 日志格式图）

### P2: 完善补充

- [x] 4.6 创建 L2-003-page-lifecycle.excalidraw.json（页面生命周期图）
- [x] 4.7 创建 L2-007-checkpoint-flow.excalidraw.json（检查点流程图）
- [x] 4.8 创建 L2-008-catalog-structure.excalidraw.json（Catalog 系统结构图）
- [x] 4.9 创建 L2-009-tuple-operations.excalidraw.json（元组操作流程图）
- [x] 4.10 创建 L2-010-sql-execution-flow.excalidraw.json（SQL 执行流程图）

### Mermaid 源文件

- [x] 4.11 生成 level2-storage/L2-001-storage-overview.md（存储引擎全景图）
- [x] 4.12 生成 level2-storage/L2-002-buffer-pool-flow.md（Buffer Pool 置换流程图）
- [x] 4.13 生成 level2-storage/L2-003-page-lifecycle.md（页面生命周期图）
- [x] 4.14 生成 level2-storage/L2-004-heap-page-structure.md（Heap Page 结构图）
- [x] 4.15 生成 level2-storage/L2-005-btree-split-flow.md（BTree 页面分裂流程图）
- [x] 4.16 生成 level2-storage/L2-006-wal-format.md（WAL 日志格式图）
- [x] 4.17 生成 level2-storage/L2-007-checkpoint-flow.md（检查点流程图）
- [x] 4.18 生成 level2-storage/L2-008-catalog-structure.md（Catalog 系统结构图）
- [x] 4.19 生成 level2-storage/L2-009-tuple-operations.md（元组操作流程图）
- [x] 4.20 生成 level2-storage/L2-010-sql-execution-flow.md（SQL 执行流程图）

## 5. Level 3: 向量索引图表

### P0: 核心必做

- [x] 5.1 创建 L3-001-index-selection-tree.excalidraw.json（向量索引选择决策树）
- [x] 5.2 创建 L3-002-hnsw-structure.excalidraw.json（HNSW 层结构图）
- [x] 5.3 创建 L3-003-hnsw-search-flow.excalidraw.json（HNSW 搜索流程图）

### P1: 重要支撑

- [x] 5.4 创建 L3-004-ivf-pq-structure.excalidraw.json（IVF+PQ 索引结构图）
- [x] 5.5 创建 L3-006-index-comparison.excalidraw.json（向量索引对比表）
- [x] 5.6 创建 L3-007-mm-engine-routing.excalidraw.json（多模态引擎路由图）

### P2: 完善补充

- [x] 5.7 创建 L3-005-diskann-arch.excalidraw.json（DiskANN 架构图）
- [x] 5.8 创建 L3-008-cross-model-query-flow.excalidraw.json（跨模型联合查询流程图）
- [x] 5.9 创建 L3-009-timeseries-agg.excalidraw.json（时序引擎聚合查询图）
- [x] 5.10 创建 L3-010-rtree-structure.excalidraw.json（R-Tree 空间索引结构图）

### Mermaid 源文件

- [x] 5.11 生成 level3-vector/L3-001-index-selection-tree.md（向量索引选择决策树）
- [x] 5.12 生成 level3-vector/L3-002-hnsw-structure.md（HNSW 层结构图）
- [x] 5.13 生成 level3-vector/L3-003-hnsw-search-flow.md（HNSW 搜索流程图）
- [x] 5.14 生成 level3-vector/L3-004-ivf-pq-structure.md（IVF+PQ 索引结构图）
- [x] 5.15 生成 level3-vector/L3-005-diskann-arch.md（DiskANN 架构图）
- [x] 5.16 生成 level3-vector/L3-006-index-comparison.md（向量索引对比表）
- [x] 5.17 生成 level3-vector/L3-007-mm-engine-routing.md（多模态引擎路由图）
- [x] 5.18 生成 level3-vector/L3-008-cross-model-query-flow.md（跨模型联合查询流程图）
- [x] 5.19 生成 level3-vector/L3-009-timeseries-agg.md（时序引擎聚合查询图）
- [x] 5.20 生成 level3-vector/L3-010-rtree-structure.md（R-Tree 空间索引结构图）

## 6. Level 4: Redis 与算法图表

### P1: 重要支撑

- [x] 6.1 创建 L4-001-redis-object-system.excalidraw.json（Redis 对象系统图）
- [x] 6.2 创建 L4-003-skiplist-structure.excalidraw.json（跳表结构图）
- [x] 6.3 创建 L4-004-redis-persistence-flow.excalidraw.json（Redis 持久化流程图）
- [x] 6.4 创建 L4-005-algo-taxonomy.excalidraw.json（算法分类全景图）
- [x] 6.5 创建 L4-006-sorting-comparison.excalidraw.json（排序算法对比表）
- [x] 6.6 创建 L4-007-ds-selection-tree.excalidraw.json（数据结构选择决策树）

### P2: 完善补充

- [x] 6.7 创建 L4-002-sds-structure.excalidraw.json（SDS 数据结构图）
- [x] 6.8 创建 L4-008-kmp-flow.excalidraw.json（KMP 匹配流程图）
- [x] 6.9 创建 L4-009-kmeans-flow.excalidraw.json（K-Means 聚类流程图）
- [x] 6.10 创建 L4-010-quantization.excalidraw.json（量化压缩原理图）

### Mermaid 源文件

- [x] 6.11 生成 level4-redis-algo/L4-001-redis-object-system.md（Redis 对象系统图）
- [x] 6.12 生成 level4-redis-algo/L4-002-sds-structure.md（SDS 数据结构图）
- [x] 6.13 生成 level4-redis-algo/L4-003-skiplist-structure.md（跳表结构图）
- [x] 6.14 生成 level4-redis-algo/L4-004-redis-persistence-flow.md（Redis 持久化流程图）
- [x] 6.15 生成 level4-redis-algo/L4-005-algo-taxonomy.md（算法分类全景图）
- [x] 6.16 生成 level4-redis-algo/L4-006-sorting-comparison.md（排序算法对比表）
- [x] 6.17 生成 level4-redis-algo/L4-007-ds-selection-tree.md（数据结构选择决策树）
- [x] 6.18 生成 level4-redis-algo/L4-008-kmp-flow.md（KMP 匹配流程图）
- [x] 6.19 生成 level4-redis-algo/L4-009-kmeans-flow.md（K-Means 聚类流程图）
- [x] 6.20 生成 level4-redis-algo/L4-010-quantization.md（量化压缩原理图）

## 7. 工具优化

- [ ] 7.1 优化 mermaid2excalidraw 工具生成 Excalidraw JSON
- [ ] 7.2 支持更多 Mermaid 语法（subgraph、stateDiagram、sequenceDiagram）
- [ ] 7.3 优化布局算法，提升生成效果

## 8. 批量生成 Excalidraw

- [x] 8.1 批量转换 level0-project/*.md → Excalidraw（3个）
- [x] 8.2 批量转换 level1-distributed/*.md → Excalidraw（10个）
- [x] 8.3 批量转换 level2-storage/*.md → Excalidraw（10个）
- [x] 8.4 批量转换 level3-vector/*.md → Excalidraw（10个）
- [x] 8.5 批量转换 level4-redis-algo/*.md → Excalidraw（10个）
