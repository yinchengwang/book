/**
 * @file gpu_ivf.c
 * @brief GPU-IVF 索引实现
 *
 * GPU 加速的 IVF（Inverted File）倒排索引。
 * 包含聚类中心训练、向量插入和搜索功能。
 */
#include "gpu_vector_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 内部类型定义
 * ======================================================================== */

/**
 * @brief GPU-IVF 索引内部结构
 */
struct gpu_ivf_index_s {
    /* 配置 */
    int32_t dim;               /**< 向量维度 */
    int32_t nlist;             /**< 聚类中心数量 */
    int32_t nprobe;            /**< 搜索探针数量 */
    int32_t max_elements;      /**< 最大元素数量 */
    int32_t metric;            /**< 度量类型 */

    /* 聚类中心 */
    float *centroids;          /**< 聚类中心向量 [nlist][dim] */
    int32_t *centroid_counts;  /**< 每个中心的向量数量 */

    /* 倒排列表 */
    int32_t **inverted_lists;  /**< 倒排列表数组 */
    int32_t *list_sizes;       /**< 每个列表的当前大小 */
    int32_t *list_capacities;  /**< 每个列表的容量 */

    /* 向量存储 */
    float *vectors;            /**< 所有向量数据 [max_elements][dim] */
    int32_t num_vectors;       /**< 当前向量数量 */

