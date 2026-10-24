/**
 * @file ts_sql_functions.h
 * @brief 时序数据库 SQL 函数头文件
 *
 * 实现时序专用 SQL 函数：
 * 1. TIME_SERIES_AGG() - 时序聚合
 * 2. FIRST/LAST - 首尾值
 * 3. RATE/DERIVATIVE - 变化率
 * 4. HISTOGRAM/BUCKET - 直方图/桶
 * 5. DATE_TRUNC/EXTRACT - 时间函数
 */
#ifndef DB_TS_SQL_FUNCTIONS_H
#define DB_TS_SQL_FUNCTIONS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 时间粒度常量
 * ======================================================================== */

/** SQL 时间粒度 */
typedef enum SqlTimeGranularity_e {
    SQL_GRANULARITY_SECOND = 0,
    SQL_GRANULARITY_MINUTE = 1,
    SQL_GRANULARITY_HOUR = 2,
    SQL_GRANULARITY_DAY = 3,
    SQL_GRANULARITY_WEEK = 4,
    SQL_GRANULARITY_MONTH = 5,
    SQL_GRANULARITY_QUARTER = 6,
    SQL_GRANULARITY_YEAR = 7
} SqlTimeGranularity;

/**
 * @brief 获取 SQL 时间粒度名称
 */
const char *sql_granularity_name(SqlTimeGranularity g);

/**
 * @brief 解析时间粒度字符串
 */
SqlTimeGranularity sql_parse_granularity(const char *str);

/* ========================================================================
 * TIME_SERIES_AGG 函数
 * ======================================================================== */

/** TIME_SERIES_AGG 聚合类型 */
typedef enum TsAggFunc_e {
    TS_AGG_SUM = 0,
    TS_AGG_AVG = 1,
    TS_AGG_MIN = 2,
    TS_AGG_MAX = 3,
    TS_AGG_COUNT = 4,
    TS_AGG_FIRST = 5,
    TS_AGG_LAST = 6,
    TS_AGG_MEDIAN = 7,
    TS_AGG_STDDEV = 8,
    TS_AGG_RATE = 9,
    TS_AGG_DERIVATIVE = 10
} TsAggFunc;

/** TIME_SERIES_AGG 参数 */
typedef struct TsAggArgs_s {
    const char *measurement;    /**< 度量名 */
    const char *time_col;       /**< 时间列名 */
    const char *value_col;      /**< 值列名 */
    const char *tag_filters;    /**< Tag 过滤条件 (JSON) */
    SqlTimeGranularity bucket;  /**< 时间桶粒度 */
    TsAggFunc func;             /**< 聚合函数 */
} TsAggArgs;

/**
 * @brief 解析 TIME_SERIES_AGG 函数参数
 *
 * @param sql_func SQL 函数调用字符串
 * @return 参数结构，失败返回 NULL
 */
TsAggArgs *ts_agg_parse_args(const char *sql_func);

/**
 * @brief 释放 TIME_SERIES_AGG 参数
 */
void ts_agg_free_args(TsAggArgs *args);

/**
 * @brief 执行 TIME_SERIES_AGG
 *
 * @param args 解析后的参数
 * @param start_time 起始时间
 * @param end_time 结束时间
 * @param out_timestamps 输出时间戳数组
 * @param out_values 输出值数组
 * @param max_results 最大结果数
 * @param out_count 实际结果数
 * @return 0 成功，-1 失败
 */
int ts_agg_execute(const TsAggArgs *args,
                   int64_t start_time, int64_t end_time,
                   int64_t *out_timestamps, double *out_values,
                   uint32_t max_results, uint32_t *out_count);

/* ========================================================================
 * FIRST/LAST 函数
 * ======================================================================== */

/** FIRST 函数参数 */
typedef struct TsFirstLastArgs_s {
    const char *measurement;    /**< 度量名 */
    const char *time_col;       /**< 时间列名 */
    const char *value_col;      /**< 值列名 */
    const char *tag_filters;    /**< Tag 过滤条件 */
    int64_t n;                  /**< 返回前 N 个 (0=全部) */
    bool descending;            /**< 是否降序 (LAST) */
} TsFirstLastArgs;

