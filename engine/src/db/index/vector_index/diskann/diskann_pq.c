/*
 * diskann_pq.c
 *
 * DiskANN PQ 量化集成实现。
 *
 * 本模块封装 PQ（Product Quantization）量化的训练、编码和解码，
 * 支持 OPQ（Optimized PQ）旋转优化以提升量化精度。
 */

#include "diskann_private.h"

#include <algo-prod/quantization/quantization.h>

/* ============================================================================
 * 常量定义
 * ============================================================================ */

/** 默认 PQ 子空间数（维度被均匀划分） */
#define DISKANN_DEFAULT_PQ_M 16

/** 默认 PQ 编码位宽 */
#define DISKANN_DEFAULT_PQ_BITS 8

/** 默认训练向量数 */
#define DISKANN_DEFAULT_TRAIN_MAX 256

/* ============================================================================
 * OPQ 辅助函数
 * ============================================================================ */

/**
 * @brief 检查是否启用 OPQ
 */
bool diskann_pq_opq_enabled(const diskann_t *index)
{
    if (!index || !index->quantizer) {
        return false;
    }

    /* 检查量化器类型是否为 PQ */
    if (quantizer_type(index->quantizer) != QUANTIZATION_TYPE_PQ) {
        return false;
    }

    /* 检查是否有 OPQ 旋转矩阵 */
    return false; /* TODO: 实际检查 quantizer 中是否启用了 OPQ */
}

/**
 * @brief 启用 OPQ 旋转优化
 *
 * OPQ 通过在训练前对数据进行 PCA 旋转，使各子空间的方差更均衡，
 * 从而提升量化精度。
 *
 * @param[inout] index 索引
 * @return 0 成功，非 0 失败
 */
