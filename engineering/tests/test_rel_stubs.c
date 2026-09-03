/**
 * @file test_rel_stubs.c
 * @brief 测试用桩函数（relational 模块依赖）
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* 前向声明 */
typedef struct kv_s kv_t;
typedef struct kv_iter_s kv_iter_t;
typedef struct buffer_s buffer_t;
typedef struct page_s page_t;
typedef struct lock_mgr_s lock_mgr_t;
typedef struct catalog_s catalog_t;
typedef struct wal_s wal_t;

/* catalog 桩 */
typedef struct {
    char *name;
    int relkind;
    int am;
    int natts;
    void *columns;
} catalog_table_t;

catalog_table_t *catalog_get_table(catalog_t *catalog, uint32_t relid) {
    (void)catalog; (void)relid;
    return NULL;
}

catalog_table_t *catalog_get_index(catalog_t *catalog, uint32_t relid) {
    (void)catalog; (void)relid;
    return NULL;
}

void *catalog_get_columns(catalog_t *catalog, uint32_t relid, int *natts) {
    (void)catalog; (void)relid;
    if (natts) *natts = 0;
    return NULL;
}

void catalog_free_table(catalog_table_t *table) {
    (void)table;
}

int catalog_drop_table(catalog_t *catalog, uint32_t relid) {
    (void)catalog; (void)relid;
    return 0;
}

int catalog_init(catalog_t *catalog, const char *path) {
    (void)catalog; (void)path;
    return 0;
}

void catalog_shutdown(catalog_t *catalog) {
    (void)catalog;
}

/* buf 桩 */
int buf_init(const char *path) {
    (void)path;
    return 0;
}

void buf_shutdown(void) {
}

int buf_unpin(buffer_t *buf) {
    (void)buf;
    return 0;
}

/* heapam 桩 */
void *heap_getnext(void *scan) {
    (void)scan;
    return NULL;
}

/* lock 桩 */
lock_mgr_t *lock_mgr_create(void) {
    return NULL;
}

int lock_acquire(lock_mgr_t *mgr, uint64_t id, int mode) {
    (void)mgr; (void)id; (void)mode;
    return 0;
}

int lock_release(lock_mgr_t *mgr, uint64_t id) {
    (void)mgr; (void)id;
    return 0;
}

/* kv 桩 */
int kv_get(kv_t *db, const char *key, size_t key_len, void *value, size_t *value_len) {
    (void)db; (void)key; (void)key_len; (void)value; (void)value_len;
    return -1;
}

int kv_put(kv_t *db, const char *key, size_t key_len, const void *value, size_t value_len) {
    (void)db; (void)key; (void)key_len; (void)value; (void)value_len;
    return 0;
}

int kv_delete(kv_t *db, const char *key, size_t key_len) {
    (void)db; (void)key; (void)key_len;
    return 0;
}

kv_iter_t *kv_scan(kv_t *db, const char *prefix, size_t prefix_len) {
    (void)db; (void)prefix; (void)prefix_len;
    return NULL;
}

int kv_iter_next(kv_iter_t *iter) {
    (void)iter;
    return 0;
}

const char *kv_iter_key(kv_iter_t *iter, size_t *key_len) {
    (void)iter;
    if (key_len) *key_len = 0;
    return NULL;
}

const void *kv_iter_value(kv_iter_t *iter, size_t *value_len) {
    (void)iter;
    if (value_len) *value_len = 0;
    return NULL;
}

size_t kv_iter_value_len(kv_iter_t *iter) {
    (void)iter;
    return 0;
}

void kv_iter_free(kv_iter_t *iter) {
    (void)iter;
}
