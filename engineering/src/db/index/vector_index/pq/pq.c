/**
 * @file pq.c
 * @brief PQ (Product Quantization) 乘积量化实现
 *
 * 核心算法：
 * - K-Means 训练：每个子空间独立训练码本
 * - 编码：找到最近码字索引
 * - ADC 距离：查表求和
 */

#include "db/index/vector_index/pq/pq.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========== 内部辅助函数 ========== */

/**
 * @brief 找到子向量的最近码字
 */
static int find_nearest_centroid(const float *sub_vec, const float *codebook,
                                   int ks, int sub_dim) {
    int best = 0;
    float best_dist = INFINITY;

    for (int c = 0; c < ks; c++) {
        float dist = 0.0f;
        for (int d = 0; d < sub_dim; d++) {
            float diff = sub_vec[d] - codebook[c * sub_dim + d];
            dist += diff * diff;
        }
        if (dist < best_dist) {
            best_dist = dist;
            best = c;
        }
    }

    return best;
}

/**
 * @brief 计算子向量到码字的距离
 */
static float compute_sub_distance(const float *sub_vec, const float *centroid,
                                   int sub_dim) {
    float dist = 0.0f;
    for (int d = 0; d < sub_dim; d++) {
        float diff = sub_vec[d] - centroid[d];
        dist += diff * diff;
    }
    return dist;
}

/* ========== 公开 API 实现 ========== */

pq_quantizer_t *pq_create(int dims, int m, int bits) {
    if (dims <= 0 || m <= 0 || bits <= 0) return NULL;
    if (dims % m != 0) return NULL;  /* 维度必须能被子空间数整除 */

    pq_quantizer_t *pq = (pq_quantizer_t *)calloc(1, sizeof(pq_quantizer_t));
    if (!pq) return NULL;

    pq->dims = dims;
    pq->m = m;
    pq->bits = bits;
    pq->ks = 1 << bits;  /* 2^bits */
    pq->sub_dim = dims / m;

    /* 分配码本空间：m 个子空间，每个有 ks 个码字，每个码字 sub_dim 维 */
    size_t codebook_size = (size_t)m * pq->ks * pq->sub_dim;
    pq->codebooks = (float *)calloc(codebook_size, sizeof(float));

    if (!pq->codebooks) {
        free(pq);
        return NULL;
    }

    pq->trained = 0;
    pq->heap_store = NULL;
    pq->storage = NULL;

    return pq;
}

void pq_destroy(pq_quantizer_t *pq) {
    if (!pq) return;
    free(pq->codebooks);
    free(pq);
}

int pq_train(pq_quantizer_t *pq, int n, const float *vectors) {
    if (!pq || !vectors || n <= 0) return -1;
    if (n < pq->ks) return -1;  /* 需要足够的训练样本 */

    /* 对每个子空间独立训练 K-Means */
    for (int sub = 0; sub < pq->m; sub++) {
        float *codebook = &pq->codebooks[sub * pq->ks * pq->sub_dim];

        /* 初始化：均匀采样作为初始中心 */
        int step = n / pq->ks;
        for (int c = 0; c < pq->ks; c++) {
            int src_idx = (c * step) * pq->dims + sub * pq->sub_dim;
            for (int d = 0; d < pq->sub_dim; d++) {
                codebook[c * pq->sub_dim + d] = vectors[src_idx + d];
            }
        }

        /* K-Means 迭代 */
        int *assignments = (int *)malloc(n * sizeof(int));
        if (!assignments) continue;

        float *new_centroids = (float *)calloc(pq->ks * pq->sub_dim, sizeof(float));
        int *counts = (int *)calloc(pq->ks, sizeof(int));
        if (!new_centroids || !counts) {
            free(assignments);
            free(new_centroids);
            free(counts);
            continue;
        }

        /* 迭代 10 次 */
        for (int iter = 0; iter < 10; iter++) {
            /* 分配：找最近的中心 */
            for (int i = 0; i < n; i++) {
                const float *sub_vec = &vectors[i * pq->dims + sub * pq->sub_dim];
                int best = find_nearest_centroid(sub_vec, codebook, pq->ks, pq->sub_dim);
                assignments[i] = best;
            }

            /* 更新：重新计算中心 */
            memset(new_centroids, 0, pq->ks * pq->sub_dim * sizeof(float));
            memset(counts, 0, pq->ks * sizeof(int));

            for (int i = 0; i < n; i++) {
                int c = assignments[i];
                const float *sub_vec = &vectors[i * pq->dims + sub * pq->sub_dim];
                for (int d = 0; d < pq->sub_dim; d++) {
                    new_centroids[c * pq->sub_dim + d] += sub_vec[d];
                }
                counts[c]++;
            }

            /* 计算平均值 */
            for (int c = 0; c < pq->ks; c++) {
                if (counts[c] > 0) {
                    for (int d = 0; d < pq->sub_dim; d++) {
                        codebook[c * pq->sub_dim + d] =
                            new_centroids[c * pq->sub_dim + d] / counts[c];
                    }
                }
            }
        }

        free(assignments);
        free(new_centroids);
        free(counts);
    }

    pq->trained = 1;
    return 0;
}

