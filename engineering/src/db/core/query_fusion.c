/**
 * @file query_fusion.c
 * @brief 多模态查询结果融合策略实现
 *
 * 实现基于排名的 RRF 融合和基于分数的加权融合。
 */

#include "db/core/query_fusion.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内存管理
 * ======================================================================== */

static void *fusion_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "[query_fusion] 内存分配失败: %zu bytes\n", size);
    }
    return ptr;
}

static void *fusion_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr && nmemb > 0 && size > 0) {
        fprintf(stderr, "[query_fusion] 内存分配失败: %zu x %zu bytes\n", nmemb, size);
    }
    return ptr;
}

/* ========================================================================
 * 配置管理
 * ======================================================================== */

FusionConfig *fusion_config_create(FusionStrategy strategy) {
    FusionConfig *config = fusion_calloc(1, sizeof(FusionConfig));
    if (!config) return NULL;

    config->strategy = strategy;
    config->vector_weight = 0.5;
    config->bm25_weight = 0.5;
    config->rrf_k = 60;
    config->top_k = 10;
    config->normalize_scores = true;

    return config;
}

FusionConfig *fusion_config_create_rrf(int rrf_k, int top_k) {
    FusionConfig *config = fusion_config_create(FUSION_STRATEGY_RRF);
    if (!config) return NULL;

    config->rrf_k = rrf_k > 0 ? rrf_k : 60;
    config->top_k = top_k > 0 ? top_k : 10;

    return config;
}

FusionConfig *fusion_config_create_weighted(double vector_weight, double bm25_weight, int top_k) {
    FusionConfig *config = fusion_config_create(FUSION_STRATEGY_WEIGHTED);
    if (!config) return NULL;

    // 归一化权重
    double total = vector_weight + bm25_weight;
    if (total > 0) {
        config->vector_weight = vector_weight / total;
        config->bm25_weight = bm25_weight / total;
    } else {
        config->vector_weight = 0.5;
        config->bm25_weight = 0.5;
    }

    config->top_k = top_k > 0 ? top_k : 10;

    return config;
}

void fusion_config_free(FusionConfig *config) {
    free(config);
}

/* ========================================================================
 * 结果列表管理
 * ======================================================================== */

QueryResultList *query_result_list_create(int capacity) {
    if (capacity <= 0) capacity = 16;

    QueryResultList *list = fusion_calloc(1, sizeof(QueryResultList));
    if (!list) return NULL;

    list->items = fusion_calloc(capacity, sizeof(QueryResultItem));
    if (!list->items) {
        free(list);
        return NULL;
    }

    list->capacity = capacity;
    list->count = 0;

    return list;
}

int query_result_list_add(QueryResultList *list, const QueryResultItem *item) {
    if (!list || !item) return -1;

    // 扩展容量
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity * 2;
        QueryResultItem *new_items = realloc(list->items, new_capacity * sizeof(QueryResultItem));
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_capacity;
    }

    memcpy(&list->items[list->count++], item, sizeof(QueryResultItem));
    return 0;
}

void query_result_list_free(QueryResultList *list) {
    if (!list) return;
    free(list->items);
    free(list);
}

QueryResultList *query_result_list_copy(const QueryResultList *list) {
    if (!list) return NULL;

    QueryResultList *copy = query_result_list_create(list->capacity);
    if (!copy) return NULL;

    copy->count = list->count;
    memcpy(copy->items, list->items, list->count * sizeof(QueryResultItem));

    return copy;
}

