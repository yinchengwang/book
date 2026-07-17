/**
 * @file doc_agg.h
 * @brief 文档聚合框架头文件
 *
 * 实现词条聚合、范围聚合、直方图聚合、统计聚合、百分位数聚合、管道聚合
 */
#ifndef DB_DOC_AGG_H
#define DB_DOC_AGG_H

#include "db/storage/doc/doc_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 聚合类型
 * ======================================================================== */

/**
 * @brief 聚合类型
 */
typedef enum DocAggType_e {
    DOC_AGG_TERM = 0,         /**< 词条聚合 */
    DOC_AGG_RANGE,            /**< 范围聚合 */
    DOC_AGG_HISTOGRAM,        /**< 直方图聚合 */
    DOC_AGG_STATS,            /**< 统计聚合 */
    DOC_AGG_PERCENTILES,      /**< 百分位数聚合 */
    DOC_AGG_CARDINALITY,      /**< 基数聚合 */
    DOC_AGG_PIPELINE,         /**< 管道聚合 */
} DocAggType;

/* ========================================================================
 * 词条聚合 (Term Aggregation)
 * ======================================================================== */

/** 词条聚合桶 */
typedef struct DocTermBucket_s {
    char *term;               /**< 词条值 */
    int64_t doc_count;        /**< 文档数量 */
    double占比;               /**< 占比 */
} DocTermBucket;

/** 词条聚合结果 */
typedef struct DocTermAggResult_s {
    DocTermBucket *buckets;   /**< 桶数组 */
    size_t num_buckets;       /**< 桶数量 */
    int64_t doc_count_error;  /**< 误差 */
    size_t bg_count;          /**< 后台计数 */
} DocTermAggResult;

/**
 * @brief 创建词条聚合
 * @param field 字段名
 * @param size 返回桶数量
 * @param min_doc_count 最小文档数
 */
void *doc_term_agg_create(const char *field, size_t size, int64_t min_doc_count);

/**
 * @brief 执行词条聚合
 */
DocTermAggResult *doc_term_agg_execute(void *agg,
                                      const char **doc_ids,
                                      const char **doc_values,
                                      size_t num_docs);

/**
 * @brief 释放词条聚合结果
 */
void doc_term_agg_free(DocTermAggResult *result);

/* ========================================================================
 * 范围聚合 (Range Aggregation)
 * ======================================================================== */

/** 范围定义 */
typedef struct DocRange_s {
    double from;              /**< 下界（含） */
    double to;                /**< 上界（不含） */
    char *key;                /**< 范围键 */
} DocRange;

/** 范围聚合桶 */
typedef struct DocRangeBucket_s {
    char *key;                /**< 范围键 */
    int64_t doc_count;        /**< 文档数量 */
    double from;              /**< 下界 */
    double to;                /**< 上界 */
    double avg;               /**< 平均值 */
    double sum;               /**< 总和 */
} DocRangeBucket;

/** 范围聚合结果 */
typedef struct DocRangeAggResult_s {
    DocRangeBucket *buckets;  /**< 桶数组 */
    size_t num_buckets;       /**< 桶数量 */
} DocRangeAggResult;

/**
 * @brief 创建范围聚合
 * @param field 字段名
 * @param ranges 范围数组
 * @param num_ranges 范围数量
 */
void *doc_range_agg_create(const char *field,
                          const DocRange *ranges,
                          size_t num_ranges);

/**
 * @brief 添加自定义范围
 */
int doc_range_agg_add_range(void *agg, const DocRange *range);

/**
 * @brief 执行范围聚合
 */
DocRangeAggResult *doc_range_agg_execute(void *agg,
                                        const char **doc_ids,
                                        const double *values,
                                        size_t num_docs);

/**
 * @brief 释放范围聚合结果
 */
void doc_range_agg_free(DocRangeAggResult *result);

/* ========================================================================
 * 直方图聚合 (Histogram Aggregation)
 * ======================================================================== */

/** 直方图聚合配置 */
typedef struct DocHistogramConfig_s {
    double interval;          /**< 直方图间隔 */
    double min_doc_count;     /**< 最小文档数 */
    bool extended_bounds;     /**< 扩展边界 */
    double min_bound;         /**< 最小边界 */
    double max_bound;         /**< 最大边界 */
} DocHistogramConfig;

/** 直方图桶 */
typedef struct DocHistogramBucket_s {
    double key;               /**< 直方图键（区间起始） */
    int64_t doc_count;        /**< 文档数量 */
    double avg;               /**< 平均值 */
    double sum;               /**< 总和 */
} DocHistogramBucket;

