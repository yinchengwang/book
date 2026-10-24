/**
 * @file analytics_sql.c
 * @brief Analytics SQL 方言支持实现
 *
 * 实现 DuckDB/ClickHouse 风格的分析函数，包括：
 * - 窗口函数（移动平均、排名、Lead/Lag 等）
 * - 表采样（Bernoulli、Reservoir）
 * - 近似计算（HyperLogLog、T-Digest）
 */
#include "db/analytics_sql.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ========================================================================
 * 内部宏定义
 * ======================================================================== */

#define HLL_DEFAULT_PRECISION 10      /**< 默认精度（1024 个桶） */
#define HLL_MAX_PRECISION 16          /**< 最大精度（65536 个桶） */
#define TDIGEST_DEFAULT_COMPRESSION 200.0  /**< 默认压缩参数 */
#define TDIGEST_MAX_CLUSTERS 2048     /**< 最大簇数量 */

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 获取随机数种子
 */
static uint32_t get_seed(uint32_t seed) {
    if (seed == 0) {
        return (uint32_t)time(NULL);
    }
    return seed;
}

/**
 * @brief 简易线性同余随机数生成器
 */
static uint32_t simple_rand(uint32_t *state) {
    *state = (*state) * 1664525u + 1013904223u;
    return *state;
}

/**
 * @brief 字符串哈希（FNV-1a）
 */
static uint64_t fnv1a_hash(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* ========================================================================
 * 窗口函数 - 初始化/销毁
 * ======================================================================== */

void analytic_window_init(analytic_window_t *window, analytic_func_type_t func_type) {
    if (window == NULL) return;
    memset(window, 0, sizeof(analytic_window_t));
    window->func_type = func_type;
    window->frame_start = FRAME_CURRENT_ROW;
    window->frame_end = FRAME_CURRENT_ROW;
}

void analytic_window_destroy(analytic_window_t *window) {
    if (window == NULL) return;
    for (int i = 0; i < window->partition_count; i++) {
        free(window->partition_by[i]);
        window->partition_by[i] = NULL;
    }
    for (int i = 0; i < window->order_count; i++) {
        free(window->order_by[i]);
        window->order_by[i] = NULL;
    }
    window->partition_count = 0;
    window->order_count = 0;
}

int analytic_window_add_partition(analytic_window_t *window, const char *column_name) {
    if (window == NULL || column_name == NULL) {
        LOG_ERROR("窗口函数添加分区列失败: 参数为空");
        return -1;
    }
    if (window->partition_count >= 16) {
        LOG_ERROR("窗口函数添加分区列失败: 分区列数量超限 (最大 16)");
        return -1;
    }
    char *copy = strdup(column_name);
    if (copy == NULL) {
        LOG_ERROR("窗口函数添加分区列失败: 内存分配失败");
        return -1;
    }
    window->partition_by[window->partition_count++] = copy;
    return 0;
}

int analytic_window_add_order(analytic_window_t *window, const char *column_name) {
    if (window == NULL || column_name == NULL) {
        LOG_ERROR("窗口函数添加排序列失败: 参数为空");
        return -1;
    }
    if (window->order_count >= 16) {
        LOG_ERROR("窗口函数添加排序列失败: 排序列数量超限 (最大 16)");
        return -1;
    }
    char *copy = strdup(column_name);
    if (copy == NULL) {
        LOG_ERROR("窗口函数添加排序列失败: 内存分配失败");
        return -1;
    }
    window->order_by[window->order_count++] = copy;
    return 0;
}

/* ========================================================================
 * 窗口函数 - 求值
 * ======================================================================== */

/**
 * @brief 移动平均实现
 */
static int eval_moving_avg(const float *values, uint32_t count, int frame_rows, float *results) {
    if (frame_rows <= 0) frame_rows = 1;
    for (uint32_t i = 0; i < count; i++) {
        double sum = 0.0;
        int n = 0;
        int start = (int)i - frame_rows + 1;
        if (start < 0) start = 0;
        for (int j = start; j <= (int)i; j++) {
            sum += values[j];
            n++;
        }
        results[i] = (n > 0) ? (float)(sum / n) : 0.0f;
    }
    return 0;
}

/**
 * @brief 移动求和实现
 */
static int eval_moving_sum(const float *values, uint32_t count, int frame_rows, float *results) {
    if (frame_rows <= 0) frame_rows = 1;
    for (uint32_t i = 0; i < count; i++) {
        double sum = 0.0;
        int start = (int)i - frame_rows + 1;
        if (start < 0) start = 0;
        for (int j = start; j <= (int)i; j++) {
            sum += values[j];
        }
        results[i] = (float)sum;
    }
    return 0;
}

/**
 * @brief Lead 函数实现
 */
static int eval_lead(const float *values, uint32_t count, int offset, float *results) {
    if (offset <= 0) offset = 1;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t target = i + (uint32_t)offset;
        results[i] = (target < count) ? values[target] : NAN;
    }
    return 0;
}

