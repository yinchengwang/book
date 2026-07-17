/*
 * btree_private.h
 *
 * B-Tree 内部结构。
 * 与 B+Tree 不同，这里 value 直接保存在每个节点记录里，内部节点也持有 payload。
 */

#ifndef DB_INDEX_BTREE_PRIVATE_H
#define DB_INDEX_BTREE_PRIVATE_H

#include "db/index/btree/btree.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct btree_record {
    void *key;          /* key 的堆拷贝。 */
    uint32_t keylen;    /* key 长度。 */
    void *value;        /* value 的堆拷贝。 */
    uint32_t valuelen;  /* value 长度。 */
} btree_record_t;

/*
 * B-Tree 节点。
 * records 长度为 max_keys，children 长度为 max_children。
 */
typedef struct btree_node {
    bool is_leaf;                /* true 表示叶子节点。 */
    uint32_t nkeys;              /* 当前已使用的 key 数。 */
    btree_record_t *records;     /* 节点内有序记录数组。 */
    struct btree_node **children;/* 子节点指针数组。 */
} btree_node_t;

struct btree_index {
    uint32_t min_degree;         /* B-Tree 最小度 t。 */
    uint32_t max_keys;           /* 每个节点最多 2t-1 个 key。 */
    uint32_t max_children;       /* 每个节点最多 2t 个孩子。 */
    uint32_t size;               /* 全树记录数。 */
    btree_compare_fn compare;    /* key 比较函数。 */
    void *compare_ctx;           /* 比较函数上下文。 */
    btree_node_t *root;          /* 根节点。 */
};

/* 统一比较入口，若用户未提供 compare 则回退到字节序比较。 */
int _btree_compare(const btree_index_t *index,
                   const void *lhs, uint32_t lhs_len,
                   const void *rhs, uint32_t rhs_len);

btree_node_t *_btree_node_create(const btree_index_t *index, bool is_leaf);
void _btree_node_drop(const btree_index_t *index, btree_node_t *node);

void _btree_record_clear(btree_record_t *record);
void _btree_record_move(btree_record_t *dest, btree_record_t *src);
int _btree_record_set(btree_record_t *record,
                      const void *key, uint32_t keylen,
                      const void *value, uint32_t valuelen);
int _btree_record_clone(btree_record_t *dest, const btree_record_t *src);
int _btree_record_replace_value(btree_record_t *record,
                                const void *value, uint32_t valuelen);
void _btree_node_free_shallow(btree_node_t *node);

/* 在节点内做 lower_bound，返回第一个 >= key 的位置。 */
uint32_t _btree_lower_bound(const btree_index_t *index,
                            const btree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *matched);

btree_record_t *_btree_find_record(const btree_index_t *index,
                                   btree_node_t *node,
                                   const void *key, uint32_t keylen);

const btree_record_t *_btree_find_record_const(const btree_index_t *index,
                                               const btree_node_t *node,
                                               const void *key, uint32_t keylen);

/* 在一个保证未满的节点中递归插入。 */
int _btree_insert_nonfull(btree_index_t *index,
                          btree_node_t *node,
                          const void *key, uint32_t keylen,
                          const void *value, uint32_t valuelen);

/* 分裂 parent->children[child_pos]，并把中位 key 提升到 parent。 */
int _btree_split_child(btree_index_t *index,
                       btree_node_t *parent,
                       uint32_t child_pos);

/* 递归删除 key，并在必要时做借位/合并修复。 */
int _btree_delete_from_node(btree_index_t *index,
                            btree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *deleted);

uint32_t _btree_compute_height(const btree_index_t *index);

#endif /* DB_INDEX_BTREE_PRIVATE_H */