int diskann_pq_enable_opq(diskann_t *index)
{
    if (!index || !diskann_pq_enabled(index)) {
        return -1;
    }

    if (!index->quantizer) {
        return -1;
    }

    /* 调用量化器的 OPQ 启用函数 */
    if (quantizer_enable_opq(index->quantizer) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief 获取 OPQ 旋转矩阵
 *
 * @param[in] index 索引
 * @param[out] rotation_matrix 旋转矩阵输出（dims × dims）
 * @param[in] matrix_size 矩阵大小
 * @return 0 成功，非 0 失败
 */
int diskann_pq_get_opq_rotation(const diskann_t *index,
                                float *rotation_matrix,
                                int32_t matrix_size)
{
    if (!index || !rotation_matrix) {
        return -1;
    }

    if (!index->quantizer) {
        return -1;
    }

    return quantizer_get_opq_rotation(index->quantizer,
                                       rotation_matrix,
                                       matrix_size);
}

/* ============================================================================
 * PQ 训练与编码
 * ============================================================================ */

/**
 * @brief 训练 PQ 量化器
 *
 * 从索引中采集训练样本，训练 PQ 码本。
 *
 * @param[inout] index 索引
 * @param[in] train_max_vectors 最大训练向量数（0 表示使用默认值）
 * @return 0 成功，非 0 失败
 */
int diskann_pq_train(diskann_t *index, int32_t train_max_vectors)
{
    float *training_vectors;
    int32_t training_count;
    int32_t max_train;

    if (!index) {
        return -1;
    }

    /* 确保量化器已创建 */
    if (diskann_ensure_quantizer(index) != 0) {
        return -1;
    }

    if (!index->quantizer) {
        return -1;
    }

    /* 确定训练样本数 */
    max_train = train_max_vectors > 0 ? train_max_vectors : index->quantization_params.train_max_vectors;
    if (max_train <= 0) {
        max_train = DISKANN_DEFAULT_TRAIN_MAX;
    }

    /* 采集训练样本 */
    if (diskann_gather_training_vectors(index, &training_vectors, &training_count) != 0) {
        return -1;
    }

    if (training_count <= 0) {
        free(training_vectors);
        return -1;
    }

    /* 重置并训练量化器 */
    quantizer_reset(index->quantizer);

    if (quantizer_train(index->quantizer, training_count, training_vectors) != 0) {
        free(training_vectors);
        return -1;
    }

    free(training_vectors);

    /* 计算码本大小 */
    index->pq_trained_codebook_size = quantizer_distance_table_size(index->quantizer) /
                                      diskann_max_i32(index->quantization_params.pq_m, 1);

    return 0;
}

/**
 * @brief 批量编码向量
 *
 * @param[inout] index 索引
 * @param[in] start_id 起始向量 ID
 * @param[in] count 向量数量
 * @return 0 成功，非 0 失败
 */
int diskann_pq_encode_batch(diskann_t *index, int32_t start_id, int32_t count)
{
    int32_t i;

    if (!index || !diskann_pq_ready(index)) {
        return -1;
    }

    if (start_id < 0 || count <= 0 || start_id + count > index->n_total) {
        return -1;
    }

    /* 确保编码存储空间足够 */
    if (diskann_ensure_code_storage(index) != 0) {
        return -1;
    }

    /* 批量编码 */
    for (i = start_id; i < start_id + count; i++) {
        if (index->deleted[i]) {
            continue;
        }

        if (quantizer_encode(index->quantizer,
                             &index->vectors[i * index->dims],
                             &index->codes[i * index->pq_code_size]) != 0) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 解码单个向量
 *
 * 从 PQ 编码恢复近似原始向量。
 *
 * @param[in] index 索引
 * @param[in] id 向量 ID
 * @param[out] vector_out 解码向量输出
 * @return 0 成功，非 0 失败
 */
int diskann_pq_decode(const diskann_t *index, int32_t id, float *vector_out)
{
    const uint8_t *code;
    float *distance_table;
    int32_t table_size;
    int32_t m;
    int32_t sub_dim;
    int32_t i;
    int32_t j;

    if (!index || !diskann_pq_ready(index) || !vector_out) {
        return -1;
    }

    if (id < 0 || id >= index->n_total || !index->codes) {
        return -1;
    }

    code = &index->codes[id * index->pq_code_size];

    /* 获取距离表 */
    table_size = quantizer_distance_table_size(index->quantizer);
    distance_table = (float *)malloc((size_t)table_size * sizeof(float));
    if (!distance_table) {
        return -1;
    }

    /* 用零向量查询获取码字中心 */
    {
        float *zero_query = (float *)calloc((size_t)index->dims, sizeof(float));
        if (!zero_query) {
            free(distance_table);
            return -1;
        }

        /* 计算到各码字的距离表 */
        quantizer_compute_distance_table(index->quantizer,
                                         index->metric,
                                         zero_query,
                                         distance_table);

        free(zero_query);
    }

    /* 重建向量 */
    m = index->quantization_params.pq_m;
    sub_dim = index->dims / m;

    for (i = 0; i < m; i++) {
        uint8_t code_val = code[i];
        int32_t base = i * 256; /* 假设 8 bits */
        const float *centroid = (const float *)(((const uint8_t *)index->quantizer) + base * sizeof(float));

        (void)centroid; /* 实际需要从量化器获取码字 */

        /* 累加到输出向量 */
        for (j = 0; j < sub_dim; j++) {
            /* TODO: 实际从码本获取并累加 */
            vector_out[i * sub_dim + j] = 0.0f;
        }
    }

    free(distance_table);
    return 0;
}

/* ============================================================================
 * PQ 距离计算
 * ============================================================================ */

/**
 * @brief 计算 PQ 近似距离
 *
 * 使用非对称距离（ADC）计算查询向量与编码向量间的近似距离。
 *
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[in] id 向量 ID
 * @return 近似距离
 */
float diskann_pq_approx_distance(const diskann_t *index,
                                 const float *query,
                                 int32_t id)
{
    float *distance_table;
    int32_t table_size;

    if (!index || !query || !diskann_pq_ready(index)) {
        return FLT_MAX;
    }

    if (id < 0 || id >= index->n_total || !index->codes) {
        return FLT_MAX;
    }

    table_size = quantizer_distance_table_size(index->quantizer);
    distance_table = (float *)malloc((size_t)table_size * sizeof(float));
    if (!distance_table) {
        return FLT_MAX;
    }

    /* 计算距离表 */
    if (quantizer_compute_distance_table(index->quantizer,
                                         index->metric,
                                         query,
                                         distance_table) != 0) {
        free(distance_table);
        return FLT_MAX;
    }

    /* 计算 ADC 距离 */
    {
        float dist = quantizer_adc_distance(index->quantizer,
                                            &index->codes[id * index->pq_code_size],
                                            distance_table);
        free(distance_table);
        return dist;
    }
}

/* ============================================================================
 * PQ 配置与信息
 * ============================================================================ */

/**
 * @brief 获取 PQ 压缩率
 *
 * @param[in] index 索引
 * @return 压缩率（原始大小 / 压缩后大小）
 */
float diskann_pq_compression_ratio(const diskann_t *index)
{
    if (!index || index->dims <= 0 || index->pq_code_size <= 0) {
        return 0.0f;
    }

    return (float)(index->dims * (int32_t)sizeof(float)) / (float)index->pq_code_size;
}

/**
 * @brief 获取 PQ 内存占用
 *
 * @param[in] index 索引
 * @return PQ 相关内存占用（字节）
 */
int64_t diskann_pq_memory_usage(const diskann_t *index)
{
    int64_t total = 0;

    if (!index) {
        return 0;
    }

    /* 编码存储 */
    if (index->codes && index->n_total > 0 && index->pq_code_size > 0) {
        total += (int64_t)index->n_total * (int64_t)index->pq_code_size;
    }

    /* 码本存储 */
    if (index->quantizer && index->pq_trained_codebook_size > 0) {
        total += (int64_t)index->pq_trained_codebook_size * (int64_t)sizeof(float);
    }

    return total;
}

/**
 * @brief 设置 PQ 参数
 *
 * @param[inout] index 索引
 * @param[in] m 子空间数
 * @param[in] bits 编码位宽
 * @param[in] train_max 训练样本数
 * @return 0 成功，非 0 失败
 */
int diskann_pq_set_params(diskann_t *index, int32_t m, int32_t bits, int32_t train_max)
{
    diskann_quantization_params_t params;

    if (!index) {
        return -1;
    }

    params.enabled = true;
    params.pq_m = m > 0 ? m : DISKANN_DEFAULT_PQ_M;
    params.pq_bits = bits > 0 ? bits : DISKANN_DEFAULT_PQ_BITS;
    params.train_max_vectors = train_max > 0 ? train_max : DISKANN_DEFAULT_TRAIN_MAX;

    return diskann_index_set_quantization_params(index, &params);
}

/**
 * @brief 获取 PQ 信息字符串
 *
 * @param[in] index 索引
 * @param[out] info_out 信息字符串缓冲区
 * @param[in] max_len 缓冲区长度
 */
void diskann_pq_get_info(const diskann_t *index, char *info_out, int32_t max_len)
{
    int32_t len;

    if (!index || !info_out || max_len <= 0) {
        return;
    }

    len = snprintf(info_out, (size_t)max_len,
                   "PQ[m=%d, bits=%d, code_size=%d, compression=%.2fx, memory=%lld bytes]",
                   index->quantization_params.pq_m,
                   index->quantization_params.pq_bits,
                   index->pq_code_size,
                   diskann_pq_compression_ratio(index),
                   (long long)diskann_pq_memory_usage(index));

    if (len < 0 || len >= max_len) {
        info_out[max_len - 1] = '\0';
    }
}
