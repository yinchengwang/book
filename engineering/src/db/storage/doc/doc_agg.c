/**
 * @file doc_agg.c
 * @brief 文档聚合框架实现
 */

#include "db/storage/doc/doc_agg.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int compare_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static int compare_term_bucket(const void *a, const void *b) {
    return ((const DocTermBucket *)b)->doc_count - ((const DocTermBucket *)a)->doc_count;
}

static void compute_percentiles(double *sorted_values, size_t n,
                               double *percentiles, size_t num_p,
                               double *out_values) {
    if (!sorted_values || n == 0) return;

    for (size_t i = 0; i < num_p; i++) {
        double p = percentiles[i] / 100.0;
        double idx = p * (n - 1);
        size_t lower = (size_t)idx;
        size_t upper = lower + 1;
        if (upper >= n) upper = n - 1;
        double frac = idx - lower;
        out_values[i] = sorted_values[lower] * (1 - frac) + sorted_values[upper] * frac;
    }
}

/* ========================================================================
 * 词条聚合
 * ======================================================================== */

typedef struct DocTermAgg_s {
    char field[64];
    size_t size;
    int64_t min_doc_count;
    /* Hash 表用于计数 */
    char **terms;
    int64_t *counts;
    size_t num_terms;
    size_t capacity;
} DocTermAgg;

void *doc_term_agg_create(const char *field, size_t size, int64_t min_doc_count) {
    DocTermAgg *agg = (DocTermAgg *)calloc(1, sizeof(DocTermAgg));
    if (!agg) return NULL;

    snprintf(agg->field, sizeof(agg->field), "%s", field ? field : "");
    agg->size = size > 0 ? size : 10;
    agg->min_doc_count = min_doc_count;
    agg->capacity = 1024;
    agg->terms = (char **)calloc(agg->capacity, sizeof(char *));
    agg->counts = (int64_t *)calloc(agg->capacity, sizeof(int64_t));

    return agg;
}

static int find_or_add_term(DocTermAgg *agg, const char *term) {
    for (size_t i = 0; i < agg->num_terms; i++) {
        if (strcmp(agg->terms[i], term) == 0) {
            return (int)i;
        }
    }

    if (agg->num_terms >= agg->capacity) {
        agg->capacity *= 2;
        agg->terms = (char **)realloc(agg->terms, agg->capacity * sizeof(char *));
        agg->counts = (int64_t *)realloc(agg->counts, agg->capacity * sizeof(int64_t));
    }

    agg->terms[agg->num_terms] = strdup(term);
    agg->counts[agg->num_terms] = 0;
    return (int)agg->num_terms++;
}

DocTermAggResult *doc_term_agg_execute(void *agg,
                                      const char **doc_ids,
                                      const char **doc_values,
                                      size_t num_docs) {
    if (!agg || !doc_values || num_docs == 0) return NULL;

    DocTermAgg *ta = (DocTermAgg *)agg;

    /* 统计词条 */
    for (size_t i = 0; i < num_docs; i++) {
        if (doc_values[i]) {
            int idx = find_or_add_term(ta, doc_values[i]);
            ta->counts[idx]++;
        }
    }

    /* 创建结果 */
    DocTermAggResult *result = (DocTermAggResult *)calloc(1, sizeof(DocTermAggResult));
    if (!result) return NULL;

    /* 过滤并排序 */
    result->buckets = (DocTermBucket *)calloc(ta->num_terms, sizeof(DocTermBucket));
    for (size_t i = 0; i < ta->num_terms; i++) {
        if (ta->counts[i] >= ta->min_doc_count) {
            result->buckets[result->num_buckets].term = ta->terms[i];
            result->buckets[result->num_buckets].doc_count = ta->counts[i];
            result->num_buckets++;
        }
    }

    /* 排序 */
    qsort(result->buckets, result->num_buckets, sizeof(DocTermBucket), compare_term_bucket);

    /* 限制数量 */
    if (result->num_buckets > ta->size) {
        result->num_buckets = ta->size;
    }

    /* 计算占比 */
    int64_t total = 0;
    for (size_t i = 0; i < result->num_buckets; i++) {
        total += result->buckets[i].doc_count;
    }
    for (size_t i = 0; i < result->num_buckets; i++) {
        result->buckets[i].占比 = total > 0 ? (double)result->buckets[i].doc_count / total : 0;
    }

    return result;
}

