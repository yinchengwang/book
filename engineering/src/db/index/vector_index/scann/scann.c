/*
 * scann.c
 *
 * ScaNN (Scalable Nearest Neighbors) 实现
 *
 * 核心思想：
 * 1. 使用量化器压缩向量（减少内存）
 * 2. 构建图结构加速搜索
 * 3. 各向同性哈希优化内积距离
 *
 * 架构：
 * - 量化层：PQ/AQ 量化
 * - 图层：类似 HNSW 的图结构
 * - 重排层：精确距离计算
 *
 * 参考文献：
 * - "Accelerating Large-Scale Inference with Anisotropic Vector Quantization"
 *   Ruiqi Guo, Philip Sun, Erik Lindgren (ICML 2020)
 */

#include <db/index/vector_index/scann/scann.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define SCANN_MAX_CANDIDATES 1000
#define SCANN_MAX_VISITED 10000

/* ── 内部结构 ── */

/* 图节点 */
typedef struct scann_node {
    int id;                     /* 向量 ID */
    float *vector;              /* 原始向量 */
    int *neighbors;             /* 邻居节点 ID */
    int n_neighbors;            /* 邻居数量 */
    int max_neighbors;          /* 最大邻居数 */
} scann_node_t;

/* ScaNN 索引结构 */
struct scann {
    int dims;                   /* 向量维度 */
    int n_neighbors;            /* 图连接度 */
    int n_vectors;              /* 已添加向量数 */
    int max_vectors;            /* 最大向量数 */

    scann_node_t *nodes;        /* 图节点数组 */
    int *entry_points;          /* 入口点 */
    int n_entry_points;         /* 入口点数量 */

    float *vectors;             /* 向量存储 */
    int *vector_ids;            /* 向量 ID */
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
 * 选择贪心邻居
 */
static int select_greedy_neighbors(scann_t *idx, int node_id, const float *vector, int k)
{
    if (!idx->nodes || node_id >= idx->n_vectors) return 0;

    scann_node_t *node = &idx->nodes[node_id];

    /* 简化实现：选择距离最近的 k 个节点作为邻居 */
    int *candidates = (int *)malloc(idx->n_vectors * sizeof(int));
    float *distances = (float *)malloc(idx->n_vectors * sizeof(float));

    int n_candidates = 0;
    for (int i = 0; i < idx->n_vectors; i++) {
        if (i != node_id) {
            candidates[n_candidates] = i;
            distances[n_candidates] = compute_l2_distance(vector, idx->vectors + i * idx->dims, idx->dims);
            n_candidates++;
        }
    }

    /* 选择 top-k */
    for (int i = 0; i < k && i < n_candidates; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n_candidates; j++) {
            if (distances[j] < distances[min_idx]) {
                min_idx = j;
            }
        }

        if (min_idx != i) {
            int tmp_id = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp_id;

            float tmp_dist = distances[i];
            distances[i] = distances[min_idx];
            distances[min_idx] = tmp_dist;
        }

        if (node->n_neighbors < node->max_neighbors) {
            node->neighbors[node->n_neighbors++] = candidates[i];
        }
    }

    free(candidates);
    free(distances);

    return node->n_neighbors;
}

/* ── 公共 API 实现 ── */

