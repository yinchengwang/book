/**
 * @file index_config.h
 * @brief 索引配置结构定义
 *
 * 本头文件定义索引子系统的统一配置结构 index_config_t，用于构造各类
 * 索引实例（HNSW、IVF、Flat 等）时传递运行时参数。
 *
 * 设计要点：
 * - 与 storage_backend.h 保持一致的类型命名（storage_backend_type_t）
 * - 配置信息包括：存储后端、持久化开关、页面大小、向量维度、HNSW 参数等
 * - 提供默认配置与配置验证两个辅助函数
 */
#ifndef DB_INDEX_CONFIG_H
#define DB_INDEX_CONFIG_H

#include "storage_backend.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 枚举定义
 * ============================================================ */

/** 距离度量类型 */
typedef enum {
    DISTANCE_L2 = 0,          /**< 欧氏距离 */
    DISTANCE_IP = 1,          /**< 内积 */
    DISTANCE_COSINE = 2,      /**< 余弦相似度 */
} distance_metric_t;

/** 量化类型 */
typedef enum {
    QUANTIZATION_TYPE_NONE = 0,    /**< 无量化 */
    QUANTIZATION_TYPE_PQ = 1,      /**< Product Quantization */
    QUANTIZATION_TYPE_OPQ = 2,     /**< Optimized PQ */
    QUANTIZATION_TYPE_LVQ = 3,     /**< Learned Vector Quantization */
} quantization_type_t;

/* ============================================================
 * 索引配置结构
 * ============================================================ */

/**
 * @brief 索引配置
 *
 * 通过 index_config_default 获取合理默认值，通过 index_config_validate
 * 校验配置合法性。任何索引实现模块（HNSW/IVF/Flat 等）都应接受该结构
 * 作为构造入口，以保持接口一致。
 */
typedef struct index_config {
    /* ---------- 存储配置 ---------- */
    storage_backend_type_t storage_type;  /**< 存储后端类型 */
    const char *storage_path;             /**< 持久化路径（可选，NULL 表示纯内存） */
    size_t page_size;                     /**< 页面大小（字节） */

    /* ---------- 持久化开关 ---------- */
    bool persist_enabled;                 /**< true: 完整持久化 | false: 纯内存 */

    /* ---------- 向量维度 ---------- */
    int32_t dims;                         /**< 向量维度，必须 > 0 */

    /* ---------- 算法参数（HNSW 系列） ---------- */
    int32_t M;                            /**< 每层连接数，必须 > 0 */
    int32_t ef_construction;              /**< 构建时搜索范围，必须 > 0 */
    int32_t ef_search;                    /**< 搜索时搜索范围，必须 > 0 */

    /* ---------- 距离与量化 ---------- */
    distance_metric_t    metric;             /**< 距离度量 */
    quantization_type_t  quantization_type;  /**< 量化类型 */
} index_config_t;

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 获取索引默认配置
 *
 * 返回一份填充了合理默认值的配置对象，方便调用方按需修改：
 *   - 存储后端：STORAGE_BACKEND_MEMORY（纯内存）
 *   - 持久化路径：NULL
 *   - 页面大小：8192 字节
 *   - 持久化开关：false
 *   - 向量维度：128
 *   - HNSW 参数：M=16, ef_construction=200, ef_search=100
 *   - 距离度量：DISTANCE_L2（欧氏距离）
 *   - 量化类型：QUANTIZATION_TYPE_NONE（无量化）
 *
 * @return 默认配置（按值返回，调用方直接修改字段即可）
 */
index_config_t index_config_default(void);

/**
 * @brief 验证索引配置有效性
 *
 * 检查必填字段是否合法，至少校验：
 *   - config 非空
 *   - dims > 0
 *   - M > 0
 *   - ef_construction > 0
 *   - ef_search > 0
 *
 * @param config 待验证的配置指针
 * @return 0 配置合法；非 0 配置非法
 */
int index_config_validate(const index_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_CONFIG_H */