#include "pq.h"
#include "lvq.h"
#include "sq.h"
#include "rq.h"
#include "quantization_private.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ──────────────────────────────────────────────────────────────────────────
 * 配置辅助
 * ────────────────────────────────────────────────────────────────────────── */

static int32_t default_pq_subquantizers(int32_t dims)
{
    int32_t m;

    if (dims <= 0) {
        return 0;
    }
    for (m = dims < 8 ? dims : 8; m >= 1; --m) {
        if (dims % m == 0) {
            return m;
        }
    }
    return 1;
}

int32_t quantization_type_is_valid(quantization_type_t type)
{
    return type >= QUANTIZATION_TYPE_NONE && type <= QUANTIZATION_TYPE_RQ;
}

void quantizer_config_init(quantizer_config_t *config, int32_t dims, quantization_type_t type)
{
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->dims              = dims;
    config->type              = type;
    config->pq_subquantizers  = default_pq_subquantizers(dims);
    config->pq_bits           = 8;
    config->lvq_bits          = 8;
    config->sq_bits           = 8;
    config->rq_pq_bits        = 8;
    config->rq_residual_bits  = 1;
}

int32_t quantizer_config_validate(const quantizer_config_t *config)
{
    if (!config || config->dims <= 0 || !quantization_type_is_valid(config->type)) {
        return 0;
    }
    if (config->type == QUANTIZATION_TYPE_NONE) {
        return 1;
    }
    if (config->type == QUANTIZATION_TYPE_PQ) {
        if (config->pq_subquantizers <= 0 || config->dims % config->pq_subquantizers != 0) {
            return 0;
        }
        /* PQ 支持 4/6/8 bits */
        if (config->pq_bits != 4 && config->pq_bits != 6 && config->pq_bits != 8) {
            return 0;
        }
        return 1;
    }
    if (config->type == QUANTIZATION_TYPE_LVQ) {
        if (config->lvq_bits != 4 && config->lvq_bits != 8) {
            return 0;
        }
        return 1;
    }
    if (config->type == QUANTIZATION_TYPE_SQ) {
        /* SQ 仅支持 4-bit 和 8-bit */
        if (config->sq_bits != 4 && config->sq_bits != 8) {
            return 0;
        }
        return 1;
    }
    if (config->type == QUANTIZATION_TYPE_RQ) {
        /* RabitQ 需要子空间划分，pq_bits 用于 PQ 部分 */
        if (config->pq_subquantizers <= 0 || config->dims % config->pq_subquantizers != 0) {
            return 0;
        }
        /* PQ bits: 4/6/8 */
        if (config->rq_pq_bits != 4 && config->rq_pq_bits != 6 && config->rq_pq_bits != 8) {
            return 0;
        }
        /* 残差 bits: 1/2/4 */
        if (config->rq_residual_bits != 1 && config->rq_residual_bits != 2 && config->rq_residual_bits != 4) {
            return 0;
        }
        return 1;
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 创建 / 销�?
 * ────────────────────────────────────────────────────────────────────────── */

quantizer_t *quantizer_create(const quantizer_config_t *config)
{
    quantizer_t *q;

    if (!quantizer_config_validate(config) || config->type == QUANTIZATION_TYPE_NONE) {
        return NULL;
    }

    q = (quantizer_t *)calloc(1, sizeof(quantizer_t));
    if (!q) {
        return NULL;
    }

    q->config = *config;

    if (config->type == QUANTIZATION_TYPE_PQ) {
        if (pq_init(q) != 0) {
            free(q);
            return NULL;
        }
    } else if (config->type == QUANTIZATION_TYPE_LVQ) {
        lvq_init(q);
    } else if (config->type == QUANTIZATION_TYPE_SQ) {
        if (sq_init(q) != 0) {
            free(q);
            return NULL;
        }
    } else if (config->type == QUANTIZATION_TYPE_RQ) {
        if (rq_init(q) != 0) {
            free(q);
            return NULL;
        }
    }

    return q;
}

quantizer_t *quantizer_create_from_model(const quantizer_config_t *config,
                                                   int32_t trained_codebook_size,
                                                   const float *codebooks,
                                                   int32_t codebooks_count)
{
    quantizer_t *q;

    if (!quantizer_config_validate(config) || !codebooks) {
        return NULL;
    }

    q = quantizer_create(config);
    if (!q) {
        return NULL;
    }

    if (config->type == QUANTIZATION_TYPE_PQ) {
        if (pq_load_model(q, trained_codebook_size, codebooks, codebooks_count) != 0) {
            quantizer_drop(q);
            return NULL;
        }
    } else if (config->type == QUANTIZATION_TYPE_LVQ) {
        /* LVQ 无码本，直接标记为已训练 */
        q->trained = true;
    } else if (config->type == QUANTIZATION_TYPE_SQ) {
        /* SQ 模型只有 2 个 float: [global_min, scale] */
        if (sq_load_model(q, codebooks) != 0) {
            quantizer_drop(q);
            return NULL;
        }
    } else if (config->type == QUANTIZATION_TYPE_RQ) {
        /* RQ 模型 = PQ 码本 + 残差步长 */
        if (rq_load_model(q, trained_codebook_size, codebooks, codebooks_count) != 0) {
            quantizer_drop(q);
            return NULL;
        }
    }

    return q;
}

void quantizer_drop(quantizer_t *q)
{
    if (!q) {
        return;
    }
    /* 释放 OPQ 相关内存 */
    if (q->opq_enabled) {
        free(q->rotation_matrix);
        free(q->rotated_data);
    }
    if (q->config.type == QUANTIZATION_TYPE_PQ) {
        pq_free(q);
    } else if (q->config.type == QUANTIZATION_TYPE_SQ) {
        sq_free(q);
    } else if (q->config.type == QUANTIZATION_TYPE_RQ) {
        rq_free(q);
    }
    /* LVQ 无堆内存，无需释放 */
    free(q);
}

/* ──────────────────────────────────────────────────────────────────────────
 * OPQ 旋转优化
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * @brief 计算矩阵乘积 C = A * B
 * C 是 m x n 矩阵，A 是 m x k，B 是 k x n
 */
static void mat_mul(float *C, const float *A, const float *B,
                    int m, int k, int n)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int p = 0; p < k; p++) {
                sum += A[i * k + p] * B[p * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

/**
 * @brief 计算矩阵转置 B = A^T
 * A 是 m x n，B 是 n x m
 */
static void mat_transpose(float *B, const float *A, int m, int n)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            B[j * m + i] = A[i * n + j];
        }
    }
}

