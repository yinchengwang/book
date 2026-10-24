/*
 * radix_tree_core.c
 *
 * Radix Tree 核心操作：创建、销毁、节点管理。
 */

#include "radix_tree_private.h"
#include <stdlib.h>
#include <string.h>

radix_node_t *_radix_node_create(const char *prefix, uint32_t prefix_len)
{
    radix_node_t *node;

    node = (radix_node_t *)calloc(1, sizeof(radix_node_t));
    if (!node) return NULL;

    if (prefix && prefix_len > 0) {
        node->prefix = (char *)malloc(prefix_len);
        if (!node->prefix) {
            free(node);
            return NULL;
        }
        memcpy(node->prefix, prefix, prefix_len);
        node->prefix_len = prefix_len;
    }

    /* 子节点数组，256 是 ASCII 字符范围。 */
    node->children = (radix_node_t **)calloc(256, sizeof(radix_node_t *));
    if (!node->children) {
        free(node->prefix);
        free(node);
        return NULL;
    }

    node->child_count = 0;
    node->first_child = 255;  /* 无效值 */

    return node;
}

void _radix_node_drop(radix_node_t *node)
{
    uint32_t i;

    if (!node) return;

    free(node->prefix);
    free(node->value);

    for (i = 0; i < 256; i++) {
        if (node->children[i]) {
            _radix_node_drop(node->children[i]);
        }
    }

    free(node->children);
    free(node);
}

radix_tree_t *radix_tree_create(void)
{
    radix_tree_t *tree;

    tree = (radix_tree_t *)calloc(1, sizeof(radix_tree_t));
    if (!tree) return NULL;

    tree->root = _radix_node_create(NULL, 0);
    if (!tree->root) {
        free(tree);
        return NULL;
    }

    tree->size = 0;
    return tree;
}

void radix_tree_drop(radix_tree_t *tree)
{
    if (!tree) return;

    _radix_node_drop(tree->root);
    free(tree);
}

uint32_t radix_tree_size(const radix_tree_t *tree)
{
    return tree ? tree->size : 0;
}

/* 查找指定字符的子节点。 */
radix_node_t *_radix_find_child(radix_node_t *node, char c)
{
    return node->children[(uint8_t)c];
}

/* 设置子节点。 */
void _radix_set_child(radix_node_t *node, char c, radix_node_t *child)
{
    uint8_t idx = (uint8_t)c;
    if (!node->children[idx] && child) {
        node->child_count++;
        if (node->first_child == 255 || idx < node->first_child) {
            node->first_child = idx;
        }
    }
    node->children[idx] = child;
}

/*
 * 查找 key。
 * 返回最后匹配的节点和匹配的字符数。
 */
radix_node_t *_radix_tree_find_node(radix_tree_t *tree,
                                     const char *key, uint32_t keylen,
                                     uint32_t *matched_len)
{
    radix_node_t *node = tree->root;
    radix_node_t *child;
    uint32_t pos = 0;
    uint32_t node_pos = 0;
    uint32_t total_matched = 0;

    (void)node_pos;

    while (pos < keylen && node) {
        child = NULL;

        /* 尝试匹配当前节点的前缀。 */
        if (node->prefix_len > 0) {
            uint32_t match_len = 0;
            uint32_t i;

            for (i = 0; i < node->prefix_len && pos + i < keylen; i++) {
                if (node->prefix[i] == key[pos + i]) {
                    match_len++;
                } else {
                    break;
                }
            }

            if (match_len > 0) {
                total_matched += match_len;
                pos += match_len;

                if (match_len < node->prefix_len) {
                    /* 前缀部分匹配，还没到叶子。 */
                    *matched_len = total_matched;
                    return node;
                }

                /* 前缀完全匹配，继续。 */
                if (pos >= keylen) {
                    /* 刚好匹配完，key 可能在当前节点结束。 */
                    *matched_len = total_matched;
                    return node;
                }
            }
        }

        /* 前缀匹配完了，看下一个字符的子节点。 */
        if (pos < keylen) {
            child = _radix_find_child(node, key[pos]);
            if (child) {
                node = child;
            } else {
                break;
            }
        }
    }

    *matched_len = total_matched;
    return node;
}

/*
 * 分裂节点：在 prefix 的 split_pos 位置分裂。
 * 原节点保留 [0, split_pos)，新节点持有 [split_pos, end)。
 */
int _radix_tree_split_node(radix_node_t *node, uint32_t split_pos,
                           radix_node_t **new_child_out)
{
    radix_node_t *new_child;
    uint32_t i;

    if (split_pos >= node->prefix_len) {
        return -1;  /* 无效位置 */
    }

    new_child = _radix_node_create(node->prefix + split_pos,
                                    node->prefix_len - split_pos);
    if (!new_child) return -1;

    /* 复制 is_end 和 value。 */
    new_child->is_end = node->is_end;
    new_child->value = node->value;
    new_child->valuelen = node->valuelen;
    node->value = NULL;
    node->valuelen = 0;
    node->is_end = false;

    /* 移动子节点。 */
    for (i = 0; i < 256; i++) {
        new_child->children[i] = node->children[i];
        node->children[i] = NULL;
        if (new_child->children[i]) {
            new_child->child_count++;
            if (new_child->first_child == 255 || i < new_child->first_child) {
                new_child->first_child = i;
            }
        }
    }
    node->child_count = 0;
    node->first_child = 255;

    /* 缩短当前节点的前缀。 */
    node->prefix_len = split_pos;
    /* prefix 已经在原位置，不需要重新分配。 */

    *new_child_out = new_child;
    return 0;
}