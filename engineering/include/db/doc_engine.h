/**
 * @file doc_engine.h
 * @brief 文档存储引擎头文件
 *
 * 定义文档存储引擎的接口和数据结构，支持 JSON 文档存储和查询。
 */
#ifndef DB_DOC_ENGINE_H
#define DB_DOC_ENGINE_H

#include "storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 文档查询结果类型
 * ======================================================================== */

/**
 * @brief 文档查询结果
 */
typedef struct doc_query_doc_s {
    char *doc_id;              /**< 文档 ID */
    uint32_t doc_id_len;       /**< 文档 ID 长度 */
    char *field_value;         /**< 匹配的字段值 */
    uint32_t field_value_len;  /**< 字段值长度 */
} doc_query_doc_t;

/**
 * @brief 文档查询结果集
 */
typedef struct doc_query_result_s {
    doc_query_doc_t *docs;     /**< 文档数组 */
    uint32_t count;            /**< 结果数量 */
    uint32_t capacity;         /**< 容量 */
} doc_query_result_t;

/**
 * @brief 文档引擎数据库
 */
typedef struct doc_engine_db_s {
    char name[256];            /**< 集合名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    uint64_t num_docs;         /**< 文档数量 */
} doc_engine_db_t;

/* ========================================================================
 * 基础 API
 * ======================================================================== */

/**
 * @brief 获取文档引擎操作表
 */
const storage_ops_t *doc_engine_get_ops(void);

/**
 * @brief 初始化文档引擎
 */
int doc_engine_init(const char *data_dir);

/**
 * @brief 关闭文档引擎
 */
int doc_engine_shutdown(void);

/**
 * @brief 创建文档集合
 */
int doc_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开文档集合
 */
void *doc_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭文档集合
 */
int doc_engine_close(void *rel);

/**
 * @brief 删除文档集合
 */
int doc_engine_drop(const char *name);

/**
 * @brief 插入文档
 */
int doc_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 获取文档
 */
int doc_engine_get(void *rel, const void *key, size_t key_len,
                   void **out_data, size_t *out_len);

/**
 * @brief 获取统计信息
 */
int doc_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * JSONPath 查询 API
 * ======================================================================== */

/**
 * @brief JSONPath 查询结果
 */
typedef struct doc_jsonpath_result_s {
    char *doc_id;              /**< 文档 ID */
    uint32_t doc_id_len;       /**< 文档 ID 长度 */
    char *jsonpath;            /**< 匹配的 JSONPath */
    char *value;               /**< 匹配的值 */
    size_t value_len;          /**< 值长度 */
} doc_jsonpath_result_t;

/**
 * @brief 使用 JSONPath 查询文档
 *
 * 支持的 JSONPath 表达式：
 * - $.field - 获取字段值
 * - $.nested.field - 获取嵌套字段
 * - $.arr[0] - 获取数组元素
 * - $.arr[*] - 获取所有数组元素
 *
 * @param rel 文档引擎句柄
 * @param jsonpath JSONPath 表达式（如 "$.name" 或 "$.store.book[0].title"）
 * @param results 输出结果（调用者负责释放）
 * @param max_results 最大结果数
 * @return 匹配结果数，-1 失败
 */
int doc_engine_query_jsonpath(void *rel, const char *jsonpath,
                              doc_jsonpath_result_t **results,
                              uint32_t max_results);

/**
 * @brief 释放 JSONPath 查询结果
 * @param results 结果数组
 * @param count 结果数量
 */
void doc_engine_free_jsonpath_results(doc_jsonpath_result_t *results, uint32_t count);

/* ========================================================================
 * 基础字段查询 API（兼容性）
 * ======================================================================== */

/**
 * @brief 查询文档（基于字段）
 * @param rel 文档引擎句柄
 * @param json_path JSONPath 路径
 * @param query_data 查询数据（未使用）
 * @param query_len 查询数据长度
 * @param results 输出结果
 * @param num_results 结果数量
 * @return 0 成功，-1 失败
 */
int doc_engine_query(void *rel,
                     const char *json_path,
                     const void *query_data, size_t query_len,
                     doc_query_result_t *results, uint32_t *num_results);

/**
 * @brief 释放文档查询结果
 * @param results 查询结果
 */
void doc_engine_free_results(doc_query_result_t *results);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_ENGINE_H */
