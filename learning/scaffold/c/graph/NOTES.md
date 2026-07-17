# graph 学习笔记

## 概念地图

图比树多一条边——就是环——这带来了整个"已访问标记"机制和"松弛"思想：

- **邻接矩阵**：`g[i][j]` = 边权，0 表示无边。适合稠密图（`E ≈ V²`）和 Floyd-Warshall 全源最短路径。判断"i 和 j 是否相邻"O(1)
- **邻接表**：每个顶点一个链表头，存 `(to, weight)` 对。适合稀疏图（`E ≪ V²`），空间 O(V+E)。遍历某顶点的所有邻边 O(degree)——Dijkstra/Prim 的默认选择
- **DFS**：递归/栈。先深入后回溯。应用：拓扑排序、强连通分量（Kosaraju/Tarjan）、二分性检测
- **BFS**：队列。逐层扩散。应用：无权最短路径、二分图检测、网络流中的层次图构建（Dinic）
- **Dijkstra**：贪心——每轮选 dist 最小的未处理节点，松弛其邻边。不适用于负权边（贪心假设不再被更新，负边打破这个假设）
- **Prim**：贪心——每轮选连接 MST 的最小权边。与 Dijkstra 的区别仅在于松弛条件——Dijkstra 松弛距离、Prim 松弛到树的边权

与 `tree` 的关系：树是无环连通图。BST 的前/中/后序是树的 DFS，层序是树的 BFS

## 踩坑记录

1. **Dijkstra 负权边**：如果图中有负权边，Dijkstra 贪心会错误——因为选了 `u` 后不会再更新 `dist[u]`，但负边可能通过其它路径缩短 `dist[u]`。此时需要 Bellman-Ford 或 SPFA
2. **邻接矩阵无边表示**：0 可能同时表示"无边"和"权为 0 的边"——如果边权可以为 0，用 `INT_MAX` 或 -1 表示无边
3. **邻接表无向图双加**：无向图每条边在邻接表中存两次（`u→v` 和 `v→u`）——如果忘了，图变成有向的，遍历/最短路全错
4. **Prim 初始顶点**：可以从任意顶点开始——MST 总权值相同。本 demo 从 v0 开始。如果图不连通，Prim 只生成一个连通分量的 MST

## 工程对照（≥100 字硬约束）

图在工程侧的直接复用点：

1. **HNSW 图结构**：`rag/src/rag/index/hnsw_index.cpp` 中 HNSW 是分层有向图——每层是一个近似 k-NN 图。HNSW 搜索用贪心 BFS（beam search），插入用启发式选边策略（类似 Prim 的"选最短边"贪心）。本卡的 BFS 和邻接表遍历是读懂 HNSW `search_layer()` 的最小工具包
2. **查询优化器的 Join Graph**：`engineering/src/db/sql/sql_planner.c` 中多表 JOIN 的优化本质是"在 join graph 上找最优连接顺序"——顶点 = 表，边 = 可能的 JOIN 条件，边权 = 估计代价。Dijkstra/Prim 的贪心思想直接迁移到 join order 搜索
3. **依赖图与死锁检测**：`engineering/src/db/storage/lock/` 中死锁检测构建"等待图"（wait-for graph）——事务 A 等事务 B 的锁 = 边 A→B，检测有向环（DFS + 回溯）即为死锁。本卡的 DFS 遍历是死锁检测的原型
4. **WAL 恢复的依赖分析**：`engineering/src/db/storage/wal/wal_buf.c` 中恢复窗口内的日志条目可能构成 commit 依赖 DAG——BFS 拓扑排序确定回放顺序

学完本卡后能动手的事：在 `rag/src/rag/index/hnsw_index.cpp` 中找到 `search_layer()` 函数，用本卡的 BFS 思维模式画出从 entry point 到 query 的搜索路径——理解 HNSW 的"局部贪心 + 全局随机入口"策略