/**
 * @brief Lag 函数实现
 */
static int eval_lag(const float *values, uint32_t count, int offset, float *results) {
    if (offset <= 0) offset = 1;
    for (uint32_t i = 0; i < count; i++) {
        int source = (int)i - offset;
        results[i] = (source >= 0) ? values[source] : NAN;
    }
    return 0;
}

/**
 * @brief First Value 函数实现
 */
static int eval_first_value(const float *values, uint32_t count, float *results) {
    if (count == 0) return 0;
    float first = values[0];
    for (uint32_t i = 0; i < count; i++) {
        results[i] = first;
    }
    return 0;
}

/**
 * @brief Last Value 函数实现
 */
static int eval_last_value(const float *values, uint32_t count, float *results) {
    if (count == 0) return 0;
    float last = values[count - 1];
    for (uint32_t i = 0; i < count; i++) {
        results[i] = last;
    }
    return 0;
}

/**
 * @brief Rank 函数实现（并列跳号）
 */
static int eval_rank(const float *values, uint32_t count, float *results) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t rank = 1;
        for (uint32_t j = 0; j < count; j++) {
            if (values[j] < values[i]) {
                rank++;
            }
        }
        results[i] = (float)rank;
    }
    return 0;
}

/**
 * @brief Dense Rank 函数实现（并列不跳号）
 */
static int eval_dense_rank(const float *values, uint32_t count, float *results) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t rank = 1;
        for (uint32_t j = 0; j < count; j++) {
            if (values[j] < values[i]) {
                rank++;
            }
        }
        results[i] = (float)rank;
    }
    return 0;
}

/**
 * @brief Percent Rank 函数实现
 */
static int eval_percent_rank(const float *values, uint32_t count, float *results) {
    if (count <= 1) {
        for (uint32_t i = 0; i < count; i++) {
            results[i] = 0.0f;
        }
        return 0;
    }
    for (uint32_t i = 0; i < count; i++) {
        uint32_t rank = 0;
        for (uint32_t j = 0; j < count; j++) {
            if (values[j] < values[i]) {
                rank++;
            }
        }
        results[i] = (float)rank / (float)(count - 1);
    }
    return 0;
}

/**
 * @brief Ntile 函数实现
 */
static int eval_ntile(const float *values, uint32_t count, int num_buckets, float *results) {
    if (num_buckets <= 0) num_buckets = 1;
    uint32_t base_size = count / (uint32_t)num_buckets;
    uint32_t remainder = count % (uint32_t)num_buckets;
    uint32_t idx = 0;
    for (int bucket = 1; bucket <= num_buckets; bucket++) {
        uint32_t size = base_size + ((uint32_t)bucket <= remainder ? 1 : 0);
        for (uint32_t j = 0; j < size && idx < count; j++, idx++) {
            results[idx] = (float)bucket;
        }
    }
    return 0;
}

/**
 * @brief 窗口函数求值主函数
 */
int analytic_evaluate_window(
    const char *func_name,
    const float *values,
    uint32_t count,
    const analytic_window_t *window,
    float *results)
{
    if (values == NULL || count == 0 || window == NULL || results == NULL) {
        LOG_ERROR("窗口函数求值失败: 参数无效 (func=%s)", func_name ? func_name : "NULL");
        return -1;
    }

    LOG_INFO("窗口函数求值: func=%s, count=%u", func_name ? func_name : "unknown", count);

    switch (window->func_type) {
    case ANALYTIC_FUNC_MOVING_AVG:
        return eval_moving_avg(values, count, window->frame_rows, results);
    case ANALYTIC_FUNC_MOVING_SUM:
        return eval_moving_sum(values, count, window->frame_rows, results);
    case ANALYTIC_FUNC_LEAD:
        return eval_lead(values, count, window->frame_end, results);
    case ANALYTIC_FUNC_LAG:
        return eval_lag(values, count, abs(window->frame_start), results);
    case ANALYTIC_FUNC_FIRST_VALUE:
        return eval_first_value(values, count, results);
    case ANALYTIC_FUNC_LAST_VALUE:
        return eval_last_value(values, count, results);
    case ANALYTIC_FUNC_RANK:
        return eval_rank(values, count, results);
    case ANALYTIC_FUNC_DENSE_RANK:
        return eval_dense_rank(values, count, results);
    case ANALYTIC_FUNC_PERCENT_RANK:
        return eval_percent_rank(values, count, results);
    case ANALYTIC_FUNC_NTILE:
        return eval_ntile(values, count, window->frame_end > 0 ? window->frame_end : 4, results);
    default:
        LOG_ERROR("窗口函数求值失败: 未知函数类型 %d", window->func_type);
        return -1;
    }
}