/** 直方图聚合结果 */
typedef struct DocHistogramAggResult_s {
    DocHistogramBucket *buckets; /**< 桶数组 */
    size_t num_buckets;      /**< 桶数量 */
} DocHistogramAggResult;

/**
 * @brief 创建直方图聚合
 * @param field 字段名
 * @param interval 直方图间隔
 * @param config 配置（可为 NULL）
 */
void *doc_histogram_agg_create(const char *field, double interval,
                               const DocHistogramConfig *config);

/**
 * @brief 执行直方图聚合
 */
DocHistogramAggResult *doc_histogram_agg_execute(void *agg,
                                                const char **doc_ids,
                                                const double *values,
                                                size_t num_docs);

/**
 * @brief 释放直方图聚合结果
 */
void doc_histogram_agg_free(DocHistogramAggResult *result);

/* ========================================================================
 * 统计聚合 (Stats Aggregation)
 * ======================================================================== */

/** 统计结果 */
typedef struct DocStats_s {
    int64_t count;            /**< 计数 */
    double min;               /**< 最小值 */
    double max;               /**< 最大值 */
    double avg;               /**< 平均值 */
    double sum;               /**< 总和 */
    double variance;          /**< 方差 */
    double std_deviation;     /**< 标准差 */
    double median;            /**< 中位数 */
} DocStats;

/** 统计聚合结果 */
typedef struct DocStatsAggResult_s {
    DocStats stats;           /**< 统计信息 */
    double percentiles[101];  /**< 百分位数（0-100） */
    bool has_percentiles;     /**< 是否有百分位数 */
} DocStatsAggResult;

/**
 * @brief 创建统计聚合
 * @param field 字段名
 * @param calc_percentiles 是否计算百分位数
 */
void *doc_stats_agg_create(const char *field, bool calc_percentiles);

/**
 * @brief 执行统计聚合
 */
DocStatsAggResult *doc_stats_agg_execute(void *agg,
                                        const double *values,
                                        size_t num_docs);

/**
 * @brief 释放统计聚合结果
 */
void doc_stats_agg_free(DocStatsAggResult *result);

/* ========================================================================
 * 百分位数聚合 (Percentiles Aggregation)
 * ======================================================================== */

/** 百分位数配置 */
typedef struct DocPercentilesConfig_s {
    double *percentiles;      /**< 百分位数值数组 */
    size_t num_percentiles;   /**< 百分位数数量 */
    double compression;       /**< 压缩参数（TDigest） */
} DocPercentilesConfig;

/** 百分位数桶 */
typedef struct DocPercentileBucket_s {
    double percentile;        /**< 百分位值 */
    double value;             /**< 对应值 */
} DocPercentileBucket;

/** 百分位数聚合结果 */
typedef struct DocPercentilesAggResult_s {
    DocPercentileBucket *buckets; /**< 桶数组 */
    size_t num_buckets;      /**< 桶数量 */
} DocPercentilesAggResult;

/**
 * @brief 创建百分位数聚合
 * @param field 字段名
 * @param config 配置（可为 NULL，默认 [1, 5, 25, 50, 75, 95, 99]）
 */
void *doc_percentiles_agg_create(const char *field,
                                 const DocPercentilesConfig *config);

/**
 * @brief 执行百分位数聚合
 */
DocPercentilesAggResult *doc_percentiles_agg_execute(void *agg,
                                                     const double *values,
                                                     size_t num_docs);

/**
 * @brief 释放百分位数聚合结果
 */
void doc_percentiles_agg_free(DocPercentilesAggResult *result);

/* ========================================================================
 * 基数聚合 (Cardinality Aggregation)
 * ======================================================================== */

/**
 * @brief 创建基数聚合
 * @param field 字段名
 * @param precision_threshold 精度阈值
 */
void *doc_cardinality_agg_create(const char *field, size_t precision_threshold);

/**
 * @brief 执行基数聚合
 * @return 基数估计值
 */
uint64_t doc_cardinality_agg_execute(void *agg,
                                    const char **values,
                                    size_t num_docs);

/* ========================================================================
 * 管道聚合 (Pipeline Aggregation)
 * ======================================================================== */

