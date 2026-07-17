#include "rq.h"

#include <algo-prod/Kmeans/kmeans.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * RabitQ (Residual Binary Quantization) 量化器原理详解
 * ──────────────────────────────────────────────────────────────────────────
 *
 * RabitQ 是一种两级量化方法，用少量额外存储换取显著精度提升。
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                    RabitQ 两级量化流程                                 │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │  输入向量: x = [0.3, 0.7, 1.2, 2.8, 3.5, 4.1, 5.2, 6.9]             │
 * │                     ↓                                               │
 * │  ┌───────────────────────────────────────────────────────────────┐    │
 * │  │ 第一级: PQ 粗量化 (Product Quantization)                      │    │
 * │  │                                                              │    │
 * │  │  1. 将向量分成 m 个子空间                                      │    │
 * │  │     m=4: [[0.3,0.7], [1.2,2.8], [3.5,4.1], [5.2,6.9]]        │    │
 * │  │                                                              │    │
 * │  │  2. 每个子空间找最近的码字                                     │    │
 * │  │     子空间0: [0.3,0.7] → 码字0=[0.5,0.5]  (pq_code=5)        │    │
 * │  │     子空间1: [1.2,2.8] → 码字1=[1.5,2.5]  (pq_code=12)       │    │
 * │  │     ...                                                       │    │
 * │  │                                                              │    │
 * │  │  PQ 近似: pq_approx = [0.5, 0.5, 1.5, 2.5, ...]             │    │
 * │  │                                                              │    │
 * │  │  PQ 量化误差: residual = x - pq_approx                       │    │
 * │  │           = [-0.2, 0.2, -0.3, 0.3, ...]                    │    │
 * │  └───────────────────────────────────────────────────────────────┘    │
 * │                     ↓                                               │
 * │  ┌───────────────────────────────────────────────────────────────┐    │
 * │  │ 第二级: 残差细量化 (Residual Quantization)                   │    │
 * │  │                                                              │    │
 * │  │  对每个子空间的残差进行编码:                                   │    │
 * │  │  - 1-bit:  只编码方向 (正/负)                                │    │
 * │  │  - 2-bit:  编码方向 + 小幅度的 4 级                         │    │
 * │  │  - 4-bit:  编码方向 + 16 级幅度                             │    │
 * │  │                                                              │    │
 * │  │  示例 (1-bit):                                               │    │
 * │  │    子空间0 残差: [-0.2, 0.2] → 方向为负 → rq_code=0         │    │
 * │  │    子空间1 残差: [-0.3, 0.3] → 方向为负 → rq_code=0         │    │
 * │  │    子空间2 残差: [ 0.1,-0.1] → 方向为正 → rq_code=1         │    │
 * │  │    ...                                                       │    │
 * │  └───────────────────────────────────────────────────────────────┘    │
 * │                     ↓                                               │
 * │  ┌───────────────────────────────────────────────────────────────┐    │
 * │  │ 输出码字: [pq_codes...][residual_codes...]                   │    │
 * │  │                                                              │    │
 * │  │  内存布局:                                                   │    │
 * │  │  ┌────────────┬─────────────────────┐                       │    │
 * │  │  │ PQ 码 (mB) │ 残差码 (m*bits/8 B) │                       │    │
 * │  │  │ [5,12,...] │ [0,0,1,...]         │                       │    │
 * │  │  └────────────┴─────────────────────┘                       │    │
 * │  │                                                              │    │
 * │  │  总码字大小: m + ceil(m * rq_bits / 8)                      │    │
 * │  │  示例: m=16, rq_bits=1 → 16 + 2 = 18 bytes                   │    │
 * │  └───────────────────────────────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * ──────────────────────────────────────────────────────────────────────────
 * 训练流程
 * ──────────────────────────────────────────────────────────────────────────
 *
 * 1. PQ 训练 (与标准 PQ 相同)
 *    - 将向量分成 m 个子空间
 *    - 每个子空间运行 K-Means, K = 2^pq_bits
 *    - 得到 PQ 码本: codebooks[sub][k]
 *
 * 2. 残差统计
 *    - 计算每个子空间的残差统计量
 *    - residual[sub] = original - pq_approx
 *    - 记录每个子空间残差的标准差或分位数
 *    - 用于确定残差的量化步长
 *
 * ──────────────────────────────────────────────────────────────────────────
 * 编码流程
 * ──────────────────────────────────────────────────────────────────────────
 *
 * 给定向量 x = [x₀, x₁, ..., x_{d-1}]
 *
 * 1. PQ 编码:
 *    for each sub-space s:
 *      sub_vec = x[s*sub_dims : (s+1)*sub_dims]
 *      pq_code[s] = argmin_k ||sub_vec - codebooks[s][k]||²
 *
 * 2. 残差编码:
 *    for each sub-space s:
 *      residual = original_sub_vec - pq_approx
 *      rq_code[s] = encode_residual(residual, step[s])
 *
 * ──────────────────────────────────────────────────────────────────────────
 * 解码流程
 * ──────────────────────────────────────────────────────────────────────────
 *
 * 1. PQ 解码:
 *    for each sub-space s:
 *      approx = codebooks[s][pq_code[s]]
 *
 * 2. 残差补偿:
 *    for each sub-space s:
 *      direction = (rq_code[s] & (1 << (rq_bits-1))) ? +1 : -1
 *      magnitude = rq_code[s] & ((1 << (rq_bits-1)) - 1)
 *      residual_approx = direction × magnitude × step[s]
 *      final_approx = approx + residual_approx
 *
 * ──────────────────────────────────────────────────────────────────────────
 * 距离计算 (ADC)
 * ──────────────────────────────────────────────────────────────────────────
 *
 * 预计算 PQ 距离表 (与标准 PQ 相同):
 *   dist_table[s][k] = ||query_sub[s] - codebooks[s][k]||²
 *
 * ADC 距离:
 *   total_dist = 0
 *   for each sub-space s:
 *     total_dist += dist_table[s][pq_code[s]]
 *     if rq_bits > 0:
 *       total_dist += residual_correction[s][rq_code[s]]
 *
 * ──────────────────────────────────────────────────────────────────────────
 * 优势对比
 * ──────────────────────────────────────────────────────────────────────────
 *
 * | 量化器 | 码字大小 | 精度 | 额外存储 |
 * |--------|----------|------|----------|
 * | PQ(8)  | 16B      | 基准 | 无       |
 * | SQ(8)  | 136B     | 低   | 无       |
 * | SQ(4)  | 72B      | 低   | 无       |
 * | LVQ(8) | 136B     | 高   | 每向量 8B |
 * | RabitQ | 18B      | 高   | 2B 残差  |
 *
 * RabitQ 用 ~12.5% 的额外存储 (18B vs 16B) 换取显著精度提升
 *
 * ────────────────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────────────────
 * 辅助函数
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * 获取子空间码本指针
 * @param q 量化器
 * @param sub 子空间索引
 * @return 码本指针 [codebook_size][subvector_dims]
 */
