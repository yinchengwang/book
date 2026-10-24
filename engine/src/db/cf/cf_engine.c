/**
 * @file cf_engine.c
 * @brief 列族存储引擎实现
 *
 * 内部使用 KV 存储，通过复合键管理数据：
 *   - {cf_name}\x01{row_key}\x01{col_name}  -> 列值（cf_column_t 序列化）
 *   - {cf_name}\x02{row_key}                 -> 行索引（空值，存在即表示行存在）
 *   - {cf_name}\x03__cf_meta__               -> CF 元数据（"exists" 标记）
 *
 * 扫描流程：通过 KV 范围扫描找到所有 \x02{row_key} 标记，
 *          再按需获取每行的所有列。
 */

#include "db/cf/cf_engine.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

/** 列族键段分隔符 */
#define CF_KEY_SEP '\x01'
#define CF_ROW_SEP '\x02'
#define CF_META_SEP '\x03'

/** CF 元数据标记 */
#define CF_META_VALUE "cf_exists"

/* ============================================================
 * 数据库结构
 * ============================================================ */

struct cf_db_s {
    kv_t  *kv;                /**< 底层 KV 存储 */
    char   path[CF_MAX_PATH_LEN]; /**< 数据库路径 */
    char  *error_msg;         /**< 错误信息 */

