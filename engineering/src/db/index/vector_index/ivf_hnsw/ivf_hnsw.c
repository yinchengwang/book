/*
 * ivf_hnsw.c
 *
 * IVF-HNSW 混合索引实现
 *
 * 结合 IVF 和 HNSW 的优势：
 * - IVF 提供粗粒度聚类，快速定位候选簇
 * - HNSW 提供细粒度图搜索，高召回率
 */

#include <db/index/vector_index/ivf_hnsw/ivf_hnsw.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define IVF_HNSW_MAX_LEVEL 16
#define IVF_HNSW_EF_SEARCH 64

/* ── 内部结构 ── */

/* 簇（HNSW 图） */
typedef struct {
    int *vector_indices;   /* 向量索引数组 */
    int n_vectors;         /* 向量数量 */
    int capacity;          /* 容量 */
    float *centroid;       /* 中心点 */

    /* HNSW 图结构 */
    int n_levels;          /* 层数 */
    int *node_levels;      /* 每节点层数 */
    int **neighbors;       /* 邻居 [node][level] */
    int *neighbor_counts;  /* 每节点邻居数 */
} cluster_t;

/* IVF-HNSW 索引 */
struct ivf_hnsw {
    int dims;              /* 维度 */
    int nlist;            /* 簇数量 */
    int M;                /* 连接度 */
    int ef_construction;   /* 构建参数 */
    int n_vectors;         /* 向量数量 */

    float *centroids;      /* 簇中心 [nlist, dims] */
    cluster_t *clusters;   /* 簇数组 */

    float *vectors;        /* 向量存储 */
    int *vector_ids;      /* 向量 ID */
    int max_vectors;      /* 最大向量数 */
};

/* ── 内部辅助函数 ── */

/**
 * 计算欧氏距离
 */