scann_t *scann_create(int dims, int n_neighbors)
{
    if (dims <= 0 || n_neighbors <= 0) {
        return NULL;
    }

    scann_t *idx = (scann_t *)calloc(1, sizeof(scann_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->n_neighbors = n_neighbors;
    idx->n_vectors = 0;
    idx->max_vectors = 10000;

    idx->nodes = (scann_node_t *)calloc(idx->max_vectors, sizeof(scann_node_t));
    idx->vectors = (float *)malloc(idx->max_vectors * dims * sizeof(float));
    idx->vector_ids = (int *)malloc(idx->max_vectors * sizeof(int));

    if (!idx->nodes || !idx->vectors || !idx->vector_ids) {
        scann_destroy(idx);
        return NULL;
    }

    idx->entry_points = (int *)malloc(10 * sizeof(int));
    idx->n_entry_points = 0;

    return idx;
}

int scann_train(scann_t *idx, int n, const float *vectors)
{
    if (!idx || n <= 0 || !vectors) {
        return -1;
    }

    /* ScaNN 训练主要是学习量化器 */
    /* 简化实现：不使用量化器，直接使用原始向量 */
    return 0;
}

int scann_add(scann_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || n <= 0 || !vectors || !ids) {
        return 0;
    }

    int added = 0;

    for (int i = 0; i < n; i++) {
        if (idx->n_vectors >= idx->max_vectors) {
            /* 扩容 */
            int new_max = idx->max_vectors * 2;
            scann_node_t *new_nodes = (scann_node_t *)realloc(idx->nodes, new_max * sizeof(scann_node_t));
            float *new_vectors = (float *)realloc(idx->vectors, new_max * idx->dims * sizeof(float));
            int *new_ids = (int *)realloc(idx->vector_ids, new_max * sizeof(int));

            if (!new_nodes || !new_vectors || !new_ids) break;

            idx->nodes = new_nodes;
            idx->vectors = new_vectors;
            idx->vector_ids = new_ids;
            idx->max_vectors = new_max;
        }

        /* 存储向量 */
        int vid = idx->n_vectors;
        memcpy(&idx->vectors[vid * idx->dims], vectors + i * idx->dims, idx->dims * sizeof(float));
        idx->vector_ids[vid] = ids[i];

        /* 初始化节点 */
        scann_node_t *node = &idx->nodes[vid];
        node->id = ids[i];
        node->vector = &idx->vectors[vid * idx->dims];
        node->max_neighbors = idx->n_neighbors * 2;
        node->neighbors = (int *)malloc(node->max_neighbors * sizeof(int));
        node->n_neighbors = 0;

        idx->n_vectors++;
        added++;

        /* 添加入口点 */
        if (idx->n_entry_points < 10) {
            idx->entry_points[idx->n_entry_points++] = vid;
        }
    }

    /* 构建图连接 */
    for (int i = 0; i < idx->n_vectors; i++) {
        select_greedy_neighbors(idx, i, idx->vectors + i * idx->dims, idx->n_neighbors);
    }

    return added;
}

int scann_search(const scann_t *idx, const float *query,
                 int k, int *ids, float *distances)
{
    if (!idx || !query || k <= 0 || !ids) {
        return -1;
    }

    if (idx->n_vectors == 0) {
        return 0;
    }

    /* 简化实现：贪心搜索 */
    int *visited = (int *)calloc(idx->n_vectors, sizeof(int));
    float *min_dists = (float *)malloc(idx->n_vectors * sizeof(float));
    int *candidates = (int *)malloc(SCANN_MAX_CANDIDATES * sizeof(int));
    float *candidate_dists = (float *)malloc(SCANN_MAX_CANDIDATES * sizeof(float));

    for (int i = 0; i < idx->n_vectors; i++) {
        min_dists[i] = FLT_MAX;
    }

    int n_candidates = 0;

    /* 从入口点开始搜索 */
    for (int e = 0; e < idx->n_entry_points; e++) {
        int start = idx->entry_points[e];
        float dist = compute_l2_distance(query, idx->vectors + start * idx->dims, idx->dims);
        min_dists[start] = dist;

        if (n_candidates < SCANN_MAX_CANDIDATES) {
            candidates[n_candidates] = start;
            candidate_dists[n_candidates] = dist;
            n_candidates++;
        }
    }

    /* 贪心搜索 */
    for (int c = 0; c < n_candidates && c < SCANN_MAX_CANDIDATES; c++) {
        int current = candidates[c];

        if (visited[current]) continue;
        visited[current] = 1;

        /* 检查邻居 */
        scann_node_t *node = &idx->nodes[current];
        for (int n = 0; n < node->n_neighbors; n++) {
            int neighbor = node->neighbors[n];
            if (neighbor < 0 || neighbor >= idx->n_vectors) continue;

            float dist = compute_l2_distance(query, idx->vectors + neighbor * idx->dims, idx->dims);

            if (dist < min_dists[neighbor]) {
                min_dists[neighbor] = dist;

                if (n_candidates < SCANN_MAX_CANDIDATES) {
                    candidates[n_candidates] = neighbor;
                    candidate_dists[n_candidates] = dist;
                    n_candidates++;
                }
            }
        }
    }

    /* 选择 top-k */
    int result_count = 0;
    for (int i = 0; i < k && i < idx->n_vectors; i++) {
        int min_idx = -1;
        float min_dist = FLT_MAX;

        for (int j = 0; j < idx->n_vectors; j++) {
            if (!visited[j]) continue;
            if (min_dists[j] < min_dist) {
                min_dist = min_dists[j];
                min_idx = j;
            }
        }

        if (min_idx >= 0) {
            ids[result_count] = idx->vector_ids[min_idx];
            if (distances) distances[result_count] = min_dists[min_idx];
            visited[min_idx] = 0;  /* 标记为已选 */
            result_count++;
        }
    }

    free(visited);
    free(min_dists);
    free(candidates);
    free(candidate_dists);

    return result_count;
}

int scann_search_batch(const scann_t *idx, int nq, const float *queries,
                       int k, int *ids, float *distances)
{
    if (!idx || nq <= 0 || !queries || k <= 0 || !ids) {
        return -1;
    }

    for (int i = 0; i < nq; i++) {
        int result = scann_search(idx, queries + i * idx->dims, k,
                                   ids + i * k,
                                   distances ? distances + i * k : NULL);
        if (result < 0) {
            return -1;
        }
    }

    return 0;
}

void scann_stats(const scann_t *idx,
                 int *out_n_vectors, int *out_n_neighbors, int *out_dims)
{
    if (!idx) return;

    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_n_neighbors) *out_n_neighbors = idx->n_neighbors;
    if (out_dims) *out_dims = idx->dims;
}

