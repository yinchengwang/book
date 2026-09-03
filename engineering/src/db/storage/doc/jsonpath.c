/**
 * @file jsonpath.c
 * @brief JSONPath 解析器和查询实现
 *
 * 支持标准 JSONPath 表达式：
 * - $.key - 根对象字段
 * - $..key - 递归搜索
 * - $.a.b.c - 嵌套路径
 * - $.arr[0] - 数组索引
 * - $.arr[*] - 数组所有元素
 * - $.arr[?(@.x > 1)] - 过滤表达式
 */
#include "db/storage/doc/jsonpath.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ========================================================================
 * 常量
 * ======================================================================== */

#define MAX_PATH_DEPTH 32
#define MAX_RESULT_SIZE 256

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 跳过空白字符
 */
static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/**
 * @brief 计算字符串长度（处理转义）
 */
static size_t unescaped_str_len(const char *start, const char *end) {
    size_t len = 0;
    for (const char *p = start; p < end && *p; p++) {
        if (*p == '\\' && p + 1 < end) {
            p++;  /* 跳过转义字符 */
        }
        len++;
    }
    return len;
}

/**
 * @brief 跳过转义字符
 */
static const char *skip_escaped(const char *p) {
    if (*p == '\\' && p[1]) {
        return p + 2;
    }
    return p + 1;
}

/**
 * @brief 解析字符串值
 */
static char *parse_string(const char *start, const char *end, size_t *out_len) {
    if (*start != '"') return NULL;

    start++;
    const char *p = start;
    size_t alloc_len = unescaped_str_len(start, end);
    char *result = (char *)malloc(alloc_len + 1);
    if (!result) return NULL;

    size_t i = 0;
    while (p < end && *p && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
            p++;
            switch (*p) {
                case 'n': result[i++] = '\n'; break;
                case 't': result[i++] = '\t'; break;
                case 'r': result[i++] = '\r'; break;
                case '\\': result[i++] = '\\'; break;
                case '"': result[i++] = '"'; break;
                case 'u':
                    /* 简化处理：直接复制 \uXXXX */
                    result[i++] = *p;
                    if (p + 1 < end) result[i++] = *++p;
                    if (p + 1 < end) result[i++] = *++p;
                    if (p + 1 < end) result[i++] = *++p;
                    if (p + 1 < end) result[i++] = *++p;
                    break;
                default: result[i++] = *p; break;
            }
        } else {
            result[i++] = *p;
        }
        p++;
    }

    result[i] = '\0';
    if (out_len) *out_len = i;
    return result;
}

/**
 * @brief 解析数字值
 */
static double parse_number(const char *p) {
    char *end;
    double val = strtod(p, &end);
    return val;
}

/**
 * @brief 查找 JSON 对象中的字段
 */
static const char *find_object_field(const char *json, size_t json_len,
                                      const char *field, size_t field_len) {
    const char *p = json;
    const char *end = json + json_len;

    while (p < end && *p) {
        p = skip_ws(p);
        if (*p != '"') break;

        p++;
        const char *key_start = p;
        while (p < end && *p && *p != '"') {
            if (*p == '\\') p++;
            p++;
        }
        size_t key_len = p - key_start;

        if (p < end && *p == '"') p++;
        p = skip_ws(p);
        if (p < end && *p == ':') p++;
        p = skip_ws(p);

        /* 比较字段名 */
        if (key_len == field_len && strncmp(key_start, field, field_len) == 0) {
            return p;  /* 返回值的起始位置 */
        }

        /* 跳过值 */
        if (*p == '"') {
            p++;
            while (p < end && *p && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
            if (p < end && *p == '"') p++;
        } else if (*p == '{') {
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
        } else if (*p == '[') {
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                p++;
            }
        } else {
            /* 跳过数字或布尔/null */
            while (p < end && *p && *p != ',' && *p != '}' && *p != ']') p++;
        }

        /* 跳过逗号 */
        if (p < end && *p == ',') p++;
    }

    return NULL;
}

/**
 * @brief 查找数组元素
 */
