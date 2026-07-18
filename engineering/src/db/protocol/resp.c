/**
 * @file resp.c
 * @brief RESP（Redis 序列化协议）解析与构建实现
 *
 * 支持 RESP2 和 RESP3 混合模式。实现流式解析和构建接口。
 */
#include "db/protocol/resp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/** 跳过 CRLF */
static inline int skip_crlf(const char *buf, int buf_len, int pos) {
    if (pos + 1 >= buf_len) return -1;
    if (buf[pos] == '\r' && buf[pos + 1] == '\n') return pos + 2;
    return -1;
}

/** 读取一行（不含 CRLF） */
static int read_line(resp_parser_t *parser, const char **out_line, int *out_len) {
    int i;
    for (i = parser->pos; i < parser->buf_len; i++) {
        if (parser->buf[i] == '\r' && i + 1 < parser->buf_len && parser->buf[i + 1] == '\n') {
            *out_line = parser->buf + parser->pos;
            *out_len = i - parser->pos;
            parser->pos = i + 2;
            return 0;
        }
    }
    return -1;  /* 未找到 CRLF */
}

/** 读取整数行 */
static int read_integer_line(resp_parser_t *parser, int64_t *out) {
    const char *line;
    int len;
    if (read_line(parser, &line, &len) != 0) return -1;

    char buf[32];
    int copy_len = len < 31 ? len : 31;
    memcpy(buf, line, copy_len);
    buf[copy_len] = '\0';

    char *endptr = NULL;
    *out = strtoll(buf, &endptr, 10);
    if (endptr == buf) return -1;
    return 0;
}

/** 读取指定长度的 bulk 数据 */
static int read_bulk_data(resp_parser_t *parser, int len, const char **out_data) {
    if (len < 0) {
        *out_data = NULL;
        return 0;
    }
    if (parser->pos + len + 2 > parser->buf_len) return -1;
    *out_data = parser->buf + parser->pos;
    parser->pos += len;
    if (skip_crlf(parser->buf, parser->buf_len, parser->pos) < 0) return -1;
    parser->pos += 2;
    return 0;
}

/* ========================================================================
 * RESP 解析器
 * ======================================================================== */

void resp_parser_init(resp_parser_t *parser, const char *buf, int buf_len) {
    parser->buf = buf;
    parser->buf_len = buf_len;
    parser->pos = 0;
}

static int parse_value_internal(resp_parser_t *parser, resp_value_t *out_value);

