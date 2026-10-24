/**
 * @file dist_hnsw.h
 * @brief 分布式 HNSW 索引接口
 *
 * Phase12 - 实现分布式向量索引，追赶 Milvus/Qdrant 水平。
 *
 * 设计目标：
 * - 支持十亿级向量规模
 * - 分片策略：基于一致性哈希
 * - 查询路由：段级别并行搜索
 * - 结果融合：RRF (Reciprocal Rank Fusion)
 */
#ifndef DB_INDEX_VECTOR_DIST_HNSW_H
#define DB_INDEX_VECTOR_DIST_HNSW_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 最大分片数 */
#define DIST_HNSW_MAX_SHARDS 256

/** 最大节点数 */
#define DIST_HNSW_MAX_NODES 64

/** 默认分片数 */
#define DIST_HNSW_DEFAULT_NUM_SHARDS 4

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/** 分片状态 */
typedef enum {
    SHARD_STATE_ACTIVE = 0,
    SHARD_STATE_REBALANCING = 1,
    SHARD_STATE_OFFLINE = 2
} dist_hnsw_shard_state_t;

/** 分片信息 */
typedef struct {
    uint32_t shard_id;
    uint64_t start_key;
    uint64_t end_key;
    dist_hnsw_shard_state_t state;
    uint64_t num_vectors;
    size_t size_bytes;
    char address[256];
    uint16_t port;
} dist_hnsw_shard_info_t;

/** 搜索结果 */
typedef struct {
    uint64_t id;
    float distance;
    void *metadata;
} dist_hnsw_search_result_t;

/** 搜索结果集合 */
typedef struct {
    dist_hnsw_search_result_t *results;
    size_t num_results;
    size_t capacity;
} dist_hnsw_result_set_t;

/** 节点信息 */
typedef struct {
    uint64_t node_id;
    char address[256];
    uint16_t port;
    bool is_local;
    uint32_t num_shards;
    uint32_t *shard_ids;
} dist_hnsw_node_info_t;

/** 分布式 HNSW 配置 */
typedef struct {
    uint32_t num_shards;
    uint32_t num_replicas;
    uint32_t hnsw_m;
    uint32_t hnsw_ef_construction;
    uint32_t hnsw_ef_search;
    int dimension;
    bool use_gpu;
    bool enable_rebalancing;
    size_t memory_limit_per_shard;
} dist_hnsw_config_t;

/** 分布式 HNSW 不透明类型 */
typedef struct dist_hnsw dist_hnsw_t;

/* ========================================================================
 * 生命周期
 * ======================================================================== */

/**
 * @brief 创建分布式 HNSW 索引
 *
 * @param config 配置
 * @return 成功返回索引指针，失败返回 NULL
 */
dist_hnsw_t *dist_hnsw_create(const dist_hnsw_config_t *config);

/**
 * @brief 打开已存在的分布式 HNSW 索引
 *
 * @param data_dir 数据目录
 * @return 成功返回索引指针，失败返回 NULL
 */
dist_hnsw_t *dist_hnsw_open(const char *data_dir);

/**
 * @brief 关闭分布式 HNSW 索引
 *
 * @param index 索引
 */
void dist_hnsw_close(dist_hnsw_t *index);

/**
 * @brief 获取配置
 */
const dist_hnsw_config_t *dist_hnsw_get_config(const dist_hnsw_t *index);

/* ========================================================================
 * 向量操作
 * ======================================================================== */

/**
 * @brief 插入向量
 *
 * @param index 索引
 * @param id 向量 ID
 * @param vector 向量数据
 * @param metadata 元数据（可为 NULL）
 * @param metadata_size 元数据大小
 * @return 0 成功
 */
int dist_hnsw_insert(dist_hnsw_t *index,
                    uint64_t id,
                    const float *vector,
                    const void *metadata,
                    size_t metadata_size);

/**
 * @brief 批量插入向量
 *
 * @param index 索引
 * @param ids 向量 ID 数组
 * @param vectors 向量数组
 * @param count 向量数量
 * @return 成功插入的数量
 */
size_t dist_hnsw_batch_insert(dist_hnsw_t *index,
                             const uint64_t *ids,
                             const float *vectors,
                             size_t count);

/**
 * @brief 删除向量
 *
 * @param index 索引
 * @param id 向量 ID
 * @return 0 成功
 */
int dist_hnsw_delete(dist_hnsw_t *index, uint64_t id);

/**
 * @brief 更新向量
 *
 * @param index 索引
 * @param id 向量 ID
 * @param vector 新向量数据
 * @return 0 成功
 */
int dist_hnsw_update(dist_hnsw_t *index,
                    uint64_t id,
                    const float *vector);

/**
 * @brief 获取向量数量
 */
uint64_t dist_hnsw_get_num_vectors(const dist_hnsw_t *index);

