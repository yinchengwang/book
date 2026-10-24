/**
 * @file ts_engine.h
 * @brief 时序存储引擎头文件
 *
 * 定义时序存储引擎的接口和数据结构，支持时序数据插入和聚合查询。
 */
#ifndef DB_TS_ENGINE_H
#define DB_TS_ENGINE_H

#include "storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 时序相关类型定义
 * ======================================================================== */

/**
 * @brief 聚合函数类型
 */
typedef enum {
    AGG_SUM = 0,      /**< 求和 */
    AGG_AVG = 1,      /**< 平均值 */
    AGG_MIN = 2,      /**< 最小值 */
    AGG_MAX = 3,      /**< 最大值 */
    AGG_COUNT = 4,    /**< 计数 */
} ts_aggregate_func_t;

/**
 * @brief 时间粒度
 *
 * 如果 ts_segment.h 已先被包含，则使用其定义；
 * 否则在此处定义。
 */
#ifndef TS_GRANULARITY_DEFINED
#define TS_GRANULARITY_DEFINED
typedef enum {
    TS_GRANULARITY_SECOND = 0,
    TS_GRANULARITY_MINUTE = 1,
    TS_GRANULARITY_HOUR = 2,
    TS_GRANULARITY_DAY = 3,
} ts_granularity_t;
#endif

/**
 * @brief 聚合结果
 */
typedef struct ts_aggregate_result_s {
    int64_t timestamp;        /**< 时间戳 */
    double value;             /**< 聚合值 */
    uint64_t count;          /**< 数据点数量 */
    double sum;              /**< 累计和 */
    double min;              /**< 最小值 */
    double max;              /**< 最大值 */
    double avg;              /**< 平均值 */
} ts_aggregate_result_t;

/**
 * @brief 查询结果集
 */
typedef struct ts_query_results_s {
    ts_aggregate_result_t *results;   /**< 结果数组 */
    int32_t count;                    /**< 结果数量 */
    int32_t capacity;                 /**< 容量 */
} ts_query_results_t;

/**
 * @brief 时序引擎数据库
 */
typedef struct ts_engine_db_s {
    char name[256];            /**< 指标名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    int64_t start_time;        /**< 开始时间 */
    int64_t end_time;          /**< 结束时间 */
    uint64_t num_points;       /**< 数据点数量 */
} ts_engine_db_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 获取时序引擎操作表
 */
const storage_ops_t *ts_engine_get_ops(void);

/**
 * @brief 初始化时序引擎
 */
int ts_engine_init(const char *data_dir);

/**
 * @brief 关闭时序引擎
 */
int ts_engine_shutdown(void);

/**
 * @brief 创建时序指标
 */
int ts_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开时序指标
 */
void *ts_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭时序指标
 */
int ts_engine_close(void *rel);

/**
 * @brief 删除时序指标
 */
int ts_engine_drop(const char *name);

/**
 * @brief 插入数据点
 */
int ts_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 查询聚合数据
 */
int ts_engine_query(void *rel,
                    int64_t start_time, int64_t end_time,
                    ts_granularity_t granularity,
                    ts_aggregate_func_t func,
                    ts_query_results_t *results);

/**
 * @brief 释放查询结果
 */
void ts_engine_free_results(ts_query_results_t *results);

/**
 * @brief 获取统计信息
 */
int ts_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取时间粒度对应的毫秒数
 */
int64_t ts_granularity_to_ms(ts_granularity_t granularity);

/**
 * @brief 将时间戳对齐到粒度边界
 */
int64_t ts_align_timestamp(int64_t timestamp, ts_granularity_t granularity);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_ENGINE_H */