/**
 * @brief 解析 FIRST 函数参数
 */
TsFirstLastArgs *ts_first_parse_args(const char *sql_func);

/**
 * @brief 解析 LAST 函数参数
 */
TsFirstLastArgs *ts_last_parse_args(const char *sql_func);

/**
 * @brief 释放 FIRST/LAST 参数
 */
void ts_first_last_free_args(TsFirstLastArgs *args);

/**
 * @brief 执行 FIRST 查询
 *
 * @param args 参数
 * @param start_time 起始时间
 * @param end_time 结束时间
 * @param out_timestamps 输出时间戳
 * @param out_values 输出值
 * @param max_results 最大结果数
 * @param out_count 实际结果数
 * @return 0 成功，-1 失败
 */
int ts_first_execute(const TsFirstLastArgs *args,
                     int64_t start_time, int64_t end_time,
                     int64_t *out_timestamps, double *out_values,
                     uint32_t max_results, uint32_t *out_count);

/**
 * @brief 执行 LAST 查询
 */
int ts_last_execute(const TsFirstLastArgs *args,
                    int64_t start_time, int64_t end_time,
                    int64_t *out_timestamps, double *out_values,
                    uint32_t max_results, uint32_t *out_count);

/* ========================================================================
 * RATE/DERIVATIVE 函数
 * ======================================================================== */

/** RATE 函数参数 */
typedef struct TsRateArgs_s {
    const char *measurement;    /**< 度量名 */
    const char *value_col;      /**< 值列名 */
    bool non_negative;          /**< 非负约束 (RATE) */
    bool derivative;            /**< true=DERIVATIVE, false=RATE */
} TsRateArgs;

/**
 * @brief 解析 RATE 函数参数
 */
TsRateArgs *ts_rate_parse_args(const char *sql_func);

/**
 * @brief 解析 DERIVATIVE 函数参数
 */
TsRateArgs *ts_derivative_parse_args(const char *sql_func);

/**
 * @brief 释放 RATE/DERIVATIVE 参数
 */
void ts_rate_free_args(TsRateArgs *args);

/**
 * @brief 执行 RATE/DERIVATIVE
 *
 * @param args 参数
 * @param timestamps 时间戳数组
 * @param values 值数组
 * @param count 数据点数
 * @param out_rates 输出变化率
 * @param out_count 输出数量
 * @return 0 成功，-1 失败
 */
int ts_rate_execute(const TsRateArgs *args,
                    const int64_t *timestamps, const double *values, uint32_t count,
                    double *out_rates, uint32_t *out_count);

/**
 * @brief 计算单个差分
 */
double ts_calculate_derivative(int64_t t1, double v1, int64_t t2, double v2);

/**
 * @brief 计算单个变化率
 */
double ts_calculate_rate(int64_t t1, double v1, int64_t t2, double v2,
                         bool non_negative);

/* ========================================================================
 * HISTOGRAM/BUCKET 函数
 * ======================================================================== */

/** HISTOGRAM 参数 */
typedef struct TsHistogramArgs_s {
    const char *value_col;      /**< 值列名 */
    double bin_width;           /**< 桶宽度 */
    double min_value;           /**< 最小值 (0=自动) */
    double max_value;           /**< 最大值 (0=自动) */
    bool cumulative;            /**< 是否累积直方图 */
} TsHistogramArgs;

/**
 * @brief 解析 HISTOGRAM 函数参数
 */
TsHistogramArgs *ts_histogram_parse_args(const char *sql_func);

/**
 * @brief 释放 HISTOGRAM 参数
 */
void ts_histogram_free_args(TsHistogramArgs *args);

/**
 * @brief 直方图桶
 */
