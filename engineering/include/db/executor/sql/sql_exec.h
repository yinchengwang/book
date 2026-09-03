/**
 * @file sql_exec.h
 * @brief SQL 查询执行器接口
 *
 * 查询执行器负责：
 * - 执行 SELECT 查询（扫描、过滤、投影）
 * - 执行 INSERT/UPDATE/DELETE
 * - 管理结果集
 */
#ifndef DB_SQL_EXEC_H
#define DB_SQL_EXEC_H

#include "db/parser/sql/sql.h"
#include "db/table.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 结果类型
 * ============================================================ */

/** 执行结果 */
typedef enum sql_exec_result_e {
    SQL_EXEC_OK = 0,
    SQL_EXEC_ERROR = -1,
    SQL_EXEC_DONE = 1   /**< 无更多行 */
} sql_exec_result_t;

/** 查询结果集 */
typedef struct sql_result_s sql_result_t;

/* ============================================================
 * 查询执行器
 * ============================================================ */

/** 执行器上下文 */
typedef struct sql_exec_s sql_exec_t;

/**
 * @brief 创建执行器
 * @param db KV 数据库句柄
 * @return 执行器句柄
 */
sql_exec_t *sql_exec_create(kv_t *db);

/**
 * @brief 销毁执行器
 * @param exec 执行器
 */
void sql_exec_destroy(sql_exec_t *exec);

/* ============================================================
 * 执行 API
 * ============================================================ */

/**
 * @brief 执行 SQL 语句
 * @param exec 执行器
 * @param node AST 节点
 * @return 结果集，失败返回 NULL
 */
sql_result_t *sql_exec(sql_exec_t *exec, const sql_node_t *node);

/**
 * @brief 执行 DDL（CREATE/DROP TABLE）
 * @param exec 执行器
 * @param node AST 节点
 * @return SQL_EXEC_OK 成功，SQL_EXEC_ERROR 失败
 */
sql_exec_result_t sql_exec_ddl(sql_exec_t *exec, const sql_node_t *node);

/**
 * @brief 获取错误信息
 * @param exec 执行器
 * @return 错误信息
 */
const char *sql_exec_errmsg(sql_exec_t *exec);

/* ============================================================
 * 结果集 API
 * ============================================================ */

/**
 * @brief 获取结果集列数
 * @param result 结果集
 * @return 列数
 */
size_t sql_result_num_columns(const sql_result_t *result);

/**
 * @brief 获取结果集行数
 * @param result 结果集
 * @return 行数
 */
size_t sql_result_num_rows(const sql_result_t *result);

/**
 * @brief 获取列名
 * @param result 结果集
 * @param col 列索引
 * @return 列名
 */
const char *sql_result_column_name(const sql_result_t *result, size_t col);

/**
 * @brief 获取列类型
 * @param result 结果集
 * @param col 列索引
 * @return 数据类型
 */
sql_data_type_t sql_result_column_type(const sql_result_t *result, size_t col);

/**
 * @brief 获取一行数据
 * @param result 结果集
 * @param row 行索引
 * @param out_values 输出值数组（调用者负责释放）
 * @return SQL_EXEC_OK 成功
 */
sql_exec_result_t sql_result_get_row(const sql_result_t *result, size_t row,
                                    void **out_values);

/**
 * @brief 释放结果集
 * @param result 结果集
 */
void sql_result_free(sql_result_t *result);

/* ============================================================
 * 表达式求值
 * ============================================================ */

/**
 * @brief 计算表达式（用于 WHERE 条件）
 * @param expr 表达式节点
 * @param row_data 行数据
 * @param meta 表元数据
 * @return true 条件成立
 */
bool sql_eval_expr(const sql_node_t *expr, const void *row_data,
                  const table_meta_t *meta);

/**
 * @brief 从行数据中提取列值
 * @param row_data 行数据
 * @param col 列定义
 * @param out_value 输出值
 * @return 0 成功
 */
int sql_extract_column(const void *row_data, const table_column_t *col,
                      void *out_value);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_EXEC_H */
