/**
 * @file sql_semantic.h
 * @brief SQL 语义分析器接口
 *
 * 语义分析负责：
 * - 表和列的存在性验证（走 catalog API，见 db/catalog.h）
 * - 类型检查
 * - 表达式求值
 *
 * T5：与 KV 存储解耦，sql_semantic_create 不再接收 kv_t。
 */
#ifndef DB_SQL_SEMANTIC_H
#define DB_SQL_SEMANTIC_H

#include "db/parser/sql/sql.h"
#include "db/catalog.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 语义上下文
 * ============================================================ */

/** 语义分析上下文 */
typedef struct sql_semantic_s sql_semantic_t;

/**
 * @brief 创建语义分析上下文
 * @return 语义上下文，失败返回 NULL
 *
 * 表/列元数据经由全局 catalog 获取，调用前需先 catalog_init()。
 */
sql_semantic_t *sql_semantic_create(void);

/**
 * @brief 销毁语义分析上下文
 * @param ctx 语义上下文
 */
void sql_semantic_destroy(sql_semantic_t *ctx);

/* ============================================================
 * 语义分析 API
 * ============================================================ */

/**
 * @brief 分析 SELECT 语句
 * @param ctx 语义上下文
 * @param node AST 节点
 * @param out_table 输出：表信息（上下文持有，下次分析前有效），可为 NULL
 * @return 0 成功，-1 失败
 */
int sql_semantic_analyze_select(sql_semantic_t *ctx, const sql_node_t *node,
                              const table_info_t **out_table);

/**
 * @brief 分析 UPDATE 语句
 * @param ctx 语义上下文
 * @param node AST 节点
 * @param out_table 输出：表信息（上下文持有，下次分析前有效），可为 NULL
 * @return 0 成功，-1 失败
 */
int sql_semantic_analyze_update(sql_semantic_t *ctx, const sql_node_t *node,
                              const table_info_t **out_table);

/**
 * @brief 分析 DELETE 语句
 * @param ctx 语义上下文
 * @param node AST 节点
 * @param out_table 输出：表信息（上下文持有，下次分析前有效），可为 NULL
 * @return 0 成功，-1 失败
 */
int sql_semantic_analyze_delete(sql_semantic_t *ctx, const sql_node_t *node,
                              const table_info_t **out_table);

/**
 * @brief 分析 CREATE TABLE 语句
 * @param ctx 语义上下文
 * @param node AST 节点
 * @return 0 成功，-1 失败
 */
int sql_semantic_analyze_create_table(sql_semantic_t *ctx, const sql_node_t *node);

/**
 * @brief 分析 DROP TABLE 语句
 * @param ctx 语义上下文
 * @param node AST 节点
 * @return 0 成功，-1 失败
 */
int sql_semantic_analyze_drop_table(sql_semantic_t *ctx, const sql_node_t *node);

/**
 * @brief 分析 INSERT 语句
 * @param ctx 语义上下文
 * @param node AST 节点
 * @param out_table 输出：表信息（上下文持有，下次分析前有效），可为 NULL
 * @return 0 成功，-1 失败
 */
int sql_semantic_analyze_insert(sql_semantic_t *ctx, const sql_node_t *node,
                              const table_info_t **out_table);

/**
 * @brief 获取错误信息
 * @param ctx 语义上下文
 * @return 错误信息
 */
const char *sql_semantic_errmsg(sql_semantic_t *ctx);

/**
 * @brief 查找列索引
 * @param columns 列信息数组
 * @param ncolumns 列数量
 * @param name 列名
 * @return 列索引，未找到返回 -1
 */
int sql_semantic_find_column(const column_info_t *columns, int ncolumns,
                             const char *name);

/**
 * @brief 获取当前表信息（内部使用）
 * @param ctx 语义上下文
 * @return 表信息，无当前表返回 NULL
 */
const table_info_t *sql_semantic_get_current_table(sql_semantic_t *ctx);

/**
 * @brief 获取当前表的列信息（内部使用）
 * @param ctx 语义上下文
 * @param out_columns 输出：列信息数组（上下文持有），可为 NULL
 * @return 列数量，无当前表返回 -1
 */
int sql_semantic_get_columns(sql_semantic_t *ctx, const column_info_t **out_columns);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_SEMANTIC_H */