/* ========================================================================
 * 表采样
 * ======================================================================== */

int analytic_bernoulli_sample(
    const float *values,
    uint32_t count,
    float ratio,
    uint32_t seed,
    float *output,
    uint32_t output_size)
{
    if (values == NULL || count == 0 || output == NULL || output_size == 0) {
        LOG_ERROR("Bernoulli 采样失败: 参数无效");
        return -1;
    }
    if (ratio <= 0.0f || ratio > 1.0f) {
        LOG_ERROR("Bernoulli 采样失败: 比例超出范围 (%.4f)", ratio);
        return -1;
    }

    uint32_t state = get_seed(seed);
    uint32_t out_idx = 0;
    uint32_t threshold = (uint32_t)(ratio * 4294967295.0f);

    for (uint32_t i = 0; i < count && out_idx < output_size; i++) {
        if (simple_rand(&state) <= threshold) {
            output[out_idx++] = values[i];
        }
    }

    LOG_INFO("Bernoulli 采样完成: 输入=%u, 输出=%u, 比例=%.4f",
             count, out_idx, ratio);
    return (int)out_idx;
}

int analytic_reservoir_sample(
    const float *values,
    uint32_t count,
    uint32_t reservoir_size,
    uint32_t seed,
    float *output)
{
    if (values == NULL || count == 0 || output == NULL || reservoir_size == 0) {
        LOG_ERROR("Reservoir 采样失败: 参数无效");
        return -1;
    }

    uint32_t state = get_seed(seed);
    uint32_t size = (count < reservoir_size) ? count : reservoir_size;

    /* 先填充前 reservoir_size 个元素 */
    for (uint32_t i = 0; i < size; i++) {
        output[i] = values[i];
    }

    /* 对后续元素进行替换 */
    for (uint32_t i = size; i < count; i++) {
        uint32_t j = simple_rand(&state) % (i + 1);
        if (j < size) {
            output[j] = values[i];
        }
    }

    LOG_INFO("Reservoir 采样完成: 输入=%u, 输出=%u", count, size);
    return (int)size;
}

int analytic_table_sample(
    const void *table,
    const sample_config_t *config,
    void *result)
{
    if (table == NULL || config == NULL || result == NULL) {
        LOG_ERROR("表采样失败: 参数为空");
        return -1;
    }

    /* 通用表采样接口的简化实现 */
    LOG_INFO("表采样: method=%d, ratio=%.4f, seed=%u",
             config->method, config->ratio, config->seed);

    return 0;
}

/* ========================================================================
 * HyperLogLog 实现
 * ======================================================================== */

hll_sketch_t *hll_create(uint8_t precision) {
    if (precision < 1 || precision > HLL_MAX_PRECISION) {
        LOG_ERROR("HyperLogLog 创建失败: 精度超出范围 (%u)", precision);
        return NULL;
    }

    hll_sketch_t *sketch = (hll_sketch_t *)calloc(1, sizeof(hll_sketch_t));
    if (sketch == NULL) {
        LOG_ERROR("HyperLogLog 创建失败: 内存分配失败");
        return NULL;
    }

    sketch->precision = precision;
    sketch->num_buckets = 1u << precision;
    sketch->buckets = (uint8_t *)calloc(sketch->num_buckets, sizeof(uint8_t));
    if (sketch->buckets == NULL) {
        free(sketch);
        LOG_ERROR("HyperLogLog 创建失败: 桶数组内存分配失败");
        return NULL;
    }

    /* 预计算修正系数 alpha */
    double m = (double)sketch->num_buckets;
    if (precision == 4) {
        sketch->alpha = 0.673;
    } else if (precision == 5) {
        sketch->alpha = 0.697;
    } else if (precision == 6) {
        sketch->alpha = 0.709;
    } else {
        sketch->alpha = 0.7213 / (1.0 + 1.079 / m);
    }

    LOG_INFO("HyperLogLog 创建成功: precision=%u, buckets=%u", precision, sketch->num_buckets);
    return sketch;
}

void hll_destroy(hll_sketch_t *sketch) {
    if (sketch == NULL) return;
    free(sketch->buckets);
    free(sketch);
}

