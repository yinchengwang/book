# 几何类型规格

## 概述

几何类型模块提供基础几何对象的定义和操作，包括 Point、LineString、Polygon 和 GeometryCollection。

## 类型定义

### Point (点)

```c
typedef struct {
    double x;
    double y;
    double z;  // 可选，默认为 0
    bool has_z;
} Point;
```

### LineString (线)

```c
typedef struct {
    Point* points;
    int npoints;
} LineString;
```

### Polygon (多边形)

```c
typedef struct {
    LineString* exterior;    // 外环（逆时针）
    LineString** holes;      // 内环数组（顺时针）
    int nholes;
} Polygon;
```

### GeometryCollection (几何集合)

```c
typedef struct {
    GeometryType type;
    Geometry** geometries;
    int ngeometries;
} GeometryCollection;
```

### Geometry (联合类型)

```c
typedef enum {
    GEOM_POINT,
    GEOM_LINESTRING,
    GEOM_POLYGON,
    GEOM_COLLECTION
} GeometryType;

typedef struct {
    GeometryType type;
    union {
        Point point;
        LineString linestring;
        Polygon polygon;
        GeometryCollection collection;
    } geom;
    BoundingBox bbox;
} Geometry;
```

## API

### 创建

| 函数 | 说明 |
|------|------|
| `geometry_create_point(x, y)` | 创建二维点 |
| `geometry_create_point_3d(x, y, z)` | 创建三维点 |
| `geometry_create_linestring(points, n)` | 创建线 |
| `geometry_create_polygon(exterior, holes, nholes)` | 创建多边形 |

### 解析/序列化

| 函数 | 说明 |
|------|------|
| `geometry_from_wkt(wkt)` | 从 WKT 解析 |
| `geometry_to_wkt(geom)` | 转换为 WKT |
| `geometry_from_wkb(wkb, len)` | 从 WKB 解析 |
| `geometry_to_wkb(geom)` | 转换为 WKB |

### 属性

| 函数 | 说明 |
|------|------|
| `geometry_is_empty(geom)` | 是否为空 |
| `geometry_is_valid(geom)` | 是否有效 |
| `geometry_num_points(geom)` | 点数 |
| `geometry_dimension(geom)` | 维度 (0/1/2) |
| `geometry_type_name(geom)` | 类型名称 |

## WKT 格式支持

| 类型 | WKT 示例 |
|------|----------|
| Point | `POINT(0 0)` |
| LineString | `LINESTRING(0 0, 1 1, 2 0)` |
| Polygon | `POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))` |
| MultiPoint | `MULTIPOINT((0 0), (1 1))` |
| MultiLineString | `MULTILINESTRING((0 0, 1 1), (2 2, 3 3))` |
| MultiPolygon | `MULTIPOLYGON(((0 0, 1 0, 1 1, 0 1, 0 0)))` |

## 验收标准

- [x] Point 创建和 WKT 解析
- [x] LineString 创建和 WKT 解析
- [x] Polygon 创建和 WKT 解析
- [x] GeometryCollection 创建
- [x] WKT 序列化
- [x] 基本属性方法
