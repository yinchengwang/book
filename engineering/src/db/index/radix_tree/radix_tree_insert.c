/*
 * radix_tree_insert.c
 *
 * Radix Tree 插入操作。
 *
 * 简化设计：每个节点存储一个字符，不压缩前缀。
 */

#include "radix_tree_private.h"
#include <stdlib.h>
#include <string.h>

int radix_tree_insert(radix_tree_t *tree,
                      const char *key, uint32_t keylen,
                      const void *value, uint32_t valuelen)
{
    radix_node_t *node = tree->root;
    uint32_t i;
    void *value_copy = NULL;

    if (!tree || !key || keylen == 0) return -1;

    /* 复制 value。 */
    if (value && valuelen > 0) {
        value_copy = malloc(valuelen);
        if (!value_copy) return -1;
        memcpy(value_copy, value, valuelen);
    }

    /* 遍历每个字符。 */
    for (i = 0; i < keylen; i++) {
        uint8_t c = (uint8_t)key[i];

        /* 查找子节点。 */
        radix_node_t *child = _radix_find_child(node, (char)c);

        if (!child) {
            /* 没有子节点，创建新节点。 */
            child = _radix_node_create(NULL, 0);
            if (!child) {
                free(value_copy);
                return -1;
            }
            child->prefix = (char *)malloc(1);
            if (!child->prefix) {
                free(child);
                free(value_copy);
                return -1;
            }
            child->prefix[0] = (char)c;
            child->prefix_len = 1;
            _radix_set_child(node, (char)c, child);
        }

        node = child;
    }

    /* 到达叶子，设置 value。 */
    if (node->is_end) {
        /* key 已存在，更新 value。 */
        free(node->value);
        node->value = value_copy;
        node->valuelen = valuelen;
    } else {
        node->is_end = true;
        node->value = value_copy;
        node->valuelen = valuelen;
        tree->size++;
    }

    return 0;
}