/**
 * @file copy_csv.c
 * @brief CSV 格式 COPY 实现
 */

#include "db/tools/copy.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ─────────────────────────────────────────────────────────────────
 * CSV 工具函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief CSV 字段是否需要引号
 */
static bool csv_needs_quote(const char *value, char delimiter, char quote)
{
    for (const char *p = value; *p; p++) {
        if (*p == quote || *p == '\n' || *p == '\r' || *p == delimiter) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 转义 CSV 字段
 */
static void csv_escape_field(FILE *fp, const char *value,
                              char quote, char escape, char delimiter)
{
    bool need_quote = csv_needs_quote(value, delimiter, quote);

    if (need_quote) {
        fputc(quote, fp);
    }

    for (const char *p = value; *p; p++) {
        if (*p == quote) {
            fputc(escape, fp);
            fputc(quote, fp);
        } else {
            fputc(*p, fp);
        }
    }

    if (need_quote) {
        fputc(quote, fp);
    }
}

/* ─────────────────────────────────────────────────────────────────
 * CSV 导出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 导出 CSV 表头
 */
int csv_write_header(FILE *fp, const char **column_names, int num_columns,
                     char delimiter)
{
    if (!fp || !column_names) {
        return -1;
    }

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            fputc(delimiter, fp);
        }
        fprintf(fp, "%s", column_names[i]);
    }
    fputc('\n', fp);
    return 0;
}

/**
 * @brief 导出 CSV 行
 */
int csv_write_row(FILE *fp, const char **values, int num_columns,
                  char delimiter, char quote, char escape,
                  const char *null_string)
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
            csv_escape_field(fp, values[i], quote, escape, delimiter);
        }
    }
    fputc('\n', fp);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * CSV 导入
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief CSV 字段解析状态
 */
typedef enum {
    CSV_FIELD_START,
    CSV_FIELD_PLAIN,
    CSV_FIELD_QUOTED,
    CSV_FIELD_QUOTED_END
} CsvFieldState;

/**
 * @brief 解析一行 CSV 数据
 * @param line 输入行
 * @param delimiter 分隔符
 * @param quote 引号字符
 * @param escape 转义字符
 * @param[out] fields 字段数组（需预先分配）
 * @param max_fields 字段数组大小
 * @param[out] num_fields 解析出的字段数
 * @return 0 成功，非 0 失败
 */
int csv_parse_line(const char *line, char delimiter, char quote, char escape,
                   char **fields, int max_fields, int *num_fields)
{
    if (!line || !fields || !num_fields) {
        return -1;
    }

    *num_fields = 0;
    CsvFieldState state = CSV_FIELD_START;
    char *field_start = (char *)line;
    char *p = (char *)line;
    char *write_ptr = (char *)line;  /* 就地处理转义 */

    while (*p && *p != '\n' && *p != '\r' && *num_fields < max_fields) {
        switch (state) {
            case CSV_FIELD_START:
                if (*p == quote) {
                    state = CSV_FIELD_QUOTED;
                    field_start = write_ptr;
                    p++;
                } else if (*p == delimiter) {
                    fields[*num_fields] = NULL;  /* 空字段 */
                    (*num_fields)++;
                    p++;
                    write_ptr = p;
                } else {
                    state = CSV_FIELD_PLAIN;
                    field_start = write_ptr;
                }
                break;

            case CSV_FIELD_PLAIN:
                if (*p == delimiter) {
                    *write_ptr = '\0';
                    fields[*num_fields] = field_start;
                    (*num_fields)++;
                    p++;
                    write_ptr = p;
                    state = CSV_FIELD_START;
                } else {
                    *write_ptr++ = *p++;
                }
                break;

            case CSV_FIELD_QUOTED:
                if (*p == escape && *(p + 1) == quote) {
                    *write_ptr++ = quote;
                    p += 2;
                } else if (*p == quote) {
                    state = CSV_FIELD_QUOTED_END;
                    p++;
                } else {
                    *write_ptr++ = *p++;
                }
                break;

            case CSV_FIELD_QUOTED_END:
                if (*p == delimiter) {
                    *write_ptr = '\0';
                    fields[*num_fields] = field_start;
                    (*num_fields)++;
                    p++;
                    write_ptr = p;
                    state = CSV_FIELD_START;
                } else {
                    /* 忽略引号后的字符 */
                    p++;
                }
                break;
        }
    }

    /* 处理最后字段 */
    if (state != CSV_FIELD_START && *num_fields < max_fields) {
        *write_ptr = '\0';
        fields[*num_fields] = field_start;
        (*num_fields)++;
    } else if (state == CSV_FIELD_START && *num_fields > 0) {
        /* 处理结尾的分隔符，添加空字段 */
        fields[*num_fields] = NULL;
        (*num_fields)++;
    }

    return 0;
}