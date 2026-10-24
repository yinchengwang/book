#include "lvq.h"

/*
 * LVQ（Locally-adaptive Vector Quantization）实现
 *
 * 核心思路
 * ─────────
 * 对每个向量独立执行标量量化：
 *   1. 扫描各分量，求 min（bias）和 max
 *   2. scale = (max - min) / (2^bits - 1)
 *      特例：若向量各分量相同（range ≈ 0），scale = 1，bias = min，所有码字写 0
 *   3. 各分量量化：code[i] = clamp( round((x[i] - bias) / scale), 0, max_code )
 *   4. 码字布局：[float32 scale (4B)][float32 bias (4B)][量化分量...]
 *
 * 4-bit 模式打包方式
 * ───────────────────
 *   每字节存两个 4-bit 量化值：低半字节存偶数分量，高半字节存奇数分量
 *   byte[i/2]:  低4位 = code[2i]，高4位 = code[2i+1]
 *
 * ADC 距离（非对称距离计算）
 * ───────────────────────────
 *   distance_table 中存储查询向量的原始浮点值（dims 个 float）；
 *   adc_distance 从 code 中还原向量近似值，计算 L2 距离。
 *   对于内积 / 余弦距离，调用方可在调用前对查询向量归一化。
 */

/* ──────────────────────────────────────────────────────────────────────────
 * 内部常量 / 辅助
 * ────────────────────────────────────────────────────────────────────────── */

/* 码字头部大小：4 字节 scale + 4 字节 bias */
#define LVQ_HEADER_BYTES 8

static int32_t lvq_max_code(const quantizer_t *q)
{
    return (1 << q->config.lvq_bits) - 1;
}

/* 量化分量字节数（不含 header） */
static int32_t lvq_payload_bytes(const quantizer_t *q)
{
    if (q->config.lvq_bits == 4) {
        return (q->config.dims + 1) / 2;
    }
    return q->config.dims; /* 8-bit */
}

static void lvq_store_float(uint8_t *dst, float v)
{
    memcpy(dst, &v, sizeof(float));
}

