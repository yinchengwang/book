/**
 * @file vdb_api.h
 * @brief MiniVecDB 统一 C SDK 接口
 *
 * 提供向量数据库的完整 C 语言 SDK 接口，整合 VectorAPI 与底层向量引擎。
 * 这是 VDB 作为"产品"的统一外部接口层。
 *
 * 使用流程：
 *   1. vdb_open() — 打开数据库
 *   2. vdb_create_collection() / vdb_get_collection() — 获取集合句柄
 *   3. vdb_insert() / vdb_search() / vdb_delete() — 向量操作
 *   4. vdb_close() — 关闭数据库
 */
#ifndef DB_VDB_API_H
#define DB_VDB_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/** 数据库句柄（不透明） */
typedef struct vdb_handle_s vdb_handle_t;

/** 集合句柄（不透明） */
typedef struct vdb_collection_s vdb_collection_t;

/* ========================================================================
 * 配置与错误码
 * ======================================================================== */

/** 错误码 */
typedef enum {
    VDB_OK = 0,
    VDB_ERR_NOT_FOUND = -1,
    VDB_ERR_EXISTS = -2,
    VDB_ERR_INVALID_PARAM = -3,
    VDB_ERR_NO_MEMORY = -4,
    VDB_ERR_IO = -5,
    VDB_ERR_INTERNAL = -99
} vdb_error_t;

/** 索引类型 */
typedef enum {
    VDB_INDEX_AUTO = 0,       /**< 自动选择 */
    VDB_INDEX_HNSW = 1,       /**< HNSW 图索引 */
    VDB_INDEX_IVF = 2,         /**< IVF 倒排索引 */
    VDB_INDEX_DISKANN = 3,    /**< DiskANN 磁盘索引 */
    VDB_INDEX_BRUTE_FORCE = 4 /**< 暴力搜索 */
} vdb_index_type_t;

/** 距离度量 */
typedef enum {
    VDB_METRIC_L2 = 0,        /**< 欧氏距离 */
    VDB_METRIC_COSINE = 1,    /**< 余弦相似度 */
    VDB_METRIC_IP = 2         /**< 内积 */
} vdb_metric_type_t;

/** 数据库配置 */
typedef struct {
    char data_dir[512];        /**< 数据目录路径 */
    bool enable_wal;           /**< 是否启用 WAL */
    int max_collections;       /**< 最大集合数，0=默认 */
    int cache_size_mb;         /**< 缓存大小（MB），0=默认 */
} vdb_config_t;

/** 集合配置 */
typedef struct {
    char name[128];            /**< 集合名称 */
    int32_t dimension;         /**< 向量维度 */
    vdb_index_type_t index_type;  /**< 索引类型 */
    vdb_metric_type_t metric_type;/**< 距离度量 */
    int32_t index_ef_search;      /**< HNSW ef_search（可选） */
    int32_t index_m;              /**< HNSW M（可选） */
    int32_t index_ef_construction;/**< HNSW ef_construction（可选） */
} vdb_collection_config_t;

/** 搜索参数 */
typedef struct {
    int32_t top_k;             /**< 返回结果数 */
    float radius;              /**< 搜索半径（0=不限） */
    bool with_distance;        /**< 是否返回距离 */
    bool with_metadata;        /**< 是否返回元数据 */
    int32_t offset;            /**< 分页偏移 */
    int32_t limit;             /**< 分页限制 */
} vdb_search_params_t;

/** 搜索结果项 */
typedef struct {
    int64_t id;                /**< 向量 ID */
    float distance;            /**< 距离 */
    float score;               /**< 归一化分数 [0,1] */
    void *metadata;            /**< 元数据 */
    int32_t metadata_size;     /**< 元数据大小 */
} vdb_result_item_t;

/** 搜索结果集 */
typedef struct {
    vdb_result_item_t *items;  /**< 结果数组 */
    int32_t count;             /**< 结果数量 */
    int32_t capacity;          /**< 容量 */
    int64_t total_time_us;     /**< 总耗时（微秒） */
} vdb_results_t;