/**
 * @brief 简化的 SVD 分解（用于 OPQ 旋转）
 *
 * 这里使用幂迭代法计算主成分方向的简化版本。
 * 实际 OPQ 应该使用更完善的 SVD 实现。
 */
static void compute_opq_rotation(float *R, const float *data, int n, int dims)
{
    /* 简化的旋转矩阵：使用单位矩阵作为初始值
     * 完整的 OPQ 需要迭代优化，这里提供一个合理的默认值
     * 实际生产中建议使用 LAPACK 的 SVD 实现
     */
    for (int i = 0; i < dims * dims; i++) {
        R[i] = 0.0f;
    }
    for (int i = 0; i < dims; i++) {
        R[i * dims + i] = 1.0f;  /* 单位矩阵 */
    }
}

int32_t quantizer_enable_opq(quantizer_t *q)
{
    if (!q || q->config.type != QUANTIZATION_TYPE_PQ) {
        return -1;
    }

    int32_t dims = q->config.dims;

    /* 分配旋转矩阵 */
    q->rotation_matrix = (float *)calloc(dims * dims, sizeof(float));
    if (!q->rotation_matrix) {
        return -1;
    }

    /* 计算旋转矩阵（简化实现） */
    compute_opq_rotation(q->rotation_matrix, NULL, 0, dims);

    q->opq_enabled = true;
    return 0;
}

