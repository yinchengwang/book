# 工程对照说明

## 与 engineering/src/db/ 的关联

### 编译器优化：循环展开与图遍历

在 `engineering/src/db/graph/graph_algo.c` 中，图遍历算法的性能与编译器优化密切相关。

**内联优化示例：**

```c
// graph_algo.c 中的边遍历（可能被内联）
static inline void graph_traverse_edges(Graph *g, int u,
                                        void (*callback)(int v, void *ctx), void *ctx)
{
    for (Edge *e = g->adj[u]; e; e = e->next) {
        callback(e->to, ctx);
    }
}
```

GCC 使用 `-O2` 时会：
1. 自动内联此函数
2. 将 `callback(e->to, ctx)` 转换为直接函数调用
3. 启用 `restrict` 指针优化

### 拓扑排序与依赖图

在数据库查询优化器中，拓扑排序用于解析执行计划中的依赖关系：

```c
// query_planner.c 中的依赖排序
typedef struct {
    int node_id;
    int dependency_count;
    int dependencies[MAX_DEPS];
} PlanNode;

int topo_sort(PlanNode *nodes, int n, int *order)
{
    int in_degree[MAX_NODES] = {0};
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < nodes[i].dependency_count; j++) {
            in_degree[nodes[i].dependency_count]++;
        }
    }
    // Kahn 算法...
}
```

这与本模块 SCC 算法中的入度计数思路一致：
- SCC 的 `in_degree[]` 统计边数
- 拓扑排序的 `in_degree[]` 统计依赖数
- 两者都使用队列驱动的 BFS 模式

### Tarjan SCC 的工程应用

1. **循环检测**：数据库存储过程中可能存在循环依赖
2. **缩点优化**：将 SCC 缩点后形成 DAG，可进行更高效的批量处理
3. **死锁检测**：强连通分量中任意顶点可达其他顶点，存在循环等待风险

### 欧拉回路的工程应用

1. **布隆过滤器遍历**：使用欧拉路径优化位图扫描
2. **电路板布线**：确保每条线路恰好经过一次
3. **物流路径规划**：车辆经过所有检查点的最优路线

### 性能优化建议

```makefile
# 启用更多优化
CFLAGS = -Wall -Wextra -O3 -march=native -funroll-loops
```

- `-O3`：启用更多循环展开
- `-march=native`：针对本机 CPU 指令集优化
- `-funroll-loops`：对已知迭代次数的循环展开
