# graph-analysis-algorithms Specification

## Purpose

定义 P1-4 图分析算法库的实现规格，包括最短路径、图遍历、中心性算法和社区检测等核心功能。

## ADDED Requirements

### Requirement: 最短路径算法

系统 SHALL 实现最短路径算法，支持无权图和带权图。

#### Scenario: Dijkstra 单源最短路径
- **WHEN** 调用 `graph_dijkstra(engine, source)` 对带权图执行 Dijkstra 算法
- **THEN** 从 source 到所有可达顶点的最短路径距离 SHALL 被计算
- **THEN** 返回值通过 `graph_path_result_t*` 输出参数传递

#### Scenario: BFS 无权最短路径
- **WHEN** 对无权图调用 `graph_bfs_shortest_path(engine, source, target)`
- **THEN** 从 source 到 target 的最少跳数路径 SHALL 被找到
- **THEN** 路径上的顶点列表 SHALL 被返回

---

### Requirement: 图遍历算法

系统 SHALL 实现图的遍历算法。

#### Scenario: BFS 广度优先遍历
- **WHEN** 调用 `graph_bfs_traverse(engine, start)` 执行 BFS
- **THEN** 按层次顺序访问所有可达顶点
- **THEN** 返回访问顺序的顶点 ID 数组

#### Scenario: DFS 深度优先遍历
- **WHEN** 调用 `graph_dfs_traverse(engine, start)` 执行 DFS
- **THEN** 深度优先顺序访问顶点
- **THEN** 支持递归和迭代两种实现

---

### Requirement: 中心性算法

系统 SHALL 实现图的中心性度量算法。

#### Scenario: PageRank 迭代计算
- **WHEN** 调用 `graph_pagerank(engine, damping, max_iter, tolerance)` 执行 PageRank
- **THEN** 每个顶点的 PageRank 值 SHALL 通过迭代计算收敛
- **THEN** 当相邻迭代的差异小于 tolerance 时 SHALL 停止迭代
- **THEN** 返回顶点 ID 到 PageRank 值的映射

#### Scenario: 度中心性计算
- **WHEN** 调用 `graph_degree_centrality(engine, vertex_id)`
- **THEN** 该顶点的入度、出度、总度 SHALL 被统计并返回

---

### Requirement: 社区检测算法

系统 SHALL 实现社区检测算法。

#### Scenario: 连通分量检测
- **WHEN** 调用 `graph_connected_components(engine)` 检测连通分量
- **THEN** 图中所有连通分量 SHALL 被识别
- **THEN** 每个顶点所属的社区 ID SHALL 被返回

---

### Requirement: 图统计函数

系统 SHALL 提供图的基本统计功能。

#### Scenario: 基本统计信息
- **WHEN** 调用 `graph_statistics(engine, &stats)`
- **THEN** 顶点数 SHALL 被返回
- **THEN** 边数 SHALL 被返回
- **THEN** 平均度数 SHALL 被计算并返回

#### Scenario: 聚类系数计算
- **WHEN** 调用 `graph_clustering_coefficient(engine, vertex_id)`
- **THEN** 该顶点的局部聚类系数 SHALL 被计算
- **THEN** 返回值范围为 [0.0, 1.0]

---

### Requirement: 算法结果结构

系统 SHALL 定义统一的算法结果返回结构。

#### Scenario: 路径结果
- **WHEN** 调用路径相关算法
- **THEN** 结果应包含路径上的顶点列表
- **THEN** 结果应包含路径总权重（对于带权图）

#### Scenario: 数值结果
- **WHEN** 调用 PageRank 或统计函数
- **THEN** 结果应包含顶点 ID 到数值的映射
- **THEN** 调用者负责释放返回的内存

---

### Requirement: API 头文件

系统 SHALL 提供规范的头文件接口。

#### Scenario: 公共 API 声明
- **WHEN** 包含 `graph_algorithms.h` 头文件
- **THEN** 所有公共 API 函数 SHALL 可用
- **THEN** 错误码定义 SHALL 可用

