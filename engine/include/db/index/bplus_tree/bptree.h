/**
 * @file include/db/index/bplus_tree/bptree.h
 * @brief B+Tree 索引公共头文件
 */
#ifndef DB_INDEX_BPTREE_H
#define DB_INDEX_BPTREE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 常量定义 ── */
#define BPTREE_DEFAULT_ORDER  64   /* 默认树阶 */
#define BPTREE_MIN_ORDER       3   /* 最小树阶 */

/* ── 前向声明 ── */
typedef struct bptree_index bptree_index_t;

/* ── 回调函数类型 ── */
typedef int (*bptree_compare_fn)(const void *lhs, size_t lhs_len,
                                  const void *rhs, size_t rhs_len,
                                  void *ctx);

typedef int (*bptree_scan_visit_fn)(const void *key, uint32_t keylen,
                                    const void *value, uint32_t valuelen,
                                    void *ctx);

/* ── 迭代器 ── */
typedef struct bptree_iter {
    bptree_index_t *index;
    void *key;
    uint32_t keylen;
    void *value;
    uint32_t valuelen;
    void *reserved;
} bptree_iter_t;

/* ── 生命周期 ── */
bptree_index_t *bptree_create(uint32_t order, bptree_compare_fn compare, void *ctx);
void bptree_destroy(bptree_index_t *index);

/* ── 核心操作 ── */
int bptree_insert(bptree_index_t *index,
                  const void *key, uint32_t keylen,
                  const void *value, uint32_t valuelen);
int bptree_delete(bptree_index_t *index,
                  const void *key, uint32_t keylen);
int bptree_lookup(const bptree_index_t *index,
                  const void *key, uint32_t keylen,
                  void **value_out, uint32_t *valuelen_out);

/* ── 范围查询 ── */
bptree_iter_t *bptree_iter_create(const bptree_index_t *index,
                                   const void *start_key, uint32_t start_keylen);
void bptree_iter_destroy(bptree_iter_t *iter);
bool bptree_iter_next(bptree_iter_t *iter);
void *bptree_iter_key(bptree_iter_t *iter, uint32_t *keylen_out);
void *bptree_iter_value(bptree_iter_t *iter, uint32_t *valuelen_out);

/* ── 统计信息 ── */
uint32_t bptree_size(const bptree_index_t *index);
uint32_t bptree_height(const bptree_index_t *index);
uint32_t bptree_leaf_count(const bptree_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_BPTREE_H */