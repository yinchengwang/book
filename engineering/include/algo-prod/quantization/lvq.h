#ifndef QUANTIZATION_LVQ_H
#define QUANTIZATION_LVQ_H

#include "quantization_private.h"

/* ── 内部 LVQ 函数，仅供 quantization.c 调用 ── */

/*
 * LVQ（Locally-adaptive Vector Quantization）内部接口
 *
 * 设计原理（参考 DiskANN/SVS LVQ 方案）：
 *   - 每个向量独立量化：记录其 scale（范围）和 bias（最小值）
 *   - 各分量量化为 lvq_bits 位（支持 4 位 / 8 位）
 *   - 码字布局：[float32 scale][float32 bias][打包的量化分量...]
 *
 * 与 PQ 的区别：
 *   - PQ：全局码本（需训练），码字是码心索引
 *   - LVQ：无码本，每向量局部缩放，码字是量化后的分量值
 *
 * 无需训练（init 即可编码），train 为可选 NOP（调用后将 trained 置 true）。
 *
 * 距离计算（ADC）：
 *   compute_distance_table 将查询向量原样存入 table（dims 个 float）；
 *   adc_distance 解码向量近似值后与查询向量计算 L2 距离。
 */

void    lvq_init(quantizer_t *q);
void    lvq_reset(quantizer_t *q);

int32_t lvq_code_size(const quantizer_t *q);
int32_t lvq_distance_table_size(const quantizer_t *q);

int32_t lvq_encode(const quantizer_t *q, const float *vector, uint8_t *code);
int32_t lvq_encode_batch(const quantizer_t *q, int32_t n, const float *vectors, uint8_t *codes);

int32_t lvq_compute_distance_table(const quantizer_t *q,
                                         distance_metric_t metric,
                                         const float *query,
                                         float *table);
float   lvq_adc_distance(const quantizer_t *q,
                               const uint8_t *code,
                               const float *distance_table);

#endif /* QUANTIZATION_LVQ_H */