    /* GPU 内存（存根） */
    gpu_memory_t *centroids_mem;   /**< 聚类中心 GPU 内存 */
    gpu_memory_t *vectors_mem;     /**< 向量数据 GPU 内存 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 计算 L2 距离
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
 * @brief 计算内积
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
 * @brief 查找最近的聚类中心
 */
static int32_t find_nearest_centroid(gpu_ivf_index_t *index, const float *vector)
{
    int32_t best_cluster = 0;
    float best_dist;

    if (index->metric == METRIC_L2 || index->metric == 0) {
        best_dist = compute_l2_distance(vector, index->centroids, index->dim);
        for (int32_t i = 1; i < index->nlist; i++) {
            float dist = compute_l2_distance(vector, index->centroids + i * index->dim, index->dim);
            if (dist < best_dist) {
                best_dist = dist;
                best_cluster = i;
            }
        }
    } else {
        best_dist = -compute_inner_product(vector, index->centroids, index->dim);
        for (int32_t i = 1; i < index->nlist; i++) {
            float dist = -compute_inner_product(vector, index->centroids + i * index->dim, index->dim);
            if (dist < best_dist) {
                best_dist = dist;
                best_cluster = i;
            }
        }
    }

    return best_cluster;
}

/**
 * @brief K-Means 聚类训练（简化版）
 */
static void kmeans_train(gpu_ivf_index_t *index, const float *vectors, int32_t n)
{
    int32_t dim = index->dim;
    int32_t nlist = index->nlist;

    /* 初始化：随机选择 nlist 个向量作为初始中心 */
    srand(42);  /* 固定种子以保证可复现性 */
    for (int32_t i = 0; i < nlist; i++) {
        int32_t sample_idx = rand() % n;
        memcpy(index->centroids + i * dim, vectors + sample_idx * dim, dim * sizeof(float));
    }

    /* 迭代优化（简化版：只做 10 次迭代） */
    float *assignments = (float *)malloc(n * sizeof(float));
    float *new_centroids = (float *)calloc(nlist * dim, sizeof(float));
    int32_t *counts = (int32_t *)calloc(nlist, sizeof(int32_t));

    for (int iter = 0; iter < 10; iter++) {
        /* 清零计数和新中心 */
        memset(counts, 0, nlist * sizeof(int32_t));
        memset(new_centroids, 0, nlist * dim * sizeof(float));

        /* 分配每个向量到最近的中心 */
        for (int32_t i = 0; i < n; i++) {
            int32_t cluster = find_nearest_centroid(index, vectors + i * dim);
            assignments[i] = (float)cluster;
            counts[cluster]++;

            /* 累加到新中心 */
            for (int32_t j = 0; j < dim; j++) {
                new_centroids[cluster * dim + j] += vectors[i * dim + j];
            }
        }

        /* 计算新中心 */
        for (int32_t i = 0; i < nlist; i++) {
            if (counts[i] > 0) {
                for (int32_t j = 0; j < dim; j++) {
                    index->centroids[i * dim + j] = new_centroids[i * dim + j] / counts[i];
                }
            }
        }
    }

    free(assignments);
    free(new_centroids);
    free(counts);

    printf("[GPU-IVF] K-Means 训练完成: nlist=%d, 迭代次数=10\n", nlist);
}

/* ========================================================================
 * GPU-IVF 索引 API 实现
 * ======================================================================== */

gpu_ivf_index_t *gpu_ivf_create(const gpu_ivf_config_t *config)
{
    gpu_ivf_index_t *index;

    if (config == NULL || config->dim <= 0 || config->nlist <= 0) {
        return NULL;
    }

    /* 分配索引结构 */
    index = (gpu_ivf_index_t *)calloc(1, sizeof(gpu_ivf_index_t));
    if (index == NULL) {
        return NULL;
    }

    /* 复制配置 */
    index->dim = config->dim;
    index->nlist = config->nlist;
    index->nprobe = (config->nprobe > 0) ? config->nprobe : 10;
    index->max_elements = (config->max_elements > 0) ? config->max_elements : 1000000;
    index->metric = config->metric;

    /* 初始化状态 */
    index->num_vectors = 0;

    /* 分配聚类中心内存 */
    index->centroids = (float *)calloc(index->nlist * index->dim, sizeof(float));
    if (index->centroids == NULL) {
        free(index);
        return NULL;
    }

    index->centroid_counts = (int32_t *)calloc(index->nlist, sizeof(int32_t));
    if (index->centroid_counts == NULL) {
        free(index->centroids);
        free(index);
        return NULL;
    }

    /* 分配倒排列表内存 */
    index->inverted_lists = (int32_t **)malloc(index->nlist * sizeof(int32_t *));
    index->list_sizes = (int32_t *)calloc(index->nlist, sizeof(int32_t));
    index->list_capacities = (int32_t *)malloc(index->nlist * sizeof(int32_t));

    if (index->inverted_lists == NULL || index->list_sizes == NULL ||
        index->list_capacities == NULL) {
        free(index->centroids);
        free(index->centroid_counts);
        free(index->inverted_lists);
        free(index->list_sizes);
        free(index->list_capacities);
        free(index);
        return NULL;
    }

    /* 初始化每个倒排列表 */
    size_t list_capacity = (index->max_elements / index->nlist) + 10;
    for (int32_t i = 0; i < index->nlist; i++) {
        index->inverted_lists[i] = (int32_t *)malloc(list_capacity * sizeof(int32_t));
        index->list_capacities[i] = (int32_t)list_capacity;
        index->list_sizes[i] = 0;
    }

    /* 分配向量存储内存 */
    index->vectors = (float *)calloc((size_t)index->max_elements * index->dim, sizeof(float));
    if (index->vectors == NULL) {
        for (int32_t i = 0; i < index->nlist; i++) {
            free(index->inverted_lists[i]);
        }
        free(index->centroids);
        free(index->centroid_counts);
        free(index->inverted_lists);
        free(index->list_sizes);
        free(index->list_capacities);
        free(index);
        return NULL;
    }

    /* TODO: 分配 GPU 内存 */

    printf("[GPU-IVF] 创建索引: dim=%d, nlist=%d, nprobe=%d, max=%d\n",
           index->dim, index->nlist, index->nprobe, index->max_elements);

    return index;
}

void gpu_ivf_destroy(gpu_ivf_index_t *index)
{
    if (index == NULL) {
        return;
    }

    /* 释放 GPU 内存 */
    if (index->centroids_mem != NULL) {
        gpu_free(index->centroids_mem);
    }
    if (index->vectors_mem != NULL) {
        gpu_free(index->vectors_mem);
    }

    /* 释放倒排列表 */
    for (int32_t i = 0; i < index->nlist; i++) {
        free(index->inverted_lists[i]);
    }

    /* 释放 CPU 内存 */
    free(index->vectors);
    free(index->centroids);
    free(index->centroid_counts);
    free(index->inverted_lists);
    free(index->list_sizes);
    free(index->list_capacities);

    free(index);
    printf("[GPU-IVF] 销毁索引\n");
}

int32_t gpu_ivf_train(gpu_ivf_index_t *index, const float *vectors, int32_t n)
{
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    /* 使用 K-Means 训练聚类中心 */
    kmeans_train(index, vectors, n);

    return 0;
}

int32_t gpu_ivf_insert(gpu_ivf_index_t *index, const float *vectors,
                       int32_t n, const int32_t *ids)
{
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    if (index->num_vectors + n > index->max_elements) {
        n = index->max_elements - index->num_vectors;
        if (n <= 0) {
            return -2;  /* 索引已满 */
        }
    }

    int32_t inserted = 0;

    for (int32_t i = 0; i < n; i++) {
        int32_t elem_id = (ids != NULL) ? ids[i] : index->num_vectors + i;

        if (elem_id >= 0 && elem_id < index->max_elements) {
            /* 复制向量数据 */
            float *dest = index->vectors + elem_id * index->dim;
            const float *src = vectors + i * index->dim;
            memcpy(dest, src, index->dim * sizeof(float));

            /* 查找所属聚类中心并加入倒排列表 */
            int32_t cluster = find_nearest_centroid(index, src);

            /* 扩展列表容量（如果需要） */
            if (index->list_sizes[cluster] >= index->list_capacities[cluster]) {
                size_t new_cap = index->list_capacities[cluster] * 2;
                int32_t *new_list = (int32_t *)realloc(
                    index->inverted_lists[cluster], new_cap * sizeof(int32_t));
                if (new_list != NULL) {
                    index->inverted_lists[cluster] = new_list;
                    index->list_capacities[cluster] = (int32_t)new_cap;
                }
            }

            /* 添加到倒排列表 */
            if (index->list_sizes[cluster] < index->list_capacities[cluster]) {
                index->inverted_lists[cluster][index->list_sizes[cluster]] = elem_id;
                index->list_sizes[cluster]++;
                index->centroid_counts[cluster]++;
                index->num_vectors++;
                inserted++;
            }
        }
    }

    printf("[GPU-IVF] 插入 %d 个向量，总数: %d\n", inserted, index->num_vectors);
    return inserted;
}

gpu_search_results_t *gpu_ivf_search(gpu_ivf_index_t *index,
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

    /* 计算查询向量到所有聚类中心的距离，找出最近的 nprobe 个 */
    typedef struct {
        int32_t cluster;
        float dist;
    } cluster_dist_t;

    cluster_dist_t *cluster_dists = (cluster_dist_t *)malloc(
        index->nlist * sizeof(cluster_dist_t));

    for (int32_t i = 0; i < index->nlist; i++) {
        cluster_dists[i].cluster = i;
        if (index->metric == METRIC_L2 || index->metric == 0) {
            cluster_dists[i].dist = compute_l2_distance(
                query, index->centroids + i * index->dim, index->dim);
        } else {
            cluster_dists[i].dist = -compute_inner_product(
                query, index->centroids + i * index->dim, index->dim);
        }
    }

    /* 简单选择排序获取最近的 nprobe 个 */
    for (int32_t i = 0; i < index->nprobe && i < index->nlist; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < index->nlist; j++) {
            if (cluster_dists[j].dist < cluster_dists[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            cluster_dist_t tmp = cluster_dists[i];
            cluster_dists[i] = cluster_dists[min_idx];
            cluster_dists[min_idx] = tmp;
        }
    }

    /* 从选中的聚类中搜索最近的 k 个向量 */
    typedef struct {
        float dist;
        int32_t id;
    } candidate_t;

    int32_t total_candidates = 0;
    for (int32_t i = 0; i < index->nprobe && i < index->nlist; i++) {
        total_candidates += index->list_sizes[cluster_dists[i].cluster];
    }

    candidate_t *candidates = (candidate_t *)malloc(total_candidates * sizeof(candidate_t));
    int32_t cand_idx = 0;

    for (int32_t i = 0; i < index->nprobe && i < index->nlist; i++) {
        int32_t cluster = cluster_dists[i].cluster;
        for (int32_t j = 0; j < index->list_sizes[cluster]; j++) {
            int32_t vec_id = index->inverted_lists[cluster][j];
            candidates[cand_idx].id = vec_id;

            if (index->metric == METRIC_L2 || index->metric == 0) {
                candidates[cand_idx].dist = compute_l2_distance(
                    query, index->vectors + vec_id * index->dim, index->dim);
            } else {
                candidates[cand_idx].dist = -compute_inner_product(
                    query, index->vectors + vec_id * index->dim, index->dim);
            }
            cand_idx++;
        }
    }

    /* 选择 Top-K */
    for (int32_t i = 0; i < k && i < cand_idx; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < cand_idx; j++) {
            if (candidates[j].dist < candidates[min_idx].dist) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            candidate_t tmp = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp;
        }
        results->items[i].id = candidates[i].id;
        results->items[i].distance = candidates[i].dist;
        results->count++;
    }

    free(candidates);
    free(cluster_dists);

    return results;
}

int32_t gpu_ivf_search_batch(gpu_ivf_index_t *index, const float *queries,
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
        gpu_search_results_t *results = gpu_ivf_search(
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

void gpu_ivf_set_nprobe(gpu_ivf_index_t *index, int32_t nprobe)
{
    if (index != NULL && nprobe > 0) {
        index->nprobe = (nprobe > index->nlist) ? index->nlist : nprobe;
    }
}