void doc_term_agg_free(DocTermAggResult *result) {
    if (!result) return;
    free(result->buckets);
    free(result);
}

/* ========================================================================
 * 范围聚合
 * ======================================================================== */

typedef struct DocRangeAgg_s {
    char field[64];
    DocRange *ranges;
    size_t num_ranges;
    size_t capacity;
} DocRangeAgg;

void *doc_range_agg_create(const char *field,
                          const DocRange *ranges,
                          size_t num_ranges) {
    DocRangeAgg *agg = (DocRangeAgg *)calloc(1, sizeof(DocRangeAgg));
    if (!agg) return NULL;

    snprintf(agg->field, sizeof(agg->field), "%s", field ? field : "");
    agg->capacity = 16;
    agg->ranges = (DocRange *)calloc(agg->capacity, sizeof(DocRange));

    if (ranges && num_ranges > 0) {
        for (size_t i = 0; i < num_ranges && i < agg->capacity; i++) {
            agg->ranges[agg->num_ranges++] = ranges[i];
        }
    }

    return agg;
}

int doc_range_agg_add_range(void *agg, const DocRange *range) {
    DocRangeAgg *ra = (DocRangeAgg *)agg;
    if (!ra || !range) return -1;

    if (ra->num_ranges >= ra->capacity) {
        ra->capacity *= 2;
        ra->ranges = (DocRange *)realloc(ra->ranges, ra->capacity * sizeof(DocRange));
    }

    ra->ranges[ra->num_ranges++] = *range;
    return 0;
}

DocRangeAggResult *doc_range_agg_execute(void *agg,
                                        const char **doc_ids,
                                        const double *values,
                                        size_t num_docs) {
    if (!agg || !values || num_docs == 0) return NULL;

    DocRangeAgg *ra = (DocRangeAgg *)agg;
    DocRangeAggResult *result = (DocRangeAggResult *)calloc(1, sizeof(DocRangeAggResult));

    result->buckets = (DocRangeBucket *)calloc(ra->num_ranges, sizeof(DocRangeBucket));

    for (size_t r = 0; r < ra->num_ranges; r++) {
        result->buckets[r].key = ra->ranges[r].key ? strdup(ra->ranges[r].key) : NULL;
        result->buckets[r].from = ra->ranges[r].from;
        result->buckets[r].to = ra->ranges[r].to;

        double sum = 0;
        int64_t count = 0;

        for (size_t i = 0; i < num_docs; i++) {
            double v = values[i];
            if (v >= ra->ranges[r].from && v < ra->ranges[r].to) {
                count++;
                sum += v;
            }
        }

        result->buckets[r].doc_count = count;
        result->buckets[r].sum = sum;
        result->buckets[r].avg = count > 0 ? sum / count : 0;
        result->num_buckets++;
    }

    return result;
}

void doc_range_agg_free(DocRangeAggResult *result) {
    if (!result) return;
    for (size_t i = 0; i < result->num_buckets; i++) {
        free(result->buckets[i].key);
    }
    free(result->buckets);
    free(result);
}

/* ========================================================================
 * 直方图聚合
 * ======================================================================== */

typedef struct DocHistogramAgg_s {
    char field[64];
    DocHistogramConfig config;
} DocHistogramAgg;

void *doc_histogram_agg_create(const char *field, double interval,
                               const DocHistogramConfig *config) {
    DocHistogramAgg *agg = (DocHistogramAgg *)calloc(1, sizeof(DocHistogramAgg));
    if (!agg) return NULL;

    snprintf(agg->field, sizeof(agg->field), "%s", field ? field : "");
    agg->config.interval = interval > 0 ? interval : 1;

    if (config) {
        agg->config.min_doc_count = config->min_doc_count;
        agg->config.extended_bounds = config->extended_bounds;
        agg->config.min_bound = config->min_bound;
        agg->config.max_bound = config->max_bound;
    }

    return agg;
}