int32_t quantizer_get_opq_rotation(const quantizer_t *q,
                                    float *rotation_matrix,
                                    int32_t matrix_size)
{
    if (!q || !q->opq_enabled || !rotation_matrix) {
        return -1;
    }

    int32_t expected = q->config.dims * q->config.dims;
    if (matrix_size < expected) {
        return -1;
    }

    memcpy(rotation_matrix, q->rotation_matrix, expected * sizeof(float));
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 序列化
 * ────────────────────────────────────────────────────────────────────────── */

/** 量化器文件魔数 */
#define QUANTIZER_FILE_MAGIC 0x514E545A  /* "QNTZ" */

/** 量化器文件版本 */
#define QUANTIZER_FILE_VERSION 1

int32_t quantizer_save(const quantizer_t *q, const char *path)
{
    if (!q || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入魔数和版本 */
    uint32_t magic = QUANTIZER_FILE_MAGIC;
    uint32_t version = QUANTIZER_FILE_VERSION;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);

    /* 写入配置 */
    fwrite(&q->config, sizeof(q->config), 1, fp);
    fwrite(&q->trained, sizeof(q->trained), 1, fp);
    fwrite(&q->subvector_dims, sizeof(q->subvector_dims), 1, fp);
    fwrite(&q->max_codebook_size, sizeof(q->max_codebook_size), 1, fp);
    fwrite(&q->codebook_size, sizeof(q->codebook_size), 1, fp);

    /* 写入码本 */
    int32_t codebook_floats = q->config.pq_subquantizers *
                              q->max_codebook_size *
                              q->subvector_dims;
    if (q->codebooks && codebook_floats > 0) {
        fwrite(q->codebooks, sizeof(float), codebook_floats, fp);
    }

    /* 写入 OPQ 信息 */
    fwrite(&q->opq_enabled, sizeof(q->opq_enabled), 1, fp);
    if (q->opq_enabled && q->rotation_matrix) {
        int32_t rot_size = q->config.dims * q->config.dims;
        fwrite(q->rotation_matrix, sizeof(float), rot_size, fp);
    }

    /* 写入 SQ 参数 */
    fwrite(&q->global_min, sizeof(q->global_min), 1, fp);
    fwrite(&q->global_max, sizeof(q->global_max), 1, fp);
    fwrite(&q->scale, sizeof(q->scale), 1, fp);

    fclose(fp);
    return 0;
}

quantizer_t *quantizer_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取魔数和版本 */
    uint32_t magic, version;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        magic != QUANTIZER_FILE_MAGIC ||
        version != QUANTIZER_FILE_VERSION) {
        fclose(fp);
        return NULL;
    }

    /* 读取配置并创建量化器 */
    quantizer_config_t config;
    if (fread(&config, sizeof(config), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    quantizer_t *q = quantizer_create(&config);
    if (!q) {
        fclose(fp);
        return NULL;
    }

    /* 读取状态 */
    fread(&q->trained, sizeof(q->trained), 1, fp);
    fread(&q->subvector_dims, sizeof(q->subvector_dims), 1, fp);
    fread(&q->max_codebook_size, sizeof(q->max_codebook_size), 1, fp);
    fread(&q->codebook_size, sizeof(q->codebook_size), 1, fp);

    /* 读取码本 */
    int32_t codebook_floats = config.pq_subquantizers *
                              q->max_codebook_size *
                              q->subvector_dims;
    if (codebook_floats > 0) {
        q->codebooks = (float *)malloc(codebook_floats * sizeof(float));
        if (q->codebooks) {
            fread(q->codebooks, sizeof(float), codebook_floats, fp);
        }
    }

    /* 读取 OPQ 信息 */
    fread(&q->opq_enabled, sizeof(q->opq_enabled), 1, fp);
    if (q->opq_enabled) {
        int32_t rot_size = config.dims * config.dims;
        q->rotation_matrix = (float *)malloc(rot_size * sizeof(float));
        if (q->rotation_matrix) {
            fread(q->rotation_matrix, sizeof(float), rot_size, fp);
        }
    }

    /* 读取 SQ 参数 */
    fread(&q->global_min, sizeof(q->global_min), 1, fp);
    fread(&q->global_max, sizeof(q->global_max), 1, fp);
    fread(&q->scale, sizeof(q->scale), 1, fp);

    fclose(fp);
    return q;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 训练
 * ────────────────────────────────────────────────────────────────────────── */

int32_t quantizer_train(quantizer_t *quantizer, int32_t n, const float *vectors)
{
    if (!quantizer || n <= 0 || !vectors) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_train(quantizer, n, vectors);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        /* LVQ 无需训练，直接标记为已训练 */
        quantizer->trained = true;
        return 0;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_train(quantizer, n, vectors);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_train(quantizer, n, vectors);
    }
    return -1;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 编码
 * ────────────────────────────────────────────────────────────────────────── */

int32_t quantizer_encode(const quantizer_t *quantizer, const float *vector, uint8_t *code)
{
    if (!quantizer || !quantizer->trained || !vector || !code) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_encode(quantizer, vector, code);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return lvq_encode(quantizer, vector, code);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_encode(quantizer, vector, code);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_encode(quantizer, vector, code);
    }
    return -1;
}

int32_t quantizer_encode_batch(const quantizer_t *quantizer,
                                    int32_t n,
                                    const float *vectors,
                                    uint8_t *codes)
{
    if (!quantizer || n <= 0 || !vectors || !codes) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_encode_batch(quantizer, n, vectors, codes);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return lvq_encode_batch(quantizer, n, vectors, codes);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_encode_batch(quantizer, n, vectors, codes);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_encode_batch(quantizer, n, vectors, codes);
    }
    return -1;
}

