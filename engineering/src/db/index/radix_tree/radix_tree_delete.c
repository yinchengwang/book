/*
 * radix_tree_delete.c
 *
 * Radix Tree 删除操作。
 *
 * 简化设计：每个节点存储一个字符。
 */

#include "radix_tree_private.h"
#include <stdlib.h>
#include <string.h>

/* 递归删除。 */
static int _radix_delete_recursive(radix_node_t *node,
                                    const char *key, uint32_t keylen,
                                    uint32_t pos)
{
    radix_node_t *child;

    if (pos >= keylen) {
        /* 到达 key 结尾。 */
        if (node->is_end) {
            node->is_end = false;
            free(node->value);
            node->value = NULL;
            node->valuelen = 0;
            return 0;
        }
        return -1;
    }

    /* 继续向下查找。 */
    child = _radix_find_child(node, key[pos]);
    if (!child) {
        return -1;
    }

    /* 递归删除。 */
    int result = _radix_delete_recursive(child, key, keylen, pos + 1);

    return result;
}

int radix_tree_delete(radix_tree_t *tree,
                      const char *key, uint32_t keylen)
{
    if (!tree || !key || keylen == 0) return -1;
    return _radix_delete_recursive(tree->root, key, keylen, 0);
}