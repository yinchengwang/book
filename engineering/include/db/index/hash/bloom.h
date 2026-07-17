/**
 * @file include/db/index/hash/bloom.h
 * @brief Bloom Filter 公共头文件
 */
#ifndef DB_INDEX_BLOOM_H
#define DB_INDEX_BLOOM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 配置与数据结构 ── */
typedef struct bloom_config {
    size_t expected_items;       /* 预期元素数量 */
    double false_positive_rate;  /* 期望假阳性率 (默认 0.01) */
} bloom_config_t;

typedef struct bloom_filter bloom_filter_t;

/* ── 生命周期 ── */
bloom_filter_t *bloom_create(const bloom_config_t *config);
void bloom_destroy(bloom_filter_t *filter);

/* ── 核心操作 ── */
int bloom_add(bloom_filter_t *filter, const void *key, size_t keylen);
bool bloom_query(const bloom_filter_t *filter, const void *key, size_t keylen);

/* ── 工具函数 ── */
size_t bloom_get_size(const bloom_filter_t *filter);
size_t bloom_get_hash_count(const bloom_filter_t *filter);
size_t bloom_get_item_count(const bloom_filter_t *filter);
size_t bloom_get_memory_usage(const bloom_filter_t *filter);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_BLOOM_H */