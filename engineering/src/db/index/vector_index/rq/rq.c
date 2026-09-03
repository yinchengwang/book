/*
 * rq.c
 *
 * RQ (Residual Quantization) 残差量化器实现
 *
 * 多级残差量化原理：
 * 1. 第一级：对原始向量进行 k-means 量化，得到量化中心和残差
 * 2. 第二级：对第一级残差进行量化，得到新的中心和残差
 * 3. 重复直到达到指定级数
 *
 * 最终码字 = [stage0_idx, stage1_idx, ..., stageN_idx]
 * 解码时累加各级的中心向量
 */

#include <db/index/vector_index/rq/rq.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define RQ_MAX_ITERS 20        /* k-means 最大迭代次数 */
#define RQ_MIN_POINTS 1        /* 每个簇最小点数 */

/* ── 内部结构 ── */

/* 单级量化器 */
typedef struct {
    int n_centroids;           /* 中心数量 */
    int dims;                   /* 维度 */
    float *centroids;           /* 中心向量 [n_centroids, dims] */
    float *distances;           /* 临时距离数组 */
} rq_stage_t;

/* RQ 量化器结构 */
struct rq {
    int dims;                   /* 向量维度 */
    int bits_per_stage;         /* 每级位数 */
    int n_stages;               /* 量化级数 */
    int n_centroids;            /* 每级中心数 = 2^bits */
    int code_size;              /* 码字大小（字节） */

    rq_stage_t *stages;         /* 各量化级 */
    int trained;                /* 是否已训练 */
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
    return dist;
}

/**
 * 初始化 k-means 中心（k-means++）
 */
static void init_centroids_kmeanspp(float *centroids, int n_centroids,
                                     const float *vectors, int n_vectors, int dims)
{
    /* 第一个中心：随机选择 */
    int first_idx = rand() % n_vectors;
    memcpy(centroids, vectors + first_idx * dims, dims * sizeof(float));

    /* 剩余中心：按概率距离加权选择 */
    float *probs = (float *)malloc(n_vectors * sizeof(float));

    for (int c = 1; c < n_centroids; c++) {
        float total_dist = 0.0f;

        for (int i = 0; i < n_vectors; i++) {
            float min_dist = FLT_MAX;
            for (int j = 0; j < c; j++) {
                float dist = compute_l2_distance(vectors + i * dims,
                                                  centroids + j * dims, dims);
                if (dist < min_dist) {
                    min_dist = dist;
                }
            }
            probs[i] = min_dist;
            total_dist += min_dist;
        }

        /* 按概率选择下一个中心 */
        float r = ((float)rand() / RAND_MAX) * total_dist;
        float cumulative = 0.0f;
        int selected = 0;

        for (int i = 0; i < n_vectors; i++) {
            cumulative += probs[i];
            if (cumulative >= r) {
                selected = i;
                break;
            }
        }

        memcpy(centroids + c * dims, vectors + selected * dims, dims * sizeof(float));
    }

    free(probs);
}

/**
 * 分配点到最近的中心
 */
static void assign_points(int *assignments, const float *vectors, int n_vectors,
                          const float *centroids, int n_centroids, int dims)
{
    for (int i = 0; i < n_vectors; i++) {
        int best_centroid = 0;
        float best_dist = compute_l2_distance(vectors + i * dims,
                                              centroids, dims);

        for (int c = 1; c < n_centroids; c++) {
            float dist = compute_l2_distance(vectors + i * dims,
                                             centroids + c * dims, dims);
            if (dist < best_dist) {
                best_dist = dist;
                best_centroid = c;
            }
        }

        assignments[i] = best_centroid;
    }
}

/**
 * 更新中心位置
 */
static int update_centroids(float *centroids, const float *vectors, int n_vectors,
                            const int *assignments, int n_centroids, int dims)
{
    /* 清零 */
    memset(centroids, 0, n_centroids * dims * sizeof(float));

    /* 累加点 */
    int *counts = (int *)calloc(n_centroids, sizeof(int));

    for (int i = 0; i < n_vectors; i++) {
        int c = assignments[i];
        for (int d = 0; d < dims; d++) {
            centroids[c * dims + d] += vectors[i * dims + d];
        }
        counts[c]++;
    }

    /* 计算均值 */
    float max_change = 0.0f;
    for (int c = 0; c < n_centroids; c++) {
        if (counts[c] >= RQ_MIN_POINTS) {
            float inv_count = 1.0f / counts[c];
            for (int d = 0; d < dims; d++) {
                float old_val = centroids[c * dims + d];
                centroids[c * dims + d] *= inv_count;

                float change = fabsf(centroids[c * dims + d] - old_val);
                if (change > max_change) {
                    max_change = change;
                }
            }
        } else {
            /* 空簇：重新随机初始化 */
            int rand_idx = rand() % n_vectors;
            memcpy(centroids + c * dims, vectors + rand_idx * dims, dims * sizeof(float));
        }
    }

    free(counts);
    return (max_change > 1e-4f) ? 1 : 0;
}

/**
 * 训练单级量化器
 */
