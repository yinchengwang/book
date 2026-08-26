/**
 * @file bq.c
 * @brief BQ (Binary Quantization) 二值化量化实现
 *
 * 核心算法：
 * 1. 阈值选择：训练时计算每维的均值/中位数作为阈值
 * 2. 编码：每维根据阈值转为 0/1
 * 3. 存储：每维 1 bit，多维打包成字节
 * 4. 距离：Hamming 距离 = popcount(XOR)
 *
 * 性能特性：
 * - 编码：O(n) 线性扫描
 * - 距离计算：O(k) 其中 k=code_size，利用 popcount 硬件加速
 * - 内存压缩：32x（float32 → bit）
 */

#include "db/index/vector_index/bq/bq.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ============================================================
 * 宏定义
 * ============================================================ */

#define BQ_SAFE_free(ptr) do { if ((ptr)) { free(ptr); (ptr) = NULL; } } while(0)

/* ============================================================
 * 生命周期管理
 * ============================================================ */

bq_quantizer_t *bq_create(int dims, BqThresholdStrategy_t strategy)
{
    if (dims <= 0 || dims > BQ_MAX_DIMS) {
        return NULL;
    }

    bq_quantizer_t *bq = (bq_quantizer_t *)calloc(1, sizeof(bq_quantizer_t));
    if (!bq) {
        return NULL;
    }

    bq->dims = dims;
    bq->strategy = strategy;

    /* 分配阈值和均值数组 */
    bq->thresholds = (float *)calloc(dims, sizeof(float));
    bq->means = (float *)calloc(dims, sizeof(float));

    if (!bq->thresholds || !bq->means) {
        BQ_SAFE_free(bq->thresholds);
        BQ_SAFE_free(bq->means);
        free(bq);
        return NULL;
    }

    bq->trained = 0;
    bq->n_samples = 0;
    bq->training_time_ms = 0.0f;

    return bq;
}

void bq_destroy(bq_quantizer_t *bq)
{
    if (!bq) {
        return;
    }

    BQ_SAFE_free(bq->thresholds);
    BQ_SAFE_free(bq->means);
    free(bq);
}

/* ============================================================
 * 训练与编码
 * ============================================================ */

/**
 * @brief 计算数组的中位数
 */
static float compute_median(float *arr, int n)
{
    if (n <= 0) return 0.0f;

    /* 复制数组以排序 */
    float *sorted = (float *)malloc(n * sizeof(float));
    if (!sorted) return 0.0f;

    memcpy(sorted, arr, n * sizeof(float));

    /* 简单排序（对于小数据集足够） */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (sorted[i] > sorted[j]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    float median;
    if (n % 2 == 0) {
        median = (sorted[n/2 - 1] + sorted[n/2]) / 2.0f;
    } else {
        median = sorted[n/2];
    }

    free(sorted);
    return median;
}

int bq_train(bq_quantizer_t *bq, int n, const float *vectors)
{
    if (!bq || !vectors || n <= 0) {
        return -1;
    }

    if (bq->dims <= 0) {
        return -1;
    }

    /* 临时缓冲区：存储每维的所有值用于计算中位数 */
    float *dim_values = (float *)malloc(n * sizeof(float));
    if (!dim_values) {
        return -1;
    }

    /* 统计耗时开始 */
    uint64_t start_time = 0;
#ifdef _WIN32
    start_time = GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif

    /* 遍历每维，计算阈值 */
    for (int d = 0; d < bq->dims; d++) {
        /* 收集该维的所有值 */
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            float val = vectors[i * bq->dims + d];
            dim_values[i] = val;
            sum += val;
        }

        /* 计算均值 */
        float mean = (float)(sum / n);
        bq->means[d] = mean;

        /* 根据策略选择阈值 */
        switch (bq->strategy) {
            case BQ_THRESHOLD_MEAN:
                bq->thresholds[d] = mean;
                break;

            case BQ_THRESHOLD_MEDIAN:
            case BQ_THRESHOLD_ADAPTIVE:
                bq->thresholds[d] = compute_median(dim_values, n);
                break;

            case BQ_THRESHOLD_LEARNED:
                /* 学习阈值需要额外处理，这里先用均值 */
                bq->thresholds[d] = mean;
                break;
        }
    }

    /* 统计耗时结束 */
#ifdef _WIN32
    bq->training_time_ms = (float)(GetTickCount64() - start_time);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t end_time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    bq->training_time_ms = (float)(end_time - start_time);
#endif

    bq->trained = 1;
    bq->n_samples = n;

    free(dim_values);
    return 0;
}

