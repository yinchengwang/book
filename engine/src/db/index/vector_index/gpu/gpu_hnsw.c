/**
 * @file gpu_hnsw.c
 * @brief GPU-HNSW 索引实现
 *
 * GPU 加速的 HNSW（Hierarchical Navigable Small World）图索引。
 * 提供批量向量插入和搜索功能。
 */
#include "gpu_vector_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内部类型定义
 * ======================================================================== */

/**
 * @brief GPU-HNSW 索引内部结构
 */
struct gpu_hnsw_index_s {
    /* 配置 */
    int32_t dim;                   /**< 向量维度 */
    int32_t M;                     /**< 每层最大连接数 */
    int32_t ef_construction;       /**< 构建时搜索范围 */
    int32_t ef_search;             /**< 搜索时搜索范围 */
    int32_t max_elements;          /**< 最大元素数量 */
    int32_t metric;                /**< 度量类型 */

    /* 状态 */
    int32_t num_elements;          /**< 当前元素数量 */
    int32_t enterpoint_node;       /**< 入口节点 ID */
    int32_t max_level;             /**< 最大层数 */

    /* GPU 内存 */
    gpu_memory_t *vectors_mem;     /**< 向量数据 GPU 内存 */
    gpu_memory_t *neighbors_mem;   /**< 邻居数据 GPU 内存 */
    gpu_memory_t *level_mem;       /**< 每元素层数 GPU 内存 */

    /* CPU 备份（用于持久化兼容） */
    float *vectors_cpu;            /**< 向量数据 CPU 副本 */
    int32_t *neighbors_cpu;        /**< 邻居数据 CPU 副本 */
    int8_t *levels_cpu;            /**< 每元素层数 CPU 副本 */