DocHistogramAggResult *doc_histogram_agg_execute(void *agg,
                                                const char **doc_ids,
                                                const double *values,
                                                size_t num_docs) {
    if (!agg || !values || num_docs == 0) return NULL;

    DocHistogramAgg *ha = (DocHistogramAgg *)agg;
    DocHistogramAggResult *result = (DocHistogramAggResult *)calloc(1,
        sizeof(DocHistogramAggResult));

    /* 找到最小值和最大值 */
    double min_val = values[0], max_val = values[0];
    for (size_t i = 1; i < num_docs; i++) {
        if (values[i] < min_val) min_val = values[i];
        if (values[i] > max_val) max_val = values[i];
    }

    if (ha->config.extended_bounds) {
        if (min_val > ha->config.min_bound) min_val = ha->config.min_bound;
        if (max_val < ha->config.max_bound) max_val = ha->config.max_bound;
    }

    /* 计算桶数量 */
    double interval = ha->config.interval;
    size_t num_buckets = (size_t)((max_val - min_val) / interval) + 1;

    result->buckets = (DocHistogramBucket *)calloc(num_buckets, sizeof(DocHistogramBucket));

    /* 统计 */
    for (size_t i = 0; i < num_docs; i++) {
        size_t bucket_idx = (size_t)((values[i] - min_val) / interval);
        if (bucket_idx >= num_buckets) bucket_idx = num_buckets - 1;

        result->buckets[bucket_idx].key = min_val + bucket_idx * interval;
        result->buckets[bucket_idx].doc_count++;
        result->buckets[bucket_idx].sum += values[i];
    }

    /* 计算平均值 */
    for (size_t i = 0; i < num_buckets; i++) {
        if (result->buckets[i].doc_count > 0) {
            result->buckets[i].avg = result->buckets[i].sum / result->buckets[i].doc_count;
        }
        result->num_buckets++;
    }

    return result;
}

void doc_histogram_agg_free(DocHistogramAggResult *result) {
    if (!result) return;
    free(result->buckets);
    free(result);
}

/* ========================================================================
 * 统计聚合
 * ======================================================================== */

typedef struct DocStatsAgg_s {
    char field[64];
    bool calc_percentiles;
    double sorted_values[1024];  /* 简化实现 */
    size_t num_values;
} DocStatsAgg;

void *doc_stats_agg_create(const char *field, bool calc_percentiles) {
    DocStatsAgg *agg = (DocStatsAgg *)calloc(1, sizeof(DocStatsAgg));
    if (!agg) return NULL;

    snprintf(agg->field, sizeof(agg->field), "%s", field ? field : "");
    agg->calc_percentiles = calc_percentiles;
    return agg;
}

