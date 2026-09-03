# 最小生成树 (Minimum Spanning Tree)

## 算法概述

最小生成树（MST）是在带权无向图中找到一棵包含所有顶点、边权重之和最小的树。

## 核心算法

| 算法 | 时间复杂度 | 适用场景 |
|------|-----------|----------|
| Prim | O(V²) / O(E log V) | 稠密图 |
| Kruskal | O(E log E) | 稀疏图 |

## 并查集

并查集（Union-Find）是 Kruskal 算法判环的核心数据结构：
- find(x)：路径压缩
- union(x, y)：按秩合并

## 工程对照

### 网络设计
- 城市光缆铺设：MST 保证连接所有城市、总长度最短
- 电信网络布线、电网设计同理

### 聚类分析
- Kruskal MST 用于层次聚类（谱聚类）
- 生成树-cut 理论用于图分割

### 近似算法
-旅行商问题（TSP）的 MST 近似比（2-近似）
- 图像分割：Graph Cut 能量最小化类似 MST

## 参考

- `learning/scaffold/ds/union_find/` — 并查集详解
- `engineering/src/algo/graph.c` — 图论算法