    /* 距离计算辅助 */
    float *query_buffer;           /**< 查询向量缓冲区 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 计算向量层数（存根：使用随机层数）
 */
static int8_t random_level(float level_factor)
{
    /* TODO: 实现实际的对数正态随机层数 */
    int8_t level = 1;
    while (level < 64 && (rand() / (float)RAND_MAX) < level_factor) {
        level++;
    }
    return level;
}

/**
 * @brief 计算 L2 距离（CPU 辅助）
 */
static float compute_l2_distance(const float *a, const float *b, int32_t dim)
{
    float dist = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return dist;
}

/**
 * @brief 计算内积（CPU 辅助）
 */
static float compute_inner_product(const float *a, const float *b, int32_t dim)
{
    float dot = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    return dot;
}

/**
 * @brief 搜索最近邻（CPU 辅助搜索，存根实现）
 *
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数量
 * @param results 输出结果
 */
static void search_knn_cpu(gpu_hnsw_index_t *index, const float *query,
                           int32_t k, gpu_search_result_item_t *results)
{
    if (index->num_elements == 0) {
        return;
    }

    /* 简化实现：线性扫描所有向量 */
    /* TODO: 实现实际的 HNSW 图搜索算法 */

    typedef struct {
        float dist;
        int32_t id;
    } candidate_t;

    candidate_t *candidates = (candidate_t *)malloc(index->num_elements * sizeof(candidate_t));
    if (candidates == NULL) {
        return;
    }

    /* 计算所有向量与查询的距离 */
    for (int32_t i = 0; i < index->num_elements; i++) {
        if (index->metric == METRIC_L2 || index->metric == 0) {
            candidates[i].dist = compute_l2_distance(
                query, index->vectors_cpu + i * index->dim, index->dim);
        } else {
            candidates[i].dist = -compute_inner_product(
                query, index->vectors_cpu + i * index->dim, index->dim);
        }
        candidates[i].id = i;
    }

    /* 简单选择排序获取 Top-K */
    for (int32_t i = 0; i < k && i < index->num_elements; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < index->num_elements; j++) {
            if (candidates[j].dist < candidates[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            candidate_t tmp = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp;
        }
        results[i].id = candidates[i].id;
        results[i].distance = candidates[i].dist;
    }

    free(candidates);
}

/* ========================================================================
 * GPU-HNSW 索引 API 实现
 * ======================================================================== */

gpu_hnsw_index_t *gpu_hnsw_create(const gpu_hnsw_config_t *config)
{
    gpu_hnsw_index_t *index;

    if (config == NULL || config->dim <= 0 || config->max_elements <= 0) {
        return NULL;
    }

    /* 分配索引结构 */
    index = (gpu_hnsw_index_t *)calloc(1, sizeof(gpu_hnsw_index_t));
    if (index == NULL) {
        return NULL;
    }

    /* 复制配置 */
    index->dim = config->dim;
    index->M = (config->M > 0) ? config->M : 16;
    index->ef_construction = (config->ef_construction > 0) ? config->ef_construction : 200;
    index->ef_search = (config->ef_search > 0) ? config->ef_search : 100;
    index->max_elements = config->max_elements;
    index->metric = config->metric;

    /* 初始化状态 */
    index->num_elements = 0;
    index->enterpoint_node = -1;
    index->max_level = 0;

    /* 分配 CPU 内存（用于兼容和调试） */
    size_t vec_size = (size_t)index->max_elements * index->dim * sizeof(float);
    index->vectors_cpu = (float *)malloc(vec_size);
    if (index->vectors_cpu == NULL) {
        free(index);
        return NULL;
    }
    memset(index->vectors_cpu, 0, vec_size);

    /* 邻居存储：每元素 M*2 个邻居（每层左右各 M 个） * 最大层数 */
    size_t max_levels = 64;  /* 假设最大 64 层 */
    size_t neigh_size = (size_t)index->max_elements * index->M * 2 * max_levels * sizeof(int32_t);
    index->neighbors_cpu = (int32_t *)malloc(neigh_size);
    if (index->neighbors_cpu == NULL) {
        free(index->vectors_cpu);
        free(index);
        return NULL;
    }
    memset(index->neighbors_cpu, 0, neigh_size);

    /* 层数存储 */
    index->levels_cpu = (int8_t *)malloc(index->max_elements * sizeof(int8_t));
    if (index->levels_cpu == NULL) {
        free(index->vectors_cpu);
        free(index->neighbors_cpu);
        free(index);
        return NULL;
    }

    /* 查询缓冲区 */
    index->query_buffer = (float *)malloc(index->dim * sizeof(float));
    if (index->query_buffer == NULL) {
        free(index->vectors_cpu);
        free(index->neighbors_cpu);
        free(index->levels_cpu);
        free(index);
        return NULL;
    }

    /* TODO: 分配 GPU 内存 */
    /* index->vectors_mem = gpu_malloc(vec_size, GPU_MEM_READ_WRITE); */
    /* index->neighbors_mem = gpu_malloc(neigh_size, GPU_MEM_READ_WRITE); */
    /* index->level_mem = gpu_malloc(index->max_elements * sizeof(int8_t), GPU_MEM_READ_WRITE); */

    printf("[GPU-HNSW] 创建索引: dim=%d, M=%d, ef_c=%d, ef_s=%d, max=%d\n",
           index->dim, index->M, index->ef_construction, index->ef_search,
           index->max_elements);

    return index;
}

void gpu_hnsw_destroy(gpu_hnsw_index_t *index)
{
    if (index == NULL) {
        return;
    }

    /* 释放 GPU 内存 */
    if (index->vectors_mem != NULL) {
        gpu_free(index->vectors_mem);
    }
    if (index->neighbors_mem != NULL) {
        gpu_free(index->neighbors_mem);
    }
    if (index->level_mem != NULL) {
        gpu_free(index->level_mem);
    }

    /* 释放 CPU 内存 */
    if (index->vectors_cpu != NULL) {
        free(index->vectors_cpu);
    }
    if (index->neighbors_cpu != NULL) {
        free(index->neighbors_cpu);
    }
    if (index->levels_cpu != NULL) {
        free(index->levels_cpu);
    }
    if (index->query_buffer != NULL) {
        free(index->query_buffer);
    }

    free(index);
    printf("[GPU-HNSW] 销毁索引\n");
}

int32_t gpu_hnsw_insert(gpu_hnsw_index_t *index, const float *vectors,
                        int32_t n, const int32_t *ids)
{
    /* TODO: 实现实际 GPU 加速插入 */
    /* 1. 将向量复制到 GPU */
    /* 2. GPU 并行计算插入位置 */
    /* 3. 更新 HNSW 图结构 */

    /* 简化实现：CPU 线性插入 */
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    if (index->num_elements + n > index->max_elements) {
        n = index->max_elements - index->num_elements;
        if (n <= 0) {
            return -2;  /* 索引已满 */
        }
    }

    /* 复制向量数据 */
    for (int32_t i = 0; i < n; i++) {
        int32_t elem_id = (ids != NULL) ? ids[i] : index->num_elements + i;
        if (elem_id >= 0 && elem_id < index->max_elements) {
            float *dest = index->vectors_cpu + elem_id * index->dim;
            const float *src = vectors + i * index->dim;
            memcpy(dest, src, index->dim * sizeof(float));

            /* 设置层数（存根） */
            float level_factor = 1.0f / (log((float)index->max_elements) + 1e-6f);
            index->levels_cpu[elem_id] = random_level(level_factor);

            if (index->enterpoint_node < 0) {
                index->enterpoint_node = elem_id;
                index->max_level = index->levels_cpu[elem_id];
            }
        }
    }

    index->num_elements += n;
    printf("[GPU-HNSW] 插入 %d 个向量，当前总数: %d\n", n, index->num_elements);

    return n;
}

int32_t gpu_hnsw_insert_batch(gpu_hnsw_index_t *index, const float *vectors,
                              int32_t n, const int32_t *ids)
{
    return gpu_hnsw_insert(index, vectors, n, ids);
}

gpu_search_results_t *gpu_hnsw_search(gpu_hnsw_index_t *index,
                                      const float *query, int32_t k)
{
    gpu_search_results_t *results;

    if (index == NULL || query == NULL || k <= 0) {
        return NULL;
    }

    /* 分配结果集 */
    results = (gpu_search_results_t *)malloc(sizeof(gpu_search_results_t));
    if (results == NULL) {
        return NULL;
    }

    results->capacity = k;
    results->count = 0;
    results->items = (gpu_search_result_item_t *)malloc(k * sizeof(gpu_search_result_item_t));
    if (results->items == NULL) {
        free(results);
        return NULL;
    }

    /* CPU 搜索（存根） */
    /* TODO: 实现 GPU 并行搜索 */
    uint64_t start_time = 0;  /* TODO: 使用实际计时 */
    search_knn_cpu(index, query, k, results->items);
    results->count = (index->num_elements < k) ? index->num_elements : k;
    uint64_t end_time = 0;    /* TODO: 使用实际计时 */

    results->total_time_ms = (end_time - start_time) / 1000000.0f;  /* 纳秒转毫秒 */

    return results;
}

int32_t gpu_hnsw_search_batch(gpu_hnsw_index_t *index, const float *queries,
                              int32_t n_queries, int32_t k,
                              int32_t *ids, float *distances)
{
    int32_t success_count = 0;

    if (index == NULL || queries == NULL || n_queries <= 0 || k <= 0) {
        return -1;
    }

    /* 逐个查询（存根） */
    /* TODO: 实现真正的批量 GPU 并行搜索 */
    for (int32_t i = 0; i < n_queries; i++) {
        gpu_search_results_t *results = gpu_hnsw_search(
            index, queries + i * index->dim, k);

        if (results != NULL) {
            for (int32_t j = 0; j < results->count && j < k; j++) {
                if (ids != NULL) {
                    ids[i * k + j] = results->items[j].id;
                }
                if (distances != NULL) {
                    distances[i * k + j] = results->items[j].distance;
                }
            }
            gpu_free_results(results);
            success_count++;
        }
    }

    return success_count;
}

void gpu_hnsw_set_ef_search(gpu_hnsw_index_t *index, int32_t ef_search)
{
    if (index != NULL && ef_search > 0) {
        index->ef_search = ef_search;
    }
}

void gpu_hnsw_get_stats(gpu_hnsw_index_t *index, int32_t *num_vectors,
                        size_t *memory_usage)
{
    if (index == NULL) {
        return;
    }

    if (num_vectors != NULL) {
        *num_vectors = index->num_elements;
    }

    if (memory_usage != NULL) {
        *memory_usage = (size_t)index->max_elements * index->dim * sizeof(float);  /* 向量 */
        *memory_usage += (size_t)index->max_elements * index->M * 2 * 64 * sizeof(int32_t);  /* 邻居 */
        *memory_usage += index->max_elements * sizeof(int8_t);  /* 层数 */
    }
}

/* ========================================================================
 * 结果集管理
 * ======================================================================== */

void gpu_free_results(gpu_search_results_t *results)
{
    if (results == NULL) {
        return;
    }

    if (results->items != NULL) {
        free(results->items);
    }

    free(results);
}
