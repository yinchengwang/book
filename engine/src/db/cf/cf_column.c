/**
 * @file cf_column.c
 * @brief 列（Column）操作实现
 *
 * 提供列的创建、释放、序列化、反序列化、TTL 检查等功能。
 */

#include "db/cf/cf_column.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 内部辅助
 * ============================================================ */

/**
 * @brief 获取当前时间（毫秒）
 */
static int64_t cf_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ============================================================
 * 列操作实现
 * ============================================================ */

cf_column_t *cf_column_create(const char *name, uint32_t name_len,
                              const void *value, uint32_t value_len,
                              int64_t timestamp, int32_t ttl_seconds) {
    if (!name || name_len == 0 || name_len > CF_MAX_COLUMN_NAME_LEN) {
        return NULL;
    }
    if (value_len > CF_MAX_COLUMN_VALUE_LEN) {
        return NULL;
    }

    cf_column_t *col = (cf_column_t *)calloc(1, sizeof(cf_column_t));
    if (!col) return NULL;

    /* 复制列名（含末尾 '\0'） */
    col->name = (char *)malloc(name_len + 1);
    if (!col->name) {
        free(col);
        return NULL;
    }
    memcpy(col->name, name, name_len);
    col->name[name_len] = '\0';
    col->name_len = name_len;

    /* 复制列值 */
    if (value_len > 0 && value) {
        col->value = malloc(value_len);
        if (!col->value) {
            free(col->name);
            free(col);
            return NULL;
        }
        memcpy(col->value, value, value_len);
    }
    col->value_len = value_len;

    /* 时间戳：0 表示自动设为当前时间 */
    col->timestamp = (timestamp == 0) ? cf_now_ms() : timestamp;
    col->ttl_seconds = ttl_seconds;

    return col;
}

cf_column_t *cf_column_clone(const cf_column_t *col) {
    if (!col) return NULL;
    return cf_column_create(col->name, col->name_len,
                            col->value, col->value_len,
                            col->timestamp, col->ttl_seconds);
}

void cf_column_free(cf_column_t *col) {
    if (!col) return;
    if (col->name) free(col->name);
    if (col->value) free(col->value);
    free(col);
}

int cf_column_compare(const cf_column_t *a, const cf_column_t *b) {
    if (!a || !b) return -1;
    size_t min_len = (a->name_len < b->name_len) ? a->name_len : b->name_len;
    int cmp = memcmp(a->name, b->name, min_len);
    if (cmp != 0) return cmp;
    if (a->name_len < b->name_len) return -1;
    if (a->name_len > b->name_len) return 1;
    return 0;
}

bool cf_column_is_expired(const cf_column_t *col, int64_t now_ms) {
    if (!col || col->ttl_seconds <= 0) return false;
    int64_t elapsed = (now_ms - col->timestamp) / 1000;
    return elapsed >= col->ttl_seconds;
}

size_t cf_column_serialized_size(const cf_column_t *col) {
    if (!col) return 0;
    /* 格式：name_len(4) + name + value_len(4) + value + timestamp(8) + ttl(4) */
    return 4 + col->name_len + 4 + col->value_len + 8 + 4;
}

int cf_column_serialize(const cf_column_t *col, void *buf, size_t buf_size) {
    if (!col || !buf) return -1;
    size_t need = cf_column_serialized_size(col);
    if (buf_size < need) return -1;

    uint8_t *p = (uint8_t *)buf;
    /* name_len */
    memcpy(p, &col->name_len, sizeof(uint32_t));
    p += sizeof(uint32_t);
    /* name */
    memcpy(p, col->name, col->name_len);
    p += col->name_len;
    /* value_len */
    memcpy(p, &col->value_len, sizeof(uint32_t));
    p += sizeof(uint32_t);
    /* value */
    if (col->value_len > 0) {
        memcpy(p, col->value, col->value_len);
        p += col->value_len;
    }
    /* timestamp */
    memcpy(p, &col->timestamp, sizeof(int64_t));
    p += sizeof(int64_t);
    /* ttl */
    memcpy(p, &col->ttl_seconds, sizeof(int32_t));
    p += sizeof(int32_t);

    return 0;
}

int cf_column_deserialize(const void *buf, size_t buf_len, cf_column_t **out_col) {
    if (!buf || !out_col || buf_len < 4 + 4 + 8 + 4) {
        return -1;
    }

    const uint8_t *p = (const uint8_t *)buf;

    /* name_len */
    uint32_t name_len;
    memcpy(&name_len, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    if (name_len == 0 || name_len > CF_MAX_COLUMN_NAME_LEN) return -1;
    if ((size_t)(p - (const uint8_t *)buf) + name_len + 4 + 8 + 4 > buf_len) {
        return -1;
    }

    /* 读取 name */
    char *name = (char *)malloc(name_len + 1);
    if (!name) return -1;
    memcpy(name, p, name_len);
    name[name_len] = '\0';
    p += name_len;

    /* value_len */
    uint32_t value_len;
    memcpy(&value_len, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    if (value_len > CF_MAX_COLUMN_VALUE_LEN) {
        free(name);
        return -1;
    }
    if ((size_t)(p - (const uint8_t *)buf) + value_len + 8 + 4 > buf_len) {
        free(name);
        return -1;
    }

    /* 读取 value */
    void *value = NULL;
    if (value_len > 0) {
        value = malloc(value_len);
        if (!value) {
            free(name);
            return -1;
        }
        memcpy(value, p, value_len);
        p += value_len;
    }

    /* timestamp */
    int64_t timestamp;
    memcpy(&timestamp, p, sizeof(int64_t));
    p += sizeof(int64_t);

    /* ttl */
    int32_t ttl;
    memcpy(&ttl, p, sizeof(int32_t));
    p += sizeof(int32_t);

    /* 构造列（保持读取的时间戳，不覆盖） */
    cf_column_t *col = (cf_column_t *)calloc(1, sizeof(cf_column_t));
    if (!col) {
        free(name);
        if (value) free(value);
        return -1;
    }
    col->name = name;
    col->name_len = name_len;
    col->value = value;
    col->value_len = value_len;
    col->timestamp = timestamp;
    col->ttl_seconds = ttl;

    *out_col = col;
    return 0;
}