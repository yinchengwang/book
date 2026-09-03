/**
 * @file concurrent_search.c
 * @brief 并发查询适配器实现
 */

#include "concurrent_search.h"
#include "vector_write_buffer.h"

#include <db/index/vector_index/hnsw/faiss_hnsw.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/**
 * @brief 并发查询适配器内部结构
 */
struct concurrent_search_adapter {
    vector_index_t *index;              /* 主索引 */
    vector_write_buffer_t *buffer;      /* 缓冲区 */

    /* 配置 */
    bool enable_buffer_search;
    int32_t buffer_search_k;
    float buffer_weight;
    bool deduplicate;

    /* 临时缓冲区（避免重复分配） */
    int32_t *temp_ids;
    float *temp_distances;
    int32_t temp_capacity;

    /* 统计 */
    int64_t total_searches;
    int64_t buffer_hits;
    int64_t deduplications;
};

/* ========================================================================
 * 结果结构
 * ======================================================================== */

concurrent_search_result_t *concurrent_search_result_create(int32_t capacity) {
    if (capacity <= 0) {
        return NULL;
    }

    concurrent_search_result_t *result =
        (concurrent_search_result_t *)calloc(1, sizeof(concurrent_search_result_t));
    if (result == NULL) {
        return NULL;
    }

    result->ids = (int32_t *)malloc((size_t)capacity * sizeof(int32_t));
    result->distances = (float *)malloc((size_t)capacity * sizeof(float));

    if (result->ids == NULL || result->distances == NULL) {
        free(result->ids);
        free(result->distances);
        free(result);
        return NULL;
    }

    result->capacity = capacity;
    result->count = 0;

    return result;
}

