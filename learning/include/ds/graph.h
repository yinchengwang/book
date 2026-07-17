/*
 * graph.h —— 图
 *
 * ============================================================
 * 概述
 * ============================================================
 * 图（Graph）由顶点（Vertex）集合和边（Edge）集合组成，是最通用的
 * 数据结构之一。几乎所有的关系网络都可以建模为图。
 *
 * 图的分类：
 * - 有向图 vs 无向图：边是否有方向
 * - 带权图 vs 无权图：边是否有权重
 * - 连通图 vs 非连通图：任意两点间是否都有路径
 * - 有环图 vs 无环图（DAG）：是否存在环路
 *
 * 图的存储方式：
 * - 邻接矩阵：graph[i][j] 表示 i→j 的边（或权重），O(V²) 空间
 * - 邻接表：每个顶点维护一个邻居链表，O(V+E) 空间，更常用
 *
 * ============================================================
 * 核心算法
 * ============================================================
 * | 算法       | 时间复杂度        | 说明                    |
 * |-----------|-----------------|------------------------|
 * | DFS       | O(V + E)        | 深度优先搜索（栈/递归）   |
 * | BFS       | O(V + E)        | 广度优先搜索（队列）      |
 * | 拓扑排序   | O(V + E)        | DAG 的线性排序           |
 * | Dijkstra  | O((V+E) log V)  | 单源最短路径（非负权）    |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 社交网络（好友关系）
 * - 地图导航（最短路径）
 * - 编译器（依赖图 → 拓扑排序）
 * - 网络路由（最短路径、最小生成树）
 * - 项目调度（AOE 网、关键路径）
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. DFS vs BFS 的选择？→ 找最短路径用 BFS，遍历所有可能用 DFS/回溯。
 * 2. 如何检测图中是否有环？→ 有向图：三色标记法或拓扑排序；无向图：并查集或 DFS。
 * 3. Dijkstra 为什么不能处理负权边？→ 贪心策略失效，已被"确定"的最短路径可能被负权边更新。
 */

#ifndef DS_GRAPH_H
#define DS_GRAPH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_graph ds_graph_t;

/*
 * 创建图。
 * vertices: 顶点数量
 * directed: true=有向图, false=无向图
 */
ds_graph_t *ds_graph_create(size_t vertices, bool directed);
void ds_graph_destroy(ds_graph_t *graph);

/* 添加边（带权重，无权图传 1.0） */
int ds_graph_add_edge(ds_graph_t *graph, size_t from, size_t to, double weight);

/* DFS 遍历（从 start 开始），visit 回调每个访问的顶点 */
typedef void (*ds_graph_visit_fn)(size_t vertex, void *user_data);
void ds_graph_dfs(const ds_graph_t *graph, size_t start, ds_graph_visit_fn visit, void *user_data);

/* BFS 遍历 */
void ds_graph_bfs(const ds_graph_t *graph, size_t start, ds_graph_visit_fn visit, void *user_data);

/* 拓扑排序（仅适用于 DAG），返回排序结果数组，结果长度在 out_len 中。调用者需 free */
size_t *ds_graph_topological_sort(const ds_graph_t *graph, size_t *out_len);

/* 演示函数 */
void ds_graph_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_GRAPH_H */
