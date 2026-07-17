# 空间存储使用指南

## 概述

空间存储引擎（Spatial Engine）专为空间数据的存储和查询设计，支持 R-Tree 索引和 Hilbert 曲线辅助索引。

## 核心特性

| 特性 | 说明 |
|------|------|
| R-Tree 索引 | 高效范围查询和最近邻 |
| Hilbert 曲线 | 空间填充曲线辅助索引 |
| 持久化存储 | 4KB 页面 + 64 字节头 |
| 几何类型 | 点、线、多边形、边界框 |
| KNN 查询 | 最近邻搜索 |

## 快速开始

### 1. 初始化引擎

```c
#include "db/storage/spatial/spatial_engine.h"

// 初始化空间引擎
spatial_engine_init("/data/spatial");

// 创建空间表
spatial_engine_create("geo_objects", SPATIAL_GEOMETRY);

// 打开空间表
void *spatial_db = spatial_engine_open("geo_objects", ACCESS_MODE_READ_WRITE);
```

### 2. 插入几何对象

```c
// 插入边界框
bbox_t bbox = bbox_create(0.0, 0.0, 10.0, 10.0);
spatial_engine_insert(spatial_db, &bbox, sizeof(bbox), 1);

// 插入点
point_t point = {120.0, 30.0};  // 经度, 纬度
spatial_engine_insert(spatial_db, &point, sizeof(point), 1);

// 插入多边形
polygon_t polygon;
polygon.num_points = 4;
polygon.points[0] = (point_t){0.0, 0.0};
polygon.points[1] = (point_t){10.0, 0.0};
polygon.points[2] = (point_t){10.0, 10.0};
polygon.points[3] = (point_t){0.0, 10.0};
spatial_engine_insert(spatial_db, &polygon, sizeof(polygon), 1);
```

### 3. 范围查询

```c
// 边界框查询
bbox_t query_bbox = bbox_create(5.0, 5.0, 15.0, 15.0);
spatial_query_result_t results[100];

int count = spatial_engine_search_bbox(spatial_db, &query_bbox, results, 100);
printf("找到 %d 个匹配对象:\n", count);
for (int i = 0; i < count; i++) {
    printf("  对象 %d: 类型=%d\n", i, results[i].type);
}
```

## R-Tree 索引

R-Tree 是空间索引的标准数据结构，支持高效的范围查询和最近邻搜索。

### 构建索引

```c
// 构建 R-Tree 索引
// 参数：节点最大条目数
int ret = spatial_engine_build_index(spatial_db, 16);
if (ret != 0) {
    fprintf(stderr, "构建索引失败\n");
}

// 构建 Hilbert 辅助索引
ret = spatial_engine_build_hilbert_index(spatial_db);
if (ret != 0) {
    fprintf(stderr, "构建 Hilbert 索引失败\n");
}
```

### 持久化

```c
// 保存 R-Tree 到文件
ret = spatial_engine_save_rtree(spatial_db);
if (ret != 0) {
    fprintf(stderr, "保存 R-Tree 失败\n");
}

// 加载 R-Tree
ret = spatial_engine_load_rtree(spatial_db);
if (ret != 0) {
    fprintf(stderr, "加载 R-Tree 失败\n");
}

// 保存 Hilbert 索引
ret = spatial_engine_save_hilbert_index(spatial_db);
if (ret != 0) {
    fprintf(stderr, "保存 Hilbert 索引失败\n");
}

// 加载 Hilbert 索引
ret = spatial_engine_load_hilbert_index(spatial_db);
if (ret != 0) {
    fprintf(stderr, "加载 Hilbert 索引失败\n");
}
```

## Hilbert 曲线索引

Hilbert 曲线是一种空间填充曲线，能够保持空间局部性，适合优化范围查询和 KNN 查询。

### 编码解码

```c
#include "db/index/hilbert.h"

// 2D 点编码为 1D Hilbert 值
double x = 123.456, y = 45.678;
uint64_t hilbert_code = hilbert_encode(x, y, 32);  // 32 位精度
printf("Hilbert 码: %lu\n", hilbert_code);

// 1D Hilbert 值解码为 2D 点
double decoded_x, decoded_y;
hilbert_decode(hilbert_code, &decoded_x, &decoded_y, 32);
printf("解码: (%.6f, %.6f)\n", decoded_x, decoded_y);
```

