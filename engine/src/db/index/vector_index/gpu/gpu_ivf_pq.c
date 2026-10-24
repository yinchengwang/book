/**
 * @file gpu_ivf_pq.c
 * @brief GPU-IVF-PQ 索引实现
 *
 * GPU 加速的 IVF-PQ（倒排文件 + 产品量化）索引。
 * 结合聚类索引和 PQ 压缩以支持大规模向量搜索。
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
 * @brief GPU-IVF-PQ 索引内部结构
 */
struct gpu_ivf_pq_index_s {
    /* 配置 */
    int32_t dim;               /**< 向量维度 */
    int32_t nlist;             /**< IVF 聚类中心数量 */
    int32_t nprobe;            /**< 搜索探针数量 */
    int32_t pq_m;              /**< PQ 子空间数 */
    int32_t pq_nbits;          /**< PQ 每子空间位数 */
    int32_t max_elements;      /**< 最大元素数量 */
    int32_t metric;            /**< 度量类型 */

    /* 派生参数 */
    int32_t pq_ncentroids;     /**< 每子空间聚类数 (2^nbits) */
    int32_t pq_dim_per_sub;    /**< 每子空间维度数 */

    /* IVF 聚类中心 */
    float *centroids;          /**< IVF 聚类中心 [nlist][dim] */
    int32_t *centroid_counts;  /**< 每个中心的向量数量 */

    /* 倒排列表（存储向量 ID） */
    int32_t **inverted_lists;  /**< 倒排列表数组 */
    int32_t *list_sizes;       /**< 每个列表的当前大小 */
    int32_t *list_capacities;  /**< 每个列表的容量 */

    /* PQ 码书和编码 */
    float *pq_codebooks;       /**< PQ 码书 [pq_m][pq_ncentroids][pq_dim_per_sub] */
    uint8_t *pq_codes;         /**< PQ 编码 [max_elements][pq_m] */
    int32_t num_vectors;       /**< 当前向量数量 */

