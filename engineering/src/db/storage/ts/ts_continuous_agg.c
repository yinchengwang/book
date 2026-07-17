/**
 * @file ts_continuous_agg.c
 * @brief 时序数据连续聚合实现
 */
#include "ts_continuous_agg.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <time.h>

/* ========================================================================
 * 连续聚合配置
 * ======================================================================== */

ContinuousAggConfig *cagg_config_create(const char *name, const char *source) {
    if (!name || !source) return NULL;

    ContinuousAggConfig *config = (ContinuousAggConfig *)calloc(1, sizeof(ContinuousAggConfig));
    if (!config) return NULL;

    config->name = name;
    config->source_measurement = source;
    config->bucket = CAGG_BUCKET_1M;
    config->func = CAGG_AVG;
    config->refresh_interval_ms = 60000; /* 默认 1 分钟 */
    config->window_end = 0; /* 到现在 */
    config->window_start = -3600000; /* 过去 1 小时 */

    return config;
}

void cagg_config_set_bucket(ContinuousAggConfig *config, CaggBucket bucket) {
    if (config) config->bucket = bucket;
}

void cagg_config_set_func(ContinuousAggConfig *config, CaggFunc func) {
    if (config) config->func = func;
}

void cagg_config_add_group_by_tag(ContinuousAggConfig *config, const char *tag) {
    if (!config || !tag) return;

    if (config->group_by_tag_count == 0) {
        config->group_by_tags = (const char **)malloc(16 * sizeof(const char *));
    }

    if (config->group_by_tags) {
        config->group_by_tags[config->group_by_tag_count++] = tag;
    }
}

void cagg_config_set_refresh_interval(ContinuousAggConfig *config, int64_t interval_ms) {
    if (config) config->refresh_interval_ms = interval_ms;
}

void cagg_config_set_window(ContinuousAggConfig *config, int64_t start_offset, int64_t end_offset) {
    if (config) {
        config->window_start = start_offset;
        config->window_end = end_offset;
    }
}

void cagg_config_free(ContinuousAggConfig *config) {
    if (config) {
        free((void *)config->group_by_tags);
        free(config);
    }
}

/* ========================================================================
 * 连续聚合状态
 * ======================================================================== */

ContinuousAggState *cagg_state_create(const ContinuousAggConfig *config) {
    if (!config) return NULL;

    ContinuousAggState *state = (ContinuousAggState *)calloc(1, sizeof(ContinuousAggState));
    if (!state) return NULL;

    strncpy(state->name, config->name, sizeof(state->name) - 1);
    state->config = (ContinuousAggConfig *)config;
    state->last_refresh_time = 0;
    state->last_processed_time = 0;

    return state;
}

void cagg_state_free(ContinuousAggState *state) {
    if (state) {
        free(state->incremental_state);
        free(state->materialized_data);
        free(state);
    }
}

/* ========================================================================
 * CDC 消费者
 * ======================================================================== */

struct CdcConsumer_s {
    const char *name;
    CdcCallback callback;
    void *user_data;
};

CdcConsumer *cdc_consumer_create(const char *name) {
    CdcConsumer *consumer = (CdcConsumer *)calloc(1, sizeof(CdcConsumer));
    if (!consumer) return NULL;
    consumer->name = name;
    return consumer;
}

void cdc_consumer_destroy(CdcConsumer *consumer) {
    free(consumer);
}

int cdc_send_event(CdcConsumer *consumer, const CdcEvent *event) {
    if (!consumer || !event) return -1;

    if (consumer->callback) {
        consumer->callback(event, consumer->user_data);
    }

    return 0;
}

int cdc_send_events_batch(CdcConsumer *consumer,
                          const CdcEvent *events,
                          uint32_t count) {
    if (!consumer || !events) return -1;

    for (uint32_t i = 0; i < count; i++) {
        cdc_send_event(consumer, &events[i]);
    }

    return 0;
}

int cdc_register_callback(CdcConsumer *consumer,
                          CdcCallback callback,
                          void *user_data) {
    if (!consumer) return -1;

    consumer->callback = callback;
    consumer->user_data = user_data;

    return 0;
}

/* ========================================================================
 * 增量计算
 * ======================================================================== */

IncrementalComputer *incremental_computer_create(CaggFunc func) {
    IncrementalComputer *comp = (IncrementalComputer *)calloc(1, sizeof(IncrementalComputer));
    if (!comp) return NULL;

    comp->func = func;
    comp->min = DBL_MAX;
    comp->max = -DBL_MAX;

    return comp;
}

int incremental_computer_add(IncrementalComputer *comp,
                             int64_t timestamp, double value) {
    if (!comp) return -1;

    comp->sum += value;
    comp->count++;

    if (value < comp->min) comp->min = value;
    if (value > comp->max) comp->max = value;

    if (comp->count == 1) {
        comp->first = value;
    }
    comp->last = value;

    (void)timestamp; /* 未使用 */
    return 0;
}

