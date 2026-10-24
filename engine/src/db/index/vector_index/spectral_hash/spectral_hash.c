/*
 * spectral_hash.c
 *
 * Spectral Hash 频谱哈希实现
 *
 * 原理：
 * Spectral Hash 的目标是学习一组哈希函数，使得相似的向量映射到相似的哈希码。
 *
 * 步骤：
 * 1. 计算数据矩阵 X 的协方差矩阵 X^T X
 * 2. 对协方差矩阵进行特征分解
 * 3. 使用前 n_bits 个特征向量
 * 4. 对每个特征向量，生成哈希函数 h(x) = sign(w^T x)
 *
 * 简化实现：
 * - 使用随机投影代替完整的特征分解
 * - 使用平衡划分代替精确的特征向量
 */

#include <db/index/vector_index/spectral_hash/spectral_hash.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

/* ── 内部结构 ── */

/* Spectral Hash 结构 */
struct spectral_hash {
    int dims;               /* 向量维度 */
    int n_bits;            /* 哈希位数 */
    int trained;            /* 是否已训练 */

    float *projections;     /* 投影向量 [n_bits, dims] */
    float *thresholds;     /* 阈值 [n_bits] */
};

/* ── 内部辅助函数 ── */

/**
 * 生成随机高斯向量
 */
static void generate_gaussian_vector(float *v, int dims)
{
    /* Box-Muller 变换 */
    for (int i = 0; i < dims; i += 2) {
        float u1 = (float)rand() / RAND_MAX;
        float u2 = (float)rand() / RAND_MAX;
        if (u1 < 1e-10f) u1 = 1e-10f;

        float r = sqrtf(-2.0f * logf(u1));
        float theta = 2.0f * (float)M_PI * u2;

        v[i] = r * cosf(theta);
        if (i + 1 < dims) {
            v[i + 1] = r * sinf(theta);
        }
    }
}

/**
 * 计算向量范数
 */
static float compute_norm(const float *v, int dims)
{
    float norm = 0.0f;
    for (int i = 0; i < dims; i++) {
        norm += v[i] * v[i];
    }
    return sqrtf(norm);
}

/**
 * 归一化向量
 */
static void normalize(float *v, int dims)
{
    float norm = compute_norm(v, dims);
    if (norm > 1e-10f) {
        float inv = 1.0f / norm;
        for (int i = 0; i < dims; i++) {
            v[i] *= inv;
        }
    }
}

/**
 * 计算投影值
 */
static float compute_projection(const float *v, const float *projection, int dims)
{
    float dot = 0.0f;
    for (int i = 0; i < dims; i++) {
        dot += v[i] * projection[i];
    }
    return dot;
}

/**
 * 计算投影均值（用于设置阈值）
 */
static float compute_projection_mean(const float *vectors, int n, int dims,
                                    const float *projection)
{
    float mean = 0.0f;
    for (int i = 0; i < n; i++) {
        mean += compute_projection(vectors + i * dims, projection, dims);
    }
    return mean / n;
}

/* ── 公共 API 实现 ── */

spectral_hash_t *spectral_hash_create(int dims, int n_bits)
{
    if (dims <= 0 || n_bits <= 0 || n_bits > 32) {
        return NULL;
    }

    spectral_hash_t *sh = (spectral_hash_t *)calloc(1, sizeof(spectral_hash_t));
    if (!sh) return NULL;

    sh->dims = dims;
    sh->n_bits = n_bits;
    sh->trained = 0;

    sh->projections = (float *)malloc(n_bits * dims * sizeof(float));
    sh->thresholds = (float *)malloc(n_bits * sizeof(float));

    if (!sh->projections || !sh->thresholds) {
        free(sh->projections);
        free(sh->thresholds);
        free(sh);
        return NULL;
    }

    return sh;
}

