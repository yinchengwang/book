#include "sq.h"

#include <float.h>
#include <math.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * SQ 量化器原理
 * ──────────────────────────────────────────────────────────────────────────
 *
 * SQ (Scalar Quantization) 是一种简单的标量量化方法：
 *
 * 1. 训练阶段：扫描所有向量，记录全局 min 和 max
 *    - global_min = min(all_vector_dims)
 *    - global_max = max(all_vector_dims)
 *    - scale = (global_max - global_min) / (2^sq_bits - 1)
 *
 * 2. 编码阶段：对每个维度独立量化
 *    - code[i] = round((x[i] - global_min) / scale)
 *    - code[i] = clamp(code[i], 0, 2^sq_bits - 1)
 *
 * 3. 距离计算 (ADC)：
 *    - 先将查询向量原样存入距离表
 *    - 距离 = sum((query[i] - (global_min + scale * code[i]))^2)
 *
 * 与 LVQ 的区别：
 * - LVQ: 每向量独立 scale/bias
 * - SQ:   全局共享 scale/bias
 * ────────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────────
 * 生命周期
 * ────────────────────────────────────────────────────────────────────────── */

int32_t sq_init(quantizer_t *q)
{
    if (!q) {
        return -1;
    }
    /* SQ 不需要堆内存分配，参数由配置和训练决定 */
    q->global_min = 0.0f;
    q->global_max = 0.0f;
    q->scale      = 0.0f;
    return 0;
}

void sq_free(quantizer_t *q)
{
    /* SQ 无堆内存 */
    if (q) {
        q->global_min = 0.0f;
        q->global_max = 0.0f;
        q->scale      = 0.0f;
    }
}

