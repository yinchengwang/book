/**
 * @file mmdb_aggregate.h
 * @brief 通用聚合框架 API
 *
 * 提供 COUNT/SUM/AVG/MIN/MAX 聚合函数、GROUP BY 分组、
 * HISTOGRAM 直方图以及分页支持。
 */
#ifndef SDK_MMDB_AGGREGATE_H
#define SDK_MMDB_AGGREGATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct mmdb_collection_s mmdb_collection_t;

/* 聚合类型 */
typedef enum {
    MMDB_AGG_COUNT,
    MMDB_AGG_SUM,
    MMDB_AGG_AVG,
    MMDB_AGG_MIN,
    MMDB_AGG_MAX,
    MMDB_AGG_HISTOGRAM,
} mmdb_agg_type_t;

/* 聚合表达式 */
typedef struct {
    const char*     field;          /* 聚合字段（metadata 字段名） */
    mmdb_agg_type_t type;           /* 聚合类型 */
    const char*     alias;          /* 输出别名 */
    /* 仅 HISTOGRAM */
    uint32_t        bucket_count;   /* bucket 数量 */
    double          bucket_min;     /* bucket 下界 */
    double          bucket_max;     /* bucket 上界 */
} mmdb_agg_expr_t;

/* 聚合查询 */
typedef struct {
    const char*     group_by;       /* 分组字段（NULL = 全局聚合） */
    mmdb_agg_expr_t aggs[8];        /* 聚合表达式（最多 8 个） */
    size_t          agg_count;      /* 实际聚合表达式数 */
    uint32_t        offset;         /* 分页偏移 */
    uint32_t        limit;          /* 分页限制（0 = 无限制） */
} mmdb_aggregate_query_t;

/* 聚合结果（单组） */
typedef struct {
    char            key[256];       /* group_by 值 */
    uint64_t        count;          /* 该组数量 */
    double          sum;            /* SUM 结果 */
    double          avg;            /* AVG 结果 */
    double          min;            /* MIN 结果 */
    double          max;            /* MAX 结果 */
    uint32_t*       histogram_buckets; /* HISTOGRAM buckets */
    uint32_t        histogram_bucket_count;
} mmdb_aggregate_result_t;

/* 聚合结果集 */
typedef struct {
    mmdb_aggregate_result_t* groups;    /* 分组结果数组 */
    size_t                  group_count;
    uint64_t                total_count;
    bool                    has_more;
} mmdb_aggregate_result_set_t;

/**
 * @brief 执行聚合查询
 * @param c         collection 句柄
 * @param query     聚合查询
 * @param filter    过滤条件（JSON，NULL 表示不过滤）
 * @param result    输出结果
 * @return 0 成功，非 0 错误码
 */
int mmdb_aggregate(mmdb_collection_t* c, const mmdb_aggregate_query_t* query,
                   const char* filter, mmdb_aggregate_result_set_t** result);

/**
 * @brief 释放聚合结果
 */
void mmdb_aggregate_result_free(mmdb_aggregate_result_set_t* result);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_AGGREGATE_H */
