/**
 * @file copy_text.c
 * @brief TEXT 格式 COPY 实现
 */

#include "db/tools/copy.h"
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────
 * TEXT 导出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 导出 TEXT 表头
 */
int text_write_header(FILE *fp, const char **column_names, int num_columns)
{
    if (!fp || !column_names) {
        return -1;
    }

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            fputc('\t', fp);
        }
        fprintf(fp, "%s", column_names[i]);
    }
    fputc('\n', fp);
    return 0;
}

/**
 * @brief 导出 TEXT 行
 */
int text_write_row(FILE *fp, const char **values, int num_columns,
                   char delimiter, const char *null_string)
{
    if (!fp || !values) {
        return -1;
    }

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            fputc(delimiter, fp);
        }

        if (values[i] == NULL || (null_string && strcmp(values[i], null_string) == 0)) {
            fprintf(fp, "%s", null_string ? null_string : "");
        } else {
            fprintf(fp, "%s", values[i]);
        }
    }
    fputc('\n', fp);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * TEXT 导入
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 解析一行 TEXT 数据
 */
int text_parse_line(const char *line, char delimiter,
                    char **fields, int max_fields, int *num_fields)
{
    if (!line || !fields || !num_fields) {
        return -1;
    }

    *num_fields = 0;
    char *p = (char *)line;
    char *field_start = p;

    while (*p && *num_fields < max_fields) {
        if (*p == delimiter) {
            *p = '\0';
            fields[*num_fields] = field_start;
            (*num_fields)++;
            p++;
            field_start = p;
        } else {
            p++;
        }
    }

    /* 处理最后字段 */
    if (*num_fields < max_fields) {
        /* 去掉末尾换行符 */
        char *end = p;
        while (end > field_start && (end[-1] == '\n' || end[-1] == '\r')) {
            end--;
        }
        *end = '\0';
        fields[*num_fields] = field_start;
        (*num_fields)++;
    }

    return 0;
}