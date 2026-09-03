# R-Tree 索引规格

## 概述

R-Tree 是一种用于空间索引的树数据结构，支持高效的范围查询和最近邻查询。

## 数据结构

### Bounding Box (最小边界矩形)

```c
typedef struct {
    double min_x, min_y;
    double max_x, max_y;
} BBox;
```

### R-Tree 节点

```c
typedef struct RTreeNode {
    bool is_leaf;
    int n_entries;
    BBox bbox;
    union {
        struct RTreeNode** children;  // 内部节点
        Geometry** geometries;        // 叶子节点
    } entries;
    struct RTreeNode* parent;
} RTreeNode;
```

### R-Tree

```c
typedef struct {
    RTreeNode* root;
    int max_entries;    // 最大条目数 (默认 16)
    int min_entries;    // 最小条目数 (默认 4)
    int size;           // 几何对象总数
} RTree;
```

## API

### 创建和销毁

| 函数 | 说明 |
|------|------|
| `rtree_create()` | 创建空 R-Tree |
| `rtree_create_with_params(max, min)` | 带参数创建 |
| `rtree_destroy(tree)` | 销毁 R-Tree |

### 插入和删除

| 函数 | 说明 |
|------|------|
| `rtree_insert(tree, geom)` | 插入几何对象 |
| `rtree_remove(tree, geom)` | 删除几何对象 |

### 查询

| 函数 | 说明 |
|------|------|
| `rtree_search_range(tree, bbox)` | 范围查询 |
| `rtree_search_point(tree, x, y)` | 点查询 |
| `rtree_knn(tree, x, y, k)` | K 近邻查询 |

### 属性

| 函数 | 说明 |
|------|------|
| `rtree_size(tree)` | 返回几何对象数量 |
| `rtree_depth(tree)` | 返回树深度 |
| `rtree_height(tree)` | 返回树高度（别名） |

## 算法

### 插入算法

1. 从根节点开始
2. 选择 MBR 增量最小的子节点
3. 递归直到叶子节点
4. 如果叶子节点已满，进行节点分裂
5. 向上更新 MBR

### 节点分裂算法

使用二次分裂：
1. 分离距离最远的两个条目
2. 贪心分配剩余条目到两个组

### 范围查询

1. 从根节点开始
2. 如果节点 MBR 与查询范围不相交，返回
3. 如果是叶子节点，检查每个几何对象
4. 如果是内部节点，递归检查子节点

## 性能指标

| 指标 | 目标 |
|------|------|
| 插入 (10K 对象) | < 100ms |
| 范围查询 | O(log n) |
| KNN 查询 | O(log n + k) |

## 验收标准

- [x] `rtree_create` 创建空索引
- [x] `rtree_insert` 插入几何对象
- [x] `rtree_search_range` 范围查询正确
- [x] `rtree_knn` KNN 查询正确
- [x] `rtree_destroy` 正确释放资源
- [ ] 性能基准测试达标