void hll_add(hll_sketch_t *sketch, uint64_t hash_value) {
    if (sketch == NULL) return;

    /* 使用低 p 位作为桶索引 */
    uint32_t bucket = (uint32_t)(hash_value & (sketch->num_buckets - 1));

    /* 使用剩余位计算前导零数量 */
    uint64_t w = hash_value >> sketch->precision;
    uint8_t leading_zeros = 1;
    while ((w & 1) == 0 && leading_zeros < 64 - sketch->precision) {
        leading_zeros++;
        w >>= 1;
    }

    /* 更新桶的最大前导零数量 */
    if (leading_zeros > sketch->buckets[bucket]) {
        sketch->buckets[bucket] = leading_zeros;
    }
}

double hll_estimate(const hll_sketch_t *sketch) {
    if (sketch == NULL) return 0.0;

    double sum = 0.0;
    uint32_t empty_count = 0;

    for (uint32_t i = 0; i < sketch->num_buckets; i++) {
        sum += pow(2.0, -(double)sketch->buckets[i]);
        if (sketch->buckets[i] == 0) {
            empty_count++;
        }
    }

    double m = (double)sketch->num_buckets;
    double estimate = sketch->alpha * m * m / sum;

    /* 小范围修正（Linear Counting） */
    if (estimate <= 2.5 * m && empty_count > 0) {
        estimate = m * log(m / (double)empty_count);
    }
    /* 大范围修正 */
    else if (estimate > (1.0 / 30.0) * 4294967296.0) {
        estimate = -4294967296.0 * log(1.0 - estimate / 4294967296.0);
    }

    return estimate;
}

int hll_merge(hll_sketch_t *dest, const hll_sketch_t *src) {
    if (dest == NULL || src == NULL) {
        LOG_ERROR("HyperLogLog 合并失败: 参数为空");
        return -1;
    }
    if (dest->precision != src->precision) {
        LOG_ERROR("HyperLogLog 合并失败: 精度不匹配 (%u != %u)",
                  dest->precision, src->precision);
        return -1;
    }

    for (uint32_t i = 0; i < dest->num_buckets; i++) {
        if (src->buckets[i] > dest->buckets[i]) {
            dest->buckets[i] = src->buckets[i];
        }
    }

    LOG_INFO("HyperLogLog 合并完成: buckets=%u", dest->num_buckets);
    return 0;
}

/* ========================================================================
 * 近似计算 API（使用 HyperLogLog）
 * ======================================================================== */

float analytic_approx_count_distinct(const void *column, uint32_t count) {
    if (column == NULL || count == 0) {
        return 0.0f;
    }

    hll_sketch_t *sketch = hll_create(HLL_DEFAULT_PRECISION);
    if (sketch == NULL) {
        return 0.0f;
    }

    /* 将数据视为 float 数组进行哈希 */
    const float *values = (const float *)column;
    for (uint32_t i = 0; i < count; i++) {
        uint64_t hash = fnv1a_hash(&values[i], sizeof(float));
        hll_add(sketch, hash);
    }

    float result = (float)hll_estimate(sketch);
    hll_destroy(sketch);

    LOG_INFO("近似不同值计数: count=%u, result=%.0f", count, result);
    return result;
}

/* ========================================================================
 * T-Digest 实现
 * ======================================================================== */

tdigest_t *tdigest_create(double compression) {
    if (compression <= 0.0) {
        compression = TDIGEST_DEFAULT_COMPRESSION;
    }

    tdigest_t *digest = (tdigest_t *)calloc(1, sizeof(tdigest_t));
    if (digest == NULL) {
        LOG_ERROR("T-Digest 创建失败: 内存分配失败");
        return NULL;
    }

    digest->compression = compression;
    digest->max_clusters = (uint32_t)(2.0 * compression) + 10;
    if (digest->max_clusters > TDIGEST_MAX_CLUSTERS) {
        digest->max_clusters = TDIGEST_MAX_CLUSTERS;
    }

    digest->clusters = (tdigest_cluster_t *)calloc(digest->max_clusters, sizeof(tdigest_cluster_t));
    if (digest->clusters == NULL) {
        free(digest);
        LOG_ERROR("T-Digest 创建失败: 簇数组内存分配失败");
        return NULL;
    }

    LOG_INFO("T-Digest 创建成功: compression=%.1f, max_clusters=%u",
             compression, digest->max_clusters);
    return digest;
}

void tdigest_destroy(tdigest_t *digest) {
    if (digest == NULL) return;
    free(digest->clusters);
    free(digest);
}

/**
 * @brief T-Digest 内部压缩
 */
