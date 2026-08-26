/**
 * @file schema.c
 * @brief Schema 验证与 JSON 序列化（轻量级手写 JSON，避免引入 cjson 依赖）
 *
 * JSON 格式：
 * {
 *   "model": "vector",
 *   "vector_dim": 128,
 *   "fields": [
 *     {"name": "label", "type": "text", "nullable": false},
 *     {"name": "score", "type": "float", "nullable": true, "default": "0.0"}
 *   ]
 * }
 */
#include "sdk/impl/schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 模型与类型名称映射                                                  */
/* ------------------------------------------------------------------ */

static const char* model_name(mmdb_model_t m) {
    switch (m) {
        case MMDB_MODEL_VECTOR:     return "vector";
        case MMDB_MODEL_GRAPH:      return "graph";
        case MMDB_MODEL_TIMESERIES: return "timeseries";
        case MMDB_MODEL_TEXT:       return "text";
        default:                    return "unknown";
    }
}

static int model_from_name(const char* s, mmdb_model_t* out) {
    if (!s || !out) return -1;
    if (strcmp(s, "vector") == 0) { *out = MMDB_MODEL_VECTOR; return 0; }
    if (strcmp(s, "graph") == 0) { *out = MMDB_MODEL_GRAPH; return 0; }
    if (strcmp(s, "timeseries") == 0) { *out = MMDB_MODEL_TIMESERIES; return 0; }
    if (strcmp(s, "text") == 0) { *out = MMDB_MODEL_TEXT; return 0; }
    return -1;
}

static const char* type_name(mmdb_data_type_t t) {
    switch (t) {
        case MMDB_TYPE_INT:        return "int";
        case MMDB_TYPE_FLOAT:      return "float";
        case MMDB_TYPE_TEXT:       return "text";
        case MMDB_TYPE_BLOB:       return "blob";
        case MMDB_TYPE_VECTOR:     return "vector";
        case MMDB_TYPE_NODE:       return "node";
        case MMDB_TYPE_EDGE:       return "edge";
        case MMDB_TYPE_DATAPOINT:  return "datapoint";
        default:                   return "unknown";
    }
}

static int type_from_name(const char* s, mmdb_data_type_t* out) {
    if (!s || !out) return -1;
    if (strcmp(s, "int") == 0) { *out = MMDB_TYPE_INT; return 0; }
    if (strcmp(s, "float") == 0) { *out = MMDB_TYPE_FLOAT; return 0; }
    if (strcmp(s, "text") == 0) { *out = MMDB_TYPE_TEXT; return 0; }
    if (strcmp(s, "blob") == 0) { *out = MMDB_TYPE_BLOB; return 0; }
    if (strcmp(s, "vector") == 0) { *out = MMDB_TYPE_VECTOR; return 0; }
    if (strcmp(s, "node") == 0) { *out = MMDB_TYPE_NODE; return 0; }
    if (strcmp(s, "edge") == 0) { *out = MMDB_TYPE_EDGE; return 0; }
    if (strcmp(s, "datapoint") == 0) { *out = MMDB_TYPE_DATAPOINT; return 0; }
    return -1;
}

/* ------------------------------------------------------------------ */
/* 校验规则                                                            */
/* ------------------------------------------------------------------ */

