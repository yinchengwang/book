/*
 * milvus_ivf.c
 *
 * Milvus IVF 风格索引实现
 *
 * 兼容 Milvus 的 IVF 接口设计：
 * - 优化的 k-means 聚类
 * - 倒排列表存储
 * - 高效的 nprobe 查询
 */

#include <db/index/vector_index/milvus_ivf/milvus_ivf.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部结构 ── */

/* 倒排列表条目 */
typedef struct {
    int *vector_indices;   /* 向量索引数组 */
    int n_vectors;        /* 向量数量 */
    int capacity;         /* 容量 */
} inverted_list_t;

/* Milvus IVF 索引 */
struct milvus_ivf {
    int dims;              /* 维度 */
    int nlist;           /* 簇数量 */
    int n_vectors;        /* 向量数量 */
    int trained;          /* 是否已训练 */

    float *centroids;    /* 簇中心 [nlist, dims] */
    inverted_list_t *inverted_lists;  /* 倒排列表 */

    float *vectors;       /* 向量存储 */
    int *vector_ids;     /* 向量 ID */
    int max_vectors;     /* 最大向量数 */
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
 * 找到最近的簇
 */
static int find_nearest_cluster(const milvus_ivf_t *idx, const float *vector)
{
    int best = 0;
    float best_dist = compute_l2_distance(vector, idx->centroids, idx->dims);

    for (int c = 1; c < idx->nlist; c++) {
        float dist = compute_l2_distance(vector, idx->centroids + c * idx->dims, idx->dims);
        if (dist < best_dist) {
            best_dist = dist;
            best = c;
        }
    }

    return best;
}

/* ── 公共 API 实现 ── */

milvus_ivf_t *milvus_ivf_create(int dims, int nlist)
{
    if (dims <= 0 || nlist <= 0) return NULL;

    milvus_ivf_t *idx = (milvus_ivf_t *)calloc(1, sizeof(milvus_ivf_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->nlist = nlist;
    idx->n_vectors = 0;
    idx->trained = 0;
    idx->max_vectors = 10000;

    /* 分配簇中心 */
    idx->centroids = (float *)calloc(nlist * dims, sizeof(float));

    /* 分配倒排列表 */
    idx->inverted_lists = (inverted_list_t *)calloc(nlist, sizeof(inverted_list_t));
    for (int c = 0; c < nlist; c++) {
        idx->inverted_lists[c].capacity = 16;
        idx->inverted_lists[c].vector_indices = (int *)malloc(16 * sizeof(int));
        idx->inverted_lists[c].n_vectors = 0;
    }

    /* 分配向量存储 */
    idx->vectors = (float *)malloc(idx->max_vectors * dims * sizeof(float));
    idx->vector_ids = (int *)malloc(idx->max_vectors * sizeof(int));

    if (!idx->centroids || !idx->vectors || !idx->vector_ids) {
        milvus_ivf_destroy(idx);
        return NULL;
    }

    return idx;
}

int milvus_ivf_train(milvus_ivf_t *idx, int n, const float *vectors)
{
    if (!idx || n <= 0 || !vectors) return -1;

    srand((unsigned int)time(NULL));

    /* k-means++ 初始化中心 */
    int *cluster_counts = (int *)calloc(idx->nlist, sizeof(int));

    /* 第一个中心：随机选择 */
    int first = rand() % n;
    memcpy(idx->centroids, vectors + first * idx->dims, idx->dims * sizeof(float));

    /* 剩余中心：按距离加权选择 */
    for (int c = 1; c < idx->nlist; c++) {
        float *best_centroid = NULL;
        float max_min_dist = -1.0f;

        /* 找到距离现有中心最远的点作为新中心 */
        for (int i = 0; i < n; i++) {
            float min_dist = FLT_MAX;
            for (int j = 0; j < c; j++) {
                float dist = compute_l2_distance(vectors + i * idx->dims,
                                              idx->centroids + j * idx->dims, idx->dims);
                if (dist < min_dist) min_dist = dist;
            }
            if (min_dist > max_min_dist) {
                max_min_dist = min_dist;
                best_centroid = vectors + i * idx->dims;
            }
        }

        if (best_centroid) {
            memcpy(idx->centroids + c * idx->dims, best_centroid, idx->dims * sizeof(float));
        }
    }

    /* k-means 迭代 */
    for (int iter = 0; iter < 20; iter++) {
        /* 清零计数 */
        memset(cluster_counts, 0, idx->nlist * sizeof(int));
        memset(idx->centroids, 0, idx->nlist * idx->dims * sizeof(float));

        /* 分配点并累加 */
        for (int i = 0; i < n; i++) {
            int cluster = find_nearest_cluster(idx, vectors + i * idx->dims);
            cluster_counts[cluster]++;

            for (int d = 0; d < idx->dims; d++) {
                idx->centroids[cluster * idx->dims + d] += vectors[i * idx->dims + d];
            }
        }

        /* 计算均值 */
        for (int c = 0; c < idx->nlist; c++) {
            if (cluster_counts[c] > 0) {
                float inv = 1.0f / cluster_counts[c];
                for (int d = 0; d < idx->dims; d++) {
                    idx->centroids[c * idx->dims + d] *= inv;
                }
            }
        }
    }

    free(cluster_counts);
    idx->trained = 1;
    return 0;
}

int milvus_ivf_add(milvus_ivf_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !idx->trained || n <= 0 || !vectors || !ids) return 0;

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

        /* 找到簇并添加到倒排列表 */
        int cluster = find_nearest_cluster(idx, vectors + i * idx->dims);
        inverted_list_t *list = &idx->inverted_lists[cluster];

        if (list->n_vectors >= list->capacity) {
            list->capacity *= 2;
            list->vector_indices = (int *)realloc(list->vector_indices, list->capacity * sizeof(int));
        }

        list->vector_indices[list->n_vectors++] = vid;
        idx->n_vectors++;
        added++;
    }

    return added;
}

int milvus_ivf_search(const milvus_ivf_t *idx, const float *query,
                     int k, int *ids, int nprobe)
{
    if (!idx || !idx->trained || !query || k <= 0 || !ids || nprobe <= 0) {
        return -1;
    }

    if (idx->n_vectors == 0) return 0;

    /* 找到最近的 nprobe 个簇 */
    typedef struct { int cluster; float dist; } cluster_dist_t;
    cluster_dist_t *cluster_dists = (cluster_dist_t *)malloc(idx->nlist * sizeof(cluster_dist_t));

    for (int c = 0; c < idx->nlist; c++) {
        cluster_dists[c].cluster = c;
        cluster_dists[c].dist = compute_l2_distance(query, idx->centroids + c * idx->dims, idx->dims);
    }

    /* 排序 */
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
    float *cand_dists = (float *)malloc(idx->n_vectors * sizeof(float));
    int n_candidates = 0;

    for (int p = 0; p < actual_nprobe; p++) {
        inverted_list_t *list = &idx->inverted_lists[cluster_dists[p].cluster];
        for (int i = 0; i < list->n_vectors; i++) {
            int vid = list->vector_indices[i];
            candidates[n_candidates] = vid;
            cand_dists[n_candidates] = compute_l2_distance(query, idx->vectors + vid * idx->dims, idx->dims);
            n_candidates++;
        }
    }

    free(cluster_dists);

    /* 选择 top-k */
    int result_count = (k < n_candidates) ? k : n_candidates;

    for (int i = 0; i < result_count; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n_candidates; j++) {
            if (cand_dists[j] < cand_dists[min_idx]) {
                min_idx = j;
            }
        }

        if (min_idx != i) {
            int tmp_id = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp_id;

            float tmp_dist = cand_dists[i];
            cand_dists[i] = cand_dists[min_idx];
            cand_dists[min_idx] = tmp_dist;
        }

        ids[i] = idx->vector_ids[candidates[i]];
    }

