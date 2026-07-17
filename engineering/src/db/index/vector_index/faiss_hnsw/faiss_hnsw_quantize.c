// faiss_hnsw_quantize.c
// HNSW 索引的量化（Quantization）支持模块
//
// 与 algo-prod/quantization/quantization.h 中的 quantizer_t 统一接口集成：
//   - faiss_hnsw_quantize_get_code_size：根据量化类型计算单个向量的编码字节数
//   - faiss_hnsw_quantize_train：创建并训练量化器（统计 min/max、codebook 等）
//   - faiss_hnsw_quantize_encode：将单个 float 向量编码为 uint8_t 码字
//   - faiss_hnsw_quantize_decode：将单个 uint8_t 码字解码为 float 向量
//   - faiss_hnsw_quantize_encode_batch：批量编码
//   - faiss_hnsw_quantize_compute_distance_table：为查询向量计算距离表（用于 ADC）
//   - faiss_hnsw_quantize_adc_distance：使用距离表和码字计算近似距离
//   - faiss_hnsw_quantize_destroy：释放量化器资源
//
// 当 quantization_type != QUANTIZATION_TYPE_NONE 时：
//   - 构建阶段：插入向量时同步编码存储到 codes 数组
//   - 搜索阶段：搜索层用量化距离（ADC/PQ table lookup）
//             底层精确重排序用解码向量
//
// 参考 FAISS HNSW.cpp 中 Quantizer 接口与量化集成方式。

#include "faiss_hnsw_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// 获取量化编码大小（字节）
//
// 各量化类型码字大小估算（与 FAISS 约定一致）：
//   - SQ  (8-bit 标量量化): dims 字节（每维 1 byte）
//   - PQ  (乘积量化):        ceil(dims/16) 字节（每 16 维 1 byte 子空间）
//   - LVQ (局部自适应):      1 字节（高度压缩，向量级 ID）
//   - RQ  (RaBitQ 残差量化): ceil(dims/4) 字节（每 4 维 1 byte）
//
// 注意：实际码字大小最终以 quantizer_code_size() 为准（训练后可能略有差异）
// =============================================================================
int32_t faiss_hnsw_quantize_get_code_size(quantization_type_t type, int32_t dims) {
    if (dims <= 0) {
        return 0;
    }

    switch (type) {
        case QUANTIZATION_TYPE_SQ:
            // SQ8: 每维 1 字节（加上 8 字节 header 用于存储 min/scale，实际由 quantizer 决定）
            return dims;
        case QUANTIZATION_TYPE_PQ:
            // PQ: 每 16 维 1 字节（典型子空间大小，pq_m=16 时）
            return (dims + 15) / 16;
        case QUANTIZATION_TYPE_LVQ:
            // LVQ: 每向量 1 字节（高度压缩）
            return 1;
        case QUANTIZATION_TYPE_RQ:
            // RQ: 每 4 维 1 字节（残差量化）
            return (dims + 3) / 4;
        case QUANTIZATION_TYPE_NONE:
        default:
            return 0;
    }
}

// =============================================================================
// 初始化量化器配置
// =============================================================================
static void faiss_hnsw_quantize_init_config(quantizer_config_t *config,
                                            int32_t dims,
                                            quantization_type_t type) {
    // 使用 algo-prod 提供的统一初始化函数（设置默认参数）
    quantizer_config_init(config, dims, type);

    // 根据任务需求设定子参数（与 brief 一致）
    switch (type) {
        case QUANTIZATION_TYPE_PQ:
            // 默认 16 子空间，每子空间 8 bit
            if (config->pq_subquantizers <= 0) {
                config->pq_subquantizers = 16;
            }
            if (config->pq_bits <= 0) {
                config->pq_bits = 8;
            }
            break;
        case QUANTIZATION_TYPE_SQ:
            if (config->sq_bits <= 0) {
                config->sq_bits = 8;
            }
            break;
        case QUANTIZATION_TYPE_LVQ:
            if (config->lvq_bits <= 0) {
                config->lvq_bits = 8;
            }
            break;
        case QUANTIZATION_TYPE_RQ:
            if (config->rq_pq_bits <= 0) {
                config->rq_pq_bits = 8;
            }
            if (config->rq_residual_bits <= 0) {
                config->rq_residual_bits = 1;
            }
            break;
        default:
            break;
    }
}

