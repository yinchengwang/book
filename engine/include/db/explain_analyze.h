/**
 * @file explain_analyze.h
 * @brief EXPLAIN/ANALYZE 执行钩子（C2-2 T3/T6）
 */
#ifndef DB_EXPLAIN_ANALYZE_H
#define DB_EXPLAIN_ANALYZE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ANALYZE [table]：从表采样填充 AttStats（当前占位，完整实现待 catalog 接入） */
int sql_analyze_table(const char *table_name);

/* EXPLAIN 输出：格式化计划 + 估算行数 + 代价 */
char *sql_explain_plan(const void *planstate);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXPLAIN_ANALYZE_H */