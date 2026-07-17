/**
 * @file vector_api.h
 * @brief 向量数据库 API 头文件
 *
 * 提供向量集合管理、向量插入、搜索、删除等 REST API 接口。
 * 
 * API 端点:
 * - POST   /api/v1/collections           创建集合
 * - GET    /api/v1/collections           列出所有集合
 * - GET    /api/v1/collections/:name     获取集合信息
 * - DELETE /api/v1/collections/:name     删除集合
 * - POST   /api/v1/collections/:name/insert  插入向量
 * - POST   /api/v1/collections/:name/search  搜索向量
 * - DELETE /api/v1/collections/:name/vectors 删除向量
 */
#ifndef DB_VECTOR_API_H
#define DB_VECTOR_API_H

#include <stdint.h>
#include <stdbool.h>
#include <db/core/vector_query.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/** 向量 API 句柄（不透明） */
typedef struct VectorAPI_s VectorAPI;

/** 集合描述符（不透明） */
typedef struct VectorCollection_s VectorCollection;

/** 搜索结果（不透明） */
typedef struct VectorSearchResults_s VectorSearchResults;

/** 距离度量（从 vector_query.h 导入 VectorIndexType） */
typedef enum VectorMetricType_e {
    VECTOR_METRIC_L2 = 0,        /**< L2 距离（欧几里得） */
    VECTOR_METRIC_COSINE = 1,    /**< 余弦相似度 */
    VECTOR_METRIC_IP = 2         /**< 内积 */
} VectorMetricType;

/** 错误码 */
typedef enum VectorAPIError_e {
    VECTOR_API_OK = 0,
    VECTOR_API_ERR_NOT_FOUND = -1,
    VECTOR_API_ERR_EXISTS = -2,
    VECTOR_API_ERR_INVALID_PARAM = -3,
    VECTOR_API_ERR_NO_MEMORY = -4,
    VECTOR_API_ERR_IO = -5,
    VECTOR_API_ERR_INTERNAL = -99
} VectorAPIError;

/* ========================================================================
 * 集合元数据
 * ======================================================================== */

/** 集合信息 */
typedef struct VectorCollectionInfo_s {
    char name[128];              /**< 集合名称 */
    int32_t dimension;           /**< 向量维度 */
    int32_t size;                /**< 向量数量 */
    VectorIndexType index_type;  /**< 索引类型 */
    VectorMetricType metric_type;/**< 距离度量 */
    int64_t created_at;          /**< 创建时间 */
    int64_t updated_at;          /**< 更新时间 */
} VectorCollectionInfo;

/** 创建集合参数 */
typedef struct VectorCreateParams_s {
    const char *name;            /**< 集合名称 */
    int32_t dimension;           /**< 向量维度 */
    VectorIndexType index_type;  /**< 索引类型 */
    VectorMetricType metric_type;/**< 距离度量 */
    int32_t index_ef_search;     /**< HNSW ef_search 参数 */
    int32_t index_m;             /**< HNSW M 参数 */
    int32_t index_ef_construction; /**< HNSW ef_construction 参数 */
} VectorCreateParams;

/** 插入向量参数 */
typedef struct VectorInsertParams_s {
    const char *collection;      /**< 集合名称 */
    int32_t n;                   /**< 向量数量 */
    int64_t *ids;                /**< 向量 ID 数组（可选，NULL 则自动生成） */
    float **vectors;             /**< 向量数据数组 */
    void **metadata;             /**< 元数据数组（可选） */
    int32_t *metadata_sizes;     /**< 元数据大小数组 */
} VectorInsertParams;

/** 搜索向量参数 */
typedef struct VectorSearchParams_s {
    const char *collection;      /**< 集合名称 */
    const float *query;          /**< 查询向量 */
    int32_t top_k;               /**< 返回结果数 */
    float radius;                /**< 搜索半径（距离阈值） */
    bool with_distance;          /**< 是否返回距离 */
    bool with_metadata;          /**< 是否返回元数据 */
    const char *filter;          /**< 过滤条件（JSON） */
} VectorSearchParams;

