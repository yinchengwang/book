# 最短路径算法 (Shortest Path)

## 算法分类

| 类型 | 算法 |
|------|------|
| 单源非负权 | Dijkstra |
| 单源允许负权 | Bellman-Ford, SPFA |
| 全源 | Floyd-Warshall |

## 核心思想

### Dijkstra
1. 从源点开始，逐步扩展已知最短距离
2. 每轮选择未访问顶点中距离最小的
3. 用该顶点去松弛其他顶点的距离

### Bellman-Ford
1. 对所有边进行 V-1 轮松弛操作
2. 可以检测负权环（第 V 轮仍有更新则存在负环）

### Floyd-Warshall
1. 动态规划：dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])
2. 逐步引入中转顶点 k，迭代更新

## 测试数据

测试图（6 顶点）：
```
v0 --2-- v1
|\\       /|
2| \\5   3/|4
|   \\ /   |
v4---v2---v3
 1  2/ 5 |
       /  |
      v5  6
```

## 运行

```bash
make && ./shortest_path_demo
```
