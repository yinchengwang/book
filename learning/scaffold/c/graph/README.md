# graph scaffold

图四件套：邻接矩阵/邻接表、DFS/BFS 遍历、Dijkstra 最短路径、Prim 最小生成树。

## 复现命令

```bash
cd learning/scaffold/graph
gcc -Wall -Wextra -O2 -std=c11 -o graph_demo main.c
./graph_demo
```

## 关键点

- **邻接矩阵 vs 邻接表**：矩阵 O(V²) 空间适合稠密图；邻接表 O(V+E) 空间适合稀疏图（大多数现实图）
- **DFS**（栈/递归）：适合连通分量、拓扑排序、强连通分量；**BFS**（队列）：适合最短路径（无权）、层次遍历
- **Dijkstra**：O(V²) 朴素实现，每轮找最近未处理节点并松弛。负权边会出 bug（需 Bellman-Ford）
- **Prim**：与 Dijkstra 结构几乎一样——区别是 key[v] = 到 MST 的最小边权（而非到起点的最短距离）

详见 NOTES.md 工程对照段。