/** 管道聚合类型 */
typedef enum DocPipelineAggType_e {
    DOC_PIPELINE_AVG = 0,     /**< 平均值管道 */
    DOC_PIPELINE_SUM,         /**< 求和管道 */
    DOC_PIPELINE_MIN,         /**< 最小值管道 */
    DOC_PIPELINE_MAX,         /**< 最大值管道 */
    DOC_PIPELINE_STATS,       /**< 统计管道 */
    DOC_PIPELINE_BUCKETS_SORT, /**< 桶排序管道 */
    DOC_PIPELINE_CUMULATIVE_SUM, /**< 累积和管道 */
    DOC_PIPELINE_MOVING_AVG,  /**< 移动平均管道 */
} DocPipelineAggType;

/** 管道聚合配置 */
typedef struct DocPipelineAggConfig_s {
    DocPipelineAggType type;  /**< 管道类型 */
    char parent_agg[64];      /**< 父聚合名 */
    char gap_policy[32];      /**< 空隙策略 */
    int window_size;          /**< 窗口大小（移动平均） */
} DocPipelineAggConfig;

/** 管道聚合 */
typedef struct DocPipelineAgg_s {
    char name[64];            /**< 聚合名 */
    DocPipelineAggConfig config; /**< 配置 */
    void *parent_result;      /**< 父聚合结果 */
} DocPipelineAgg;

/**
 * @brief 创建管道聚合
 * @param name 聚合名
 * @param config 配置
 */
DocPipelineAgg *doc_pipeline_agg_create(const char *name,
                                       const DocPipelineAggConfig *config);

/**
 * @brief 销毁管道聚合
 */
void doc_pipeline_agg_destroy(DocPipelineAgg *agg);

/**
 * @brief 执行管道聚合
 * @param agg 管道聚合
 * @param parent_result 父聚合结果
 * @return 执行结果（JSON 格式）
 */
char *doc_pipeline_agg_execute(DocPipelineAgg *agg, const void *parent_result);

/* ========================================================================
 * 聚合执行器
 * ======================================================================== */

/** 聚合定义 */
typedef struct DocAggDef_s {
    char name[64];            /**< 聚合名 */
    DocAggType type;          /**< 聚合类型 */
    char field[64];           /**< 字段名 */
    void *config;             /**< 配置 */
    DocPipelineAgg **pipelines; /**< 管道聚合数组 */
    size_t num_pipelines;     /**< 管道数量 */
} DocAggDef;

/** 聚合执行器 */
typedef struct DocAggExecutor_s {
    DocAggDef *aggregations;  /**< 聚合定义数组 */
    size_t num_aggregations;  /**< 聚合数量 */
    void *mem_pool;           /**< 内存池 */
} DocAggExecutor;

/**
 * @brief 创建聚合执行器
 */
DocAggExecutor *doc_agg_executor_create(void *mem_pool);

/**
 * @brief 销毁聚合执行器
 */
void doc_agg_executor_destroy(DocAggExecutor *exec);

/**
 * @brief 添加聚合
 */
int doc_agg_executor_add(DocAggExecutor *exec, const DocAggDef *agg);

/**
 * @brief 执行所有聚合
 * @param exec 执行器
 * @param docs 文档数组
 * @param num_docs 文档数量
 * @param results 结果数组（调用者负责释放）
 * @return 结果数量
 */
size_t doc_agg_executor_execute(DocAggExecutor *exec,
                               const char **docs,
                               size_t num_docs,
                               char ***results);

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

/**
 * @brief AGG_TERM 函数
 *
 * AGG_TERM(field, size)
 *
 * 示例:
 *   SELECT AGG_TERM('category', 10) FROM documents
 */
char *doc_sql_term_agg(const char *field, size_t size,
                      const char **doc_ids, const char **doc_values,
                      size_t num_docs);

/**
 * @brief AGG_RANGE 函数
 *
 * AGG_RANGE(field, ranges_json)
 *
 * 示例:
 *   SELECT AGG_RANGE('price', '[{"from":0,"to":100},{"from":100,"to":500}]')
 */
char *doc_sql_range_agg(const char *field,
                       const char *ranges_json,
                       const char **doc_ids,
                       const double *values,
                       size_t num_docs);

/**
 * @brief AGG_STATS 函数
 *
 * AGG_STATS(field)
 */
char *doc_sql_stats_agg(const char *field,
                       const double *values,
                       size_t num_docs);

/**
 * @brief AGG_PERCENTILES 函数
 *
 * AGG_PERCENTILES(field, percentiles)
 */
char *doc_sql_percentiles_agg(const char *field,
                              const double *percentiles,
                              size_t num_percentiles,
                              const double *values,
                              size_t num_docs);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_AGG_H */