const QueryResultItem *query_result_list_get(const QueryResultList *list, int i) {
    if (!list || i < 0 || i >= list->count) return NULL;
    return &list->items[i];
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 查找结果项的排名
 * @param results 结果数组
 * @param num_results 结果数量
 * @param id 要查找的 ID
 * @return 排名 (1-based)，0 表示未找到
 */
static int find_rank(const QueryResultItem *results, int num_results, int64_t id) {
    for (int i = 0; i < num_results; i++) {
        if (results[i].id == id) {
            return i + 1;  // 1-based 排名
        }
    }
    return 0;  // 未找到
}

/**
 * @brief 查找结果项
 * @param results 结果数组
 * @param num_results 结果数量
 * @param id 要查找的 ID
 * @return 结果项指针，未找到返回 NULL
 */
static const QueryResultItem *find_item(const QueryResultItem *results, int num_results, int64_t id) {
    for (int i = 0; i < num_results; i++) {
        if (results[i].id == id) {
            return &results[i];
        }
    }
    return NULL;
}

/**
 * @brief 归一化分数到 [0, 1]
 */
static double normalize_score(double score, double min_score, double max_score) {
    if (max_score <= min_score) return 1.0;
    return (score - min_score) / (max_score - min_score);
}

/**
 * @brief 交换结果项
 */
static void swap_items(QueryResultItem *a, QueryResultItem *b) {
    QueryResultItem temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief 基于 combined_score 的快速排序比较函数
 */
static int compare_score_desc(const void *a, const void *b) {
    const QueryResultItem *ia = (const QueryResultItem *)a;
    const QueryResultItem *ib = (const QueryResultItem *)b;
    if (ib->combined_score > ia->combined_score) return 1;
    if (ib->combined_score < ia->combined_score) return -1;
    return 0;
}

/**
 * @brief 构建融合结果映射
 */
typedef struct FusedItem_s {
    int64_t id;
    double rrf_score;
    double vector_score;
    double bm25_score;
    int vector_rank;
    int bm25_rank;
} FusedItem;

/* ========================================================================
 * RRF 融合实现
 * ======================================================================== */

QueryResultList *fusion_rrf(const QueryResultItem *vector_results, int num_vector,
                            const QueryResultItem *bm25_results, int num_bm25,
                            const FusionConfig *config) {
    if (!config) return NULL;

    int k = config->rrf_k > 0 ? config->rrf_k : 60;
    int top_k = config->top_k > 0 ? config->top_k : 10;
    double vector_weight = config->vector_weight;
    double bm25_weight = config->bm25_weight;

    // 计算需要的结果列表大小
    int total_items = num_vector + num_bm25;
    if (total_items == 0) {
        return query_result_list_create(0);
    }

    // 分配临时映射
    FusedItem *fused = fusion_calloc(total_items, sizeof(FusedItem));
    if (!fused) return NULL;

    int fused_count = 0;

    // 添加向量结果
    for (int i = 0; i < num_vector; i++) {
        fused[fused_count].id = vector_results[i].id;
        fused[fused_count].vector_rank = i + 1;
        fused[fused_count].bm25_rank = 0;
        fused[fused_count].vector_score = vector_results[i].vector_score;
        fused[fused_count].bm25_score = 0;
        // RRF 分数: 1.0 / (k + rank)
        fused[fused_count].rrf_score = vector_weight * (1.0 / (k + i + 1));
        fused_count++;
    }

    // 合并 BM25 结果
    for (int i = 0; i < num_bm25; i++) {
        // 检查是否已存在
        int existing_idx = -1;
        for (int j = 0; j < fused_count; j++) {
            if (fused[j].id == bm25_results[i].id) {
                existing_idx = j;
                break;
            }
        }

        if (existing_idx >= 0) {
            // 已有，更新排名和分数
            fused[existing_idx].bm25_rank = i + 1;
            fused[existing_idx].bm25_score = bm25_results[i].bm25_score;
            fused[existing_idx].rrf_score += bm25_weight * (1.0 / (k + i + 1));
        } else {
            // 新增
            if (fused_count >= total_items) {
                // 扩展容量
                int new_capacity = total_items * 2;
                FusedItem *new_fused = realloc(fused, new_capacity * sizeof(FusedItem));
                if (!new_fused) {
                    free(fused);
                    return NULL;
                }
                fused = new_fused;
                total_items = new_capacity;
            }

            fused[fused_count].id = bm25_results[i].id;
            fused[fused_count].vector_rank = 0;
            fused[fused_count].bm25_rank = i + 1;
            fused[fused_count].vector_score = 0;
            fused[fused_count].bm25_score = bm25_results[i].bm25_score;
            fused[fused_count].rrf_score = bm25_weight * (1.0 / (k + i + 1));
            fused_count++;
        }
    }

    // 按 RRF 分数排序
    // 使用简单的选择排序（数据量通常不大）
    for (int i = 0; i < fused_count - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < fused_count; j++) {
            if (fused[j].rrf_score > fused[max_idx].rrf_score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            FusedItem temp = fused[i];
            fused[i] = fused[max_idx];
            fused[max_idx] = temp;
        }
    }

    // 创建结果列表
    QueryResultList *result = query_result_list_create(top_k);
    if (!result) {
        free(fused);
        return NULL;
    }

    int count = (fused_count < top_k) ? fused_count : top_k;
    for (int i = 0; i < count; i++) {
        QueryResultItem item;
        memset(&item, 0, sizeof(item));
        item.id = fused[i].id;
        item.vector_score = fused[i].vector_score;
        item.bm25_score = fused[i].bm25_score;
        item.combined_score = fused[i].rrf_score;
        item.vector_rank = fused[i].vector_rank;
        item.bm25_rank = fused[i].bm25_rank;
        item.source = (fused[i].vector_rank > 0 && fused[i].bm25_rank > 0) ? SOURCE_HYBRID :
                      (fused[i].vector_rank > 0 ? SOURCE_VECTOR : SOURCE_BM25);

        query_result_list_add(result, &item);
    }

    free(fused);
    return result;
}

/* ========================================================================
 * 加权融合实现
 * ======================================================================== */

QueryResultList *fusion_weighted(const QueryResultItem *vector_results, int num_vector,
                                  const QueryResultItem *bm25_results, int num_bm25,
                                  const FusionConfig *config) {
    if (!config) return NULL;

    double vector_weight = config->vector_weight;
    double bm25_weight = config->bm25_weight;
    int top_k = config->top_k > 0 ? config->top_k : 10;
    bool normalize = config->normalize_scores;

    // 计算需要的容量
    int total_items = num_vector + num_bm25;
    if (total_items == 0) {
        return query_result_list_create(0);
    }

    // 分配临时映射
    FusedItem *fused = fusion_calloc(total_items, sizeof(FusedItem));
    if (!fused) return NULL;

    int fused_count = 0;

    // 计算分数范围（用于归一化）
    double vector_min = 0, vector_max = 0;
    double bm25_min = 0, bm25_max = 0;

    if (normalize) {
        if (num_vector > 0) {
            vector_min = vector_results[0].vector_score;
            vector_max = vector_results[0].vector_score;
            for (int i = 1; i < num_vector; i++) {
                if (vector_results[i].vector_score < vector_min) vector_min = vector_results[i].vector_score;
                if (vector_results[i].vector_score > vector_max) vector_max = vector_results[i].vector_score;
            }
        }
        if (num_bm25 > 0) {
            bm25_min = bm25_results[0].bm25_score;
            bm25_max = bm25_results[0].bm25_score;
            for (int i = 1; i < num_bm25; i++) {
                if (bm25_results[i].bm25_score < bm25_min) bm25_min = bm25_results[i].bm25_score;
                if (bm25_results[i].bm25_score > bm25_max) bm25_max = bm25_results[i].bm25_score;
            }
        }
    }

    // 添加向量结果
    for (int i = 0; i < num_vector; i++) {
        fused[fused_count].id = vector_results[i].id;
        fused[fused_count].vector_rank = i + 1;
        fused[fused_count].bm25_rank = 0;

        double norm_vector_score = normalize ?
            normalize_score(vector_results[i].vector_score, vector_min, vector_max) :
            vector_results[i].vector_score;
        fused[fused_count].vector_score = norm_vector_score;
        fused[fused_count].bm25_score = 0;

        fused[fused_count].rrf_score = vector_weight * norm_vector_score;
        fused_count++;
    }

    // 合并 BM25 结果
    for (int i = 0; i < num_bm25; i++) {
        int existing_idx = -1;
        for (int j = 0; j < fused_count; j++) {
            if (fused[j].id == bm25_results[i].id) {
                existing_idx = j;
                break;
            }
        }

        double norm_bm25_score = normalize ?
            normalize_score(bm25_results[i].bm25_score, bm25_min, bm25_max) :
            bm25_results[i].bm25_score;

        if (existing_idx >= 0) {
            fused[existing_idx].bm25_rank = i + 1;
            fused[existing_idx].bm25_score = norm_bm25_score;
            fused[existing_idx].rrf_score += bm25_weight * norm_bm25_score;
        } else {
            if (fused_count >= total_items) {
                int new_capacity = total_items * 2;
                FusedItem *new_fused = realloc(fused, new_capacity * sizeof(FusedItem));
                if (!new_fused) {
                    free(fused);
                    return NULL;
                }
                fused = new_fused;
                total_items = new_capacity;
            }

            fused[fused_count].id = bm25_results[i].id;
            fused[fused_count].vector_rank = 0;
            fused[fused_count].bm25_rank = i + 1;
            fused[fused_count].vector_score = 0;
            fused[fused_count].bm25_score = norm_bm25_score;
            fused[fused_count].rrf_score = bm25_weight * norm_bm25_score;
            fused_count++;
        }
    }

    // 按加权分数排序
    for (int i = 0; i < fused_count - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < fused_count; j++) {
            if (fused[j].rrf_score > fused[max_idx].rrf_score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            FusedItem temp = fused[i];
            fused[i] = fused[max_idx];
            fused[max_idx] = temp;
        }
    }

    // 创建结果列表
    QueryResultList *result = query_result_list_create(top_k);
    if (!result) {
        free(fused);
        return NULL;
    }

    int count = (fused_count < top_k) ? fused_count : top_k;
    for (int i = 0; i < count; i++) {
        QueryResultItem item;
        memset(&item, 0, sizeof(item));
        item.id = fused[i].id;
        item.vector_score = fused[i].vector_score;
        item.bm25_score = fused[i].bm25_score;
        item.combined_score = fused[i].rrf_score;
        item.vector_rank = fused[i].vector_rank;
        item.bm25_rank = fused[i].bm25_rank;
        item.source = (fused[i].vector_rank > 0 && fused[i].bm25_rank > 0) ? SOURCE_HYBRID :
                      (fused[i].vector_rank > 0 ? SOURCE_VECTOR : SOURCE_BM25);

        query_result_list_add(result, &item);
    }

    free(fused);
    return result;
}

/* ========================================================================
 * 自定义融合
 * ======================================================================== */

QueryResultList *fusion_custom(const QueryResultItem *items, int num_items,
                                const FusionConfig *config,
                                FusionCallback callback, void *ctx) {
    if (!items || num_items <= 0 || !config || !callback) return NULL;

    int top_k = config->top_k > 0 ? config->top_k : num_items;

    // 分配分数数组
    double *scores = fusion_calloc(num_items, sizeof(double));
    if (!scores) return NULL;

    // 调用自定义回调计算分数
    callback(items, num_items, config, scores, ctx);

    // 复制并添加 combined_score
    QueryResultItem *items_copy = fusion_calloc(num_items, sizeof(QueryResultItem));
    if (!items_copy) {
        free(scores);
        return NULL;
    }

    for (int i = 0; i < num_items; i++) {
        items_copy[i] = items[i];
        items_copy[i].combined_score = scores[i];
    }

    // 排序
    qsort(items_copy, num_items, sizeof(QueryResultItem), compare_score_desc);

    // 取 top_k
    int count = (num_items < top_k) ? num_items : top_k;
    QueryResultList *result = query_result_list_create(count);
    if (!result) {
        free(scores);
        free(items_copy);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        query_result_list_add(result, &items_copy[i]);
    }

    free(scores);
    free(items_copy);

    return result;
}

/* ========================================================================
 * 主融合入口
 * ======================================================================== */

QueryResultList *fusion_execute(const QueryResultItem *vector_results, int num_vector,
                                 const QueryResultItem *bm25_results, int num_bm25,
                                 const FusionConfig *config) {
    if (!config) return NULL;

    switch (config->strategy) {
        case FUSION_STRATEGY_RRF:
            return fusion_rrf(vector_results, num_vector, bm25_results, num_bm25, config);

        case FUSION_STRATEGY_WEIGHTED:
            return fusion_weighted(vector_results, num_vector, bm25_results, num_bm25, config);

        case FUSION_STRATEGY_CUSTOM:
            // 自定义融合需要特殊处理，这里返回 NULL
            return NULL;

        default:
            return NULL;
    }
}
