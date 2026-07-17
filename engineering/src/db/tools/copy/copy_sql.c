/**
 * @file copy_sql.c
 * @brief SQL INSERT 格式 COPY 实现
 *
 * 生成 INSERT 语句，便于审计或迁移到其他数据库。
 */

#include "db/tools/copy.h"
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────
 * SQL 转义
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 转义 SQL 字符串值（单引号）
 */
static void sql_escape_string(FILE *fp, const char *str)
{
    fputc('\'', fp);
    for (const char *p = str; *p; p++) {
        if (*p == '\'') {
            /* 单引号转义为两个单引号 */
            fputc('\'', fp);
            fputc('\'', fp);
        } else {
            fputc(*p, fp);
        }
    }
    fputc('\'', fp);
}

/* ─────────────────────────────────────────────────────────────────
 * SQL 导出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 生成 INSERT 语句
 * @param fp 输出文件
 * @param table_name 表名
 * @param column_names 列名数组
 * @param values 值数组
 * @param num_columns 列数
 * @param null_string NULL 表示字符串
 * @return 0 成功，非 0 失败
 */
int sql_write_insert(FILE *fp, const char *table_name,
                     const char **column_names, const char **values,
                     int num_columns, const char *null_string)
{
    if (!fp || !table_name || !column_names || !values) {
        return -1;
    }

    /* INSERT INTO table (col1, col2, ...) VALUES (...) */
    fprintf(fp, "INSERT INTO %s (", table_name);

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            fprintf(fp, ", ");
        }
        fprintf(fp, "%s", column_names[i]);
    }

    fprintf(fp, ") VALUES (");

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            fprintf(fp, ", ");
        }

        if (values[i] == NULL || (null_string && strcmp(values[i], null_string) == 0)) {
            fprintf(fp, "NULL");
        } else {
            sql_escape_string(fp, values[i]);
        }
    }

    fprintf(fp, ");\n");
    return 0;
}