// =============================================================================
// 训练量化器
//
// 根据 idx->quantization_type 创建并训练量化器：
//   1. 检查 idx 是否有效、quantization_type 是否为 NONE
//   2. 初始化 quantizer_config_t
//   3. 调用 quantizer_create 创建量化器
//   4. 调用 quantizer_train 训练（统计 min/max、PQ codebook 等）
//   5. 设置 idx->code_size = quantizer_code_size(quantizer)
//
// @param idx     HNSW 索引
// @param n       训练向量数
// @param vectors 训练向量数组 [n * dims]
// @return        0 成功；-1 失败
// =============================================================================
int32_t faiss_hnsw_quantize_train(faiss_hnsw_t *idx, int32_t n, const float *vectors) {
    if (!idx) {
        return -1;
    }

    // 量化类型为 NONE 或训练数据无效：跳过
    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }
    if (n <= 0 || !vectors) {
        return -1;
    }

    // 校验量化类型是否合法
    if (!quantization_type_is_valid(idx->quantization_type)) {
        return -1;
    }

    // 已有量化器则先释放（支持重新训练）
    if (idx->quantizer) {
        quantizer_drop(idx->quantizer);
        idx->quantizer = NULL;
        idx->code_size = 0;
    }

    // 配置量化器参数
    quantizer_config_t config;
    faiss_hnsw_quantize_init_config(&config, idx->dims, idx->quantization_type);

    // 校验配置
    int32_t validate_ret = quantizer_config_validate(&config);
    if (validate_ret == 0) {
        return -1;
    }

    // 创建量化器
    idx->quantizer = quantizer_create(&config);
    if (!idx->quantizer) {
        return -1;
    }

    // 训练量化器
    int32_t ret = quantizer_train(idx->quantizer, n, vectors);
    if (ret != 0) {
        quantizer_drop(idx->quantizer);
        idx->quantizer = NULL;
        return -1;
    }

    // 获取实际码字大小
    idx->code_size = quantizer_code_size(idx->quantizer);
    if (idx->code_size <= 0) {
        // 兜底使用估算公式
        idx->code_size = faiss_hnsw_quantize_get_code_size(idx->quantization_type, idx->dims);
    }

    return 0;
}

// =============================================================================
// 编码单个向量
//
// 调用 quantizer_encode 将 vector 编码到 code 缓冲区：
//   - 缓冲区 code 必须至少有 idx->code_size 字节
//   - 当 quantization_type == NONE 时直接返回 0（无编码）
//
// @param idx    HNSW 索引（必须已训练过量化器，除非 type==NONE）
// @param vector 输入向量 [dims]
// @param code   输出码字缓冲区 [code_size]
// @return       写入的字节数；-1 失败
// =============================================================================
int32_t faiss_hnsw_quantize_encode_one(const faiss_hnsw_t *idx,
                                       const float *vector,
                                       uint8_t *code) {
    if (!idx || !vector || !code) {
        return -1;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }

    if (!idx->quantizer) {
        return -1;
    }

    return quantizer_encode(idx->quantizer, vector, code);
}

// =============================================================================
// 编码并写入 codes 数组（按 vec_id 索引）
//
// 与 HNSW 插入流程集成：
//   1. 若 quantization_type == NONE 直接返回 0
//   2. 计算 code 写入位置：&codes[vec_id * code_size]
//   3. 调用 quantizer_encode 编码
//
// @param idx    HNSW 索引
// @param vec_id 向量 ID（用于定位 codes 中的位置）
// @param vector 输入向量 [dims]
// @return       写入字节数；-1 失败
// =============================================================================
int32_t faiss_hnsw_quantize_encode(faiss_hnsw_t *idx, int32_t vec_id, const float *vector) {
    if (!idx || !vector) {
        return -1;
    }
    if (vec_id < 0) {
        return -1;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }

    if (!idx->quantizer || !idx->codes || idx->code_size <= 0) {
        return -1;
    }
    if (vec_id >= idx->n_total) {
        return -1;
    }

    uint8_t *code = idx->codes + (size_t)vec_id * (size_t)idx->code_size;
    return quantizer_encode(idx->quantizer, vector, code);
}

// =============================================================================
// 解码单个码字
//
// 通过 SQ 的全局统计参数（min/scale）执行反归一化。
// PQ / LVQ / RQ 不提供精确解码（返回 -1，调用方应改用精确重排序）。
//
// SQ8 反量化算法：
//   假设训练阶段 SQ 已统计得到 global_min 和 scale，
//   code[i]  = clamp((vector[i] - global_min) / scale, 0, 255)
//   vector[i] = global_min + code[i] * scale
//
// 注意：当前 SQ 模块未提供 model 导出接口供外部使用（仅 quantizer_export_model
//   可读取），这里采用归一化假设：使用 quantizer 模型导出的 min/scale。
//
// @param idx    HNSW 索引
// @param code   输入码字 [code_size]
// @param vector 输出向量 [dims]
// @return       0 成功；-1 不支持或失败
// =============================================================================
int32_t faiss_hnsw_quantize_decode_one(const faiss_hnsw_t *idx,
                                       const uint8_t *code,
                                       float *vector) {
    if (!idx || !code || !vector) {
        return -1;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }

    if (!idx->quantizer) {
        return -1;
    }

    // 仅 SQ 支持精确反量化（PQ/LVQ/RQ 的精确解码需要重建搜索或不支持）
    if (idx->quantization_type != QUANTIZATION_TYPE_SQ) {
        return -1;
    }

    // 导出 SQ 模型参数：[global_min, scale]
    int32_t model_count = quantizer_model_float_count(idx->quantizer);
    if (model_count < 2) {
        return -1;
    }

    float params[2] = {0.0f, 1.0f};
    int32_t trained_size = 0;
    int32_t exported = quantizer_export_model(idx->quantizer, params, 2, &trained_size);
    if (exported < 0) {
        return -1;
    }

    float global_min = params[0];
    float scale = (params[1] != 0.0f) ? params[1] : (1.0f / 255.0f);

    // SQ8: code 长度为 dims（每维 1 字节）
    if (idx->code_size < idx->dims) {
        return -1;
    }

    for (int32_t i = 0; i < idx->dims; i++) {
        vector[i] = global_min + (float)code[i] * scale;
    }

    return 0;
}