/* ========================================================================
 * 数据库生命周期
 * ======================================================================== */

/**
 * @brief 打开数据库
 * @param path 数据目录路径
 * @param config 数据库配置（可为 NULL 使用默认值）
 * @return 数据库句柄，失败返回 NULL
 */
vdb_handle_t *vdb_open(const char *path, const vdb_config_t *config);

/**
 * @brief 关闭数据库
 * @param db 数据库句柄
 * @return 0 成功，负数失败
 */
int vdb_close(vdb_handle_t *db);

/**
 * @brief 获取最后错误信息
 * @param db 数据库句柄
 * @return 错误信息字符串
 */
const char *vdb_error(vdb_handle_t *db);

/* ========================================================================
 * 集合管理
 * ======================================================================== */

/**
 * @brief 创建集合
 * @param db 数据库句柄
 * @param name 集合名称
 * @param config 集合配置
 * @return 0 成功，负数失败
 */
int vdb_create_collection(vdb_handle_t *db, const char *name,
                          const vdb_collection_config_t *config);

/**
 * @brief 删除集合
 * @param db 数据库句柄
 * @param name 集合名称
 * @return 0 成功，负数失败
 */
int vdb_drop_collection(vdb_handle_t *db, const char *name);

/**
 * @brief 获取集合句柄
 * @param db 数据库句柄
 * @param name 集合名称
 * @return 集合句柄，失败返回 NULL
 */
vdb_collection_t *vdb_get_collection(vdb_handle_t *db, const char *name);

/**
 * @brief 列出所有集合名称
 * @param db 数据库句柄
 * @param names 输出名称数组（调用者通过 vdb_free_names 释放）
 * @param count 输出数量
 * @return 0 成功，负数失败
 */
int vdb_list_collections(vdb_handle_t *db, char ***names, int32_t *count);

/**
 * @brief 释放集合名称数组
 * @param names 名称数组
 * @param count 数量
 */
void vdb_free_names(char **names, int32_t count);

/* ========================================================================
 * 向量操作
 * ======================================================================== */

/**
 * @brief 插入向量
 * @param coll 集合句柄
 * @param vector 向量数据
 * @param dim 向量维度
 * @param metadata 元数据（可为 NULL）
 * @param metadata_size 元数据大小
 * @return 分配的向量 ID，负数失败
 */
int64_t vdb_insert(vdb_collection_t *coll, const float *vector, int32_t dim,
                   const void *metadata, int32_t metadata_size);

/**
 * @brief 批量插入向量
 * @param coll 集合句柄
 * @param vectors 向量数组 [n][dim]
 * @param dim 向量维度
 * @param n 向量数量
 * @param ids 输出 ID 数组（可为 NULL）
 * @return 成功插入数量，负数失败
 */
int32_t vdb_insert_batch(vdb_collection_t *coll, const float *vectors,
                         int32_t dim, int32_t n, int64_t *ids);

/**
 * @brief 删除向量
 * @param coll 集合句柄
 * @param id 向量 ID
 * @return 0 成功，负数失败
 */
int vdb_delete(vdb_collection_t *coll, int64_t id);

/**
 * @brief 批量删除向量
 * @param coll 集合句柄
 * @param ids 要删除的 ID 数组
 * @param n ID 数量
 * @return 成功删除数量，负数失败
 */
int32_t vdb_delete_batch(vdb_collection_t *coll, const int64_t *ids, int32_t n);

/* ========================================================================
 * 查询接口
 * ======================================================================== */

/**
 * @brief 搜索最近邻
 * @param coll 集合句柄
 * @param query 查询向量
 * @param dim 向量维度
 * @param params 搜索参数
 * @return 搜索结果集（调用者通过 vdb_results_free 释放），失败返回 NULL
 */
vdb_results_t *vdb_search(vdb_collection_t *coll, const float *query, int32_t dim,
                          const vdb_search_params_t *params);

/**
 * @brief 获取集合向量数量
 * @param coll 集合句柄
 * @return 向量数量，负数失败
 */
int32_t vdb_size(vdb_collection_t *coll);

