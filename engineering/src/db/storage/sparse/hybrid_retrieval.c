/**
 * @file hybrid_retrieval.c
 * @brief 混合检索实现
 *
 * 将稠密向量检索、稀疏向量检索、BM25 全文检索的分数进行加权融合。
 * 支持 Min-Max 归一化，确保各路分数处于相同量级。
 * top-k 选择使用 min-heap，复杂度 O(n log k)。
 */
#include "db/hybrid_retrieval.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========================================================================
 * Min-Heap（最小堆）用于 top-k 选择
 *
 * 维护一个大小为 k 的最小堆，堆顶为当前 k 个最大元素中的最小值。
 * 遍历所有候选时，若新候选分数大于堆顶，则替换堆顶并调整。
 * 最终堆中即为 top-k 结果，依次弹出即为降序排列。
 * ======================================================================== */

typedef struct {
    float *scores;
    uint32_t *ids;
    int size;
    int capacity;
} min_heap_t;

static void min_heap_siftdown(min_heap_t *heap, int i) {
    while (1) {
        int smallest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left < heap->size && heap->scores[left] < heap->scores[smallest])
            smallest = left;
        if (right < heap->size && heap->scores[right] < heap->scores[smallest])
            smallest = right;
        if (smallest == i) break;
        /* swap */
        float tmp_s = heap->scores[i];
        heap->scores[i] = heap->scores[smallest];
        heap->scores[smallest] = tmp_s;
        uint32_t tmp_id = heap->ids[i];
        heap->ids[i] = heap->ids[smallest];
        heap->ids[smallest] = tmp_id;
        i = smallest;
    }
}

static void min_heap_siftup(min_heap_t *heap, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap->scores[i] >= heap->scores[parent]) break;
        float tmp_s = heap->scores[i];
        heap->scores[i] = heap->scores[parent];
        heap->scores[parent] = tmp_s;
        uint32_t tmp_id = heap->ids[i];
        heap->ids[i] = heap->ids[parent];
        heap->ids[parent] = tmp_id;
        i = parent;
    }
}

static void min_heap_init(min_heap_t *heap, int capacity) {
    heap->scores = (float *)malloc((size_t)capacity * sizeof(float));
    heap->ids = (uint32_t *)malloc((size_t)capacity * sizeof(uint32_t));
    heap->size = 0;
    heap->capacity = capacity;
}

static void min_heap_push(min_heap_t *heap, float score, uint32_t id) {
    int i = heap->size++;
    heap->scores[i] = score;
    heap->ids[i] = id;
    min_heap_siftup(heap, i);
}

static float min_heap_peek(min_heap_t *heap) {
    return heap->scores[0];
}

static void min_heap_pop(min_heap_t *heap) {
    heap->size--;
    heap->scores[0] = heap->scores[heap->size];
    heap->ids[0] = heap->ids[heap->size];
    if (heap->size > 0)
        min_heap_siftdown(heap, 0);
}

static int min_heap_size(min_heap_t *heap) {
    return heap->size;
}

static void min_heap_destroy(min_heap_t *heap) {
    free(heap->scores);
    free(heap->ids);
    heap->scores = NULL;
    heap->ids = NULL;
    heap->size = 0;
    heap->capacity = 0;
}

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief Min-Max 归一化到 [0, 1]
 */
static void normalize_scores(float *scores, uint32_t count) {
    if (count == 0) return;

    float min_val = scores[0];
    float max_val = scores[0];
    for (uint32_t i = 1; i < count; i++) {
        if (scores[i] < min_val) min_val = scores[i];
        if (scores[i] > max_val) max_val = scores[i];
    }

    float range = max_val - min_val;
    if (range < 1e-10f) {
        /* 所有分数相同，归一化为 1 */
        for (uint32_t i = 0; i < count; i++) {
            scores[i] = 1.0f;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            scores[i] = (scores[i] - min_val) / range;
        }
    }
}

/**
 * @brief 比较函数：按 final_score 降序排列
 */
