# 最短路径算法 (Shortest Path)

## 算法概述

最短路径问题是图论中的核心问题：在带权图中找到两个顶点之间代价最小的路径。

## 核心算法

| 算法 | 时间复杂度 | 特点 |
|------|-----------|------|
| Dijkstra | O(V²) / O(E log V) | 非负权边，最常用 |
| Bellman-Ford | O(VE) | 支持负权，可检测负环 |
| SPFA | 期望 O(E) | Bellman-Ford 队列优化 |
| Floyd-Warshall | O(V³) | 全源最短路 |

## 工程对照

### 路由协议
- OSPF（Open Shortest Path First）：基于 Dijkstra 计算最短路径树
- RIP（Routing Information Protocol）：基于 Bellman-Ford 距离向量
- BGP 路径属性中的 MED（Multi-Exit Discriminator）类似最短路径权重

### GPS 导航
- 实时路况使用动态权重 Dijkstra 或 A* 启发式搜索
- 多目的地使用 Floyd-Warshall 预计算距离矩阵

### 网络流
- 最大流问题中利用最短增广路径（Edmonds-Karp = BFS 最短路）
- 费用流使用连续最短路径（SSP）每次找最小费用路径

### 数据库查询优化
- Join 顺序选择类似最短路径问题，使用动态规划
- 分布式数据库中查询计划代价估算参考路径长度模型

## 参考

- `engineering/src/algo/graph.c` — 图论算法实现
- `learning/scaffold/ds/graph_repr/` — 图的表示方法