void concurrent_search_result_destroy(concurrent_search_result_t *result) {
    if (result == NULL) {
        return;
    }

    free(result->ids);
    free(result->distances);
    free(result);
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

concurrent_search_adapter_t *concurrent_search_create(
    vector_index_t *index,
    vector_write_buffer_t *buffer,
    const concurrent_search_config_t *config) {

    if (index == NULL) {
        return NULL;
    }

    concurrent_search_adapter_t *adapter =
        (concurrent_search_adapter_t *)calloc(1, sizeof(concurrent_search_adapter_t));
    if (adapter == NULL) {
        return NULL;
    }

    adapter->index = index;
    adapter->buffer = buffer;

    /* 默认配置 */
    adapter->enable_buffer_search = true;
    adapter->buffer_search_k = CONCURRENT_SEARCH_DEFAULT_BUFFER_K;
    adapter->buffer_weight = CONCURRENT_SEARCH_DEFAULT_BUFFER_WEIGHT;
    adapter->deduplicate = true;

    /* 使用用户配置覆盖 */
    if (config != NULL) {
        adapter->enable_buffer_search = config->enable_buffer_search;
        adapter->buffer_search_k = config->buffer_search_k > 0 ?
            config->buffer_search_k : CONCURRENT_SEARCH_DEFAULT_BUFFER_K;
        adapter->buffer_weight = config->buffer_weight;
        adapter->deduplicate = config->deduplicate;
    }

    /* 分配临时缓冲区 */
    int32_t temp_size = 1024;  /* 默认容量 */
    adapter->temp_ids = (int32_t *)malloc((size_t)temp_size * sizeof(int32_t));
    adapter->temp_distances = (float *)malloc((size_t)temp_size * sizeof(float));

    if (adapter->temp_ids == NULL || adapter->temp_distances == NULL) {
        free(adapter->temp_ids);
        free(adapter->temp_distances);
        free(adapter);
        return NULL;
    }

    adapter->temp_capacity = temp_size;
    adapter->total_searches = 0;
    adapter->buffer_hits = 0;
    adapter->deduplications = 0;

    return adapter;
}

void concurrent_search_destroy(concurrent_search_adapter_t *adapter) {
    if (adapter == NULL) {
        return;
    }

    free(adapter->temp_ids);
    free(adapter->temp_distances);
    free(adapter);
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 确保临时缓冲区足够大
 */
static int32_t ensure_temp_capacity(concurrent_search_adapter_t *adapter, int32_t needed) {
    if (adapter == NULL || needed <= 0) {
        return -1;
    }

    if (needed <= adapter->temp_capacity) {
        return 0;
    }

    int32_t new_capacity = adapter->temp_capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    int32_t *new_ids = (int32_t *)realloc(adapter->temp_ids, (size_t)new_capacity * sizeof(int32_t));
    float *new_distances = (float *)realloc(adapter->temp_distances, (size_t)new_capacity * sizeof(float));

    if (new_ids == NULL || new_distances == NULL) {
        if (new_ids) free(new_ids);
        if (new_distances) free(new_distances);
        return -1;
    }

    adapter->temp_ids = new_ids;
    adapter->temp_distances = new_distances;
    adapter->temp_capacity = new_capacity;

    return 0;
}

/**
 * @brief 简单的暴力搜索（用于缓冲区）
 *
 * TODO: 实际应该调用 HNSW/DiskANN 的搜索接口
 * 这里简化实现，实际需要传入搜索函数指针
 */
static int32_t brute_search_buffer(
    const float *query,
    const float *buffer_data,
    int32_t buffer_size,
    int32_t dims,
    int32_t k,
    int32_t *ids,
    float *distances) {

    if (query == NULL || buffer_data == NULL || buffer_size <= 0 || k <= 0) {
        return 0;
    }

    /* 简化实现：计算所有向量的距离并排序 */
    typedef struct {
        int32_t id;
        float dist;
    } pair_t;

    pair_t *pairs = (pair_t *)malloc((size_t)buffer_size * sizeof(pair_t));
    if (pairs == NULL) {
        return 0;
    }

    /* 计算距离 */
    for (int32_t i = 0; i < buffer_size; i++) {
        const float *vec = buffer_data + (size_t)i * dims;
        float dist = 0.0f;
        for (int32_t d = 0; d < dims; d++) {
            float diff = query[d] - vec[d];
            dist += diff * diff;
        }
        pairs[i].id = i + INT_MAX / 2;  /* 缓冲区 ID 使用特殊前缀 */
        pairs[i].dist = dist;
    }

    /* 简单选择排序（对大数据集应使用堆） */
    for (int32_t i = 0; i < buffer_size - 1 && i < k; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < buffer_size; j++) {
            if (pairs[j].dist < pairs[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            pair_t tmp = pairs[i];
            pairs[i] = pairs[min_idx];
            pairs[min_idx] = tmp;
        }
    }

    /* 复制结果 */
    int32_t count = buffer_size < k ? buffer_size : k;
    for (int32_t i = 0; i < count; i++) {
        ids[i] = pairs[i].id;
        distances[i] = pairs[i].dist;
    }

    free(pairs);
    return count;
}

/* ========================================================================
 * 查询实现
 * ======================================================================== */

int32_t concurrent_search(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids) {

    if (adapter == NULL || query == NULL || k <= 0) {
        return -1;
    }

    adapter->total_searches++;

    /* 确保临时缓冲区足够 */
    int32_t temp_k = k * 2;  /* 预留合并空间 */
    if (ensure_temp_capacity(adapter, temp_k * 2) != 0) {
        return -1;
    }

    /* 第一步：搜索主索引 */
    int32_t main_count = 0;
    if (adapter->index != NULL) {
        /* 调用 HNSW 搜索接口，搜索更多候选以留出缓冲区空间 */
        int32_t search_k = k * 2;
        main_count = faiss_hnsw_index_search(
            (faiss_hnsw_t *)adapter->index,
            query,
            search_k,
            100,
            adapter->temp_distances + k,  /* 临时存储 */
            adapter->temp_ids + k
        );
        if (main_count > 0) {
            /* 复制到临时缓冲区开头 */
            for (int32_t i = 0; i < main_count; i++) {
                adapter->temp_ids[i] = adapter->temp_ids[k + i];
                adapter->temp_distances[i] = adapter->temp_distances[k + i];
            }
        }
    }

    /* 第二步：搜索缓冲区（如果启用） */
    int32_t buffer_count = 0;
    if (adapter->enable_buffer_search && adapter->buffer != NULL) {
        int32_t buffer_size = vector_buffer_size(adapter->buffer);
        if (buffer_size > 0) {
            /* 获取缓冲区数据 */
            int32_t max_fetch = adapter->buffer_search_k;
            float *buffer_data = (float *)malloc((size_t)max_fetch * 128 * sizeof(float));  /* 假设 dims <= 128 */
            if (buffer_data != NULL) {
                /* 简化：直接获取所有缓冲区数据 */
                /* TODO: 实际需要 vector_buffer_peek 或类似接口 */
                buffer_count = 0;  /* 占位 */
                free(buffer_data);
            }

            if (buffer_count > 0) {
                adapter->buffer_hits++;
            }
        }
    }

    /* 第三步：合并结果 */
    int32_t total_count = main_count + buffer_count;
    if (total_count == 0) {
        return 0;
    }

    /* 简化：直接复制主索引结果到输出 */
    /* TODO: 实际需要合并逻辑 */
    int32_t result_count = main_count < k ? main_count : k;
    for (int32_t i = 0; i < result_count; i++) {
        ids[i] = adapter->temp_ids[i];
        distances[i] = adapter->temp_distances[i];
    }

    return result_count;
}

int32_t concurrent_search_main_only(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids) {

    if (adapter == NULL || query == NULL || k <= 0) {
        return -1;
    }

    if (adapter->index == NULL) {
        return 0;
    }

    /* 调用 HNSW 搜索接口 */
    return faiss_hnsw_index_search(
        (faiss_hnsw_t *)adapter->index,
        query,
        k,
        100,  /* ef_search 默认值 */
        distances,
        ids
    );
}

int32_t concurrent_search_buffer_only(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids) {

    if (adapter == NULL || query == NULL || k <= 0) {
        return -1;
    }

    if (adapter->buffer == NULL) {
        return 0;
    }

    int32_t buffer_size = vector_buffer_size(adapter->buffer);
    if (buffer_size == 0) {
        return 0;
    }

    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * 结果合并
 * ======================================================================== */

int32_t concurrent_search_merge_results(
    const int32_t *main_ids,
    const float *main_distances,
    int32_t main_count,
    const int32_t *buffer_ids,
    const float *buffer_distances,
    int32_t buffer_count,
    int32_t k,
    float buffer_weight,
    int32_t *output_ids,
    float *output_distances) {

    if (k <= 0 || output_ids == NULL || output_distances == NULL) {
        return 0;
    }

    /* 构建合并候选列表 */
    typedef struct {
        int32_t id;
        float dist;
    } candidate_t;

    int32_t total_count = main_count + buffer_count;
    if (total_count == 0) {
        return 0;
    }

    candidate_t *candidates = (candidate_t *)malloc((size_t)total_count * sizeof(candidate_t));
    if (candidates == NULL) {
        return 0;
    }

    int32_t idx = 0;

    /* 添加主索引结果 */
    for (int32_t i = 0; i < main_count; i++) {
        candidates[idx].id = main_ids[i];
        candidates[idx].dist = main_distances[i];
        idx++;
    }

    /* 添加缓冲区结果（应用权重） */
    for (int32_t i = 0; i < buffer_count; i++) {
        candidates[idx].id = buffer_ids[i];
        candidates[idx].dist = buffer_distances[i] * buffer_weight;
        idx++;
    }

    /* 选择 Top-K（简单选择排序） */
    for (int32_t i = 0; i < total_count - 1 && i < k; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < total_count; j++) {
            if (candidates[j].dist < candidates[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            candidate_t tmp = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp;
        }
    }

    /* 复制结果 */
    int32_t result_count = total_count < k ? total_count : k;
    for (int32_t i = 0; i < result_count; i++) {
        output_ids[i] = candidates[i].id;
        output_distances[i] = candidates[i].dist;
    }

    free(candidates);
    return result_count;
}

int32_t concurrent_search_deduplicate(
    int32_t *ids,
    float *distances,
    int32_t count) {

    if (ids == NULL || distances == NULL || count <= 0) {
        return 0;
    }

    int32_t write_idx = 0;

    for (int32_t read_idx = 0; read_idx < count; read_idx++) {
        /* 检查是否重复 */
        bool is_duplicate = false;
        for (int32_t i = 0; i < write_idx; i++) {
            if (ids[i] == ids[read_idx]) {
                is_duplicate = true;
                break;
            }
        }

        if (!is_duplicate) {
            if (write_idx != read_idx) {
                ids[write_idx] = ids[read_idx];
                distances[write_idx] = distances[read_idx];
            }
            write_idx++;
        }
    }

    return write_idx;
}

/* ========================================================================
 * 配置更新
 * ======================================================================== */

void concurrent_search_set_buffer_enabled(
    concurrent_search_adapter_t *adapter,
    bool enable) {

    if (adapter != NULL) {
        adapter->enable_buffer_search = enable;
    }
}

bool concurrent_search_is_buffer_enabled(const concurrent_search_adapter_t *adapter) {
    return adapter ? adapter->enable_buffer_search : false;
}

void concurrent_search_update_buffer(
    concurrent_search_adapter_t *adapter,
    vector_write_buffer_t *buffer) {

    if (adapter != NULL) {
        adapter->buffer = buffer;
    }
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

void concurrent_search_get_stats(
    const concurrent_search_adapter_t *adapter,
    concurrent_search_stats_t *stats) {

    if (adapter == NULL || stats == NULL) {
        return;
    }

    stats->total_searches = adapter->total_searches;
    stats->buffer_hits = adapter->buffer_hits;
    stats->deduplications = adapter->deduplications;
    stats->avg_result_count = adapter->total_searches > 0 ?
        (int32_t)(adapter->total_searches / adapter->total_searches) : 0;  /* TODO: 实际计算 */
}