int bq_encode(const bq_quantizer_t *bq, const float *vector, uint8_t *code)
{
    if (!bq || !vector || !code) {
        return -1;
    }

    if (!bq->trained) {
        return -1;
    }

    int code_size = BQ_CODE_SIZE(bq->dims);

    /* 逐维编码，打包成位向量 */
    memset(code, 0, code_size);

    for (int d = 0; d < bq->dims; d++) {
        int byte_idx = d / 8;
        int bit_idx = d % 8;

        /* 阈值判断：vector[d] >= threshold[d] → 1 : 0 */
        if (vector[d] >= bq->thresholds[d]) {
            code[byte_idx] |= (1 << bit_idx);
        }
    }

    return 0;
}

int bq_encode_batch(const bq_quantizer_t *bq, int n,
                    const float *vectors, uint8_t *codes)
{
    if (!bq || !vectors || !codes || n <= 0) {
        return -1;
    }

    if (!bq->trained) {
        return -1;
    }

    int encoded = 0;
    for (int i = 0; i < n; i++) {
        if (bq_encode(bq, &vectors[i * bq->dims],
                     &codes[i * BQ_CODE_SIZE(bq->dims)]) == 0) {
            encoded++;
        }
    }

    return encoded;
}

int bq_decode(const bq_quantizer_t *bq, const uint8_t *code,
              float *vector, float value_if_0, float value_if_1)
{
    if (!bq || !code || !vector) {
        return -1;
    }

    for (int d = 0; d < bq->dims; d++) {
        int byte_idx = d / 8;
        int bit_idx = d % 8;

        /* 提取位值 */
        int bit = (code[byte_idx] >> bit_idx) & 1;

        /* 根据位值恢复原始值 */
        vector[d] = bit ? value_if_1 : value_if_0;
    }

    return 0;
}

/* ============================================================
 * 学习阈值（可选扩展）
 * ============================================================ */

bq_learned_thresholds_t *bq_learn_thresholds(int dims, int n,
                                              const float *vectors,
                                              int max_iter)
{
    if (dims <= 0 || n <= 0 || !vectors || max_iter <= 0) {
        return NULL;
    }

    bq_learned_thresholds_t *lt = (bq_learned_thresholds_t *)
        calloc(1, sizeof(bq_learned_thresholds_t));
    if (!lt) {
        return NULL;
    }

    lt->dims = dims;
    lt->thresholds = (float *)calloc(dims, sizeof(float));
    if (!lt->thresholds) {
        free(lt);
        return NULL;
    }

    /* 初始化阈值为均值 */
    for (int d = 0; d < dims; d++) {
        double sum = 0.0;
        for (int i = 0; i < n; i++) {
            sum += vectors[i * dims + d];
        }
        lt->thresholds[d] = (float)(sum / n);
    }

    /* 迭代优化（简化版：使用 one-dimensional k-Means） */
    for (int iter = 0; iter < max_iter; iter++) {
        double total_error = 0.0;

        /* 计算每个簇的均值作为新阈值 */
        float *sum0 = (float *)calloc(dims, sizeof(float));
        float *sum1 = (float *)calloc(dims, sizeof(float));
        int *cnt0 = (int *)calloc(dims, sizeof(int));
        int *cnt1 = (int *)calloc(dims, sizeof(int));

        if (!sum0 || !sum1 || !cnt0 || !cnt1) {
            BQ_SAFE_free(sum0);
            BQ_SAFE_free(sum1);
            BQ_SAFE_free(cnt0);
            BQ_SAFE_free(cnt1);
            break;
        }

        for (int i = 0; i < n; i++) {
            for (int d = 0; d < dims; d++) {
                float val = vectors[i * dims + d];
                if (val >= lt->thresholds[d]) {
                    sum1[d] += val;
                    cnt1[d]++;
                } else {
                    sum0[d] += val;
                    cnt0[d]++;
                }
            }
        }

        /* 更新阈值 */
        for (int d = 0; d < dims; d++) {
            float old_th = lt->thresholds[d];
            float new_th;

            if (cnt0[d] > 0 && cnt1[d] > 0) {
                new_th = (sum0[d] / cnt0[d] + sum1[d] / cnt1[d]) / 2.0f;
            } else if (cnt0[d] > 0) {
                new_th = sum0[d] / cnt0[d];
            } else {
                new_th = sum1[d] / cnt1[d];
            }

            lt->thresholds[d] = new_th;
            total_error += fabs(new_th - old_th);
        }

        BQ_SAFE_free(sum0);
        BQ_SAFE_free(sum1);
        BQ_SAFE_free(cnt0);
        BQ_SAFE_free(cnt1);

        /* 收敛检测 */
        if (total_error < 1e-6) {
            break;
        }
    }

    lt->trained = 1;
    return lt;
}

void bq_learned_thresholds_destroy(bq_learned_thresholds_t *lt)
{
    if (!lt) return;
    BQ_SAFE_free(lt->thresholds);
    free(lt);
}

/* ============================================================
 * 距离计算
 * ============================================================ */

