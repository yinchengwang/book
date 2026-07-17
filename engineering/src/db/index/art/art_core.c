/*
 * art_core.c
 *
 * ART (Adaptive Radix Tree) 核心操作。
 */

#include "art_private.h"
#include <stdlib.h>
#include <string.h>

/* 查找子节点。 */
art_node_t *_art_find_child(const art_node_t *node, uint8_t key)
{
    uint32_t i;

    switch (node->type) {
    case ART_N4: {
        art_n4_t *n4 = (art_n4_t *)node;
        for (i = 0; i < n4->node.count; i++) {
            if (n4->keys[i] == key) return n4->children[i];
        }
        break;
    }
    case ART_N16: {
        art_n16_t *n16 = (art_n16_t *)node;
        for (i = 0; i < n16->node.count; i++) {
            if (n16->keys[i] == key) return n16->children[i];
        }
        break;
    }
    case ART_N48: {
        art_n48_t *n48 = (art_n48_t *)node;
        return n48->children[key];
    }
    case ART_N256: {
        art_n256_t *n256 = (art_n256_t *)node;
        return n256->children[key];
    }
    }
    return NULL;
}

art_node_t *_art_create_node(art_node_type_t type)
{
    art_node_t *node = NULL;

    switch (type) {
    case ART_N4: {
        art_n4_t *n4 = (art_n4_t *)calloc(1, sizeof(art_n4_t));
        if (n4) {
            n4->node.type = ART_N4;
            node = &n4->node;
        }
        break;
    }
    case ART_N16: {
        art_n16_t *n16 = (art_n16_t *)calloc(1, sizeof(art_n16_t));
        if (n16) {
            n16->node.type = ART_N16;
            node = &n16->node;
        }
        break;
    }
    case ART_N48: {
        art_n48_t *n48 = (art_n48_t *)calloc(1, sizeof(art_n48_t));
        if (n48) {
            n48->node.type = ART_N48;
            node = &n48->node;
        }
        break;
    }
    case ART_N256: {
        art_n256_t *n256 = (art_n256_t *)calloc(1, sizeof(art_n256_t));
        if (n256) {
            n256->node.type = ART_N256;
            node = &n256->node;
        }
        break;
    }
    }

    return node;
}

void _art_node_drop(art_node_t *node)
{
    uint32_t i;

    if (!node) return;

    /* 检查是否是叶子节点。 */
    if (node->is_leaf) {
        art_leaf_t *leaf = (art_leaf_t *)node;
        free(leaf->key);
        free(leaf->value);
        free(leaf);
        return;
    }

    switch (node->type) {
    case ART_N4: {
        art_n4_t *n4 = (art_n4_t *)node;
        for (i = 0; i < n4->node.count; i++) {
            if (n4->children[i]) {
                _art_node_drop(n4->children[i]);
            }
        }
        break;
    }
    case ART_N16: {
        art_n16_t *n16 = (art_n16_t *)node;
        for (i = 0; i < n16->node.count; i++) {
            if (n16->children[i]) {
                _art_node_drop(n16->children[i]);
            }
        }
        break;
    }
    case ART_N48: {
        art_n48_t *n48 = (art_n48_t *)node;
        for (i = 0; i < 256; i++) {
            if (n48->children[i]) {
                _art_node_drop(n48->children[i]);
            }
        }
        break;
    }
    case ART_N256: {
        art_n256_t *n256 = (art_n256_t *)node;
        for (i = 0; i < 256; i++) {
            if (n256->children[i]) {
                _art_node_drop(n256->children[i]);
            }
        }
        break;
    }
    }

    free(node->prefix);
    free(node);
}

art_tree_t *art_create(void)
{
    art_tree_t *tree = (art_tree_t *)calloc(1, sizeof(art_tree_t));
    return tree;
}

void art_destroy(art_tree_t *tree)
{
    if (!tree) return;
    _art_node_drop(tree->root);
    free(tree);
}

uint32_t art_size(const art_tree_t *tree)
{
    return tree ? tree->size : 0;
}
