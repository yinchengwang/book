/**
 * @file adaptive_quantization.h
 * @brief 自适应量化参数选择头文件
 *
 * 根据数据特征自动选择最优量化参数：
 * 1. 数据分布分析（方差、偏度、峰值）
 * 2. 压缩比与精度权衡
 * 3. 量化类型选择（SQ/PQ/OPQ/LVQ）
 */
#ifndef ADAPTIVE_QUANTIZATION_H
#define ADAPTIVE_QUANTIZATION_H

#include "quantization.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 数据特征分析
 * ======================================================================== */

/** 数据分布特征 */
typedef struct DataDistribution_s {
    float mean;               /**< 均值 */
    float std_dev;            /**< 标准差 */
    float variance;           /**< 方差 */
    float min_val;            /**< 最小值 */
    float max_val;            /**< 最大值 */
    float range;              /**< 范围 */
    float skewness;           /**< 偏度 */
    float kurtosis;           /**< 峰度 */
    float entropy;            /**< 信息熵 */
    float sparsity;           /**< 稀疏度 */
    int32_t dim;              /**< 向量维度 */
    int32_t sample_count;     /**< 样本数量 */
} DataDistribution;

/**
 * @brief 分析向量数据分布特征
 *
 * @param vectors 输入向量 [n][dims]
 * @param n 向量数量
 * @param dims 向量维度
 * @param distribution 输出分布特征
 * @return 0 成功，-1 失败
 */
int32_t analyze_vector_distribution(const float *vectors,
                                    int32_t n,
                                    int32_t dims,
                                    DataDistribution *distribution);

/**
 * @brief 释放分布特征内存
 */
void distribution_free(DataDistribution *distribution);

/* ========================================================================
 * 量化参数推荐
 * ======================================================================== */

/** 量化配置推荐 */
typedef struct QuantizationRecommendation_s {
    quantization_type_t type;     /**< 推荐量化类型 */
    int32_t subquantizers;        /**< 子空间数量 */
    int32_t bits;                 /**< 编码位数 */
    float compression_ratio;      /**< 压缩比 */
    float estimated_error;        /**< 估计误差 */
    float recall_loss;            /**< 召回损失估计 */
    int32_t codebook_size;        /**< 码本大小 */
    int32_t train_sample_size;    /**< 推荐训练样本数 */
    float memory_mb;              /**< 估计内存使用 (MB) */
} QuantizationRecommendation;

/** 优化目标 */
typedef enum OptimizationGoal_e {
    GOAL_BALANCED = 0,           /**< 平衡精度和压缩比 */
    GOAL_HIGH_RECALL = 1,        /**< 高召回率优先 */
    GOAL_HIGH_COMPRESSION = 2,   /**< 高压缩比优先 */
    GOAL_LOW_LATENCY = 3         /**< 低延迟优先 */
} OptimizationGoal;

/**
 * @brief 获取量化参数推荐
 *
 * @param distribution 数据分布特征
 * @param goal 优化目标
 * @return 推荐配置，失败返回 NULL
 */
QuantizationRecommendation *get_quantization_recommendation(
    const DataDistribution *distribution,
    OptimizationGoal goal);

/**
 * @brief 释放推荐配置内存
 */
void recommendation_free(QuantizationRecommendation *rec);

/**
 * @brief 创建量化器并自动选择参数
 *
 * @param vectors 训练向量
 * @param n 向量数量
 * @param dims 向量维度
 * @param goal 优化目标
 * @return 配置好的量化器，失败返回 NULL
 */
quantizer_t *create_adaptive_quantizer(const float *vectors,
                                       int32_t n,
                                       int32_t dims,
                                       OptimizationGoal goal);

/**
 * @brief 评估量化器质量
 *
 * @param quantizer 量化器
 * @param vectors 测试向量
 * @param n 向量数量
 * @param sample_size 采样大小
 * @param out_recall 输出召回率
 * @param out_error 输出量化误差
 * @return 0 成功，-1 失败
 */
int32_t evaluate_quantizer(const quantizer_t *quantizer,
                           const float *vectors,
                           int32_t n,
                           int32_t sample_size,
                           float *out_recall,
                           float *out_error);

/**
 * @brief 比较两种量化配置
 *
 * @param vectors 测试向量
 * @param n 向量数量
 * @param config_a 配置 A
 * @param config_b 配置 B
 * @return 返回更好的配置 (1=a, -1=b, 0=相同)
 */
int32_t compare_quantizer_configs(const float *vectors,
                                  int32_t n,
                                  const quantizer_config_t *config_a,
                                  const quantizer_config_t *config_b);

/* ========================================================================
 * 内存估算
 * ======================================================================== */

/**
 * @brief 估算量化器内存使用
 *
 * @param dims 向量维度
 * @param type 量化类型
 * @param subquantizers 子空间数
 * @param bits 编码位数
 * @param use_opq 是否使用 OPQ
 * @return 内存使用估算 (bytes)
 */
int64_t estimate_quantizer_memory(int32_t dims,
                                  quantization_type_t type,
                                  int32_t subquantizers,
                                  int32_t bits,
                                  bool use_opq);

/**
 * @brief 估算压缩后存储大小
 *
 * @param dims 向量维度
 * @param n_vectors 向量数量
 * @param type 量化类型
 * @param bits 编码位数
 * @return 压缩后存储大小 (bytes)
 */
int64_t estimate_compressed_size(int32_t dims,
                                 int32_t n_vectors,
                                 quantization_type_t type,
                                 int32_t bits);

/**
 * @brief 计算压缩比
 *
 * @param original_size 原始大小
 * @param compressed_size 压缩后大小
 * @return 压缩比
 */
float calculate_compression_ratio(int64_t original_size,
                                  int64_t compressed_size);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 获取量化类型名称
 */
const char *quantization_type_name(quantization_type_t type);

/**
 * @brief 获取优化目标名称
 */
const char *optimization_goal_name(OptimizationGoal goal);

/**
 * @brief 检查量化配置是否有效
 */
bool is_valid_quantization_config(quantization_type_t type,
                                  int32_t dims,
                                  int32_t subquantizers,
                                  int32_t bits);

/**
 * @brief 获取推荐子空间数
 *
 * 根据向量维度推荐合适的子空间数
 */
int32_t recommend_subquantizers(int32_t dims);

/**
 * @brief 获取推荐编码位数
 *
 * 根据数据分布和优化目标推荐编码位数
 */
int32_t recommend_bits(quantization_type_t type,
                       OptimizationGoal goal);

#ifdef __cplusplus
}
#endif

#endif /* ADAPTIVE_QUANTIZATION_H */
