/**
 * @file doc_engine.h
 * @brief 文档存储引擎头文件
 *
 * 定义文档存储引擎的接口和数据结构，支持 JSON 文档存储和查询。
 */
#ifndef DB_DOC_ENGINE_H
#define DB_DOC_ENGINE_H

#include "storage_engine.h"
#include "db/mm_pool.h"
#include "db/storage/doc/bm25.h"
#include "db/storage/doc/doc_inverted.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct lock_manager_s lock_manager_t;

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

    /* 内存池相关 */
    void *mem_pool;            /**< 内存池指针 */
    bool use_mem_pool;         /**< 是否使用内存池 */

    /* 倒排索引相关 */
    void *inverted_index;      /**< 倒排索引指针 */
    bool use_inverted_index;   /**< 是否使用倒排索引 */

    /* 并发控制 */
    lock_manager_t *lockmgr;   /**< 锁管理器 */
    void *rwlock;              /**< 读写锁 */
    bool use_lock;             /**< 是否启用锁 */

    /* BM25 打分器 */
    bm25_scorer_t *bm25_scorer; /**< BM25 评分器 */
    bool use_bm25;              /**< 是否启用 BM25 */
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

/* ========================================================================
 * 倒排索引相关 API
 * ======================================================================== */

/**
 * @brief 启用倒排索引
 *
 * @param rel 文档引擎句柄
 * @param tokenizer 分词器类型（可为 NULL，使用默认）
 * @return 0 成功，-1 失败
 */
int doc_engine_enable_inverted_index(void *rel, const char *tokenizer);

/**
 * @brief 搜索文档（使用倒排索引）
 *
 * @param rel 文档引擎句柄
 * @param query 查询字符串
 * @param results 输出结果（调用者分配）
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t doc_engine_inverted_search(void *rel, const char *query,
                                    void *results, uint32_t max_results);

/**
 * @brief 保存倒排索引
 *
 * @param rel 文档引擎句柄
 * @return 0 成功，-1 失败
 */
int doc_engine_save_inverted_index(void *rel);

/**
 * @brief 禁用倒排索引
 *
 * @param rel 文档引擎句柄
 */
void doc_engine_disable_inverted_index(void *rel);

/* ========================================================================
 * 内存池管理 API
 * ======================================================================== */

/**
 * @brief 启用/禁用内存池
 *
 * @param rel 文档引擎句柄
 * @param use_pool true 启用内存池，false 禁用
 * @return 0 成功，-1 失败
 */
int doc_engine_enable_mem_pool(void *rel, bool use_pool);

/**
 * @brief 获取内存池统计信息
 *
 * @param rel 文档引擎句柄
 * @param stats 输出统计信息
 * @return 0 成功，-1 失败
 */
int doc_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats);

/* ========================================================================
 * 并发锁控制 API
 * ======================================================================== */

/**
 * @brief 启用/禁用并发锁
 *
 * @param rel 文档引擎句柄
 * @param use_lock true 启用锁，false 禁用
 * @return 0 成功，-1 失败
 */
int doc_engine_enable_lock(void *rel, bool use_lock);

/**
 * @brief 获取文档引擎的读锁
 *
 * @param rel 文档引擎句柄
 * @return 0 成功，-1 失败
 */
int doc_engine_read_lock(void *rel);

/**
 * @brief 释放文档引擎的读锁
 *
 * @param rel 文档引擎句柄
 */
void doc_engine_read_unlock(void *rel);

/**
 * @brief 获取文档引擎的写锁
 *
 * @param rel 文档引擎句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，LOCK_TIMEOUT 超时，-1 失败
 */
int doc_engine_write_lock(void *rel, uint32_t timeout_ms);

/**
 * @brief 释放文档引擎的写锁
 *
 * @param rel 文档引擎句柄
 */
void doc_engine_write_unlock(void *rel);

/* ========================================================================
 * BM25 打分 API
 * ======================================================================== */

/**
 * @brief 启用 BM25 打分
 *
 * @param rel 文档引擎句柄
 * @param k1 BM25 k1 参数（默认 1.2）
 * @param b BM25 b 参数（默认 0.75）
 * @return 0 成功，-1 失败
 */
int doc_engine_enable_bm25(void *rel, double k1, double b);

/**
 * @brief 使用 BM25 搜索并打分
 *
 * @param rel 文档引擎句柄
 * @param query 查询字符串
 * @param results 输出结果（包含 BM25 分数，按分数降序）
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t doc_engine_bm25_search(void *rel, const char *query,
                                 doc_inverted_result_t *results,
                                 uint32_t max_results);

/**
 * @brief 设置索引信息（用于 BM25 计算）
 *
 * @param rel 文档引擎句柄
 * @param num_docs 文档总数
 * @param avg_doc_len 平均文档长度
 */
void doc_engine_set_bm25_index_info(void *rel, uint64_t num_docs,
                                     uint32_t avg_doc_len);

/**
 * @brief 禁用 BM25 打分
 *
 * @param rel 文档引擎句柄
 */
void doc_engine_disable_bm25(void *rel);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_ENGINE_H */
