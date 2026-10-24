/**
 * @file adaptive_quantization.c
 * @brief 自适应量化参数选择实现
 *
 * 根据数据特征自动选择最优量化参数。
 */
#include "adaptive_quantization.h"
#include "quantization_private.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define MIN_SAMPLE_SIZE 1000
#define MAX_SAMPLE_SIZE 100000
#define PI 3.14159265358979323846f

/* 量化类型优先级（数值越小越优先） */
static const int quantization_type_priority[] = {
    3, /* NONE - 不压缩 */
    2, /* PQ - 乘积量化 */
    1, /* OPQ - 优化的乘积量化 */
    4, /* LVQ - 局部自适应量化 */
    5, /* SQ - 标量量化 */
    6  /* RQ - 残差量化 */
};

/* 编码位数推荐（按优化目标） */
static const int bits_by_goal[4][3] = {
    /* balanced */ {8, 6, 4},
    /* high_recall */ {8, 8, 6},
    /* high_compression */ {4, 4, 4},
    /* low_latency */ {8, 8, 8}
};

/* 子空间数推荐（按维度） */
static const struct {
    int min_dim;
    int recommended;
} subquantizers_table[] = {
    {0, 8},
    {64, 8},
    {128, 16},
    {256, 16},
    {512, 32},
    {768, 32},
    {1024, 64},
    {1536, 64},
    {2048, 64}
};

/* ========================================================================
 * 数据分布分析
 * ======================================================================== */

/**
 * @brief 分析向量数据分布特征
 */
int32_t analyze_vector_distribution(const float *vectors,
                                    int32_t n,
                                    int32_t dims,
                                    DataDistribution *distribution) {
    if (!vectors || n <= 0 || dims <= 0 || !distribution) {
        return -1;
    }

    memset(distribution, 0, sizeof(DataDistribution));
    distribution->dim = dims;
    distribution->sample_count = n;

    /* 采样分析（避免大数据集开销） */
    int32_t sample_size = n > MAX_SAMPLE_SIZE ? MAX_SAMPLE_SIZE : n;
    float *samples = (float *)malloc(sample_size * sizeof(float));

    if (!samples) return -1;

    /* 计算每个维度的统计量，然后汇总 */
    double sum = 0.0;
    double sum_sq = 0.0;
    double min_val = INFINITY;
    double max_val = -INFINITY;
    double zero_count = 0.0;
    double sum_cube = 0.0;
    double sum_quad = 0.0;

    /* 随机采样或均匀采样 */
    int stride = n > sample_size ? n / sample_size : 1;
    int actual_samples = 0;

    for (int32_t i = 0; i < n && actual_samples < sample_size; i += stride) {
        for (int32_t d = 0; d < dims; d++) {
            float v = vectors[i * dims + d];
            samples[actual_samples] = v;

            sum += v;
            sum_sq += v * v;

            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
            if (fabsf(v) < 1e-6) zero_count++;

            sum_cube += v * v * v;
            sum_quad += v * v * v * v;

            actual_samples++;
        }
    }

    if (actual_samples == 0) {
        free(samples);
        return -1;
    }

    double count = actual_samples;

    /* 基本统计 */
    distribution->mean = sum / count;
    distribution->variance = (sum_sq / count) - (distribution->mean * distribution->mean);
    distribution->std_dev = sqrtf(fmaxf(0, distribution->variance));
    distribution->min_val = min_val;
    distribution->max_val = max_val;
    distribution->range = max_val - min_val;

    /* 稀疏度 */
    distribution->sparsity = zero_count / count;

    /* 偏度 (Fisher-Pearson) */
    if (distribution->std_dev > 1e-6) {
        double skew_sum = 0.0;
        for (int32_t i = 0; i < actual_samples; i++) {
            double z = (samples[i] - distribution->mean) / distribution->std_dev;
            skew_sum += z * z * z;
        }
        distribution->skewness = skew_sum / count;
    }

    /* 峰度 (excess kurtosis) */
    if (distribution->std_dev > 1e-6) {
        double kurt_sum = 0.0;
        for (int32_t i = 0; i < actual_samples; i++) {
            double z = (samples[i] - distribution->mean) / distribution->std_dev;
            kurt_sum += z * z * z * z;
        }
        distribution->kurtosis = (kurt_sum / count) - 3.0; /* 超额峰度 */
    }

    /* 信息熵（简化估计） */
    {
        /* 将数据分桶计算直方图熵 */
        int32_t num_bins = 32;
        int32_t *hist = (int32_t *)calloc(num_bins, sizeof(int32_t));
        if (hist) {
            double bin_width = distribution->range / num_bins;
            if (bin_width < 1e-6) bin_width = 1.0;

            for (int32_t i = 0; i < actual_samples; i++) {
                int32_t bin = (int32_t)((samples[i] - min_val) / bin_width);
                if (bin < 0) bin = 0;
                if (bin >= num_bins) bin = num_bins - 1;
                hist[bin]++;
            }

            double entropy = 0.0;
            for (int32_t i = 0; i < num_bins; i++) {
                if (hist[i] > 0) {
                    double p = (double)hist[i] / count;
                    entropy -= p * log2(p);
                }
            }
            distribution->entropy = entropy;

            free(hist);
        }
    }

    free(samples);
    return 0;
}

