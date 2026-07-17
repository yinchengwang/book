/**
 * @file ts_sql_functions.c
 * @brief 时序数据库 SQL 函数实现
 */
#include "ts_sql_functions.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>

/* ========================================================================
 * 时间粒度
 * ======================================================================== */

const char *sql_granularity_name(SqlTimeGranularity g) {
    switch (g) {
        case SQL_GRANULARITY_SECOND: return "second";
        case SQL_GRANULARITY_MINUTE: return "minute";
        case SQL_GRANULARITY_HOUR: return "hour";
        case SQL_GRANULARITY_DAY: return "day";
        case SQL_GRANULARITY_WEEK: return "week";
        case SQL_GRANULARITY_MONTH: return "month";
        case SQL_GRANULARITY_QUARTER: return "quarter";
        case SQL_GRANULARITY_YEAR: return "year";
        default: return "unknown";
    }
}

SqlTimeGranularity sql_parse_granularity(const char *str) {
    if (!str) return SQL_GRANULARITY_SECOND;

    if (strcmp(str, "second") == 0 || strcmp(str, "s") == 0)
        return SQL_GRANULARITY_SECOND;
    if (strcmp(str, "minute") == 0 || strcmp(str, "m") == 0)
        return SQL_GRANULARITY_MINUTE;
    if (strcmp(str, "hour") == 0 || strcmp(str, "h") == 0)
        return SQL_GRANULARITY_HOUR;
    if (strcmp(str, "day") == 0 || strcmp(str, "d") == 0)
        return SQL_GRANULARITY_DAY;
    if (strcmp(str, "week") == 0 || strcmp(str, "w") == 0)
        return SQL_GRANULARITY_WEEK;
    if (strcmp(str, "month") == 0)
        return SQL_GRANULARITY_MONTH;
    if (strcmp(str, "quarter") == 0 || strcmp(str, "q") == 0)
        return SQL_GRANULARITY_QUARTER;
    if (strcmp(str, "year") == 0 || strcmp(str, "y") == 0)
        return SQL_GRANULARITY_YEAR;

    return SQL_GRANULARITY_SECOND;
}

/* ========================================================================
 * TIME_SERIES_AGG 函数
 * ======================================================================== */

TsAggArgs *ts_agg_parse_args(const char *sql_func) {
    if (!sql_func) return NULL;

    TsAggArgs *args = (TsAggArgs *)calloc(1, sizeof(TsAggArgs));
    if (!args) return NULL;

    /* 简化解析：提取关键参数 */
    /* 格式：TIME_SERIES_AGG(measurement, time_col, value_col, bucket, func) */

    /* 默认值 */
    args->bucket = SQL_GRANULARITY_MINUTE;
    args->func = TS_AGG_AVG;

    return args;
}

void ts_agg_free_args(TsAggArgs *args) {
    free(args);
}

int ts_agg_execute(const TsAggArgs *args,
                   int64_t start_time, int64_t end_time,
                   int64_t *out_timestamps, double *out_values,
                   uint32_t max_results, uint32_t *out_count) {
    if (!args || !out_count) return -1;

    /* 简化实现：返回空结果 */
    *out_count = 0;
    (void)start_time;
    (void)end_time;
    (void)out_timestamps;
    (void)out_values;
    (void)max_results;

    return 0;
}

/* ========================================================================
 * FIRST/LAST 函数
 * ======================================================================== */

TsFirstLastArgs *ts_first_parse_args(const char *sql_func) {
    if (!sql_func) return NULL;

    TsFirstLastArgs *args = (TsFirstLastArgs *)calloc(1, sizeof(TsFirstLastArgs));
    if (!args) return NULL;

    args->descending = false;
    args->n = 1;

    return args;
}

TsFirstLastArgs *ts_last_parse_args(const char *sql_func) {
    TsFirstLastArgs *args = ts_first_parse_args(sql_func);
    if (args) {
        args->descending = true;
    }
    return args;
}

void ts_first_last_free_args(TsFirstLastArgs *args) {
    free(args);
}

int ts_first_execute(const TsFirstLastArgs *args,
                     int64_t start_time, int64_t end_time,
                     int64_t *out_timestamps, double *out_values,
                     uint32_t max_results, uint32_t *out_count) {
    if (!args || !out_count) return -1;

    /* 简化实现：返回空 */
    *out_count = 0;
    (void)start_time;
    (void)end_time;
    (void)out_timestamps;
    (void)out_values;
    (void)max_results;

    return 0;
}

int ts_last_execute(const TsFirstLastArgs *args,
                    int64_t start_time, int64_t end_time,
                    int64_t *out_timestamps, double *out_values,
                    uint32_t max_results, uint32_t *out_count) {
    return ts_first_execute(args, start_time, end_time,
                            out_timestamps, out_values, max_results, out_count);
}

/* ========================================================================
 * RATE/DERIVATIVE 函数
 * ======================================================================== */

