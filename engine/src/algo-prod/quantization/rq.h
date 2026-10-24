#ifndef QUANTIZATION_RQ_H
#define QUANTIZATION_RQ_H

#include "quantization_private.h"

/* ── RabitQ 量化器内部函数 ── */

/**
 * RabitQ 初始化
 * 分配 PQ 码本和残差步长数组
 * @param q 量化器
 * @return 0 成功, -1 失败
 */
int32_t rq_init(quantizer_t *q);

/**
 * RabitQ 释放资源
 * @param q 量化器
 */
void rq_free(quantizer_t *q);

/**
 * RabitQ 重置状态
 * @param q 量化器
 */
void rq_reset(quantizer_t *q);

/**
 * RabitQ 两级训练
 * 1. PQ 码本训练 (调用 pq_train)
 * 2. 残差统计计算
 * @param q 量化器
 * @param n 向量数量
 * @param vectors 训练向量 [n][dims]
 * @return 0 成功, -1 失败
 */
int32_t rq_train(quantizer_t *q, int32_t n, const float *vectors);

/**
 * RabitQ 两级编码
 * @param q 量化器
 * @param vector 输入向量 [dims]
 * @param code 输出码字: [pq_codes][residual_codes]
 * @return 0 成功, -1 失败
 */
int32_t rq_encode(const quantizer_t *q, const float *vector, uint8_t *code);

/**
 * RabitQ 批量编码
 * @param q 量化器
 * @param n 向量数量
 * @param vectors 输入向量 [n][dims]
 * @param codes 输出码字 [n][code_size]
 * @return 0 成功, -1 失败
 */
int32_t rq_encode_batch(const quantizer_t *q, int32_t n, const float *vectors, uint8_t *codes);

/**
 * RabitQ 计算距离表（PQ 距离表）
 * @param q 量化器
 * @param metric 距离度量
 * @param query 查询向量 [dims]
 * @param table 输出距离表 [pq_subquantizers][codebook_size]
 * @return 0 成功, -1 失败
 */
int32_t rq_compute_distance_table(const quantizer_t *q,
                                   distance_metric_t metric,
                                   const float *query,
                                   float *table);

/**
 * RabitQ 计算 ADC 距离
 * PQ 距离 + 残差校正
 * @param q 量化器
 * @param code 码字
 * @param table PQ 距离表
 * @return 距离值
 */
float rq_adc_distance(const quantizer_t *q, const uint8_t *code, const float *table);

/**
 * RabitQ 码字大小
 * PQ 码 + 残差码
 * @param q 量化器
 * @return 码字字节数: m + ceil(m * rq_residual_bits / 8)
 */
int32_t rq_code_size(const quantizer_t *q);

/**
 * RabitQ 距离表大小
 * @param q 量化器
 * @return 距离表 float 数量
 */
int32_t rq_distance_table_size(const quantizer_t *q);

/**
 * RabitQ 模型参数数量
 * @param q 量化器
 * @return float 数量（PQ 码本 + 残差步长）
 */
int32_t rq_model_float_count(const quantizer_t *q);

/**
 * RabitQ 导出模型
 * @param q 量化器
 * @param codebooks 输出码本
 * @param count 码本缓冲区大小
 * @param trained_codebook_size 输出实际码本大小
 * @return 0 成功, -1 失败
 */
int32_t rq_export_model(const quantizer_t *q,
                        float *codebooks,
                        int32_t count,
                        int32_t *trained_codebook_size);

/**
 * RabitQ 导入模型
 * @param q 量化器
 * @param trained_codebook_size 码本大小
 * @param codebooks 输入码本
 * @param count 码本数量
 * @return 0 成功, -1 失败
 */
int32_t rq_load_model(quantizer_t *q,
                      int32_t trained_codebook_size,
                      const float *codebooks,
                      int32_t count);

#endif /* QUANTIZATION_RQ_H */
