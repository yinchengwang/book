/**
 * @file filter_parser.c
 * @brief 极简 JSON → SQL 过滤器编译器
 *
 * 本实现为教育/演示目的，采用纯字符扫描方式避免引入 cjson。
 * 仅支持一维对象 + 单层操作符，不支持嵌套数组内的对象。
 */
#include "sdk/impl/filter_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 字符串缓冲                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    char*  buf;
    size_t len;
    size_t cap;
} sb_t;

static void sb_init(sb_t* s) {
    s->cap = 256;
    s->buf = (char*)malloc(s->cap);
    s->len = 0;
    if (s->buf) s->buf[0] = '\0';
}

static void sb_free(sb_t* s) {
    free(s->buf);
    s->buf = NULL;
    s->len = s->cap = 0;
}

static int sb_append(sb_t* s, const char* p, size_t n) {
    if (s->len + n + 1 > s->cap) {
        size_t new_cap = s->cap * 2;
        while (new_cap < s->len + n + 1) new_cap *= 2;
        char* nb = (char*)realloc(s->buf, new_cap);
        if (!nb) return -1;
        s->buf = nb;
        s->cap = new_cap;
    }
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = '\0';
    return 0;
}

static int sb_appends(sb_t* s, const char* p) {
    return sb_append(s, p, strlen(p));
}

/* ------------------------------------------------------------------ */
/* 参数收集                                                            */
/* ------------------------------------------------------------------ */

static int params_push_int(mmdb_filter_params_t* p, int64_t v) {
    int* narr = (int*)realloc(p->int_values, sizeof(int) * (p->int_count + 1));
    if (!narr) return -1;
    p->int_values = narr;
    /* 存为 int64_t 但以 int 表示；本实现用于分数等小整数足够 */
    p->int_values[p->int_count] = (int)v;
    p->int_count++;
    return 0;
}

static int params_push_text(mmdb_filter_params_t* p, const char* s) {
    char** narr = (char**)realloc(p->text_values,
                                  sizeof(char*) * (p->text_count + 1));
    if (!narr) return -1;
    p->text_values = narr;
    p->text_values[p->text_count] = mmdb_strdup_internal(s);
    if (!p->text_values[p->text_count]) return -1;
    p->text_count++;
    return 0;
}

/* ------------------------------------------------------------------ */
/* JSON 字符串值扫描（取下一个字符串字面量）                            */
/* ------------------------------------------------------------------ */

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    return p;
}

/* 解析字符串字面量（"…"），写入 out（最大 out_cap-1），返回推进后的指针 */
static const char* parse_string(const char* p, char* out, size_t out_cap) {
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            if (i + 2 >= out_cap) return NULL;
            out[i++] = *(p + 1);
            p += 2;
        } else {
            if (i + 1 >= out_cap) return NULL;
            out[i++] = *p++;
        }
    }
    if (*p != '"') return NULL;
    out[i] = '\0';
    return p + 1;
}

/* 解析裸字面量（true/false/null/数字），写入 out（数字时 out 为字符串形式） */
static const char* parse_literal(const char* p, char* out, size_t out_cap) {
    size_t i = 0;
    if (*p == 't' && strncmp(p, "true", 4) == 0) {
        if (out_cap < 5) return NULL;
        memcpy(out, "true", 5);
        return p + 4;
    }
    if (*p == 'f' && strncmp(p, "false", 5) == 0) {
        if (out_cap < 6) return NULL;
        memcpy(out, "false", 6);
        return p + 5;
    }
    if (*p == 'n' && strncmp(p, "null", 4) == 0) {
        if (out_cap < 5) return NULL;
        memcpy(out, "null", 5);
        return p + 4;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']' && *p != '[' &&
           *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
        if (i + 1 >= out_cap) return NULL;
        out[i++] = *p++;
    }
    out[i] = '\0';
    return p;
}

/* ------------------------------------------------------------------ */
/* 操作符映射                                                          */
/* ------------------------------------------------------------------ */

static const char* op_sql(const char* op) {
    if (strcmp(op, "$eq") == 0)  return "=";
    if (strcmp(op, "$ne") == 0)  return "!=";
    if (strcmp(op, "$gt") == 0)  return ">";
    if (strcmp(op, "$lt") == 0)  return "<";
    if (strcmp(op, "$gte") == 0) return ">=";
    if (strcmp(op, "$lte") == 0) return "<=";
    return NULL;
}

