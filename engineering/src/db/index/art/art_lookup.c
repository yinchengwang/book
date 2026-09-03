/*
 * art_lookup.c
 *
 * ART 查找操作。
 */

#include "art_private.h"
#include <stdlib.h>
#include <string.h>

int art_search(const art_tree_t *tree,
               const uint8_t *key, uint32_t keylen,
               const void **value_out, uint32_t *valuelen_out)
{
    art_node_t *node;
    uint32_t depth = 0;

    if (!tree || !key || keylen == 0) return -1;

    node = tree->root;
    while (node) {
        /* 检查是否是叶子节点。 */
        if (node->is_leaf) {
            art_leaf_t *leaf = (art_leaf_t *)node;
            if (leaf->keylen == keylen && memcmp(leaf->key, key, keylen) == 0) {
                if (value_out) *value_out = leaf->value;
                if (valuelen_out) *valuelen_out = leaf->valuelen;
                return 0;
            }
            return -1;
        }

        /* 检查前缀。 */
        if (node->prefix_len > 0) {
            if (depth + node->prefix_len > keylen) return -1;
            if (memcmp(node->prefix, key + depth, node->prefix_len) != 0) {
                return -1;
            }
            depth += node->prefix_len;
        }

        if (depth >= keylen) return -1;

        /* 查找子节点并继续。 */
        node = _art_find_child(node, key[depth]);
        depth++;
    }

    return -1;
}

int art_range(art_tree_t *tree,
              const uint8_t *min_key, uint32_t min_keylen,
              const uint8_t *max_key, uint32_t max_keylen,
              art_callback_fn callback, void *ctx)
{
    if (!tree) return -1;

    /* 使用递归遍历所有叶子。 */
    return _art_collect_leaves(tree->root, min_key, min_keylen,
                               max_key, max_keylen, callback, ctx);
}

/* 递归收集在范围内的叶子。 */
int _art_collect_leaves(art_node_t *node,
                        const uint8_t *min_key, uint32_t min_keylen,
                        const uint8_t *max_key, uint32_t max_keylen,
                        art_callback_fn callback, void *ctx)
{
    uint32_t i;
    int count = 0;

    if (!node) return 0;

    /* 检查是否是叶子节点。 */
    if (node->is_leaf) {
        art_leaf_t *leaf = (art_leaf_t *)node;
        /* 检查是否在范围内。 */
        /* 如果 leaf 太短，跳过 */
        if (leaf->keylen < min_keylen) return 0;

        int cmp_min = memcmp(leaf->key, min_key, min_keylen);
        int cmp_max = memcmp(leaf->key, max_key, max_keylen);

        if (cmp_min >= 0 && cmp_max <= 0) {
            if (callback) callback(leaf->key, leaf->keylen, leaf->value, leaf->valuelen, ctx);
            return 1;
        }
        return 0;
    }

    /* 递归遍历所有子节点。 */
    switch (node->type) {
    case ART_N4: {
        art_n4_t *n4 = (art_n4_t *)node;
        for (i = 0; i < n4->node.count; i++) {
            count += _art_collect_leaves(n4->children[i], min_key, min_keylen,
                                         max_key, max_keylen, callback, ctx);
        }
        break;
    }
    case ART_N16: {
        art_n16_t *n16 = (art_n16_t *)node;
        for (i = 0; i < n16->node.count; i++) {
            count += _art_collect_leaves(n16->children[i], min_key, min_keylen,
                                         max_key, max_keylen, callback, ctx);
        }
        break;
    }
    case ART_N48: {
        art_n48_t *n48 = (art_n48_t *)node;
        for (i = 0; i < 256; i++) {
            if (n48->children[i]) {
                count += _art_collect_leaves(n48->children[i], min_key, min_keylen,
                                             max_key, max_keylen, callback, ctx);
            }
        }
        break;
    }
    case ART_N256: {
        art_n256_t *n256 = (art_n256_t *)node;
        for (i = 0; i < 256; i++) {
            if (n256->children[i]) {
                count += _art_collect_leaves(n256->children[i], min_key, min_keylen,
                                             max_key, max_keylen, callback, ctx);
            }
        }
        break;
    }
    }

    return count;
}