/** 搜索结果项 */
typedef struct VectorSearchResult_s {
    int64_t id;                  /**< 向量 ID */
    float distance;              /**< 距离 */
    void *metadata;              /**< 元数据 */
    int32_t metadata_size;       /**< 元数据大小 */
} VectorSearchResult;

/* ========================================================================
 * API 生命周期
 * ======================================================================== */

/**
 * @brief 创建向量 API 实例
 * @param data_dir 数据目录路径
 * @return API 句柄，失败返回 NULL
 */
VectorAPI *vector_api_create(const char *data_dir);

/**
 * @brief 销毁向量 API 实例
 * @param api API 句柄
 */
void vector_api_destroy(VectorAPI *api);

/**
 * @brief 获取错误信息
 * @param api API 句柄
 * @return 错误信息字符串
 */
const char *vector_api_error(VectorAPI *api);

/* ========================================================================
 * 集合管理
 * ======================================================================== */

/**
 * @brief 创建集合
 * @param api API 句柄
 * @param params 创建参数
 * @return 0 成功，负数失败
 */
int vector_api_create_collection(VectorAPI *api, const VectorCreateParams *params);

/**
 * @brief 删除集合
 * @param api API 句柄
 * @param name 集合名称
 * @return 0 成功，负数失败
 */
int vector_api_drop_collection(VectorAPI *api, const char *name);

/**
 * @brief 获取集合信息
 * @param api API 句柄
 * @param name 集合名称
 * @param info 输出集合信息
 * @return 0 成功，负数失败
 */
int vector_api_get_collection(VectorAPI *api, const char *name,
                              VectorCollectionInfo *info);

/**
 * @brief 列出所有集合
 * @param api API 句柄
 * @param names 输出数组（调用者负责释放）
 * @param n 输出集合数量
 * @return 0 成功，负数失败
 */
int vector_api_list_collections(VectorAPI *api, char ***names, int32_t *n);

/**
 * @brief 释放集合名称数组
 * @param names 名称数组
 * @param n 数量
 */
void vector_api_free_names(char **names, int32_t n);

/* ========================================================================
 * 向量操作
 * ======================================================================== */

/**
 * @brief 插入向量
 * @param api API 句柄
 * @param params 插入参数
 * @param out_ids 输出 ID 数组（如果 params->ids 为 NULL）
 * @return 成功插入数量，负数失败
 */
int vector_api_insert(VectorAPI *api, const VectorInsertParams *params,
                      int64_t *out_ids);

/**
 * @brief 搜索向量
 * @param api API 句柄
 * @param params 搜索参数
 * @return 搜索结果，失败返回 NULL
 */
VectorSearchResults *vector_api_search(VectorAPI *api, const VectorSearchParams *params);

/**
 * @brief 获取搜索结果数量
 * @param results 搜索结果
 * @return 结果数量
 */
int32_t vector_api_results_count(const VectorSearchResults *results);

/**
 * @brief 获取搜索结果项
 * @param results 搜索结果
 * @param index 索引
 * @return 结果项，索引无效返回 NULL
 */
const VectorSearchResult *vector_api_results_get(const VectorSearchResults *results,
                                                  int32_t index);

/**
 * @brief 释放搜索结果
 * @param results 搜索结果
 */
void vector_api_results_free(VectorSearchResults *results);

/**
 * @brief 删除向量
 * @param api API 句柄
 * @param collection 集合名称
 * @param ids 要删除的 ID 数组
 * @param n ID 数量
 * @return 成功删除数量，负数失败
 */
int vector_api_delete(VectorAPI *api, const char *collection,
                      const int64_t *ids, int32_t n);

/**
 * @brief 获取集合大小
 * @param api API 句柄
 * @param collection 集合名称
 * @return 向量数量，负数失败
 */
int32_t vector_api_size(VectorAPI *api, const char *collection);

/* ========================================================================
 * 持久化
 * ======================================================================== */

/**
 * @brief 保存所有集合到磁盘
 * @param api API 句柄
 * @return 0 成功，负数失败
 */
int vector_api_save(VectorAPI *api);

/**
 * @brief 加载所有集合从磁盘
 * @param api API 句柄
 * @return 0 成功，负数失败
 */
int vector_api_load(VectorAPI *api);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_API_H */
