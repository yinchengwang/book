/**
 * @file ts_engine.h
 * @brief 时序存储引擎头文件
 *
 * 定义时序存储引擎的接口和数据结构，支持时序数据插入和聚合查询。
 */
#ifndef DB_TS_ENGINE_H
#define DB_TS_ENGINE_H

#include "storage_engine.h"
#include "db/mm_pool.h"
#include "db/storage/ts/ts_segment.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct lock_manager_s lock_manager_t;

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

/* 注意: ts_granularity_t 已从 ts_segment.h 中定义 */
/* 使用 TS_GRANULARITY_SECOND, TS_GRANULARITY_MINUTE, TS_GRANULARITY_HOUR, TS_GRANULARITY_DAY */

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

    /* 内存池相关 */
    void *mem_pool;            /**< 内存池指针 */
    bool use_mem_pool;         /**< 是否使用内存池 */

    /* 分段索引相关 */
    void *segment_index;       /**< 分段索引指针 */
    bool use_segment_index;    /**< 是否使用分段索引 */

    /* 压缩相关 */
    void *compressor;          /**< 压缩器指针（ts_compressor_t*） */
    bool compression_enabled;  /**< 是否启用压缩 */

    /* 保留策略相关 */
    int64_t raw_retention_days;         /**< 原始数据保留天数（0=无限） */
    int64_t compressed_retention_days;   /**< 压缩数据保留天数 */
    int64_t compaction_interval_hours;   /**< 压缩检查间隔（小时） */
    int64_t last_compaction_time;        /**< 上次压缩时间 */
    uint64_t expired_points;             /**< 过期数据点数 */
    uint64_t deleted_points;             /**< 已删除数据点数 */

    /* 并发控制 */
    lock_manager_t *lockmgr;     /**< 锁管理器 */
    void *rwlock;                /**< 读写锁 */
    bool use_lock;               /**< 是否启用锁 */
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
 * 压缩相关 API
 * ======================================================================== */

/**
 * @brief 启用数据压缩
 * @param rel 时序引擎句柄
 * @return 0 成功，-1 失败
 */
int ts_engine_enable_compression(void *rel);

/**
 * @brief 禁用数据压缩
 * @param rel 时序引擎句柄
 * @return 0 成功，-1 失败
 */
int ts_engine_disable_compression(void *rel);

/**
 * @brief 获取压缩统计信息
 * @param rel 时序引擎句柄
 * @param total_points 总数据点数
 * @param compression_ratio 压缩比（原始大小/压缩大小）
 * @return 0 成功，-1 失败
 */
int ts_engine_compression_stats(void *rel, uint64_t *total_points, double *compression_ratio);

/* ========================================================================
 * 保留策略相关 API
 * ======================================================================== */

/* 前向声明：保留策略结构体在 ts_retention.h 中定义 */
struct ts_retention_policy_s;
typedef struct ts_retention_policy_s ts_retention_policy_t;

/**
 * @brief 设置保留策略
 * @param rel 时序引擎句柄
 * @param policy 保留策略配置
 * @return 0 成功，-1 失败
 */
int ts_engine_set_retention(void *rel, const ts_retention_policy_t *policy);

/**
 * @brief 获取保留策略
 * @param rel 时序引擎句柄
 * @param policy 输出保留策略
 * @return 0 成功，-1 失败
 */
int ts_engine_get_retention(void *rel, ts_retention_policy_t *policy);

/**
 * @brief 执行压缩和清理
 * @param rel 时序引擎句柄
 * @return 0 成功，-1 失败
 */
int ts_engine_compact(void *rel);

/**
 * @brief 删除过期数据
 * @param rel 时序引擎句柄
 * @param older_than_ms 删除此时间戳之前的记录（毫秒）
 * @return 删除的记录数，失败返回 -1
 */
int ts_engine_expire(void *rel, int64_t older_than_ms);

/**
 * @brief 获取保留统计信息
 * @param rel 时序引擎句柄
 * @param total_points 输出总数据点数
 * @param expired_points 输出过期数据点数
 * @param deleted_points 输出已删除数据点数
 * @return 0 成功，-1 失败
 */
int ts_engine_retention_stats(void *rel, uint64_t *total_points,
                               uint64_t *expired_points, uint64_t *deleted_points);

/* ========================================================================
 * 分段索引相关 API
 * ======================================================================== */

/**
 * @brief 启用分段索引
 *
 * @param rel 时序引擎句柄
 * @param seg_size 每段数据点数
 * @param granularity 时间粒度
 * @return 0 成功，-1 失败
 */
int ts_engine_enable_segment_index(void *rel, uint32_t seg_size, int granularity);

/**
 * @brief 获取分段索引统计
 *
 * @param rel 时序引擎句柄
 * @param seg_count 输出段数
 * @param total_points 输出总数据点数
 * @return 0 成功，-1 失败
 */
int ts_engine_segment_stats(void *rel, uint32_t *seg_count, uint64_t *total_points);

/**
 * @brief 使用分段索引进行时间范围查询
 *
 * @param rel 时序引擎句柄
 * @param start_time 起始时间戳
 * @param end_time 结束时间戳
 * @param results 查询结果
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t ts_engine_segment_query(void *rel, int64_t start_time, int64_t end_time,
                                  void *results, uint32_t max_results);

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

/* ========================================================================
 * 内存池管理 API
 * ======================================================================== */

/**
 * @brief 启用/禁用内存池
 *
 * @param rel 时序引擎句柄
 * @param use_pool true 启用内存池，false 禁用
 * @return 0 成功，-1 失败
 */
int ts_engine_enable_mem_pool(void *rel, bool use_pool);

/**
 * @brief 获取内存池统计信息
 *
 * @param rel 时序引擎句柄
 * @param stats 输出统计信息
 * @return 0 成功，-1 失败
 */
int ts_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats);

/* ========================================================================
 * 并发锁控制 API
 * ======================================================================== */

/**
 * @brief 启用/禁用并发锁
 *
 * @param rel 时序引擎句柄
 * @param use_lock true 启用锁，false 禁用
 * @return 0 成功，-1 失败
 */
int ts_engine_enable_lock(void *rel, bool use_lock);

/**
 * @brief 获取时序引擎的读锁
 *
 * @param rel 时序引擎句柄
 * @return 0 成功，-1 失败
 */
int ts_engine_read_lock(void *rel);

/**
 * @brief 释放时序引擎的读锁
 *
 * @param rel 时序引擎句柄
 */
void ts_engine_read_unlock(void *rel);

/**
 * @brief 获取时序引擎的写锁
 *
 * @param rel 时序引擎句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，LOCK_TIMEOUT 超时，-1 失败
 */
int ts_engine_write_lock(void *rel, uint32_t timeout_ms);

/**
 * @brief 释放时序引擎的写锁
 *
 * @param rel 时序引擎句柄
 */
void ts_engine_write_unlock(void *rel);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_ENGINE_H */
