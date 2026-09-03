/*
 * bptree_private.h
 *
 * B+Tree 内部结构。
 * 所有 value 只存放在叶子节点，内部节点只保留分隔键，叶子之间通过 next_leaf 串联。
 */

#ifndef BPTREE_PRIVATE_H
#define BPTREE_PRIVATE_H

#include <db/index/bplus_tree/bptree.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct bptree_record {
    void *key;          /* key 的堆拷贝。 */
    uint32_t keylen;    /* key 长度。 */
    void *value;        /* 仅叶子节点使用。 */
    uint32_t valuelen;  /* value 长度。 */
} bptree_record_t;

/*
 * B+Tree 节点。
 * 内部节点的 records[i] 是 children[i + 1] 的分隔键；叶子节点保存真实记录。
 */
typedef struct bptree_node {
    bool is_leaf;                 /* true 表示叶子。 */
    uint32_t nkeys;               /* 当前 key 数。 */
    bptree_record_t *records;     /* 有序记录/分隔键数组。 */
    struct bptree_node **children;/* 内部节点子指针数组。 */
    struct bptree_node *next_leaf;/* 叶子链表的后继节点。 */
} bptree_node_t;

struct bptree_index {
    uint32_t order;               /* 树阶，决定节点容量。 */
    uint32_t max_keys;            /* 节点最大 key 数。 */
    uint32_t max_children;        /* 节点最大孩子数。 */
    uint32_t size;                /* 记录总数。 */
    uint32_t leaf_count;          /* 当前叶子节点数量。 */
    bptree_compare_fn compare;    /* key 比较函数。 */
    void *compare_ctx;            /* 比较函数上下文。 */
    bptree_node_t *root;          /* 根节点。 */
};

int _bptree_compare(const bptree_index_t *index,
                    const void *lhs, uint32_t lhs_len,
                    const void *rhs, uint32_t rhs_len);

bptree_node_t *_bptree_node_create(const bptree_index_t *index, bool is_leaf);
void _bptree_node_drop(bptree_node_t *node);

void _bptree_record_clear(bptree_record_t *record);
void _bptree_record_move(bptree_record_t *dest, bptree_record_t *src);
int _bptree_record_set(bptree_record_t *record,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen);
int _bptree_record_clone(bptree_record_t *dest, const bptree_record_t *src);
int _bptree_record_replace_value(bptree_record_t *record,
                                 const void *value, uint32_t valuelen);
void _bptree_node_free_shallow(bptree_node_t *node);

/* 在叶子节点内做 lower_bound，返回第一个 >= key 的位置。 */
uint32_t _bptree_leaf_lower_bound(const bptree_index_t *index,
                                  const bptree_node_t *node,
                                  const void *key, uint32_t keylen,
                                  bool *matched);

/* 在内部节点中决定 key 应下探到哪个 child。 */
uint32_t _bptree_internal_child_pos(const bptree_index_t *index,
                                    const bptree_node_t *node,
                                    const void *key, uint32_t keylen);

bptree_node_t *_bptree_find_leaf(const bptree_index_t *index,
                                 const void *key, uint32_t keylen);

const bptree_record_t *_bptree_find_record(const bptree_index_t *index,
                                           const void *key, uint32_t keylen,
                                           bptree_node_t **leaf_out,
                                           uint32_t *pos_out);

/*
 * 递归插入。
 * 当子树分裂时，通过 promoted/right_child 向上传递新的分隔键和右兄弟。
 */
int _bptree_insert_recursive(bptree_index_t *index,
                             bptree_node_t *node,
                             const void *key, uint32_t keylen,
                             const void *value, uint32_t valuelen,
                             bptree_record_t *promoted,
                             bptree_node_t **right_child,
                             bool *inserted_new_key);

/* 递归删除，并在必要时维护最小占用约束。 */
int _bptree_delete_recursive(bptree_index_t *index,
                             bptree_node_t *node,
                             const void *key, uint32_t keylen,
                             bool *deleted);

/* 当某个 child 的首键变化时，刷新父节点中的分隔键。 */
int _bptree_refresh_separator_from_child(bptree_node_t *parent,
                                         uint32_t child_pos,
                                         const bptree_index_t *index);

/* 返回某棵子树最左侧记录，用于更新分隔键。 */
const bptree_record_t *_bptree_leftmost_record(const bptree_node_t *node);

uint32_t _bptree_compute_height(const bptree_index_t *index);

uint32_t _bptree_min_leaf_keys(const bptree_index_t *index);
uint32_t _bptree_min_internal_keys(const bptree_index_t *index);


#endif /* BPTREE_PRIVATE_H */