void sq_reset(quantizer_t *q)
{
    if (q) {
        q->trained    = false;
        q->global_min = 0.0f;
        q->global_max = 0.0f;
        q->scale      = 0.0f;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 训练：全局统计 min/max
 * ────────────────────────────────────────────────────────────────────────── */

int32_t sq_train(quantizer_t *q, int32_t n, const float *vectors)
{
    int32_t i;
    int32_t d;

    if (!q || n <= 0 || !vectors) {
        return -1;
    }

    /* 初始化全局 min/max */
    q->global_min = FLT_MAX;
    q->global_max = -FLT_MAX;

    /* 扫描所有向量找全局 min/max */
    for (i = 0; i < n; ++i) {
        for (d = 0; d < q->config.dims; ++d) {
            float v = vectors[i * q->config.dims + d];
            if (v < q->global_min) {
                q->global_min = v;
            }
            if (v > q->global_max) {
                q->global_max = v;
            }
        }
    }

    /* 计算量化步长 */
    {
        float range    = q->global_max - q->global_min;
        int32_t levels = (1 << q->config.sq_bits) - 1; /* 2^sq_bits - 1 */

        if (levels <= 0 || range <= 0.0f) {
            /* 处理特殊情况：所有值相同 */
            q->scale = 1.0f;
        } else {
            q->scale = range / (float)levels;
        }
    }

    q->trained = true;
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 辅助：计算某位在码字中的位置
 * ────────────────────────────────────────────────────────────────────────── */

static inline void sq_encode_bits(const quantizer_t *q, const float *vector, uint8_t *code)
{
    int32_t d;
    int32_t sq_bits  = q->config.sq_bits;
    int32_t dims     = q->config.dims;
    int32_t levels   = (1 << sq_bits) - 1;
    float global_min = q->global_min;
    float scale      = q->scale;

    if (sq_bits == 8) {
        /* 8-bit: 每维度正好一字节，无需打包 */
        for (d = 0; d < dims; ++d) {
            int32_t val = (int32_t)roundf((vector[d] - global_min) / scale);
            if (val < 0) {
                val = 0;
            } else if (val > levels) {
                val = levels;
            }
            code[d] = (uint8_t)val;
        }
    } else {
        /* 4-bit: 每 2 维度打包为 1 字节 */
        /* 或其他情况 */
        int32_t bits_per_dim = sq_bits;
        int32_t dims_per_byte = 8 / bits_per_dim;

        for (d = 0; d < dims; ++d) {
            int32_t val = (int32_t)roundf((vector[d] - global_min) / scale);
            if (val < 0) {
                val = 0;
            } else if (val > levels) {
                val = levels;
            }

            /* 打包到字节中 */
            {
                int32_t byte_pos  = d / dims_per_byte;
                int32_t bit_offset = (d % dims_per_byte) * bits_per_dim;
                code[byte_pos] |= ((uint8_t)val & ((1 << bits_per_dim) - 1)) << bit_offset;
            }
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 编码
 * ────────────────────────────────────────────────────────────────────────── */

int32_t sq_encode(const quantizer_t *q, const float *vector, uint8_t *code)
{
    if (!q || !q->trained || !vector || !code) {
        return -1;
    }
    sq_encode_bits(q, vector, code);
    return 0;
}

int32_t sq_encode_batch(const quantizer_t *q, int32_t n, const float *vectors, uint8_t *codes)
{
    int32_t i;
    int32_t cs;

    if (!q || n <= 0 || !vectors || !codes) {
        return -1;
    }

    cs = sq_code_size(q);
    for (i = 0; i < n; ++i) {
        sq_encode_bits(q, &vectors[i * q->config.dims], &codes[i * cs]);
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 距离计算
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * SQ 的距离表就是查询向量本身
 * 对于 SQ，我们直接把查询向量存入距离表，
 * 在 adc_distance 时重构近似向量并计算距离
 */
int32_t sq_compute_distance_table(const quantizer_t *q,
                                   distance_metric_t metric,
                                   const float *query,
                                   float *table)
{
    int32_t d;

    if (!q || !q->trained || !query || !table) {
        return -1;
    }

    /* SQ 的"距离表"实际存储查询向量，用于后续重构 */
    for (d = 0; d < q->config.dims; ++d) {
        table[d] = query[d];
    }

    (void)metric; /* 未使用，SQ 只有 L2 */
    return 0;
}

float sq_adc_distance(const quantizer_t *q, const uint8_t *code, const float *table)
{
    float dist = 0.0f;
    int32_t d;
    int32_t dims     = q->config.dims;
    int32_t sq_bits  = q->config.sq_bits;
    float global_min = q->global_min;
    float scale      = q->scale;

    if (!q || !q->trained || !code || !table) {
        return FLT_MAX;
    }

    if (sq_bits == 8) {
        /* 8-bit: 每维度一字节，直接解码 */
        for (d = 0; d < dims; ++d) {
            float approx = global_min + scale * (float)code[d];
            float diff   = table[d] - approx;
            dist += diff * diff;
        }
    } else {
        /* 4-bit: 需要从打包的字节中提取 */
        int32_t levels      = (1 << sq_bits) - 1;
        int32_t dims_per_byte = 8 / sq_bits;

        for (d = 0; d < dims; ++d) {
            int32_t byte_pos   = d / dims_per_byte;
            int32_t bit_offset = (d % dims_per_byte) * sq_bits;
            int32_t val        = (code[byte_pos] >> bit_offset) & levels;

            float approx = global_min + scale * (float)val;
            float diff   = table[d] - approx;
            dist += diff * diff;
        }
    }

    return dist;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 尺寸查询
 * ────────────────────────────────────────────────────────────────────────── */

int32_t sq_code_size(const quantizer_t *q)
{
    int32_t dims    = q->config.dims;
    int32_t sq_bits = q->config.sq_bits;

    if (sq_bits == 8) {
        /* 8-bit: 每维度一字节 */
        return 8 + dims; /* 8 字节头部 (min/max 各 4 字节) + 数据 */
    } else {
        /* 其他 bit 数：向上取整 */
        return 8 + (dims * sq_bits + 7) / 8;
    }
}

int32_t sq_distance_table_size(const quantizer_t *q)
{
    return q ? q->config.dims : 0;
}

int32_t sq_model_float_count(const quantizer_t *q)
{
    (void)q;
    return 2; /* global_min, scale */
}

/* ──────────────────────────────────────────────────────────────────────────
 * 模型导入/导出
 * ────────────────────────────────────────────────────────────────────────── */

int32_t sq_export_model(const quantizer_t *q, float *params)
{
    if (!q || !q->trained || !params) {
        return -1;
    }
    params[0] = q->global_min;
    params[1] = q->scale;
    return 2;
}

int32_t sq_load_model(quantizer_t *q, const float *params)
{
    if (!q || !params) {
        return -1;
    }
    q->global_min = params[0];
    q->global_max = params[0] + params[1] * ((1 << q->config.sq_bits) - 1);
    q->scale      = params[1];
    q->trained    = true;
    return 0;
}
