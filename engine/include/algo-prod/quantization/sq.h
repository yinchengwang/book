#ifndef QUANTIZATION_SQ_H
#define QUANTIZATION_SQ_H

#include "quantization_private.h"

/* ── SQ 量化器内部函数 ── */

/**
 * SQ 初始化
 * @param q 量化器
 * @return 0 成功, -1 失败
 */
int32_t sq_init(quantizer_t *q);

/**
 * SQ 释放资源
 * @param q 量化器
 */
void sq_free(quantizer_t *q);

/**
 * SQ 重置状态
 * @param q 量化器
 */
void sq_reset(quantizer_t *q);

/**
 * SQ 训练：全局统计 min/max
 * @param q 量化器
 * @param n 向量数量
 * @param vectors 训练向量 [n][dims]
 * @return 0 成功, -1 失败
 */
int32_t sq_train(quantizer_t *q, int32_t n, const float *vectors);

/**
 * SQ 编码单个向量
 * @param q 量化器
 * @param vector 输入向量 [dims]
 * @param code 输出码字
 * @return 0 成功, -1 失败
 */
int32_t sq_encode(const quantizer_t *q, const float *vector, uint8_t *code);

/**
 * SQ 批量编码
 * @param q 量化器
 * @param n 向量数量
 * @param vectors 输入向量 [n][dims]
 * @param codes 输出码字 [n][code_size]
 * @return 0 成功, -1 失败
 */
int32_t sq_encode_batch(const quantizer_t *q, int32_t n, const float *vectors, uint8_t *codes);

/**
 * SQ 计算距离表（对于 SQ，直接存储查询向量）
 * @param q 量化器
 * @param metric 距离度量
 * @param query 查询向量 [dims]
 * @param table 输出距离表 [dims]
 * @return 0 成功, -1 失败
 */
int32_t sq_compute_distance_table(const quantizer_t *q,
                                   distance_metric_t metric,
                                   const float *query,
                                   float *table);

/**
 * SQ 计算 ADC 距离
 * @param q 量化器
 * @param code 码字
 * @param table 距离表（查询向量）
 * @return 距离值
 */
float sq_adc_distance(const quantizer_t *q, const uint8_t *code, const float *table);

/**
 * SQ 码字大小
 * @param q 量化器
 * @return 码字字节数: 8 (header) + ceil(dims * sq_bits / 8)
 */
int32_t sq_code_size(const quantizer_t *q);

/**
 * SQ 距离表大小
 * @param q 量化器
 * @return 距离表 float 数量: dims
 */
int32_t sq_distance_table_size(const quantizer_t *q);

/**
 * SQ 模型参数数量
 * @param q 量化器
 * @return float 数量: 2 (global_min, scale)
 */
int32_t sq_model_float_count(const quantizer_t *q);

/**
 * SQ 导出模型
 * @param q 量化器
 * @param params 输出参数 [2]: [global_min, scale]
 * @return 0 成功, -1 失败
 */
int32_t sq_export_model(const quantizer_t *q, float *params);

/**
 * SQ 导入模型
 * @param q 量化器
 * @param params 输入参数 [2]: [global_min, scale]
 * @return 0 成功, -1 失败
 */
int32_t sq_load_model(quantizer_t *q, const float *params);

#endif /* QUANTIZATION_SQ_H */
