/**
 * @file spatial_functions.h
 * @brief 空间函数扩展接口
 *
 * Phase12 - 实现空间函数，追赶 PostGIS 水平。
 */
#ifndef DB_STORAGE_SPATIAL_FUNCTIONS_H
#define DB_STORAGE_SPATIAL_FUNCTIONS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 几何类型 */
typedef enum {
    GEOM_POINT = 0,
    GEOM_LINESTRING = 1,
    GEOM_POLYGON = 2,
    GEOM_MULTIPOINT = 3,
    GEOM_MULTILINESTRING = 4,
    GEOM_MULTIPOLYGON = 5,
    GEOM_GEOMETRYCOLLECTION = 6
} geom_type_t;

/** 几何对象 */
typedef struct geometry {
    geom_type_t type;
    int srid;              /**< 空间参考系 */
    void *data;            /**< WKB 或内部表示 */
    size_t size;            /**< 数据大小 */
} geometry_t;

/** 空间关系结果 */
typedef struct {
    bool result;
    double distance;
} spatial_predicate_result_t;

/** 空间操作结果 */
typedef struct {
    geometry_t *geom;
    double value;
} spatial_operation_result_t;

/** 创建几何对象 */
geometry_t *geom_create_point(double x, double y);
geometry_t *geom_create_point_3d(double x, double y, double z);
geometry_t *geom_create_linestring(double *coords, size_t num_points);
geometry_t *geom_create_polygon(double *ring, size_t num_points);
geometry_t *geom_from_wkb(const void *wkb, size_t size);
geometry_t *geom_from_wkt(const char *wkt);

/** 销毁几何对象 */
void geom_destroy(geometry_t *geom);

/** 空间谓词 */
bool geom_contains(const geometry_t *a, const geometry_t *b);
bool geom_intersects(const geometry_t *a, const geometry_t *b);
bool geom_within(const geometry_t *a, const geometry_t *b);
bool geom_touches(const geometry_t *a, const geometry_t *b);
bool geom_crosses(const geometry_t *a, const geometry_t *b);
bool geom_disjoint(const geometry_t *a, const geometry_t *b);
bool geom_overlaps(const geometry_t *a, const geometry_t *b);

/** 距离函数 */
double geom_distance(const geometry_t *a, const geometry_t *b);
double geom_distance_spheroid(const geometry_t *a, const geometry_t *b);

/** 空间操作 */
geometry_t *geom_buffer(const geometry_t *geom, double distance);
geometry_t *geom_intersection(const geometry_t *a, const geometry_t *b);
geometry_t *geom_union(const geometry_t *a, const geometry_t *b);
geometry_t *geom_difference(const geometry_t *a, const geometry_t *b);
geometry_t *geom_centroid(const geometry_t *geom);
geometry_t *geom_boundary(const geometry_t *geom);
geometry_t *geom_envelope(const geometry_t *geom);

/** 测量函数 */
double geom_area(const geometry_t *geom);
double geom_length(const geometry_t *geom);
double geom_perimeter(const geometry_t *geom);

/** 有效性检查 */
bool geom_is_valid(const geometry_t *geom);
bool geom_is_simple(const geometry_t *geom);

/** 维度 */
int geom_dimension(const geometry_t *geom);
int geom_num_points(const geometry_t *geom);
int geom_num_rings(const geometry_t *geom);

/** 输出 */
char *geom_to_wkt(const geometry_t *geom);
void *geom_to_wkb(const geometry_t *geom, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_SPATIAL_FUNCTIONS_H */