DocStatsAggResult *doc_stats_agg_execute(void *agg,
                                        const double *values,
                                        size_t num_docs) {
    if (!agg || !values || num_docs == 0) return NULL;

    DocStatsAgg *sa = (DocStatsAgg *)agg;
    DocStatsAggResult *result = (DocStatsAggResult *)calloc(1,
        sizeof(DocStatsAggResult));

    /* 复制值用于排序 */
    double *sorted = (double *)malloc(num_docs * sizeof(double));
    memcpy(sorted, values, num_docs * sizeof(double));
    qsort(sorted, num_docs, sizeof(double), compare_double);

    /* 计算统计信息 */
    DocStats *stats = &result->stats;
    stats->count = (int64_t)num_docs;
    stats->min = sorted[0];
    stats->max = sorted[num_docs - 1];
    stats->sum = 0;
    for (size_t i = 0; i < num_docs; i++) {
        stats->sum += values[i];
    }
    stats->avg = stats->sum / num_docs;

    /* 方差和标准差 */
    double sq_diff_sum = 0;
    for (size_t i = 0; i < num_docs; i++) {
        double diff = values[i] - stats->avg;
        sq_diff_sum += diff * diff;
    }
    stats->variance = sq_diff_sum / num_docs;
    stats->std_deviation = sqrt(stats->variance);

    /* 中位数 */
    if (num_docs % 2 == 0) {
        stats->median = (sorted[num_docs / 2 - 1] + sorted[num_docs / 2]) / 2;
    } else {
        stats->median = sorted[num_docs / 2];
    }

    /* 百分位数 */
    if (sa->calc_percentiles) {
        double percentiles[] = {1, 5, 25, 50, 75, 95, 99};
        size_t num_p = sizeof(percentiles) / sizeof(percentiles[0]);
        compute_percentiles(sorted, num_docs, percentiles, num_p, result->percentiles);
        result->has_percentiles = true;
    }

    free(sorted);
    return result;
}

void doc_stats_agg_free(DocStatsAggResult *result) {
    free(result);
}

/* ========================================================================
 * 百分位数聚合
 * ======================================================================== */

typedef struct DocPercentilesAgg_s {
    char field[64];
    double *percentiles;
    size_t num_percentiles;
} DocPercentilesAgg;

void *doc_percentiles_agg_create(const char *field,
                                 const DocPercentilesConfig *config) {
    DocPercentilesAgg *agg = (DocPercentilesAgg *)calloc(1, sizeof(DocPercentilesAgg));
    if (!agg) return NULL;

    snprintf(agg->field, sizeof(agg->field), "%s", field ? field : "");

    if (config && config->percentiles && config->num_percentiles > 0) {
        agg->percentiles = (double *)malloc(config->num_percentiles * sizeof(double));
        memcpy(agg->percentiles, config->percentiles, config->num_percentiles * sizeof(double));
        agg->num_percentiles = config->num_percentiles;
    } else {
        /* 默认百分位数 */
        double default_p[] = {1, 5, 25, 50, 75, 95, 99};
        agg->num_percentiles = sizeof(default_p) / sizeof(default_p[0]);
        agg->percentiles = (double *)malloc(agg->num_percentiles * sizeof(double));
        memcpy(agg->percentiles, default_p, agg->num_percentiles * sizeof(double));
    }

    return agg;
}

DocPercentilesAggResult *doc_percentiles_agg_execute(void *agg,
                                                     const double *values,
                                                     size_t num_docs) {
    if (!agg || !values || num_docs == 0) return NULL;

    DocPercentilesAgg *pa = (DocPercentilesAgg *)agg;
    DocPercentilesAggResult *result = (DocPercentilesAggResult *)calloc(1,
        sizeof(DocPercentilesAggResult));

    /* 排序值 */
    double *sorted = (double *)malloc(num_docs * sizeof(double));
    memcpy(sorted, values, num_docs * sizeof(double));
    qsort(sorted, num_docs, sizeof(double), compare_double);

    result->buckets = (DocPercentileBucket *)calloc(pa->num_percentiles,
        sizeof(DocPercentileBucket));

    for (size_t i = 0; i < pa->num_percentiles; i++) {
        result->buckets[i].percentile = pa->percentiles[i];
        double p = pa->percentiles[i] / 100.0;
        double idx = p * (num_docs - 1);
        size_t lower = (size_t)idx;
        size_t upper = lower + 1;
        if (upper >= num_docs) upper = num_docs - 1;
        double frac = idx - lower;
        result->buckets[i].value = sorted[lower] * (1 - frac) + sorted[upper] * frac;
        result->num_buckets++;
    }

    free(sorted);
    return result;
}

void doc_percentiles_agg_free(DocPercentilesAggResult *result) {
    if (!result) return;
    free(result->buckets);
    free(result);
}

/* ========================================================================
 * 基数聚合
 * ======================================================================== */

