/**
 * @file concurrent_search.h
 * @brief 并发查询适配器 - 支持"主索引+缓冲区"联合查询
 *
 * 设计原则：
 * 1. 无锁读取：缓冲区支持并发读取，不阻塞插入
 * 2. 结果去重：合并主索引和缓冲区的结果时去重
 * 3. 统一接口：对外提供与普通索引相同的查询接口
 */

#ifndef CONCURRENT_SEARCH_H
#define CONCURRENT_SEARCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct concurrent_search_adapter concurrent_search_adapter_t;
typedef struct vector_index vector_index_t;
typedef struct vector_write_buffer vector_write_buffer_t;

/* ========================================================================
 * 查询结果
 * ======================================================================== */

/**
 * @brief 联合查询结果
 */
typedef struct concurrent_search_result {
    int32_t *ids;           /* 结果 ID 数组 */
    float *distances;       /* 结果距离数组 */
    int32_t count;          /* 实际结果数 */
    int32_t capacity;       /* 数组容量 */
} concurrent_search_result_t;

/**
 * @brief 创建结果结构
 * @param capacity 容量
 * @return 结果结构，失败返回 NULL
 */
concurrent_search_result_t *concurrent_search_result_create(int32_t capacity);

/**
 * @brief 销毁结果结构
 * @param result 结果结构
 */
void concurrent_search_result_destroy(concurrent_search_result_t *result);

/* ========================================================================
 * 适配器
 * ======================================================================== */

/**
 * @brief 并发查询配置
 */
typedef struct concurrent_search_config {
    bool enable_buffer_search;    /* 是否搜索缓冲区 */
    int32_t buffer_search_k;      /* 缓冲区搜索返回的候选数 */
    float buffer_weight;          /* 缓冲区结果权重调整 */
    bool deduplicate;             /* 是否去重 */
} concurrent_search_config_t;

#define CONCURRENT_SEARCH_DEFAULT_BUFFER_K 50
#define CONCURRENT_SEARCH_DEFAULT_BUFFER_WEIGHT 1.0f

/**
 * @brief 创建并发查询适配器
 * @param index 主索引
 * @param buffer 缓冲区（可为 NULL）
 * @param config 配置（NULL 使用默认配置）
 * @return 适配器句柄，失败返回 NULL
 */
concurrent_search_adapter_t *concurrent_search_create(
    vector_index_t *index,
    vector_write_buffer_t *buffer,
    const concurrent_search_config_t *config);

/**
 * @brief 销毁并发查询适配器
 * @param adapter 适配器句柄
 */
void concurrent_search_destroy(concurrent_search_adapter_t *adapter);

/**
 * @brief 联合查询
 *
 * 同时查询主索引和缓冲区，合并结果后返回 Top-K。
 *
 * @param adapter 适配器句柄
 * @param query 查询向量
 * @param k 返回结果数
 * @param distances 输出距离数组（预分配 k）
 * @param ids 输出 ID 数组（预分配 k）
 * @return 实际返回结果数，-1 失败
 */
int32_t concurrent_search(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);

/**
 * @brief 仅搜索主索引
 *
 * 不搜索缓冲区，仅在主索引中查询。
 *
 * @param adapter 适配器句柄
 * @param query 查询向量
 * @param k 返回结果数
 * @param distances 输出距离数组
 * @param ids 输出 ID 数组
 * @return 实际返回结果数，-1 失败
 */
int32_t concurrent_search_main_only(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);

/**
 * @brief 仅搜索缓冲区
 *
 * 仅在缓冲区中查询，用于调试或特殊场景。
 *
 * @param adapter 适配器句柄
 * @param query 查询向量
 * @param k 返回结果数
 * @param distances 输出距离数组
 * @param ids 输出 ID 数组
 * @return 实际返回结果数，-1 失败
 */
int32_t concurrent_search_buffer_only(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);

/* ========================================================================
 * 结果合并
 * ======================================================================== */

/**
 * @brief 合并两组搜索结果
 *
 * 合并主索引和缓冲区的结果，返回 Top-K。
 *
 * @param main_ids 主索引结果 ID
 * @param main_distances 主索引结果距离
 * @param main_count 主索引结果数
 * @param buffer_ids 缓冲区结果 ID
 * @param buffer_distances 缓冲区结果距离
 * @param buffer_count 缓冲区结果数
 * @param k 返回结果数
 * @param buffer_weight 缓冲区结果权重
 * @param output_ids 输出 ID 数组
 * @param output_distances 输出距离数组
 * @return 实际输出结果数
 */
int32_t concurrent_search_merge_results(
    const int32_t *main_ids,
    const float *main_distances,
    int32_t main_count,
    const int32_t *buffer_ids,
    const float *buffer_distances,
    int32_t buffer_count,
    int32_t k,
    float buffer_weight,
    int32_t *output_ids,
    float *output_distances);

/**
 * @brief 结果去重
 *
 * 从结果中移除重复的 ID。
 *
 * @param ids 输入/输出 ID 数组（原地修改）
 * @param distances 输入/输出距离数组（原地修改）
 * @param count 输入结果数
 * @return 去重后的结果数
 */
int32_t concurrent_search_deduplicate(
    int32_t *ids,
    float *distances,
    int32_t count);

/* ========================================================================
 * 配置更新
 * ======================================================================== */

/**
 * @brief 设置是否启用缓冲区搜索
 * @param adapter 适配器句柄
 * @param enable 是否启用
 */
void concurrent_search_set_buffer_enabled(
    concurrent_search_adapter_t *adapter,
    bool enable);

/**
 * @brief 检查缓冲区搜索是否启用
 * @param adapter 适配器句柄
 * @return true 启用
 */
bool concurrent_search_is_buffer_enabled(const concurrent_search_adapter_t *adapter);

/**
 * @brief 更新缓冲区
 * @param adapter 适配器句柄
 * @param buffer 新缓冲区（可为 NULL）
 */
void concurrent_search_update_buffer(
    concurrent_search_adapter_t *adapter,
    vector_write_buffer_t *buffer);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 并发查询统计
 */
typedef struct concurrent_search_stats {
    int64_t total_searches;       /* 累计查询次数 */
    int64_t buffer_hits;          /* 缓冲区命中次数 */
    int64_t deduplications;       /* 去重次数 */
    int32_t avg_result_count;     /* 平均结果数 */
} concurrent_search_stats_t;

/**
 * @brief 获取统计信息
 * @param adapter 适配器句柄
 * @param stats 输出统计信息
 */
void concurrent_search_get_stats(
    const concurrent_search_adapter_t *adapter,
    concurrent_search_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* CONCURRENT_SEARCH_H */