### Hilbert 辅助范围查询

```c
// 使用 Hilbert 索引进行范围查询
bbox_t query_bbox = bbox_create(100.0, 20.0, 110.0, 30.0);
spatial_query_result_t hilbert_results[100];

int count = spatial_engine_hilbert_search(spatial_db, &query_bbox, 
                                          hilbert_results, 100);
printf("Hilbert 范围查询找到 %d 个结果\n", count);
```

### Hilbert 辅助 KNN 查询

```c
// 使用 Hilbert 索引进行最近邻查询
point_t query_point = {105.0, 25.0};  // 查询点
spatial_query_result_t knn_results[5];

int count = spatial_engine_hilbert_knn(spatial_db, &query_point, 5,
                                        knn_results, 5);
printf("Hilbert KNN 查询找到 %d 个最近邻\n", count);

for (int i = 0; i < count; i++) {
    printf("  [%d] 对象 ID: %lu, 距离: %.4f\n", 
           i, knn_results[i].object_id, knn_results[i].distance);
}
```

## 几何操作

### 创建几何对象

```c
// 点
point_t pt = {10.5, 20.3};

// 边界框
bbox_t box = bbox_create(0.0, 0.0, 100.0, 100.0);

// 线段
line_t line;
line.p1 = (point_t){0.0, 0.0};
line.p2 = (point_t){100.0, 100.0};

// 多边形
polygon_t poly;
poly.num_points = 4;
poly.points[0] = (point_t){0.0, 0.0};
poly.points[1] = (point_t){100.0, 0.0};
poly.points[2] = (point_t){100.0, 100.0};
poly.points[3] = (point_t){0.0, 100.0};
```

### 几何关系判断

```c
// 判断点是否在边界框内
bool inside = bbox_contains_point(&box, &pt);
printf("点在框内: %s\n", inside ? "是" : "否");

// 判断两个边界框是否相交
bbox_t box2 = bbox_create(50.0, 50.0, 150.0, 150.0);
bool intersects = bbox_intersects(&box, &box2);
printf("框相交: %s\n", intersects ? "是" : "否");

// 计算边界框面积
double area = bbox_area(&box);
printf("框面积: %.2f\n", area);

// 计算两个边界框的距离
double dist = bbox_distance(&box, &box2);
printf("框距离: %.2f\n", dist);
```

### 几何计算

```c
// 计算两点之间的距离
double pt_dist = point_distance(&pt1, &pt2);
printf("点距离: %.4f\n", pt_dist);

// 计算线段长度
double line_len = line_length(&line);
printf("线段长度: %.4f\n", line_len);

// 计算多边形面积
double poly_area = polygon_area(&poly);
printf("多边形面积: %.4f\n", poly_area);
```

## 完整示例

