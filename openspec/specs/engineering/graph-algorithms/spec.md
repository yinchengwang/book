# graph-algorithms Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 遍历算法

系统 SHALL 实现图遍历算法。

#### Scenario: BFS 遍历
- **WHEN** 执行 BFS
- **WHEN** 从起点开始
- **THEN** 按层次遍历图
- **THEN** 所有可达顶点 SHALL 被访问

#### Scenario: DFS 遍历
- **WHEN** 执行 DFS
- **WHEN** 从起点开始
- **THEN** 深度优先遍历图
- **THEN** 回溯 SHALL 被正确处理

---

### Requirement: 最短路径算法

系统 SHALL 实现最短路径算法。

#### Scenario: 单源最短路径 (Dijkstra)
- **WHEN** 执行 Dijkstra 算法
- **WHEN** 从源顶点到其他所有顶点
- **THEN** 最短路径 SHALL 被计算
- **THEN** 路径权重 SHALL 被最小化

#### Scenario: 带权边
- **WHEN** 边有权重
- **THEN** 权重 SHALL 被考虑
- **THEN** 加权最短路径 SHALL 被计算

#### Scenario: 无权最短路径 (BFS)
- **WHEN** 所有边权重相同
- **WHEN** 使用 BFS 计算最短路径
- **THEN** 跳数最少的路径 SHALL 被找到

---

### Requirement: 中心性算法

系统 SHALL 实现图的中心性算法。

#### Scenario: PageRank
- **WHEN** 执行 PageRank 算法
- **WHEN** 在整个图上
- **THEN** 每个顶点的 PageRank 值 SHALL 被计算
- **THEN** 重要性排序 SHALL 被生成

#### Scenario: 度中心性
- **WHEN** 计算度中心性
- **THEN** 每个顶点的度数 SHALL 被统计
- **THEN** 入度、出度、总度 SHALL 被区分

#### Scenario: 介数中心性
- **WHEN** 计算介数中心性
- **THEN** 经过每个顶点的最短路径数 SHALL 被统计

---

### Requirement: 社区检测算法

系统 SHALL 实现社区检测算法。

#### Scenario: Louvain 算法
- **WHEN** 执行 Louvain 社区检测
- **WHEN** 在整个图上
- **THEN** 社区 SHALL 被识别
- **THEN** 模块度 SHALL 被优化

#### Scenario: Label Propagation
- **WHEN** 执行标签传播算法
- **WHEN** 迭代传播标签
- **THEN** 社区 SHALL 被收敛
- **THEN** 计算效率 SHALL 被优化

---

### Requirement: 图算法 SQL 接口

系统 SHALL 提供图算法的 SQL 调用接口。

#### Scenario: PageRank 调用
- **WHEN** 执行 `SELECT GRAPH_PAGERANK('social', damping => 0.85, max_iter => 100)`
- **THEN** PageRank 结果 SHALL 被返回

#### Scenario: 算法结果存储
- **WHEN** 执行图算法
- **THEN** 结果 SHALL 可以被存储为顶点/边属性
- **THEN** 后续查询 SHALL 可以使用

#### Scenario: 算法参数配置
- **WHEN** 调用图算法
- **WHEN** 指定参数（如迭代次数、阈值）
- **THEN** 参数 SHALL 被正确传递
- **THEN** 算法 SHALL 使用指定参数执行

