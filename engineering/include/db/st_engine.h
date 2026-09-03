/**
 * @file st_engine.h
 * @brief 时空存储引擎头文件
 *
 * 组合空间和时间维度，支持时空查询分析。
 * 用于轨迹分析、传感器网络、事件监控等场景。
 */
#ifndef DB_ST_ENGINE_H
#define DB_ST_ENGINE_H

#include "storage_engine.h"
#include "spatial_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 时空相关类型定义
 * ======================================================================== */

/**
 * @brief 时空点（带时间戳的空间点）
 */
typedef struct st_point_s {
    double x;            /**< X 坐标 */
    double y;            /**< Y 坐标 */
    int64_t timestamp;   /**< 时间戳（毫秒） */
} st_point_t;

/**
 * @brief 时空边界框（带时间范围）
 */
typedef struct st_bbox_s {
    double min_x;        /**< 最小 X */
    double min_y;        /**< 最小 Y */
    double max_x;        /**< 最大 X */
    double max_y;        /**< 最大 Y */
    int64_t start_time;  /**< 开始时间 */
    int64_t end_time;    /**< 结束时间 */
} st_bbox_t;

/**
 * @brief 时空查询类型
 */
typedef enum {
    ST_QUERY_BBOX_TIME = 0,      /**< 时空边界框查询 */
    ST_QUERY_NEAREST_TIME,       /**< 时间加权最近邻 */
    ST_QUERY_TRAJECTORY,         /**< 轨迹查询 */
    ST_QUERY_AGGREGATE_TIME,     /**< 时间聚合查询 */
} st_query_type_t;

/**
 * @brief 时空查询结果
 */
typedef struct st_query_result_s {
    void *data;               /**< 数据 */
    size_t len;               /**< 数据长度 */
    uint64_t id;              /**< 对象 ID */
    double distance;          /**< 空间距离 */
    int64_t time_diff;        /**< 时间差 */
    double score;             /**< 综合评分 */
} st_query_result_t;

/**
 * @brief 时空轨迹点
 */
typedef struct st_trajectory_point_s {
    st_point_t point;         /**< 位置和时间 */
    double speed;             /**< 速度 */
    double heading;           /**< 航向角 */
} st_trajectory_point_t;

/* forward declare rtree_t to avoid pulling in rtree.h everywhere */
typedef struct rtree_s rtree_t;

/**
 * @brief 时空引擎数据库
 */
typedef struct st_engine_db_s {
    char name[256];            /**< 数据集名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    uint64_t num_objects;      /**< 对象数量 */
    int64_t start_time;        /**< 开始时间 */
    int64_t end_time;          /**< 结束时间 */
    bbox_t spatial_bounds;     /**< 空间边界 */

    rtree_t *spatial_index;    /**< 空间索引（R-Tree） */
} st_engine_db_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 获取时空引擎操作表
 */
const storage_ops_t *st_engine_get_ops(void);

/**
 * @brief 初始化时空引擎
 */
int st_engine_init(const char *data_dir);

/**
 * @brief 关闭时空引擎
 */
int st_engine_shutdown(void);

/**
 * @brief 创建时空数据集
 */
int st_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开时空数据集
 */
void *st_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭时空数据集
 */
int st_engine_close(void *rel);

/**
 * @brief 删除时空数据集
 */
int st_engine_drop(const char *name);

/**
 * @brief 插入时空对象
 */
int st_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 获取统计信息
 */
int st_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * 时空查询 API
 * ======================================================================== */

/**
 * @brief 时空边界框查询参数
 */
typedef struct st_bbox_time_args_s {
    st_bbox_t bbox;            /**< 时空边界框 */
    uint32_t limit;            /**< 最大返回数量 */
    uint32_t offset;           /**< 跳过数量 */
} st_bbox_time_args_t;

/**
 * @brief 时空边界框查询
 * @param rel 数据集句柄
 * @param args 查询参数
 * @param results 输出结果数组
 * @param num_results 输出结果数量
 * @return 0 成功，-1 失败
 */
int st_engine_query_bbox_time(void *rel, const st_bbox_time_args_t *args,
                               st_query_result_t *results, uint32_t *num_results);

/**
 * @brief 时空最近邻查询
 * @param rel 数据集句柄
 * @param point 参考点
 * @param time_window 时间窗口（毫秒）
 * @param k 返回数量
 * @param results 输出结果数组
 * @param num_results 输出结果数量
 * @return 0 成功，-1 失败
 */
int st_engine_nearest_time(void *rel, const st_point_t *point,
                            int64_t time_window, uint32_t k,
                            st_query_result_t *results, uint32_t *num_results);

/**
 * @brief 轨迹查询（获取指定对象的轨迹）
 * @param rel 数据集句柄
 * @param object_id 对象 ID
 * @param start_time 开始时间
 * @param end_time 结束时间
 * @param points 输出轨迹点数组
 * @param num_points 输出轨迹点数量
 * @return 0 成功，-1 失败
 */
int st_engine_trajectory(void *rel, uint64_t object_id,
                          int64_t start_time, int64_t end_time,
                          st_trajectory_point_t *points, uint32_t *num_points);

/**
 * @brief 时间聚合查询
 * @param rel 数据集句柄
 * @param bbox 空间边界框
 * @param start_time 开始时间
 * @param end_time 结束时间
 * @param bucket_ms 聚合桶大小（毫秒）
 * @param count_results 每个桶的对象数量
 * @param num_buckets 桶数量
 * @return 0 成功，-1 失败
 */
int st_engine_aggregate_time(void *rel, const bbox_t *bbox,
                              int64_t start_time, int64_t end_time,
                              int64_t bucket_ms,
                              uint64_t *count_results, uint32_t *num_buckets);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 创建时空点
 */
st_point_t st_point_create(double x, double y, int64_t timestamp);

/**
 * @brief 创建时空边界框
 */
st_bbox_t st_bbox_create(double min_x, double min_y, double max_x, double max_y,
                          int64_t start_time, int64_t end_time);

/**
 * @brief 检查点是否在时空边界框内
 */
bool st_bbox_contains_point(const st_bbox_t *bbox, const st_point_t *point);

/**
 * @brief 计算两点之间的欧氏距离
 */
double st_distance(const st_point_t *a, const st_point_t *b);

/**
 * @brief 计算时空综合距离（空间距离 + 时间距离加权）
 */
double st_combined_distance(const st_point_t *a, const st_point_t *b,
                             double spatial_weight, double temporal_weight);

#ifdef __cplusplus
}
#endif

#endif /* DB_ST_ENGINE_H */
