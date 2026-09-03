/**
 * @file kv_engine.c
 * @brief KV 存储引擎实现
 *
 * 基于现有 kv.h 实现 storage_ops_t 接口，封装 KV 存储引擎。
 */
#include "kv_engine.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define KV_ENGINE_NAME "kv_engine"
#define KV_DB_EXT ".kvdb"

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

typedef struct kv_engine_global_s {
    char data_dir[512];
    bool initialized;
} kv_engine_global_t;

static kv_engine_global_t g_kv_engine = {
    .data_dir = {0},
    .initialized = false
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int get_db_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/kv_%s%s",
             g_kv_engine.data_dir, name, KV_DB_EXT);
    return 0;
}

/**
 * @brief 从数据中提取 key
 * @param data 数据格式: key_len(4) + key + value_len(4) + value
 * @param len 数据总长度
 * @param key_len 输出：key 长度
 * @return key 指针，失败返回 NULL
 */
static const void *extract_key(const void *data, size_t len, size_t *key_len) {
    if (data == NULL || len < sizeof(uint32_t) * 2) return NULL;

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t klen;
    memcpy(&klen, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    /* key_len + key + value_len(4) + 至少1字节value */
    if (sizeof(uint32_t) + klen + sizeof(uint32_t) > len) return NULL;
    *key_len = klen;
    return ptr;  /* key 从 key_len 后面开始 */
}

/**
 * @brief 从数据中提取 value
 * @param data 数据格式: key_len(4) + key + value_len(4) + value
 * @param len 数据总长度
 * @param value_len 输出：value 长度
 * @return value 指针，失败返回 NULL
 */
static const void *extract_value(const void *data, size_t len, size_t *value_len) {
    if (data == NULL || len < sizeof(uint32_t) * 2) return NULL;

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t klen;
    memcpy(&klen, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t) + klen;  /* 跳过 key_len 和 key */

    if ((size_t)(ptr - (const uint8_t *)data) + sizeof(uint32_t) > len) return NULL;

    uint32_t vlen;
    memcpy(&vlen, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if ((size_t)(ptr - (const uint8_t *)data) + vlen > len) return NULL;
    *value_len = vlen;
    return ptr;  /* value 从 value_len 后面开始 */
}

/* ========================================================================
 * 存储引擎操作实现
 * ======================================================================== */

static int kv_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    char path[512];
    get_db_path(name, path, sizeof(path));
    kv_t *kv = kv_open(path);
    if (kv != NULL) {
        kv_close(kv);
    }
    return 0;
}

static void *kv_engine_table_open(const char *name, AccessMode mode) {
    (void)mode;
    char path[512];
    get_db_path(name, path, sizeof(path));

    kv_t *kv = kv_open(path);
    if (kv == NULL) {
        return NULL;
    }

    kv_engine_db_t *db = (kv_engine_db_t *)malloc(sizeof(kv_engine_db_t));
    if (db == NULL) {
        kv_close(kv);
        return NULL;
    }

    db->kv = kv;
    db->mode = mode;
    strncpy(db->db_path, path, sizeof(db->db_path) - 1);
    db->db_path[sizeof(db->db_path) - 1] = '\0';  /* 确保 NULL 终止 */
    strncpy(db->name, name, sizeof(db->name) - 1);
    db->name[sizeof(db->name) - 1] = '\0';  /* 确保 NULL 终止 */

    return db;
}

static int kv_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;
    kv_flush(db->kv);
    kv_close(db->kv);
    free(db);
    return 0;
}

static int kv_engine_table_drop(const char *name) {
    char path[512];
    get_db_path(name, path, sizeof(path));
    return remove(path) == 0 ? 0 : -1;
}

static int kv_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;

    /* 数据格式: key + value（直接存储） */
    /* 尝试提取 key 长度（假设 key 后面紧跟 value） */
    /* 先尝试解析 len=value_len 的格式 */
    size_t key_len, value_len;
    const void *key = extract_key(data, len, &key_len);
    const void *value = extract_value(data, len, &value_len);

    if (key == NULL || value == NULL) {
        /* 如果解析失败，假设整个数据是 value，key 为空 */
        kv_result_t result = kv_put(db->kv, "", 0, data, len);
        return (result == KV_OK) ? 0 : -1;
    }

    kv_result_t result = kv_put(db->kv, key, key_len, value, value_len);
    return (result == KV_OK) ? 0 : -1;
}

static int kv_engine_tuple_update(void *rel, const void *key, size_t key_len,
                                   const void *new_value, size_t new_value_len) {
    if (rel == NULL || key == NULL || new_value == NULL) return -1;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;

    /* KV 的 update 就是直接覆盖，kv_put 会自动替换已存在的 key */
    kv_result_t result = kv_put(db->kv, key, key_len, new_value, new_value_len);
    return (result == KV_OK) ? 0 : -1;
}

static int kv_engine_tuple_delete(void *rel, const void *key, size_t key_len) {
    if (rel == NULL || key == NULL) return -1;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;
    kv_result_t result = kv_delete(db->kv, key, key_len);
    return (result == KV_OK || result == KV_NOT_FOUND) ? 0 : -1;
}

