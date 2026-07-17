/*
 * diskann_fresh.h
 *
 * DiskANN FreshDiskANN 增量更新头文件。
 * 支持动态区/静态区分离的增量更新。
 */

#ifndef DISKANN_FRESH_H
#define DISKANN_FRESH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <db/index/vector_index/diskann/diskann.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FreshDiskANN 上下文
 * ============================================================================ */

/**
 * @brief FreshDiskANN 上下文结构
 */
typedef struct diskann_fresh_context {
    diskann_fresh_config_t config;   /**< Fresh 配置 */

    /* 动态区数据 */
    float *fresh_vectors;            /**< 动态区向量 (capacity * dims) */
    int32_t *fresh_neighbors;        /**< 动态区邻接表 */
    int32_t *fresh_neighbor_counts;  /**< 动态区每个节点的邻居数量 */
    int32_t fresh_count;             /**< 动态区当前向量数 */

    /* 量化器（动态区） */
    quantizer_t *fresh_quantizer;    /**< 动态区量化器 */
    quantizer_config_t quantizer_config; /**< 量化器配置 */
    uint8_t *fresh_codes;            /**< 动态区 PQ 编码 */

    /* 状态 */
    bool needs_merge;                /**< 是否需要合并 */
} diskann_fresh_context_t;

/* ============================================================================
 * FreshDiskANN 函数
 * ============================================================================ */

/**
 * @brief 创建 Fresh 上下文
 * @param[in] config Fresh 配置
 * @param[in] dims 向量维度
 * @return 上下文指针，失败返回 NULL
 */
diskann_fresh_context_t *diskann_fresh_create(const diskann_fresh_config_t *config, int32_t dims);

/**
 * @brief 释放 Fresh 上下文
 * @param[in] ctx Fresh 上下文
 */
void diskann_fresh_free(diskann_fresh_context_t *ctx);

/**
 * @brief 初始化动态区
 * @param[in] ctx Fresh 上下文
 * @param[in] dims 向量维度
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_init(diskann_fresh_context_t *ctx, int32_t dims);

/**
 * @brief 插入向量到动态区
 * @param[in] ctx Fresh 上下文
 * @param[in] vector 向量 (dims,)
 * @param[out] label 分配的标签（可为 NULL）
 * @return 成功返回 0，容量满返回 1，失败返回 -1
 */
int32_t diskann_fresh_insert(diskann_fresh_context_t *ctx,
                             const float *vector,
                             int32_t *label);

/**
 * @brief 在动态区构建图
 * @param[in] ctx Fresh 上下文
 * @param[in] index_size 目标邻居数
 * @param[in] search_list_size 搜索候选数
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_build(diskann_fresh_context_t *ctx,
                            int32_t index_size,
                            int32_t search_list_size);

/**
 * @brief 在动态区搜索
 * @param[in] ctx Fresh 上下文
 * @param[in] query 查询向量 (dims,)
 * @param[in] k 返回结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[out] distances 距离数组 (k,)
 * @param[out] labels 标签数组 (k,)
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_search(diskann_fresh_context_t *ctx,
                             const float *query,
                             int32_t k,
                             int32_t search_list_size,
                             float *distances,
                             int32_t *labels);

/**
 * @brief 检查是否需要合并
 * @param[in] ctx Fresh 上下文
 * @return 需要合并返回 1，不需要返回 0
 */
int32_t diskann_fresh_should_merge(diskann_fresh_context_t *ctx);

/**
 * @brief 持久化动态区
 * @param[in] ctx Fresh 上下文
 * @param[in] file 输出文件
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_persist(diskann_fresh_context_t *ctx, FILE *file);

/**
 * @brief 加载动态区
 * @param[in] config Fresh 配置
 * @param[in] dims 向量维度
 * @param[in] file 输入文件
 * @return Fresh 上下文，失败返回 NULL
 */
diskann_fresh_context_t *diskann_fresh_load(const diskann_fresh_config_t *config,
                                            int32_t dims,
                                            FILE *file);

/**
 * @brief 合并动态区到静态区
 * @param[in] ctx Fresh 上下文
 * @param[in] frozen_index 静态区索引
 * @param[in] metric 距离度量
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_merge(diskann_fresh_context_t *ctx,
                            diskann_t *frozen_index,
                            distance_metric_t metric);

/**
 * @brief 更新搜索入口点
 * @param[in] ctx Fresh 上下文
 * @param[in] frozen_index 静态区索引
 * @param[out] new_entry_point 新的入口点
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_update_entry_point(diskann_fresh_context_t *ctx,
                                         diskann_t *frozen_index,
                                         int32_t *new_entry_point);

/**
 * @brief 清空动态区
 * @param[in] ctx Fresh 上下文
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_clear(diskann_fresh_context_t *ctx);

/**
 * @brief 获取动态区向量数
 * @param[in] ctx Fresh 上下文
 * @return 动态区向量数
 */
int32_t diskann_fresh_count(const diskann_fresh_context_t *ctx);

/**
 * @brief 获取动态区容量
 * @param[in] ctx Fresh 上下文
 * @return 动态区容量
 */
int32_t diskann_fresh_capacity(const diskann_fresh_context_t *ctx);

/**
 * @brief 获取动态区向量数据
 * @param[in] ctx Fresh 上下文
 * @param[out] vectors 向量数组指针
 * @param[out] count 向量数量
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_fresh_get_vectors(const diskann_fresh_context_t *ctx,
                                  float **vectors,
                                  int32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* DISKANN_FRESH_H */