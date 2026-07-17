/**
 * @file ts_mview.h
 * @brief 时序数据物化视图
 *
 * 预计算聚合结果，支持 AVG/MIN/MAX/COUNT/PERCENTILE 等聚合函数。
 * 物化视图可大幅加速聚合查询。
 */
#ifndef DB_TS_MVIEW_H
#define DB_TS_MVIEW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 物化视图魔数 */
#define TS_MVIEW_MAGIC 0x54535657  /* "TSVW" */

/** 物化视图版本 */
#define TS_MVIEW_VERSION 1

/** 支持的聚合函数 */
typedef enum {
    TS_AGG_SUM = 0,
    TS_AGG_AVG = 1,
    TS_AGG_MIN = 2,
    TS_AGG_MAX = 3,
    TS_AGG_COUNT = 4,
    TS_AGG_PERCENTILE = 5,
} ts_agg_func_t;

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 物化视图记录
 */
typedef struct ts_mview_record_s {
    int64_t timestamp;     /**< 对齐到粒度边界的时间戳 */
    double value;          /**< 聚合结果值 */
    uint32_t count;        /**< 参与聚合的数据点数 */
    double min_val;        /**< 粒度内最小值 */
    double max_val;        /**< 粒度内最大值 */
    double sum;            /**< 累计和 */
} ts_mview_record_t;

/**
 * @brief 物化视图
 */
typedef struct ts_mview_s {
    char name[64];                 /**< 视图名 */
    char data_dir[512];            /**< 数据目录 */
    ts_agg_func_t func;            /**< 聚合函数 */
    int32_t granularity;           /**< 时间粒度（毫秒） */
    int64_t percentile;            /**< 百分位数（用于 PERCENTILE） */

    /* 数据 */
    ts_mview_record_t *records;    /**< 物化记录数组 */
    uint32_t count;                /**< 记录数 */
    uint32_t capacity;             /**< 容量 */

    /* 元数据 */
    int64_t last_refresh;          /**< 上次刷新时间 */
    int64_t range_start;           /**< 数据时间范围起始 */
    int64_t range_end;             /**< 数据时间范围结束 */
} ts_mview_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建物化视图
 *
 * @param name 视图名
 * @param data_dir 数据目录
 * @param func 聚合函数
 * @param granularity 时间粒度（毫秒）
 * @return 物化视图句柄，失败返回 NULL
 */
ts_mview_t *ts_mview_create(const char *name, const char *data_dir,
                            ts_agg_func_t func, int32_t granularity);

/**
 * @brief 打开已存在的物化视图
 *
 * @param data_dir 数据目录
 * @param name 视图名
 * @return 物化视图句柄，失败返回 NULL
 */
ts_mview_t *ts_mview_open(const char *data_dir, const char *name);

/**
 * @brief 保存物化视图
 *
 * @param mview 物化视图
 * @return 0 成功，-1 失败
 */
int ts_mview_save(ts_mview_t *mview);

/**
 * @brief 关闭并释放物化视图
 *
 * @param mview 物化视图
 */
void ts_mview_destroy(ts_mview_t *mview);

/* ========================================================================
 * 数据操作 API
 * ======================================================================== */

/**
 * @brief 添加数据点
 *
 * @param mview 物化视图
 * @param timestamp 时间戳
 * @param value 值
 * @return 0 成功，-1 失败
 */
int ts_mview_add(ts_mview_t *mview, int64_t timestamp, double value);

/**
 * @brief 刷新物化视图
 *
 * 根据当前缓冲区数据计算聚合结果。
 *
 * @param mview 物化视图
 * @return 0 成功，-1 失败
 */
int ts_mview_refresh(ts_mview_t *mview);

/**
 * @brief 查询物化视图
 *
 * @param mview 物化视图
 * @param start_ts 起始时间戳
 * @param end_ts 结束时间戳
 * @param results 结果数组（调用者分配）
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t ts_mview_query(const ts_mview_t *mview,
                        int64_t start_ts, int64_t end_ts,
                        ts_mview_record_t *results,
                        uint32_t max_results);

/* ========================================================================
 * 聚合计算 API
 * ======================================================================== */

/**
 * @brief 计算聚合值
 *
 * @param records 记录数组
 * @param count 记录数
 * @param func 聚合函数
 * @return 聚合结果
 */
double ts_mview_aggregate(const ts_mview_record_t *records, uint32_t count,
                          ts_agg_func_t func);

/**
 * @brief 计算百分位数
 *
 * @param records 记录数组
 * @param count 记录数
 * @param percentile 百分位数（0-100）
 * @return 百分位值
 */
double ts_mview_percentile(const ts_mview_record_t *records, uint32_t count,
                           double percentile);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_MVIEW_H */