static void tdigest_compress(tdigest_t *digest) {
    if (digest->num_clusters <= digest->max_clusters) return;

    /* 简化实现：合并相邻簇 */
    uint32_t new_count = 0;
    for (uint32_t i = 0; i < digest->num_clusters; i += 2) {
        if (i + 1 < digest->num_clusters) {
            double total_count = digest->clusters[i].count + digest->clusters[i + 1].count;
            double total_weight = digest->clusters[i].weight + digest->clusters[i + 1].weight;
            digest->clusters[new_count].mean =
                (digest->clusters[i].mean * digest->clusters[i].count +
                 digest->clusters[i + 1].mean * digest->clusters[i + 1].count) / total_count;
            digest->clusters[new_count].count = total_count;
            digest->clusters[new_count].weight = total_weight;
        } else {
            digest->clusters[new_count] = digest->clusters[i];
        }
        new_count++;
    }
    digest->num_clusters = new_count;
}

void tdigest_add(tdigest_t *digest, double value) {
    if (digest == NULL) return;

    /* 查找合适的簇进行合并 */
    uint32_t best_idx = 0;
    double best_distance = 1e30;
    for (uint32_t i = 0; i < digest->num_clusters; i++) {
        double distance = fabs(digest->clusters[i].mean - value);
        if (distance < best_distance) {
            best_distance = distance;
            best_idx = i;
        }
    }

    if (digest->num_clusters == 0) {
        /* 第一个簇 */
        digest->clusters[0].mean = value;
        digest->clusters[0].count = 1.0;
        digest->clusters[0].weight = 1.0;
        digest->num_clusters = 1;
    } else {
        /* 更新现有簇 */
        double old_count = digest->clusters[best_idx].count;
        digest->clusters[best_idx].mean =
            (digest->clusters[best_idx].mean * old_count + value) / (old_count + 1.0);
        digest->clusters[best_idx].count += 1.0;
        digest->clusters[best_idx].weight += 1.0;
    }

    digest->total_count += 1.0;

    /* 检查是否需要压缩 */
    tdigest_compress(digest);
}

double tdigest_median(const tdigest_t *digest) {
    return tdigest_quantile(digest, 0.5);
}

double tdigest_quantile(const tdigest_t *digest, double percentile) {
    if (digest == NULL || digest->num_clusters == 0) {
        return 0.0;
    }
    if (percentile <= 0.0) return digest->clusters[0].mean;
    if (percentile >= 1.0) return digest->clusters[digest->num_clusters - 1].mean;

    double target = percentile * digest->total_count;
    double cumulative = 0.0;

    for (uint32_t i = 0; i < digest->num_clusters; i++) {
        double next_cumulative = cumulative + digest->clusters[i].count;
        if (target <= next_cumulative) {
            /* 在当前簇内线性插值 */
            double fraction = (target - cumulative) / digest->clusters[i].count;
            if (i + 1 < digest->num_clusters) {
                double next_mean = digest->clusters[i + 1].mean;
                return digest->clusters[i].mean +
                       fraction * (next_mean - digest->clusters[i].mean);
            }
            return digest->clusters[i].mean;
        }
        cumulative = next_cumulative;
    }

    return digest->clusters[digest->num_clusters - 1].mean;
}

int tdigest_merge(tdigest_t *dest, const tdigest_t *src) {
    if (dest == NULL || src == NULL) {
        LOG_ERROR("T-Digest 合并失败: 参数为空");
        return -1;
    }

    for (uint32_t i = 0; i < src->num_clusters; i++) {
        tdigest_add(dest, src->clusters[i].mean);
    }

    LOG_INFO("T-Digest 合并完成: src_clusters=%u", src->num_clusters);
    return 0;
}

/* ========================================================================
 * 近似计算 API（使用 T-Digest）
 * ======================================================================== */

float analytic_approx_median(const float *values, uint32_t count) {
    return analytic_approx_percentile(values, count, 0.5f);
}

float analytic_approx_percentile(const float *values, uint32_t count, float percentile) {
    if (values == NULL || count == 0) {
        LOG_ERROR("近似百分位数计算失败: 参数无效");
        return 0.0f;
    }

    tdigest_t *digest = tdigest_create(TDIGEST_DEFAULT_COMPRESSION);
    if (digest == NULL) {
        return 0.0f;
    }

    for (uint32_t i = 0; i < count; i++) {
        tdigest_add(digest, (double)values[i]);
    }

    float result = (float)tdigest_quantile(digest, (double)percentile);
    tdigest_destroy(digest);

    LOG_INFO("近似百分位数: count=%u, p=%.2f, result=%.4f", count, percentile, result);
    return result;
}