    /* GPU 内存（存根） */
    gpu_memory_t *centroids_mem;
    gpu_memory_t *codebooks_mem;
    gpu_memory_t *codes_mem;
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
 * @brief 查找最近的 IVF 聚类中心
 */
static int32_t find_nearest_centroid(gpu_ivf_pq_index_t *index, const float *vector)
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
 * @brief 训练 PQ 码书（K-Means 每个子空间）
 */
static void train_pq_codebooks(gpu_ivf_pq_index_t *index, const float *vectors, int32_t n)
{
    int32_t dim = index->dim;
    int32_t m = index->pq_m;
    int32_t ncentroids = index->pq_ncentroids;
    int32_t dim_per_sub = index->pq_dim_per_sub;

    printf("[GPU-IVF-PQ] 训练 PQ 码书: m=%d, ncentroids=%d, dim_per_sub=%d\n",
           m, ncentroids, dim_per_sub);

    /* 对每个子空间进行 K-Means 训练 */
    for (int32_t s = 0; s < m; s++) {
        /* 提取子空间向量 */
        float *sub_vectors = (float *)malloc(n * dim_per_sub * sizeof(float));
        float *centroids = index->pq_codebooks + s * ncentroids * dim_per_sub;

        for (int32_t i = 0; i < n; i++) {
            for (int32_t j = 0; j < dim_per_sub; j++) {
                int32_t src_idx = i * dim + s * dim_per_sub + j;
                sub_vectors[i * dim_per_sub + j] = vectors[src_idx];
            }
        }

        /* 简单 K-Means：随机初始化 + 一次迭代 */
        srand(s);  /* 不同子空间不同种子 */
        for (int32_t c = 0; c < ncentroids; c++) {
            int32_t sample_idx = rand() % n;
            memcpy(centroids + c * dim_per_sub,
                   sub_vectors + sample_idx * dim_per_sub,
                   dim_per_sub * sizeof(float));
        }

        /* TODO: 完整 K-Means 迭代 */

        free(sub_vectors);
    }
}

/**
 * @brief PQ 编码单个向量
 */
static void encode_pq_vector(gpu_ivf_pq_index_t *index, const float *vector, uint8_t *code)
{
    int32_t dim = index->dim;
    int32_t m = index->pq_m;
    int32_t ncentroids = index->pq_ncentroids;
    int32_t dim_per_sub = index->pq_dim_per_sub;

    for (int32_t s = 0; s < m; s++) {
        int32_t best_c = 0;
        float best_dist = -1.0f;

        const float *sub_vec = vector + s * dim_per_sub;
        const float *codebook = index->pq_codebooks + s * ncentroids * dim_per_sub;

        for (int32_t c = 0; c < ncentroids; c++) {
            float dist = compute_l2_distance(sub_vec, codebook + c * dim_per_sub, dim_per_sub);
            if (best_dist < 0 || dist < best_dist) {
                best_dist = dist;
                best_c = c;
            }
        }

        code[s] = (uint8_t)best_c;
    }
}

/* ========================================================================
 * GPU-IVF-PQ 索引 API 实现
 * ======================================================================== */

gpu_ivf_pq_index_t *gpu_ivf_pq_create(const gpu_ivf_pq_config_t *config)
{
    gpu_ivf_pq_index_t *index;

    if (config == NULL || config->dim <= 0 || config->nlist <= 0 ||
        config->pq_m <= 0 || config->pq_nbits <= 0) {
        return NULL;
    }

    /* 分配索引结构 */
    index = (gpu_ivf_pq_index_t *)calloc(1, sizeof(gpu_ivf_pq_index_t));
    if (index == NULL) {
        return NULL;
    }

    /* 复制配置 */
    index->dim = config->dim;
    index->nlist = config->nlist;
    index->nprobe = (config->nprobe > 0) ? config->nprobe : 10;
    index->pq_m = config->pq_m;
    index->pq_nbits = config->pq_nbits;
    index->max_elements = (config->max_elements > 0) ? config->max_elements : 1000000;
    index->metric = config->metric;

    /* 派生参数 */
    index->pq_ncentroids = 1 << config->pq_nbits;  /* 2^nbits */
    index->pq_dim_per_sub = config->dim / config->pq_m;

    /* 验证 PQ 参数 */
    if (index->pq_dim_per_sub * config->pq_m != config->dim) {
        printf("[GPU-IVF-PQ] 错误: dim (%d) 必须能被 pq_m (%d) 整除\n",
               config->dim, config->pq_m);
        free(index);
        return NULL;
    }

    /* 初始化状态 */
    index->num_vectors = 0;

    /* 分配 IVF 聚类中心内存 */
    index->centroids = (float *)calloc(index->nlist * index->dim, sizeof(float));
    index->centroid_counts = (int32_t *)calloc(index->nlist, sizeof(int32_t));

    /* 分配倒排列表 */
    index->inverted_lists = (int32_t **)malloc(index->nlist * sizeof(int32_t *));
    index->list_sizes = (int32_t *)calloc(index->nlist, sizeof(int32_t));
    index->list_capacities = (int32_t *)malloc(index->nlist * sizeof(int32_t));

    /* 分配 PQ 码书: [pq_m][ncentroids][dim_per_sub] */
    size_t codebook_size = (size_t)index->pq_m * index->pq_ncentroids * index->pq_dim_per_sub;
    index->pq_codebooks = (float *)calloc(codebook_size, sizeof(float));

    /* 分配 PQ 编码: [max_elements][pq_m] */
    index->pq_codes = (uint8_t *)malloc((size_t)index->max_elements * index->pq_m);

    if (index->centroids == NULL || index->centroid_counts == NULL ||
        index->inverted_lists == NULL || index->list_sizes == NULL ||
        index->list_capacities == NULL || index->pq_codebooks == NULL ||
        index->pq_codes == NULL) {
        /* 清理 */
        free(index->centroids);
        free(index->centroid_counts);
        free(index->inverted_lists);
        free(index->list_sizes);
        free(index->list_capacities);
        free(index->pq_codebooks);
        free(index->pq_codes);
        free(index);
        return NULL;
    }

    /* 初始化倒排列表 */
    size_t list_capacity = (index->max_elements / index->nlist) + 10;
    for (int32_t i = 0; i < index->nlist; i++) {
        index->inverted_lists[i] = (int32_t *)malloc(list_capacity * sizeof(int32_t));
        index->list_capacities[i] = (int32_t)list_capacity;
        index->list_sizes[i] = 0;
    }

    printf("[GPU-IVF-PQ] 创建索引: dim=%d, nlist=%d, pq_m=%d, nbits=%d, ncentroids=%d\n",
           index->dim, index->nlist, index->pq_m, index->pq_nbits, index->pq_ncentroids);

    return index;
}

void gpu_ivf_pq_destroy(gpu_ivf_pq_index_t *index)
{
    if (index == NULL) {
        return;
    }

    /* 释放 GPU 内存 */
    if (index->centroids_mem != NULL) gpu_free(index->centroids_mem);
    if (index->codebooks_mem != NULL) gpu_free(index->codebooks_mem);
    if (index->codes_mem != NULL) gpu_free(index->codes_mem);

    /* 释放倒排列表 */
    for (int32_t i = 0; i < index->nlist; i++) {
        free(index->inverted_lists[i]);
    }

    /* 释放 CPU 内存 */
    free(index->centroids);
    free(index->centroid_counts);
    free(index->inverted_lists);
    free(index->list_sizes);
    free(index->list_capacities);
    free(index->pq_codebooks);
    free(index->pq_codes);

    free(index);
    printf("[GPU-IVF-PQ] 销毁索引\n");
}

int32_t gpu_ivf_pq_train(gpu_ivf_pq_index_t *index, const float *vectors, int32_t n)
{
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    /* 简化实现：使用随机中心 */
    /* TODO: 完整 K-Means 训练 */
    srand(42);
    for (int32_t i = 0; i < index->nlist; i++) {
        int32_t sample_idx = rand() % n;
        memcpy(index->centroids + i * index->dim,
               vectors + sample_idx * index->dim,
               index->dim * sizeof(float));
    }

    /* 训练 PQ 码书 */
    train_pq_codebooks(index, vectors, n);

    printf("[GPU-IVF-PQ] 训练完成: %d 个向量\n", n);
    return 0;
}

int32_t gpu_ivf_pq_insert(gpu_ivf_pq_index_t *index, const float *vectors,
                          int32_t n, const int32_t *ids)
{
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    if (index->num_vectors + n > index->max_elements) {
        n = index->max_elements - index->num_vectors;
        if (n <= 0) return -2;
    }

    int32_t inserted = 0;

    for (int32_t i = 0; i < n; i++) {
        int32_t elem_id = (ids != NULL) ? ids[i] : index->num_vectors + i;

        if (elem_id >= 0 && elem_id < index->max_elements) {
            /* PQ 编码向量 */
            uint8_t *code = index->pq_codes + elem_id * index->pq_m;
            const float *vec = vectors + i * index->dim;
            encode_pq_vector(index, vec, code);

            /* 查找并加入倒排列表 */
            int32_t cluster = find_nearest_centroid(index, vec);

            if (index->list_sizes[cluster] >= index->list_capacities[cluster]) {
                size_t new_cap = index->list_capacities[cluster] * 2;
                int32_t *new_list = (int32_t *)realloc(
                    index->inverted_lists[cluster], new_cap * sizeof(int32_t));
                if (new_list != NULL) {
                    index->inverted_lists[cluster] = new_list;
                    index->list_capacities[cluster] = (int32_t)new_cap;
                }
            }

            if (index->list_sizes[cluster] < index->list_capacities[cluster]) {
                index->inverted_lists[cluster][index->list_sizes[cluster]] = elem_id;
                index->list_sizes[cluster]++;
                index->centroid_counts[cluster]++;
                index->num_vectors++;
                inserted++;
            }
        }
    }

    printf("[GPU-IVF-PQ] 插入 %d 个向量，总数: %d\n", inserted, index->num_vectors);
    return inserted;
}

/**
 * @brief 计算查询与 PQ 编码向量的非对称距离
 */
static float compute_pq_asymmetric_distance(gpu_ivf_pq_index_t *index,
                                            const float *query,
                                            const uint8_t *code,
                                            int32_t vec_id)
{
    int32_t m = index->pq_m;
    int32_t dim_per_sub = index->pq_dim_per_sub;
    float total_dist = 0.0f;

    /* 预计算查询的每个子空间向量 */
    float *query_subs = (float *)malloc(m * dim_per_sub * sizeof(float));
    for (int32_t s = 0; s < m; s++) {
        for (int32_t j = 0; j < dim_per_sub; j++) {
            query_subs[s * dim_per_sub + j] = query[s * dim_per_sub + j];
        }
    }

    /* 对每个子空间计算距离 */
    for (int32_t s = 0; s < m; s++) {
        int32_t centroid_id = code[s];
        const float *centroid = index->pq_codebooks +
            s * index->pq_ncentroids * dim_per_sub +
            centroid_id * dim_per_sub;
        const float *q_sub = query_subs + s * dim_per_sub;

        float sub_dist = compute_l2_distance(q_sub, centroid, dim_per_sub);
        total_dist += sub_dist;
    }

    free(query_subs);
    return total_dist;
}

gpu_search_results_t *gpu_ivf_pq_search(gpu_ivf_pq_index_t *index,
                                        const float *query, int32_t k)
{
    gpu_search_results_t *results;

    if (index == NULL || query == NULL || k <= 0) {
        return NULL;
    }

    results = (gpu_search_results_t *)malloc(sizeof(gpu_search_results_t));
    if (results == NULL) return NULL;

    results->capacity = k;
    results->count = 0;
    results->items = (gpu_search_result_item_t *)malloc(k * sizeof(gpu_search_result_item_t));
    if (results->items == NULL) {
        free(results);
        return NULL;
    }

    /* 找最近的 nprobe 个聚类 */
    typedef struct { int32_t cluster; float dist; } cluster_dist_t;
    cluster_dist_t *cluster_dists = (cluster_dist_t *)malloc(index->nlist * sizeof(cluster_dist_t));

    for (int32_t i = 0; i < index->nlist; i++) {
        cluster_dists[i].cluster = i;
        cluster_dists[i].dist = compute_l2_distance(
            query, index->centroids + i * index->dim, index->dim);
    }

    /* 选择 Top-nprobe */
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

    /* 收集候选向量并计算 PQ 距离 */
    typedef struct { float dist; int32_t id; } candidate_t;
    int32_t max_candidates = 0;
    for (int32_t i = 0; i < index->nprobe && i < index->nlist; i++) {
        max_candidates += index->list_sizes[cluster_dists[i].cluster];
    }

    candidate_t *candidates = (candidate_t *)malloc(max_candidates * sizeof(candidate_t));
    int32_t cand_idx = 0;

    for (int32_t i = 0; i < index->nprobe && i < index->nlist; i++) {
        int32_t cluster = cluster_dists[i].cluster;
        for (int32_t j = 0; j < index->list_sizes[cluster]; j++) {
            int32_t vec_id = index->inverted_lists[cluster][j];
            uint8_t *code = index->pq_codes + vec_id * index->pq_m;
            candidates[cand_idx].id = vec_id;
            candidates[cand_idx].dist = compute_pq_asymmetric_distance(index, query, code, vec_id);
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

int32_t gpu_ivf_pq_search_batch(gpu_ivf_pq_index_t *index, const float *queries,
                                int32_t n_queries, int32_t k,
                                int32_t *ids, float *distances)
{
    int32_t success_count = 0;

    if (index == NULL || queries == NULL || n_queries <= 0 || k <= 0) {
        return -1;
    }

    for (int32_t i = 0; i < n_queries; i++) {
        gpu_search_results_t *results = gpu_ivf_pq_search(
            index, queries + i * index->dim, k);

        if (results != NULL) {
            for (int32_t j = 0; j < results->count && j < k; j++) {
                if (ids != NULL) ids[i * k + j] = results->items[j].id;
                if (distances != NULL) distances[i * k + j] = results->items[j].distance;
            }
            gpu_free_results(results);
            success_count++;
        }
    }

    return success_count;
}

void gpu_ivf_pq_set_nprobe(gpu_ivf_pq_index_t *index, int32_t nprobe)
{
    if (index != NULL && nprobe > 0) {
        index->nprobe = (nprobe > index->nlist) ? index->nlist : nprobe;
    }
}
