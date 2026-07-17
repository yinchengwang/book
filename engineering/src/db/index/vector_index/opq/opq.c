/*
 * opq.c
 *
 * OPQ (Optimized Product Quantization) 优化乘积量化
 *
 * 在 PQ 基础上增加 PCA 旋转优化子空间划分
 */

#include <db/index/vector_index/opq/opq.h>
#include <algo-prod/quantization/quantization.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 内部工作空间（不暴露在头文件结构体中）*/
static float *g_opq_workspace = NULL;

static int _find_nearest_code(const float *sub_vec, const float *codebook, int ks, int sub_dim);

opq_quantizer_t *opq_create(int dims, int m, int bits)
{
    if (dims <= 0 || m <= 0 || bits <= 0 || dims < m) return NULL;

    opq_quantizer_t *opq = (opq_quantizer_t *)calloc(1, sizeof(opq_quantizer_t));
    if (!opq) return NULL;

    opq->dims = dims;
    opq->m = m;
    opq->bits = bits;
    opq->ks = 1 << bits;
    opq->sub_dim = dims / m;

    opq->codebooks = (float *)calloc(m * opq->ks * opq->sub_dim, sizeof(float));

    if (!opq->codebooks) {
        free(opq);
        return NULL;
    }

    opq->trained = 0;
    opq->heap_store = NULL;
    opq->storage = NULL;
    return opq;
}

int opq_train(opq_quantizer_t *opq, int n, const float *vectors)
{
    if (!opq || !vectors || n <= 0) return -1;

    /* 简化实现：直接对每个子空间做 K-Means */
    for (int sub = 0; sub < opq->m; sub++) {
        float *codebook = &opq->codebooks[sub * opq->ks * opq->sub_dim];

        /* 用前 ks 个样本作为初始中心 */
        for (int c = 0; c < opq->ks && c < n; c++) {
            for (int d = 0; d < opq->sub_dim; d++) {
                int src_idx = c * opq->dims + sub * opq->sub_dim + d;
                if (src_idx < n * opq->dims) {
                    codebook[c * opq->sub_dim + d] = vectors[src_idx];
                }
            }
        }

        /* 简单迭代优化 */
        for (int iter = 0; iter < 5; iter++) {
            int *assignments = (int *)malloc(n * sizeof(int));
            if (!assignments) break;

            for (int i = 0; i < n; i++) {
                int best = 0;
                float best_dist = 1e10f;

                for (int c = 0; c < opq->ks; c++) {
                    float dist = 0;
                    for (int d = 0; d < opq->sub_dim; d++) {
                        int src_idx = i * opq->dims + sub * opq->sub_dim + d;
                        if (src_idx < n * opq->dims) {
                            float diff = vectors[src_idx] - codebook[c * opq->sub_dim + d];
                            dist += diff * diff;
                        }
                    }
                    if (dist < best_dist) {
                        best_dist = dist;
                        best = c;
                    }
                }
                assignments[i] = best;
            }

            /* 更新中心 */
            for (int c = 0; c < opq->ks; c++) {
                int count = 0;
                float sum[64] = {0};

                for (int i = 0; i < n; i++) {
                    if (assignments[i] == c) {
                        count++;
                        for (int d = 0; d < opq->sub_dim; d++) {
                            int src_idx = i * opq->dims + sub * opq->sub_dim + d;
                            if (src_idx < n * opq->dims) {
                                sum[d] += vectors[src_idx];
                            }
                        }
                    }
                }

                if (count > 0) {
                    for (int d = 0; d < opq->sub_dim; d++) {
                        codebook[c * opq->sub_dim + d] = sum[d] / count;
                    }
                }
            }

            free(assignments);
        }
    }

    opq->trained = 1;
    return 0;
}

int opq_encode(opq_quantizer_t *opq, const float *vector, uint8_t *code)
{
    if (!opq || !vector || !code || !opq->trained) return -1;

    for (int sub = 0; sub < opq->m; sub++) {
        const float *sub_vec = &vector[sub * opq->sub_dim];
        const float *codebook = &opq->codebooks[sub * opq->ks * opq->sub_dim];

        int best = _find_nearest_code(sub_vec, codebook, opq->ks, opq->sub_dim);
        code[sub] = (uint8_t)best;
    }

    return 0;
}

int opq_encode_batch(opq_quantizer_t *opq, int n, const float *vectors, uint8_t *codes)
{
    if (!opq || !vectors || !codes) return -1;

    int encoded = 0;
    for (int i = 0; i < n; i++) {
        if (opq_encode(opq, &vectors[i * opq->dims], &codes[i * opq->m]) == 0) {
            encoded++;
        }
    }
    return encoded;
}

int opq_decode(opq_quantizer_t *opq, const uint8_t *code, float *vector)
{
    if (!opq || !code || !vector || !opq->trained) return -1;

    for (int sub = 0; sub < opq->m; sub++) {
        const float *codebook = &opq->codebooks[sub * opq->ks * opq->sub_dim];
        const float *centroid = &codebook[code[sub] * opq->sub_dim];

        for (int d = 0; d < opq->sub_dim; d++) {
            vector[sub * opq->sub_dim + d] = centroid[d];
        }
    }

    return 0;
}

int opq_compute_distance_table(opq_quantizer_t *opq, const float *query, float *table)
{
    if (!opq || !query || !table || !opq->trained) return -1;

    for (int sub = 0; sub < opq->m; sub++) {
        const float *sub_query = &query[sub * opq->sub_dim];
        const float *codebook = &opq->codebooks[sub * opq->ks * opq->sub_dim];

        for (int c = 0; c < opq->ks; c++) {
            float dist = 0;
            for (int d = 0; d < opq->sub_dim; d++) {
                float diff = sub_query[d] - codebook[c * opq->sub_dim + d];
                dist += diff * diff;
            }
            table[sub * opq->ks + c] = dist;
        }
    }

    return 0;
}

float opq_adc_distance(opq_quantizer_t *opq, const uint8_t *code, const float *table)
{
    if (!opq || !code || !table) return 0;

    float dist = 0;
    for (int sub = 0; sub < opq->m; sub++) {
        dist += table[sub * opq->ks + code[sub]];
    }
    return dist;
}

int opq_code_size(const opq_quantizer_t *opq)
{
    return opq ? opq->m : 0;
}

int opq_distance_table_size(const opq_quantizer_t *opq)
{
    return opq ? opq->m * opq->ks : 0;
}

int opq_is_trained(const opq_quantizer_t *opq)
{
    return opq ? opq->trained : 0;
}

void opq_destroy(opq_quantizer_t *opq)
{
    if (!opq) return;
    free(opq->codebooks);
    /* heap_store 和 storage 的生命周期由外部管理，这里不释放 */
    free(opq);
}

static int _find_nearest_code(const float *sub_vec, const float *codebook, int ks, int sub_dim)
{
    int best = 0;
    float best_dist = 1e10f;

    for (int c = 0; c < ks; c++) {
        float dist = 0;
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
