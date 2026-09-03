/**
 * @file copy_options.c
 * @brief COPY 选项解析实现
 *
 * 解析 COPY WITH (...) 语法中的选项字符串。
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "db/tools/copy.h"

/* ─────────────────────────────────────────────────────────────────
 * 内部辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 跳过空白字符
 */
static const char *skip_whitespace(const char *str)
{
    while (str && *str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

/**
 * @brief 解析带引号的字符
 * @return 解析得到的字符，-1 表示失败
 */
static int parse_quoted_char(const char **ptr)
{
    const char *p = *ptr;

    p = skip_whitespace(p);
    if (!p || *p == '\0') {
        return -1;
    }

    char quote = *p;
    if (quote != '\'' && quote != '"') {
        /* 不是带引号的，可能是单字符 */
        *ptr = p + 1;
        return (int)(unsigned char)*p;
    }

    p++;
    if (*p == '\0') {
        return -1;
    }

    /* 处理转义字符 */
    int ch;
    if (*p == '\\') {
        p++;
        switch (*p) {
            case 'n':  ch = '\n'; break;
            case 't':  ch = '\t'; break;
            case 'r':  ch = '\r'; break;
            case '\\': ch = '\\'; break;
            case '\'': ch = '\''; break;
            case '"':  ch = '"';  break;
            default:   ch = (int)(unsigned char)*p; break;
        }
        p++;
    } else {
        ch = (int)(unsigned char)*p;
        p++;
    }

    /* 期望匹配的后引号 */
    if (*p != quote) {
        return -1;
    }
    p++;

    *ptr = p;
    return ch;
}

/**
 * @brief 解析标识符（不区分大小写比较）
 */
static bool parse_keyword(const char **ptr, const char *keyword)
{
    const char *p = skip_whitespace(*ptr);
    size_t klen = strlen(keyword);

    if (strncmp(p, keyword, klen) == 0) {
        char next = p[klen];
        if (next == '\0' || isspace((unsigned char)next) ||
            next == '(' || next == ')' || next == ',') {
            *ptr = p + klen;
            return true;
        }
    }
    return false;
}

/**
 * @brief 解析带引号的字符串
 */
static char *parse_quoted_string(const char **ptr)
{
    const char *p = skip_whitespace(*ptr);
    if (!p || (*p != '\'' && *p != '"')) {
        return NULL;
    }

    char quote = *p;
    p++;

    /* 计算字符串长度 */
    size_t len = 0;
    const char *start = p;
    while (*p && *p != quote) {
        /* 处理转义字符 */
        if (*p == '\\' && p[1]) {
            p++;
        }
        p++;
        len++;
    }

    if (*p != quote) {
        return NULL;  /* 未找到结束引号 */
    }

    /* 分配内存并复制 */
    char *result = (char *)malloc(len + 1);
    if (!result) {
        return NULL;
    }

    p = start;
    char *dst = result;
    while (len > 0) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case 'n':  *dst++ = '\n'; break;
                case 't':  *dst++ = '\t'; break;
                case 'r':  *dst++ = '\r'; break;
                case '\\': *dst++ = '\\'; break;
                case '\'': *dst++ = '\''; break;
                case '"':  *dst++ = '"';  break;
                default:   *dst++ = *p;  break;
            }
        } else {
            *dst++ = *p;
        }
        p++;
        len--;
    }
    *dst = '\0';

    *ptr = p + 1;  /* 跳过结束引号 */
    return result;
}

/**
 * @brief 解析布尔值
 */
static bool parse_boolean(const char **ptr)
{
    const char *p = skip_whitespace(*ptr);

    if (parse_keyword(&p, "true") || parse_keyword(&p, "on") || parse_keyword(&p, "yes")) {
        *ptr = p;
        return true;
    }
    if (parse_keyword(&p, "false") || parse_keyword(&p, "off") || parse_keyword(&p, "no")) {
        *ptr = p;
        return false;
    }

    /* 尝试解析数字 */
    if (*p == '1') {
        *ptr = p + 1;
        return true;
    }
    if (*p == '0') {
        *ptr = p + 1;
        return false;
    }

    return true;  /* 默认值 */
}

/* ─────────────────────────────────────────────────────────────────
 * 公共 API 实现
 * ───────────────────────────────────────────────────────────────── */

int copy_parse_options(const char *options_str, CopyOptions *opts)
{
    if (!opts) {
        return -1;
    }

    /* 如果 options_str 为空或 NULL，使用默认选项 */
    if (!options_str || *options_str == '\0') {
        *opts = copy_default_options();
        return 0;
    }

    /* 从默认选项开始 */
    *opts = copy_default_options();

    const char *p = options_str;

    while (*p) {
        p = skip_whitespace(p);
        if (*p == '\0' || *p == ')') {
            break;
        }

        /* 解析 FORMAT */
        if (parse_keyword(&p, "FORMAT") || parse_keyword(&p, "FORMAT")) {
            p = skip_whitespace(p);
            if (parse_keyword(&p, "csv")) {
                opts->format = COPY_FORMAT_CSV;
            } else if (parse_keyword(&p, "text")) {
                opts->format = COPY_FORMAT_TEXT;
            } else if (parse_keyword(&p, "json")) {
                opts->format = COPY_FORMAT_JSON;
            } else if (parse_keyword(&p, "binary")) {
                opts->format = COPY_FORMAT_BINARY;
            } else if (parse_keyword(&p, "sql")) {
                opts->format = COPY_FORMAT_SQL;
            }
            continue;
        }

        /* 解析 DELIMITER */
        if (parse_keyword(&p, "DELIMITER")) {
            int ch = parse_quoted_char(&p);
            if (ch >= 0) {
                opts->delimiter = (char)ch;
            }
            continue;
        }

        /* 解析 NULL */
        if (parse_keyword(&p, "NULL")) {
            char *null_str = parse_quoted_string(&p);
            if (null_str) {
                opts->null_string = null_str;
            }
            continue;
        }

        /* 解析 HEADER */
        if (parse_keyword(&p, "HEADER")) {
            opts->header = parse_boolean(&p);
            continue;
        }

        /* 解析 QUOTE */
        if (parse_keyword(&p, "QUOTE")) {
            int ch = parse_quoted_char(&p);
            if (ch >= 0) {
                opts->quote_char = (char)ch;
            }
            continue;
        }

        /* 解析 ESCAPE */
        if (parse_keyword(&p, "ESCAPE")) {
            int ch = parse_quoted_char(&p);
            if (ch >= 0) {
                opts->escape_char = (char)ch;
            }
            continue;
        }

        /* 未知选项，跳过该标识符 */
        while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != ',') {
            p++;
        }
    }

    return 0;
}