```c
#include "db/storage/spatial/spatial_engine.h"
#include "db/index/hilbert.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    // 1. 初始化
    spatial_engine_init("/tmp/spatial");
    spatial_engine_create("cities", SPATIAL_GEOMETRY);
    
    void *spatial_db = spatial_engine_open("cities", ACCESS_MODE_READ_WRITE);
    
    // 2. 插入城市数据（模拟）
    printf("插入空间数据...\n");
    
    typedef struct {
        const char *name;
        double lon, lat;
    } city_t;
    
    city_t cities[] = {
        {"北京", 116.4, 39.9},
        {"上海", 121.5, 31.2},
        {"广州", 113.3, 23.1},
        {"深圳", 114.1, 22.5},
        {"杭州", 120.2, 30.3},
        {"南京", 118.8, 32.1},
        {"武汉", 114.3, 30.6},
        {"成都", 104.1, 30.7},
        {"西安", 108.9, 34.3},
        {"重庆", 106.5, 29.5}
    };
    
    for (int i = 0; i < 10; i++) {
        bbox_t bbox = bbox_create(
            cities[i].lon - 0.5, cities[i].lat - 0.5,
            cities[i].lon + 0.5, cities[i].lat + 0.5
        );
        spatial_engine_insert(spatial_db, &bbox, sizeof(bbox), i + 1);
        printf("  添加城市: %s (%.1f, %.1f)\n", 
               cities[i].name, cities[i].lon, cities[i].lat);
    }
    
    // 3. 构建索引
    printf("\n构建索引...\n");
    spatial_engine_build_index(spatial_db, 16);
    printf("  R-Tree 索引构建完成\n");
    
    spatial_engine_build_hilbert_index(spatial_db);
    printf("  Hilbert 索引构建完成\n");
    
    // 4. Hilbert 编码测试
    printf("\nHilbert 编码测试:\n");
    for (int i = 0; i < 5; i++) {
        uint64_t code = hilbert_encode(cities[i].lon, cities[i].lat, 32);
        printf("  %s: (%.1f, %.1f) -> Hilbert: %lu\n",
               cities[i].name, cities[i].lon, cities[i].lat, code);
    }
    
    // 5. 范围查询
    printf("\n范围查询 (华东地区):\n");
    bbox_t east_china = bbox_create(115.0, 25.0, 125.0, 35.0);
    spatial_query_result_t range_results[100];
    
    int range_count = spatial_engine_search_bbox(spatial_db, &east_china, 
                                                  range_results, 100);
    printf("  找到 %d 个城市\n", range_count);
    for (int i = 0; i < range_count; i++) {
        printf("    对象 ID: %lu\n", range_results[i].object_id);
    }
    
    // 6. Hilbert 辅助范围查询
    printf("\nHilbert 辅助范围查询:\n");
    spatial_query_result_t hilbert_results[100];
    int hilbert_count = spatial_engine_hilbert_search(spatial_db, &east_china,
                                                       hilbert_results, 100);
    printf("  找到 %d 个城市\n", hilbert_count);
    
    // 7. KNN 查询
    printf("\nKNN 查询 (以武汉为中心):\n");
    point_t wuhan = {114.3, 30.6};
    spatial_query_result_t knn_results[5];
    
    int knn_count = spatial_engine_hilbert_knn(spatial_db, &wuhan, 5,
                                                knn_results, 5);
    printf("  找到 %d 个最近邻:\n", knn_count);
    for (int i = 0; i < knn_count; i++) {
        printf("    [%d] ID=%lu, 距离=%.4f\n",
               i, knn_results[i].object_id, knn_results[i].distance);
    }
    
    // 8. 保存索引
    printf("\n保存索引...\n");
    spatial_engine_save_rtree(spatial_db);
    spatial_engine_save_hilbert_index(spatial_db);
    
    // 9. 清理
    spatial_engine_close(spatial_db);
    spatial_engine_shutdown();
    
    return 0;
}
```

## Hilbert 阶数选择

| 数据范围 | 推荐阶数 | 说明 |
|----------|----------|------|
| 全球范围 | 32 位 | 高精度 |
| 国家范围 | 24-28 位 | 平衡精度和性能 |
| 城市范围 | 16-20 位 | 足够精度 |

## Hilbert 曲线特性

```
┌────────────────────────────────────────┐
│           Hilbert 曲线特性              │
├────────────────────────────────────────┤
│                                        │
│  ┌──┬──┐     ┌────┐                   │
│  │0 │1 │     │ 3→ │                   │
│  ├──┼──┤     │ ↓  │                   │
│  │3 │2 │ ──▶ │ 2  │ ──▶ 空间填充      │
│  └──┴──┘     │ ↓  │                   │
│              │ 1  │                   │
│              │ ← 0│                   │
│              └────┘                   │
│                                        │
│  1. 局部性保持：相邻点在 Hilbert 曲线   │
│     上也相邻                                          │
│  2. O(1) 编码：快速 2D→1D 转换        │
│  3. 范围查询：减少需要扫描的区域       │
│                                        │
└────────────────────────────────────────┘
```

## 常见问题

**Q: Hilbert 曲线相比 Z-Order 有什么优势？**
A: Hilbert 曲线更好地保持空间局部性，Z-Order 在某些查询模式下会导致聚集效应。

**Q: R-Tree 和 Hilbert 索引如何选择？**
A: R-Tree 适合精确范围查询，Hilbert 适合近似查询和 KNN。根据查询模式选择。

**Q: 如何确定 Hilbert 阶数？**
A: 阶数越高精度越高，但 Hilbert 值范围更大。根据数据范围和精度需求选择。
