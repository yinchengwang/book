/**
 * @file sql_parse.c
 * @brief canonical SQL 解析器统一入口实现（T6, WS-C）
 *
 * 封装 flex/bison 生成的 sql_yy* 符号，对外提供 sql_parse() 单一入口。
 * 非线程安全：parsetree/yy_error_buf 为静态存储，见 sql_parse.h 注释。
 */

#include "db/parser/sql/sql_parse.h"
#include <stddef.h>

/* flex/bison 生成符号（prefix=sql_yy） */
typedef void *YY_BUFFER_STATE;
extern YY_BUFFER_STATE sql_yy_scan_string(const char *str);
extern void sql_yy_delete_buffer(YY_BUFFER_STATE b);
extern int sql_yyparse(void);
extern Node *sql_yy_get_parsetree(void);
extern const char *sql_yy_get_error(void);
extern void sql_yy_reset_state(void);

Node *sql_parse(const char *sql) {
    if (sql == NULL) return NULL;
    sql_yy_reset_state();
    YY_BUFFER_STATE buf = sql_yy_scan_string(sql);
    if (buf == NULL) return NULL;
    int rc = sql_yyparse();
    sql_yy_delete_buffer(buf);
    if (rc != 0) return NULL;
    return sql_yy_get_parsetree();
}

const char *sql_parse_last_error(void) {
    return sql_yy_get_error();
}