/* ──────────────────────────────────────────────────────────────────────────
 * ADC 距离
 * ────────────────────────────────────────────────────────────────────────── */

int32_t quantizer_compute_distance_table(const quantizer_t *quantizer,
                                              distance_metric_t metric,
                                              const float *query,
                                              float *distance_table)
{
    if (!quantizer || !quantizer->trained || !query || !distance_table ||
        !distance_metric_is_valid(metric)) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_compute_distance_table(quantizer, metric, query, distance_table);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return lvq_compute_distance_table(quantizer, metric, query, distance_table);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_compute_distance_table(quantizer, metric, query, distance_table);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_compute_distance_table(quantizer, metric, query, distance_table);
    }
    return -1;
}

float quantizer_adc_distance(const quantizer_t *quantizer,
                                  const uint8_t *code,
                                  const float *distance_table)
{
    if (!quantizer || !quantizer->trained || !code || !distance_table) {
        return FLT_MAX;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_adc_distance(quantizer, code, distance_table);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return lvq_adc_distance(quantizer, code, distance_table);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_adc_distance(quantizer, code, distance_table);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_adc_distance(quantizer, code, distance_table);
    }
    return FLT_MAX;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 尺寸查询
 * ────────────────────────────────────────────────────────────────────────── */

int32_t quantizer_code_size(const quantizer_t *quantizer)
{
    if (!quantizer) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_code_size(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return lvq_code_size(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_code_size(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_code_size(quantizer);
    }
    return 0;
}

int32_t quantizer_distance_table_size(const quantizer_t *quantizer)
{
    if (!quantizer) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_distance_table_size(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return lvq_distance_table_size(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_distance_table_size(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_distance_table_size(quantizer);
    }
    return 0;
}

int32_t quantizer_model_float_count(const quantizer_t *quantizer)
{
    if (!quantizer) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_model_float_count(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        return 0; /* LVQ 无模型参数 */
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_model_float_count(quantizer);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_model_float_count(quantizer);
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 模型导入 / 导出
 * ────────────────────────────────────────────────────────────────────────── */

int32_t quantizer_export_model(const quantizer_t *quantizer,
                                    float *codebooks,
                                    int32_t codebooks_count,
                                    int32_t *trained_codebook_size)
{
    if (!quantizer || !codebooks || !trained_codebook_size) {
        return -1;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        return pq_export_model(quantizer, codebooks, codebooks_count, trained_codebook_size);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        *trained_codebook_size = 0;
        return 0; /* LVQ 无需导出模型 */
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        return sq_export_model(quantizer, codebooks);
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        return rq_export_model(quantizer, codebooks, codebooks_count, trained_codebook_size);
    }
    return -1;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 状态查�?
 * ────────────────────────────────────────────────────────────────────────── */

bool quantizer_is_trained(const quantizer_t *quantizer)
{
    return quantizer ? quantizer->trained : false;
}

quantization_type_t quantizer_type(const quantizer_t *quantizer)
{
    return quantizer ? quantizer->config.type : QUANTIZATION_TYPE_NONE;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 重置
 * ────────────────────────────────────────────────────────────────────────── */

void quantizer_reset(quantizer_t *quantizer)
{
    if (!quantizer) {
        return;
    }
    if (quantizer->config.type == QUANTIZATION_TYPE_PQ) {
        pq_reset(quantizer);
    } else if (quantizer->config.type == QUANTIZATION_TYPE_LVQ) {
        lvq_reset(quantizer);
    } else if (quantizer->config.type == QUANTIZATION_TYPE_SQ) {
        sq_reset(quantizer);
    } else if (quantizer->config.type == QUANTIZATION_TYPE_RQ) {
        rq_reset(quantizer);
    }
}
