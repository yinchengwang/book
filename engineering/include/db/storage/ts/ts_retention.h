/**
 * @file ts_retention.h
 * @brief 时序数据保留策略
 *
 * 支持多级保留策略、数据降采样和自动清理。
 */
#ifndef DB_STORAGE_TS_RETENTION_H
#define DB_STORAGE_TS_RETENTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ts_record_t 定义来自 ts_compress.h */
#include "ts_compress.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 保留策略类型定义
 * ======================================================================== */

/**
 * @brief 保留层级
 */
typedef enum ts_retention_tier_e {
    TS_RETENTION_HOT = 0,    /**< 热数据层（原始数据） */
    TS_RETENTION_WARM = 1,   /**< 温数据层（降采样数据） */
    TS_RETENTION_COLD = 2,   /**< 冷数据层（长期归档） */
    TS_RETENTION_MAX = 3
} ts_retention_tier_t;

/**
 * @brief 降采样聚合函数
 */
typedef enum ts_downsampling_func_e {
    TS_DOWNSAMPLE_AVG = 0,   /**< 平均值 */
    TS_DOWNSAMPLE_MIN = 1,   /**< 最小值 */
    TS_DOWNSAMPLE_MAX = 2,   /**< 最大值 */
    TS_DOWNSAMPLE_SUM = 3,   /**< 求和 */
    TS_DOWNSAMPLE_FIRST = 4, /**< 第一个值 */
    TS_DOWNSAMPLE_LAST = 5,  /**< 最后一个值 */
    TS_DOWNSAMPLE_COUNT = 6  /**< 计数 */
} ts_downsampling_func_t;

/* ========================================================================
 * 兼容旧版 API 的类型定义
 * ======================================================================== */

/**
 * @brief 保留策略配置（兼容旧版）
 *
 * 定义数据的保留策略，包括原始数据和压缩数据的保留天数。
 */
typedef struct ts_retention_policy_s {
    /* 兼容字段 */
    int64_t raw_retention_days;         /**< 原始数据保留天数（0=无限） */
    int64_t compressed_retention_days;   /**< 压缩数据保留天数（0=无限） */
    int64_t compaction_interval_hours;   /**< 压缩检查间隔（小时） */

    /* 新增：多级保留策略 */
    ts_retention_tier_t default_tier;    /**< 默认层级 */
    int64_t hot_ttl_ms;                  /**< 热数据保留时间（毫秒） */
    int64_t warm_ttl_ms;                 /**< 温数据保留时间（毫秒） */
    int64_t cold_ttl_ms;                 /**< 冷数据保留时间（毫秒） */

    /* 降采样规则 */
    int64_t downsample_interval_ms;      /**< 降采样间隔（毫秒），0 表示不降采样 */
    ts_downsampling_func_t downsample_func; /**< 降采样聚合函数 */

    /* 调度 */
    int64_t cleanup_interval_ms;         /**< 清理间隔（毫秒） */
    int64_t last_cleanup_time;           /**< 上次清理时间 */
} ts_retention_policy_t;

/**
 * @brief 保留统计信息
 */
typedef struct ts_retention_stats_s {
    uint64_t total_points;      /**< 总数据点数 */
    uint64_t expired_points;    /**< 已过期数据点数 */
    uint64_t deleted_points;    /**< 已删除数据点数 */
    int64_t last_compaction;    /**< 上次压缩时间（毫秒时间戳） */
} ts_retention_stats_t;

/**
 * @brief 清理结果
 */
typedef struct ts_cleanup_result_s {
    int64_t  cleaned_points;      /**< 清理的数据点数 */
    int64_t  archived_points;     /**< 归档的数据点数 */
    int64_t  downsampled_points;  /**< 降采样的数据点数 */
    int64_t  duration_ms;         /**< 清理耗时 */
    int64_t  next_cleanup_time;   /**< 下次清理时间 */
} ts_cleanup_result_t;

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/**
 * @brief 保留策略默认值
 */
#define TS_RETENTION_DEFAULT_RAW_DAYS 30
#define TS_RETENTION_DEFAULT_COMPRESSED_DAYS 90
#define TS_RETENTION_DEFAULT_COMPACTION_INTERVAL_HOURS 24

/** 多级保留默认值（天） */
#define TS_RETENTION_DEFAULT_HOT_DAYS 7
#define TS_RETENTION_DEFAULT_WARM_DAYS 30
#define TS_RETENTION_DEFAULT_COLD_DAYS 365

/** 毫秒转换 */
#define TS_DAYS_TO_MS(days) ((int64_t)(days) * 24 * 60 * 60 * 1000)
#define TS_HOURS_TO_MS(hours) ((int64_t)(hours) * 60 * 60 * 1000)

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 获取默认保留策略
 * @return 默认保留策略配置
 */
