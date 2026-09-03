/*
 * art_delete.c
 *
 * ART 删除操作。
 */

#include "art_private.h"
#include <stdlib.h>
#include <string.h>

/* 删除节点。 */
int _art_delete_node(art_tree_t *tree, art_node_t **node,
                             const uint8_t *key, uint32_t keylen, uint32_t depth)
{
    art_node_t *n = *node;
    uint32_t i;

    if (!n) return -1;

    /* 检查是否是叶子节点。 */
    if (n->is_leaf) {
        art_leaf_t *leaf = (art_leaf_t *)n;
        if (leaf->keylen == keylen && memcmp(leaf->key, key, keylen) == 0) {
            free(leaf->key);
            free(leaf->value);
            free(leaf);
            *node = NULL;
            tree->size--;
            return 0;
        }
        return -1;
    }

    /* 检查前缀。 */
    if (n->prefix_len > 0 && depth < keylen) {
        uint32_t match_len = (n->prefix_len < (keylen - depth)) ? n->prefix_len : (keylen - depth);
        if (memcmp(n->prefix, key + depth, match_len) != 0) {
            return -1;
        }
        depth += n->prefix_len;
    }

    if (depth >= keylen) return -1;

    /* 查找子节点。 */
    art_node_t *child = _art_find_child(n, key[depth]);
    if (!child) return -1;

    /* 递归删除。 */
    int result = _art_delete_node(tree, &child, key, keylen, depth + 1);

    /* 如果子节点被删除，更新父节点。 */
    if (result == 0 && child == NULL) {
        uint8_t k = key[depth];

        /* 从父节点移除子节点。 */
        switch (n->type) {
        case ART_N4: {
            art_n4_t *n4 = (art_n4_t *)n;
            for (i = 0; i < n4->node.count; i++) {
                if (n4->keys[i] == k) {
                    /* 移动后面的元素。 */
                    for (; i < n4->node.count - 1; i++) {
                        n4->keys[i] = n4->keys[i + 1];
                        n4->children[i] = n4->children[i + 1];
                    }
                    n4->node.count--;
                    break;
                }
            }
            break;
        }
        case ART_N16: {
            art_n16_t *n16 = (art_n16_t *)n;
            for (i = 0; i < n16->node.count; i++) {
                if (n16->keys[i] == k) {
                    for (; i < n16->node.count - 1; i++) {
                        n16->keys[i] = n16->keys[i + 1];
                        n16->children[i] = n16->children[i + 1];
                    }
                    n16->node.count--;
                    break;
                }
            }
            break;
        }
        case ART_N48:
            n->count--;
            break;
        case ART_N256:
            n->count--;
            break;
        }
    }

    return result;
}

int art_delete(art_tree_t *tree,
               const uint8_t *key, uint32_t keylen)
{
    if (!tree || !key || keylen == 0) return -1;
    return _art_delete_node(tree, &tree->root, key, keylen, 0);
}