static scan_desc_t *kv_engine_scan_begin(void *rel,
                                         const scan_key_t *keys, int nkeys,
                                         ScanDirection direction) {
    (void)keys;
    (void)nkeys;
    (void)direction;

    if (rel == NULL) return NULL;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;
    kv_iter_t *iter = kv_scan(db->kv, NULL, 0, NULL, 0);
    if (iter == NULL) return NULL;

    kv_engine_scan_t *scan = (kv_engine_scan_t *)malloc(sizeof(kv_engine_scan_t));
    if (scan == NULL) {
        kv_iter_free(iter);
        return NULL;
    }
    scan->db = db;
    scan->iter = iter;

    return (scan_desc_t *)scan;
}

static int kv_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    if (scan == NULL) return -1;
    kv_engine_scan_t *kv_scan = (kv_engine_scan_t *)scan;

    if (kv_iter_next(kv_scan->iter) != KV_OK) {
        return 1;
    }

    size_t key_len = kv_iter_key_len(kv_scan->iter);
    size_t value_len = kv_iter_value_len(kv_scan->iter);
    const void *key = kv_iter_key(kv_scan->iter);
    const void *value = kv_iter_value(kv_scan->iter);

    size_t total_len = sizeof(uint32_t) * 2 + key_len + value_len;
    if (total_len > *out_len) {
        *out_len = total_len;
        return -2;
    }

    uint8_t *ptr = (uint8_t *)out_data;
    memcpy(ptr, &key_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, key, key_len);
    ptr += key_len;
    memcpy(ptr, &value_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, value, value_len);

    *out_len = total_len;
    return 0;
}

static int kv_engine_scan_end(scan_desc_t *scan) {
    if (scan == NULL) return -1;
    kv_engine_scan_t *kv_scan = (kv_engine_scan_t *)scan;
    if (kv_scan->iter) kv_iter_free(kv_scan->iter);
    free(kv_scan);
    return 0;
}

static int kv_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));

    if (name == NULL) return 0;

    char path[512];
    get_db_path(name, path, sizeof(path));

    kv_t *kv = kv_open(path);
    if (kv == NULL) return -1;

    kv_stats_t s;
    if (kv_stats(kv, &s) == KV_OK) {
        stats->num_objects = s.num_keys;
        stats->num_pages = s.page_count;
        stats->cache_hit_rate = s.cache_hit_rate;
    }

    kv_close(kv);
    return 0;
}

/* ========================================================================
 * 引擎操作表
 * ======================================================================== */

static const storage_ops_t g_kv_engine_ops = {
    .name = KV_ENGINE_NAME,
    .model = MODEL_KV,
    .init = kv_engine_init,
    .shutdown = kv_engine_shutdown,
    .table_create = kv_engine_table_create,
    .table_open = kv_engine_table_open,
    .table_close = kv_engine_table_close,
    .table_drop = kv_engine_table_drop,
    .tuple_insert = kv_engine_tuple_insert,
    .tuple_update = kv_engine_tuple_update,
    .tuple_delete = kv_engine_tuple_delete,
    .scan_begin = kv_engine_scan_begin,
    .scan_next = kv_engine_scan_next,
    .scan_end = kv_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = kv_engine_get_stats,
};

/* ========================================================================
 * 引擎生命周期
 * ======================================================================== */

int kv_engine_init(const char *data_dir) {
    if (g_kv_engine.initialized) return 0;
    if (data_dir == NULL) return -1;

    strncpy(g_kv_engine.data_dir, data_dir, sizeof(g_kv_engine.data_dir) - 1);
    g_kv_engine.initialized = true;
    return 0;
}

int kv_engine_shutdown(void) {
    g_kv_engine.initialized = false;
    return 0;
}

/* ========================================================================
 * API 实现
 * ======================================================================== */

const storage_ops_t *kv_engine_get_ops(void) {
    return &g_kv_engine_ops;
}

int kv_engine_create(const char *name, const storage_schema_t *schema) {
    return kv_engine_table_create(name, schema);
}

void *kv_engine_open(const char *name, AccessMode mode) {
    return kv_engine_table_open(name, mode);
}

int kv_engine_close(void *rel) {
    return kv_engine_table_close(rel);
}

int kv_engine_drop(const char *name) {
    return kv_engine_table_drop(name);
}

int kv_engine_insert(void *rel, const void *data, size_t len) {
    return kv_engine_tuple_insert(rel, data, len);
}

int kv_engine_update(void *rel, const void *key, size_t key_len,
                     const void *new_value, size_t new_value_len) {
    return kv_engine_tuple_update(rel, key, key_len, new_value, new_value_len);
}

int kv_engine_delete(void *rel, const void *key, size_t key_len) {
    return kv_engine_tuple_delete(rel, key, key_len);
}

int kv_engine_get(void *rel, const void *key, size_t key_len,
                  void **out_value, size_t *out_len) {
    if (rel == NULL || key == NULL) return -1;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;
    kv_result_t result = kv_get(db->kv, key, key_len, out_value, (size_t *)out_len);
    return (result == KV_NOT_FOUND) ? 1 : ((result == KV_OK) ? 0 : -1);
}

bool kv_engine_exists(void *rel, const void *key, size_t key_len) {
    if (rel == NULL || key == NULL) return false;
    kv_engine_db_t *db = (kv_engine_db_t *)rel;
    void *val = NULL;
    size_t val_len = 0;
    kv_result_t result = kv_get(db->kv, key, key_len, &val, &val_len);
    return (result == KV_OK);
}

int kv_engine_stats(const char *name, storage_stats_t *stats) {
    return kv_engine_get_stats(name, stats);
}
