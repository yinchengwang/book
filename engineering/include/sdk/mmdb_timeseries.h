/**
 * @file mmdb_timeseries.h
 * @brief 时序模型 API
 */
#ifndef SDK_MMDB_TIMESERIES_H
#define SDK_MMDB_TIMESERIES_H

#include "sdk/mmdb.h"
#include "sdk/mmdb_aggregate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 时序聚合表达式（滑动窗口） */
typedef struct {
    const char*     field;          /* 聚合字段 */
    mmdb_agg_type_t type;           /* 聚合类型 */
    uint64_t        window_ms;      /* 窗口大小（毫秒） */
    uint64_t        slide_ms;       /* 滑动步长（毫秒，0 = 不滑动） */
} mmdb_ts_agg_expr_t;

/* 时序聚合查询 */
typedef struct {
    uint64_t        start_time;     /* 查询起始时间 */
    uint64_t        end_time;       /* 查询结束时间 */
    mmdb_ts_agg_expr_t aggs[4];     /* 聚合表达式（最多 4 个） */
    size_t          agg_count;
    bool            fill_empty;     /* 空窗口是否补零 */
} mmdb_ts_aggregate_query_t;

int mmdb_timeseries_append(mmdb_collection_t* c, const mmdb_datapoint_t* dp);
int mmdb_timeseries_append_batch(mmdb_collection_t* c, const mmdb_datapoint_t* dps,
                                   size_t n);
int mmdb_timeseries_query(mmdb_collection_t* c, const mmdb_ts_query_t* q,
                           mmdb_result_t* out);

/**
 * @brief 时序聚合查询（滑动窗口）
 * @param c         collection 句柄
 * @param query     聚合查询参数
 * @param result    输出聚合结果集
 * @return 0 成功，非 0 错误码
 */
int mmdb_ts_aggregate(mmdb_collection_t* c, const mmdb_ts_aggregate_query_t* query,
                      mmdb_aggregate_result_set_t** result);

#ifdef __cplusplus
}
#endif

#endif