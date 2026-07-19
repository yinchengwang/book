/**
 * @file sql_ddl.h
 * @brief DDL 执行器接口
 *
 * 提供 ALTER TABLE 等 DDL 语句的执行能力。
 */
#ifndef DB_SQL_DDL_H
#define DB_SQL_DDL_H

#include "db/sql/sql_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行 ALTER TABLE 语句
 * @param stmt ALTER TABLE 语句 AST
 * @param db   数据库句柄
 * @return 0 成功，-1 失败
 */
int exec_alter_table(AlterTableStmt *stmt, void *db);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_DDL_H */