TsRateArgs *ts_rate_parse_args(const char *sql_func) {
    if (!sql_func) return NULL;

    TsRateArgs *args = (TsRateArgs *)calloc(1, sizeof(TsRateArgs));
    if (!args) return NULL;

    args->non_negative = true;
    args->derivative = false;

    return args;
}

TsRateArgs *ts_derivative_parse_args(const char *sql_func) {
    TsRateArgs *args = ts_rate_parse_args(sql_func);
    if (args) {
        args->non_negative = false;
        args->derivative = true;
    }
    return args;
}

void ts_rate_free_args(TsRateArgs *args) {
    free(args);
}

double ts_calculate_derivative(int64_t t1, double v1, int64_t t2, double v2) {
    int64_t dt = t2 - t1;
    if (dt == 0) return 0.0;
    return (v2 - v1) / (double)dt;
}

double ts_calculate_rate(int64_t t1, double v1, int64_t t2, double v2,
                         bool non_negative) {
    double rate = ts_calculate_derivative(t1, v1, t2, v2);
    if (non_negative && rate < 0) rate = 0;
    return rate;
}

int ts_rate_execute(const TsRateArgs *args,
                    const int64_t *timestamps, const double *values, uint32_t count,
                    double *out_rates, uint32_t *out_count) {
    if (!args || !timestamps || !values || !out_count) return -1;

    uint32_t n = 0;
    for (uint32_t i = 1; i < count && n < count - 1; i++) {
        if (args->derivative) {
            out_rates[n] = ts_calculate_derivative(
                timestamps[i - 1], values[i - 1],
                timestamps[i], values[i]);
        } else {
            out_rates[n] = ts_calculate_rate(
                timestamps[i - 1], values[i - 1],
                timestamps[i], values[i],
                args->non_negative);
        }
        n++;
    }

    *out_count = n;
    return 0;
}

/* ========================================================================
 * HISTOGRAM 函数
 * ======================================================================== */

TsHistogramArgs *ts_histogram_parse_args(const char *sql_func) {
    if (!sql_func) return NULL;

    TsHistogramArgs *args = (TsHistogramArgs *)calloc(1, sizeof(TsHistogramArgs));
    if (!args) return NULL;

    args->bin_width = 1.0;
    args->min_value = 0;
    args->max_value = 0;
    args->cumulative = false;

    return args;
}

void ts_histogram_free_args(TsHistogramArgs *args) {
    free(args);
}

int ts_histogram_execute(const TsHistogramArgs *args,
                         const double *values, uint32_t count,
                         HistogramBucket *out_buckets, uint32_t max_buckets,
                         uint32_t *out_count) {
    if (!args || !values || !out_count) return -1;

    /* 计算范围 */
    double min_val = args->min_value;
    double max_val = args->max_value;

    if (min_val == 0 && max_val == 0) {
        min_val = DBL_MAX;
        max_val = -DBL_MAX;
        for (uint32_t i = 0; i < count; i++) {
            if (values[i] < min_val) min_val = values[i];
            if (values[i] > max_val) max_val = values[i];
        }
    }

    /* 计算桶数 */
    double bin_width = args->bin_width;
    uint32_t num_buckets = (uint32_t)ceil((max_val - min_val) / bin_width);
    if (num_buckets > max_buckets) num_buckets = max_buckets;

    /* 初始化桶 */
    for (uint32_t i = 0; i < num_buckets; i++) {
        out_buckets[i].lower_bound = min_val + i * bin_width;
        out_buckets[i].upper_bound = out_buckets[i].lower_bound + bin_width;
        out_buckets[i].count = 0;
    }

    /* 分配值到桶 */
    for (uint32_t i = 0; i < count; i++) {
        int bucket_idx = (int)((values[i] - min_val) / bin_width);
        if (bucket_idx < 0) bucket_idx = 0;
        if (bucket_idx >= (int)num_buckets) bucket_idx = num_buckets - 1;
        out_buckets[bucket_idx].count++;
    }

    /* 累积直方图 */
    if (args->cumulative) {
        for (uint32_t i = 1; i < num_buckets; i++) {
            out_buckets[i].count += out_buckets[i - 1].count;
        }
    }

    *out_count = num_buckets;
    return 0;
}

/* ========================================================================
 * 时间函数
 * ======================================================================== */

/** 获取粒度对应的毫秒数 */
static int64_t granularity_to_ms(SqlTimeGranularity g) {
    switch (g) {
        case SQL_GRANULARITY_SECOND: return 1000;
        case SQL_GRANULARITY_MINUTE: return 60000;
        case SQL_GRANULARITY_HOUR: return 3600000;
        case SQL_GRANULARITY_DAY: return 86400000;
        case SQL_GRANULARITY_WEEK: return 604800000;
        case SQL_GRANULARITY_MONTH: return 2592000000LL;  /* 30 天 */
        case SQL_GRANULARITY_QUARTER: return 7776000000LL; /* 90 天 */
        case SQL_GRANULARITY_YEAR: return 31536000000LL;  /* 365 天 */
        default: return 1000;
    }
}

