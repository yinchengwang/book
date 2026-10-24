# P2-3 空间分析函数 设计文档

## 架构概述

空间分析模块提供地理空间数据处理能力，包括几何类型、空间索引和空间操作。

```
┌─────────────────────────────────────────────────────────────┐
│                    Spatial API Layer                          │
├─────────────────────────────────────────────────────────────┤
│  Geometry Types  │  Spatial Index  │  Spatial Predicates │
├─────────────────────────────────────────────────────────────┤
│                    Spatial Storage Engine                     │
└─────────────────────────────────────────────────────────────┘
```

## 核心组件

### 1. 几何类型 (`spatial_geo.h`)

- **Point**：二维/三维点
- **LineString**：折线
- **Polygon**：多边形
- **GeometryCollection**：几何集合

### 2. R-Tree 索引 (`rtree.h`)

- 内存中的 R-Tree 构建
- 文件持久化支持
- 范围查询
- KNN 查询

### 3. 空间谓词 (`spatial_geo.c`)

| 函数 | 说明 |
|------|------|
| `ST_Contains` | 几何 A 是否包含几何 B |
| `ST_Intersects` | 几何 A 与几何 B 是否相交 |
| `ST_Within` | 几何 A 是否在几何 B 内 |
| `ST_Equals` | 几何 A 与几何 B 是否相等 |
| `ST_Touches` | 几何 A 是否接触几何 B |
| `ST_Crosses` | 几何 A 是否穿越几何 B |

### 4. 空间操作 (`spatial_geo.c`)

| 函数 | 说明 |
|------|------|
| `ST_Area` | 计算多边形面积 |
| `ST_Length` | 计算线长度 |
| `ST_Distance` | 计算两点/几何间距离 |
| `ST_Centroid` | 计算几何重心 |
| `ST_Buffer` | 生成缓冲区 |
| `ST_Union` | 几何合并 |
| `ST_Intersection` | 几何交集 |

## API 设计

### 几何创建

```c
// 从 WKT 创建几何
Geometry* geometry_from_wkt(const char* wkt);

// 创建点
Geometry* geometry_create_point(double x, double y);

// 创建线
Geometry* geometry_create_linestring(double* coords, int npoints);

// 创建多边形
Geometry* geometry_create_polygon(double* ring, int npoints);

// 销毁几何
void geometry_destroy(Geometry* geom);
```

### WKT 序列化

```c
// 转换为 WKT 字符串
char* geometry_to_wkt(const Geometry* geom);

// 获取 WKT 长度
size_t geometry_wkt_length(const Geometry* geom);
```

### 空间关系

```c
// 判断相交
bool geom_intersects(const Geometry* a, const Geometry* b);

// 判断包含
bool geom_contains(const Geometry* a, const Geometry* b);

// 判断在内
bool geom_within(const Geometry* a, const Geometry* b);

// 计算距离
double geom_distance(const Geometry* a, const Geometry* b);
```

### 空间测量

```c
// 计算面积
double geom_area(const Geometry* geom);

// 计算长度
double geom_length(const Geometry* geom);

// 计算重心
bool geom_centroid(const Geometry* geom, double* cx, double* cy);
```

## 数据结构

### Bounding Box

```c
typedef struct {
    double min_x, min_y;
    double max_x, max_y;
} BoundingBox;
```

### R-Tree 节点

```c
typedef struct RTreeNode {
    int is_leaf;
    int n_entries;
    BoundingBox bbox;
    struct RTreeNode** children;  // 内部节点
    Geometry** geometries;       // 叶子节点
    struct RTreeNode* parent;
} RTreeNode;
```

## 性能优化

1. **MBR 剪枝**：使用最小边界矩形加速判断
2. **四叉树预索引**：大数据集使用四叉树分层
3. **空间填充曲线**：Z-Order 曲线优化范围查询