// =============================================================================
// 解码 codes 数组中的码字到 vector（按 vec_id 索引）
//
// @param idx    HNSW 索引
// @param vec_id 向量 ID
// @param vector 输出向量 [dims]
// @return       0 成功；-1 失败
// =============================================================================
int32_t faiss_hnsw_quantize_decode(faiss_hnsw_t *idx, int32_t vec_id, float *vector) {
    if (!idx || !vector) {
        return -1;
    }
    if (vec_id < 0) {
        return -1;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }

    if (!idx->quantizer || !idx->codes || idx->code_size <= 0) {
        return -1;
    }
    if (vec_id >= idx->n_total) {
        return -1;
    }

    const uint8_t *code = idx->codes + (size_t)vec_id * (size_t)idx->code_size;
    return faiss_hnsw_quantize_decode_one(idx, code, vector);
}

// =============================================================================
// 批量编码
//
// @param idx     HNSW 索引
// @param n       向量数
// @param vectors 输入向量 [n * dims]
// @param codes   输出码字 [n * code_size]
// @return        0 成功；-1 失败
// =============================================================================
int32_t faiss_hnsw_quantize_encode_batch(const faiss_hnsw_t *idx,
                                         int32_t n,
                                         const float *vectors,
                                         uint8_t *codes) {
    if (!idx || !vectors || !codes) {
        return -1;
    }
    if (n <= 0) {
        return 0;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }

    if (!idx->quantizer) {
        return -1;
    }

    return quantizer_encode_batch(idx->quantizer, n, vectors, codes);
}

// =============================================================================
// 计算查询向量的距离表（用于 ADC / PQ 快速距离）
//
// @param idx            HNSW 索引
// @param query          查询向量 [dims]
// @param distance_table 输出距离表（大小由 quantizer_distance_table_size 决定）
// @return               0 成功；-1 失败
// =============================================================================
int32_t faiss_hnsw_quantize_compute_distance_table(const faiss_hnsw_t *idx,
                                                  distance_metric_t metric,
                                                  const float *query,
                                                  float *distance_table) {
    if (!idx || !query || !distance_table) {
        return -1;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0;
    }

    if (!idx->quantizer) {
        return -1;
    }

    return quantizer_compute_distance_table(idx->quantizer, metric, query, distance_table);
}

// =============================================================================
// 使用距离表计算单个码字的 ADC 距离
//
// @param idx            HNSW 索引
// @param code           码字 [code_size]
// @param distance_table 预计算的距离表
// @return               距离值（>= 0）
// =============================================================================
float faiss_hnsw_quantize_adc_distance(const faiss_hnsw_t *idx,
                                       const uint8_t *code,
                                       const float *distance_table) {
    if (!idx || !code || !distance_table) {
        return -1.0f;
    }

    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return 0.0f;
    }

    if (!idx->quantizer) {
        return -1.0f;
    }

    return quantizer_adc_distance(idx->quantizer, code, distance_table);
}

// =============================================================================
// 获取距离表大小（float 元素个数）
// =============================================================================
int32_t faiss_hnsw_quantize_distance_table_size(const faiss_hnsw_t *idx) {
    if (!idx || !idx->quantizer) {
        return 0;
    }
    return quantizer_distance_table_size(idx->quantizer);
}

// =============================================================================
// 检查量化器是否已训练
// =============================================================================
bool faiss_hnsw_quantize_is_trained(const faiss_hnsw_t *idx) {
    if (!idx) {
        return false;
    }
    if (idx->quantization_type == QUANTIZATION_TYPE_NONE) {
        return true;  // NONE 视为永远"就绪"
    }
    if (!idx->quantizer) {
        return false;
    }
    return quantizer_is_trained(idx->quantizer);
}

// =============================================================================
// 释放量化器资源（不释放 codes 数组本身，由 faiss_hnsw_index_drop 统一处理）
// =============================================================================
void faiss_hnsw_quantize_destroy(faiss_hnsw_t *idx) {
    if (!idx) {
        return;
    }

    if (idx->quantizer) {
        quantizer_drop(idx->quantizer);
        idx->quantizer = NULL;
    }

    idx->code_size = 0;
}