int incremental_computer_merge(IncrementalComputer *dest,
                               const IncrementalComputer *src) {
    if (!dest || !src) return -1;

    dest->sum += src->sum;
    dest->count += src->count;

    if (src->min < dest->min) dest->min = src->min;
    if (src->max > dest->max) dest->max = src->max;

    if (src->count > 0 && dest->count == 0) {
        dest->first = src->first;
    }
    dest->last = src->last;

    return 0;
}

double incremental_computer_get_result(const IncrementalComputer *comp) {
    if (!comp || comp->count == 0) return 0.0;

    switch (comp->func) {
        case CAGG_SUM: return comp->sum;
        case CAGG_AVG: return comp->sum / comp->count;
        case CAGG_MIN: return comp->min;
        case CAGG_MAX: return comp->max;
        case CAGG_COUNT: return (double)comp->count;
        case CAGG_FIRST: return comp->first;
        case CAGG_LAST: return comp->last;
        default: return 0.0;
    }
}

void incremental_computer_reset(IncrementalComputer *comp) {
    if (!comp) return;

    comp->sum = 0;
    comp->count = 0;
    comp->min = DBL_MAX;
    comp->max = -DBL_MAX;
    comp->first = 0;
    comp->last = 0;
}

void incremental_computer_free(IncrementalComputer *comp) {
    free(comp);
}

/* ========================================================================
 * 自动降采样策略
 * ======================================================================== */

DownsamplingPolicy *downsampling_policy_default(void) {
    DownsamplingPolicy *policy = (DownsamplingPolicy *)calloc(1, sizeof(DownsamplingPolicy));
    if (!policy) return NULL;

    /* 1分钟保留7天，5分钟保留30天，1小时保留1年 */
    downsampling_policy_add_rule(policy, CAGG_BUCKET_1M, 7, CAGG_AVG);
    downsampling_policy_add_rule(policy, CAGG_BUCKET_5M, 30, CAGG_AVG);
    downsampling_policy_add_rule(policy, CAGG_BUCKET_1H, 365, CAGG_AVG);
    downsampling_policy_add_rule(policy, CAGG_BUCKET_1D, 0, CAGG_AVG); /* 永久保留 */

    policy->raw_retention_days = 7;

    return policy;
}

int downsampling_policy_add_rule(DownsamplingPolicy *policy,
                                 CaggBucket bucket,
                                 int64_t retention_days,
                                 CaggFunc func) {
    if (!policy) return -1;

    if (policy->rule_count == 0) {
        policy->rules = (DownsampleRule *)malloc(8 * sizeof(DownsampleRule));
    }

    if (!policy->rules) return -1;

    DownsampleRule *rule = &policy->rules[policy->rule_count++];
    rule->bucket = bucket;
    rule->retention_days = retention_days;
    rule->func = func;
    rule->priority = policy->rule_count;

    return 0;
}

const DownsampleRule *downsampling_policy_get_rule(const DownsamplingPolicy *policy,
                                                   int64_t data_age_days) {
    if (!policy) return NULL;

    for (uint32_t i = 0; i < policy->rule_count; i++) {
        if (policy->rules[i].retention_days == 0 ||
            data_age_days < policy->rules[i].retention_days) {
            return &policy->rules[i];
        }
    }

    return policy->rule_count > 0 ? &policy->rules[policy->rule_count - 1] : NULL;
}

void downsampling_policy_free(DownsamplingPolicy *policy) {
    if (policy) {
        free(policy->rules);
        free(policy);
    }
}

/* ========================================================================
 * 连续聚合管理器
 * ======================================================================== */

ContinuousAggManager *cagg_manager_create(const char *data_dir) {
    ContinuousAggManager *mgr = (ContinuousAggManager *)calloc(1, sizeof(ContinuousAggManager));
    if (!mgr) return NULL;

    if (data_dir) {
        strncpy(mgr->data_dir, data_dir, sizeof(mgr->data_dir) - 1);
    }

    mgr->agg_capacity = 16;
    mgr->aggregations = (ContinuousAggState **)calloc(mgr->agg_capacity, sizeof(ContinuousAggState *));

    mgr->cdc_consumer = cdc_consumer_create("cagg_manager");

    mgr->downsample_policy = downsampling_policy_default();

    return mgr;
}

void cagg_manager_destroy(ContinuousAggManager *mgr) {
    if (!mgr) return;

    for (uint32_t i = 0; i < mgr->agg_count; i++) {
        cagg_state_free(mgr->aggregations[i]);
    }
    free(mgr->aggregations);

    cdc_consumer_destroy(mgr->cdc_consumer);
    downsampling_policy_free(mgr->downsample_policy);

    free(mgr);
}

/* ========================================================================
 * 连续聚合操作
 * ======================================================================== */

int cagg_manager_create_agg(ContinuousAggManager *mgr,
                            const ContinuousAggConfig *config) {
    if (!mgr || !config) return -1;

    /* 扩容检查 */
    if (mgr->agg_count >= mgr->agg_capacity) {
        mgr->agg_capacity *= 2;
        ContinuousAggState **new_aggs = (ContinuousAggState **)realloc(
            mgr->aggregations, mgr->agg_capacity * sizeof(ContinuousAggState *));
        if (!new_aggs) return -1;
        mgr->aggregations = new_aggs;
    }

    ContinuousAggState *state = cagg_state_create(config);
    if (!state) return -1;

    mgr->aggregations[mgr->agg_count++] = state;
    return 0;
}

