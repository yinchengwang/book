/*
 * btree.h
 *
 * B-Tree 索引公共 API。
 *
 * 特性：
 *   - value 与 key 一起保存在每个节点
 *   - 溢出时分裂，提升中位数 key
 *   - 比较函数可自定义，NULL 则使用字节序比较
 */

#ifndef DB_INDEX_BTREE_H
#define DB_INDEX_BTREE_H

#include <db/index/shared/tree_page.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BTREE_DEFAULT_MIN_DEGREE 16u
#define BTREE_MIN_MIN_DEGREE 2u

typedef int (*btree_compare_fn)(const void *lhs, uint32_t lhs_len,
                                const void *rhs, uint32_t rhs_len,
                                void *ctx);

typedef struct btree_index btree_index_t;

btree_index_t *btree_index_create(uint32_t min_degree,
                                  btree_compare_fn compare,
                                  void *compare_ctx);

void btree_index_drop(btree_index_t *index);

int btree_index_insert(btree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen);

int btree_index_upsert(btree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen);

int btree_index_delete(btree_index_t *index,
                       const void *key, uint32_t keylen);

int btree_index_lookup(const btree_index_t *index,
                       const void *key, uint32_t keylen,
                       void **value_out, uint32_t *valuelen_out);

int btree_index_save(const btree_index_t *index,
                     const char *path,
                     const tree_index_persist_options_t *options);

btree_index_t *btree_index_load(const char *path,
                                btree_compare_fn compare,
                                void *compare_ctx);

uint32_t btree_index_size(const btree_index_t *index);
uint32_t btree_index_height(const btree_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_BTREE_H */
