/*
 * hnsw_sq.c
 *
 * HNSW-SQ 混合索引实现
 *
 * 结合 HNSW 图搜索和 SQ 标量量化的优势：
 * - SQ 提供高压缩比（4x）
 * - HNSW 提供高效的近似搜索
 * - 重排序恢复精度
 */

#include <db/index/vector_index/hnsw_sq/hnsw_sq.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define HNSW_SQ_MAX_LEVEL 16
#define HNSW_SQ_EF_SEARCH 64

/* ── 内部结构 ── */

/* SQ 量化器（简化版：每个维度独立量化） */
typedef struct {
    float *mins;      /* 每维度最小值 */
    float *maxs;      /* 每维度最大值 */
    float scale;      /* 缩放因子 */
    int trained;
} sq_quantizer_t;

/* HNSW 节点 */
typedef struct hnsw_sq_node {
    int id;               /* 向量 ID */
    int level;           /* 节点所在最高层 */
    float *vector;       /* 原始向量（用于重排序） */

    /* SQ 编码向量 */
    uint8_t *sq_code;    /* SQ 编码 [dims] */

    /* 图连接 */
    int *neighbors;      /* 邻居数组（每层） */
    int *neighbor_counts; /* 每层邻居数 */
} hnsw_sq_node_t;

/* HNSW-SQ 索引结构 */
struct hnsw_sq {
    int dims;             /* 维度 */
    int M;               /* 连接度 */
    int ef_construction;  /* 构建候选集大小 */
    int n_vectors;        /* 向量数量 */
    int max_level;        /* 最大层数 */

    sq_quantizer_t sq;   /* SQ 量化器 */

    hnsw_sq_node_t **nodes;  /* 节点数组 */
    int *level_0_entry;     /* 每层入口点 */

    float *vectors;       /* 向量存储 */
    int max_vectors;      /* 最大向量数 */
};

/* ── 内部辅助函数 ── */

/**
 * SQ 量化：编码
 */
static void sq_encode(sq_quantizer_t *sq, const float *vector, uint8_t *code, int dims)
{
    for (int d = 0; d < dims; d++) {
        float val = vector[d];
        float normalized = (val - sq->mins[d]) / (sq->maxs[d] - sq->mins[d] + 1e-8f);
        normalized = (normalized < 0) ? 0 : (normalized > 1 ? 1 : normalized);
        code[d] = (uint8_t)(normalized * 255.0f);
    }
}

/**
 * 计算 SQ 距离（近似）
 */