int cagg_manager_drop_agg(ContinuousAggManager *mgr, const char *name) {
    if (!mgr || !name) return -1;

    for (uint32_t i = 0; i < mgr->agg_count; i++) {
        if (strcmp(mgr->aggregations[i]->name, name) == 0) {
            cagg_state_free(mgr->aggregations[i]);

            /* 移动后续元素 */
            for (uint32_t j = i; j < mgr->agg_count - 1; j++) {
                mgr->aggregations[j] = mgr->aggregations[j + 1];
            }
            mgr->agg_count--;
            return 0;
        }
    }

    return -1;
}

ContinuousAggState *cagg_manager_get_agg(const ContinuousAggManager *mgr,
                                         const char *name) {
    if (!mgr || !name) return NULL;

    for (uint32_t i = 0; i < mgr->agg_count; i++) {
        if (strcmp(mgr->aggregations[i]->name, name) == 0) {
            return mgr->aggregations[i];
        }
    }

    return NULL;
}

int cagg_manager_refresh(ContinuousAggManager *mgr, const char *name) {
    ContinuousAggState *state = cagg_manager_get_agg(mgr, name);
    if (!state) return -1;

    /* 更新刷新时间 */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    state->last_refresh_time = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    state->refresh_count++;

    return 0;
}

int cagg_manager_refresh_all(ContinuousAggManager *mgr) {
    if (!mgr) return -1;

    for (uint32_t i = 0; i < mgr->agg_count; i++) {
        cagg_manager_refresh(mgr, mgr->aggregations[i]->name);
    }

    return 0;
}

int cagg_manager_refresh_incremental(ContinuousAggManager *mgr,
                                     const char *name,
                                     int64_t since_time) {
    if (!mgr) return -1;

    ContinuousAggState *state = cagg_manager_get_agg(mgr, name);
    if (!state) return -1;

    /* 只刷新自上次处理以来的新数据 */
    if (since_time > state->last_processed_time) {
        state->last_processed_time = since_time;
        return cagg_manager_refresh(mgr, name);
    }

    return 0;
}

/* ========================================================================
 * 查询改写
 * ======================================================================== */

int cagg_manager_analyze_query(const ContinuousAggManager *mgr,
                               const char *sql,
                               QueryRewrite *out_rewrite) {
    if (!mgr || !sql || !out_rewrite) return -1;

    /* 简化实现：检查是否可以使用物化视图 */
    memset(out_rewrite, 0, sizeof(QueryRewrite));
    out_rewrite->original_query = sql;
    out_rewrite->speedup_estimate = 1.0;

    /* 检查是否有匹配的连续聚合 */
    for (uint32_t i = 0; i < mgr->agg_count; i++) {
        const ContinuousAggState *state = mgr->aggregations[i];
        if (state && state->config) {
            /* 简化：假设可以使用第一个匹配的聚合 */
            out_rewrite->materialized_name = state->name;
            out_rewrite->speedup_estimate = 10.0;
            return 0;
        }
    }

    return -1; /* 无法改写 */
}

int cagg_manager_rewrite_query(const ContinuousAggManager *mgr,
                               const char *sql,
                               char *out_buffer,
                               size_t buffer_size) {
    if (!mgr || !sql || !out_buffer) return -1;

    QueryRewrite rewrite;
    if (cagg_manager_analyze_query(mgr, sql, &rewrite) != 0) {
        /* 无法改写，返回原查询 */
        strncpy(out_buffer, sql, buffer_size - 1);
        return -1;
    }

    /* 简化：生成改写查询 */
    if (rewrite.materialized_name) {
        snprintf(out_buffer, buffer_size,
                 "SELECT time, value FROM %s WHERE %s",
                 rewrite.materialized_name, "time >= now() - interval '1 day'");
    } else {
        strncpy(out_buffer, sql, buffer_size - 1);
    }

    out_buffer[buffer_size - 1] = '\0';
    return 0;
}

void query_rewrite_free(QueryRewrite *rewrite) {
    /* 简化：无需释放 */
    (void)rewrite;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

void cagg_manager_stats(const ContinuousAggManager *mgr,
                        uint32_t *out_agg_count,
                        uint64_t *out_total_input_points,
                        uint64_t *out_total_output_points,
                        int64_t *out_last_refresh_time) {
    if (!mgr) return;

    if (out_agg_count) *out_agg_count = mgr->agg_count;

    uint64_t input_total = 0, output_total = 0;
    int64_t last_refresh = 0;

    for (uint32_t i = 0; i < mgr->agg_count; i++) {
        ContinuousAggState *state = mgr->aggregations[i];
        if (state) {
            input_total += state->processed_points;
            output_total += state->output_points;
            if (state->last_refresh_time > last_refresh) {
                last_refresh = state->last_refresh_time;
            }
        }
    }

    if (out_total_input_points) *out_total_input_points = input_total;
    if (out_total_output_points) *out_total_output_points = output_total;
    if (out_last_refresh_time) *out_last_refresh_time = last_refresh;
}