/* 判定字符串字面量能否作为数字 */
static int is_number(const char* s) {
    if (!s || !*s) return 0;
    const char* p = s;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    while (*p) {
        if (*p != '.' && (*p < '0' || *p > '9')) return 0;
        p++;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 主编译函数                                                          */
/* ------------------------------------------------------------------ */

char* mmdb_filter_compile(const char* json, mmdb_filter_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
    if (!json) {
        /* 无 filter：返回空字符串表示无 WHERE */
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    json = skip_ws(json);
    if (*json != '{') {
        /* 空过滤：返回空字符串表示无 WHERE */
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    json++;

    sb_t where;
    sb_init(&where);
    int first = 1;

    while (1) {
        json = skip_ws(json);
        if (*json == '}') {
            json++;
            break;
        }
        char key[128];
        if (*json != '"') {
            sb_free(&where);
            mmdb_filter_params_free(params);
            return NULL;
        }
        json = parse_string(json, key, sizeof(key));
        if (!json) {
            sb_free(&where);
            mmdb_filter_params_free(params);
            return NULL;
        }
        json = skip_ws(json);
        if (*json != ':') {
            sb_free(&where);
            mmdb_filter_params_free(params);
            return NULL;
        }
        json++;
        json = skip_ws(json);

        if (!first) sb_appends(&where, " AND ");
        first = 0;

        if (*json == '{') {
            /* 操作符对象：{"$gt": v} */
            json++;
            char op[16];
            char lit[256];
            json = skip_ws(json);
            if (*json != '"') goto fail;
            json = parse_string(json, op, sizeof(op));
            if (!json) goto fail;
            json = skip_ws(json);
            if (*json != ':') goto fail;
            json++;
            json = skip_ws(json);

            const char* sql_op = op_sql(op);
            int is_set_op = (strcmp(op, "$in") == 0 || strcmp(op, "$nin") == 0);
            if (!sql_op && !is_set_op) goto fail; /* 未知操作符 */

            if (is_set_op) {
                if (*json != '[') goto fail;
                json++;
                sb_appends(&where, key);
                sb_appends(&where, strcmp(op, "$in") == 0 ? " IN (" : " NOT IN (");
                int sub_first = 1;
                while (1) {
                    json = skip_ws(json);
                    if (*json == ']') { json++; break; }
                    if (!sub_first) sb_appends(&where, ",");
                    sub_first = 0;
                    if (*json == '"') {
                        json = parse_string(json, lit, sizeof(lit));
                        if (!json) goto fail;
                        sb_appends(&where, "?");
                        params_push_text(params, lit);
                    } else {
                        json = parse_literal(json, lit, sizeof(lit));
                        if (!json) goto fail;
                        sb_appends(&where, "?");
                        if (is_number(lit)) {
                            params_push_int(params, strtoll(lit, NULL, 10));
                        } else {
                            params_push_text(params, lit);
                        }
                    }
                    json = skip_ws(json);
                    if (*json == ',') json++;
                }
                sb_appends(&where, ")");
            } else {
                /* 标量操作符 */
                if (*json == '"') {
                    json = parse_string(json, lit, sizeof(lit));
                } else {
                    json = parse_literal(json, lit, sizeof(lit));
                }
                if (!json) goto fail;
                sb_appends(&where, key);
                sb_appends(&where, " ");
                sb_appends(&where, sql_op);
                sb_appends(&where, " ?");
                if (is_number(lit) && strcmp(op, "$eq") != 0 &&
                    strcmp(op, "$ne") != 0) {
                    /* 数字操作符：转 int */
                    params_push_int(params, strtoll(lit, NULL, 10));
                } else {
                    params_push_text(params, lit);
                }
            }

            json = skip_ws(json);
            if (*json != '}') goto fail;
            json++;
        } else {
            /* 直接字面量：{key: literal} → key = ? */
            char lit[256];
            if (*json == '"') {
                json = parse_string(json, lit, sizeof(lit));
            } else {
                json = parse_literal(json, lit, sizeof(lit));
            }
            if (!json) goto fail;
            sb_appends(&where, key);
            sb_appends(&where, " = ?");
            if (is_number(lit)) {
                params_push_int(params, strtoll(lit, NULL, 10));
            } else {
                params_push_text(params, lit);
            }
        }

        json = skip_ws(json);
        if (*json == ',') json++;
    }

    return where.buf;

fail:
    sb_free(&where);
    mmdb_filter_params_free(params);
    return NULL;
}

void mmdb_filter_params_free(mmdb_filter_params_t* p) {
    if (!p) return;
    free(p->int_values);
    p->int_values = NULL;
    if (p->text_values) {
        for (size_t i = 0; i < p->text_count; i++) free(p->text_values[i]);
        free(p->text_values);
        p->text_values = NULL;
    }
    p->int_count = p->text_count = 0;
}

int mmdb_filter_bind(sqlite3_stmt* stmt, const mmdb_filter_params_t* p,
                     int start_idx) {
    if (!p) return start_idx;
    int idx = start_idx;
    for (size_t i = 0; i < p->int_count; i++) {
        if (sqlite3_bind_int64(stmt, idx++, p->int_values[i]) != SQLITE_OK)
            return -1;
    }
    for (size_t i = 0; i < p->text_count; i++) {
        if (sqlite3_bind_text(stmt, idx++, p->text_values[i], -1,
                              SQLITE_TRANSIENT) != SQLITE_OK)
            return -1;
    }
    return idx;
}