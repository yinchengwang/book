#ifndef QUANTIZATION_PRIVATE_H
#define QUANTIZATION_PRIVATE_H

#include <algo-prod/quantization/quantization.h>
#include <algo-prod/distance/distance.h>

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * 统一量化器内部结构体
 *
 * PQ 字段  ：type == QUANTIZATION_TYPE_PQ 时有效
 * LVQ 字段 ：type == QUANTIZATION_TYPE_LVQ 时有效
 *           （LVQ 无需堆内存，所有参数均可由 config 推导）
 * SQ 字段  ：type == QUANTIZATION_TYPE_SQ 时有效
 * RQ 字段  ：type == QUANTIZATION_TYPE_RQ 时有效（RabitQ 残差量化）
 */
struct quantizer {
    quantizer_config_t config;
    bool trained;

    /* ── PQ ── */
    int32_t subvector_dims;    /* 子向量维度 = dims / pq_subquantizers */
    int32_t max_codebook_size; /* 最大码本大小 = 2^pq_bits */
    int32_t codebook_size;     /* 实际码本大小（训练样本不足时 < max） */
    float  *codebooks;         /* [pq_subquantizers][max_codebook_size][subvector_dims] */

    /* ── OPQ (Optimized PQ) 旋转优化 ── */
    bool opq_enabled;              /* 是否启用 OPQ */
    float *rotation_matrix;        /* 旋转矩阵 [dims x dims]（列优先存储） */
    float *rotated_data;           /* 训练时存储旋转后的数据（临时） */

    /* ── SQ (Scalar Quantization) ── */
    float global_min;  /* 全局最小值 */
    float global_max;  /* 全局最大值 */
    float scale;       /* 量化步长 = (max - min) / (2^sq_bits - 1) */

    /* ── RQ (RabitQ - Residual Quantization) ── */
    /* RQ 使用 PQ 码本结构 (codebooks)，额外存储残差参数 */
    float *residual_steps;  /* 每个子空间的残差量化步长 [pq_subquantizers] */
};

#endif /* QUANTIZATION_PRIVATE_H */