    /* 统计信息缓存 */
    char                *stats_cf_name;    /**< 已缓存的列族名 */
    bool                 stats_dirty;      /**< 需要重新统计 */
    cf_family_stats_t    cached_stats;    /**< 缓存的统计值 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 拼接行索引键：{cf_name}{ROW_SEP}{row_key} */
static int build_row_idx_key(const char *cf_name,
                             const char *row_key, uint32_t row_key_len,
                             void **out_key, size_t *out_len) {
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    size_t total = cf_len + 1 + row_key_len;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;
    memcpy(buf, cf_name, cf_len);
    buf[cf_len] = CF_ROW_SEP;
    memcpy(buf + cf_len + 1, row_key, row_key_len);
    *out_key = buf;
    *out_len = total;
    return 0;
}

/** 拼接行索引扫描的起始键：{cf_name}{ROW_SEP} */
static int build_row_scan_start(const char *cf_name,
                                const char *row_key, uint32_t row_key_len,
                                void **out_key, size_t *out_len) {
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    if (!row_key || row_key_len == 0) {
        /* 起始为 {cf}{ROW_SEP} */
        size_t total = cf_len + 1;
        uint8_t *buf = (uint8_t *)malloc(total);
        if (!buf) return -1;
        memcpy(buf, cf_name, cf_len);
        buf[cf_len] = CF_ROW_SEP;
        *out_key = buf;
        *out_len = total;
        return 0;
    }
    /* 起始为 {cf}{ROW_SEP}{row_key} */
    return build_row_idx_key(cf_name, row_key, row_key_len, out_key, out_len);
}

/** 拼接行索引扫描的结束键：{cf_name}{ROW_SEP}\xFF（同一 cf 范围的最大值） */
static int build_row_scan_end(const char *cf_name,
                              const char *row_key, uint32_t row_key_len,
                              void **out_key, size_t *out_len) {
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    if (!row_key || row_key_len == 0) {
        /* 结束为 {cf}{ROW_SEP+1}，即 {cf}{META_SEP} */
        size_t total = cf_len + 1;
        uint8_t *buf = (uint8_t *)malloc(total);
        if (!buf) return -1;
        memcpy(buf, cf_name, cf_len);
        buf[cf_len] = (uint8_t)(CF_ROW_SEP + 1);  /* 进入下一个分隔段 */
        *out_key = buf;
        *out_len = total;
        return 0;
    }
    /* 结束为 {cf}{ROW_SEP}{row_key}\xFF，确保包含相同前缀的所有行 */
    size_t total = cf_len + 1 + row_key_len + 1;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;
    memcpy(buf, cf_name, cf_len);
    buf[cf_len] = CF_ROW_SEP;
    memcpy(buf + cf_len + 1, row_key, row_key_len);
    buf[total - 1] = 0xFF;
    *out_key = buf;
    *out_len = total;
    return 0;
}

/** 拼接列键前缀：{cf_name}{KEY_SEP}{row_key}{KEY_SEP} */
static int build_col_key_prefix(const char *cf_name,
                                const char *row_key, uint32_t row_key_len,
                                void **out_key, size_t *out_len) {
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    size_t total = cf_len + 1 + row_key_len + 1;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;
    memcpy(buf, cf_name, cf_len);
    buf[cf_len] = CF_KEY_SEP;
    memcpy(buf + cf_len + 1, row_key, row_key_len);
    buf[cf_len + 1 + row_key_len] = CF_KEY_SEP;
    *out_key = buf;
    *out_len = total;
    return 0;
}

/** 拼接列键：{cf_name}{KEY_SEP}{row_key}{KEY_SEP}{col_name} */
static int build_col_key(const char *cf_name,
                         const char *row_key, uint32_t row_key_len,
                         const char *col_name, uint32_t col_name_len,
                         void **out_key, size_t *out_len) {
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    size_t total = cf_len + 1 + row_key_len + 1 + col_name_len;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;
    size_t off = 0;
    memcpy(buf + off, cf_name, cf_len); off += cf_len;
    buf[off++] = CF_KEY_SEP;
    memcpy(buf + off, row_key, row_key_len); off += row_key_len;
    buf[off++] = CF_KEY_SEP;
    memcpy(buf + off, col_name, col_name_len); off += col_name_len;
    *out_key = buf;
    *out_len = total;
    return 0;
}

/** 拼接 CF 元数据键：{cf_name}{META_SEP}__cf_meta__ */
static int build_cf_meta_key(const char *cf_name,
                             void **out_key, size_t *out_len) {
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    /* 格式：{cf}{META_SEP}__cf_meta__ */
    const char *suffix = "__cf_meta__";
    size_t suffix_len = strlen(suffix);
    size_t total = cf_len + 1 + suffix_len;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return -1;
    memcpy(buf, cf_name, cf_len);
    buf[cf_len] = CF_META_SEP;
    memcpy(buf + cf_len + 1, suffix, suffix_len);
    *out_key = buf;
    *out_len = total;
    return 0;
}

/** 设置错误信息 */
static void cf_set_error(cf_db_t *db, const char *msg) {
    if (!db) return;
    if (db->error_msg) free(db->error_msg);
    db->error_msg = msg ? strdup(msg) : NULL;
}

/** 使统计缓存失效（如果针对同一列族） */
static void cf_invalidate_stats_if_same_cf(cf_db_t *db, const char *cf_name) {
    if (!db || !cf_name) return;
    if (db->stats_cf_name && strcmp(db->stats_cf_name, cf_name) == 0) {
        db->stats_dirty = true;
    }
}

/* ============================================================
 * 数据库生命周期
 * ============================================================ */

cf_db_t *cf_open(const char *path) {
    if (!path) return NULL;

    cf_db_t *db = (cf_db_t *)calloc(1, sizeof(cf_db_t));
    if (!db) return NULL;

    strncpy(db->path, path, CF_MAX_PATH_LEN - 1);
    db->kv = kv_open(path);
    if (!db->kv) {
        free(db);
        return NULL;
    }
    db->stats_dirty = true;  /* 强制首次统计进行全量扫描 */

    LOG_INFO("CF database opened: %s", path);
    return db;
}

cf_result_t cf_close(cf_db_t *db) {
    if (!db) return CF_INVALID;
    if (db->kv) kv_close(db->kv);
    if (db->error_msg) free(db->error_msg);
    if (db->stats_cf_name) free(db->stats_cf_name);
    free(db);
    return CF_OK;
}

const char *cf_errmsg(const cf_db_t *db) {
    if (!db) return "invalid db handle";
    if (db->error_msg) return db->error_msg;
    if (db->kv) {
        const char *kv_err = kv_errmsg(db->kv);
        if (kv_err) return kv_err;
    }
    return "no error";
}

cf_result_t cf_flush(cf_db_t *db) {
    if (!db || !db->kv) return CF_INVALID;
    kv_result_t r = kv_flush(db->kv);
    return (r == KV_OK) ? CF_OK : CF_ERROR;
}

/* ============================================================
 * 列族管理
 * ============================================================ */

cf_result_t cf_create_family(cf_db_t *db, const char *cf_name) {
    if (!db || !cf_name || strlen(cf_name) == 0
        || strlen(cf_name) > CF_MAX_CF_NAME_LEN) {
        cf_set_error(db, "invalid column family name");
        return CF_INVALID;
    }

    /* 检查是否已存在 */
    void *meta_key = NULL;
    size_t meta_key_len = 0;
    if (build_cf_meta_key(cf_name, &meta_key, &meta_key_len) != 0) {
        cf_set_error(db, "out of memory");
        return CF_NOMEM;
    }

    kv_result_t r = kv_put(db->kv, meta_key, meta_key_len,
                           CF_META_VALUE, strlen(CF_META_VALUE));
    free(meta_key);

    if (r != KV_OK) {
        cf_set_error(db, "failed to create column family");
        return CF_ERROR;
    }

    LOG_INFO("Column family created: %s", cf_name);
    return CF_OK;
}

cf_result_t cf_drop_family(cf_db_t *db, const char *cf_name) {
    if (!db || !cf_name) return CF_INVALID;

    uint32_t cf_len = (uint32_t)strlen(cf_name);
    /* 删除该 cf 前缀的所有键（{cf}{0x01..0x03}*）*/
    /* 通过三个范围扫描分别删除三类键 */
    kv_iter_t *iter = NULL;

    /* 删除所有列数据：扫描 {cf}{KEY_SEP} 起始到 {cf}{KEY_SEP+1} */
    {
        size_t start_len, end_len;
        void *start_key, *end_key;

        start_key = malloc(cf_len + 1);
        if (!start_key) return CF_NOMEM;
        memcpy(start_key, cf_name, cf_len);
        ((uint8_t *)start_key)[cf_len] = CF_KEY_SEP;
        start_len = cf_len + 1;

        end_key = malloc(cf_len + 1);
        if (!end_key) { free(start_key); return CF_NOMEM; }
        memcpy(end_key, cf_name, cf_len);
        ((uint8_t *)end_key)[cf_len] = (uint8_t)(CF_KEY_SEP + 1);
        end_len = cf_len + 1;

        iter = kv_scan(db->kv, start_key, start_len, end_key, end_len);
        free(start_key);
        free(end_key);

        if (iter) {
            while (kv_iter_next(iter) == KV_OK) {
                kv_delete(db->kv, kv_iter_key(iter), kv_iter_key_len(iter));
            }
            kv_iter_free(iter);
            iter = NULL;
        }
    }

    /* 删除行索引：{cf}{ROW_SEP} 范围 */
    {
        size_t start_len, end_len;
        void *start_key, *end_key;

        start_key = malloc(cf_len + 1);
        if (!start_key) return CF_NOMEM;
        memcpy(start_key, cf_name, cf_len);
        ((uint8_t *)start_key)[cf_len] = CF_ROW_SEP;
        start_len = cf_len + 1;

        end_key = malloc(cf_len + 1);
        if (!end_key) { free(start_key); return CF_NOMEM; }
        memcpy(end_key, cf_name, cf_len);
        ((uint8_t *)end_key)[cf_len] = (uint8_t)(CF_ROW_SEP + 1);
        end_len = cf_len + 1;

        iter = kv_scan(db->kv, start_key, start_len, end_key, end_len);
        free(start_key);
        free(end_key);

        if (iter) {
            while (kv_iter_next(iter) == KV_OK) {
                kv_delete(db->kv, kv_iter_key(iter), kv_iter_key_len(iter));
            }
            kv_iter_free(iter);
            iter = NULL;
        }
    }

    /* 删除 CF 元数据：{cf}{META_SEP}__cf_meta__ */
    {
        void *meta_key = NULL;
        size_t meta_key_len = 0;
        if (build_cf_meta_key(cf_name, &meta_key, &meta_key_len) == 0) {
            kv_delete(db->kv, meta_key, meta_key_len);
            free(meta_key);
        }
    }

    /* 使该列族的缓存统计失效 */
    if (db->stats_cf_name && strcmp(db->stats_cf_name, cf_name) == 0) {
        free(db->stats_cf_name);
        db->stats_cf_name = NULL;
        db->stats_dirty = false;
    }

    LOG_INFO("Column family dropped: %s", cf_name);
    return CF_OK;
}

bool cf_family_exists(cf_db_t *db, const char *cf_name) {
    if (!db || !cf_name) return false;

    void *meta_key = NULL;
    size_t meta_key_len = 0;
    if (build_cf_meta_key(cf_name, &meta_key, &meta_key_len) != 0) {
        return false;
    }

    bool exists = kv_exists(db->kv, meta_key, meta_key_len);
    free(meta_key);
    return exists;
}

cf_result_t cf_list_families(cf_db_t *db,
                             char ***out_names,
                             uint32_t *out_count) {
    if (!db || !out_names || !out_count) return CF_INVALID;

    *out_names = NULL;
    *out_count = 0;

    /* 扫描整个键空间，过滤出 {cf_name}{META_SEP}__cf_meta__ 格式的键 */
    kv_iter_t *iter = kv_scan(db->kv, NULL, 0, NULL, 0);
    if (!iter) return CF_OK;  /* 空数据库 */

    const size_t meta_suffix_len = strlen("__cf_meta__");

    /* 收集列族名（用于去重） */
    uint32_t count = 0;
    size_t cap = 16;
    char **names = (char **)calloc(cap, sizeof(char *));
    if (!names) {
        kv_iter_free(iter);
        return CF_NOMEM;
    }

    while (kv_iter_next(iter) == KV_OK) {
        const void *k = kv_iter_key(iter);
        size_t klen = kv_iter_key_len(iter);

        /* 必须是 {cf_name}{META_SEP}__cf_meta__ 格式 */
        if (klen < meta_suffix_len + 1) continue;
        if (memcmp((const char *)k + klen - meta_suffix_len,
                   "__cf_meta__", meta_suffix_len) != 0) continue;

        /* cf_name 部分：去掉末尾 META_SEP + __cf_meta__ */
        size_t cf_name_len = klen - 1 - meta_suffix_len;
        if (cf_name_len == 0) continue;

        const uint8_t *kb = (const uint8_t *)k;
        /* 检查倒数 meta_suffix_len+1 处是否为 META_SEP */
        if (kb[cf_name_len] != CF_META_SEP) continue;

        /* 去重：避免同一 cf 出现多次（理论上 cf_create_family 不会重复，
           但防御性检查） */
        bool duplicate = false;
        for (uint32_t i = 0; i < count; i++) {
            if (strlen(names[i]) == cf_name_len &&
                memcmp(names[i], kb, cf_name_len) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        /* 扩容 */
        if (count >= cap) {
            cap *= 2;
            char **new_names = (char **)realloc(names, cap * sizeof(char *));
            if (!new_names) {
                for (uint32_t i = 0; i < count; i++) free(names[i]);
                free(names);
                kv_iter_free(iter);
                return CF_NOMEM;
            }
            names = new_names;
        }

        names[count] = (char *)malloc(cf_name_len + 1);
        if (!names[count]) {
            for (uint32_t i = 0; i < count; i++) free(names[i]);
            free(names);
            kv_iter_free(iter);
            return CF_NOMEM;
        }
        memcpy(names[count], kb, cf_name_len);
        names[count][cf_name_len] = '\0';
        count++;
    }
    kv_iter_free(iter);

    *out_names = names;
    *out_count = count;
    return CF_OK;
}

void cf_free_family_list(char **names, uint32_t count) {
    if (!names) return;
    for (uint32_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

/* ============================================================
 * 单列操作
 * ============================================================ */

cf_result_t cf_put(cf_db_t *db,
                   const char *cf_name,
                   const char *row_key, uint32_t row_key_len,
                   const char *col_name, uint32_t col_name_len,
                   const void *value, uint32_t value_len,
                   int32_t ttl_seconds) {
    if (!db || !cf_name || !row_key || row_key_len == 0
        || !col_name || col_name_len == 0) {
        cf_set_error(db, "invalid arguments");
        return CF_INVALID;
    }

    /* 确保列族存在（惰性创建） */
    if (!cf_family_exists(db, cf_name)) {
        cf_result_t r = cf_create_family(db, cf_name);
        if (r != CF_OK) return r;
    }

    /* 构造列 */
    cf_column_t *col = cf_column_create(col_name, col_name_len,
                                        value, value_len,
                                        0, ttl_seconds);
    if (!col) {
        cf_set_error(db, "failed to create column");
        return CF_NOMEM;
    }

    /* 序列化列 */
    size_t col_buf_size = cf_column_serialized_size(col);
    uint8_t *col_buf = (uint8_t *)malloc(col_buf_size);
    if (!col_buf) {
        cf_column_free(col);
        cf_set_error(db, "out of memory");
        return CF_NOMEM;
    }
    if (cf_column_serialize(col, col_buf, col_buf_size) != 0) {
        free(col_buf);
        cf_column_free(col);
        cf_set_error(db, "serialize failed");
        return CF_ERROR;
    }

    /* 构造列键 */
    void *col_key = NULL;
    size_t col_key_len = 0;
    if (build_col_key(cf_name, row_key, row_key_len,
                      col_name, col_name_len,
                      &col_key, &col_key_len) != 0) {
        free(col_buf);
        cf_column_free(col);
        cf_set_error(db, "out of memory");
        return CF_NOMEM;
    }

    /* 写入列数据 */
    kv_result_t r = kv_put(db->kv, col_key, col_key_len, col_buf, col_buf_size);
    free(col_key);
    free(col_buf);

    if (r != KV_OK) {
        cf_column_free(col);
        cf_set_error(db, "kv_put failed");
        return CF_ERROR;
    }

    /* 写入行索引（标记行存在） */
    void *row_idx_key = NULL;
    size_t row_idx_key_len = 0;
    if (build_row_idx_key(cf_name, row_key, row_key_len,
                          &row_idx_key, &row_idx_key_len) != 0) {
        cf_column_free(col);
        return CF_NOMEM;
    }
    kv_put(db->kv, row_idx_key, row_idx_key_len, "1", 1);
    free(row_idx_key);

    cf_column_free(col);
    cf_invalidate_stats_if_same_cf(db, cf_name);
    return CF_OK;
}

cf_result_t cf_get(cf_db_t *db,
                   const char *cf_name,
                   const char *row_key, uint32_t row_key_len,
                   const char *col_name, uint32_t col_name_len,
                   void **out_value, uint32_t *out_len) {
    if (!db || !cf_name || !row_key || row_key_len == 0
        || !col_name || col_name_len == 0
        || !out_value || !out_len) {
        return CF_INVALID;
    }

    *out_value = NULL;
    *out_len = 0;

    /* 构造列键 */
    void *col_key = NULL;
    size_t col_key_len = 0;
    if (build_col_key(cf_name, row_key, row_key_len,
                      col_name, col_name_len,
                      &col_key, &col_key_len) != 0) {
        return CF_NOMEM;
    }

    /* 读取列数据 */
    void *raw = NULL;
    size_t raw_len = 0;
    kv_result_t r = kv_get(db->kv, col_key, col_key_len, &raw, &raw_len);
    free(col_key);

    if (r == KV_NOT_FOUND) return CF_NOT_FOUND;
    if (r != KV_OK) {
        if (raw) free(raw);
        return CF_ERROR;
    }

    /* 反序列化得到列结构 */
    cf_column_t *col = NULL;
    if (cf_column_deserialize(raw, raw_len, &col) != 0) {
        free(raw);
        cf_set_error(db, "deserialize failed");
        return CF_CORRUPT;
    }
    free(raw);

    /* 输出值的副本 */
    if (col->value_len > 0) {
        *out_value = malloc(col->value_len);
        if (!*out_value) {
            cf_column_free(col);
            return CF_NOMEM;
        }
        memcpy(*out_value, col->value, col->value_len);
    }
    *out_len = col->value_len;
    cf_column_free(col);
    return CF_OK;
}

cf_result_t cf_delete_column(cf_db_t *db,
                             const char *cf_name,
                             const char *row_key, uint32_t row_key_len,
                             const char *col_name, uint32_t col_name_len) {
    if (!db || !cf_name || !row_key || row_key_len == 0
        || !col_name || col_name_len == 0) {
        return CF_INVALID;
    }

    void *col_key = NULL;
    size_t col_key_len = 0;
    if (build_col_key(cf_name, row_key, row_key_len,
                      col_name, col_name_len,
                      &col_key, &col_key_len) != 0) {
        return CF_NOMEM;
    }

    /* 检查列是否存在 */
    if (!kv_exists(db->kv, col_key, col_key_len)) {
        free(col_key);
        return CF_NOT_FOUND;
    }

    kv_result_t r = kv_delete(db->kv, col_key, col_key_len);
    free(col_key);

    if (r == KV_OK) cf_invalidate_stats_if_same_cf(db, cf_name);
    return (r == KV_OK) ? CF_OK : CF_ERROR;
}

bool cf_exists(cf_db_t *db,
               const char *cf_name,
               const char *row_key, uint32_t row_key_len,
               const char *col_name, uint32_t col_name_len) {
    if (!db || !cf_name || !row_key || row_key_len == 0
        || !col_name || col_name_len == 0) {
        return false;
    }

    void *col_key = NULL;
    size_t col_key_len = 0;
    if (build_col_key(cf_name, row_key, row_key_len,
                      col_name, col_name_len,
                      &col_key, &col_key_len) != 0) {
        return false;
    }

    bool exists = kv_exists(db->kv, col_key, col_key_len);
    free(col_key);
    return exists;
}

/* ============================================================
 * 行操作
 * ============================================================ */

cf_result_t cf_get_row(cf_db_t *db,
                       const char *cf_name,
                       const char *row_key, uint32_t row_key_len,
                       cf_row_t **out_row) {
    if (!db || !cf_name || !row_key || row_key_len == 0 || !out_row) {
        return CF_INVALID;
    }
    *out_row = NULL;

    /* 检查行是否存在 */
    void *idx_key = NULL;
    size_t idx_key_len = 0;
    if (build_row_idx_key(cf_name, row_key, row_key_len,
                          &idx_key, &idx_key_len) != 0) {
        return CF_NOMEM;
    }
    if (!kv_exists(db->kv, idx_key, idx_key_len)) {
        free(idx_key);
        return CF_NOT_FOUND;
    }
    free(idx_key);

    /* 创建空行 */
    cf_row_t *row = cf_row_create(row_key, row_key_len);
    if (!row) return CF_NOMEM;

    /* 扫描 {cf}{KEY_SEP}{row_key}{KEY_SEP} 前缀的所有列 */
    void *prefix = NULL;
    size_t prefix_len = 0;
    if (build_col_key_prefix(cf_name, row_key, row_key_len,
                             &prefix, &prefix_len) != 0) {
        cf_row_free(row);
        return CF_NOMEM;
    }

    /* 范围：prefix 到 prefix + \xFF */
    size_t end_len = prefix_len + 1;
    uint8_t *end_key = (uint8_t *)malloc(end_len);
    if (!end_key) {
        free(prefix);
        cf_row_free(row);
        return CF_NOMEM;
    }
    memcpy(end_key, prefix, prefix_len);
    end_key[prefix_len] = 0xFF;

    kv_iter_t *iter = kv_scan(db->kv, prefix, prefix_len, end_key, end_len);
    free(prefix);
    free(end_key);

    if (!iter) {
        /* 无列但行存在，返回空行 */
        *out_row = row;
        return CF_OK;
    }

    while (kv_iter_next(iter) == KV_OK) {
        const void *v = kv_iter_value(iter);
        size_t vlen = kv_iter_value_len(iter);

        cf_column_t *col = NULL;
        if (cf_column_deserialize(v, vlen, &col) != 0) {
            kv_iter_free(iter);
            cf_row_free(row);
            cf_set_error(db, "deserialize column failed");
            return CF_CORRUPT;
        }
        if (cf_row_add_column(row, col) != 0) {
            cf_column_free(col);
            kv_iter_free(iter);
            cf_row_free(row);
            return CF_NOMEM;
        }
        cf_column_free(col);
    }
    kv_iter_free(iter);

    *out_row = row;
    return CF_OK;
}

cf_result_t cf_delete_row(cf_db_t *db,
                          const char *cf_name,
                          const char *row_key, uint32_t row_key_len) {
    if (!db || !cf_name || !row_key || row_key_len == 0) {
        return CF_INVALID;
    }

    /* 先获取行内所有列，逐个删除 */
    cf_row_t *row = NULL;
    cf_result_t r = cf_get_row(db, cf_name, row_key, row_key_len, &row);
    if (r == CF_NOT_FOUND) return CF_NOT_FOUND;
    if (r != CF_OK) return r;

    /* 删除每列 */
    for (uint32_t i = 0; i < row->num_columns; i++) {
        cf_column_t *col = row->columns[i];
        cf_delete_column(db, cf_name, row_key, row_key_len,
                         col->name, col->name_len);
    }
    cf_row_free(row);

    /* 删除行索引 */
    void *idx_key = NULL;
    size_t idx_key_len = 0;
    if (build_row_idx_key(cf_name, row_key, row_key_len,
                          &idx_key, &idx_key_len) != 0) {
        return CF_NOMEM;
    }
    kv_delete(db->kv, idx_key, idx_key_len);
    free(idx_key);

    cf_invalidate_stats_if_same_cf(db, cf_name);
    return CF_OK;
}

bool cf_row_exists(cf_db_t *db,
                   const char *cf_name,
                   const char *row_key, uint32_t row_key_len) {
    if (!db || !cf_name || !row_key || row_key_len == 0) return false;

    void *idx_key = NULL;
    size_t idx_key_len = 0;
    if (build_row_idx_key(cf_name, row_key, row_key_len,
                          &idx_key, &idx_key_len) != 0) {
        return false;
    }
    bool exists = kv_exists(db->kv, idx_key, idx_key_len);
    free(idx_key);
    return exists;
}

/* ============================================================
 * 扫描
 * ============================================================ */

/**
 * @brief 行扫描迭代器
 */
struct cf_iter_s {
    kv_iter_t       *kv_iter;    /**< 底层 KV 迭代器（扫描行索引） */
    cf_db_t         *db;         /**< 数据库句柄 */
    cf_row_t        *current;    /**< 当前行（延迟加载） */
    char            *cf_name;    /**< 列族名 */
    uint32_t         cf_len;     /**< 列族名长度（预计算） */
};

cf_iter_t *cf_scan_rows(cf_db_t *db,
                        const char *cf_name,
                        const char *start_row_key, uint32_t start_len,
                        const char *end_row_key, uint32_t end_len) {
    if (!db || !cf_name) return NULL;

    void *start_key = NULL;
    size_t start_key_len = 0;
    void *end_key = NULL;
    size_t end_key_len = 0;

    if (build_row_scan_start(cf_name, start_row_key, start_len,
                             &start_key, &start_key_len) != 0) {
        return NULL;
    }
    if (build_row_scan_end(cf_name, end_row_key, end_len,
                           &end_key, &end_key_len) != 0) {
        free(start_key);
        return NULL;
    }

    kv_iter_t *kv_iter = kv_scan(db->kv, start_key, start_key_len,
                                  end_key, end_key_len);
    free(start_key);
    free(end_key);

    if (!kv_iter) return NULL;

    cf_iter_t *iter = (cf_iter_t *)calloc(1, sizeof(cf_iter_t));
    if (!iter) {
        kv_iter_free(kv_iter);
        return NULL;
    }
    iter->kv_iter = kv_iter;
    iter->db = db;
    iter->cf_name = strdup(cf_name);
    iter->cf_len = (uint32_t)strlen(cf_name);
    if (!iter->cf_name) {
        kv_iter_free(kv_iter);
        free(iter);
        return NULL;
    }
    return iter;
}

cf_result_t cf_iter_next(cf_iter_t *iter) {
    if (!iter) return CF_INVALID;

    /* 释放上一行 */
    if (iter->current) {
        cf_row_free(iter->current);
        iter->current = NULL;
    }

    /* 前进 KV 迭代器 */
    kv_result_t r = kv_iter_next(iter->kv_iter);
    if (r == KV_NOT_FOUND) return CF_NOT_FOUND;
    if (r != KV_OK) return CF_ERROR;

    /* 从 KV 键中提取 row_key */
    const void *k = kv_iter_key(iter->kv_iter);
    size_t klen = kv_iter_key_len(iter->kv_iter);

    /* 键格式: {cf_name}{ROW_SEP}{row_key} */
    if (klen < (size_t)(iter->cf_len + 1)) return CF_ERROR;
    if (((const uint8_t *)k)[iter->cf_len] != CF_ROW_SEP) return CF_ERROR;

    const char *row_key = (const char *)k + iter->cf_len + 1;
    uint32_t row_key_len = (uint32_t)(klen - iter->cf_len - 1);

    /* 加载整行 */
    cf_result_t cr = cf_get_row(iter->db, iter->cf_name,
                                row_key, row_key_len,
                                &iter->current);
    if (cr != CF_OK) return cr;
    return CF_OK;
}

const cf_row_t *cf_iter_row(cf_iter_t *iter) {
    if (!iter) return NULL;
    return iter->current;
}

const char *cf_iter_row_key(cf_iter_t *iter) {
    if (!iter || !iter->current) return NULL;
    return iter->current->row_key;
}

uint32_t cf_iter_row_key_len(cf_iter_t *iter) {
    if (!iter || !iter->current) return 0;
    return iter->current->row_key_len;
}

void cf_iter_free(cf_iter_t *iter) {
    if (!iter) return;
    if (iter->current) cf_row_free(iter->current);
    if (iter->kv_iter) kv_iter_free(iter->kv_iter);
    if (iter->cf_name) free(iter->cf_name);
    free(iter);
}

/* ============================================================
 * 批量操作
 * ============================================================ */

cf_result_t cf_batch_execute(cf_db_t *db,
                             const cf_batch_op_t *ops, uint32_t n_ops,
                             cf_result_t *out_results) {
    if (!db || !ops || n_ops == 0) return CF_INVALID;

    cf_result_t worst = CF_OK;
    for (uint32_t i = 0; i < n_ops; i++) {
        const cf_batch_op_t *op = &ops[i];
        cf_result_t r = CF_ERROR;

        switch (op->type) {
            case CF_BATCH_PUT:
                r = cf_put(db, op->cf_name,
                           op->row_key, op->row_key_len,
                           op->col_name, op->col_name_len,
                           op->value, op->value_len, 0);
                break;
            case CF_BATCH_DELETE_COL:
                r = cf_delete_column(db, op->cf_name,
                                     op->row_key, op->row_key_len,
                                     op->col_name, op->col_name_len);
                break;
            case CF_BATCH_DELETE_ROW:
                r = cf_delete_row(db, op->cf_name,
                                  op->row_key, op->row_key_len);
                break;
            default:
                r = CF_INVALID;
                break;
        }

        if (out_results) out_results[i] = r;
        if (r != CF_OK && worst == CF_OK) worst = r;
    }

    return worst;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

cf_result_t cf_family_stats(cf_db_t *db, const char *cf_name,
                            cf_family_stats_t *stats) {
    if (!db || !cf_name || !stats) return CF_INVALID;

    /* 缓存命中且未脏：直接返回 */
    if (!db->stats_dirty && db->stats_cf_name &&
        strcmp(db->stats_cf_name, cf_name) == 0) {
        *stats = db->cached_stats;
        return CF_OK;
    }

    /* 全量扫描 */
    cf_family_stats_t new_stats = {0};
    void *start_key = NULL;
    size_t start_key_len = 0;
    void *end_key = NULL;
    size_t end_key_len = 0;

    if (build_row_scan_start(cf_name, NULL, 0,
                             &start_key, &start_key_len) != 0) {
        return CF_NOMEM;
    }
    if (build_row_scan_end(cf_name, NULL, 0,
                           &end_key, &end_key_len) != 0) {
        free(start_key);
        return CF_NOMEM;
    }

    kv_iter_t *iter = kv_scan(db->kv, start_key, start_key_len,
                              end_key, end_key_len);
    free(start_key);
    free(end_key);

    if (iter) {
        while (kv_iter_next(iter) == KV_OK) {
            size_t klen = kv_iter_key_len(iter);
            size_t vlen = kv_iter_value_len(iter);
            new_stats.num_rows++;
            new_stats.total_size += klen + vlen;
        }
        kv_iter_free(iter);
    }

    /* 计算列数 */
    uint32_t cf_len = (uint32_t)strlen(cf_name);
    void *col_start = malloc(cf_len + 1);
    if (!col_start) return CF_NOMEM;
    memcpy(col_start, cf_name, cf_len);
    ((uint8_t *)col_start)[cf_len] = CF_KEY_SEP;

    void *col_end = malloc(cf_len + 1);
    if (!col_end) { free(col_start); return CF_NOMEM; }
    memcpy(col_end, cf_name, cf_len);
    ((uint8_t *)col_end)[cf_len] = (uint8_t)(CF_KEY_SEP + 1);

    kv_iter_t *col_iter = kv_scan(db->kv, col_start, cf_len + 1,
                                  col_end, cf_len + 1);
    free(col_start);
    free(col_end);

    if (col_iter) {
        while (kv_iter_next(col_iter) == KV_OK) {
            new_stats.num_columns++;
            new_stats.total_size += kv_iter_key_len(col_iter);
        }
        kv_iter_free(col_iter);
    }

    /* 更新缓存 */
    if (db->stats_cf_name) free(db->stats_cf_name);
    db->stats_cf_name = strdup(cf_name);
    if (!db->stats_cf_name) return CF_NOMEM;
    db->cached_stats = new_stats;
    db->stats_dirty = false;

    *stats = new_stats;
    return CF_OK;
}