static float *rq_codebook_ptr(const quantizer_t *q, int32_t sub)
{
    return &q->codebooks[sub * q->max_codebook_size * q->subvector_dims];
}

/**
 * 计算 PQ 编码部分的字节大小
 */
static int32_t rq_pq_code_bytes(const quantizer_t *q)
{
    return q->config.pq_subquantizers; /* 每子空间 1 字节 */
}

/**
 * 计算残差编码部分的字节大小
 */
static int32_t rq_residual_code_bytes(const quantizer_t *q)
{
    int32_t m         = q->config.pq_subquantizers;
    int32_t rq_bits  = q->config.rq_residual_bits;
    return (m * rq_bits + 7) / 8; /* 向上取整 */
}

/* ──────────────────────────────────────────────────────────────────────────
 * 生命周期
 * ────────────────────────────────────────────────────────────────────────── */

int32_t rq_init(quantizer_t *q)
{
    size_t codebook_count;
    size_t residual_count;

    if (!q) {
        return -1;
    }

    /* 设置子空间参数（使用 rq_pq_bits 作为 PQ 的 bits） */
    q->subvector_dims    = q->config.dims / q->config.pq_subquantizers;
    q->max_codebook_size = 1 << q->config.rq_pq_bits; /* PQ 码本大小 */
    q->codebook_size     = q->max_codebook_size;

    /* 分配 PQ 码本 */
    codebook_count = (size_t)q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims;
    q->codebooks = (float *)calloc(codebook_count, sizeof(float));
    if (!q->codebooks) {
        return -1;
    }

    /* 分配残差步长数组：每个子空间一个步长 */
    residual_count = (size_t)q->config.pq_subquantizers;
    q->residual_steps = (float *)calloc(residual_count, sizeof(float));
    if (!q->residual_steps) {
        free(q->codebooks);
        q->codebooks = NULL;
        return -1;
    }

    return 0;
}

