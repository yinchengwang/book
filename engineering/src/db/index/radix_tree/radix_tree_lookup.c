/*
 * radix_tree_lookup.c
 *
 * Radix Tree 查找、前缀搜索，最长前缀匹配。
 *
 * 简化设计：每个节点存储一个字符。
 */

#include "radix_tree_private.h"
#include <stdlib.h>
#include <string.h>

int radix_tree_search(const radix_tree_t *tree,
                      const char *key, uint32_t keylen,
                      const void **value_out, uint32_t *valuelen_out)
{
    radix_node_t *node = tree->root;
    uint32_t pos;

    if (!tree || !key || keylen == 0) return -1;

    /* 遍历每个字符。 */
    for (pos = 0; pos < keylen; pos++) {
        uint8_t c = (uint8_t)key[pos];
        node = _radix_find_child(node, (char)c);
        if (!node) {
            return -1;
        }
    }

    /* 检查是否是 key 结尾。 */
    if (node && node->is_end) {
        if (value_out) *value_out = node->value;
        if (valuelen_out) *valuelen_out = node->valuelen;
        return 0;
    }

    return -1;
}

/* 递归收集以 prefix 开头的所有 key。 */
static int _radix_collect(radix_node_t *node,
                           char *prefix, uint32_t prefix_len,
                           radix_tree_callback_fn callback,
                           void *ctx)
{
    uint32_t i;
    int count = 0;

    if (!node) return 0;

    /* 如果当前节点是 key 结尾，调用回调。 */
    if (node->is_end) {
        if (callback(prefix, prefix_len, node->value, node->valuelen, ctx) != 0) {
            return count;
        }
        count++;
    }

    /* 递归遍历子节点。 */
    for (i = 0; i < 256; i++) {
        if (node->children[i]) {
            char new_prefix[1024];
            uint32_t new_len = prefix_len;

            /* 添加当前节点的前缀。 */
            if (node->prefix_len > 0) {
                memcpy(new_prefix, prefix, prefix_len);
                memcpy(new_prefix + prefix_len, node->prefix, node->prefix_len);
                new_len += node->prefix_len;
            } else {
                memcpy(new_prefix, prefix, prefix_len);
            }

            /* 添加当前字符。 */
            new_prefix[new_len++] = (char)i;

            count += _radix_collect(node->children[i],
                                   new_prefix, new_len,
                                   callback, ctx);
        }
    }

    return count;
}

int radix_tree_prefix(radix_tree_t *tree,
                      const char *prefix, uint32_t prefixlen,
                      radix_tree_callback_fn callback,
                      void *ctx)
{
    radix_node_t *node = tree->root;
    uint32_t pos;

    if (!tree || !callback) return -1;

    /* 如果 prefix 为空，返回所有 key。 */
    if (!prefix || prefixlen == 0) {
        return _radix_collect(node, NULL, 0, callback, ctx);
    }

    /* 找到 prefix 对应的节点。 */
    for (pos = 0; pos < prefixlen; pos++) {
        uint8_t c = (uint8_t)prefix[pos];
        node = _radix_find_child(node, (char)c);
        if (!node) {
            return 0;
        }
    }

    /* prefix 匹配完成，收集以此开头的所有 key。 */
    return _radix_collect(node, NULL, 0, callback, ctx);
}

uint32_t radix_tree_longest_prefix(const radix_tree_t *tree,
                                    const char *key, uint32_t keylen,
                                    char *matched_key, uint32_t *matched_len)
{
    radix_node_t *node = tree->root;
    uint32_t pos;
    uint32_t longest = 0;

    if (!tree || !key || keylen == 0) return 0;

    /* 遍历每个字符。 */
    for (pos = 0; pos < keylen; pos++) {
        uint8_t c = (uint8_t)key[pos];

        /* 记录当前匹配位置（如果这是 key 结尾）。 */
        if (node->is_end) {
            longest = pos;
        }

        node = _radix_find_child(node, (char)c);
        if (!node) {
            break;
        }
    }

    /* 检查最后一个节点。 */
    if (node && node->is_end) {
        longest = pos;
    }

    if (matched_key && matched_len) {
        *matched_len = longest;
        if (longest > 0 && longest <= keylen) {
            memcpy(matched_key, key, longest);
        }
    }

    return longest;
}