ts_retention_policy_t ts_retention_default_policy(void);

/**
 * @brief 创建多级保留策略
 *
 * @param hot_ttl_days    热数据保留天数（默认 7）
 * @param warm_ttl_days   温数据保留天数（默认 30）
 * @param cold_ttl_days   冷数据保留天数（默认 365）
 * @return 保留策略指针，调用者负责释放
 */
ts_retention_policy_t *ts_retention_policy_create(
    int32_t hot_ttl_days,
    int32_t warm_ttl_days,
    int32_t cold_ttl_days);

/**
 * @brief 释放保留策略
 *
 * @param policy 保留策略
 */
void ts_retention_policy_free(ts_retention_policy_t *policy);

/**
 * @brief 验证保留策略配置是否有效
 * @param policy 保留策略
 * @return true 有效，false 无效
 */
bool ts_retention_policy_valid(const ts_retention_policy_t *policy);

/* ========================================================================
 * 持久化 API
 * ======================================================================== */

/**
 * @brief 从文件加载保留策略
 * @param data_dir 数据目录
 * @param name 指标名称
 * @param policy 输出保留策略
 * @return 0 成功，-1 失败
 */
int ts_retention_load_policy(const char *data_dir, const char *name,
                              ts_retention_policy_t *policy);

/**
 * @brief 保存保留策略到文件
 * @param data_dir 数据目录
 * @param name 指标名称
 * @param policy 保留策略
 * @return 0 成功，-1 失败
 */
int ts_retention_policy_save(const char *data_dir, const char *name,
                              const ts_retention_policy_t *policy);

/* ========================================================================
 * 清理执行 API
 * ======================================================================== */

/**
 * @brief 执行数据清理
 *
 * @param data_dir   数据目录
 * @param policy     保留策略
 * @param now_ms     当前时间戳（毫秒）
 * @param result     输出清理结果（可为 NULL）
 * @return 0 成功，-1 失败
 */
int ts_retention_cleanup(const char *data_dir,
                         ts_retention_policy_t *policy,
                         int64_t now_ms,
                         ts_cleanup_result_t *result);

/**
 * @brief 检查数据是否需要清理
 *
 * @param last_timestamp 最后数据点时间戳
 * @param tier           数据层级
 * @param policy         保留策略
 * @param now_ms         当前时间戳
 * @return true 需要清理
 */
bool ts_retention_need_cleanup(int64_t last_timestamp,
                               ts_retention_tier_t tier,
                               const ts_retention_policy_t *policy,
                               int64_t now_ms);

/**
 * @brief 计算数据的保留截止时间
 *
 * @param tier     数据层级
 * @param policy   保留策略
 * @return 保留截止时间戳（毫秒），0 表示永久保留
 */
int64_t ts_retention_cutoff_time(ts_retention_tier_t tier,
                                  const ts_retention_policy_t *policy);

/**
 * @brief 执行压缩和清理
 *
 * 遍历压缩块，删除过期数据。应该定期调用。
 * @param rel 时序引擎句柄
 * @return 0 成功，-1 失败
 */
int ts_engine_compact(void *rel);

/**
 * @brief 删除过期数据
 *
 * 按时间戳删除所有早于指定时间的记录。
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
 * 降采样 API
 * ======================================================================== */

/**
 * @brief 执行降采样聚合
 *
 * @param records     输入记录数组
 * @param num_records 记录数量
 * @param interval_ms 采样间隔（毫秒）
 * @param func        聚合函数
 * @param out_records 输出记录数组（调用者分配）
 * @param max_out     输出数组最大容量
 * @return 输出的记录数，-1 失败
 */
int32_t ts_retention_downsample(const ts_record_t *records,
                                 int32_t num_records,
                                 int64_t interval_ms,
                                 ts_downsampling_func_t func,
                                 ts_record_t *out_records,
                                 int32_t max_out);

/**
 * @brief 获取降采样函数名称
 *
 * @param func 聚合函数类型
 * @return 函数名称
 */
const char *ts_downsampling_func_name(ts_downsampling_func_t func);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 将天数转换为毫秒
 * @param days 天数
 * @return 对应的毫秒数
 */
int64_t ts_days_to_ms(int64_t days);

/**
 * @brief 将小时转换为毫秒
 * @param hours 小时数
 * @return 对应的毫秒数
 */
int64_t ts_hours_to_ms(int64_t hours);

/**
 * @brief 计算当前时间减去指定天数后的时间戳
 * @param days 天数
 * @return 时间戳（毫秒）
 */
int64_t ts_timestamp_minus_days(int64_t days);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TS_RETENTION_H */