typedef struct HistogramBucket_s {
    double lower_bound;         /**< 下界 */
    double upper_bound;         /**< 上界 */
    uint64_t count;             /**< 计数 */
} HistogramBucket;

/**
 * @brief 执行 HISTOGRAM
 *
 * @param args 参数
 * @param values 值数组
 * @param count 数据点数
 * @param out_buckets 输出桶
 * @param max_buckets 最大桶数
 * @param out_count 实际桶数
 * @return 0 成功，-1 失败
 */
int ts_histogram_execute(const TsHistogramArgs *args,
                         const double *values, uint32_t count,
                         HistogramBucket *out_buckets, uint32_t max_buckets,
                         uint32_t *out_count);

/* ========================================================================
 * 时间函数
 * ======================================================================== */

/** 时间部分 */
typedef enum TimeField_e {
    TIME_YEAR = 0,
    TIME_MONTH = 1,
    TIME_DAY = 2,
    TIME_HOUR = 3,
    TIME_MINUTE = 4,
    TIME_SECOND = 5,
    TIME_DOW = 6,       /**< 一周中的第几天 (0=周日) */
    TIME_DOY = 7,       /**< 一年中的第几天 */
    TIME_EPOCH = 8      /**< Unix 时间戳 */
} TimeField;

/**
 * @brief DATE_TRUNC - 截断时间到指定粒度
 *
 * @param timestamp 输入时间戳
 * @param granularity 截断粒度
 * @return 截断后的时间戳
 */
int64_t date_trunc(int64_t timestamp, SqlTimeGranularity granularity);

/**
 * @brief EXTRACT - 提取时间部分
 *
 * @param timestamp 时间戳
 * @param field 要提取的字段
 * @return 提取的值
 */
int64_t time_extract(int64_t timestamp, TimeField field);

/**
 * @brief DATE_BUCKET - PostgreSQL 风格的日期桶
 *
 * @param timestamp 输入时间戳
 * @param interval 间隔 (毫秒)
 * @param origin 起点时间戳 (0=1970-01-01)
 * @return 桶起始时间戳
 */
int64_t date_bucket(int64_t timestamp, int64_t interval, int64_t origin);

/**
 * @brief 获取当前时间戳
 */
int64_t now_ms(void);

/**
 * @brief 获取当前时间戳 (秒)
 */
int64_t now(void);

/* ========================================================================
 * BUCKET 函数 (InfluxDB 风格)
 * ======================================================================== */

/** BUCKET 函数参数 */
typedef struct TsBucketArgs_s {
    const char *value_col;      /**< 值列名 */
    double bucket_count;        /**< 桶数量 */
    double start;               /**< 起始值 */
    double stop;                /**< 结束值 (0=最大值) */
} TsBucketArgs;

/**
 * @brief 解析 BUCKET 函数参数
 */
TsBucketArgs *ts_bucket_parse_args(const char *sql_func);

/**
 * @brief 释放 BUCKET 参数
 */
void ts_bucket_free_args(TsBucketArgs *args);

/**
 * @brief BUCKET 桶边界
 */
typedef struct BucketBound_s {
    double value;               /**< 桶值 */
    int64_t timestamp;          /**< 对应时间戳 */
    uint32_t bucket_id;         /**< 桶 ID */
} BucketBound;

/**
 * @brief 执行 BUCKET
 *
 * @param args 参数
 * @param values 值数组
 * @param timestamps 时间戳数组
 * @param count 数据点数
 * @param out_bounds 输出桶边界
 * @param max_buckets 最大桶数
 * @param out_count 实际桶数
 * @return 0 成功，-1 失败
 */
int ts_bucket_execute(const TsBucketArgs *args,
                      const double *values, const int64_t *timestamps, uint32_t count,
                      BucketBound *out_bounds, uint32_t max_buckets, uint32_t *out_count);

/**
 * @brief 获取值对应的桶 ID
 */
uint32_t get_bucket_id(double value, double bucket_width, double start);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_SQL_FUNCTIONS_H */
