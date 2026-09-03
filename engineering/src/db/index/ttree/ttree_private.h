/*
 * ttree_private.h
 *
 * T-Tree 内部结构。
 * 采用简化设计：类似 B+Tree，但用 left/right 二叉指针导航。
 */

#ifndef DB_INDEX_TTREE_PRIVATE_H
#define DB_INDEX_TTREE_PRIVATE_H

#include <db/index/ttree/ttree.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TTREE_DEFAULT_MIN_KEYS 4u
#define TTREE_MIN_MIN_KEYS 2u

typedef struct ttree_record {
    void *key;
    uint32_t keylen;
    void *value;
    uint32_t valuelen;
} ttree_record_t;

/*
 * T-Tree 节点。
 * 简化实现：每个节点存储 [min_keys, 2*min_keys-1] 个有序 key。
 * 内部节点：left/right 二叉指针导航。
 * 叶子节点：prev/next 链表支持范围查询。
 */
typedef struct ttree_node {
    bool is_leaf;
    uint32_t nkeys;
    ttree_record_t *records;     /* 有序 key-value 数组。 */
    struct ttree_node *left;
    struct ttree_node *right;
    struct ttree_node *prev;     /* 叶子链表前驱。 */
    struct ttree_node *next;     /* 叶子链表后继。 */
} ttree_node_t;

struct ttree_index {
    uint32_t min_keys;
    uint32_t max_keys;
    uint32_t size;
    ttree_compare_fn compare;
    void *compare_ctx;
    ttree_node_t *root;
};

/* === 核心函数 === */
int _ttree_compare(const ttree_index_t *index,
                   const void *lhs, uint32_t lhs_len,
                   const void *rhs, uint32_t rhs_len);

ttree_node_t *_ttree_node_create(const ttree_index_t *index);
void _ttree_node_drop(ttree_node_t *node);
void _ttree_node_free_shallow(ttree_node_t *node);

void _ttree_record_clear(ttree_record_t *record);
int _ttree_record_set(ttree_record_t *record,
                      const void *key, uint32_t keylen,
                      const void *value, uint32_t valuelen);
int _ttree_record_clone(ttree_record_t *dest, const ttree_record_t *src);

uint32_t _ttree_compute_height(const ttree_index_t *index);

/* === 查找 === */
uint32_t _ttree_lower_bound(const ttree_index_t *index,
                            const ttree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *matched);

int _ttree_find_in_node(const ttree_index_t *index,
                        const ttree_node_t *node,
                        const void *key, uint32_t keylen);

const ttree_record_t *_ttree_find_record_const(const ttree_index_t *index,
                                               const ttree_node_t *node,
                                               const void *key, uint32_t keylen);

/* === 插入 === */
int _ttree_insert_into_node(ttree_index_t *index,
                            ttree_node_t *node,
                            const void *key, uint32_t keylen,
                            const void *value, uint32_t valuelen);

int _ttree_split_leaf(ttree_index_t *index,
                      ttree_node_t *left,
                      ttree_node_t **right_out);

int _ttree_split_internal(ttree_index_t *index,
                          ttree_node_t *node,
                          ttree_record_t *up_record,
                          ttree_node_t **right_out);

ttree_node_t *_ttree_find_leaf_for_insert(ttree_index_t *index,
                                          ttree_node_t *node,
                                          const void *key, uint32_t keylen);

/* === 删除 === */
bool _ttree_node_underflow(const ttree_index_t *index, const ttree_node_t *node);

int _ttree_borrow_from_right(ttree_index_t *index,
                             ttree_node_t *node,
                             ttree_node_t *right);

int _ttree_borrow_from_left(ttree_index_t *index,
                            ttree_node_t *node,
                            ttree_node_t *left);

int _ttree_merge_with_right(ttree_index_t *index,
                            ttree_node_t *node,
                            ttree_node_t *right);

int _ttree_delete_recursive(ttree_index_t *index,
                            ttree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *deleted);

#endif /* DB_INDEX_TTREE_PRIVATE_H */
