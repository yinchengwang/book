/**
 * @file include/db/index/skip_list/skip_list.h
 * @brief Skip List 索引公共头文件
 */
#ifndef DB_INDEX_SKIP_LIST_H
#define DB_INDEX_SKIP_LIST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 常量定义 ── */
#define SKIP_LIST_DEFAULT_MAX_LEVEL 16
#define SKIP_LIST_MIN_MAX_LEVEL       1

/* ── 前向声明 ── */
typedef struct skip_list skip_list_t;

/* ── 回调函数类型 ── */
typedef int (*skip_list_compare_fn)(const void *lhs, uint32_t lhs_len,
                                    const void *rhs, uint32_t rhs_len,
                                    void *ctx);

typedef int (*skip_list_range_callback_fn)(const void *key, uint32_t keylen,
                                           const void *value, uint32_t valuelen,
                                           void *ctx);

/* ── 生命周期 ── */
skip_list_t *skip_list_create(skip_list_compare_fn compare,
                               void *compare_ctx,
                               uint32_t max_level);
void skip_list_drop(skip_list_t *list);

/* ── 核心操作 ── */
int skip_list_insert(skip_list_t *list,
                     const void *key, uint32_t keylen,
                     const void *value, uint32_t valuelen);
int skip_list_delete(skip_list_t *list,
                     const void *key, uint32_t keylen);
int skip_list_search(const skip_list_t *list,
                     const void *key, uint32_t keylen,
                     const void **value_out, uint32_t *valuelen_out);

/* ── 范围查询 ── */
int skip_list_range(skip_list_t *list,
                    const void *min_key, uint32_t min_keylen,
                    const void *max_key, uint32_t max_keylen,
                    skip_list_range_callback_fn callback,
                    void *ctx);

/* ── 统计信息 ── */
uint32_t skip_list_size(const skip_list_t *list);
uint32_t skip_list_level(const skip_list_t *list);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_SKIP_LIST_H */