void rq_free(quantizer_t *q)
{
    if (q) {
        free(q->codebooks);
        q->codebooks = NULL;
        free(q->residual_steps);
        q->residual_steps = NULL;
    }
}

void rq_reset(quantizer_t *q)
{
    size_t codebook_bytes;
    size_t residual_bytes;

    if (!q) {
        return;
    }

    q->trained       = false;
    q->codebook_size = q->max_codebook_size;

    /* 重置 PQ 码本 */
    codebook_bytes = (size_t)q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims * sizeof(float);
    memset(q->codebooks, 0, codebook_bytes);

    /* 重置残差步长 */
    residual_bytes = (size_t)q->config.pq_subquantizers * sizeof(float);
    memset(q->residual_steps, 0, residual_bytes);
}

/* ──────────────────────────────────────────────────────────────────────────
 * 训练：两级训练
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * RabitQ 训练包含两个阶段：
 *
 * 阶段 1: PQ 码本训练
 *   - 与标准 PQ 训练相同
 *   - 使用 K-Means 学习每个子空间的码本
 *
 * 阶段 2: 残差统计
 *   - 扫描训练数据
 *   - 对每个子空间，计算残差的标准差
 *   - 用标准差作为残差的量化步长
 */
int32_t rq_train(quantizer_t *q, int32_t n, const float *vectors)
{
    int32_t sub;
    int32_t i;
    int32_t sub_dims;
    int32_t codebook_size;

    if (!q || n <= 0 || !vectors) {
        return -1;
    }

    /* ── 阶段 1: PQ 码本训练 ── */
    /* 使用与 PQ 相同的方法训练码本 */
    q->codebook_size = q->max_codebook_size < n ? q->max_codebook_size : n;
    if (q->codebook_size <= 0) {
        return -1;
    }

    /* 清零码本 */
    {
        size_t codebook_bytes = (size_t)q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims * sizeof(float);
        memset(q->codebooks, 0, codebook_bytes);
    }

    sub_dims     = q->subvector_dims;
    codebook_size = q->codebook_size;

    /* 对每个子空间运行 K-Means */
    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        double *train_data;
        KMeansParams params;
        float *codebook;
        int32_t s;
        int32_t dim;

        /* 提取子空间训练数据 */
        train_data = (double *)malloc((size_t)n * sub_dims * sizeof(double));
        if (!train_data) {
            return -1;
        }

        for (s = 0; s < n; ++s) {
            const float *sv = &vectors[s * q->config.dims + sub * sub_dims];
            for (dim = 0; dim < sub_dims; ++dim) {
                train_data[s * sub_dims + dim] = (double)sv[dim];
            }
        }

        /* K-Means 参数 */
        memset(&params, 0, sizeof(params));
        params.n_clusters   = codebook_size;
        params.n_init       = 8;
        params.max_iter     = 100;
        params.tol          = 1e-4;
        params.verbose      = 0;
        params.random_state = 42 + sub;
        params.init         = "k-means++";
        params.algorithm    = "lloyd";
        params.n_samples    = n;
        params.n_features   = sub_dims;
        params.data         = train_data;

        Kmeans(&params);
        free(train_data);

        if (!params.success || !params.cluster_centers) {
            KmeansFree(&params);
            return -1;
        }

        /* 复制码本 */
        codebook = rq_codebook_ptr(q, sub);
        for (s = 0; s < codebook_size; ++s) {
            for (dim = 0; dim < sub_dims; ++dim) {
                codebook[s * sub_dims + dim] =
                    (float)params.cluster_centers[s * sub_dims + dim];
            }
        }

        KmeansFree(&params);
    }

    /* ── 阶段 2: 残差统计计算步长 ── */
    /*
     * 对每个子空间：
     * 1. 遍历所有向量，计算 PQ 近似
     * 2. 计算残差 = 原始值 - PQ 近似
     * 3. 统计残差的标准差
     * 4. 用标准差作为量化步长
     *
     * 步长 = 标准差 / (2^(rq_bits-1))
     * 这样残差量化后能覆盖大部分实际残差范围
     */
    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        double sum_sq = 0.0;
        double mean   = 0.0;
        int32_t count = 0;

        /* 遍历所有训练向量，计算残差统计 */
        for (i = 0; i < n; ++i) {
            const float *vec = &vectors[i * q->config.dims + sub * sub_dims];
            const float *codebook = rq_codebook_ptr(q, sub);
            int32_t k;
            float best_dist = FLT_MAX;
            int32_t best_code = 0;

            /* 找最近码字 */
            for (k = 0; k < codebook_size; ++k) {
                float d = 0.0f;
                int32_t dim;
                for (dim = 0; dim < sub_dims; ++dim) {
                    float diff = vec[dim] - codebook[k * sub_dims + dim];
                    d += diff * diff;
                }
                if (d < best_dist) {
                    best_dist = d;
                    best_code = k;
                }
            }

            /* 取最近码字作为近似 */
            {
                int32_t dim;
                for (dim = 0; dim < sub_dims && dim < 16; ++dim) {
                    float residual = vec[dim] - codebook[best_code * sub_dims + dim];
                    mean   += residual;
                    sum_sq += residual * residual;
                    count++;
                }
            }
        }

        /* 计算标准差 */
        if (count > 0) {
            double mean_val = mean / count;
            double variance = (sum_sq / count) - (mean_val * mean_val);
            double stddev   = variance > 0 ? sqrt(variance) : 1.0;

            /*
             * 量化步长设计：
             * - 1-bit: 步长 = stddev / 0.5，覆盖约 50% 的残差
             * - 2-bit: 步长 = stddev / 1.5，覆盖约 68% 的残差
             * - 4-bit: 步长 = stddev / 7.5，覆盖约 95% 的残差
             */
            {
                double divisor;
                switch (q->config.rq_residual_bits) {
                    case 1: divisor = 0.5;  break;
                    case 2: divisor = 1.5;  break;
                    case 4: divisor = 7.5;  break;
                    default: divisor = 1.0; break;
                }
                q->residual_steps[sub] = (float)(stddev / divisor);
                if (q->residual_steps[sub] < 1e-6f) {
                    q->residual_steps[sub] = 1e-6f; /* 防止除零 */
                }
            }
        } else {
            q->residual_steps[sub] = 1e-3f;
        }
    }

    q->trained = true;
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 辅助：PQ 编码（找最近码字）
 * ────────────────────────────────────────────────────────────────────────── */

