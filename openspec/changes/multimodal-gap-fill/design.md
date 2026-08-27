# P1-4 图分析算法库 设计文档

## 架构设计

### 模块结构

```
graph_algorithms.h (公共 API)
       ↓
graph_algorithms.c (实现)
       ↓
graph_engine.h (图存储引擎接口)
```

### API 设计

```c
/* 最短路径结果 */
typedef struct {
    int64_t *path;        /* 路径顶点 ID 数组 */
    size_t path_length;   /* 路径长度（顶点数） */
    double total_cost;    /* 总代价 */
} graph_path_result_t;

/* Dijkstra 单源最短路径 */
int graph_dijkstra(
    const graph_engine_t *graph,
    int64_t source,
    int64_t target,
    graph_path_result_t *result);

/* BFS 无权图最短路径 */
int graph_bfs_shortest_path(
    const graph_engine_t *graph,
    int64_t source,
    int64_t target,
    graph_path_result_t *result);

/* PageRank 迭代计算 */
int graph_pagerank(
    const graph_engine_t *graph,
    double *scores,           /* 输出：每个顶点的 PageRank 分数 */
    size_t num_vertices,
    int max_iterations,
    double damping_factor,
    double tolerance);

/* 连通分量检测 */
int graph_connected_components(
    const graph_engine_t *graph,
    int64_t *component_ids,   /* 输出：每个顶点所属连通分量 ID */
    size_t *num_components);  /* 输出：连通分量数量 */

/* 图统计 */
int graph_stats(
    const graph_engine_t *graph,
    size_t *num_vertices,
    size_t *num_edges,
    double *avg_degree,
    double *clustering_coefficient);
```

### 算法实现要点

1. **Dijkstra 算法**
   - 使用最小堆优化，时间复杂度 O((V+E) log V)
   - 支持带权图（边权重 ≥ 0）
   - 返回完整路径和总代价

2. **BFS 算法**
   - 使用队列实现，时间复杂度 O(V+E)
   - 适用于无权图或等权图
   - 记录前驱节点以重建路径

3. **PageRank 算法**
   - 迭代计算：PR(v) = (1-d)/N + d * Σ(PR(u)/deg(u))
   - 支持阻尼因子（默认 0.85）
   - 收敛条件：迭代次数上限或分数变化 < tolerance

4. **连通分量**
   - 使用 Union-Find 数据结构
   - 或 BFS/DFS 遍历标记

### 数据结构

```c
/* 最小堆（用于 Dijkstra） */
typedef struct {
    int64_t vertex;
    double priority;
} graph_heap_node_t;

/* 访问标记数组 */
typedef struct {
    bool *visited;
    int64_t *parent;
    double *distance;
} graph_traversal_state_t;
```

### 内存管理

- 所有结果结构体由调用者分配（栈或堆）
- 算法函数填充结果字段，不分配内存
- `graph_path_result_t.path` 需要调用者分配足够大小的数组

### 错误处理

- 返回 0 表示成功，非 0 表示失败
- 常见错误码：
  - `-1`：参数无效（NULL 指针）
  - `-2`：源/目标顶点不存在
  - `-3`：路径不存在（不可达）
  - `-4`：内存不足