int scann_save(const scann_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头部 */
    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->n_neighbors, sizeof(int), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int), 1, fp);

    /* 写入向量 */
    fwrite(idx->vectors, sizeof(float), idx->n_vectors * idx->dims, fp);
    fwrite(idx->vector_ids, sizeof(int), idx->n_vectors, fp);

    /* 写入图结构 */
    for (int i = 0; i < idx->n_vectors; i++) {
        fwrite(&idx->nodes[i].n_neighbors, sizeof(int), 1, fp);
        fwrite(idx->nodes[i].neighbors, sizeof(int), idx->nodes[i].n_neighbors, fp);
    }

    fclose(fp);
    return 0;
}

scann_t *scann_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, n_neighbors, n_vectors;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_neighbors, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    scann_t *idx = scann_create(dims, n_neighbors);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读取向量 */
    idx->n_vectors = n_vectors;
    fread(idx->vectors, sizeof(float), n_vectors * dims, fp);
    fread(idx->vector_ids, sizeof(int), n_vectors, fp);

    /* 重建图结构 */
    for (int i = 0; i < n_vectors; i++) {
        scann_node_t *node = &idx->nodes[i];
        node->id = idx->vector_ids[i];
        node->vector = &idx->vectors[i * dims];
        node->max_neighbors = n_neighbors * 2;
        node->neighbors = (int *)malloc(node->max_neighbors * sizeof(int));

        fread(&node->n_neighbors, sizeof(int), 1, fp);
        fread(node->neighbors, sizeof(int), node->n_neighbors, fp);
    }

    fclose(fp);
    return idx;
}

void scann_destroy(scann_t *idx)
{
    if (!idx) return;

    if (idx->nodes) {
        for (int i = 0; i < idx->n_vectors; i++) {
            free(idx->nodes[i].neighbors);
        }
        free(idx->nodes);
    }

    free(idx->vectors);
    free(idx->vector_ids);
    free(idx->entry_points);
    free(idx);
}