/* ========================================================================
 * 搜索操作
 * ======================================================================== */

/**
 * @brief KNN 搜索
 *
 * @param index 索引
 * @param query 查询向量
 * @param k 返回最近邻数量
 * @param results 结果数组（需预先分配 k 个元素）
 * @return 实际返回结果数量
 */
size_t dist_hnsw_search(dist_hnsw_t *index,
                       const float *query,
                       size_t k,
                       dist_hnsw_search_result_t *results);

/**
 * @brief 范围搜索
 *
 * @param index 索引
 * @param query 查询向量
 * @param radius 搜索半径
 * @param results 结果数组
 * @param max_results 最大结果数
 * @return 实际返回结果数量
 */
size_t dist_hnsw_range_search(dist_hnsw_t *index,
                              const float *query,
                              float radius,
                              dist_hnsw_search_result_t *results,
                              size_t max_results);

/**
 * @brief 带过滤的搜索
 *
 * @param index 索引
 * @param query 查询向量
 * @param k 返回数量
 * @param filter_ids 需要过滤的 ID（NULL 表示不过滤）
 * @param filter_count 过滤 ID 数量
 * @param results 结果数组
 * @return 实际返回结果数量
 */
size_t dist_hnsw_search_with_filter(dist_hnsw_t *index,
                                    const float *query,
                                    size_t k,
                                    const uint64_t *filter_ids,
                                    size_t filter_count,
                                    dist_hnsw_search_result_t *results);

/**
 * @brief 释放搜索结果
 */
void dist_hnsw_free_results(dist_hnsw_search_result_t *results, size_t count);

/* ========================================================================
 * 分片管理
 * ======================================================================== */

/**
 * @brief 添加分片
 *
 * @param index 索引
 * @param shard_id 分片 ID
 * @param address 节点地址
 * @param port 端口
 * @return 0 成功
 */
int dist_hnsw_add_shard(dist_hnsw_t *index,
                        uint32_t shard_id,
                        const char *address,
                        uint16_t port);

/**
 * @brief 移除分片
 *
 * @param index 索引
 * @param shard_id 分片 ID
 * @return 0 成功
 */
int dist_hnsw_remove_shard(dist_hnsw_t *index, uint32_t shard_id);

/**
 * @brief 获取分片信息
 *
 * @param index 索引
 * @param shard_id 分片 ID
 * @return 分片信息（NULL 不存在）
 */
const dist_hnsw_shard_info_t *dist_hnsw_get_shard_info(const dist_hnsw_t *index,
                                                        uint32_t shard_id);

/**
 * @brief 获取所有分片信息
 *
 * @param index 索引
 * @param out_count 输出分片数量
 * @return 分片信息数组（调用者负责释放）
 */
dist_hnsw_shard_info_t *dist_hnsw_get_all_shards(const dist_hnsw_t *index,
                                                  uint32_t *out_count);

/**
 * @brief 触发分片再平衡
 *
 * @param index 索引
 * @return 0 成功
 */
int dist_hnsw_rebalance(dist_hnsw_t *index);

/* ========================================================================
 * 节点管理
 * ======================================================================== */

/**
 * @brief 添加节点
 *
 * @param index 索引
 * @param node_id 节点 ID
 * @param address 节点地址
 * @param port 端口
 * @return 0 成功
 */
int dist_hnsw_add_node(dist_hnsw_t *index,
                      uint64_t node_id,
                      const char *address,
                      uint16_t port);

/**
 * @brief 移除节点
 *
 * @param index 索引
 * @param node_id 节点 ID
 * @return 0 成功
 */
int dist_hnsw_remove_node(dist_hnsw_t *index, uint64_t node_id);

/**
 * @brief 获取节点数量
 */
uint32_t dist_hnsw_get_num_nodes(const dist_hnsw_t *index);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/** 分布式 HNSW 统计信息 */
typedef struct {
    uint64_t num_vectors;
    uint32_t num_shards;
    uint32_t num_nodes;
    uint64_t total_memory_usage;
    uint64_t search_requests;
    double avg_search_latency_ms;
    double avg_search_qps;
} dist_hnsw_stats_t;

/**
 * @brief 获取统计信息
 */
void dist_hnsw_get_stats(const dist_hnsw_t *index, dist_hnsw_stats_t *stats);

/* ========================================================================
 * 持久化
 * ======================================================================== */

/**
 * @brief 保存索引到磁盘
 *
 * @param index 索引
 * @return 0 成功
 */
int dist_hnsw_save(dist_hnsw_t *index);

/**
 * @brief 从磁盘加载索引
 *
 * @param index 索引
 * @return 0 成功
 */
int dist_hnsw_load(dist_hnsw_t *index);

/**
 * @brief 异步保存
 *
 * @param index 索引
 */
void dist_hnsw_save_async(dist_hnsw_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_DIST_HNSW_H */
