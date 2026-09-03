#ifndef DISKANN_INDEX_H
#define DISKANN_INDEX_H

/**
 * @file diskann.h
 * @brief DiskANN 向量索引库
 *
 * ============================================================================
 * 概述
 * ============================================================================
 *
 * DiskANN (Disk-based Approximate Nearest Neighbor) 是微软提出的面向 SSD 的
 * 近似最近邻搜索算法，基于 Vamana 图结构，通过块式分页文件实现"图在磁盘、
 * 数据按需加载"，突破了全内存索引的容量上限。
 *
 * **核心特性**：
 * - Vamana 图构建（单层稀疏邻居图）
 * - PQ/SQ/LVQ/RQ 量化压缩
 * - 块式持久化存储
 * - 增量插入与删除
 *
 * ============================================================================
 * 算法介绍
 * ============================================================================
 *
 * ### 1. Vamana 图算法
 *
 * DiskANN 使用 Vamana 算法构建单层图，相比 HNSW 的分层结构更加简洁：
 *
 * - **Robust Prune**：给定候选池（按距离排序），对候选从近到远依次检查。
 *   如果已有选中邻居比当前候选更近，则当前候选被"遮挡"跳过。
 *   α 参数控制遮挡容忍度：α × dist(selected, candidate) ≤ dist(candidate, node)
 *
 * - **多轮剪枝**：从 α=1.0 开始，每轮 ×1.2，逐步收紧剪枝，避免早期过度丢弃
 *
 * - **冻结点种子**：搜索和构图都从多个冻结点出发，保证图的可导航性
 *
 * ```
 *      ┌─────●─────┐
 *      │     │     │
 *      ●     │     ●      长程边（稀疏跳） + 短程边（精细搜） = 可导航小世界
 *      │╲    │    ╱│
 *      ●─────●─────●
 * ```
 *
 * ### 2. Merge-Vamana（分图合并）
 *
 * 解决超大规模数据（亿级向量）的单次构图瓶颈，支持分图独立构建后合并。
 *
 * **适用场景**：数据量超过单机内存或单次构图时间过长
 *
 * **分区策略**：
 * - Random：随机分配，简单高效
 * - K-Means：聚类分区，适合有明显簇结构的数据
 * - Coordinate-Range：按坐标范围切分，适合地理/坐标数据
 *
 * **合并流程**：
 * ```
 *  ┌─────────┐  ┌─────────┐  ┌─────────┐
 *  │ 子图 G1  │  │ 子图 G2  │  │ 子图 G3  │     ← 独立分区构建
 *  └────┬────┘  └────┬────┘  └────┬────┘
 *       └───────────┼───────────┘
 *                   ↓
 *          ┌─────────────────┐
 *          │  节点去重 + 边合并 │
 *          │  跨分区边构建    │
 *          └────────┬────────┘
 *                   ↓
 *          ┌─────────────────┐
 *          │  全局图 G       │
 *          └─────────────────┘
 * ```
 *
 * **参数说明**：
 * - `partition_count`：分区数量，建议 4-16
 * - `overlap_ratio`：重叠比例（0.0-1.0），用于跨分区边构建，默认 0.1
 *
 * ### 3. SPANN (SParse ANN)
 *
 * 分区层次索引，通过将向量空间分区使搜索只访问相关分区，减少 IO 操作。
 *
 * **适用场景**：搜索延迟敏感、需要减少磁盘 IO 的场景
 *
 * **核心思想**：预先将向量空间划分为多个分区，搜索时只访问最近的几个分区
 *
 * ```
 *  Query ──→ Partition Selection (路由)
 *                  │
 *      ┌───────────┼───────────┐
 *      ↓           ↓           ↓
 *    P1(skip)   P2(search)  P3(search)   ← 根据距离选择分区
 *      ↓           ↓           ↓
 *    跳过      搜索子图     搜索子图
 *                  ↓
 *          结果合并 → Top-K
 * ```
 *
 * **参数说明**：
 * - `max_partitions`：最大分区数，建议 64-256
 * - `search_partitions`：搜索时访问的分区数，建议 4-16
 * - `min_partition_size`：每分区最小向量数，避免过多小分区
 *
 * ### 4. FreshDiskANN（增量更新）
 *
 * 将索引分为静态区（Frozen）和动态区（Fresh），支持高频增量插入。
 *
 * **适用场景**：需要频繁插入新向量、不接受全量重建的场景
 *
 * **核心思想**：
 * - 动态区（Fresh）：内存中的新增数据，支持高频插入
 * - 静态区（Frozen）：磁盘上的历史数据，已构建好图结构
 * - 搜索时同时访问两区，合并结果
 *
 * ```
 *  ┌─────────────────────┐    ┌─────────────────────┐
 *  │   静态区 (Frozen)   │    │   动态区 (Fresh)    │
 *  │                     │    │                     │
 *  │  向量 + 图结构       │    │  向量 + 图结构       │
 *  │  磁盘持久化         │    │  内存索引           │
 *  └─────────────────────┘    └─────────────────────┘
 *           ↓                          ↑
 *    搜索(磁盘IO)              插入(直接写入)
 *           ↓                          │
 *           └────────┬─────────────────┘
 *                    ↓
 *              结果合并 → Top-K
 * ```
 *
 * **合并策略**：当动态区达到 `merge_threshold` 时，自动或手动合并到静态区
 *
 * **参数说明**：
 * - `fresh_capacity`：动态区容量，建议根据内存大小设置
 * - `merge_threshold`：触发合并的阈值，建议 capacity 的 80%
 *
 * ============================================================================
 * 使用示例
 * ============================================================================
 */

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 配置结构
 * ============================================================================ */