static float lvq_load_float(const uint8_t *src)
{
    float v;
    memcpy(&v, src, sizeof(float));
    return v;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 生命周期
 * ────────────────────────────────────────────────────────────────────────── */

void lvq_init(quantizer_t *q)
{
    /*
     * LVQ 无需训练和额外内存分配；
     * 初始化后即可编码，trained 在 quantizer_create 中保持 false，
     * train 调用后（或直接）置 true。
     */
    q->trained = true; /* LVQ 无需训练，创建即可用 */
}

void lvq_reset(quantizer_t *q)
{
    q->trained = true; /* 重置后同样立即可用 */
}

/* ──────────────────────────────────────────────────────────────────────────
 * 尺寸查询
 * ────────────────────────────────────────────────────────────────────────── */

int32_t lvq_code_size(const quantizer_t *q)
{
    return LVQ_HEADER_BYTES + lvq_payload_bytes(q);
}

int32_t lvq_distance_table_size(const quantizer_t *q)
{
    return q->config.dims; /* 存储查询向量的 dims 个 float */
}

/* ──────────────────────────────────────────────────────────────────────────
 * 编码：生成 [scale][bias][量化分量...] 的码字
 * ────────────────────────────────────────────────────────────────────────── */

int32_t lvq_encode(const quantizer_t *q, const float *vector, uint8_t *code)
{
    int32_t dims    = q->config.dims;
    int32_t bits    = q->config.lvq_bits;
    int32_t max_c   = lvq_max_code(q);
    float min_val   = FLT_MAX;
    float max_val   = -FLT_MAX;
    float range;
    float scale;
    float bias;
    uint8_t *payload = code + LVQ_HEADER_BYTES;
    int32_t i;

    /* 1. 求当前向量的 min / max */
    for (i = 0; i < dims; ++i) {
        if (vector[i] < min_val) min_val = vector[i];
        if (vector[i] > max_val) max_val = vector[i];
    }

    /* 2. 计算 scale 和 bias */
    range = max_val - min_val;
    if (range < FLT_MIN) {
        scale = 1.0f;
    } else {
        scale = range / (float)max_c;
    }
    bias = min_val;

    lvq_store_float(code + 0, scale);
    lvq_store_float(code + 4, bias);

    /* 3. 量化各分量 */
    if (bits == 8) {
        for (i = 0; i < dims; ++i) {
            float norm = (vector[i] - bias) / scale;
            int32_t q_val = (int32_t)(norm + 0.5f);
            if (q_val < 0)      q_val = 0;
            if (q_val > max_c)  q_val = max_c;
            payload[i] = (uint8_t)q_val;
        }
    } else {
        /* 4-bit：每字节两个分量，低半字节存偶数索引 */
        int32_t num_bytes = lvq_payload_bytes(q);
        memset(payload, 0, (size_t)num_bytes);
        for (i = 0; i < dims; ++i) {
            float norm = (vector[i] - bias) / scale;
            int32_t q_val = (int32_t)(norm + 0.5f);
            if (q_val < 0)      q_val = 0;
            if (q_val > max_c)  q_val = max_c;
            if (i % 2 == 0) {
                payload[i / 2] = (uint8_t)(q_val & 0x0F);
            } else {
                payload[i / 2] |= (uint8_t)((q_val & 0x0F) << 4);
            }
        }
    }

    return 0;
}

int32_t lvq_encode_batch(const quantizer_t *q,
                               int32_t n,
                               const float *vectors,
                               uint8_t *codes)
{
    int32_t i;
    int32_t cs = lvq_code_size(q);

    for (i = 0; i < n; ++i) {
        if (lvq_encode(q, &vectors[i * q->config.dims], &codes[i * cs]) != 0) {
            return -1;
        }
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * ADC 距离
 * ────────────────────────────────────────────────────────────────────────── */

/*
 * compute_distance_table：将查询向量存入 table，供 adc_distance 使用。
 * 若 metric 为余弦距离，调用方应在调用前对 query 正规化。
 */
int32_t lvq_compute_distance_table(const quantizer_t *q,
                                         distance_metric_t metric,
                                         const float *query,
                                         float *table)
{
    (void)metric; /* LVQ 在 adc_distance 时才使用度量 */
    memcpy(table, query, (size_t)q->config.dims * sizeof(float));
    return 0;
}

/*
 * adc_distance：解码向量近似值，计算与查询向量的 L2 距离。
 *
 * 解码公式：x_approx[i] = bias + scale * code[i]
 * 距离公式：sum( (query[i] - x_approx[i])^2 )
 */
float lvq_adc_distance(const quantizer_t *q,
                              const uint8_t *code,
                              const float *distance_table)
{
    const float *query   = distance_table;
    int32_t dims         = q->config.dims;
    int32_t bits         = q->config.lvq_bits;
    const uint8_t *payload = code + LVQ_HEADER_BYTES;
    float scale          = lvq_load_float(code + 0);
    float bias           = lvq_load_float(code + 4);
    float dist           = 0.0f;
    int32_t i;

    if (bits == 8) {
        for (i = 0; i < dims; ++i) {
            float approx = bias + scale * (float)payload[i];
            float diff   = query[i] - approx;
            dist += diff * diff;
        }
    } else {
        for (i = 0; i < dims; ++i) {
            int32_t q_val;
            float approx;
            float diff;

            if (i % 2 == 0) {
                q_val = (int32_t)(payload[i / 2] & 0x0F);
            } else {
                q_val = (int32_t)((payload[i / 2] >> 4) & 0x0F);
            }
            approx = bias + scale * (float)q_val;
            diff   = query[i] - approx;
            dist  += diff * diff;
        }
    }

    return dist;
}