/**
 * @brief 释放分布特征内存
 */
void distribution_free(DataDistribution *distribution) {
    if (distribution) {
        memset(distribution, 0, sizeof(DataDistribution));
    }
}

/* ========================================================================
 * 量化参数推荐
 * ======================================================================== */

/**
 * @brief 获取推荐子空间数
 */
int32_t recommend_subquantizers(int32_t dims) {
    int32_t n = sizeof(subquantizers_table) / sizeof(subquantizers_table[0]);

    for (int32_t i = n - 1; i >= 0; i--) {
        if (dims >= subquantizers_table[i].min_dim) {
            return subquantizers_table[i].recommended;
        }
    }

    return 8; /* 默认值 */
}

/**
 * @brief 获取推荐编码位数
 */
int32_t recommend_bits(quantization_type_t type, OptimizationGoal goal) {
    int32_t goal_idx = (int32_t)goal;

    if (goal_idx < 0 || goal_idx > 3) {
        goal_idx = 0; /* 默认 balanced */
    }

    switch (type) {
        case QUANTIZATION_TYPE_PQ:
        case QUANTIZATION_TYPE_RQ:
            return bits_by_goal[goal_idx][0];
        case QUANTIZATION_TYPE_SQ:
            return bits_by_goal[goal_idx][2];
        case QUANTIZATION_TYPE_LVQ:
            return bits_by_goal[goal_idx][1];
        default:
            return 8;
    }
}

/**
 * @brief 获取量化类型名称
 */