/**
 * @brief 分区策略枚举
 */
typedef enum diskann_partition_method {
    DISKANN_PARTITION_AUTO = 0,           /**< 自动选择（根据数据分布） */
    DISKANN_PARTITION_RANDOM = 1,         /**< 随机分区 */
    DISKANN_PARTITION_KMEANS = 2,         /**< K-Means 聚类分区 */
    DISKANN_PARTITION_COORDINATE_RANGE = 3, /**< 坐标范围分区 */
} diskann_partition_method_t;

/**
 * @brief 量化配置
 */
typedef struct diskann_quantization_config {
    bool enabled;                    /**< 是否启用量化 */
    quantization_type_t type;        /**< 量化类型: PQ/SQ/LVQ/RQ */
    int32_t subquantizers;           /**< m: 子空间数 (PQ 专用) */
    int32_t bits;                    /**< bits: 4/6/8 */
    int32_t train_max_vectors;       /**< 训练时使用的最大向量数 */
} diskann_quantization_config_t;

/**
 * @brief Merge-Vamana 配置
 */
typedef struct diskann_merge_config {
    bool enabled;                    /**< 是否启用合并模式 */
    diskann_partition_method_t partition_method; /**< 分区策略 */
    int32_t partition_count;         /**< 分区数量 */
    float overlap_ratio;             /**< 重叠比例 (0.0-1.0) */
} diskann_merge_config_t;

/**
 * @brief SPANN 配置
 */
typedef struct diskann_spann_config {
    bool enabled;                    /**< 是否启用 SPANN */
    int32_t max_partitions;          /**< 最大分区数 */
    int32_t min_partition_size;      /**< 每分区最小向量数 */
    int32_t search_partitions;       /**< 搜索时最多访问分区数 */
} diskann_spann_config_t;

/**
 * @brief FreshDiskANN 配置
 */
typedef struct diskann_fresh_config {
    bool enabled;                    /**< 是否启用 Fresh 模式 */
    int32_t fresh_capacity;          /**< 动态区容量 */
    int32_t merge_threshold;         /**< 触发合并的阈值 */
} diskann_fresh_config_t;

/**
 * @brief 存储配置
 */
typedef struct diskann_storage_config {
    int32_t page_size;               /**< 页大小 */
    int32_t frozen_point_count;      /**< 冻结点数量 */
} diskann_storage_config_t;

/* ============================================================================
 * 向后兼容类型别名
 * ============================================================================ */

/**
 * @brief PQ 参数别名（保留用于向后兼容）
 */
typedef struct diskann_quantization_params {
    int32_t pq_m;
    int32_t pq_bits;
    int32_t train_max_vectors;
    bool enabled;
} diskann_quantization_params_t;

/**
 * @brief 存储参数别名（保留用于向后兼容）
 */
