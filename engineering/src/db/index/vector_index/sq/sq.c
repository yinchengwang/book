/**
 * @file sq.c
 * @brief SQ (Scalar Quantization) 标量量化实现
 *
 * 核心算法：
 * - 训练：统计每维最小/最大值
 * - 编码：线性映射到 [0, 255]
 * - 解码：线性映射回原始范围
 * - 距离：按量化值计算
 */

#include "db/index/vector_index/sq/sq.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========== 公开 API 实现 ========== */

sq_quantizer_t *sq_create(int dims, int bits) {
    if (dims <= 0 || bits <= 0) return NULL;
    if (bits != 8) return NULL;  /* 目前只支持 8-bit */

    sq_quantizer_t *sq = (sq_quantizer_t *)calloc(1, sizeof(sq_quantizer_t));
    if (!sq) return NULL;

    sq->dims = dims;
    sq->bits = bits;

    sq->mins = (float *)calloc(dims, sizeof(float));
    sq->maxs = (float *)calloc(dims, sizeof(float));
    sq->scales = (float *)calloc(dims, sizeof(float));

    if (!sq->mins || !sq->maxs || !sq->scales) {
        free(sq->mins);
        free(sq->maxs);
        free(sq->scales);
        free(sq);
        return NULL;
    }

    sq->trained = 0;

    return sq;
}

void sq_destroy(sq_quantizer_t *sq) {
    if (!sq) return;
    free(sq->mins);
    free(sq->maxs);
    free(sq->scales);
    free(sq);
}

int sq_train(sq_quantizer_t *sq, int n, const float *vectors) {
    if (!sq || !vectors || n <= 0) return -1;

    /* 初始化为第一个向量的值 */
    for (int d = 0; d < sq->dims; d++) {
        sq->mins[d] = vectors[d];
        sq->maxs[d] = vectors[d];
    }

    /* 扫描所有向量，找到每维的最小/最大值 */
    for (int i = 1; i < n; i++) {
        for (int d = 0; d < sq->dims; d++) {
            float val = vectors[i * sq->dims + d];
            if (val < sq->mins[d]) sq->mins[d] = val;
            if (val > sq->maxs[d]) sq->maxs[d] = val;
        }
    }

    /* 计算缩放因子 */
    for (int d = 0; d < sq->dims; d++) {
        float range = sq->maxs[d] - sq->mins[d];
        if (range > 0) {
            sq->scales[d] = 255.0f / range;
        } else {
            sq->scales[d] = 0.0f;  /* 常量维度 */
        }
    }

    sq->trained = 1;
    return 0;
}

int sq_encode(sq_quantizer_t *sq, const float *vector, uint8_t *code) {
    if (!sq || !vector || !code || !sq->trained) return -1;

    for (int d = 0; d < sq->dims; d++) {
        float normalized = (vector[d] - sq->mins[d]) * sq->scales[d];

        /* 裁剪到 [0, 255] */
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 255.0f) normalized = 255.0f;

        code[d] = (uint8_t)(normalized + 0.5f);  /* 四舍五入 */
    }

    return 0;
}

int sq_encode_batch(sq_quantizer_t *sq, int n, const float *vectors, uint8_t *codes) {
    if (!sq || !vectors || !codes) return -1;

    int encoded = 0;
    for (int i = 0; i < n; i++) {
        if (sq_encode(sq, &vectors[i * sq->dims], &codes[i * sq->dims]) == 0) {
            encoded++;
        }
    }
    return encoded;
}

int sq_decode(sq_quantizer_t *sq, const uint8_t *code, float *vector) {
    if (!sq || !code || !vector || !sq->trained) return -1;

    for (int d = 0; d < sq->dims; d++) {
        /* 逆映射：code / 255 * range + min */
        vector[d] = code[d] / 255.0f * (sq->maxs[d] - sq->mins[d]) + sq->mins[d];
    }

    return 0;
}

float sq_distance_to_vector(sq_quantizer_t *sq, const uint8_t *code, const float *vector) {
    if (!sq || !code || !vector || !sq->trained) return INFINITY;

    float dist = 0.0f;

    for (int d = 0; d < sq->dims; d++) {
        /* 将 code 解码回浮点值 */
        float decoded = code[d] / 255.0f * (sq->maxs[d] - sq->mins[d]) + sq->mins[d];

        float diff = decoded - vector[d];
        dist += diff * diff;
    }

    return dist;
}

float sq_distance(sq_quantizer_t *sq, const uint8_t *code1, const uint8_t *code2) {
    if (!sq || !code1 || !code2) return INFINITY;

    float dist = 0.0f;

    for (int d = 0; d < sq->dims; d++) {
        /* 直接用量化值计算距离（更快，但有精度损失） */
        float diff = (float)code1[d] - (float)code2[d];

        /* 考虑缩放因子 */
        float scale = (sq->maxs[d] - sq->mins[d]) / 255.0f;
        diff *= scale;

        dist += diff * diff;
    }

    return dist;
}

int sq_code_size(const sq_quantizer_t *sq) {
    return sq ? sq->dims : 0;
}

int sq_is_trained(const sq_quantizer_t *sq) {
    return sq ? sq->trained : 0;
}