static int train_stage(rq_stage_t *stage, const float *vectors, int n_vectors, int dims)
{
    /* 分配数组 */
    stage->centroids = (float *)malloc(stage->n_centroids * dims * sizeof(float));
    if (!stage->centroids) return -1;

    stage->distances = (float *)malloc(n_vectors * sizeof(float));
    if (!stage->distances) {
        free(stage->centroids);
        return -1;
    }

    stage->dims = dims;

    /* k-means++ 初始化 */
    init_centroids_kmeanspp(stage->centroids, stage->n_centroids,
                             vectors, n_vectors, dims);

    /* 迭代聚类 */
    int *assignments = (int *)malloc(n_vectors * sizeof(int));

    for (int iter = 0; iter < RQ_MAX_ITERS; iter++) {
        /* 分配点 */
        assign_points(assignments, vectors, n_vectors,
                      stage->centroids, stage->n_centroids, dims);

        /* 更新中心 */
        if (!update_centroids(stage->centroids, vectors, n_vectors,
                               assignments, stage->n_centroids, dims)) {
            break;
        }
    }

    free(assignments);
    return 0;
}

/* ── 公共 API 实现 ── */

rq_t *rq_create(int dims, int bits_per_stage, int n_stages)
{
    if (dims <= 0 || bits_per_stage <= 0 || bits_per_stage > 8 || n_stages <= 0) {
        return NULL;
    }

    rq_t *rq = (rq_t *)calloc(1, sizeof(rq_t));
    if (!rq) return NULL;

    rq->dims = dims;
    rq->bits_per_stage = bits_per_stage;
    rq->n_stages = n_stages;
    rq->n_centroids = 1 << bits_per_stage;  /* 2^bits */
    rq->code_size = n_stages;  /* 每级一个字节 */

    rq->stages = (rq_stage_t *)calloc(n_stages, sizeof(rq_stage_t));
    if (!rq->stages) {
        free(rq);
        return NULL;
    }

    for (int s = 0; s < n_stages; s++) {
        rq->stages[s].n_centroids = rq->n_centroids;
    }

    rq->trained = 0;

    return rq;
}

int rq_train(rq_t *rq, int n, const float *vectors)
{
    if (!rq || n <= 0 || !vectors) {
        return -1;
    }

    srand((unsigned int)time(NULL));

    /* 创建残差数组 */
    float *residuals = (float *)malloc(n * rq->dims * sizeof(float));
    if (!residuals) return -1;

    /* 第一级：对原始向量训练 */
    if (train_stage(&rq->stages[0], vectors, n, rq->dims) != 0) {
        free(residuals);
        return -1;
    }

    /* 计算残差 */
    int *assignments = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) {
        /* 找到最近的中心 */
        int best = 0;
        float best_dist = compute_l2_distance(vectors + i * rq->dims,
                                              rq->stages[0].centroids, rq->dims);
        for (int c = 1; c < rq->n_centroids; c++) {
            float dist = compute_l2_distance(vectors + i * rq->dims,
                                             rq->stages[0].centroids + c * rq->dims, rq->dims);
            if (dist < best_dist) {
                best_dist = dist;
                best = c;
            }
        }
        assignments[i] = best;

        /* 残差 = 向量 - 中心 */
        for (int d = 0; d < rq->dims; d++) {
            residuals[i * rq->dims + d] = vectors[i * rq->dims + d] -
                                          rq->stages[0].centroids[best * rq->dims + d];
        }
    }

    /* 后续级：对残差训练 */
    for (int s = 1; s < rq->n_stages; s++) {
        if (train_stage(&rq->stages[s], residuals, n, rq->dims) != 0) {
            free(residuals);
            free(assignments);
            return -1;
        }

        /* 计算新的残差 */
        for (int i = 0; i < n; i++) {
            /* 找到最近的中心 */
            int best = 0;
            float best_dist = compute_l2_distance(residuals + i * rq->dims,
                                                  rq->stages[s].centroids, rq->dims);
            for (int c = 1; c < rq->n_centroids; c++) {
                float dist = compute_l2_distance(residuals + i * rq->dims,
                                                 rq->stages[s].centroids + c * rq->dims, rq->dims);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = c;
                }
            }

            /* 更新残差 */
            for (int d = 0; d < rq->dims; d++) {
                residuals[i * rq->dims + d] -= rq->stages[s].centroids[best * rq->dims + d];
            }
        }
    }

    free(residuals);
    free(assignments);

    rq->trained = 1;
    return 0;
}

