/**
 * @file spatial_engine.h
 * @brief 空间存储引擎头文件
 *
 * 定义空间存储引擎的接口和数据结构，支持几何数据存储和空间查询。
 */
#ifndef DB_STORAGE_SPATIAL_ENGINE_H
#define DB_STORAGE_SPATIAL_ENGINE_H

#include "db/storage_engine.h"
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
#ifndef POINT_T_DEFINED
#define POINT_T_DEFINED
typedef struct point_s {
    double x;    /**< X 坐标 */
    double y;    /**< Y 坐标 */
} point_t;
#endif

/**
 * @brief 边界框
 */
#ifndef BBOX_T_DEFINED
#define BBOX_T_DEFINED
typedef struct bbox_s {
    double min_x;   /**< 最小 X */
    double min_y;   /**< 最小 Y */
    double max_x;   /**< 最大 X */
    double max_y;   /**< 最大 Y */
} bbox_t;
#endif

/* ========================================================================
 * 空间查询类型定义
 * ======================================================================== */

/**
 * @brief 空间查询结果
 */
typedef struct spatial_query_result_s {
    void *data;               /**< 结果数据 */
    size_t len;              /**< 数据长度 */
    uint64_t id;             /**< 对象 ID */
    double distance;         /**< 距离 */
} spatial_query_result_t;

/**
 * @brief 空间查询参数
 */
typedef struct spatial_query_args_s {
    bbox_t bbox;              /**< 查询边界框 */
    point_t point;           /**< 查询点（KNN 查询用） */
    uint32_t offset;          /**< 结果偏移 */
    uint32_t limit;           /**< 结果限制 */
    bool is_knn;             /**< 是否 KNN 查询 */
} spatial_query_args_t;

/**
 * @brief 空间引擎数据库
 */
typedef struct spatial_engine_db_s {
    char name[256];            /**< 数据集名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    uint64_t num_geometries;   /**< 几何对象数量 */

    /* R-Tree 索引 */
    void *rtree_index;         /**< R-Tree 索引指针 */
    bool index_built;           /**< 索引是否已构建 */

    /* Hilbert 辅助索引 */
    void *hilbert_index;       /**< Hilbert 辅助索引指针 */
    bool hilbert_built;         /**< Hilbert 索引是否已构建 */
} spatial_engine_db_t;

/* ========================================================================
 * R-Tree 索引 API
 * ======================================================================== */

/**
 * @brief 构建 R-Tree 空间索引
 *
 * @param rel 空间引擎句柄
 * @return 0 成功，-1 失败
 */
int spatial_engine_build_index(void *rel);

/**
 * @brief 使用 R-Tree 进行边界框查询
 *
 * @param rel 空间引擎句柄
 * @param bbox 查询边界框
 * @param results 结果数组（调用者分配）
 * @param max_results 最大结果数
 * @return 匹配数量
 */
int spatial_engine_search_bbox(void *rel, const bbox_t *bbox,
                               void *results, int max_results);

/**
 * @brief 使用 R-Tree 进行最近邻查询
 *
 * @param rel 空间引擎句柄
 * @param point 查询点
 * @param k 返回的最近邻数量
 * @param results 结果数组（调用者分配）
 * @param max_results 数组容量
 * @return 找到的数量
 */
int spatial_engine_search_nearest(void *rel, const point_t *point, int k,
                                   void *results, int max_results);

/**
 * @brief 保存索引到文件
 *
 * @param rel 空间引擎句柄
 * @param path 文件路径（可为 NULL，使用默认路径）
 * @return 0 成功，-1 失败
 */
int spatial_engine_save_index(void *rel, const char *path);

/**
 * @brief 从文件加载索引
 *
 * @param rel 空间引擎句柄
 * @param path 文件路径（可为 NULL，使用默认路径）
 * @return 0 成功，-1 失败
 */
int spatial_engine_load_index(void *rel, const char *path);

/**
 * @brief 删除索引
 *
 * @param rel 空间引擎句柄
 * @return 0 成功，-1 失败
 */
int spatial_engine_drop_index(void *rel);

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
 * Hilbert 辅助索引查询 API
 * ======================================================================== */

/**
 * @brief 使用 Hilbert 辅助索引进行范围查询
 *
 * @param rel 空间引擎句柄
 * @param bbox 查询边界框
 * @param results 结果数组
 * @param max_results 最大结果数
 * @return 匹配数量
 */
int spatial_engine_hilbert_search(void *rel, const bbox_t *bbox,
                                  spatial_query_result_t *results,
                                  uint32_t max_results);

/**
 * @brief 使用 Hilbert 辅助索引进行 KNN 查询
 *
 * @param rel 空间引擎句柄
 * @param point 查询点
 * @param k 返回数量
 * @param results 结果数组
 * @param max_results 数组容量
 * @return 找到的数量
 */
int spatial_engine_hilbert_knn(void *rel, const point_t *point, int k,
                               spatial_query_result_t *results,
                               uint32_t max_results);

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
