/**
 * @file spatial_geo.h
 * @brief 空间几何对象头文件
 *
 * 实现 Point/Linestring/Polygon 几何对象、WKT 解析和序列化、空间关系判断
 */
#ifndef DB_SPATIAL_GEO_H
#define DB_SPATIAL_GEO_H

#include "db/storage/spatial/spatial_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 几何类型
 * ======================================================================== */

/**
 * @brief 几何对象类型
 */
typedef enum SpatialGeomType_e {
    SPATIAL_GEOM_POINT = 0,         /**< 点 */
    SPATIAL_GEOM_LINESTRING = 1,    /**< 线段 */
    SPATIAL_GEOM_POLYGON = 2,       /**< 多边形 */
    SPATIAL_GEOM_MULTIPOINT = 3,    /**< 多点 */
    SPATIAL_GEOM_MULTILINESTRING = 4, /**< 多线段 */
    SPATIAL_GEOM_MULTIPOLYGON = 5,  /**< 多多边形 */
    SPATIAL_GEOM_GEOMETRYCOLLECTION = 6, /**< 几何集合 */
} SpatialGeomType;

/* ========================================================================
 * 几何对象结构
 * ======================================================================== */

/** 坐标点 */
typedef struct SpatialCoord_s {
    double x;                       /**< X 坐标 */
    double y;                       /**< Y 坐标 */
} SpatialCoord;

/** 线段 */
typedef struct SpatialLine_s {
    SpatialCoord *points;           /**< 点数组 */
    size_t num_points;              /**< 点数量 */
} SpatialLine;

/** 环（Polygon 的边界） */
typedef struct SpatialRing_s {
    SpatialCoord *points;           /**< 点数组（首尾相同） */
    size_t num_points;              /**< 点数量 */
} SpatialRing;

/** 多边形 */
typedef struct SpatialPolygon_s {
    SpatialRing *exterior;          /**< 外环 */
    SpatialRing **interiors;        /**< 内环数组（孔洞） */
    size_t num_interiors;           /**< 内环数量 */
} SpatialPolygon;

/** 几何对象联合体 */
typedef struct SpatialGeometry_s {
    SpatialGeomType type;           /**< 几何类型 */
    bbox_t bbox;                    /**< 边界框 */
    union {
        SpatialCoord point;         /**< 点 */
        SpatialLine line;           /**< 线段 */
        SpatialPolygon polygon;     /**< 多边形 */
    } geom;
} SpatialGeometry;

/* ========================================================================
 * 几何对象创建和销毁
 * ======================================================================== */

/**
 * @brief 创建点
 */
SpatialGeometry *spatial_point_create(double x, double y);

/**
 * @brief 创建线段
 */
SpatialGeometry *spatial_line_create(const SpatialCoord *points, size_t num_points);

/**
 * @brief 创建多边形
 */
SpatialGeometry *spatial_polygon_create(const SpatialCoord *exterior,
                                        size_t num_exterior,
                                        const SpatialCoord **interiors,
                                        size_t num_interiors);

/**
 * @brief 复制几何对象
 */
SpatialGeometry *spatial_geometry_clone(const SpatialGeometry *geom);

/**
 * @brief 销毁几何对象
 */
void spatial_geometry_destroy(SpatialGeometry *geom);

/**
 * @brief 获取几何对象边界框
 */
bbox_t spatial_geometry_bbox(const SpatialGeometry *geom);

/* ========================================================================
 * WKT 解析和序列化
 * ======================================================================== */

/**
 * @brief WKT 解析错误码
 */
typedef enum SpatialWktError_e {
    SPATIAL_WKT_OK = 0,
    SPATIAL_WKT_INVALID_FORMAT = -1,
    SPATIAL_WKT_UNSUPPORTED_TYPE = -2,
    SPATIAL_WKT_PARSE_ERROR = -3,
} SpatialWktError;

/**
 * @brief 解析 WKT 字符串
 * @param wkt WKT 字符串（如 "POINT(1 2)"）
 * @param out_error 错误码输出（可为 NULL）
 * @return 几何对象，失败返回 NULL
 */
SpatialGeometry *spatial_wkt_parse(const char *wkt, SpatialWktError *out_error);

/**
 * @brief 获取 WKT 解析错误信息
 */
const char *spatial_wkt_error_str(SpatialWktError error);

/**
 * @brief 几何对象转 WKT 字符串
 * @param geom 几何对象
 * @param out_len 输出字符串长度（可为 NULL）
 * @return WKT 字符串（调用者负责释放）
 */
char *spatial_wkt_serialize(const SpatialGeometry *geom, size_t *out_len);

/**
 * @brief 几何对象转 WKT 字符串（带精度控制）
 */
