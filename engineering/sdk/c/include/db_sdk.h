/**
 * @file db_sdk.h
 * @brief 数据库统一 C SDK 接口（Task 3.9 + 3.10）
 *
 * 提供 SQL 和向量操作的统一 C API，作为外部 SDK 层。
 * 封装 PlannerContext + executor_run + vdb_api 管线。
 */
#ifndef DB_SDK_H
#define DB_SDK_H

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
typedef struct db_sdk_s db_sdk_t;

/** 查询结果 */
typedef struct db_sdk_result_s {
    char **columns;          /**< 列名数组 */
    int num_cols;           /**< 列数 */
    char ***rows;           /**< 行数据（rows[i][j] 是文本） */
    int num_rows;           /**< 行数 */
    char *error_msg;        /**< 错误消息 */
} db_sdk_result_t;

/* ========================================================================
 * 数据库生命周期
 * ======================================================================== */

/**
 * @brief 打开数据库
 * @param path 数据库路径
 * @return 数据库句柄，NULL 表示失败
 */
db_sdk_t *db_sdk_open(const char *path);

/**
 * @brief 关闭数据库
 * @param db 数据库句柄
 * @return 0 成功
 */
int db_sdk_close(db_sdk_t *db);

/* ========================================================================
 * SQL 执行
 * ======================================================================== */

/**
 * @brief 执行 SQL 查询
 * @param db 数据库句柄
 * @param sql SQL 语句
 * @return 查询结果（需调用 db_sdk_free_result 释放）
 */
db_sdk_result_t *db_sdk_query(db_sdk_t *db, const char *sql);

/**
 * @brief 释放查询结果
 * @param result 查询结果
 */
void db_sdk_free_result(db_sdk_result_t *result);

/**
 * @brief 获取最后错误消息
 * @param db 数据库句柄
 * @return 错误消息字符串
 */
const char *db_sdk_error(db_sdk_t *db);

/* ========================================================================
 * 向量操作：错误码
 * ======================================================================== */

#define DB_SDK_OK               0
#define DB_SDK_ERR_NOT_FOUND    (-1)
#define DB_SDK_ERR_EXISTS       (-2)
#define DB_SDK_ERR_INVALID_PARAM (-3)
#define DB_SDK_ERR_NO_MEMORY    (-4)
#define DB_SDK_ERR_IO           (-5)
#define DB_SDK_ERR_INTERNAL     (-99)

/* ========================================================================
 * 向量操作：集合管理
 * ======================================================================== */

/** 集合句柄（不透明） */
typedef struct db_sdk_collection_s db_sdk_collection_t;

/** 索引类型 */
typedef enum {
    DB_SDK_INDEX_AUTO = 0,
    DB_SDK_INDEX_HNSW = 1,
    DB_SDK_INDEX_IVF = 2,
    DB_SDK_INDEX_DISKANN = 3,
    DB_SDK_INDEX_BRUTE_FORCE = 4
} db_sdk_index_type_t;

/** 距离度量 */
typedef enum {
    DB_SDK_METRIC_L2 = 0,
    DB_SDK_METRIC_COSINE = 1,
    DB_SDK_METRIC_IP = 2
} db_sdk_metric_type_t;

/** 集合配置 */
typedef struct {
    char name[128];
    int32_t dimension;
    db_sdk_index_type_t index_type;
    db_sdk_metric_type_t metric_type;
    int32_t index_ef_search;
    int32_t index_m;
    int32_t index_ef_construction;
} db_sdk_collection_config_t;

/**
 * @brief 创建集合
 * @param db 数据库句柄
 * @param name 集合名称
 * @param config 集合配置
 * @return DB_SDK_OK 成功，负数失败
 */
int db_sdk_create_collection(db_sdk_t *db, const char *name,
                             const db_sdk_collection_config_t *config);

/**
 * @brief 删除集合
 * @param db 数据库句柄
 * @param name 集合名称
 * @return DB_SDK_OK 成功，负数失败
 */
int db_sdk_drop_collection(db_sdk_t *db, const char *name);

/**
 * @brief 列出所有集合名称
 * @param db 数据库句柄
 * @param names 输出名称数组（调用者通过 db_sdk_free_names 释放）
 * @param count 输出数量
 * @return DB_SDK_OK 成功，负数失败
 */
int db_sdk_list_collections(db_sdk_t *db, char ***names, int32_t *count);

/**
 * @brief 释放集合名称数组
 * @param names 名称数组
 * @param count 数量
 */
void db_sdk_free_names(char **names, int32_t count);

/**
 * @brief 获取集合句柄
 * @param db 数据库句柄
 * @param name 集合名称
 * @return 集合句柄，失败返回 NULL
 */
db_sdk_collection_t *db_sdk_get_collection(db_sdk_t *db, const char *name);

/**
 * @brief 获取集合配置信息
 * @param coll 集合句柄
 * @param config 输出配置
 * @return DB_SDK_OK 成功，负数失败
 */
int db_sdk_collection_info(db_sdk_collection_t *coll,
                           db_sdk_collection_config_t *config);

/**
 * @brief 获取集合向量数量
 * @param coll 集合句柄
 * @return 向量数量，负数失败
 */
int32_t db_sdk_size(db_sdk_collection_t *coll);

/* ========================================================================
 * 向量操作：增删查
 * ======================================================================== */

/** 搜索参数 */
typedef struct {
    int32_t top_k;
    float radius;
    bool with_distance;
    bool with_metadata;
    int32_t offset;
    int32_t limit;
} db_sdk_search_params_t;

/** 搜索结果项 */
typedef struct {
    int64_t id;
    float distance;
    float score;
    void *metadata;
    int32_t metadata_size;
} db_sdk_result_item_t;

/** 搜索结果集 */
typedef struct {
    db_sdk_result_item_t *items;
    int32_t count;
    int32_t capacity;
    int64_t total_time_us;
} db_sdk_results_t;

/**
 * @brief 插入向量
 * @param coll 集合句柄
 * @param vector 向量数据
 * @param dim 向量维度
 * @param metadata 元数据（可为 NULL）
 * @param metadata_size 元数据大小
 * @return 分配的向量 ID，负数失败
 */
int64_t db_sdk_insert(db_sdk_collection_t *coll, const float *vector,
                      int32_t dim, const void *metadata, int32_t metadata_size);

/**
 * @brief 搜索最近邻
 * @param coll 集合句柄
 * @param query 查询向量
 * @param dim 向量维度
 * @param params 搜索参数
 * @return 搜索结果集（调用者通过 db_sdk_results_free 释放），失败返回 NULL
 */
db_sdk_results_t *db_sdk_search(db_sdk_collection_t *coll, const float *query,
                                 int32_t dim, const db_sdk_search_params_t *params);

/**
 * @brief 删除向量
 * @param coll 集合句柄
 * @param id 向量 ID
 * @return DB_SDK_OK 成功，负数失败
 */
int db_sdk_delete(db_sdk_collection_t *coll, int64_t id);

/**
 * @brief 释放搜索结果集
 * @param results 搜索结果集
 */
void db_sdk_results_free(db_sdk_results_t *results);

#ifdef __cplusplus
}
#endif

#endif /* DB_SDK_H */