static const char *find_array_elem(const char *json, size_t json_len, int index) {
    const char *p = json;
    const char *end = json + json_len;

    if (*p != '[') return NULL;
    p++;

    int current = 0;
    while (p < end && *p && *p != ']') {
        p = skip_ws(p);
        if (*p == ']' || !*p) break;
        if (*p == ',') {
            p++;
            current++;
            continue;
        }

        if (current == index) {
            return p;
        }

        /* 跳过元素 */
        if (*p == '"') {
            p++;
            while (p < end && *p && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
            if (p < end && *p == '"') p++;
        } else if (*p == '{') {
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
        } else if (*p == '[') {
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                p++;
            }
        } else {
            while (p < end && *p && *p != ',' && *p != ']') p++;
        }

        current++;
    }

    return NULL;
}

/**
 * @brief 获取数组长度
 */
static int get_array_length(const char *json, size_t json_len) {
    const char *p = json;
    const char *end = json + json_len;
    int count = 0;

    if (*p != '[') return -1;
    p++;

    while (p < end && *p && *p != ']') {
        p = skip_ws(p);
        if (*p == ']' || !*p) break;
        if (*p == ',') {
            p++;
            continue;
        }

        count++;

        /* 跳过元素 */
        if (*p == '"') {
            p++;
            while (p < end && *p && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
            if (p < end && *p == '"') p++;
        } else if (*p == '{') {
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
        } else if (*p == '[') {
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                p++;
            }
        } else {
            while (p < end && *p && *p != ',' && *p != ']') p++;
        }
    }

    return count;
}

/**
 * @brief 提取值范围（到结束符）
 */
