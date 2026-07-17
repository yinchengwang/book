/**
 * @file copy_json.c
 * @brief JSON Lines 格式 COPY 实现
 *
 * JSON Lines 格式：每行一个 JSON 对象
 * 示例：{"id":1,"name":"Alice","age":30}
 */

#include "db/tools/copy.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ─────────────────────────────────────────────────────────────────
 * JSON 转义
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 转义 JSON 字符串并输出到文件
 */
static void json_escape_string(FILE *fp, const char *str)
{
    fputc('"', fp);
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  fprintf(fp, "\\\""); break;
            case '\\': fprintf(fp, "\\\\"); break;
            case '\n': fprintf(fp, "\\n"); break;
            case '\r': fprintf(fp, "\\r"); break;
            case '\t': fprintf(fp, "\\t"); break;
            default:
                if ((unsigned char)*p < 0x20) {
                    fprintf(fp, "\\u%04x", (unsigned char)*p);
                } else {
                    fputc(*p, fp);
                }
                break;
        }
    }
    fputc('"', fp);
}

/* ─────────────────────────────────────────────────────────────────
 * JSON 导出
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 导出 JSON 行
 * @param fp 输出文件
 * @param column_names 列名数组
 * @param values 值数组
 * @param num_columns 列数
 * @param null_string NULL 表示字符串
 * @return 0 成功，非 0 失败
 */
int json_write_row(FILE *fp, const char **column_names, const char **values,
                   int num_columns, const char *null_string)
{
    if (!fp || !column_names || !values) {
        return -1;
    }

    fputc('{', fp);

    for (int i = 0; i < num_columns; i++) {
        if (i > 0) {
            fputc(',', fp);
        }

        /* 输出列名 */
        json_escape_string(fp, column_names[i]);
        fputc(':', fp);

        /* 输出值 */
        if (values[i] == NULL || (null_string && strcmp(values[i], null_string) == 0)) {
            fprintf(fp, "null");
        } else {
            json_escape_string(fp, values[i]);
        }
    }

    fputc('}', fp);
    fputc('\n', fp);
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * JSON 导入（解析）
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 跳过空白字符
 */
static const char *json_skip_ws(const char *p)
{
    while (*p && (unsigned char)*p <= 0x20) {
        p++;
    }
    return p;
}

/**
 * @brief 解析 JSON 字符串值
 * @param[in,out] p 输入指针（会被修改）
 * @param[out] out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return 解析后的位置，失败返回 NULL
 */
static const char *json_parse_string_value(const char *p, char *out, size_t out_size)
{
    if (*p != '"') {
        return NULL;
    }
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_size) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"':  out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                case '/':  out[i++] = '/'; break;
                case 'n':  out[i++] = '\n'; break;
                case 'r':  out[i++] = '\r'; break;
                case 't':  out[i++] = '\t'; break;
                case 'u': {
                    /* 简单 unicode 转义：仅支持 \uXXXX 转成 UTF-8 */
                    unsigned int code = 0;
                    for (int j = 0; j < 4; j++) {
                        p++;
                        if (*p >= '0' && *p <= '9')       code = (code << 4) + (*p - '0');
                        else if (*p >= 'a' && *p <= 'f')  code = (code << 4) + (*p - 'a' + 10);
                        else if (*p >= 'A' && *p <= 'F')  code = (code << 4) + (*p - 'A' + 10);
                        else return NULL;
                    }
                    /* 将 Unicode 码点编码为 UTF-8 */
                    if (code < 0x80) {
                        out[i++] = (char)code;
                    } else if (code < 0x800) {
                        out[i++] = (char)(0xC0 | (code >> 6));
                        out[i++] = (char)(0x80 | (code & 0x3F));
                    } else {
                        out[i++] = (char)(0xE0 | (code >> 12));
                        out[i++] = (char)(0x80 | ((code >> 6) & 0x3F));
                        out[i++] = (char)(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    out[i++] = *p;
                    break;
            }
            p++;
        } else {
            out[i++] = *p++;
        }
    }

    if (*p != '"') {
        return NULL;
    }
    out[i] = '\0';
    return p + 1;
}

/**
 * @brief 解析 JSON 行
 * @param line 输入行
 * @param keys 键数组（输出）
 * @param values 值数组（输出）
 * @param max_pairs 最大键值对数
 * @param[out] num_pairs 解析出的键值对数
 * @return 0 成功，非 0 失败
 */
int json_parse_line(const char *line, char **keys, char **values,
                    int max_pairs, int *num_pairs)
{
    if (!line || !keys || !values || !num_pairs) {
        return -1;
    }

    *num_pairs = 0;
    const char *p = json_skip_ws(line);

    /* 必须是对象起始 */
    if (*p != '{') {
        return -1;
    }
    p++;

    /* 空对象 */
    p = json_skip_ws(p);
    if (*p == '}') {
        return 0;
    }

    while (*p && *num_pairs < max_pairs) {
        p = json_skip_ws(p);

        /* 解析 key */
        if (*p != '"') {
            return -1;
        }
        /* 就地解析 key，写入 keys 数组指向的缓冲区 */
        p = json_parse_string_value(p, keys[*num_pairs], 256);
        if (!p) {
            return -1;
        }

        p = json_skip_ws(p);
        if (*p != ':') {
            return -1;
        }
        p++;

        p = json_skip_ws(p);

        /* 解析 value */
        if (*p == '"') {
            p = json_parse_string_value(p, values[*num_pairs], 256);
            if (!p) {
                return -1;
            }
        } else if (strncmp(p, "null", 4) == 0) {
            values[*num_pairs][0] = '\0';
            p += 4;
        } else if (strncmp(p, "true", 4) == 0) {
            strcpy(values[*num_pairs], "true");
            p += 4;
        } else if (strncmp(p, "false", 5) == 0) {
            strcpy(values[*num_pairs], "false");
            p += 5;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            /* 数字值：拷贝直到非数字/小数点/指数符号 */
            size_t i = 0;
            while (*p && i < 255) {
                if (*p == '-' || *p == '+' || *p == '.' ||
                    *p == 'e' || *p == 'E' ||
                    (*p >= '0' && *p <= '9')) {
                    values[*num_pairs][i++] = *p++;
                } else {
                    break;
                }
            }
            values[*num_pairs][i] = '\0';
        } else {
            return -1;
        }

        (*num_pairs)++;

        p = json_skip_ws(p);
        if (*p == ',') {
            p++;
        } else if (*p == '}') {
            p++;
            return 0;
        } else {
            return -1;
        }
    }

    return 0;
}