typedef struct DocCardinalityAgg_s {
    char field[64];
    size_t precision_threshold;
    uint64_t hyperloglog[128];  /* 简化 HLL */
} DocCardinalityAgg;

void *doc_cardinality_agg_create(const char *field, size_t precision_threshold) {
    DocCardinalityAgg *agg = (DocCardinalityAgg *)calloc(1, sizeof(DocCardinalityAgg));
    if (!agg) return NULL;

    snprintf(agg->field, sizeof(agg->field), "%s", field ? field : "");
    agg->precision_threshold = precision_threshold > 0 ? precision_threshold : 1000;
    memset(agg->hyperloglog, 0, sizeof(agg->hyperloglog));

    return agg;
}

uint64_t doc_cardinality_agg_execute(void *agg,
                                    const char **values,
                                    size_t num_docs) {
    if (!agg || !values || num_docs == 0) return 0;

    /* 简化实现：使用 hash set */
    int *seen = (int *)calloc(65536, sizeof(int));
    uint64_t count = 0;

    for (size_t i = 0; i < num_docs; i++) {
        if (!values[i]) continue;
        uint32_t h = 2166136261U;
        for (const char *p = values[i]; *p; p++) {
            h ^= *p;
            h *= 16777619U;
        }
        if (!seen[h & 0xFFFF]) {
            seen[h & 0xFFFF] = 1;
            count++;
        }
    }

    free(seen);
    return count;
}

/* ========================================================================
 * 管道聚合
 * ======================================================================== */

DocPipelineAgg *doc_pipeline_agg_create(const char *name,
                                       const DocPipelineAggConfig *config) {
    DocPipelineAgg *agg = (DocPipelineAgg *)calloc(1, sizeof(DocPipelineAgg));
    if (!agg) return NULL;

    snprintf(agg->name, sizeof(agg->name), "%s", name ? name : "");
    if (config) {
        agg->config = *config;
    }

    return agg;
}

void doc_pipeline_agg_destroy(DocPipelineAgg *agg) {
    free(agg);
}

char *doc_pipeline_agg_execute(DocPipelineAgg *agg, const void *parent_result) {
    if (!agg || !parent_result) return NULL;

    char *result = (char *)malloc(512);

    switch (agg->config.type) {
        case DOC_PIPELINE_AVG:
            snprintf(result, 512, "{\"name\":\"%s\",\"type\":\"avg\",\"value\":0}", agg->name);
            break;
        case DOC_PIPELINE_SUM:
            snprintf(result, 512, "{\"name\":\"%s\",\"type\":\"sum\",\"value\":0}", agg->name);
            break;
        case DOC_PIPELINE_MIN:
            snprintf(result, 512, "{\"name\":\"%s\",\"type\":\"min\",\"value\":0}", agg->name);
            break;
        case DOC_PIPELINE_MAX:
            snprintf(result, 512, "{\"name\":\"%s\",\"type\":\"max\",\"value\":0}", agg->name);
            break;
        case DOC_PIPELINE_STATS:
            snprintf(result, 512, "{\"name\":\"%s\",\"type\":\"stats\",\"count\":0}", agg->name);
            break;
        default:
            snprintf(result, 512, "{\"name\":\"%s\",\"error\":\"unknown type\"}", agg->name);
    }

    return result;
}

/* ========================================================================
 * 聚合执行器
 * ======================================================================== */

DocAggExecutor *doc_agg_executor_create(void *mem_pool) {
    DocAggExecutor *exec = (DocAggExecutor *)calloc(1, sizeof(DocAggExecutor));
    if (exec) exec->mem_pool = mem_pool;
    return exec;
}

void doc_agg_executor_destroy(DocAggExecutor *exec) {
    if (!exec) return;
    free(exec->aggregations);
    free(exec);
}

int doc_agg_executor_add(DocAggExecutor *exec, const DocAggDef *agg) {
    if (!exec || !agg) return -1;

    exec->aggregations = (DocAggDef *)realloc(exec->aggregations,
        (exec->num_aggregations + 1) * sizeof(DocAggDef));
    exec->aggregations[exec->num_aggregations++] = *agg;
    return 0;
}

