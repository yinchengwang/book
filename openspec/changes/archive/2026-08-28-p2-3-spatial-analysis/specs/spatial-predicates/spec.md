# 空间谓词规格

## 概述

空间谓词用于判断两个几何对象之间的空间关系，返回布尔值。

## 谓词定义

### DE-9IM 矩阵

空间谓词基于 Dimensionally Extended Nine-Intersection Model (DE-9IM)，定义两个几何对象的交集维度模式。

### 标准谓词

| 谓词 | 说明 | DE-9IM 模式 |
|------|------|--------------|
| Equals | 相等 | `T*F**FFF*` |
| Disjoint | 不相交 | `FF*FF****` |
| Intersects | 相交 | `********` (非 disjoint) |
| Touches | 接触 | `FT*******` 或 `F**T*****` 或 `F***T****` |
| Crosses | 穿越 | `T*T******` (点/线) 或 `T*****T**` (线/面) |
| Within | 在内 | `T*F**F***` |
| Contains | 包含 | `T*****FF*` |
| Overlaps | 重叠 | `T*T***T**` (点/点, 面/面) |

## API

### 相交关系

```c
// 判断是否相交
bool geom_intersects(const Geometry* a, const Geometry* b);

// 判断是否不相交
bool geom_disjoint(const Geometry* a, const Geometry* b);

// 判断是否接触
bool geom_touches(const Geometry* a, const Geometry* b);

// 判断是否穿越
bool geom_crosses(const Geometry* a, const Geometry* b);
```

### 包含关系

```c
// 判断 A 是否包含 B
bool geom_contains(const Geometry* a, const Geometry* b);

// 判断 A 是否在 B 内
bool geom_within(const Geometry* a, const Geometry* b);

// 判断 A 是否覆盖 B
bool geom_covers(const Geometry* a, const Geometry* b);

// 判断 A 是否被 B 覆盖
bool geom_covered_by(const Geometry* a, const Geometry* b);
```

### 相等关系

```c
// 判断是否相等（精确）
bool geom_equals(const Geometry* a, const Geometry* b);

// 判断是否相等（容差）
bool geom_equals_exact(const Geometry* a, const Geometry* b, double tolerance);
```

### 距离关系

```c
// 计算最短距离
double geom_distance(const Geometry* a, const Geometry* b);

// 判断距离是否在范围内
bool geom_is_within_distance(const Geometry* a, const Geometry* b, double distance);

// 判断是否相交（带缓冲区）
bool geom_is_within_distance_allow_null(const Geometry* a, const Geometry* b, double distance);
```

## 算法实现

### 矩形相交快速判断

```c
// 快速 MBR 判断
bool bbox_intersects(const BBox* a, const BBox* b) {
    return !(a->max_x < b->min_x || b->max_x < a->min_x ||
             a->max_y < b->min_y || b->max_y < a->min_y);
}
```

### 点-矩形相交

```c
bool point_in_bbox(const Point* p, const BBox* bbox) {
    return p->x >= bbox->min_x && p->x <= bbox->max_x &&
           p->y >= bbox->min_y && p->y <= bbox->max_y;
}
```

### 线-线相交 (Crosses)

使用 Liang-Barsky 算法或 Cohen-Sutherland 裁剪。

### 点-多边形 (Within/Contains)

使用射线投射算法：
1. 从点向右发出一条射线
2. 计算射线与多边形边界的交点数量
3. 奇数 = 在内部，偶数 = 在外部

## 验收标准

- [x] `geom_intersects` 正确判断相交
- [x] `geom_disjoint` 正确判断不相交
- [x] `geom_contains` 正确判断包含
- [x] `geom_within` 正确判断在内
- [x] `geom_touches` 正确判断接触
- [x] `geom_crosses` 正确判断穿越
- [x] `geom_equals` 正确判断相等
- [x] `geom_distance` 正确计算距离