int mmdb_schema_validate(const mmdb_schema_t* schema) {
    if (!schema) return MMDB_ERR_INVALID;

    /* 模型合法性 */
    if (schema->model != MMDB_MODEL_VECTOR &&
        schema->model != MMDB_MODEL_GRAPH &&
        schema->model != MMDB_MODEL_TIMESERIES &&
        schema->model != MMDB_MODEL_TEXT) {
        return MMDB_ERR_INVALID;
    }

    /* 向量模型必须指定维度 */
    if (schema->model == MMDB_MODEL_VECTOR) {
        if (schema->vector_dim == 0 || schema->vector_dim > 65536) {
            return MMDB_ERR_INVALID;
        }
    }

    /* 字段名不可重复，不可空 */
    if (schema->fields && schema->field_count > 0) {
        for (size_t i = 0; i < schema->field_count; i++) {
            const mmdb_field_def_t* f = &schema->fields[i];
            if (!f->name || f->name[0] == '\0') return MMDB_ERR_INVALID;
            for (size_t j = i + 1; j < schema->field_count; j++) {
                if (strcmp(f->name, schema->fields[j].name) == 0) {
                    return MMDB_ERR_ALREADY;
                }
            }
        }
    }

    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* JSON 序列化（手写，避免引入 cjson）                                  */
/* ------------------------------------------------------------------ */

char* mmdb_schema_to_json(const mmdb_schema_t* schema) {
    if (!schema) return NULL;

    /* 预估缓冲大小：256 字节 + 每字段 ~128 字节 */
    size_t cap = 256 + (schema->field_count ? schema->field_count * 128 : 0);
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int n = snprintf(buf, cap,
                     "{\"model\":\"%s\",\"vector_dim\":%zu,\"fields\":[",
                     model_name(schema->model), schema->vector_dim);
    if (n < 0 || (size_t)n >= cap) {
        free(buf);
        return NULL;
    }
    size_t off = (size_t)n;

    for (size_t i = 0; i < schema->field_count; i++) {
        const mmdb_field_def_t* f = &schema->fields[i];
        const char* sep = (i + 1 < schema->field_count) ? "," : "";
        n = snprintf(buf + off, cap - off,
                     "{\"name\":\"%s\",\"type\":\"%s\",\"nullable\":%s,"
                     "\"default\":%s}%s",
                     f->name ? f->name : "",
                     type_name(f->type),
                     f->nullable ? "true" : "false",
                     f->default_value_json ? f->default_value_json : "null",
                     sep);
        if (n < 0 || (size_t)n >= cap - off) {
            free(buf);
            return NULL;
        }
        off += (size_t)n;
    }

    if (off + 3 > cap) {
        free(buf);
        return NULL;
    }
    buf[off++] = ']';
    buf[off++] = '}';
    buf[off] = '\0';
    return buf;
}

/* ------------------------------------------------------------------ */
/* JSON 反序列化（极简：手写键值扫描）                                  */
/* ------------------------------------------------------------------ */

/* 查找 "key":<value>，返回 value 起始指针；写入 value_len */
static const char* find_key(const char* json, const char* key, size_t* value_len) {
    char needle[64];
    int n = snprintf(needle, sizeof(needle), "\"%s\":", key);
    if (n < 0 || (size_t)n >= sizeof(needle)) return NULL;
    const char* p = strstr(json, needle);
    if (!p) return NULL;
    p += n;
    while (*p == ' ') p++;
    const char* start = p;
    const char* end = NULL;
    if (*p == '"') {
        /* 字符串值 */
        start = p + 1;
        end = start;
        while (*end && !(*end == '"' && *(end - 1) != '\\')) end++;
    } else if (*p == 't' || *p == 'f') {
        end = (*p == 't') ? p + 4 : p + 5;
    } else {
        end = p;
        while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\n') end++;
    }
    *value_len = (size_t)(end - start);
    return start;
}

/* 拷贝指定长度的子串为新字符串 */
static char* substr_dup(const char* s, size_t n) {
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

int mmdb_schema_from_json(const char* json, mmdb_schema_t* out) {
    if (!json || !out) return MMDB_ERR_INVALID;
    memset(out, 0, sizeof(*out));

    size_t vlen = 0;
    const char* v;

    v = find_key(json, "model", &vlen);
    if (!v) return MMDB_ERR_CORRUPT;
    char* model_s = substr_dup(v, vlen);
    if (!model_s) return MMDB_ERR_NOMEM;
    if (model_from_name(model_s, &out->model) != 0) {
        free(model_s);
        return MMDB_ERR_INVALID;
    }
    free(model_s);

    v = find_key(json, "vector_dim", &vlen);
    if (v && vlen > 0) {
        char buf[32];
        size_t cp = vlen < sizeof(buf) - 1 ? vlen : sizeof(buf) - 1;
        memcpy(buf, v, cp);
        buf[cp] = '\0';
        out->vector_dim = (size_t)strtoull(buf, NULL, 10);
    }

    /* 字段数组扫描（手写极简） */
    const char* fields = strstr(json, "\"fields\":");
    if (!fields) return MMDB_OK; /* 允许无字段 */
    fields += 9;
    while (*fields == ' ') fields++;
    if (*fields != '[') return MMDB_ERR_CORRUPT;
    fields++;

    /* 第一次扫描计数 */
    size_t count = 0;
    int depth = 0;
    const char* p = fields;
    while (*p && !(depth == 0 && *p == ']')) {
        if (*p == '{') depth++;
        if (*p == '}') depth--;
        if (depth == 0 && *p == ',') count++;
        p++;
    }
    if (*p == ']') count++;  /* 末尾对象也计数 */
    if (count == 0) return MMDB_OK;

    out->fields = (mmdb_field_def_t*)calloc(count, sizeof(mmdb_field_def_t));
    if (!out->fields) return MMDB_ERR_NOMEM;
    out->field_count = count;

    /* 第二次扫描解析每个对象 */
    p = fields;
    size_t idx = 0;
    while (*p && idx < count) {
        while (*p && *p != '{') p++;
        if (!*p) break;
        const char* obj_start = p;
        int d = 0;
        while (*p) {
            if (*p == '{') d++;
            if (*p == '}') { d--; if (d == 0) { p++; break; } }
            p++;
        }
        const char* obj_end = p;
        size_t obj_len = (size_t)(obj_end - obj_start);

        char* obj = substr_dup(obj_start, obj_len);
        if (!obj) {
            /* 失败：释放已分配 */
            for (size_t i = 0; i < idx; i++) {
                free((void*)out->fields[i].name);
                free((void*)out->fields[i].default_value_json);
            }
            free(out->fields);
            out->fields = NULL;
            out->field_count = 0;
            return MMDB_ERR_NOMEM;
        }

        size_t lv = 0;
        const char* lv_p;
        lv_p = find_key(obj, "name", &lv);
        if (lv_p) out->fields[idx].name = substr_dup(lv_p, lv);
        lv_p = find_key(obj, "type", &lv);
        char type_buf[32] = {0};
        if (lv_p) {
            size_t cp = lv < sizeof(type_buf) - 1 ? lv : sizeof(type_buf) - 1;
            memcpy(type_buf, lv_p, cp);
            type_buf[cp] = '\0';
            type_from_name(type_buf, &out->fields[idx].type);
        }
        lv_p = find_key(obj, "nullable", &lv);
        if (lv_p && lv >= 4 && strncmp(lv_p, "true", 4) == 0) {
            out->fields[idx].nullable = 1;
        }
        lv_p = find_key(obj, "default", &lv);
        if (lv_p) {
            /* null 字面量转为 NULL */
            if (lv == 4 && strncmp(lv_p, "null", 4) == 0) {
                out->fields[idx].default_value_json = NULL;
            } else {
                out->fields[idx].default_value_json = substr_dup(lv_p, lv);
            }
        }
        free(obj);
        idx++;
    }

    return MMDB_OK;
}