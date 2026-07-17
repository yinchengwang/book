/*
 * art_private.h
 *
 * ART (Adaptive Radix Tree) 内部结构。
 */

#ifndef DB_INDEX_ART_PRIVATE_H
#define DB_INDEX_ART_PRIVATE_H

#include "db/index/art/art.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ART 节点类型。 */
typedef enum {
    ART_N4 = 0,
    ART_N16,
    ART_N48,
    ART_N256
} art_node_type_t;

/* ART 节点头。 */
typedef struct art_node {
    bool is_leaf;           /* 是否是叶子节点 */
    art_node_type_t type;   /* 节点类型（仅内部节点使用） */
    uint32_t count;         /* 子节点数量 */
    uint32_t prefix_len;
    uint8_t *prefix;
} art_node_t;

/* N4: 4 个子节点。 */
typedef struct {
    art_node_t node;
    uint8_t keys[4];
    struct art_node *children[4];
} art_n4_t;

/* N16: 16 个子节点。 */
typedef struct {
    art_node_t node;
    uint8_t keys[16];
    struct art_node *children[16];
} art_n16_t;

/* N48: 48 个子节点（256 空间中使用 48 个）。 */
typedef struct {
    art_node_t node;
    struct art_node *children[256];
    uint8_t indexes[256];  /* 子节点索引 */
} art_n48_t;

/* N256: 256 个子节点。 */
typedef struct {
    art_node_t node;
    struct art_node *children[256];
} art_n256_t;

/* ART 叶子节点。必须与 art_node_t 的前几个字段布局兼容！ */
typedef struct art_leaf {
    bool is_leaf;       /* 必须是第一个字段，与 art_node_t 兼容 */
    /* 以下是叶子特有字段 */
    uint8_t *key;
    uint32_t keylen;
    void *value;
    uint32_t valuelen;
} art_leaf_t;

struct art_tree {
    art_node_t *root;
    uint32_t size;
};

/* 辅助函数。 */
void _art_node_drop(art_node_t *node);
art_node_t *_art_create_node(art_node_type_t type);
int _art_insert_node(art_tree_t *tree, art_node_t **node,
                     const uint8_t *key, uint32_t keylen,
                     const void *value, uint32_t valuelen,
                     uint32_t depth);
int _art_delete_node(art_tree_t *tree, art_node_t **node,
                     const uint8_t *key, uint32_t keylen,
                     uint32_t depth);
art_node_t *_art_find_child(const art_node_t *node, uint8_t key);
int _art_collect_leaves(art_node_t *node,
                        const uint8_t *min_key, uint32_t min_keylen,
                        const uint8_t *max_key, uint32_t max_keylen,
                        art_callback_fn callback, void *ctx);

#endif /* DB_INDEX_ART_PRIVATE_H */