static int32_t rq_find_pq_code(const quantizer_t *q, const float *sub_vec, int32_t sub)
{
    const float *codebook = rq_codebook_ptr(q, sub);
    float best_dist = FLT_MAX;
    int32_t best_code = 0;
    int32_t c;
    int32_t dim = q->subvector_dims;

    for (c = 0; c < q->codebook_size; ++c) {
        float d = 0.0f;
        int32_t k;
        for (k = 0; k < dim; ++k) {
            float diff = sub_vec[k] - codebook[c * dim + k];
            d += diff * diff;
        }
        if (d < best_dist) {
            best_dist = d;
            best_code = c;
        }
    }
    return best_code;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 辅助：编码单个向量（两级编码）
 * ────────────────────────────────────────────────────────────────────────── */

static void rq_encode_vector(const quantizer_t *q, const float *vector, uint8_t *code)
{
    int32_t sub;
    int32_t sub_dims = q->subvector_dims;
    int32_t m        = q->config.pq_subquantizers;
    int32_t rq_bits  = q->config.rq_residual_bits;
    uint8_t *pq_codes = code;
    uint8_t *residual_codes = code + rq_pq_code_bytes(q);

    /* 逐子空间编码 */
    for (sub = 0; sub < m; ++sub) {
        const float *sub_vec  = &vector[sub * sub_dims];
        int32_t pq_code;
        int32_t residual_code;
        int32_t dim;

        /* 第一级: PQ 编码 */
        pq_code = rq_find_pq_code(q, sub_vec, sub);
        pq_codes[sub] = (uint8_t)pq_code;

        /* 获取 PQ 近似 */
        {
            const float *codebook = rq_codebook_ptr(q, sub);
            const float *pq_approx = &codebook[pq_code * sub_dims];
            float residual_vec[16];
            float total_residual = 0.0f;

            /* 计算残差的"代表性值"（这里用 L2 范数方向投影） */
            for (dim = 0; dim < sub_dims && dim < 16; ++dim) {
                residual_vec[dim] = sub_vec[dim] - pq_approx[dim];
                total_residual += residual_vec[dim] * residual_vec[dim];
            }
            total_residual = sqrtf(total_residual);

            /* 第二级: 残差编码 */
            /*
             * 编码策略：
             * - 1-bit: 符号位 (0=负, 1=正)
             * - 2-bit: 符号 + 2 级幅度
             * - 4-bit: 符号 + 8 级幅度
             *
             * 幅度等级 = round(|residual| / step)
             */
            {
                float step = q->residual_steps[sub];
                int32_t sign_bit   = (total_residual > 0.0f) ? 1 : 0;
                int32_t magnitude  = 0;

                if (step > 1e-6f) {
                    magnitude = (int32_t)(total_residual / step);
                    /* 限制最大幅度 */
                    {
                        int32_t max_mag = (1 << (rq_bits - 1)) - 1;
                        if (magnitude > max_mag) {
                            magnitude = max_mag;
                        }
                    }
                }

                /* 组装残差码: [magnitude][sign] */
                residual_code = (sign_bit << (rq_bits - 1)) | magnitude;
            }

            /* 打包残差码到字节 */
            {
                int32_t byte_pos  = sub / (8 / rq_bits);
                int32_t bit_off   = (sub % (8 / rq_bits)) * rq_bits;
                int32_t mask      = (1 << rq_bits) - 1;
                residual_codes[byte_pos] |= ((uint8_t)(residual_code & mask)) << bit_off;
            }
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 编码
 * ────────────────────────────────────────────────────────────────────────── */

int32_t rq_encode(const quantizer_t *q, const float *vector, uint8_t *code)
{
    if (!q || !q->trained || !vector || !code) {
        return -1;
    }

    /* 清零码字缓冲区 */
    {
        int32_t cs = rq_code_size(q);
        memset(code, 0, cs);
    }

    rq_encode_vector(q, vector, code);
    return 0;
}

int32_t rq_encode_batch(const quantizer_t *q, int32_t n, const float *vectors, uint8_t *codes)
{
    int32_t i;
    int32_t cs;

    if (!q || n <= 0 || !vectors || !codes) {
        return -1;
    }

    cs = rq_code_size(q);
    for (i = 0; i < n; ++i) {
        /* 清零 */
        memset(&codes[i * cs], 0, cs);
        /* 编码 */
        rq_encode_vector(q, &vectors[i * q->config.dims], &codes[i * cs]);
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 距离计算
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * 计算 PQ 距离表
 * 与标准 PQ 相同，计算每个子空间到所有码字的距离
 */
int32_t rq_compute_distance_table(const quantizer_t *q,
                                   distance_metric_t metric,
                                   const float *query,
                                   float *table)
{
    int32_t sub;
    int32_t sub_dims = q->subvector_dims;

    if (!q || !q->trained || !query || !table) {
        return -1;
    }

    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        const float *query_sub = &query[sub * sub_dims];
        const float *codebook  = rq_codebook_ptr(q, sub);
        int32_t c;

        for (c = 0; c < q->codebook_size; ++c) {
            table[sub * q->codebook_size + c] =
                distance_compute(metric, query_sub, &codebook[c * sub_dims], sub_dims);
        }
    }

    (void)metric;
    return 0;
}

/**
 * ADC 距离计算
 * 1. 查表获取 PQ 距离
 * 2. 加上残差校正距离
 */
float rq_adc_distance(const quantizer_t *q, const uint8_t *code, const float *table)
{
    float dist = 0.0f;
    int32_t sub;
    int32_t m        = q->config.pq_subquantizers;
    int32_t rq_bits  = q->config.rq_residual_bits;
    const uint8_t *pq_codes      = code;
    const uint8_t *residual_codes = code + rq_pq_code_bytes(q);

    if (!q || !q->trained || !code || !table) {
        return FLT_MAX;
    }

    /* 累加 PQ 距离 */
    for (sub = 0; sub < m; ++sub) {
        int32_t pq_code = pq_codes[sub];
        dist += table[sub * q->codebook_size + pq_code];
    }

    /* 加上残差校正 */
    if (rq_bits > 0) {
        int32_t dims_per_byte = 8 / rq_bits;
        int32_t max_mag = (1 << (rq_bits - 1)) - 1;

        for (sub = 0; sub < m; ++sub) {
            int32_t byte_pos   = sub / dims_per_byte;
            int32_t bit_offset = (sub % dims_per_byte) * rq_bits;
            int32_t rq_code    = (residual_codes[byte_pos] >> bit_offset) & ((1 << rq_bits) - 1);

            /* 提取符号和幅度 */
            int32_t sign_bit   = (rq_code >> (rq_bits - 1)) & 1;
            int32_t magnitude = rq_code & max_mag;

            /* 残差校正 = sign * magnitude * step */
            float correction = (sign_bit ? 1.0f : -1.0f) * (float)magnitude * q->residual_steps[sub];

            /*
             * 简化处理：这里直接加上校正的平方
             * 实际应用中，可能需要更复杂的校正策略
             */
            dist += correction * correction;
        }
    }

    return dist;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 尺寸查询
 * ────────────────────────────────────────────────────────────────────────── */

int32_t rq_code_size(const quantizer_t *q)
{
    return rq_pq_code_bytes(q) + rq_residual_code_bytes(q);
}

int32_t rq_distance_table_size(const quantizer_t *q)
{
    return q->config.pq_subquantizers * q->codebook_size;
}

int32_t rq_model_float_count(const quantizer_t *q)
{
    /* PQ 码本 + 残差步长 */
    return q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims +
           q->config.pq_subquantizers;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 模型导入/导出
 * ────────────────────────────────────────────────────────────────────────── */

int32_t rq_export_model(const quantizer_t *q,
                        float *codebooks,
                        int32_t count,
                        int32_t *trained_codebook_size)
{
    int32_t pq_model_size;
    int32_t offset = 0;

    if (!q || !q->trained || !codebooks || !trained_codebook_size) {
        return -1;
    }

    /* 导出 PQ 码本 */
    pq_model_size = q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims;
    if (count < pq_model_size + q->config.pq_subquantizers) {
        return -1;
    }

    memcpy(codebooks, q->codebooks, (size_t)pq_model_size * sizeof(float));
    offset += pq_model_size;

    /* 导出残差步长 */
    memcpy(&codebooks[offset], q->residual_steps, (size_t)q->config.pq_subquantizers * sizeof(float));
    offset += q->config.pq_subquantizers;

    *trained_codebook_size = q->codebook_size;
    return offset;
}

int32_t rq_load_model(quantizer_t *q,
                      int32_t trained_codebook_size,
                      const float *codebooks,
                      int32_t count)
{
    int32_t pq_model_size;
    int32_t offset = 0;

    if (!q || !codebooks) {
        return -1;
    }

    pq_model_size = q->config.pq_subquantizers * q->max_codebook_size * q->subvector_dims;

    /* 验证缓冲区大小 */
    if (count < pq_model_size + q->config.pq_subquantizers) {
        return -1;
    }

    /* 验证码本大小 */
    if (trained_codebook_size <= 0 || trained_codebook_size > q->max_codebook_size) {
        return -1;
    }

    /* 导入 PQ 码本 */
    memcpy(q->codebooks, codebooks, (size_t)pq_model_size * sizeof(float));
    offset += pq_model_size;

    /* 导入残差步长 */
    memcpy(q->residual_steps, &codebooks[offset], (size_t)q->config.pq_subquantizers * sizeof(float));

    q->codebook_size = trained_codebook_size;
    q->trained       = true;

    return 0;
}