typedef struct diskann_storage_params {
    int32_t page_size;
    int32_t frozen_point_count;
} diskann_storage_params_t;

/**
 * @brief 统一配置入口
 */
typedef struct diskann_config {
    /* 基础参数 */
    int32_t dims;                    /**< 向量维度 */
    int32_t index_size;              /**< R: 目标邻居数 */
    int32_t build_search_list_size;  /**< L: 构图候选数 */
    distance_metric_t metric;        /**< 距离度量 */

    /* 各模块配置 */
    diskann_quantization_config_t quantization; /**< 量化配置 */
    diskann_merge_config_t merge;               /**< Merge-Vamana 配置 */
    diskann_spann_config_t spann;               /**< SPANN 配置 */
    diskann_fresh_config_t fresh;               /**< FreshDiskANN 配置 */
    diskann_storage_config_t storage;           /**< 存储配置 */
} diskann_config_t;

/**
 * @brief 索引不透明类型声明
 */
typedef struct diskann diskann_t;

/* ============================================================================
 * 配置管理函数
 * ============================================================================ */

/**
 * @brief 创建默认配置
 * @param[in] dims 向量维度
 * @param[in] metric 距离度量
 * @return 配置结构指针，失败返回 NULL
 */
diskann_config_t *diskann_config_default(int32_t dims, distance_metric_t metric);

/**
 * @brief 创建完整配置
 * @param[in] dims 向量维度
 * @param[in] index_size 目标邻居数
 * @param[in] build_search_list_size 构图候选数
 * @param[in] metric 距离度量
 * @return 配置结构指针，失败返回 NULL
 */
diskann_config_t *diskann_config_create(int32_t dims,
                                        int32_t index_size,
                                        int32_t build_search_list_size,
                                        distance_metric_t metric);

/**
 * @brief 验证配置有效性
 * @param[in] config 配置指针
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_config_validate(const diskann_config_t *config);

/**
 * @brief 验证量化配置
 * @param[in] config 配置指针
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_quantization_config_validate(const diskann_quantization_config_t *config);

/**
 * @brief 验证 Merge 配置
 * @param[in] config 配置指针
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_merge_config_validate(const diskann_merge_config_t *config);

/**
 * @brief 验证 SPANN 配置
 * @param[in] config 配置指针
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_spann_config_validate(const diskann_spann_config_t *config);

/**
 * @brief 验证 Fresh 配置
 * @param[in] config 配置指针
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_fresh_config_validate(const diskann_fresh_config_t *config);

/**
 * @brief 克隆配置
 * @param[in] config 配置指针
 * @return 配置副本，失败返回 NULL
 */
diskann_config_t *diskann_config_clone(const diskann_config_t *config);

/**
 * @brief 释放配置
 * @param[in] config 配置指针
 */
void diskann_config_free(diskann_config_t *config);

/**
 * @brief 从旧参数创建配置
 * @param[in] dims 向量维度
 * @param[in] index_size 目标邻居数
 * @param[in] build_search_list_size 构图候选数
 * @param[in] metric 距离度量
 * @param[in] pq_params PQ 参数（可为 NULL）
 * @param[in] storage_params 存储参数（可为 NULL）
 * @return 配置结构指针，失败返回 NULL
 */
diskann_config_t *diskann_config_from_params(int32_t dims,
                                             int32_t index_size,
                                             int32_t build_search_list_size,
                                             distance_metric_t metric,
                                             const diskann_quantization_params_t *pq_params,
                                             const diskann_storage_params_t *storage_params);

/**
 * @brief 从配置导出旧参数
 * @param[in] config 配置指针
 * @param[out] pq_params PQ 参数（可为 NULL）
 * @param[out] storage_params 存储参数（可为 NULL）
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_config_to_params(const diskann_config_t *config,
                                 diskann_quantization_params_t *pq_params,
                                 diskann_storage_params_t *storage_params);

/**
 * @brief 保存配置到文件
 * @param[in] config 配置指针
 * @param[in] path 文件路径
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_config_save(const diskann_config_t *config, const char *path);

/**
 * @brief 从文件加载配置
 * @param[in] path 文件路径
 * @return 配置结构指针，失败返回 NULL
 */
diskann_config_t *diskann_config_load(const char *path);

