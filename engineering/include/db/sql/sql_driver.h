/**
 * @file sql_driver.h
 * @brief SQL 执行驱动 - 端到端 SQL 执行入口
 *
 * Task 5.2: SQL 解析器端到端集成
 *
 * 封装 execute_sql() 入口函数，整合解析器、计划器、执行器：
 * 1. 解析 SQL 文本（sql_parser）
 * 2. 生成查询计划（planner）
 * 3. 执行计划（executor）
 * 4. 收集结果（QueryResult）
 */

#ifndef DB_SQL_DRIVER_H
#define DB_SQL_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * QueryResult - 查询结果结构
 * ======================================================================== */

/**
 * @brief 查询结果结构
 *
 * 用于返回 SQL 执行结果，包括：
 * - 结果集的行列数
 * - 列名数组
 * - 行数据（字符串形式）
 * - 错误信息
 */
typedef struct QueryResult {
    int      nrows;           /**< 结果行数 */
    int      ncols;           /**< 结果列数 */
    char   **col_names;       /**< 列名数组（ncols 个元素） */
    char  ***rows;            /**< 行数据（nrows 行，每行 ncols 列） */
    char    *error_msg;       /**< 错误信息（成功时为 NULL） */
} QueryResult;

/* ========================================================================
 * 核心执行 API
 * ======================================================================== */

/**
 * @brief 执行 SQL 语句
 *
 * 完整的 SQL 执行流程：
 * 1. 解析 SQL 文本生成 AST
 * 2. 生成查询计划
 * 3. 初始化执行器
 * 4. 执行查询并收集结果
 * 5. 返回 QueryResult
 *
 * @param sql SQL 语句字符串
 * @param db  数据库句柄（kv_t*）
 *
 * @return 查询结果结构；失败时 error_msg 非空
 */
QueryResult *execute_sql(const char *sql, void *db);

/**
 * @brief 释放查询结果
 *
 * @param result 查询结果（可为 NULL）
 */
void FreeQueryResult(QueryResult *result);

/**
 * @brief 执行 DDL 语句
 *
 * 专门用于 CREATE TABLE / DROP TABLE 等 DDL 语句。
 *
 * @param sql DDL 语句
 * @param db  数据库句柄
 *
 * @return 0 表示成功；非 0 表示失败
 */
int execute_ddl(const char *sql, void *db);

/**
 * @brief 执行 DML 语句（不返回结果）
 *
 * 用于 INSERT / UPDATE / DELETE 等 DML 语句。
 *
 * @param sql DML 语句
 * @param db  数据库句柄
 *
 * @return 影响的行数；-1 表示失败
 */
int execute_dml(const char *sql, void *db);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 创建空的 QueryResult
 *
 * @return 新创建的 QueryResult；失败返回 NULL
 */
QueryResult *CreateQueryResult(void);

/**
 * @brief 设置 QueryResult 的错误信息
 *
 * @param result 查询结果
 * @param fmt    格式化字符串
 * @param ...    可变参数
 */
void SetQueryResultError(QueryResult *result, const char *fmt, ...);

/**
 * @brief 打印 QueryResult（调试用）
 *
 * @param result 查询结果
 */
void PrintQueryResult(const QueryResult *result);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_DRIVER_H */