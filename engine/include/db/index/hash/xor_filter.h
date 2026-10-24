/**
 * @file include/db/index/hash/xor_filter.h
 * @brief Xor Filter 公共头文件
 */
#ifndef DB_INDEX_XOR_FILTER_H
#define DB_INDEX_XOR_FILTER_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XOR_FILTER_FP_SIZE     3   /* 指纹大小（字节），24 位 */
#define XOR_FILTER_ARRAY_COUNT 3   /* 数组数量 */

typedef struct xor_filter xor_filter_t;

typedef struct {
    size_t expected_items; /* 预期元素数量 */
} xor_filter_config_t;

/* ── 生命周期 ── */
xor_filter_t *xor_filter_create(const xor_filter_config_t *config);
void xor_filter_destroy(xor_filter_t *filter);

/* ── 核心操作 ── */
int xor_filter_add(xor_filter_t *filter, const void *key, size_t keylen);
bool xor_filter_query(const xor_filter_t *filter, const void *key, size_t keylen);
int xor_filter_build(xor_filter_t *filter);
bool xor_filter_is_built(const xor_filter_t *filter);

/* ── 工具函数 ── */
size_t xor_filter_get_size(const xor_filter_t *filter);
size_t xor_filter_get_item_count(const xor_filter_t *filter);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_XOR_FILTER_H */