int pq_encode(pq_quantizer_t *pq, const float *vector, uint8_t *code) {
    if (!pq || !vector || !code || !pq->trained) return -1;

    for (int sub = 0; sub < pq->m; sub++) {
        const float *sub_vec = &vector[sub * pq->sub_dim];
        const float *codebook = &pq->codebooks[sub * pq->ks * pq->sub_dim];

        int best = find_nearest_centroid(sub_vec, codebook, pq->ks, pq->sub_dim);
        code[sub] = (uint8_t)best;
    }

    return 0;
}

int pq_encode_batch(pq_quantizer_t *pq, int n, const float *vectors, uint8_t *codes) {
    if (!pq || !vectors || !codes) return -1;

    int encoded = 0;
    for (int i = 0; i < n; i++) {
        if (pq_encode(pq, &vectors[i * pq->dims], &codes[i * pq->m]) == 0) {
            encoded++;
        }
    }
    return encoded;
}

int pq_decode(pq_quantizer_t *pq, const uint8_t *code, float *vector) {
    if (!pq || !code || !vector || !pq->trained) return -1;

    for (int sub = 0; sub < pq->m; sub++) {
        const float *codebook = &pq->codebooks[sub * pq->ks * pq->sub_dim];
        const float *centroid = &codebook[code[sub] * pq->sub_dim];

        for (int d = 0; d < pq->sub_dim; d++) {
            vector[sub * pq->sub_dim + d] = centroid[d];
        }
    }

    return 0;
}

int pq_compute_distance_table(pq_quantizer_t *pq, const float *query, float *table) {
    if (!pq || !query || !table || !pq->trained) return -1;

    for (int sub = 0; sub < pq->m; sub++) {
        const float *sub_query = &query[sub * pq->sub_dim];
        const float *codebook = &pq->codebooks[sub * pq->ks * pq->sub_dim];

        for (int c = 0; c < pq->ks; c++) {
            float dist = compute_sub_distance(sub_query, &codebook[c * pq->sub_dim], pq->sub_dim);
            table[sub * pq->ks + c] = dist;
        }
    }

    return 0;
}

float pq_adc_distance(pq_quantizer_t *pq, const uint8_t *code, const float *table) {
    if (!pq || !code || !table) return INFINITY;

    float dist = 0.0f;
    for (int sub = 0; sub < pq->m; sub++) {
        dist += table[sub * pq->ks + code[sub]];
    }
    return dist;
}

float pq_sdt_distance(pq_quantizer_t *pq, const uint8_t *code1, const uint8_t *code2) {
    if (!pq || !code1 || !code2 || !pq->trained) return INFINITY;

    float dist = 0.0f;
    for (int sub = 0; sub < pq->m; sub++) {
        const float *codebook = &pq->codebooks[sub * pq->ks * pq->sub_dim];
        const float *centroid1 = &codebook[code1[sub] * pq->sub_dim];
        const float *centroid2 = &codebook[code2[sub] * pq->sub_dim];

        for (int d = 0; d < pq->sub_dim; d++) {
            float diff = centroid1[d] - centroid2[d];
            dist += diff * diff;
        }
    }
    return dist;
}

int pq_code_size(const pq_quantizer_t *pq) {
    return pq ? pq->m : 0;
}

int pq_distance_table_size(const pq_quantizer_t *pq) {
    return pq ? pq->m * pq->ks : 0;
}

int pq_is_trained(const pq_quantizer_t *pq) {
    return pq ? pq->trained : 0;
}