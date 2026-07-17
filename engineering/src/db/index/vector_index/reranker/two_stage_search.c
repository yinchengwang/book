/**
 * @file two_stage_search.c
 * @brief 两阶段检索实现
 */

#include "db/index/vector_index/reranker/two_stage_search.h"
#include "db/index/vector_index/reranker/reranker.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 两阶段检索器内部结构 */
struct TwoStageSearcher_s {
    void *stage1_index;          /**< Stage 1 索引 */
    Reranker *reranker;          /**< Stage 2 精排器 */
    TwoStageConfig config;       /**< 配置 */

    /* 统计 */
    TwoStageStats stats;
};

/* ========================================================================
 * 时间工具
 * ======================================================================== */

static int64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* ========================================================================
 * 配置默认值
 * ======================================================================== */

TwoStageConfig two_stage_config_default(int32_t dims) {
    TwoStageConfig config = {
        .stage1 = {
            .k = 100,
            .ef_search = 100,
            .use_pq = true
        },
        .stage2 = {
            .reranker_type = RERANKER_PRECISE,
            .reranker_config = { 0 },
            .enable_mmr = false,
            .mmr_lambda = 0.5f
        },
        .monitor = {
            .enable_timing = true,
            .enable_stats = true
        }
    };

    config.stage2.reranker_config = reranker_config_default(RERANKER_PRECISE, dims);

    return config;
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

TwoStageSearcher *two_stage_searcher_create(void *stage1_index,
                                           const TwoStageConfig *config) {
    if (config == NULL) {
        return NULL;
    }

    TwoStageSearcher *searcher = (TwoStageSearcher *)calloc(1, sizeof(TwoStageSearcher));
    if (searcher == NULL) {
        return NULL;
    }

    searcher->stage1_index = stage1_index;

    /* 复制配置 */
    searcher->config = *config;

    /* 确保精排配置维度正确 */
    if (config->stage2.reranker_config.dims == 0) {
        searcher->config.stage2.reranker_config.dims = 128;  /* 默认维度 */
    }

    /* 创建精排器 */
    RerankerConfig reranker_cfg = reranker_config_default(
        config->stage2.reranker_type,
        searcher->config.stage2.reranker_config.dims);

    /* 如果用户提供了配置，使用用户配置 */
    if (config->stage2.reranker_config.dims > 0) {
        reranker_cfg = config->stage2.reranker_config;
    }

    searcher->reranker = reranker_create(config->stage2.reranker_type, &reranker_cfg);
    if (searcher->reranker == NULL) {
        free(searcher);
        return NULL;
    }

    /* 初始化统计 */
    memset(&searcher->stats, 0, sizeof(searcher->stats));

    return searcher;
}

void two_stage_searcher_destroy(TwoStageSearcher *searcher) {
    if (searcher == NULL) {
        return;
    }

    if (searcher->reranker) {
        reranker_destroy(searcher->reranker);
    }

    free(searcher);
}

/* ========================================================================
 * 两阶段检索
 * ======================================================================== */

/**
 * @brief 获取向量数据（抽象接口）
 *
 * 注意：实际实现需要与底层索引交互
 * 这里使用占位实现
 */
static const float *get_vector_data(void *index, int32_t id) {
    (void)index;
    (void)id;
    /* TODO: 从底层索引获取向量数据 */
    return NULL;
}

int32_t two_stage_search(TwoStageSearcher *searcher,
                         const float *query,
                         int32_t k,
                         TwoStageResult *results,
                         TwoStageStats *stats) {
    if (searcher == NULL || query == NULL || results == NULL) {
        return -1;
    }

    int64_t start_time = get_time_us();

    /* Stage 1: 粗排 */
    int32_t stage1_k = searcher->config.stage1.k;
    if (stage1_k < k) {
        stage1_k = k * 2;  /* 默认取 2 倍候选 */
    }

    int32_t *stage1_ids = (int32_t *)malloc((size_t)stage1_k * sizeof(int32_t));
    float *stage1_distances = (float *)malloc((size_t)stage1_k * sizeof(float));

    if (stage1_ids == NULL || stage1_distances == NULL) {
        free(stage1_ids);
        free(stage1_distances);
        return -1;
    }

    int64_t stage1_start = get_time_us();

    /* 调用 Stage 1 索引搜索（如果有） */
    if (searcher->stage1_index != NULL) {
        /* TODO: 调用实际索引搜索 */
        /* 这里暂时返回空结果，表示没有 Stage 1 索引 */
        free(stage1_ids);
        free(stage1_distances);

        if (stats) {
            memset(stats, 0, sizeof(TwoStageStats));
        }

        /* 尝试直接使用精排器 */
        if (searcher->reranker) {
            float *dummy_candidates = (float *)malloc((size_t)k * searcher->config.stage2.reranker_config.dims * sizeof(float));
            if (dummy_candidates) {
                /* 返回空结果（因为没有候选数据） */
                free(dummy_candidates);
            }
        }

        return 0;
    }

    int64_t stage1_end = get_time_us();

    /* 如果没有 Stage 1 索引，使用精排器返回空结果 */
    free(stage1_ids);
    free(stage1_distances);

    if (stats) {
        stats->stage1_time_us = stage1_end - stage1_start;
        stats->stage2_time_us = 0;
        stats->total_time_us = get_time_us() - start_time;
        stats->candidates = 0;
        stats->returned = 0;
    }

    return 0;
}

/* ========================================================================
 * Stage 1 / Stage 2 分离执行
 * ======================================================================== */

int32_t two_stage_search_stage1(TwoStageSearcher *searcher,
                                const float *query,
                                int32_t k,
                                int32_t *ids,
                                float *distances) {
    if (searcher == NULL || query == NULL || ids == NULL) {
        return -1;
    }

    if (searcher->stage1_index == NULL) {
        return 0;  /* 没有 Stage 1 索引 */
    }

    /* TODO: 调用实际索引搜索 */
    (void)k;
    (void)distances;

    return 0;
}

int32_t two_stage_search_stage2(TwoStageSearcher *searcher,
                                const float *query,
                                const float *candidates,
                                int32_t n_candidates,
                                int32_t k,
                                int32_t *reranked_ids,
                                float *scores) {
    if (searcher == NULL || query == NULL || candidates == NULL) {
        return -1;
    }

    if (searcher->reranker == NULL) {
        return -1;
    }

    return reranker_rerank(searcher->reranker, query, candidates,
                           n_candidates, k, scores, reranked_ids);
}

/* ========================================================================
 * 统计
 * ======================================================================== */

void two_stage_searcher_get_stats(const TwoStageSearcher *searcher,
                                  TwoStageStats *stats) {
    if (searcher == NULL || stats == NULL) {
        return;
    }

    *stats = searcher->stats;
}

void two_stage_searcher_reset_stats(TwoStageSearcher *searcher) {
    if (searcher == NULL) {
        return;
    }

    memset(&searcher->stats, 0, sizeof(searcher->stats));
}

/* ========================================================================
 * 便捷函数
 * ======================================================================== */

int32_t mmr_rerank_standalone(const float *candidates,
                              int32_t *ids,
                              int32_t n,
                              int32_t k,
                              float lambda,
                              distance_metric_t metric,
                              int32_t *reranked_ids) {
    if (candidates == NULL || ids == NULL || reranked_ids == NULL) {
        return -1;
    }

    /* 创建临时 MMR 精排器 */
    RerankerConfig config = reranker_config_default(RERANKER_MMR, 128);
    config.mmr.lambda = lambda;
    config.metric = metric;

    Reranker *reranker = reranker_create(RERANKER_MMR, &config);
    if (reranker == NULL) {
        return -1;
    }

    float *scores = (float *)malloc((size_t)n * sizeof(float));
    if (scores == NULL) {
        reranker_destroy(reranker);
        return -1;
    }

    int32_t result = reranker_rerank(reranker, NULL, candidates,
                                     n, k, scores, reranked_ids);

    free(scores);
    reranker_destroy(reranker);

    return result;
}
