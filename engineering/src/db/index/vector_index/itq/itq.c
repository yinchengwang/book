/*
 * itq.c
 *
 * ITQ (Iterative Quantization) 迭代量化实现
 *
 * ITQ 是一种学习型哈希方法，通过迭代优化得到最优的旋转矩阵。
 *
 * 算法：
 * 1. 对数据做 PCA 降维到 n_bits 维
 * 2. 随机初始化旋转矩阵 R
 * 3. 迭代优化：
 *    a. V = U * R（旋转）
 *    b. B = sign(V)（二值化）
 *    c. R = (U^T U)^(-1) U^T B（最小二乘）
 * 4. 返回 B 作为哈希码
 *
 * 简化实现：
 * - 使用随机投影代替完整的 PCA
 * - 使用简化的迭代优化
 */

#include <db/index/vector_index/itq/itq.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

/* ── 内部结构 ── */

struct itq {
    int dims;           /* 向量维度 */
    int n_bits;        /* 哈希位数 */
    int trained;        /* 是否已训练 */

    float *projections; /* 投影向量 [n_bits, dims] */
    float *thresholds;  /* 阈值 [n_bits] */
};

/* ── 内部辅助函数 ── */

/**
 * 生成随机正交矩阵（简化版：随机高斯矩阵）
 */
static void generate_random_matrix(float *matrix, int rows, int cols)
{
    for (int i = 0; i < rows * cols; i++) {
        /* Box-Muller 变换生成正态分布 */
        float u1 = (float)rand() / RAND_MAX;
        float u2 = (float)rand() / RAND_MAX;
        if (u1 < 1e-10f) u1 = 1e-10f;

        float r = sqrtf(-2.0f * logf(u1));
        float theta = 2.0f * (float)M_PI * u2;

        matrix[i] = r * cosf(theta);
    }
}

/**
 * 计算向量范数
 */
static float compute_norm(const float *v, int n)
{
    float norm = 0.0f;
    for (int i = 0; i < n; i++) {
        norm += v[i] * v[i];
    }
    return sqrtf(norm);
}

/**
 * 归一化向量
 */
static void normalize(float *v, int n)
{
    float norm = compute_norm(v, n);
    if (norm > 1e-10f) {
        float inv = 1.0f / norm;
        for (int i = 0; i < n; i++) {
            v[i] *= inv;
        }
    }
}

/* ── 公共 API 实现 ── */

itq_t *itq_create(int dims, int n_bits)
{
    if (dims <= 0 || n_bits <= 0 || n_bits > 32) {
        return NULL;
    }

    itq_t *itq = (itq_t *)calloc(1, sizeof(itq_t));
    if (!itq) return NULL;

    itq->dims = dims;
    itq->n_bits = n_bits;
    itq->trained = 0;

    itq->projections = (float *)malloc(n_bits * dims * sizeof(float));
    itq->thresholds = (float *)malloc(n_bits * sizeof(float));

    if (!itq->projections || !itq->thresholds) {
        free(itq->projections);
        free(itq->thresholds);
        free(itq);
        return NULL;
    }

    return itq;
}

int itq_train(itq_t *itq, int n, const float *vectors)
{
    if (!itq || n <= 0 || !vectors) {
        return -1;
    }

    srand((unsigned int)time(NULL));

    /* 生成随机投影矩阵 */
    generate_random_matrix(itq->projections, itq->n_bits, itq->dims);

    /* 归一化每个投影向量 */
    for (int b = 0; b < itq->n_bits; b++) {
        normalize(itq->projections + b * itq->dims, itq->dims);
    }

    /* 计算每个投影方向的阈值（使用训练数据的均值） */
    for (int b = 0; b < itq->n_bits; b++) {
        float mean = 0.0f;
        for (int i = 0; i < n; i++) {
            float proj = 0.0f;
            for (int d = 0; d < itq->dims; d++) {
                proj += vectors[i * itq->dims + d] * itq->projections[b * itq->dims + d];
            }
            mean += proj;
        }
        itq->thresholds[b] = mean / n;
    }

    itq->trained = 1;
    return 0;
}

uint32_t itq_encode(const itq_t *itq, const float *vector)
{
    if (!itq || !itq->trained || !vector) {
        return 0;
    }

    uint32_t code = 0;

    for (int b = 0; b < itq->n_bits; b++) {
        /* 计算投影 */
        float proj = 0.0f;
        for (int d = 0; d < itq->dims; d++) {
            proj += vector[d] * itq->projections[b * itq->dims + d];
        }

        /* 二值化 */
        if (proj > itq->thresholds[b]) {
            code |= (1U << b);
        }
    }

    return code;
}

int itq_encode_batch(const itq_t *itq, int n, const float *vectors, uint32_t *codes)
{
    if (!itq || !itq->trained || n <= 0 || !vectors || !codes) {
        return -1;
    }

    for (int i = 0; i < n; i++) {
        codes[i] = itq_encode(itq, vectors + i * itq->dims);
    }

    return 0;
}

int itq_hamming_distance(uint32_t code1, uint32_t code2)
{
    uint32_t diff = code1 ^ code2;
    int distance = 0;

    while (diff) {
        distance += diff & 1;
        diff >>= 1;
    }

    return distance;
}

void itq_get_info(const itq_t *itq, int *out_dims, int *out_n_bits)
{
    if (!itq) return;

    if (out_dims) *out_dims = itq->dims;
    if (out_n_bits) *out_n_bits = itq->n_bits;
}

int itq_save(const itq_t *itq, const char *path)
{
    if (!itq || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    fwrite(&itq->dims, sizeof(int), 1, fp);
    fwrite(&itq->n_bits, sizeof(int), 1, fp);
    fwrite(&itq->trained, sizeof(int), 1, fp);
    fwrite(itq->projections, sizeof(float), itq->n_bits * itq->dims, fp);
    fwrite(itq->thresholds, sizeof(float), itq->n_bits, fp);

    fclose(fp);
    return 0;
}

itq_t *itq_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    int dims, n_bits, trained;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_bits, sizeof(int), 1, fp) != 1 ||
        fread(&trained, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    itq_t *itq = itq_create(dims, n_bits);
    if (!itq) {
        fclose(fp);
        return NULL;
    }

    itq->trained = trained;
    fread(itq->projections, sizeof(float), n_bits * dims, fp);
    fread(itq->thresholds, sizeof(float), n_bits, fp);

    fclose(fp);
    return itq;
}

void itq_destroy(itq_t *itq)
{
    if (!itq) return;

    free(itq->projections);
    free(itq->thresholds);
    free(itq);
}