static float compute_sq_distance(sq_quantizer_t *sq, const uint8_t *code1, const uint8_t *code2, int dims)
{
    float dist = 0.0f;
    for (int d = 0; d < dims; d++) {
        float v1 = sq->mins[d] + (code1[d] / 255.0f) * (sq->maxs[d] - sq->mins[d]);
        float v2 = sq->mins[d] + (code2[d] / 255.0f) * (sq->maxs[d] - sq->mins[d]);
        float diff = v1 - v2;
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * 计算精确距离
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

/* ── 公共 API 实现 ── */

hnsw_sq_t *hnsw_sq_create(int dims, int M, int ef_construction)
{
    if (dims <= 0 || M <= 0 || ef_construction <= 0) {
        return NULL;
    }

    hnsw_sq_t *idx = (hnsw_sq_t *)calloc(1, sizeof(hnsw_sq_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->M = M;
    idx->ef_construction = ef_construction;
    idx->n_vectors = 0;
    idx->max_level = HNSW_SQ_MAX_LEVEL;
    idx->max_vectors = 10000;

    /* 初始化 SQ 量化器 */
    idx->sq.mins = (float *)calloc(dims, sizeof(float));
    idx->sq.maxs = (float *)malloc(dims * sizeof(float));
    idx->sq.trained = 0;
    for (int d = 0; d < dims; d++) {
        idx->sq.maxs[d] = 1.0f;
    }

    /* 分配节点数组 */
    idx->nodes = (hnsw_sq_node_t **)calloc(idx->max_vectors, sizeof(hnsw_sq_node_t *));

    /* 分配向量存储 */
    idx->vectors = (float *)malloc(idx->max_vectors * dims * sizeof(float));

    /* 入口点 */
    idx->level_0_entry = (int *)malloc(HNSW_SQ_MAX_LEVEL * sizeof(int));
    for (int l = 0; l < HNSW_SQ_MAX_LEVEL; l++) {
        idx->level_0_entry[l] = -1;
    }

    if (!idx->sq.mins || !idx->sq.maxs || !idx->nodes || !idx->vectors || !idx->level_0_entry) {
        hnsw_sq_destroy(idx);
        return NULL;
    }

    return idx;
}

int hnsw_sq_train(hnsw_sq_t *idx, int n, const float *vectors)
{
    if (!idx || n <= 0 || !vectors) return -1;

    /* 计算每维度的 min/max */
    for (int d = 0; d < idx->dims; d++) {
        idx->sq.mins[d] = vectors[d];
        idx->sq.maxs[d] = vectors[d];

        for (int i = 1; i < n; i++) {
            float val = vectors[i * idx->dims + d];
            if (val < idx->sq.mins[d]) idx->sq.mins[d] = val;
            if (val > idx->sq.maxs[d]) idx->sq.maxs[d] = val;
        }
    }

    idx->sq.trained = 1;
    return 0;
}

int hnsw_sq_add(hnsw_sq_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !idx->sq.trained || n <= 0 || !vectors || !ids) {
        return 0;
    }

    int added = 0;

    for (int i = 0; i < n; i++) {
        if (idx->n_vectors >= idx->max_vectors) {
            /* 扩容 */
            int new_max = idx->max_vectors * 2;
            hnsw_sq_node_t **new_nodes = (hnsw_sq_node_t **)realloc(idx->nodes, new_max * sizeof(hnsw_sq_node_t *));
            float *new_vectors = (float *)realloc(idx->vectors, new_max * idx->dims * sizeof(float));

            if (!new_nodes || !new_vectors) break;

            idx->nodes = new_nodes;
            idx->vectors = new_vectors;
            idx->max_vectors = new_max;
        }

        /* 存储向量 */
        int vid = idx->n_vectors;
        memcpy(&idx->vectors[vid * idx->dims], vectors + i * idx->dims, idx->dims * sizeof(float));

        /* 创建节点 */
        hnsw_sq_node_t *node = (hnsw_sq_node_t *)calloc(1, sizeof(hnsw_sq_node_t));
        if (!node) continue;

        node->id = ids[i];
        node->level = random_level(idx->max_level - 1);
        node->vector = &idx->vectors[vid * idx->dims];
        node->sq_code = (uint8_t *)malloc(idx->dims * sizeof(uint8_t));

        /* SQ 编码 */
        sq_encode(&idx->sq, vectors + i * idx->dims, node->sq_code, idx->dims);

        /* 邻居初始化 */
        node->neighbor_counts = (int *)calloc(node->level + 1, sizeof(int));
        node->neighbors = (int *)calloc((node->level + 1) * idx->M, sizeof(int));

        /* 简化的图构建：只连接到最近的已有节点 */
        if (idx->n_vectors > 0 && node->level == 0) {
            int best = 0;
            float best_dist = compute_sq_distance(&idx->sq, node->sq_code,
                                                  idx->nodes[0]->sq_code, idx->dims);

            for (int j = 1; j < idx->n_vectors; j++) {
                float dist = compute_sq_distance(&idx->sq, node->sq_code,
                                               idx->nodes[j]->sq_code, idx->dims);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = j;
                }
            }

            node->neighbors[0] = best;
            node->neighbor_counts[0] = 1;
        }

        idx->nodes[vid] = node;
        idx->n_vectors++;
        added++;
    }

    return added;
}

int hnsw_sq_search(const hnsw_sq_t *idx, const float *query,
                  int k, int *ids, float *distances)
{
    if (!idx || !idx->sq.trained || !query || k <= 0 || !ids) {
        return -1;
    }

    if (idx->n_vectors == 0) return 0;

    /* 编码查询向量 */
    uint8_t *query_code = (uint8_t *)malloc(idx->dims * sizeof(uint8_t));
    sq_encode((sq_quantizer_t *)&idx->sq, query, query_code, idx->dims);

    /* 收集候选集（使用 SQ 距离） */
    int *candidates = (int *)malloc(idx->n_vectors * sizeof(int));
    float *sq_distances = (float *)malloc(idx->n_vectors * sizeof(float));

    for (int i = 0; i < idx->n_vectors; i++) {
        candidates[i] = i;
        sq_distances[i] = compute_sq_distance(&idx->sq, query_code,
                                              idx->nodes[i]->sq_code, idx->dims);
    }

    /* 简单选择 top-k */
    for (int i = 0; i < k && i < idx->n_vectors; i++) {
        int min_idx = i;
        for (int j = i + 1; j < idx->n_vectors; j++) {
            if (sq_distances[j] < sq_distances[min_idx]) {
                min_idx = j;
            }
        }

        if (min_idx != i) {
            int tmp_id = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp_id;

            float tmp_dist = sq_distances[i];
            sq_distances[i] = sq_distances[min_idx];
            sq_distances[min_idx] = tmp_dist;
        }
    }

    /* 使用精确距离重排序（重排序） */
    int result_count = (k < idx->n_vectors) ? k : idx->n_vectors;
    for (int i = 0; i < result_count; i++) {
        /* 重新计算精确距离 */
        float exact_dist = compute_l2_distance(query, idx->nodes[candidates[i]]->vector, idx->dims);
        ids[i] = idx->nodes[candidates[i]]->id;
        if (distances) distances[i] = exact_dist;
    }

    /* 最终排序 */
    for (int i = 0; i < result_count - 1; i++) {
        for (int j = i + 1; j < result_count; j++) {
            if (distances[j] < distances[i]) {
                int tmp_id = ids[i];
                ids[i] = ids[j];
                ids[j] = tmp_id;

                float tmp_dist = distances[i];
                distances[i] = distances[j];
                distances[j] = tmp_dist;
            }
        }
    }

    free(query_code);
    free(candidates);
    free(sq_distances);

    return result_count;
}

void hnsw_sq_stats(const hnsw_sq_t *idx, int *out_n_vectors, int *out_dims)
{
    if (!idx) return;

    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_dims) *out_dims = idx->dims;
}

int hnsw_sq_save(const hnsw_sq_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 头部 */
    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->M, sizeof(int), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int), 1, fp);

    /* SQ 量化器 */
    fwrite(idx->sq.mins, sizeof(float), idx->dims, fp);
    fwrite(idx->sq.maxs, sizeof(float), idx->dims, fp);

    /* 向量 */
    fwrite(idx->vectors, sizeof(float), idx->n_vectors * idx->dims, fp);

    /* 节点信息 */
    for (int i = 0; i < idx->n_vectors; i++) {
        fwrite(&idx->nodes[i]->id, sizeof(int), 1, fp);
        fwrite(&idx->nodes[i]->level, sizeof(int), 1, fp);
        fwrite(idx->nodes[i]->sq_code, sizeof(uint8_t), idx->dims, fp);
    }

    fclose(fp);
    return 0;
}

