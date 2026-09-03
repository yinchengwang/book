/**
 * @file include/db/index/hash/cuckoo.h
 * @brief Cuckoo Filter 公共头文件
 */
#ifndef DB_INDEX_CUCKOO_H
#define DB_INDEX_CUCKOO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CUCKOO_BUCKET_SIZE  2   /* 每个桶的槽位数（2 路） */
#define CUCKOO_FP_SIZE      1   /* 指纹大小（字节） */
#define CUCKOO_MAX_KICK     500 /* 最大踢出次数 */

typedef struct {
    uint8_t fingerprint;
    bool    occupied;
} cuckoo_slot_t;

typedef struct {
    cuckoo_slot_t slots[CUCKOO_BUCKET_SIZE];
} cuckoo_bucket_t;

typedef struct cuckoo_filter cuckoo_filter_t;

typedef struct {
    size_t expected_items;
} cuckoo_config_t;

/* ── 生命周期 ── */
cuckoo_filter_t *cuckoo_create(const cuckoo_config_t *config);
void cuckoo_destroy(cuckoo_filter_t *filter);

/* ── 核心操作 ── */
int cuckoo_add(cuckoo_filter_t *filter, const void *key, size_t keylen);
bool cuckoo_query(const cuckoo_filter_t *filter, const void *key, size_t keylen);
int cuckoo_delete(cuckoo_filter_t *filter, const void *key, size_t keylen);

/* ── 工具函数 ── */
size_t cuckoo_get_item_count(const cuckoo_filter_t *filter);
size_t cuckoo_get_bucket_count(const cuckoo_filter_t *filter);
double cuckoo_get_load_factor(const cuckoo_filter_t *filter);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_CUCKOO_H */