/**
 * @brief 获取集合维度
 * @param coll 集合句柄
 * @return 向量维度，负数失败
 */
int32_t vdb_dimension(vdb_collection_t *coll);

/**
 * @brief 获取集合配置信息
 * @param coll 集合句柄
 * @param config 输出配置
 * @return 0 成功，负数失败
 */
int vdb_collection_info(vdb_collection_t *coll, vdb_collection_config_t *config);

/* ========================================================================
 * 结果集管理
 * ======================================================================== */

/**
 * @brief 释放搜索结果集
 * @param results 搜索结果集
 */
void vdb_results_free(vdb_results_t *results);

/* ========================================================================
 * 持久化
 * ======================================================================== */

/**
 * @brief 保存所有集合到磁盘
 * @param db 数据库句柄
 * @return 0 成功，负数失败
 */
int vdb_save(vdb_handle_t *db);

/**
 * @brief 从磁盘加载所有集合
 * @param db 数据库句柄
 * @return 0 成功，负数失败
 */
int vdb_load(vdb_handle_t *db);

/* ========================================================================
 * 多模态查询接口
 * ======================================================================== */

/** 融合策略 */
typedef enum {
    VDB_FUSION_RRF = 0,       /**< Reciprocal Rank Fusion */
    VDB_FUSION_WEIGHTED = 1,  /**< 加权融合 */
    VDB_FUSION_CUSTOM = 2     /**< 自定义融合 */
} vdb_fusion_strategy_t;

/** 过滤搜索参数 */
typedef struct {
    vdb_search_params_t base; /**< 基础搜索参数 */
    const char *filter_expr;  /**< 过滤表达式（如 "price < 100 AND category = \"electronics\""） */
} vdb_filtered_search_params_t;

/** 混合搜索参数 */
typedef struct {
    vdb_search_params_t base;     /**< 基础搜索参数 */
    const char *text_query;       /**< 文本查询（用于 BM25） */
    const char *filter_expr;      /**< 过滤表达式（可选） */
    vdb_fusion_strategy_t fusion; /**< 融合策略 */
    float vector_weight;          /**< 向量权重（0-1），融合时使用 */
    float bm25_weight;            /**< BM25 权重（0-1），融合时使用 */
    int rrf_k;                    /**< RRF k 参数，默认 60 */
} vdb_hybrid_search_params_t;

/**
 * @brief 带过滤的向量搜索
 * @param coll 集合句柄
 * @param query 查询向量
 * @param dim 向量维度
 * @param params 过滤搜索参数
 * @return 搜索结果集（调用者通过 vdb_results_free 释放），失败返回 NULL
 */
vdb_results_t *vdb_search_filtered(vdb_collection_t *coll, const float *query, int32_t dim,
                                    const vdb_filtered_search_params_t *params);

/**
 * @brief 混合搜索（向量 + BM25）
 * @param coll 集合句柄
 * @param query 查询向量
 * @param dim 向量维度
 * @param params 混合搜索参数
 * @return 搜索结果集（调用者通过 vdb_results_free 释放），失败返回 NULL
 */
vdb_results_t *vdb_hybrid_search(vdb_collection_t *coll, const float *query, int32_t dim,
                                  const vdb_hybrid_search_params_t *params);

/**
 * @brief 获取过滤表达式解析错误
 * @param db 数据库句柄
 * @return 错误信息（如果上次过滤解析失败）
 */
const char *vdb_filter_error(vdb_handle_t *db);

/* ========================================================================
 * 统计与指标
 * ======================================================================== */

/**
 * @brief 数据库统计信息
 */
typedef struct {
    int32_t collection_count;   /**< 集合数量 */
    int64_t total_vectors;      /**< 总向量数 */
    int64_t total_memory_bytes; /**< 总内存占用 */
    int64_t uptime_ms;          /**< 运行时长（毫秒） */
} vdb_stats_t;

/**
 * @brief 获取数据库统计信息
 * @param db 数据库句柄
 * @param stats 输出统计
 * @return 0 成功，负数失败
 */
int vdb_stats(vdb_handle_t *db, vdb_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_VDB_API_H */