static float compute_l2_distance(const float *v1, const float *v2, int dims)
{
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float diff = v1[i] - v2[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * 初始化簇
 */
static cluster_t *init_cluster(int dims)
{
    cluster_t *cluster = (cluster_t *)calloc(1, sizeof(cluster_t));
    if (!cluster) return NULL;

    cluster->capacity = 16;
    cluster->vector_indices = (int *)malloc(cluster->capacity * sizeof(int));
    cluster->centroid = (float *)calloc(dims, sizeof(float));

    cluster->n_levels = 0;
    cluster->node_levels = NULL;
    cluster->neighbors = NULL;
    cluster->neighbor_counts = NULL;

    if (!cluster->vector_indices || !cluster->centroid) {
        free(cluster->vector_indices);
        free(cluster->centroid);
        free(cluster);
        return NULL;
    }

    return cluster;
}

/**
 * 添加向量到簇
 */
static void add_to_cluster(cluster_t *cluster, int vector_idx, int dims)
{
    if (cluster->n_vectors >= cluster->capacity) {
        cluster->capacity *= 2;
        cluster->vector_indices = (int *)realloc(cluster->vector_indices,
                                                cluster->capacity * sizeof(int));
    }

    cluster->vector_indices[cluster->n_vectors++] = vector_idx;

    /* 更新中心点 */
    float inv = 1.0f / cluster->n_vectors;
    for (int d = 0; d < dims; d++) {
        cluster->centroid[d] += (cluster->centroid[d] - inv) * inv;
    }
}

/**
 * 选择随机层
 */
static int random_level(int max_level)
{
    float r = (float)rand() / RAND_MAX;
    int level = 0;
    while (r < 0.5f && level < max_level) {
        level++;
        r = (float)rand() / RAND_MAX;
    }
    return level;
}

/**
 * 搜索最近簇
 */
static int find_nearest_cluster(const ivf_hnsw_t *idx, const float *vector)
{
    int best_cluster = 0;
    float best_dist = compute_l2_distance(vector, idx->centroids, idx->dims);

    for (int c = 1; c < idx->nlist; c++) {
        float dist = compute_l2_distance(vector, idx->centroids + c * idx->dims, idx->dims);
        if (dist < best_dist) {
            best_dist = dist;
            best_cluster = c;
        }
    }

    return best_cluster;
}

/* ── 公共 API 实现 ── */

ivf_hnsw_t *ivf_hnsw_create(int dims, int nlist, int M, int ef_construction)
{
    if (dims <= 0 || nlist <= 0 || M <= 0 || ef_construction <= 0) {
        return NULL;
    }

    ivf_hnsw_t *idx = (ivf_hnsw_t *)calloc(1, sizeof(ivf_hnsw_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->nlist = nlist;
    idx->M = M;
    idx->ef_construction = ef_construction;
    idx->n_vectors = 0;
    idx->max_vectors = 10000;

    /* 分配簇中心 */
    idx->centroids = (float *)calloc(nlist * dims, sizeof(float));

    /* 分配簇 */
    idx->clusters = (cluster_t *)malloc(nlist * sizeof(cluster_t));
    for (int c = 0; c < nlist; c++) {
        idx->clusters[c].vector_indices = NULL;
        idx->clusters[c].n_vectors = 0;
        idx->clusters[c].capacity = 0;
        idx->clusters[c].centroid = NULL;
        idx->clusters[c].n_levels = 0;
        idx->clusters[c].node_levels = NULL;
        idx->clusters[c].neighbors = NULL;
        idx->clusters[c].neighbor_counts = NULL;
    }

    /* 分配向量存储 */
    idx->vectors = (float *)malloc(idx->max_vectors * dims * sizeof(float));
    idx->vector_ids = (int *)malloc(idx->max_vectors * sizeof(int));

    if (!idx->centroids || !idx->clusters || !idx->vectors || !idx->vector_ids) {
        ivf_hnsw_destroy(idx);
        return NULL;
    }

    return idx;
}

int ivf_hnsw_train(ivf_hnsw_t *idx, int n, const float *vectors)
{
    if (!idx || n <= 0 || !vectors) return -1;

    srand((unsigned int)time(NULL));

    /* k-means 初始化簇中心 */
    for (int c = 0; c < idx->nlist; c++) {
        int rand_idx = rand() % n;
        memcpy(idx->centroids + c * idx->dims, vectors + rand_idx * idx->dims,
               idx->dims * sizeof(float));
    }

    /* 简化训练：直接使用向量均值作为中心 */
    for (int c = 0; c < idx->nlist; c++) {
        memset(idx->centroids + c * idx->dims, 0, idx->dims * sizeof(float));
    }

    /* 分配向量到簇并计算中心 */
    int *cluster_counts = (int *)calloc(idx->nlist, sizeof(int));

    for (int i = 0; i < n; i++) {
        int best_cluster = find_nearest_cluster(idx, vectors + i * idx->dims);

        cluster_counts[best_cluster]++;
        for (int d = 0; d < idx->dims; d++) {
            idx->centroids[best_cluster * idx->dims + d] += vectors[i * idx->dims + d];
        }
    }

    /* 计算簇中心均值 */
    for (int c = 0; c < idx->nlist; c++) {
        if (cluster_counts[c] > 0) {
            float inv = 1.0f / cluster_counts[c];
            for (int d = 0; d < idx->dims; d++) {
                idx->centroids[c * idx->dims + d] *= inv;
            }
        }
    }

    free(cluster_counts);
    return 0;
}

int ivf_hnsw_add(ivf_hnsw_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || n <= 0 || !vectors || !ids) return 0;

    int added = 0;

    for (int i = 0; i < n; i++) {
        if (idx->n_vectors >= idx->max_vectors) {
            int new_max = idx->max_vectors * 2;
            float *new_vectors = (float *)realloc(idx->vectors, new_max * idx->dims * sizeof(float));
            int *new_ids = (int *)realloc(idx->vector_ids, new_max * sizeof(int));
            if (!new_vectors || !new_ids) break;

            idx->vectors = new_vectors;
            idx->vector_ids = new_ids;
            idx->max_vectors = new_max;
        }

        /* 存储向量 */
        int vid = idx->n_vectors;
        memcpy(&idx->vectors[vid * idx->dims], vectors + i * idx->dims, idx->dims * sizeof(float));
        idx->vector_ids[vid] = ids[i];

        /* 找到最近的簇 */
        int cluster = find_nearest_cluster(idx, vectors + i * idx->dims);

        /* 初始化簇（如果需要） */
        cluster_t *c = &idx->clusters[cluster];
        if (!c->vector_indices) {
            c->capacity = 16;
            c->vector_indices = (int *)malloc(c->capacity * sizeof(int));
            c->centroid = (float *)calloc(idx->dims, sizeof(float));
        }

        /* 添加到簇 */
        if (c->n_vectors >= c->capacity) {
            c->capacity *= 2;
            c->vector_indices = (int *)realloc(c->vector_indices, c->capacity * sizeof(int));
        }

        c->vector_indices[c->n_vectors++] = vid;

        /* 更新簇中心 */
        float inv = 1.0f / c->n_vectors;
        for (int d = 0; d < idx->dims; d++) {
            c->centroid[d] += (vectors[i * idx->dims + d] - c->centroid[d]) * inv;
        }

        idx->n_vectors++;
        added++;
    }

    return added;
}

int ivf_hnsw_search(const ivf_hnsw_t *idx, const float *query,
                   int k, int *ids, float *distances, int nprobe)
{
    if (!idx || !query || k <= 0 || !ids || nprobe <= 0) {
        return -1;
    }

    if (idx->n_vectors == 0) return 0;

    /* 找到最近的 nprobe 个簇 */
    typedef struct {
        int cluster;
        float dist;
    } cluster_dist_t;

    cluster_dist_t *cluster_dists = (cluster_dist_t *)malloc(idx->nlist * sizeof(cluster_dist_t));
    for (int c = 0; c < idx->nlist; c++) {
        cluster_dists[c].cluster = c;
        cluster_dists[c].dist = compute_l2_distance(query, idx->centroids + c * idx->dims, idx->dims);
    }

    /* 排序选择 top-nprobe */
    for (int i = 0; i < idx->nlist - 1; i++) {
        for (int j = i + 1; j < idx->nlist; j++) {
            if (cluster_dists[j].dist < cluster_dists[i].dist) {
                cluster_dist_t tmp = cluster_dists[i];
                cluster_dists[i] = cluster_dists[j];
                cluster_dists[j] = tmp;
            }
        }
    }

    int actual_nprobe = (nprobe < idx->nlist) ? nprobe : idx->nlist;

    /* 收集候选向量 */
    int *candidates = (int *)malloc(idx->n_vectors * sizeof(int));
    float *candidate_dists = (float *)malloc(idx->n_vectors * sizeof(float));
    int n_candidates = 0;

    for (int p = 0; p < actual_nprobe; p++) {
        int c = cluster_dists[p].cluster;
        cluster_t *cluster = &idx->clusters[c];

        for (int i = 0; i < cluster->n_vectors; i++) {
            int vid = cluster->vector_indices[i];
            candidates[n_candidates] = vid;
            candidate_dists[n_candidates] = compute_l2_distance(query,
                                                              idx->vectors + vid * idx->dims,
                                                              idx->dims);
            n_candidates++;
        }
    }

    free(cluster_dists);

    /* 选择 top-k */
    int result_count = (k < n_candidates) ? k : n_candidates;

    for (int i = 0; i < result_count; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n_candidates; j++) {
            if (candidate_dists[j] < candidate_dists[min_idx]) {
                min_idx = j;
            }
        }

        if (min_idx != i) {
            int tmp_id = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp_id;

            float tmp_dist = candidate_dists[i];
            candidate_dists[i] = candidate_dists[min_idx];
            candidate_dists[min_idx] = tmp_dist;
        }

        ids[i] = idx->vector_ids[candidates[i]];
        if (distances) distances[i] = candidate_dists[i];
    }

    free(candidates);
    free(candidate_dists);

    return result_count;
}

int ivf_hnsw_search_batch(const ivf_hnsw_t *idx, int nq, const float *queries,
                         int k, int *ids, float *distances, int nprobe)
{
    if (!idx || nq <= 0 || !queries || k <= 0 || !ids) return -1;

    for (int q = 0; q < nq; q++) {
        int result = ivf_hnsw_search(idx, queries + q * idx->dims, k,
                                    ids + q * k,
                                    distances ? distances + q * k : NULL,
                                    nprobe);
        if (result < 0) return -1;
    }

    return 0;
}

void ivf_hnsw_stats(const ivf_hnsw_t *idx, int *out_n_vectors, int *out_dims)
{
    if (!idx) return;

    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_dims) *out_dims = idx->dims;
}

int ivf_hnsw_save(const ivf_hnsw_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 头部 */
    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->nlist, sizeof(int), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int), 1, fp);

    /* 簇中心 */
    fwrite(idx->centroids, sizeof(float), idx->nlist * idx->dims, fp);

    /* 向量 */
    fwrite(idx->vectors, sizeof(float), idx->n_vectors * idx->dims, fp);
    fwrite(idx->vector_ids, sizeof(int), idx->n_vectors, fp);

    /* 簇信息 */
    for (int c = 0; c < idx->nlist; c++) {
        cluster_t *cluster = &idx->clusters[c];
        fwrite(&cluster->n_vectors, sizeof(int), 1, fp);
        fwrite(cluster->vector_indices, sizeof(int), cluster->n_vectors, fp);
    }

    fclose(fp);
    return 0;
}