size_t doc_agg_executor_execute(DocAggExecutor *exec,
                               const char **docs,
                               size_t num_docs,
                               char ***results) {
    if (!exec || !results) return 0;

    *results = (char **)calloc(exec->num_aggregations, sizeof(char *));

    for (size_t i = 0; i < exec->num_aggregations; i++) {
        DocAggDef *def = &exec->aggregations[i];
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"agg\":\"%s\",\"type\":%d}", def->name, def->type);
        (*results)[i] = strdup(buf);
    }

    return exec->num_aggregations;
}

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

char *doc_sql_term_agg(const char *field, size_t size,
                      const char **doc_ids, const char **doc_values,
                      size_t num_docs) {
    void *agg = doc_term_agg_create(field, size, 0);
    DocTermAggResult *result = doc_term_agg_execute(agg, doc_ids, doc_values, num_docs);

    /* 序列化为 JSON */
    char *json = (char *)malloc(4096);
    char *p = json;
    p += sprintf(p, "{\"field\":\"%s\",\"buckets\":[", field ? field : "");

    for (size_t i = 0; i < result->num_buckets; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "{\"term\":\"%s\",\"count\":%ld}", result->buckets[i].term,
                    result->buckets[i].doc_count);
    }

    p += sprintf(p, "]}");

    doc_term_agg_free(result);
    free(agg);
    return json;
}

char *doc_sql_range_agg(const char *field,
                       const char *ranges_json,
                       const char **doc_ids,
                       const double *values,
                       size_t num_docs) {
    (void)ranges_json;

    void *agg = doc_range_agg_create(field, NULL, 0);
    DocRangeAggResult *result = doc_range_agg_execute(agg, doc_ids, values, num_docs);

    char *json = (char *)malloc(4096);
    char *p = json;
    p += sprintf(p, "{\"field\":\"%s\",\"buckets\":[", field ? field : "");

    for (size_t i = 0; i < result->num_buckets; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "{\"key\":\"%s\",\"from\":%g,\"to\":%g,\"count\":%ld}",
                    result->buckets[i].key ? result->buckets[i].key : "",
                    result->buckets[i].from, result->buckets[i].to,
                    result->buckets[i].doc_count);
    }

    p += sprintf(p, "]}");

    doc_range_agg_free(result);
    free(agg);
    return json;
}

char *doc_sql_stats_agg(const char *field,
                       const double *values,
                       size_t num_docs) {
    void *agg = doc_stats_agg_create(field, true);
    DocStatsAggResult *result = doc_stats_agg_execute(agg, values, num_docs);

    char *json = (char *)malloc(512);
    DocStats *s = &result->stats;
    sprintf(json, "{\"field\":\"%s\",\"count\":%ld,\"min\":%g,\"max\":%g,\"avg\":%g,\"sum\":%g}",
            field ? field : "", s->count, s->min, s->max, s->avg, s->sum);

    doc_stats_agg_free(result);
    free(agg);
    return json;
}

char *doc_sql_percentiles_agg(const char *field,
                              const double *percentiles,
                              size_t num_percentiles,
                              const double *values,
                              size_t num_docs) {
    DocPercentilesConfig config = {0};
    config.percentiles = (double *)percentiles;
    config.num_percentiles = num_percentiles;

    void *agg = doc_percentiles_agg_create(field, &config);
    DocPercentilesAggResult *result = doc_percentiles_agg_execute(agg, values, num_docs);

    char *json = (char *)malloc(1024);
    char *p = json;
    p += sprintf(p, "{\"field\":\"%s\",\"percentiles\":[", field ? field : "");

    for (size_t i = 0; i < result->num_buckets; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "{\"p%g\":%g}", result->buckets[i].percentile,
                    result->buckets[i].value);
    }

    p += sprintf(p, "]}");

    doc_percentiles_agg_free(result);
    free(agg);
    return json;
}