    free(candidates);
    free(cand_dists);

    return result_count;
}

int milvus_ivf_search_batch(const milvus_ivf_t *idx, int nq, const float *queries,
                           int k, int *ids, int nprobe)
{
    if (!idx || nq <= 0 || !queries || k <= 0 || !ids) return -1;

    for (int q = 0; q < nq; q++) {
        int result = milvus_ivf_search(idx, queries + q * idx->dims, k, ids + q * k, nprobe);
        if (result < 0) return -1;
    }

    return 0;
}

void milvus_ivf_stats(const milvus_ivf_t *idx, int *out_n_vectors, int *out_dims, int *out_nlist)
{
    if (!idx) return;

    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_dims) *out_dims = idx->dims;
    if (out_nlist) *out_nlist = idx->nlist;
}

int milvus_ivf_save(const milvus_ivf_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->nlist, sizeof(int), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int), 1, fp);
    fwrite(&idx->trained, sizeof(int), 1, fp);

    fwrite(idx->centroids, sizeof(float), idx->nlist * idx->dims, fp);
    fwrite(idx->vectors, sizeof(float), idx->n_vectors * idx->dims, fp);
    fwrite(idx->vector_ids, sizeof(int), idx->n_vectors, fp);

    for (int c = 0; c < idx->nlist; c++) {
        fwrite(&idx->inverted_lists[c].n_vectors, sizeof(int), 1, fp);
        fwrite(idx->inverted_lists[c].vector_indices, sizeof(int),
               idx->inverted_lists[c].n_vectors, fp);
    }

    fclose(fp);
    return 0;
}

milvus_ivf_t *milvus_ivf_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    int dims, nlist, n_vectors, trained;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&nlist, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1 ||
        fread(&trained, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    milvus_ivf_t *idx = milvus_ivf_create(dims, nlist);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    idx->trained = trained;
    idx->n_vectors = n_vectors;

    fread(idx->centroids, sizeof(float), nlist * dims, fp);
    fread(idx->vectors, sizeof(float), n_vectors * dims, fp);
    fread(idx->vector_ids, sizeof(int), n_vectors, fp);

    for (int c = 0; c < nlist; c++) {
        fread(&idx->inverted_lists[c].n_vectors, sizeof(int), 1, fp);
        fread(idx->inverted_lists[c].vector_indices, sizeof(int),
               idx->inverted_lists[c].n_vectors, fp);
    }

    fclose(fp);
    return idx;
}

void milvus_ivf_destroy(milvus_ivf_t *idx)
{
    if (!idx) return;

    if (idx->inverted_lists) {
        for (int c = 0; c < idx->nlist; c++) {
            free(idx->inverted_lists[c].vector_indices);
        }
        free(idx->inverted_lists);
    }

    free(idx->centroids);
    free(idx->vectors);
    free(idx->vector_ids);
    free(idx);
}