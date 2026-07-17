# R-Tree 空间索引

## 概述

R-Tree 是一种**空间索引**结构，用于高效存储和查询多维矩形（MBR - Minimum Bounding Rectangle）。

## 设计背景

- 1984 年由 Guttman 提出
- 广泛用于**地理信息系统（GIS）**
- 支持**矩形区域查询**

## 数据结构

```
R-Tree 结构：

        [Root MBR: (0,0)-(100,100)]
              │
    ┌─────────┴─────────┐
    │                   │
[MBR: (0,0)-(50,50)]  [MBR: (50,50)-(100,100)]
    │                   │
┌───┴───┐           ┌───┴───┐
│       │           │       │
R1    R2           R3     R4
```

节点结构：
```c
typedef struct rtree_node {
    bool is_leaf;
    uint32_t nentries;
    rect_t mbr;  // 最小边界矩形
    union {
        rtree_entry_t *entries;     // 叶子节点
        struct rtree_node **children;  // 内部节点
    } u;
} rtree_node_t;

typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} rect_t;
```

## 核心算法

### 插入算法
1. 从根开始，选择**最小面积增量**的子节点
2. 递归直到叶子节点
3. 若叶子节点满了，**分裂节点**

### 分裂算法（Quadratic Split）
1. 选两个种子条目，距离最远
2. 依次分配剩余条目到面积增量最小的组

### 查询算法
1. 从根开始，检查 MBR 是否与查询矩形相交
2. 不相交 → 剪枝
3. 相交 → 递归到子节点
4. 叶子节点 → 返回匹配的条目

## 操作复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| 插入 | O(n^(k-1)/k) | k 为维度 |
| 删除 | O(n^(k-1)/k) | |
| 搜索 | O(n^(k-1)/k) | 使用 MBR 剪枝 |

实际复杂度远优于线性搜索。

## 变体

| 变体 | 特点 |
|------|------|
| R+ Tree | 节点不重叠，搜索更快 |
| R* Tree | 优化分裂和重新插入 |
| Hilbert R-Tree | 使用 Hilbert 曲线排序 |

## 应用场景

| 系统 | 用途 |
|------|------|
| PostgreSQL | PostGIS 地理扩展 |
| SQLite | R-Tree 虚拟表 |
| MongoDB | 2dsphere 索引 |
| Google Maps | 空间查询 |

## 代码实现

项目路径：`src/index/tree/rtree/`

核心模块：
- `rtree_core.c` - 创建、销毁、节点管理
- `rtree_insert.c` - 插入（含节点选择）
- `rtree_delete.c` - 删除
- `rtree_search.c` - 矩形查询

## 面试要点

1. **R-Tree 如何加速空间查询？** - 使用 MBR 剪枝
2. **R-Tree 的 MBR 是什么？** - 最小边界矩形
3. **R-Tree 与 B-Tree 的区别？** - 多维 vs 一维
4. **R-Tree 的分裂算法有哪些？** - Quadratic Split、Linear Split

## 参考资料

- Guttman, A. (1984). "R-Trees: A Dynamic Index Structure for Spatial Searching"
- PostGIS 文档