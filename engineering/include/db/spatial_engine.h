/**
 * @file spatial_engine.h
 * @brief 空间存储引擎头文件
 *
 * 定义空间存储引擎的接口和数据结构，支持几何数据存储和空间查询。
 */
#ifndef DB_SPATIAL_ENGINE_H
#define DB_SPATIAL_ENGINE_H

#include "storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 空间相关类型定义
 * ======================================================================== */

/**
 * @brief 几何类型
 */
typedef enum {
    GEOM_POINT = 0,       /**< 点 */
    GEOM_LINESTRING = 1,  /**< 线段 */
    GEOM_POLYGON = 2,     /**< 多边形 */
} geometry_type_t;

/**
 * @brief 二维点
 */
typedef struct point_s {
    double x;    /**< X 坐标 */
    double y;    /**< Y 坐标 */
} point_t;

/**
 * @brief 边界框
 */
typedef struct bbox_s {
    double min_x;   /**< 最小 X */
    double min_y;   /**< 最小 Y */
    double max_x;   /**< 最大 X */
    double max_y;   /**< 最大 Y */
} bbox_t;

/**
 * @brief 空间引擎数据库
 */
typedef struct spatial_engine_db_s {
    char name[256];            /**< 数据集名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    uint64_t num_geometries;   /**< 几何对象数量 */
} spatial_engine_db_t;

/**
 * @brief 获取空间引擎操作表
 */
const storage_ops_t *spatial_engine_get_ops(void);

/**
 * @brief 初始化空间引擎
 */
int spatial_engine_init(const char *data_dir);

/**
 * @brief 关闭空间引擎
 */
int spatial_engine_shutdown(void);

/**
 * @brief 创建空间数据集
 */
int spatial_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开空间数据集
 */
void *spatial_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭空间数据集
 */
int spatial_engine_close(void *rel);

/**
 * @brief 删除空间数据集
 */
int spatial_engine_drop(const char *name);

/**
 * @brief 插入几何对象
 */
int spatial_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 获取统计信息
 */
int spatial_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * 空间查询 API
 * ======================================================================== */

/**
 * @brief 空间查询结果
 */
typedef struct spatial_query_result_s {
    void *data;           /**< 几何数据 */
    size_t len;           /**< 数据长度 */
    uint64_t id;          /**< 几何对象 ID */
    double distance;      /**< 距离（仅用于最近邻查询） */
} spatial_query_result_t;

/**
 * @brief 空间查询参数
 */
typedef struct spatial_query_args_s {
    bbox_t bbox;              /**< 查询边界框 */
    uint32_t limit;           /**< 最大返回数量 */
    uint32_t offset;          /**< 跳过数量 */
} spatial_query_args_t;

/**
 * @brief 边界框范围查询
 * @param rel 数据集句柄
 * @param args 查询参数
 * @param results 输出结果数组
 * @param num_results 输出结果数量
 * @return 0 成功，-1 失败
 */
int spatial_engine_query_bbox(void *rel, const spatial_query_args_t *args,
                               spatial_query_result_t *results, uint32_t *num_results);

/**
 * @brief 最近邻查询
 * @param rel 数据集句柄
 * @param point 查询点
 * @param k 返回最近邻数量
 * @param results 输出结果数组（按距离排序）
 * @param num_results 输出结果数量
 * @return 0 成功，-1 失败
 */
int spatial_engine_nearest(void *rel, const point_t *point, uint32_t k,
                            spatial_query_result_t *results, uint32_t *num_results);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 创建边界框
 */
bbox_t bbox_create(double min_x, double min_y, double max_x, double max_y);

/**
 * @brief 检查点是否在边界框内
 */
bool bbox_contains_point(const bbox_t *bbox, const point_t *point);

/**
 * @brief 检查两个边界框是否相交
 */
bool bbox_intersects(const bbox_t *a, const bbox_t *b);

#ifdef __cplusplus
}
#endif

#endif /* DB_SPATIAL_ENGINE_H */
