/**
 * @file reranker.c
 * @brief ReRanker 抽象接口实现
 */

#include "db/index/vector_index/reranker/reranker.h"
#include "db/index/vector_index/reranker/precise_reranker.h"
#include "db/index/vector_index/reranker/multi_metric_reranker.h"
#include "db/index/vector_index/reranker/mmr_reranker.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** ReRanker 内部结构 */
struct Reranker_s {
    RerankerType type;           /**< 重排类型 */
    char name[64];               /**< 名称 */

    /* 实际实现 */
    union {
        void *precise;
        void *multi_metric;
        void *mmr;
        void *custom;
    } impl;

    /* 自定义实现数据 */
    rerank_func_t custom_func;
    void *custom_user_data;

    RerankerConfig config;
};

/* ========================================================================
 * 工厂注册表
 * ======================================================================== */

#define MAX_REGISTERED_RERANKERS 16

typedef struct {
    char name[64];
    rerank_func_t func;
    void *user_data;
} RegisteredReranker;

static RegisteredReranker g_registered[MAX_REGISTERED_RERANKERS];
static int32_t g_registered_count = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ========================================================================
 * 配置默认值
 * ======================================================================== */

RerankerConfig reranker_config_default(RerankerType type, int32_t dims) {
    RerankerConfig config = {
        .type = type,
        .dims = dims,
        .metric = DISTANCE_METRIC_L2_SQUARED,
        .precise = { .use_simd = true },
        .multi_metric = {
            .metrics = { DISTANCE_METRIC_L2_SQUARED, DISTANCE_METRIC_COSINE, DISTANCE_METRIC_INNER_PRODUCT },
            .weights = { 0.4f, 0.3f, 0.3f },
            .num_metrics = 3
        },
        .mmr = {
            .lambda = 0.5f,
            .diversity_dim = 0
        }
    };
    return config;
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

Reranker *reranker_create(RerankerType type, const RerankerConfig *config) {
    if (config == NULL || config->dims <= 0) {
        return NULL;
    }

    Reranker *reranker = (Reranker *)calloc(1, sizeof(Reranker));
    if (reranker == NULL) {
        return NULL;
    }

    reranker->type = type;
    reranker->config = *config;

    /* 根据类型创建实际实现 */
    switch (type) {
        case RERANKER_PRECISE:
            strcpy(reranker->name, "precise");
            reranker->impl.precise = precise_reranker_create(config);
            if (reranker->impl.precise == NULL) {
                free(reranker);
                return NULL;
            }
            break;

        case RERANKER_MULTI_METRIC:
            strcpy(reranker->name, "multi_metric");
            reranker->impl.multi_metric = multi_metric_reranker_create(config);
            if (reranker->impl.multi_metric == NULL) {
                free(reranker);
                return NULL;
            }
            break;

        case RERANKER_MMR:
            strcpy(reranker->name, "mmr");
            reranker->impl.mmr = mmr_reranker_create(config);
            if (reranker->impl.mmr == NULL) {
                free(reranker);
                return NULL;
            }
            break;

        case RERANKER_CUSTOM:
            strcpy(reranker->name, "custom");
            break;

        default:
            free(reranker);
            return NULL;
    }

    return reranker;
}

void reranker_destroy(Reranker *reranker) {
    if (reranker == NULL) {
        return;
    }

    switch (reranker->type) {
        case RERANKER_PRECISE:
            if (reranker->impl.precise) {
                precise_reranker_destroy(reranker->impl.precise);
            }
            break;

        case RERANKER_MULTI_METRIC:
            if (reranker->impl.multi_metric) {
                multi_metric_reranker_destroy(reranker->impl.multi_metric);
            }
            break;

        case RERANKER_MMR:
            if (reranker->impl.mmr) {
                mmr_reranker_destroy(reranker->impl.mmr);
            }
            break;

        case RERANKER_CUSTOM:
            /* 自定义实现不清理，由用户负责 */
            break;
    }

    free(reranker);
}

/* ========================================================================
 * 重排操作
 * ======================================================================== */

int32_t reranker_rerank(Reranker *reranker,
                        const float *query,
                        const float *candidates,
                        int32_t n_candidates,
                        int32_t k,
                        float *scores,
                        int32_t *reranked_ids) {
    if (reranker == NULL || query == NULL || candidates == NULL) {
        return -1;
    }

    if (n_candidates <= 0 || k <= 0) {
        return 0;
    }

    int32_t actual_k = (k > n_candidates) ? n_candidates : k;

    switch (reranker->type) {
        case RERANKER_PRECISE:
            return precise_reranker_rerank(reranker->impl.precise,
                                           query, candidates, n_candidates,
                                           actual_k, scores, reranked_ids);

        case RERANKER_MULTI_METRIC:
            return multi_metric_reranker_rerank(reranker->impl.multi_metric,
                                                query, candidates, n_candidates,
                                                actual_k, scores, reranked_ids);

        case RERANKER_MMR:
            return mmr_reranker_rerank(reranker->impl.mmr,
                                       query, candidates, n_candidates,
                                       actual_k, scores, reranked_ids);

        case RERANKER_CUSTOM:
            if (reranker->custom_func) {
                return reranker->custom_func(query, candidates, n_candidates,
                                             actual_k, scores, reranked_ids,
                                             reranker->custom_user_data);
            }
            return -1;

        default:
            return -1;
    }
}

/* ========================================================================
 * 属性访问
 * ======================================================================== */

RerankerType reranker_get_type(const Reranker *reranker) {
    return reranker ? reranker->type : RERANKER_PRECISE;
}

const char *reranker_get_name(const Reranker *reranker) {
    return reranker ? reranker->name : "";
}

/* ========================================================================
 * 工厂注册
 * ======================================================================== */

int32_t reranker_register(const char *name, rerank_func_t func, void *user_data) {
    if (name == NULL || func == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_mutex);

    if (g_registered_count >= MAX_REGISTERED_RERANKERS) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    strncpy(g_registered[g_registered_count].name, name, sizeof(g_registered[0].name) - 1);
    g_registered[g_registered_count].func = func;
    g_registered[g_registered_count].user_data = user_data;
    g_registered_count++;

    pthread_mutex_unlock(&g_mutex);

    return 0;
}

int32_t reranker_apply(const char *name,
                       const float *query,
                       const float *candidates,
                       int32_t n_candidates,
                       int32_t k,
                       float *scores,
                       int32_t *reranked_ids) {
    if (name == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_mutex);

    int32_t found = -1;
    for (int32_t i = 0; i < g_registered_count; i++) {
        if (strcmp(g_registered[i].name, name) == 0) {
            found = i;
            break;
        }
    }

    pthread_mutex_unlock(&g_mutex);

    if (found < 0) {
        return -1;
    }

    return g_registered[found].func(query, candidates, n_candidates,
                                    k, scores, reranked_ids,
                                    g_registered[found].user_data);
}

int32_t reranker_list_registered(char names[][64], int32_t max_names) {
    pthread_mutex_lock(&g_mutex);

    int32_t count = (g_registered_count < max_names) ? g_registered_count : max_names;
    for (int32_t i = 0; i < count; i++) {
        strncpy(names[i], g_registered[i].name, 63);
        names[i][63] = '\0';
    }

    pthread_mutex_unlock(&g_mutex);

    return count;
}