hnsw_sq_t *hnsw_sq_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, M, n_vectors;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&M, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    hnsw_sq_t *idx = hnsw_sq_create(dims, M, 64);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读取 SQ 量化器 */
    fread(idx->sq.mins, sizeof(float), dims, fp);
    fread(idx->sq.maxs, sizeof(float), dims, fp);
    idx->sq.trained = 1;

    /* 读取向量 */
    idx->n_vectors = n_vectors;
    fread(idx->vectors, sizeof(float), n_vectors * dims, fp);

    /* 读取节点 */
    for (int i = 0; i < n_vectors; i++) {
        hnsw_sq_node_t *node = (hnsw_sq_node_t *)calloc(1, sizeof(hnsw_sq_node_t));
        node->vector = &idx->vectors[i * dims];
        node->sq_code = (uint8_t *)malloc(dims * sizeof(uint8_t));

        fread(&node->id, sizeof(int), 1, fp);
        fread(&node->level, sizeof(int), 1, fp);
        fread(node->sq_code, sizeof(uint8_t), dims, fp);

        node->neighbor_counts = (int *)calloc(node->level + 1, sizeof(int));
        node->neighbors = (int *)calloc((node->level + 1) * M, sizeof(int));

        idx->nodes[i] = node;
    }

    fclose(fp);
    return idx;
}

void hnsw_sq_destroy(hnsw_sq_t *idx)
{
    if (!idx) return;

    if (idx->nodes) {
        for (int i = 0; i < idx->n_vectors; i++) {
            if (idx->nodes[i]) {
                free(idx->nodes[i]->sq_code);
                free(idx->nodes[i]->neighbors);
                free(idx->nodes[i]->neighbor_counts);
                free(idx->nodes[i]);
            }
        }
        free(idx->nodes);
    }

    free(idx->vectors);
    free(idx->sq.mins);
    free(idx->sq.maxs);
    free(idx->level_0_entry);
    free(idx);
}