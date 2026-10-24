/**
 * @file rdf_engine.h
 * @brief RDF 知识图谱引擎头文件
 *
 * 实现 Triple Store 和 SPARQL 子集查询能力。
 * 用于知识图谱存储和推理查询。
 */
#ifndef DB_RDF_ENGINE_H
#define DB_RDF_ENGINE_H

#include "storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * RDF 相关类型定义
 * ======================================================================== */

/**
 * @brief RDF 术语类型
 */
typedef enum {
    RDF_URI = 0,         /**< URI 引用 */
    RDF_BLANK = 1,       /**< 空白节点 */
    RDF_LITERAL = 2,     /**< 字面量 */
} rdf_term_type_t;

/**
 * @brief RDF 术语
 */
typedef struct rdf_term_s {
    rdf_term_type_t type;    /**< 术语类型 */
    char value[512];         /**< 值 */
    char lang[16];           /**< 语言标签（用于字面量） */
    char datatype[64];       /**< 数据类型 URI（用于字面量） */
} rdf_term_t;

/**
 * @brief RDF 三元组 (Subject-Predicate-Object)
 */
typedef struct rdf_triple_s {
    rdf_term_t subject;      /**< 主语 */
    rdf_term_t predicate;    /**< 谓语 */
    rdf_term_t object;       /**< 宾语 */
    int64_t timestamp;       /**< 时间戳 */
} rdf_triple_t;

/**
 * @brief SPARQL 查询类型
 */
typedef enum {
    SPARQL_SELECT = 0,       /**< SELECT 查询 */
    SPARQL_CONSTRUCT = 1,    /**< CONSTRUCT 查询 */
    SPARQL_ASK = 2,          /**< ASK 查询 */
} sparql_query_type_t;

/**
 * @brief SPARQL 绑定变量
 */
typedef struct sparql_binding_s {
    char var_name[64];       /**< 变量名（不带 ?） */
    rdf_term_t value;        /**< 绑定值 */
} sparql_binding_t;

/**
 * @brief SPARQL 查询结果
 */
typedef struct sparql_result_s {
    sparql_binding_t *bindings;  /**< 绑定数组 */
    int32_t binding_count;       /**< 绑定数量 */
    int32_t row_count;           /**< 结果行数 */
    int32_t capacity;            /**< 容量 */
} sparql_result_t;

/**
 * @brief RDF 引擎数据库
 */
typedef struct rdf_engine_db_s {
    char name[256];            /**< 图名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    uint64_t num_triples;      /**< 三元组数量 */
    uint64_t num_subjects;     /**< 主语数量 */
    uint64_t num_predicates;   /**< 谓语数量 */
    uint64_t num_objects;      /**< 宾语数量 */
} rdf_engine_db_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 获取 RDF 引擎操作表
 */
const storage_ops_t *rdf_engine_get_ops(void);

/**
 * @brief 初始化 RDF 引擎
 */
int rdf_engine_init(const char *data_dir);

/**
 * @brief 关闭 RDF 引擎
 */
int rdf_engine_shutdown(void);

/**
 * @brief 创建 RDF 图
 */
int rdf_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开 RDF 图
 */
void *rdf_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭 RDF 图
 */
int rdf_engine_close(void *rel);

/**
 * @brief 删除 RDF 图
 */
int rdf_engine_drop(const char *name);

/**
 * @brief 插入三元组
 */
int rdf_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 删除三元组
 */
int rdf_engine_delete(void *rel, const rdf_triple_t *triple);

/**
 * @brief 获取统计信息
 */
int rdf_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * 三元组查询 API
 * ======================================================================== */

/**
 * @brief 三元组模式匹配查询
 *
 * @param rel 图句柄
 * @param subject 主语模式（NULL 表示任意）
 * @param predicate 谓语模式（NULL 表示任意）
 * @param object 宾语模式（NULL 表示任意）
 * @param results 结果数组
 * @param max_results 最大结果数
 * @param num_results 实际结果数
 * @return 0 成功，-1 失败
 */
int rdf_engine_match(void *rel,
                     const rdf_term_t *subject,
                     const rdf_term_t *predicate,
                     const rdf_term_t *object,
                     rdf_triple_t *results,
                     int32_t max_results,
                     int32_t *num_results);

/**
 * @brief 获取指定主语的所有出边
 */
int rdf_engine_get_outgoing(void *rel, const rdf_term_t *subject,
                             rdf_triple_t *results, int32_t max_results,
                             int32_t *num_results);

/**
 * @brief 获取指定宾语的所有入边
 */
int rdf_engine_get_incoming(void *rel, const rdf_term_t *object,
                             rdf_triple_t *results, int32_t max_results,
                             int32_t *num_results);

/* ========================================================================
 * SPARQL 查询 API
 * ======================================================================== */

/**
 * @brief 执行 SPARQL SELECT 查询
 *
 * @param rel 图句柄
 * @param sparql_query SPARQL 查询字符串
 * @param result 查询结果
 * @return 0 成功，-1 失败
 */
int rdf_engine_sparql_select(void *rel, const char *sparql_query,
                              sparql_result_t *result);

/**
 * @brief 执行 SPARQL ASK 查询
 *
 * @param rel 图句柄
 * @param sparql_query SPARQL ASK 查询字符串
 * @return true 存在匹配，false 不存在匹配
 */
bool rdf_engine_sparql_ask(void *rel, const char *sparql_query);

/**
 * @brief 释放 SPARQL 查询结果
 */
void rdf_engine_free_result(sparql_result_t *result);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 创建 URI 术语
 */
rdf_term_t rdf_term_uri(const char *uri);

/**
 * @brief 创建空白节点术语
 */
rdf_term_t rdf_term_blank(const char *id);

/**
 * @brief 创建字面量术语
 */
rdf_term_t rdf_term_literal(const char *value, const char *lang,
                             const char *datatype);

/**
 * @brief 比较两个 RDF 术语是否相等
 */
bool rdf_term_equals(const rdf_term_t *a, const rdf_term_t *b);

/**
 * @brief 检查术语是否匹配模式（模式中 type=ANY 表示通配）
 */
bool rdf_term_matches(const rdf_term_t *term, const rdf_term_t *pattern);

#ifdef __cplusplus
}
#endif

#endif /* DB_RDF_ENGINE_H */