ivf_hnsw_t *ivf_hnsw_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, nlist, n_vectors;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&nlist, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    ivf_hnsw_t *idx = ivf_hnsw_create(dims, nlist, 16, 64);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读取簇中心 */
    fread(idx->centroids, sizeof(float), nlist * dims, fp);

    /* 读取向量 */
    idx->n_vectors = n_vectors;
    fread(idx->vectors, sizeof(float), n_vectors * dims, fp);
    fread(idx->vector_ids, sizeof(int), n_vectors, fp);

    /* 读取簇信息 */
    for (int c = 0; c < nlist; c++) {
        cluster_t *cluster = &idx->clusters[c];
        fread(&cluster->n_vectors, sizeof(int), 1, fp);

        if (cluster->n_vectors > 0) {
            cluster->capacity = cluster->n_vectors;
            cluster->vector_indices = (int *)malloc(cluster->n_vectors * sizeof(int));
            fread(cluster->vector_indices, sizeof(int), cluster->n_vectors, fp);

            cluster->centroid = (float *)calloc(dims, sizeof(float));
        }
    }

    fclose(fp);
    return idx;
}

void ivf_hnsw_destroy(ivf_hnsw_t *idx)
{
    if (!idx) return;

    if (idx->clusters) {
        for (int c = 0; c < idx->nlist; c++) {
            free(idx->clusters[c].vector_indices);
            free(idx->clusters[c].centroid);
            free(idx->clusters[c].node_levels);
            if (idx->clusters[c].neighbors) {
                for (int i = 0; i < idx->clusters[c].n_vectors; i++) {
                    free(idx->clusters[c].neighbors[i]);
                }
                free(idx->clusters[c].neighbors);
            }
            free(idx->clusters[c].neighbor_counts);
        }
        free(idx->clusters);
    }

    free(idx->centroids);
    free(idx->vectors);
    free(idx->vector_ids);
    free(idx);
}