static int parse_array_items(resp_parser_t *parser, resp_value_t *out_value, int count) {
    if (count <= 0) {
        out_value->data.collection.elements = NULL;
        out_value->data.collection.count = 0;
        return 0;
    }

    out_value->data.collection.elements = (resp_value_t *)calloc((size_t)count, sizeof(resp_value_t));
    if (!out_value->data.collection.elements) return -1;
    out_value->data.collection.count = count;

    for (int i = 0; i < count; i++) {
        if (parse_value_internal(parser, &out_value->data.collection.elements[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_value_internal(resp_parser_t *parser, resp_value_t *out_value) {
    if (parser->pos >= parser->buf_len) return -1;

    memset(out_value, 0, sizeof(resp_value_t));
    char type_byte = parser->buf[parser->pos++];

    switch (type_byte) {
        case '+': {  /* Simple String */
            const char *line;
            int len;
            if (read_line(parser, &line, &len) != 0) return -1;
            out_value->type = RESP_STRING;
            out_value->data.string.str = line;
            out_value->data.string.len = len;
            return 0;
        }
        case '-': {  /* Error */
            const char *line;
            int len;
            if (read_line(parser, &line, &len) != 0) return -1;
            out_value->type = RESP_ERROR;
            out_value->data.string.str = line;
            out_value->data.string.len = len;
            return 0;
        }
        case ':': {  /* Integer */
            int64_t val;
            if (read_integer_line(parser, &val) != 0) return -1;
            out_value->type = RESP_INTEGER;
            out_value->data.integer = val;
            return 0;
        }
        case '$': {  /* Bulk String */
            int64_t len;
            if (read_integer_line(parser, &len) != 0) return -1;
            out_value->type = RESP_BULK_STRING;
            out_value->data.bulk_string.len = (int)len;
            if (len < 0) {
                out_value->type = RESP_NULL;
                out_value->data.bulk_string.str = NULL;
            } else {
                if (read_bulk_data(parser, (int)len, &out_value->data.bulk_string.str) != 0)
                    return -1;
            }
            return 0;
        }
        case '*': {  /* Array */
            int64_t count;
            if (read_integer_line(parser, &count) != 0) return -1;
            out_value->type = RESP_ARRAY;
            if (count < 0) {
                out_value->type = RESP_NULL;
                out_value->data.collection.count = 0;
                out_value->data.collection.elements = NULL;
                return 0;
            }
            return parse_array_items(parser, out_value, (int)count);
        }
        case '#': {  /* Boolean (RESP3) */
            const char *line;
            int len;
            if (read_line(parser, &line, &len) != 0) return -1;
            out_value->type = RESP_BOOLEAN;
            out_value->data.boolean = (len > 0 && line[0] == 't');
            return 0;
        }
        case ',': {  /* Double (RESP3) */
            const char *line;
            int len;
            if (read_line(parser, &line, &len) != 0) return -1;
            out_value->type = RESP_DOUBLE;
            char buf[64];
            int copy_len = len < 63 ? len : 63;
            memcpy(buf, line, copy_len);
            buf[copy_len] = '\0';
            out_value->data.dval = strtod(buf, NULL);
            return 0;
        }
        case '%': {  /* Map (RESP3) */
            int64_t count;
            if (read_integer_line(parser, &count) != 0) return -1;
            out_value->type = RESP_MAP;
            if (count < 0) return -1;
            return parse_array_items(parser, out_value, (int)(count * 2));
        }
        case '~': {  /* Set (RESP3) */
            int64_t count;
            if (read_integer_line(parser, &count) != 0) return -1;
            out_value->type = RESP_SET;
            if (count < 0) return -1;
            return parse_array_items(parser, out_value, (int)count);
        }
        case '>': {  /* Push (RESP3) */
            int64_t count;
            if (read_integer_line(parser, &count) != 0) return -1;
            out_value->type = RESP_PUSH;
            return parse_array_items(parser, out_value, (int)count);
        }
        case '=': {  /* Verbatim String (RESP3) */
            int64_t len;
            if (read_integer_line(parser, &len) != 0) return -1;
            out_value->type = RESP_VERBATIM;
            if (len < 0) return -1;
            if (read_bulk_data(parser, (int)len, &out_value->data.bulk_string.str) != 0)
                return -1;
            out_value->data.bulk_string.len = (int)len;
            return 0;
        }
        case '(': {  /* Big Number (RESP3) */
            const char *line;
            int len;
            if (read_line(parser, &line, &len) != 0) return -1;
            out_value->type = RESP_BIG_NUMBER;
            out_value->data.string.str = line;
            out_value->data.string.len = len;
            return 0;
        }
        case '_': {  /* Nil (RESP3) */
            if (skip_crlf(parser->buf, parser->buf_len, parser->pos) < 0) return -1;
            parser->pos += 2;
            out_value->type = RESP_NIL;
            return 0;
        }
        default:
            return -1;
    }
}

int resp_parse_next(resp_parser_t *parser, resp_value_t *out_value) {
    return parse_value_internal(parser, out_value);
}

void resp_free_value(resp_value_t *value) {
    if (!value) return;

    if (value->type == RESP_ARRAY || value->type == RESP_MAP ||
        value->type == RESP_SET || value->type == RESP_PUSH) {
        if (value->data.collection.elements) {
            for (int i = 0; i < value->data.collection.count; i++) {
                resp_free_value(&value->data.collection.elements[i]);
            }
            free(value->data.collection.elements);
            value->data.collection.elements = NULL;
        }
    }
    memset(value, 0, sizeof(resp_value_t));
}

/* ========================================================================
 * RESP 构建器
 * ======================================================================== */

static int write_raw(char *buf, int buf_size, int pos, const char *data, int len) {
    if (pos + len > buf_size) return -1;
    memcpy(buf + pos, data, (size_t)len);
    pos += len;
    buf[pos] = '\0';
    return pos;
}

int resp_build_string(char *buf, int buf_size, const char *str) {
    int pos = 0;
    buf[pos++] = '+';
    if (str) {
        int slen = (int)strlen(str);
        if (pos + slen + 2 > buf_size) return -1;
        memcpy(buf + pos, str, (size_t)slen);
        pos += slen;
    }
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_error(char *buf, int buf_size, const char *msg) {
    int pos = 0;
    buf[pos++] = '-';
    if (msg) {
        int slen = (int)strlen(msg);
        if (pos + slen + 2 > buf_size) return -1;
        memcpy(buf + pos, msg, (size_t)slen);
        pos += slen;
    }
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_integer(char *buf, int buf_size, int64_t val) {
    int pos = 0;
    buf[pos++] = ':';
    char num[32];
    int nlen = snprintf(num, sizeof(num), "%lld", (long long)val);
    if (pos + nlen + 2 > buf_size) return -1;
    memcpy(buf + pos, num, (size_t)nlen);
    pos += nlen;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_bulk_string(char *buf, int buf_size, const char *str, int len) {
    int pos = 0;
    buf[pos++] = '$';
    if (str == NULL || len < 0) {
        buf[pos++] = '-';
        buf[pos++] = '1';
        buf[pos++] = '\r';
        buf[pos++] = '\n';
        buf[pos] = '\0';
        return pos;
    }
    char num[32];
    int nlen = snprintf(num, sizeof(num), "%d", len);
    if (pos + nlen + 2 > buf_size) return -1;
    memcpy(buf + pos, num, (size_t)nlen);
    pos += nlen;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    if (pos + len + 2 > buf_size) return -1;
    memcpy(buf + pos, str, (size_t)len);
    pos += len;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_array_header(char *buf, int buf_size, int count) {
    int pos = 0;
    buf[pos++] = '*';
    char num[32];
    int nlen = snprintf(num, sizeof(num), "%d", count);
    if (pos + nlen + 2 > buf_size) return -1;
    memcpy(buf + pos, num, (size_t)nlen);
    pos += nlen;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_boolean(char *buf, int buf_size, bool val) {
    int pos = 0;
    buf[pos++] = '#';
    buf[pos++] = val ? 't' : 'f';
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_double(char *buf, int buf_size, double val) {
    int pos = 0;
    buf[pos++] = ',';
    char num[64];
    int nlen = snprintf(num, sizeof(num), "%.17g", val);
    if (pos + nlen + 2 > buf_size) return -1;
    memcpy(buf + pos, num, (size_t)nlen);
    pos += nlen;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_map_header(char *buf, int buf_size, int count) {
    int pos = 0;
    buf[pos++] = '%';
    char num[32];
    int nlen = snprintf(num, sizeof(num), "%d", count);
    if (pos + nlen + 2 > buf_size) return -1;
    memcpy(buf + pos, num, (size_t)nlen);
    pos += nlen;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return pos;
}

int resp_build_ok(char *buf, int buf_size) {
    return resp_build_string(buf, buf_size, "OK");
}

int resp_build_pong(char *buf, int buf_size) {
    return resp_build_string(buf, buf_size, "PONG");
}

/* ========================================================================
 * RESP 命令解析
 * ======================================================================== */

int resp_parse_command(resp_parser_t *parser, resp_command_t *out_cmd) {
    memset(out_cmd, 0, sizeof(resp_command_t));

    resp_value_t value;
    memset(&value, 0, sizeof(value));

    if (resp_parse_next(parser, &value) != 0) return -1;

    /* 命令必须是 RESP Array */
    if (value.type != RESP_ARRAY) {
        resp_free_value(&value);
        return -1;
    }

    int count = value.data.collection.count;
    out_cmd->argc = count;
    out_cmd->argv = (char **)calloc((size_t)(count + 1), sizeof(char *));
    if (!out_cmd->argv) {
        resp_free_value(&value);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        resp_value_t *elem = &value.data.collection.elements[i];
        if (elem->type == RESP_BULK_STRING) {
            int len = elem->data.bulk_string.len;
            const char *data = elem->data.bulk_string.str;
            out_cmd->argv[i] = (char *)malloc((size_t)(len + 1));
            if (out_cmd->argv[i]) {
                memcpy(out_cmd->argv[i], data, (size_t)len);
                out_cmd->argv[i][len] = '\0';
            }
        } else if (elem->type == RESP_STRING) {
            int len = elem->data.string.len;
            const char *data = elem->data.string.str;
            out_cmd->argv[i] = (char *)malloc((size_t)(len + 1));
            if (out_cmd->argv[i]) {
                memcpy(out_cmd->argv[i], data, (size_t)len);
                out_cmd->argv[i][len] = '\0';
            }
        }
    }

    /* 释放原始 RESP 值（argv 已复制出来） */
    value.data.collection.elements = NULL; /* 防止 resp_free_value 释放我们已复制的内存 */
    resp_free_value(&value);

    return 0;
}

void resp_free_command(resp_command_t *cmd) {
    if (!cmd) return;
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
        cmd->argv = NULL;
    }
    cmd->argc = 0;
}