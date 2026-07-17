# Spec: docs-diagrams-vector

## ADDED Requirements

### Requirement: 向量索引与多模态存储图表文件结构

`docs/diagrams/level3-vector/` 目录 SHALL 包含以下 Excalidraw 图表文件：

- `L3-001-index-selection-tree.excalidraw.json` - 向量索引选择决策树
- `L3-002-hnsw-structure.excalidraw.json` - HNSW 层结构图
- `L3-003-hnsw-search-flow.excalidraw.json` - HNSW 搜索流程图
- `L3-004-ivf-pq-structure.excalidraw.json` - IVF+PQ 索引结构图
- `L3-005-diskann-arch.excalidraw.json` - DiskANN 架构图
- `L3-006-index-comparison.excalidraw.json` - 向量索引对比表
- `L3-007-mm-engine-routing.excalidraw.json` - 多模态引擎路由图
- `L3-008-cross-model-query-flow.excalidraw.json` - 跨模型联合查询流程图
- `L3-009-timeseries-agg.excalidraw.json` - 时序引擎聚合查询图
- `L3-010-rtree-structure.excalidraw.json` - R-Tree 空间索引结构图

#### Scenario: 目录结构完整性检查

- **WHEN** 用户在 `docs/diagrams/level3-vector/` 目录下列出所有文件
- **THEN** 应当包含上述十个 `.excalidraw.json` 文件

### Requirement: 向量索引选择决策树内容

`L3-001-index-selection-tree.excalidraw.json` SHALL 包含决策分支：

- 数据规模判断：小规模 → HNSW，中规模 → IVF+PQ，大规模 → DiskANN
- 精度要求：高精度 → HNSW，中精度 → IVF+PQ，低精度 → PQ-only
- 内存限制：内存充足 → HNSW，内存受限 → DiskANN

#### Scenario: 索引选择决策正确性

- **WHEN** 用户根据场景选择向量索引
- **THEN** 可以通过决策树快速定位合适的索引类型

### Requirement: HNSW 层结构图内容

`L3-002-hnsw-structure.excalidraw.json` SHALL 包含：

- 多层结构：Layer 0（底层全连接）、Layer 1、Layer 2、Layer 3（顶层稀疏）
- 搜索路径：顶层稀疏搜索 → 逐步向下 → 底层精确搜索
- 贪婪遍历算法示意

#### Scenario: HNSW 层结构清晰性

- **WHEN** 用户查看 HNSW 层结构图
- **THEN** 可以理解多层跳表的构造原理

### Requirement: HNSW 搜索流程图内容

`L3-003-hnsw-search-flow.excalidraw.json` SHALL 包含：

- 从顶层开始搜索
- 在当前层找最近邻
- 跳转到下一层
- 重复直到最后一层
- 返回最近邻结果

#### Scenario: HNSW 搜索流程完整性

- **WHEN** 用户查看 HNSW 搜索流程图
- **THEN** 可以理解从顶层到底层的搜索路径

### Requirement: IVF+PQ 索引结构图内容

`L3-004-ivf-pq-structure.excalidraw.json` SHALL 包含：

- 聚类中心点（Centroid）
- 倒排文件（每个中心关联一批向量）
- Product Quantization：向量分块 → 编码 → 压缩
- 查询时先定位最近的聚类中心，再在聚类内搜索

#### Scenario: IVF+PQ 结构清晰性

- **WHEN** 用户查看 IVF+PQ 结构图
- **THEN** 可以理解倒排索引和乘积量化的原理

### Requirement: DiskANN 架构图内容

`L3-005-diskann-arch.excalidraw.json` SHALL 包含：

- Vamana 图（SSD 友好）
- Beam Search 机制
- 顶点排序优化 I/O
- 与 HNSW 的区别（内存 vs 磁盘）

#### Scenario: DiskANN 架构完整性

- **WHEN** 用户查看 DiskANN 架构图
- **THEN** 可以理解 DiskANN 的设计目标和架构

### Requirement: 向量索引对比表内容

`L3-006-index-comparison.excalidraw.json` SHALL 包含对比维度：

- 查询 QPS
- 内存占用
- 构建速度
- 适用场景
- 索引类型：HNSW、IVF+PQ、DiskANN、BM25

#### Scenario: 索引对比完整性

- **WHEN** 用户查看向量索引对比表
- **THEN** 可以快速对比各索引类型的特点

### Requirement: 多模态引擎路由图内容

`L3-007-mm-engine-routing.excalidraw.json` SHALL 包含：

- 引擎路由器根据数据类型选择
- KV 引擎（kv_engine）
- Vector 引擎（vector_engine）
- Timeseries 引擎（ts_engine）
- Document 引擎（doc_engine）
- Spatial 引擎（spatial_engine）
- Graph 引擎（graph_engine）

#### Scenario: 引擎路由完整性

- **WHEN** 用户查看多模态引擎路由图
- **THEN** 可以理解各引擎的适用场景

### Requirement: 跨模型联合查询流程图内容

`L3-008-cross-model-query-flow.excalidraw.json` SHALL 包含：

- SQL 示例：向量相似 + 条件过滤 + 空间查询
- 查询规划器分解查询
- 各子查询路由到对应索引
- 候选集合并
- 返回最终结果

#### Scenario: 跨模型查询流程完整性

- **WHEN** 用户查看跨模型联合查询流程图
- **THEN** 可以理解复杂查询的执行流程

### Requirement: 时序引擎聚合查询图内容

`L3-009-timeseries-agg.excalidraw.json` SHALL 包含：

- 时间分区设计
- 聚合函数：COUNT、SUM、AVG、MAX、MIN
- GROUP BY 时间窗口
- 降采样（Downsampling）
- 保留策略（Retention Policy）

#### Scenario: 时序聚合查询完整性

- **WHEN** 用户查看时序引擎聚合查询图
- **THEN** 可以理解时序数据的聚合查询模式

### Requirement: R-Tree 空间索引结构图内容

`L3-010-rtree-structure.excalidraw.json` SHALL 包含：

- R-Tree 树形结构
- 最小边界矩形（MBR）
- 节点分裂算法
- 空间查询：Point Query、Range Query、KNN Query

#### Scenario: R-Tree 结构清晰性

- **WHEN** 用户查看 R-Tree 结构图
- **THEN** 可以理解 R-Tree 的索引原理