int bq_hamming_distance(const uint8_t *code1, const uint8_t *code2,
                         int code_size)
{
    if (!code1 || !code2 || code_size <= 0) {
        return -1;
    }

    int hamming = 0;

    /* 按 64 位块处理 */
    const uint64_t *p64_1 = (const uint64_t *)code1;
    const uint64_t *p64_2 = (const uint64_t *)code2;
    int n64 = code_size / 8;

    for (int i = 0; i < n64; i++) {
        uint64_t xor_val = p64_1[i] ^ p64_2[i];
        hamming += bq_popcount64(xor_val);
    }

    /* 处理剩余字节：逐字节计算 popcount */
    int remainder = code_size % 8;
    if (remainder > 0) {
        int start = n64 * 8;
        for (int i = 0; i < remainder; i++) {
            uint8_t xor_val = code1[start + i] ^ code2[start + i];
            hamming += bq_popcount32(xor_val);
        }
    }

    return hamming;
}

int bq_hamming_batch(const uint8_t *code, const uint8_t *codes,
                     int n, int code_size, int *distances)
{
    if (!code || !codes || n <= 0 || code_size <= 0) {
        return -1;
    }

    int best_idx = 0;
    int best_dist = INT_MAX;

    for (int i = 0; i < n; i++) {
        const uint8_t *curr_code = &codes[i * code_size];
        int dist = bq_hamming_distance(code, curr_code, code_size);

        if (dist >= 0 && dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }

        if (distances) {
            distances[i] = dist;
        }
    }

    return best_idx;
}

void bq_compute_distance_table(const uint8_t *query_code, int code_size,
                               uint16_t *table)
{
    if (!query_code || !table || code_size <= 0) {
        return;
    }

    /* 为每个字节位置和每个可能的字节值计算距离 */
    for (int byte_idx = 0; byte_idx < code_size; byte_idx++) {
        uint8_t qb = query_code[byte_idx];

        for (int val = 0; val < 256; val++) {
            uint8_t xor_val = qb ^ (uint8_t)val;
            /* 8 位 popcount：异或后 1 的个数 */
            table[byte_idx * 256 + val] = (uint16_t)bq_popcount32(xor_val);
        }
    }
}

int bq_distance_from_table(const uint16_t *table, int code_size,
                           const uint8_t *target_code)
{
    if (!table || !target_code || code_size <= 0) {
        return -1;
    }

    int total_dist = 0;

    for (int byte_idx = 0; byte_idx < code_size; byte_idx++) {
        int val = target_code[byte_idx];
        total_dist += table[byte_idx * 256 + val];
    }

    return total_dist;
}

/* ============================================================
 * 状态查询
 * ============================================================ */

int bq_code_size(int dims)
{
    return BQ_CODE_SIZE(dims);
}

int bq_get_code_size(const bq_quantizer_t *bq)
{
    return bq ? BQ_CODE_SIZE(bq->dims) : 0;
}

int bq_is_trained(const bq_quantizer_t *bq)
{
    return bq ? bq->trained : 0;
}

float bq_compression_ratio(int dims)
{
    if (dims <= 0) return 0.0f;

    /* float32: dims * 4 字节
     * bit:     dims / 8 字节
     * 压缩率 = 32 */
    return (float)(dims * 4 * 8) / (float)dims;  /* = 32 */
}

float bq_get_threshold(const bq_quantizer_t *bq, int dim)
{
    if (!bq || dim < 0 || dim >= bq->dims) {
        return 0.0f;
    }
    return bq->thresholds[dim];
}

void bq_print_info(const bq_quantizer_t *bq)
{
    if (!bq) {
        printf("BQ Quantizer: NULL\n");
        return;
    }

    const char *strategy_name;
    switch (bq->strategy) {
        case BQ_THRESHOLD_MEAN:     strategy_name = "MEAN"; break;
        case BQ_THRESHOLD_MEDIAN:  strategy_name = "MEDIAN"; break;
        case BQ_THRESHOLD_ADAPTIVE: strategy_name = "ADAPTIVE"; break;
        case BQ_THRESHOLD_LEARNED: strategy_name = "LEARNED"; break;
        default:                    strategy_name = "UNKNOWN"; break;
    }

    printf("BQ Quantizer Info:\n");
    printf("  Dims: %d\n", bq->dims);
    printf("  Code size: %d bytes\n", bq_get_code_size(bq));
    printf("  Strategy: %s\n", strategy_name);
    printf("  Trained: %s\n", bq->trained ? "YES" : "NO");
    printf("  Samples: %lld\n", (long long)bq->n_samples);
    printf("  Training time: %.2f ms\n", bq->training_time_ms);
    printf("  Compression ratio: %.2fx\n", bq_compression_ratio(bq->dims));

    if (bq->trained) {
        printf("  Threshold range: [%.4f, %.4f]\n",
               bq->thresholds[0], bq->thresholds[bq->dims - 1]);
    }
}