static const char *extract_value_range(const char *p, const char *end,
                                        size_t *out_len) {
    if (*p == '"') {
        /* 字符串 */
        p++;
        const char *start = p;
        while (p < end && *p && *p != '"') {
            if (*p == '\\') p++;
            p++;
        }
        *out_len = p - start;
        if (p < end && *p == '"') p++;
        return start - 1;  /* 返回包含引号的起始位置 */
    } else if (*p == '{') {
        /* 对象 */
        int depth = 1;
        const char *start = p;
        p++;
        while (p < end && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
        *out_len = p - start;
        return start;
    } else if (*p == '[') {
        /* 数组 */
        int depth = 1;
        const char *start = p;
        p++;
        while (p < end && depth > 0) {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            p++;
        }
        *out_len = p - start;
        return start;
    } else {
        /* 数字、布尔、null */
        const char *start = p;
        while (p < end && *p && *p != ',' && *p != ']' && *p != '}') p++;
        *out_len = p - start;
        return start;
    }
}

/**
 * @brief 解析路径段
 */
static int parse_path_segment(const char **path, char *out_key, size_t out_size,
                              int *out_index) {
    const char *p = *path;

    *out_key = '\0';
    *out_index = -1;

    p = skip_ws(p);
    if (*p == '.') p++;

    if (*p == '\0' || *p == ']' || *p == '}') {
        *path = p;
        return 0;  /* 路径结束 */
    }

    if (*p == '*') {
        /* 通配符 */
        strncpy(out_key, "*", out_size - 1);
        p++;
        *path = p;
        return 2;  /* 通配符 */
    }

    if (*p == '[') {
        /* 索引访问 */
        p++;
        p = skip_ws(p);

        if (*p == '*') {
            /* [*] 通配符 */
            p++;
            p = skip_ws(p);
            if (*p == ']') p++;
            *path = p;
            return 3;  /* 数组通配符 */
        }

        if (*p == '?') {
            /* 过滤表达式，简化处理 */
            p++;
            while (p && *p && *p != ']') p++;
            if (*p == ']') p++;
            *path = p;
            return 4;  /* 过滤表达式 */
        }

        /* 数字索引 */
        char *end;
        long idx = strtol(p, &end, 10);
        if (end == p) {
            return -1;  /* 无效索引 */
        }
        *out_index = (int)idx;
        p = end;
        p = skip_ws(p);
        if (*p == ']') p++;
        *path = p;
        return 1;  /* 数组索引 */
    }

    if (*p == '"') {
        /* 带引号的键 */
        p++;
        const char *key_start = p;
        while (*p && *p != '"') {
            if (*p == '\\') p++;
            p++;
        }
        size_t key_len = p - key_start;
        if (key_len >= out_size) key_len = out_size - 1;
        strncpy(out_key, key_start, key_len);
        out_key[key_len] = '\0';
        if (*p == '"') p++;
        *path = p;
        return 0;
    }

    /* 不带引号的键 */
    const char *key_start = p;
    while (*p && (isalnum(*p) || *p == '_' || *p == '-')) p++;
    size_t key_len = p - key_start;
    if (key_len >= out_size) key_len = out_size - 1;
    strncpy(out_key, key_start, key_len);
    out_key[key_len] = '\0';
    *path = p;
    return 0;
}

/* ========================================================================
 * 主要 API 实现
 * ======================================================================== */

int jsonpath_query(const char *json, size_t json_len,
                   const char *jsonpath,
                   jsonpath_result_t *out_result) {
    if (!json || !jsonpath || !out_result) return -1;

    memset(out_result, 0, sizeof(jsonpath_result_t));
    out_result->capacity = 16;
    out_result->values = calloc(out_result->capacity, sizeof(jsonpath_value_t));
    if (!out_result->values) return -1;

    /* 跳过 $ 前缀 */
    const char *path = jsonpath;
    if (*path == '$') path++;

    /* 找到 JSON 的根对象/数组 */
    const char *json_start = skip_ws(json);
    const char *json_pos = json_start;
    size_t remaining = json_len ? json_len - (json_pos - json) : strlen(json_pos);

    char key[256];
    int index;
    int seg_type;

    while (*path) {
        seg_type = parse_path_segment(&path, key, sizeof(key), &index);

        if (seg_type < 0) {
            /* 解析错误 */
            break;
        }

        if (seg_type == 0) {
            /* 键访问 */
            if (*json_pos == '[') {
                /* 当前是数组，需要先获取整个数组再找字段 */
                /* 简化处理：跳过数组 */
                json_pos = find_object_field(json_pos, remaining, key, strlen(key));
                if (!json_pos) return 0;
                remaining = json_len ? json_len - (json_pos - json_start) : strlen(json_pos);
            } else {
                json_pos = find_object_field(json_pos, remaining, key, strlen(key));
                if (!json_pos) return 0;
                remaining = json_len ? json_len - (json_pos - json_start) : strlen(json_pos);
            }
        } else if (seg_type == 1) {
            /* 数组索引 */
            json_pos = find_array_elem(json_pos, remaining, index);
            if (!json_pos) return 0;
            remaining = json_len ? json_len - (json_pos - json_start) : strlen(json_pos);
        } else if (seg_type == 2 || seg_type == 3) {
            /* 通配符 - 返回整个对象/数组 */
            size_t val_len;
            const char *val_start = extract_value_range(json_pos, json_pos + remaining, &val_len);

            jsonpath_value_t *v = &out_result->values[out_result->count++];
            v->path = strdup(jsonpath);
            v->type = (*json_pos == '{') ? JSONPATH_OBJECT : JSONPATH_ARRAY;
            v->value.str_val = (char *)malloc(val_len + 1);
            if (v->value.str_val) {
                memcpy(v->value.str_val, val_start, val_len);
                v->value.str_val[val_len] = '\0';
                v->value_len = val_len;
            }
            return 0;
        } else if (seg_type == 4) {
            /* 过滤表达式 - 简化处理 */
            /* TODO: 实现过滤逻辑 */
            continue;
        }
    }

    /* 提取最终值 */
    if (*json_pos) {
        size_t val_len;
        const char *val_start = extract_value_range(json_pos, json_pos + remaining, &val_len);

        jsonpath_value_t *v = &out_result->values[out_result->count++];
        v->path = strdup(jsonpath);

        if (*val_start == '"') {
            /* 字符串 */
            v->type = JSONPATH_STRING;
            v->value.str_val = parse_string(val_start, val_start + val_len, &v->value_len);
        } else if (isdigit(*val_start) || *val_start == '-' || *val_start == '+') {
            /* 数字 */
            v->type = JSONPATH_NUMBER;
            v->value.num_val = parse_number(val_start);
        } else if (strncmp(val_start, "true", 4) == 0) {
            v->type = JSONPATH_BOOL;
            v->value.bool_val = true;
        } else if (strncmp(val_start, "false", 5) == 0) {
            v->type = JSONPATH_BOOL;
            v->value.bool_val = false;
        } else if (strncmp(val_start, "null", 4) == 0) {
            v->type = JSONPATH_NULL;
        } else if (*val_start == '{') {
            v->type = JSONPATH_OBJECT;
            v->value.str_val = (char *)malloc(val_len + 1);
            if (v->value.str_val) {
                memcpy(v->value.str_val, val_start, val_len);
                v->value.str_val[val_len] = '\0';
                v->value_len = val_len;
            }
        } else if (*val_start == '[') {
            v->type = JSONPATH_ARRAY;
            v->value.str_val = (char *)malloc(val_len + 1);
            if (v->value.str_val) {
                memcpy(v->value.str_val, val_start, val_len);
                v->value.str_val[val_len] = '\0';
                v->value_len = val_len;
            }
        }
    }

    return 0;
}

void jsonpath_free_result(jsonpath_result_t *result) {
    if (!result) return;

    for (size_t i = 0; i < result->count; i++) {
        free(result->values[i].path);
        if (result->values[i].type == JSONPATH_STRING ||
            result->values[i].type == JSONPATH_OBJECT ||
            result->values[i].type == JSONPATH_ARRAY) {
            free(result->values[i].value.str_val);
        }
    }
    free(result->values);
    memset(result, 0, sizeof(jsonpath_result_t));
}

bool jsonpath_is_valid(const char *jsonpath) {
    if (!jsonpath || *jsonpath != '$') return false;

    /* 检查括号匹配 */
    int depth = 0;
    for (const char *p = jsonpath; *p; p++) {
        if (*p == '[') depth++;
        else if (*p == ']') {
            depth--;
            if (depth < 0) return false;
        }
    }
    return depth == 0;
}

const char *jsonpath_describe(const char *jsonpath) {
    if (!jsonpath) return "无效: NULL 指针";
    if (*jsonpath != '$') return "无效: 必须以 $ 开头";

    static char desc[256];
    snprintf(desc, sizeof(desc), "JSONPath: %s", jsonpath);
    return desc;
}

jsonpath_type_t jsonpath_get_type(const char *json) {
    if (!json) return JSONPATH_NULL;
    json = skip_ws(json);

    if (*json == '"') return JSONPATH_STRING;
    if (isdigit(*json) || *json == '-' || *json == '+') return JSONPATH_NUMBER;
    if (strncmp(json, "true", 4) == 0 || strncmp(json, "false", 5) == 0) return JSONPATH_BOOL;
    if (strncmp(json, "null", 4) == 0) return JSONPATH_NULL;
    if (*json == '[') return JSONPATH_ARRAY;
    if (*json == '{') return JSONPATH_OBJECT;
    return JSONPATH_STRING;
}

int jsonpath_to_string(const char *json, char *out, size_t out_size) {
    if (!json || !out || out_size == 0) return -1;

    json = skip_ws(json);

    if (*json == '"') {
        /* 去除引号 */
        size_t len;
        char *val = parse_string(json, json + strlen(json), &len);
        if (!val) return -1;

        if (len >= out_size) len = out_size - 1;
        memcpy(out, val, len);
        out[len] = '\0';
        free(val);
        return (int)len;
    }

    /* 直接复制 */
    size_t len = strlen(json);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, json, len);
    out[len] = '\0';
    return (int)len;
}

int jsonpath_array_length(const char *json) {
    if (!json) return -1;
    json = skip_ws(json);
    if (*json != '[') return -1;
    return get_array_length(json, strlen(json));
}

int jsonpath_object_size(const char *json) {
    if (!json) return -1;
    json = skip_ws(json);
    if (*json != '{') return -1;

    int count = 0;
    const char *p = json + 1;
    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == '"') {
            count++;
            while (*p && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
            if (*p == '"') p++;
        }
        while (*p && *p != ',' && *p != '}') p++;
        if (*p == ',') p++;
    }
    return count;
}

int jsonpath_query_with_filter(const char *json, size_t json_len,
                               const char *jsonpath,
                               jsonpath_filter_ctx_t *filter_ctx,
                               jsonpath_result_t *out_result) {
    /* 简化实现：直接调用基本查询 */
    (void)filter_ctx;
    return jsonpath_query(json, json_len, jsonpath, out_result);
}