int spectral_hash_train(spectral_hash_t *sh, int n, const float *vectors)
{
    if (!sh || n <= 0 || !vectors) {
        return -1;
    }

    srand((unsigned int)time(NULL));

    /* 为每个哈希位生成投影向量 */
    for (int b = 0; b < sh->n_bits; b++) {
        /* 生成随机高斯向量 */
        generate_gaussian_vector(sh->projections + b * sh->dims, sh->dims);

        /* 归一化 */
        normalize(sh->projections + b * sh->dims, sh->dims);

        /* 计算阈值（使用投影均值使哈希码平衡） */
        sh->thresholds[b] = compute_projection_mean(vectors, n, sh->dims,
                                                    sh->projections + b * sh->dims);
    }

    sh->trained = 1;
    return 0;
}

uint32_t spectral_hash_encode(const spectral_hash_t *sh, const float *vector)
{
    if (!sh || !sh->trained || !vector) {
        return 0;
    }

    uint32_t code = 0;

    for (int b = 0; b < sh->n_bits; b++) {
        /* 计算投影 */
        float proj = compute_projection(vector, sh->projections + b * sh->dims, sh->dims);

        /* 哈希函数：sign(proj - threshold) */
        if (proj > sh->thresholds[b]) {
            code |= (1U << b);
        }
    }

    return code;
}

int spectral_hash_encode_batch(const spectral_hash_t *sh, int n,
                             const float *vectors, uint32_t *codes)
{
    if (!sh || !sh->trained || n <= 0 || !vectors || !codes) {
        return -1;
    }

    for (int i = 0; i < n; i++) {
        codes[i] = spectral_hash_encode(sh, vectors + i * sh->dims);
    }

    return 0;
}

int spectral_hash_hamming_distance(uint32_t code1, uint32_t code2, int n_bits)
{
    /* XOR 后计算 1 的个数 */
    uint32_t diff = code1 ^ code2;
    int distance = 0;

    /* 只计算 n_bits 位 */
    uint32_t mask = (n_bits >= 32) ? 0xFFFFFFFF : ((1U << n_bits) - 1);
    diff &= mask;

    while (diff) {
        distance += diff & 1;
        diff >>= 1;
    }

    return distance;
}

void spectral_hash_get_info(const spectral_hash_t *sh, int *out_dims, int *out_n_bits)
{
    if (!sh) return;

    if (out_dims) *out_dims = sh->dims;
    if (out_n_bits) *out_n_bits = sh->n_bits;
}

int spectral_hash_save(const spectral_hash_t *sh, const char *path)
{
    if (!sh || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头部 */
    fwrite(&sh->dims, sizeof(int), 1, fp);
    fwrite(&sh->n_bits, sizeof(int), 1, fp);
    fwrite(&sh->trained, sizeof(int), 1, fp);

    /* 写入投影向量 */
    fwrite(sh->projections, sizeof(float), sh->n_bits * sh->dims, fp);
    fwrite(sh->thresholds, sizeof(float), sh->n_bits, fp);

    fclose(fp);
    return 0;
}

spectral_hash_t *spectral_hash_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, n_bits, trained;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_bits, sizeof(int), 1, fp) != 1 ||
        fread(&trained, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建量化器 */
    spectral_hash_t *sh = spectral_hash_create(dims, n_bits);
    if (!sh) {
        fclose(fp);
        return NULL;
    }

    sh->trained = trained;

    /* 读取投影向量 */
    if (fread(sh->projections, sizeof(float), n_bits * dims, fp) != (size_t)(n_bits * dims)) {
        fclose(fp);
        spectral_hash_destroy(sh);
        return NULL;
    }

    /* 读取阈值 */
    if (fread(sh->thresholds, sizeof(float), n_bits, fp) != (size_t)n_bits) {
        fclose(fp);
        spectral_hash_destroy(sh);
        return NULL;
    }

    fclose(fp);
    return sh;
}

void spectral_hash_destroy(spectral_hash_t *sh)
{
    if (!sh) return;

    free(sh->projections);
    free(sh->thresholds);
    free(sh);
}