int rq_encode(rq_t *rq, int n, const float *vectors, uint8_t *codes)
{
    if (!rq || !rq->trained || n <= 0 || !vectors || !codes) {
        return -1;
    }

    float *temp = (float *)malloc(rq->dims * sizeof(float));
    float *residual = (float *)malloc(n * rq->dims * sizeof(float));

    /* 复制原始向量作为残差 */
    memcpy(residual, vectors, n * rq->dims * sizeof(float));

    for (int i = 0; i < n; i++) {
        float *vec = residual + i * rq->dims;

        for (int s = 0; s < rq->n_stages; s++) {
            /* 找到最近的中心 */
            int best = 0;
            float best_dist = compute_l2_distance(vec, rq->stages[s].centroids, rq->dims);

            for (int c = 1; c < rq->n_centroids; c++) {
                float dist = compute_l2_distance(vec, rq->stages[s].centroids + c * rq->dims, rq->dims);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = c;
                }
            }

            codes[i * rq->n_stages + s] = (uint8_t)best;

            /* 更新残差 */
            for (int d = 0; d < rq->dims; d++) {
                vec[d] -= rq->stages[s].centroids[best * rq->dims + d];
            }
        }
    }

    free(temp);
    free(residual);

    return 0;
}

int rq_decode(rq_t *rq, int n, const uint8_t *codes, float *vectors)
{
    if (!rq || !rq->trained || n <= 0 || !codes || !vectors) {
        return -1;
    }

    /* 清零输出 */
    memset(vectors, 0, n * rq->dims * sizeof(float));

    /* 累加各级的中心 */
    for (int s = 0; s < rq->n_stages; s++) {
        for (int i = 0; i < n; i++) {
            int centroid_idx = codes[i * rq->n_stages + s];
            for (int d = 0; d < rq->dims; d++) {
                vectors[i * rq->dims + d] += rq->stages[s].centroids[centroid_idx * rq->dims + d];
            }
        }
    }

    return 0;
}

float rq_compute_distance(const rq_t *rq, const float *query, const uint8_t *codes)
{
    if (!rq || !rq->trained || !query || !codes) {
        return FLT_MAX;
    }

    /* 解码并计算距离 */
    float decoded[128];  /* 假设最大维度 */
    if (rq->dims > 128) {
        /* 动态分配 */
        float *decoded_large = (float *)malloc(rq->dims * sizeof(float));
        memset(decoded_large, 0, rq->dims * sizeof(float));

        for (int s = 0; s < rq->n_stages; s++) {
            int centroid_idx = codes[s];
            for (int d = 0; d < rq->dims; d++) {
                decoded_large[d] += rq->stages[s].centroids[centroid_idx * rq->dims + d];
            }
        }

        float dist = compute_l2_distance(query, decoded_large, rq->dims);
        free(decoded_large);
        return dist;
    }

    /* 小维度优化 */
    memset(decoded, 0, rq->dims * sizeof(float));

    for (int s = 0; s < rq->n_stages; s++) {
        int centroid_idx = codes[s];
        for (int d = 0; d < rq->dims; d++) {
            decoded[d] += rq->stages[s].centroids[centroid_idx * rq->dims + d];
        }
    }

    return compute_l2_distance(query, decoded, rq->dims);
}

void rq_get_info(const rq_t *rq, int *out_dims, int *out_bits,
                 int *out_stages, int *out_code_size)
{
    if (!rq) return;

    if (out_dims) *out_dims = rq->dims;
    if (out_bits) *out_bits = rq->bits_per_stage;
    if (out_stages) *out_stages = rq->n_stages;
    if (out_code_size) *out_code_size = rq->code_size;
}

int rq_save(const rq_t *rq, const char *path)
{
    if (!rq || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头部 */
    fwrite(&rq->dims, sizeof(int), 1, fp);
    fwrite(&rq->bits_per_stage, sizeof(int), 1, fp);
    fwrite(&rq->n_stages, sizeof(int), 1, fp);

    /* 写入各级的码书 */
    for (int s = 0; s < rq->n_stages; s++) {
        rq_stage_t *stage = &rq->stages[s];
        fwrite(&stage->n_centroids, sizeof(int), 1, fp);
        fwrite(stage->centroids, sizeof(float), stage->n_centroids * rq->dims, fp);
    }

    fclose(fp);
    return 0;
}

rq_t *rq_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, bits, stages;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&bits, sizeof(int), 1, fp) != 1 ||
        fread(&stages, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建量化器 */
    rq_t *rq = rq_create(dims, bits, stages);
    if (!rq) {
        fclose(fp);
        return NULL;
    }

    /* 读取码书 */
    for (int s = 0; s < stages; s++) {
        rq_stage_t *stage = &rq->stages[s];

        if (fread(&stage->n_centroids, sizeof(int), 1, fp) != 1) {
            fclose(fp);
            rq_destroy(rq);
            return NULL;
        }

        stage->centroids = (float *)malloc(stage->n_centroids * dims * sizeof(float));
        if (!stage->centroids) {
            fclose(fp);
            rq_destroy(rq);
            return NULL;
        }

        if (fread(stage->centroids, sizeof(float), stage->n_centroids * dims, fp) !=
            (size_t)(stage->n_centroids * dims)) {
            fclose(fp);
            rq_destroy(rq);
            return NULL;
        }
    }

    rq->trained = 1;

    fclose(fp);
    return rq;
}

void rq_destroy(rq_t *rq)
{
    if (!rq) return;

    if (rq->stages) {
        for (int s = 0; s < rq->n_stages; s++) {
            free(rq->stages[s].centroids);
            free(rq->stages[s].distances);
        }
        free(rq->stages);
    }

    free(rq);
}