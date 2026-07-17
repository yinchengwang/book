/**
 * @file vector_index_selector.h
 * @brief 向量索引选择器
 *
 * 根据数据规模、维度、查询要求等参数自动选择最优索引类型。
 */
#ifndef DB_VECTOR_INDEX_SELECTOR_H
#define DB_VECTOR_INDEX_SELECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 索引类型枚举
 * ======================================================================== */

/**
 * @brief 向量索引类型
 */
typedef enum {
    VECTOR_INDEX_BRUTE_FORCE = 0,  /**< 暴力搜索 */
    VECTOR_INDEX_HNSW = 1,        /**< HNSW 图索引 */
    VECTOR_INDEX_IVF_PQ = 2,      /**< IVF-PQ 聚类索引 */
    VECTOR_INDEX_DISKANN = 3,     /**< DiskANN 磁盘索引 */
} vector_index_type_t;

/**
 * @brief 索引选择决策
 */
typedef struct {
    vector_index_type_t index_type;  /**< 推荐的索引类型 */
    int param1;                        /**< 第一个参数（如 HNSW 的 M，或 IVF 的 nlist） */
    int param2;                        /**< 第二个参数（如 HNSW 的 ef，或 IVF 的 nprobe） */
    float estimated_memory_mb;         /**< 预估内存使用（MB） */
    float estimated_qps;              /**< 预估 QPS */
    float estimated_recall;           /**< 预估召回率 */
} vector_index_decision_t;

/**
 * @brief 数据规模信息
 */
typedef struct {
    uint64_t num_vectors;       /**< 向量数量 */
    int32_t dimension;          /**< 向量维度 */
    uint64_t available_memory_mb; /**< 可用内存（MB） */
    float target_qps;           /**< 目标 QPS */
    float target_recall;        /**< 目标召回率 */
    bool is_static;              /**< 是否静态数据（不频繁更新） */
} vector_data_info_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 选择最优索引类型
 *
 * @param info 数据规模信息
 * @param decision 输出决策结果
 * @return 0 成功，-1 失败
 */
int vector_index_selector_choose(const vector_data_info_t *info,
                               vector_index_decision_t *decision);

/**
 * @brief 根据索引类型生成参数
 *
 * @param index_type 索引类型
 * @param info 数据规模信息
 * @param param1 输出第一个参数
 * @param param2 输出第二个参数
 */
void vector_index_selector_generate_params(vector_index_type_t index_type,
                                        const vector_data_info_t *info,
                                        int *param1, int *param2);

/**
 * @brief VectorIndexType → vector_index_type_t 映射
 *
 * 将 VectorAPI 使用的枚举值转换为 Selector 内部枚举。
 * 无法映射时默认回退到 VECTOR_INDEX_HNSW。
 *
 * @param type VectorIndexType 枚举值
 * @return 对应的 vector_index_type_t 枚举值
 */
vector_index_type_t vector_index_type_convert(int type);

/**
 * @brief 估算索引内存使用
 *
 * @param index_type 索引类型
 * @param num_vectors 向量数量
 * @param dimension 向量维度
 * @param param1 第一个参数
 * @param param2 第二个参数
 * @return 预估内存使用（MB）
 */
float vector_index_selector_estimate_memory(vector_index_type_t index_type,
                                        uint64_t num_vectors,
                                        int32_t dimension,
                                        int param1, int param2);

/**
 * @brief 获取索引类型名称
 *
 * @param index_type 索引类型
 * @return 类型名称字符串
 */
const char *vector_index_selector_get_name(vector_index_type_t index_type);

/**
 * @brief 获取索引类型描述
 *
 * @param index_type 索引类型
 * @return 类型描述字符串
 */
const char *vector_index_selector_get_desc(vector_index_type_t index_type);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_INDEX_SELECTOR_H */