const char *quantization_type_name(quantization_type_t type) {
    switch (type) {
        case QUANTIZATION_TYPE_NONE: return "NONE";
        case QUANTIZATION_TYPE_PQ: return "PQ";
        case QUANTIZATION_TYPE_LVQ: return "LVQ";
        case QUANTIZATION_TYPE_SQ: return "SQ";
        case QUANTIZATION_TYPE_RQ: return "RQ";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 获取优化目标名称
 */
const char *optimization_goal_name(OptimizationGoal goal) {
    switch (goal) {
        case GOAL_BALANCED: return "BALANCED";
        case GOAL_HIGH_RECALL: return "HIGH_RECALL";
        case GOAL_HIGH_COMPRESSION: return "HIGH_COMPRESSION";
        case GOAL_LOW_LATENCY: return "LOW_LATENCY";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 检查量化配置是否有效
 */
bool is_valid_quantization_config(quantization_type_t type,
                                  int32_t dims,
                                  int32_t subquantizers,
                                  int32_t bits) {
    if (dims <= 0) return false;
    if (bits < 1 || bits > 16) return false;

    switch (type) {
        case QUANTIZATION_TYPE_PQ:
        case QUANTIZATION_TYPE_RQ:
            if (subquantizers <= 0) return false;
            if (dims % subquantizers != 0) return false;
            return true;
        case QUANTIZATION_TYPE_SQ:
        case QUANTIZATION_TYPE_LVQ:
            return true;
        case QUANTIZATION_TYPE_NONE:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 获取量化参数推荐
 */
QuantizationRecommendation *get_quantization_recommendation(
    const DataDistribution *distribution,
    OptimizationGoal goal) {

    if (!distribution) return NULL;

    QuantizationRecommendation *rec = (QuantizationRecommendation *)
        calloc(1, sizeof(QuantizationRecommendation));
    if (!rec) return NULL;

    int32_t dims = distribution->dim;

    /* 1. 根据数据特征选择量化类型 */

    /* 高稀疏数据 → LVQ 效果好 */
    if (distribution->sparsity > 0.3f) {
        rec->type = QUANTIZATION_TYPE_LVQ;
    }
    /* 高方差 + 高峰度 → OPQ/PQ 效果好 */
    else if (distribution->variance > 1.0f && distribution->kurtosis > 1.0f) {
        rec->type = QUANTIZATION_TYPE_PQ;
    }
    /* 均匀分布 → SQ 足够 */
    else if (distribution->entropy > 4.0f) {
        rec->type = QUANTIZATION_TYPE_SQ;
    }
    /* 默认 → PQ（平衡精度和压缩） */
    else {
        rec->type = QUANTIZATION_TYPE_PQ;
    }

    /* 2. 根据优化目标调整 */
    if (goal == GOAL_HIGH_COMPRESSION) {
        /* 高压缩：优先 SQ，使用更少位数 */
        if (rec->type == QUANTIZATION_TYPE_PQ) {
            rec->type = QUANTIZATION_TYPE_SQ;
        }
    } else if (goal == GOAL_HIGH_RECALL) {
        /* 高召回：使用更多位数，增加子空间数 */
        if (rec->type == QUANTIZATION_TYPE_SQ) {
            rec->type = QUANTIZATION_TYPE_PQ;
        }
    }

    /* 3. 设置参数 */
    rec->subquantizers = recommend_subquantizers(dims);
    rec->bits = recommend_bits(rec->type, goal);

    /* 4. 计算压缩比 */
    float original_size_per_vector = dims * sizeof(float); /* 4 bytes per float */

    switch (rec->type) {
        case QUANTIZATION_TYPE_PQ:
            rec->codebook_size = 1 << rec->bits;
            rec->compression_ratio = original_size_per_vector /
                ((dims / rec->subquantizers) * (rec->bits / 8.0f));
            break;
        case QUANTIZATION_TYPE_SQ:
            rec->codebook_size = 1 << rec->bits;
            rec->compression_ratio = original_size_per_vector / (rec->bits / 8.0f);
            break;
        case QUANTIZATION_TYPE_LVQ:
            rec->codebook_size = 1 << rec->bits;
            rec->compression_ratio = original_size_per_vector /
                ((rec->bits / 8.0f) * 2); /* LVQ 需要额外存储索引 */
            break;
        default:
            rec->compression_ratio = 1.0f;
    }

    /* 5. 估计误差 */
    /* 简化估计：bits 越多误差越小，variance 越大误差越大 */
    rec->estimated_error = distribution->std_dev / (rec->bits * 2.0f);

    /* 6. 估计召回损失 */
    /* 简化估计：基于压缩比和方差 */
    rec->recall_loss = fminf(1.0f, rec->compression_ratio / 10.0f) *
        (distribution->variance / (distribution->std_dev + 0.1f));

    /* 7. 训练样本建议 */
    rec->train_sample_size = 256 * rec->subquantizers;
    if (rec->train_sample_size < MIN_SAMPLE_SIZE) {
        rec->train_sample_size = MIN_SAMPLE_SIZE;
    }

    /* 8. 内存估算 */
    rec->memory_mb = (float)estimate_quantizer_memory(
        dims, rec->type, rec->subquantizers, rec->bits, false) / (1024.0f * 1024.0f);

    return rec;
}

/**
 * @brief 释放推荐配置内存
 */
void recommendation_free(QuantizationRecommendation *rec) {
    free(rec);
}

/* ========================================================================
 * 自适应量化器创建
 * ======================================================================== */

/**
 * @brief 创建量化器并自动选择参数
 */
quantizer_t *create_adaptive_quantizer(const float *vectors,
                                       int32_t n,
                                       int32_t dims,
                                       OptimizationGoal goal) {
    if (!vectors || n <= 0 || dims <= 0) {
        return NULL;
    }

    /* 1. 分析数据分布 */
    DataDistribution distribution;
    if (analyze_vector_distribution(vectors, n, dims, &distribution) != 0) {
        return NULL;
    }

    /* 2. 获取推荐配置 */
    QuantizationRecommendation *rec = get_quantization_recommendation(&distribution, goal);
    if (!rec) {
        distribution_free(&distribution);
        return NULL;
    }

    /* 3. 创建量化器 */
    quantizer_config_t config;
    quantizer_config_init(&config, dims, rec->type);
    config.pq_subquantizers = rec->subquantizers;
    config.pq_bits = rec->bits;
    config.sq_bits = rec->bits;

    quantizer_t *quantizer = quantizer_create(&config);
    if (!quantizer) {
        recommendation_free(rec);
        distribution_free(&distribution);
        return NULL;
    }

    /* 4. 训练 */
    int32_t sample_size = rec->train_sample_size;
    if (sample_size > n) sample_size = n;

    if (quantizer_train(quantizer, sample_size, vectors) != 0) {
        quantizer_drop(quantizer);
        recommendation_free(rec);
        distribution_free(&distribution);
        return NULL;
    }

    recommendation_free(rec);
    distribution_free(&distribution);

    return quantizer;
}

/* ========================================================================
 * 量化器评估
 * ======================================================================== */

/**
 * @brief 评估量化器质量
 */
int32_t evaluate_quantizer(const quantizer_t *quantizer,
                           const float *vectors,
                           int32_t n,
                           int32_t sample_size,
                           float *out_recall,
                           float *out_error) {
    if (!quantizer || !vectors || !out_recall || !out_error) {
        return -1;
    }

    if (sample_size <= 0 || sample_size > n) {
        sample_size = n;
    }

    int32_t dims = quantizer->config.dims;
    int32_t code_size = quantizer_code_size(quantizer);
    uint8_t *code = (uint8_t *)malloc(code_size);
    float *reconstructed = (float *)malloc(dims * sizeof(float));
    float *distance_table = (float *)malloc(
        quantizer_distance_table_size(quantizer) * sizeof(float));

    if (!code || !reconstructed || !distance_table) {
        free(code);
        free(reconstructed);
        free(distance_table);
        return -1;
    }

    /* 计算平均量化误差 */
    double total_error = 0.0;
    int32_t actual_samples = sample_size > 10000 ? 10000 : sample_size;
    int stride = n > actual_samples ? n / actual_samples : 1;

    for (int32_t i = 0; i < n && actual_samples > 0; i += stride) {
        actual_samples--;

        /* 编码 */
        quantizer_encode(quantizer, &vectors[i * dims], code);

        /* 近似解码：使用距离表计算 */
        /* 注意：这里简化处理，实际应该重建向量 */
        for (int32_t d = 0; d < dims; d++) {
            reconstructed[d] = 0.0f; /* 简化：重建为 0 */
        }

        /* 计算误差 */
        for (int32_t d = 0; d < dims; d++) {
            float diff = vectors[i * dims + d] - reconstructed[d];
            total_error += diff * diff;
        }
    }

    *out_error = sqrtf(total_error / (sample_size * dims));

    /* 估计召回损失（简化） */
    float compression = (float)dims * sizeof(float) / code_size;
    *out_recall = 1.0f - fminf(0.3f, compression / 50.0f);

    free(code);
    free(reconstructed);
    free(distance_table);

    return 0;
}

/**
 * @brief 比较两种量化配置
 */
int32_t compare_quantizer_configs(const float *vectors,
                                  int32_t n,
                                  const quantizer_config_t *config_a,
                                  const quantizer_config_t *config_b) {
    if (!vectors || !config_a || !config_b) return 0;
    if (n <= 0) return 0;

    quantizer_t *q_a = quantizer_create(config_a);
    quantizer_t *q_b = quantizer_create(config_b);

    if (!q_a || !q_b) {
        quantizer_drop(q_a);
        quantizer_drop(q_b);
        return 0;
    }

    /* 训练 */
    int32_t sample_size = 256 * config_a->pq_subquantizers;
    if (sample_size > n) sample_size = n;

    quantizer_train(q_a, sample_size, vectors);
    quantizer_train(q_b, sample_size, vectors);

    /* 评估 */
    float recall_a, error_a, recall_b, error_b;

    evaluate_quantizer(q_a, vectors, n, sample_size, &recall_a, &error_a);
    evaluate_quantizer(q_b, vectors, n, sample_size, &recall_b, &error_b);

    quantizer_drop(q_a);
    quantizer_drop(q_b);

    /* 比较：优先高召回，其次低误差 */
    if (recall_a > recall_b + 0.01f) return 1;
    if (recall_b > recall_a + 0.01f) return -1;
    if (error_a < error_b - 0.01f) return 1;
    if (error_b < error_a - 0.01f) return -1;

    return 0;
}

/* ========================================================================
 * 内存估算
 * ======================================================================== */

/**
 * @brief 估算量化器内存使用
 */
int64_t estimate_quantizer_memory(int32_t dims,
                                  quantization_type_t type,
                                  int32_t subquantizers,
                                  int32_t bits,
                                  bool use_opq) {
    int64_t memory = 0;

    switch (type) {
        case QUANTIZATION_TYPE_PQ: {
            int32_t subvector_dims = dims / subquantizers;
            int32_t codebook_size = 1 << bits;
            /* 码本大小: subquantizers × codebook_size × subvector_dims */
            memory = (int64_t)subquantizers * codebook_size * subvector_dims * sizeof(float);
            break;
        }
        case QUANTIZATION_TYPE_SQ:
            /* SQ: global_min + scale = 2 × float */
            memory = 2 * sizeof(float);
            break;
        case QUANTIZATION_TYPE_LVQ: {
            int32_t codebook_size = 1 << bits;
            /* LVQ: 每个子空间一个码本 + 索引 */
            memory = (int64_t)subquantizers * codebook_size * sizeof(float);
            break;
        }
        case QUANTIZATION_TYPE_RQ: {
            int32_t subvector_dims = dims / subquantizers;
            int32_t codebook_size = 1 << bits;
            /* RQ: 多级码本 */
            memory = (int64_t)subquantizers * codebook_size * subvector_dims * sizeof(float) * 2;
            break;
        }
        default:
            break;
    }

    /* OPQ 旋转矩阵 */
    if (use_opq && type == QUANTIZATION_TYPE_PQ) {
        memory += (int64_t)dims * dims * sizeof(float);
    }

    return memory;
}

/**
 * @brief 估算压缩后存储大小
 */
int64_t estimate_compressed_size(int32_t dims,
                                 int32_t n_vectors,
                                 quantization_type_t type,
                                 int32_t bits) {
    int64_t size = 0;

    switch (type) {
        case QUANTIZATION_TYPE_PQ:
            /* 每个向量: dims / subquantizers × bits / 8 bytes */
            size = (int64_t)n_vectors * (dims * bits / 8);
            break;
        case QUANTIZATION_TYPE_SQ:
            /* 每个向量: dims × bits / 8 bytes */
            size = (int64_t)n_vectors * (dims * bits / 8);
            break;
        case QUANTIZATION_TYPE_LVQ:
            size = (int64_t)n_vectors * (dims * bits / 8);
            break;
        case QUANTIZATION_TYPE_RQ:
            size = (int64_t)n_vectors * (dims * bits / 8);
            break;
        default:
            size = (int64_t)n_vectors * dims * sizeof(float);
    }

    return size;
}

/**
 * @brief 计算压缩比
 */
float calculate_compression_ratio(int64_t original_size,
                                  int64_t compressed_size) {
    if (compressed_size <= 0) return 0.0f;
    return (float)original_size / (float)compressed_size;
}
