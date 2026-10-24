/**
 * @file sql_parse.h
 * @brief canonical SQL 解析器统一入口（T6, WS-C）
 *
 * 所有 SQL 解析调用方（planner、executor、validator 等）统一经此入口，
 * 返回 parsenodes.h 定义的 planner-facing AST。
 *
 * 线程模型：底层 parsetree 与错误缓冲为静态存储，非线程安全；
 * 仅保证单线程内顺序调用安全（每次 sql_parse() 调用前自动重置状态）。
 */

#ifndef DB_PARSER_SQL_PARSE_H
#define DB_PARSER_SQL_PARSE_H

#include "db/parser/sql/parsenodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 解析单条 SQL 语句
 * @param sql SQL 文本（单条语句）
 * @return 成功返回 AST 根节点（SelectStmt 等）；失败返回 NULL，
 *         错误信息经 sql_parse_last_error() 获取
 */
Node *sql_parse(const char *sql);

/**
 * @brief 最近一次解析的错误串（带 line:column 位置）
 * @return 错误描述；无错误时为空串（永不返回 NULL）
 */
const char *sql_parse_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_PARSER_SQL_PARSE_H */