char *spatial_wkt_serialize_precision(const SpatialGeometry *geom,
                                      int precision);

/* ========================================================================
 * 空间关系判断
 * ======================================================================== */

/**
 * @brief 空间关系类型
 */
typedef enum SpatialRelation_e {
    SPATIAL_REL_INTERSECTS = 0,     /**< 相交 */
    SPATIAL_REL_CONTAINS,           /**< 包含 */
    SPATIAL_REL_WITHIN,             /**< 被包含 */
    SPATIAL_REL_COVERS,             /**< 覆盖 */
    SPATIAL_REL_COVERED_BY,         /**< 被覆盖 */
    SPATIAL_REL_DISJOINT,           /**< 不相交 */
    SPATIAL_REL_EQUALS,             /**< 相等 */
    SPATIAL_REL_TOUCHES,            /**< 相切 */
    SPATIAL_REL_CROSSES,            /**< 穿越 */
} SpatialRelation;

/**
 * @brief 检查空间关系
 */
bool spatial_check_relation(const SpatialGeometry *a,
                           const SpatialGeometry *b,
                           SpatialRelation relation);

/* ========================================================================
 * 距离计算
 * ======================================================================== */

/**
 * @brief 计算两点之间的欧氏距离
 */
double spatial_distance_point_point(const SpatialCoord *p1, const SpatialCoord *p2);

/**
 * @brief 计算点到线段的距离
 */
double spatial_distance_point_line(const SpatialCoord *p, const SpatialLine *line);

/**
 * @brief 计算点到多边形的距离
 */
double spatial_distance_point_polygon(const SpatialCoord *p, const SpatialPolygon *poly);

/**
 * @brief 计算 ST_Distance（欧氏距离）
 */
double spatial_distance(const SpatialGeometry *a, const SpatialGeometry *b);

/**
 * @brief 计算球面距离（ST_Distance_Sphere）
 * @param p1 第一个点
 * @param p2 第二个点
 * @param radius 球体半径（默认地球半径 6371000 米）
 * @return 距离（与 radius 单位相同）
 */
double spatial_distance_sphere(const SpatialCoord *p1, const SpatialCoord *p2,
                               double radius);

/* ========================================================================
 * 空间操作
 * ======================================================================== */

/**
 * @brief 点是否在多边形内
 */
bool spatial_point_in_polygon(const SpatialCoord *p, const SpatialPolygon *poly);

/**
 * @brief 点是否在边界框内
 */
bool spatial_point_in_bbox(const SpatialCoord *p, const bbox_t *bbox);

/**
 * @brief 边界框是否相交
 */
bool spatial_bbox_intersects(const bbox_t *a, const bbox_t *b);

/**
 * @brief ST_Contains 判断
 */
bool spatial_contains(const SpatialGeometry *container, const SpatialGeometry *item);

/**
 * @brief ST_Within 判断
 */
bool spatial_within(const SpatialGeometry *inner, const SpatialGeometry *outer);

/**
 * @brief ST_Intersects 判断
 */
bool spatial_intersects(const SpatialGeometry *a, const SpatialGeometry *b);

/**
 * @brief ST_Buffer 计算（简化实现）
 * @param geom 几何对象
 * @param distance 缓冲距离
 * @return 缓冲后的几何对象
 */
SpatialGeometry *spatial_buffer(const SpatialGeometry *geom, double distance);

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

/**
 * @brief ST_Distance SQL 函数
 *
 * ST_Distance(geom1, geom2)
 */
double spatial_sql_distance(const char *wkt1, const char *wkt2);

/**
 * @brief ST_Distance_Sphere SQL 函数
 *
 * ST_Distance_Sphere(point1, point2, radius)
 */
double spatial_sql_distance_sphere(double x1, double y1, double x2, double y2,
                                   double radius);

/**
 * @brief ST_Contains SQL 函数
 *
 * ST_Contains(geom1, geom2)
 */
bool spatial_sql_contains(const char *wkt1, const char *wkt2);

/**
 * @brief ST_Within SQL 函数
 *
 * ST_Within(geom1, geom2)
 */
bool spatial_sql_within(const char *wkt1, const char *wkt2);

/**
 * @brief ST_Intersects SQL 函数
 *
 * ST_Intersects(geom1, geom2)
 */
bool spatial_sql_intersects(const char *wkt1, const char *wkt2);

/**
 * @brief ST_Buffer SQL 函数
 *
 * ST_Buffer(geom, distance)
 */
char *spatial_sql_buffer(const char *wkt, double distance);

#ifdef __cplusplus
}
#endif

#endif /* DB_SPATIAL_GEO_H */