/* ============================================================================
 * 新 API：使用统一配置
 * ============================================================================ */

/**
 * @brief 使用统一配置创建索引
 * @param[in] config 配置指针
 * @return 索引指针，失败返回 NULL
 */
diskann_t *diskann_index_create_with_config(const diskann_config_t *config);

/* ============================================================================
 * 旧 API：保持向后兼容
 * ============================================================================ */

/**
 * @brief 创建索引（旧 API）
 */
diskann_t *diskann_index_create(int32_t dims,
                                int32_t index_size,
                                int32_t build_search_list_size,
                                distance_metric_t metric);
int32_t diskann_index_add(diskann_t *index, int32_t n, const float *vectors);
int32_t diskann_index_insert(diskann_t *index, const float *vector, int32_t *label);
int32_t diskann_index_delete(diskann_t *index, int32_t label);
int32_t diskann_index_build(diskann_t *index);
int32_t diskann_index_train_pq(diskann_t *index);
int32_t diskann_index_search(diskann_t *index,
                             const float *query,
                             int32_t k,
                             int32_t search_list_size,
                             int32_t max_iterations,
                             float *distances,
                             int32_t *labels);

/* ============================================================================
 * 扩展搜索 API
 * ============================================================================ */

/**
 * @brief 在选中分区中搜索（SPANN 模式）
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[in] selected_partitions 选中的分区 ID
 * @param[in] selected_count 选中分区数量
 * @param[in] partition_ids 每个向量所属分区 ID
 * @param[in] k 结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[out] distances 距离数组
 * @param[out] labels 标签数组
 * @return 成功返回结果数量，失败返回 -1
 */
int32_t diskann_search_in_partitions(diskann_t *index,
                                     const float *query,
                                     const int32_t *selected_partitions,
                                     int32_t selected_count,
                                     const int32_t *partition_ids,
                                     int32_t k,
                                     int32_t search_list_size,
                                     float *distances,
                                     int32_t *labels);

/**
 * @brief 在动态区和静态区同时搜索（Fresh 模式）
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[in] k 结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[out] distances 距离数组
 * @param[out] labels 标签数组
 * @return 成功返回结果数量，失败返回 -1
 */
int32_t diskann_search_dual_zone(diskann_t *index,
                                 const float *query,
                                 int32_t k,
                                 int32_t search_list_size,
                                 float *distances,
                                 int32_t *labels);

int32_t diskann_index_save(const diskann_t *index, const char *path);
diskann_t *diskann_index_load(const char *path);

/* ============================================================================
 * 扩展持久化 API（支持 SPANN 和 FreshDiskANN）
 * ============================================================================ */

/**
 * @brief 保存索引到文件（扩展版，支持 SPANN 和 Fresh）
 * @param[in] index 索引
 * @param[in] path 文件路径
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_index_save_extended(const diskann_t *index, const char *path);

/**
 * @brief 从文件加载索引（扩展版，支持 SPANN 和 Fresh）
 * @param[in] path 文件路径
 * @return 索引指针，失败返回 NULL
 */
diskann_t *diskann_index_load_extended(const char *path);
int32_t diskann_index_size(const diskann_t *index);
int32_t diskann_index_active_size(const diskann_t *index);
int32_t diskann_index_dims(const diskann_t *index);
int32_t diskann_index_entry_point(const diskann_t *index);
int32_t diskann_index_frozen_point_count(const diskann_t *index);
bool diskann_index_is_built(const diskann_t *index);
bool diskann_index_has_pq(const diskann_t *index);
int32_t diskann_index_set_alpha(diskann_t *index, float alpha);
float diskann_index_get_alpha(const diskann_t *index);
int32_t diskann_index_set_quantization_params(diskann_t *index,
                                              const diskann_quantization_params_t *params);
int32_t diskann_index_get_quantization_params(const diskann_t *index,
                                              diskann_quantization_params_t *params);
int32_t diskann_index_set_storage_params(diskann_t *index,
                                         const diskann_storage_params_t *params);
int32_t diskann_index_get_storage_params(const diskann_t *index,
                                         diskann_storage_params_t *params);
void diskann_index_drop(diskann_t *index);

#ifdef __cplusplus
}
#endif

#endif