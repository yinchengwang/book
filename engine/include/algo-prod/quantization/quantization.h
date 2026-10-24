#ifndef QUANTIZATION_QUANTIZATION_H
#define QUANTIZATION_QUANTIZATION_H

/**
 * @file quantization.h
 * @brief 统一量化器接口
 *
 * 支持的量化类型:
 * - QUANTIZATION_TYPE_PQ:  乘积量化 (Product Quantization)
 * - QUANTIZATION_TYPE_LVQ: 局部自适应量化 (Locally-adaptive Vector Quantization)
 * - QUANTIZATION_TYPE_SQ:  标量量化 (Scalar Quantization)
 * - QUANTIZATION_TYPE_RQ:  残差量化 (RabitQ, Residual Binary Quantization)
 *
 * 使用示例:
 * @code
 *   // 创建 PQ 量化器
 *   quantizer_config_t config;
 *   quantizer_config_init(&config, 128, QUANTIZATION_TYPE_PQ);
 *   config.pq_subquantizers = 16;
 *   config.pq_bits = 8;
 *
 *   quantizer_t *q = quantizer_create(&config);
 *   quantizer_train(q, n, vectors);
 *
 *   // 编码
 *   uint8_t code[16];
 *   quantizer_encode(q, vector, code);
 *
 *   // 距离计算
 *   float table[4096];
 *   quantizer_compute_distance_table(q, DISTANCE_METRIC_L2, query, table);
 *   float dist = quantizer_adc_distance(q, code, table);
 *
 *   quantizer_drop(q);
 * @endcode
 */

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 量化器类型枚举
 */
typedef enum quantization_type {
    QUANTIZATION_TYPE_NONE = 0,  /**< 无量化 */
    QUANTIZATION_TYPE_PQ   = 1,  /**< 乘积量化 (Product Quantization) */
    QUANTIZATION_TYPE_LVQ  = 2,  /**< 局部自适应量化 (LVQ) */
    QUANTIZATION_TYPE_SQ   = 3,  /**< 标量量化 (Scalar Quantization) */
    QUANTIZATION_TYPE_RQ   = 4,  /**< 残差量化 (RabitQ) */
} quantization_type_t;

typedef struct quantizer quantizer_t;

typedef struct quantizer_config {
    int32_t dims;              /**< 向量维度 */
    quantization_type_t type;  /**< 量化器类型 */
    int32_t pq_subquantizers;  /**< PQ 子空间数量 (默认: 自动选择) */
    int32_t pq_bits;           /**< PQ bits: 4/6/8 (默认: 8) */
    int32_t lvq_bits;          /**< LVQ bits: 4/8 (默认: 8) */
    int32_t sq_bits;           /**< SQ bits: 4/8 (默认: 8) */
    int32_t rq_pq_bits;        /**< RabitQ PQ bits: 4/6/8 (默认: 8) */
    int32_t rq_residual_bits;  /**< RabitQ 残差 bits: 1/2/4 (默认: 1) */
} quantizer_config_t;

int32_t quantization_type_is_valid(quantization_type_t type);
void quantizer_config_init(quantizer_config_t *config, int32_t dims, quantization_type_t type);
int32_t quantizer_config_validate(const quantizer_config_t *config);
quantizer_t *quantizer_create(const quantizer_config_t *config);
int32_t quantizer_train(quantizer_t *quantizer, int32_t n, const float *vectors);
int32_t quantizer_encode(const quantizer_t *quantizer, const float *vector, uint8_t *code);
int32_t quantizer_encode_batch(const quantizer_t *quantizer, int32_t n, const float *vectors, uint8_t *codes);
int32_t quantizer_compute_distance_table(const quantizer_t *quantizer,
                                         distance_metric_t metric,
                                         const float *query,
                                         float *distance_table);
float quantizer_adc_distance(const quantizer_t *quantizer, const uint8_t *code, const float *distance_table);
int32_t quantizer_code_size(const quantizer_t *quantizer);
int32_t quantizer_distance_table_size(const quantizer_t *quantizer);
int32_t quantizer_model_float_count(const quantizer_t *quantizer);
int32_t quantizer_export_model(const quantizer_t *quantizer,
                               float *codebooks,
                               int32_t codebooks_count,
                               int32_t *trained_codebook_size);
quantizer_t *quantizer_create_from_model(const quantizer_config_t *config,
                                         int32_t trained_codebook_size,
                                         const float *codebooks,
                                         int32_t codebooks_count);
bool quantizer_is_trained(const quantizer_t *quantizer);
quantization_type_t quantizer_type(const quantizer_t *quantizer);
void quantizer_reset(quantizer_t *quantizer);
void quantizer_drop(quantizer_t *quantizer);

/* ========================================================================
 * OPQ (Optimized PQ) 旋转优化
 * ======================================================================== */

/**
 * @brief 启用 OPQ 旋转优化
 *
 * OPQ 通过在训练前对数据进行 PCA 旋转，使各子空间的方差更均衡，
 * 从而提升量化精度。
 *
 * @param quantizer 量化器
 * @return 0 成功，-1 失败
 */
int32_t quantizer_enable_opq(quantizer_t *quantizer);

/**
 * @brief 获取 OPQ 旋转矩阵
 *
 * @param quantizer 量化器
 * @param rotation_matrix 输出旋转矩阵 [dims x dims]
 * @param matrix_size 矩阵大小（应为 dims * dims）
 * @return 0 成功，-1 未启用 OPQ 或失败
 */
int32_t quantizer_get_opq_rotation(const quantizer_t *quantizer,
                                    float *rotation_matrix,
                                    int32_t matrix_size);

/* ========================================================================
 * IVF-PQ 索引序列化
 * ======================================================================== */

/**
 * @brief 保存量化器到文件
 *
 * @param quantizer 量化器
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int32_t quantizer_save(const quantizer_t *quantizer, const char *path);

/**
 * @brief 从文件加载量化器
 *
 * @param path 文件路径
 * @return 量化器句柄，失败返回 NULL
 */
quantizer_t *quantizer_load(const char *path);

#ifdef __cplusplus
}
#endif

#endif