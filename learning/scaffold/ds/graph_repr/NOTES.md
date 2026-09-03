# 工程对照说明

## 与 engineering/src/db/storage/ 的关联

### 图数据模型在数据库中的表示

在 `engineering/include/db/storage_engine.h` 中，定义了 `DataModel` 枚举，其中 `MODEL_GRAPH = 2` 表示图模型。图表示法（邻接矩阵 vs 邻接表）是数据库图引擎的核心存储决策。

### 两种表示法的工程权衡

| 维度 | 邻接矩阵 | 邻接表 |
|------|----------|--------|
| 空间 | O(V^2) 固定开销 | O(V+E) 按边扩展 |
| 边查询 | O(1) | O(deg) |
| 邻居遍历 | O(V) | O(deg) |
| 内存局部性 | 差（随机访问） | 好（链表顺序） |

### 工程实现参考

`engineering/src/db/graph/graph_algo.c` 中的图遍历算法（BFS/DFS）底层依赖邻接表结构：

```c
/* graph_algo.c 中的邻接边获取 */
graph_vertex_get_out_edges(iter->graph, vid, NULL, &out_edges, &out_count);

for (size_t i = 0; i < out_count; i++) {
    graph_edge_t *edge = NULL;
    graph_edge_get(iter->graph, out_edges[i], &edge);
    // ...
}
```

这与本 scaffold 中邻接表的核心操作完全对应：

```c
/* 本模块的邻接表遍历 */
AdjEdge *e = adj[i].head;
while (e) {
    // 处理每条边 e->vertex, e->weight
    e = e->next;
}
```

### 数据库存储引擎的抽象层

`storage_engine.h` 定义的 `storage_ops_t` 函数表是引擎无关抽象：
- `table_create/drop` 对应图的创建/删除
- `tuple_insert/delete` 对应边的增删
- `scan_begin/next` 对应图的遍历接口

本 scaffold 聚焦于表示层，是理解数据库图引擎存储设计的基石。