int64_t date_trunc(int64_t timestamp, SqlTimeGranularity granularity) {
    int64_t ms = granularity_to_ms(granularity);

    switch (granularity) {
        case SQL_GRANULARITY_MONTH: {
            /* 特殊处理月份 */
            time_t t = timestamp / 1000;
            struct tm *tm_info = localtime(&t);
            if (tm_info) {
                tm_info->tm_mday = 1;
                tm_info->tm_hour = 0;
                tm_info->tm_min = 0;
                tm_info->tm_sec = 0;
                return (int64_t)mktime(tm_info) * 1000;
            }
            break;
        }

        case SQL_GRANULARITY_YEAR: {
            time_t t = timestamp / 1000;
            struct tm *tm_info = localtime(&t);
            if (tm_info) {
                tm_info->tm_mon = 0;
                tm_info->tm_mday = 1;
                tm_info->tm_hour = 0;
                tm_info->tm_min = 0;
                tm_info->tm_sec = 0;
                return (int64_t)mktime(tm_info) * 1000;
            }
            break;
        }

        default:
            return (timestamp / ms) * ms;
    }

    return timestamp;
}

int64_t time_extract(int64_t timestamp, TimeField field) {
    time_t t = timestamp / 1000;
    struct tm *tm_info = localtime(&t);
    if (!tm_info) return 0;

    switch (field) {
        case TIME_YEAR: return tm_info->tm_year + 1900;
        case TIME_MONTH: return tm_info->tm_mon + 1;
        case TIME_DAY: return tm_info->tm_mday;
        case TIME_HOUR: return tm_info->tm_hour;
        case TIME_MINUTE: return tm_info->tm_min;
        case TIME_SECOND: return tm_info->tm_sec;
        case TIME_DOW: return tm_info->tm_wday;
        case TIME_DOY: return tm_info->tm_yday + 1;
        case TIME_EPOCH: return (int64_t)t;
        default: return 0;
    }
}

int64_t date_bucket(int64_t timestamp, int64_t interval, int64_t origin) {
    if (interval <= 0) return timestamp;
    if (origin == 0) origin = 0; /* 默认起点 1970-01-01 */

    int64_t offset = timestamp - origin;
    if (offset < 0) {
        int64_t n = (-offset) / interval + 1;
        return origin - n * interval;
    }
    return origin + (offset / interval) * interval;
}

int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int64_t now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec;
}

/* ========================================================================
 * BUCKET 函数
 * ======================================================================== */

TsBucketArgs *ts_bucket_parse_args(const char *sql_func) {
    if (!sql_func) return NULL;

    TsBucketArgs *args = (TsBucketArgs *)calloc(1, sizeof(TsBucketArgs));
    if (!args) return NULL;

    args->bucket_count = 10;
    args->start = 0;
    args->stop = 0;

    return args;
}

void ts_bucket_free_args(TsBucketArgs *args) {
    free(args);
}

uint32_t get_bucket_id(double value, double bucket_width, double start) {
    if (bucket_width <= 0) return 0;
    int32_t id = (int32_t)((value - start) / bucket_width);
    return id < 0 ? 0 : id;
}

int ts_bucket_execute(const TsBucketArgs *args,
                      const double *values, const int64_t *timestamps, uint32_t count,
                      BucketBound *out_bounds, uint32_t max_buckets, uint32_t *out_count) {
    if (!args || !values || !out_count) return -1;

    /* 计算范围 */
    double min_val = args->start;
    double max_val = args->stop;

    if (max_val == 0) {
        max_val = min_val;
        for (uint32_t i = 0; i < count; i++) {
            if (values[i] > max_val) max_val = values[i];
        }
    }

    /* 计算桶宽 */
    double bucket_width = (max_val - min_val) / args->bucket_count;
    if (bucket_width <= 0) bucket_width = 1;

    /* 初始化桶边界 */
    uint32_t num_buckets = (uint32_t)args->bucket_count;
    if (num_buckets > max_buckets) num_buckets = max_buckets;

    for (uint32_t i = 0; i < num_buckets; i++) {
        out_bounds[i].value = min_val + (i + 0.5) * bucket_width;
        out_bounds[i].bucket_id = i;
        out_bounds[i].timestamp = 0;
    }

    /* 分配值到桶并记录时间戳 */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t bucket_id = get_bucket_id(values[i], bucket_width, min_val);
        if (bucket_id >= num_buckets) bucket_id = num_buckets - 1;

        /* 记录该桶的第一个时间戳 */
        if (out_bounds[bucket_id].timestamp == 0) {
            out_bounds[bucket_id].timestamp = timestamps ? timestamps[i] : 0;
        }
    }

    *out_count = num_buckets;
    return 0;
}