static int compare_results_desc(const void *a, const void *b) {
    float sa = ((const hybrid_result_t*)a)->final_score;
    float sb = ((const hybrid_result_t*)b)->final_score;
    if (sb > sa) return 1;
    if (sb < sa) return -1;
    return 0;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

hybrid_config_t hybrid_config_default(void) {
    hybrid_config_t config;
    config.alpha = 0.4f;
    config.beta = 0.3f;
    config.gamma = 0.3f;
    config.normalize_scores = true;
    return config;
}

int hybrid_search(
    const float *query_dense,
    uint32_t dense_dim,
    const sparse_vector_t *query_sparse,
    const char *query_text,
    const void *dense_index,
    uint32_t dense_count,
    const bm25_index_t *bm25_index,
    const hybrid_config_t *config,
    uint32_t top_k,
    hybrid_result_t *results,
    uint32_t *num_results)
{
    (void)query_sparse; /* 稀疏检索待后续引擎接入 */

    if (!results || !num_results || top_k == 0) {
        LOG_ERROR("hybrid_search: 参数错误");
        return -1;
    }

    *num_results = 0;

    /* 权重归一化 */
    hybrid_config_t cfg = config ? *config : hybrid_config_default();
    float w_sum = cfg.alpha + cfg.beta + cfg.gamma;
    if (w_sum < 1e-10f) {
        LOG_ERROR("hybrid_search: 权重之和为 0");
        return -1;
    }
    cfg.alpha /= w_sum;
    cfg.beta /= w_sum;
    cfg.gamma /= w_sum;

    /*
     * 策略：收集所有候选 ID，对每个候选计算三路分数。
     * 实际项目中应使用各检索引擎的 top-k 接口，
     * 此处为简化实现，遍历稠密索引和 BM25 索引的所有文档。
     */

    /* 候选 ID 集合（使用简单数组） */
    uint32_t candidate_cap = top_k * 4;
    uint64_t *candidate_ids = malloc(candidate_cap * sizeof(uint64_t));
    if (!candidate_ids) {
        LOG_ERROR("hybrid_search: 内存分配失败");
        return -1;
    }
    uint32_t candidate_count = 0;

    /* 辅助分数数组 */
    float *dense_scores = calloc(candidate_cap, sizeof(float));
    float *sparse_scores = calloc(candidate_cap, sizeof(float));
    float *bm25_scores_arr = calloc(candidate_cap, sizeof(float));

    if (!dense_scores || !sparse_scores || !bm25_scores_arr) {
        free(candidate_ids);
        free(dense_scores);
        free(sparse_scores);
        free(bm25_scores_arr);
        LOG_ERROR("hybrid_search: 分数数组分配失败");
        return -1;
    }

    /*
     * Step 1: 从稠密索引收集候选
     * dense_index 暂时视为 float* 数组（简化），实际应调用向量检索引擎
     * 这里假设 dense_index 为 NULL 时跳过稠密检索
     */
    if (query_dense && dense_index && dense_count > 0) {
        /* 简化：遍历所有稠密向量计算余弦相似度 */
        const float *vectors = (const float *)dense_index;
        for (uint32_t i = 0; i < dense_count && candidate_count < candidate_cap; i++) {
            uint64_t id = (uint64_t)i;

            /* 计算点积作为相似度（假设向量已归一化） */
            float dot = 0.0f;
            for (uint32_t d = 0; d < dense_dim; d++) {
                dot += query_dense[d] * vectors[i * dense_dim + d];
            }

            candidate_ids[candidate_count] = id;
            dense_scores[candidate_count] = dot;
            candidate_count++;
        }
    }

    /*
     * Step 2: BM25 检索
     */
    if (query_text && bm25_index) {
        /* 对已有候选和 BM25 结果取并集 */
        uint64_t bm25_results[4096];
        float bm25_s[4096];
        uint32_t bm25_count = (uint32_t)bm25_search(bm25_index, query_text,
                                                      (candidate_count > 4096 ? 4096 : candidate_count > top_k * 2 ? top_k * 2 : 4096),
                                                      bm25_results, bm25_s);

        for (uint32_t i = 0; i < (uint32_t)bm25_count && candidate_count < candidate_cap; i++) {
            /* 查找是否已存在 */
            bool found = false;
            for (uint32_t j = 0; j < candidate_count; j++) {
                if (candidate_ids[j] == bm25_results[i]) {
                    bm25_scores_arr[j] = bm25_s[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                candidate_ids[candidate_count] = bm25_results[i];
                bm25_scores_arr[candidate_count] = bm25_s[i];
                candidate_count++;
            }
        }
    }

    if (candidate_count == 0) {
        *num_results = 0;
        free(candidate_ids);
        free(dense_scores);
        free(sparse_scores);
        free(bm25_scores_arr);
        return 0;
    }

    /*
     * Step 3: 归一化各路分数
     */
    if (cfg.normalize_scores) {
        if (dense_index) normalize_scores(dense_scores, candidate_count);
        if (query_text && bm25_index) normalize_scores(bm25_scores_arr, candidate_count);
        /* sparse_scores 暂未填充，默认 0 */
    }

    /*
     * Step 4: 加权融合
     */
    for (uint32_t i = 0; i < candidate_count; i++) {
        results[i].id = candidate_ids[i];
        results[i].dense_score = dense_scores[i];
        results[i].sparse_score = sparse_scores[i];
        results[i].bm25_score = bm25_scores_arr[i];
        results[i].final_score = cfg.alpha * dense_scores[i]
                               + cfg.beta * sparse_scores[i]
                               + cfg.gamma * bm25_scores_arr[i];
    }

    /*
     * Step 5: 使用 min-heap 取 top-k（O(n log k)，优于 O(n log n) 排序）
     *
     * 策略：先对前 k 个元素建最小堆，再遍历剩余 n-k 个候选。
     * 若某候选分数大于堆顶（当前 k 个中的最小），则替换堆顶并调整。
     * 最终堆中即为 top-k（无序），依次 pop 堆顶得到降序结果。
     */
    if (candidate_count <= top_k) {
        /* 候选数 <= k，直接排序返回 */
        qsort(results, candidate_count, sizeof(hybrid_result_t), compare_results_desc);
        *num_results = candidate_count;
    } else {
        min_heap_t heap;
        min_heap_init(&heap, (int)top_k);

        /* 填充前 k 个元素建初始堆（自下而上堆化 O(k)）*/
        for (uint32_t i = 0; i < top_k; i++) {
            heap.scores[heap.size] = results[i].final_score;
            heap.ids[heap.size] = i;
            heap.size++;
        }
        for (int i = (int)(top_k / 2) - 1; i >= 0; i--) {
            min_heap_siftdown(&heap, i);
        }

        /* 遍历剩余候选，比堆顶大则替换并调整（O((n-k) log k)）*/
        for (uint32_t i = top_k; i < candidate_count; i++) {
            if (results[i].final_score > min_heap_peek(&heap)) {
                heap.scores[0] = results[i].final_score;
                heap.ids[0] = i;
                min_heap_siftdown(&heap, 0);
            }
        }

        /* 依次弹出堆顶得到降序排列，同时将结果写回 results[0..top_k-1] */
        hybrid_result_t *sorted = (hybrid_result_t *)malloc((size_t)top_k * sizeof(hybrid_result_t));
        if (!sorted) {
            min_heap_destroy(&heap);
            free(candidate_ids);
            free(dense_scores);
            free(sparse_scores);
            free(bm25_scores_arr);
            LOG_ERROR("hybrid_search: 内存分配失败");
            return -1;
        }
        for (uint32_t i = 0; i < top_k; i++) {
            /* 堆顶是当前 k 个中的最小值 */
            uint32_t src_idx = heap.ids[0];
            sorted[top_k - 1 - i] = results[src_idx]; /* 逆序填入：第 i 次 pop 是第 i 大 */
            /* 弹出堆顶 */
            heap.size--;
            if (heap.size > 0) {
                heap.scores[0] = heap.scores[heap.size];
                heap.ids[0] = heap.ids[heap.size];
                min_heap_siftdown(&heap, 0);
            }
        }
        /* 复制排序结果回 results */
        memcpy(results, sorted, (size_t)top_k * sizeof(hybrid_result_t));
        free(sorted);

        min_heap_destroy(&heap);
        *num_results = top_k;
    }

    free(candidate_ids);
    free(dense_scores);
    free(sparse_scores);
    free(bm25_scores_arr);

    LOG_INFO("hybrid_search: 候选数=%u, 返回=%u", candidate_count, *num_results);
    return 0;
}
