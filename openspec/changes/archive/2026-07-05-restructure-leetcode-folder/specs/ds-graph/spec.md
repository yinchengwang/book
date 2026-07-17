# ds-graph 规格

## 概述

图是由节点和边组成的非线性数据结构。本规格定义图的完整实现及可视化文档要求。

## 目录结构

```
src/ds/graph/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── graph.c       # 图实现
│   └── graph.hpp
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `graph_create()` | O(1) |
| 添加边 | `graph_add_edge()` | O(1) |
| BFS 遍历 | `graph_bfs()` | O(V+E) |
| DFS 遍历 | `graph_dfs()` | O(V+E) |
| 最短路径 | `graph_shortest_path()` | O(V+E) |

## README.md 文档要求

1. 有向图/无向图可视化
2. 邻接表表示
3. BFS/DFS 遍历过程可视化

## 验收标准

- [ ] 图的基本操作实现完成
- [ ] BFS/DFS 实现
- [ ] README.md 包含遍历可视化
- [ ] CMakeLists.txt 正确配置
