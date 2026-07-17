# Graph Index Scaffold

图索引学习脚手架：NSW + HNSW 原理演示。

## 概述

本模块演示基于图的向量最近邻搜索算法核心原理：

- **NSW**（Navigable Small World）：单层近邻图，贪心搜索
- **HNSW**（Hierarchical NSW）：多层跳跃式搜索，贪心下降 + 扩展剪枝

## 编译运行

```bash
gcc -std=c11 -Wall -O2 -o test main.c -lm
./test
```

## 核心概念

### NSW 图

- 每个节点与若干最近邻居双向连接
- 搜索：从入口点出发，贪心遍历邻居直到收敛
- 连边：插入时选择 efConstruction 个最近邻居

### HNSW 图

- 节点有随机层高（指数分布），高层节点少
- 搜索分两阶段：
  1. 贪心下降：从高层稀疏图快速定位区域
  2. 扩展搜索：在第 0 层用候选队列精细搜索

### 搜索剪枝

- **visited 集合**：避免节点重复入队
- **距离下界**：结果集最远距离作为剪枝阈值
- **efSearch 上限**：超过阈值提前终止

## 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| DIM | 4 | 向量维度 |
| MAX_NODES | 1024 | 最大节点数 |
| MAX_NEIGHBORS | 3 | 每节点最大邻居数 |
| MAX_LEVEL | 6 | HNSW 最大层数 |
| EF_SEARCH | 16 | 搜索候选队列大小 |
| EF_CONSTRUCTION | 16 | 构图邻居数 |

## 工程参考

详细实现参考 `engineering/src/db/index/vector_index/hnsw/`。
