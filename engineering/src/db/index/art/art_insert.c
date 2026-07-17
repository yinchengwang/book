/*
 * art_insert.c
 *
 * ART 插入操作。
 */

#include "art_private.h"
#include <stdlib.h>
#include <string.h>

/* 辅助：添加子节点。返回 0 成功，-1 失败。 */
static int _art_add_child(art_node_t *node, uint8_t key, art_node_t *child)
{
    switch (node->type) {
    case ART_N4: {
        art_n4_t *n4 = (art_n4_t *)node;
        if (n4->node.count >= 4) return -1;  /* N4 已满 */
        uint32_t i = n4->node.count;
        while (i > 0 && n4->keys[i - 1] > key) {
            n4->keys[i] = n4->keys[i - 1];
            n4->children[i] = n4->children[i - 1];
            i--;
        }
        n4->keys[i] = key;
        n4->children[i] = child;
        n4->node.count++;
        break;
    }
    case ART_N16: {
        art_n16_t *n16 = (art_n16_t *)node;
        if (n16->node.count >= 16) return -1;
        uint32_t i = n16->node.count;
        while (i > 0 && n16->keys[i - 1] > key) {
            n16->keys[i] = n16->keys[i - 1];
            n16->children[i] = n16->children[i - 1];
            i--;
        }
        n16->keys[i] = key;
        n16->children[i] = child;
        n16->node.count++;
        break;
    }
    case ART_N48: {
        art_n48_t *n48 = (art_n48_t *)node;
        /* N48 使用索引数组，找一个空槽。 */
        uint32_t i;
        for (i = 0; i < 256; i++) {
            if (n48->children[i] == NULL) break;
        }
        if (i >= 256) return -1;  /* 没有空槽 */
        n48->indexes[key] = (uint8_t)i;
        n48->children[i] = child;
        n48->node.count++;
        break;
    }
    case ART_N256: {
        art_n256_t *n256 = (art_n256_t *)node;
        if (n256->children[key] != NULL) return -1;  /* 已存在 */
        n256->children[key] = child;
        n256->node.count++;
        break;
    }
    default:
        return -1;
    }
    return 0;
}

int _art_insert_node(art_tree_t *tree, art_node_t **node,
                     const uint8_t *key, uint32_t keylen,
                     const void *value, uint32_t valuelen,
                     uint32_t depth)
{
    art_node_t *n;
    art_leaf_t *leaf;
    uint8_t k;

    if (!*node) {
        /* 创建叶子节点。 */
        leaf = (art_leaf_t *)calloc(1, sizeof(art_leaf_t));
        if (!leaf) return -1;

        leaf->is_leaf = true;

        leaf->key = (uint8_t *)malloc(keylen);
        if (!leaf->key) {
            free(leaf);
            return -1;
        }
        memcpy(leaf->key, key, keylen);
        leaf->keylen = keylen;

        leaf->value = malloc(valuelen);
        if (!leaf->value) {
            free(leaf->key);
            free(leaf);
            return -1;
        }
        memcpy(leaf->value, value, valuelen);
        leaf->valuelen = valuelen;

        /* 用叶子替换当前节点。 */
        *node = (art_node_t *)leaf;
        tree->size++;
        return 0;
    }

    n = *node;

    /* 检查是否是叶子节点。 */
    if (n->is_leaf) {
        /* 需要分裂叶子节点。 */
        art_leaf_t *old_leaf = (art_leaf_t *)n;
        uint32_t old_keylen = old_leaf->keylen;
        uint8_t *old_key = old_leaf->key;

        /* 找到公共前缀长度。 */
        uint32_t common_len = 0;
        uint32_t max_common = (old_keylen < keylen) ? old_keylen : keylen;
        while (common_len < max_common && old_key[depth + common_len] == key[depth + common_len]) {
            common_len++;
        }

        if (common_len > 0) {
            /* 创建内部节点持有公共前缀。 */
            art_node_t *inner = _art_create_node(ART_N4);
            if (!inner) return -1;
            inner->prefix = (uint8_t *)malloc(common_len);
            if (!inner->prefix) {
                free(inner);
                return -1;
            }
            memcpy(inner->prefix, key + depth, common_len);
            inner->prefix_len = common_len;

            /* 在公共前缀后创建两个叶子。 */
            art_leaf_t *leaf1 = old_leaf;  /* 复用 old_leaf */
            art_leaf_t *leaf2 = (art_leaf_t *)calloc(1, sizeof(art_leaf_t));
            if (!leaf2) {
                free(inner->prefix);
                free(inner);
                return -1;
            }

            leaf2->is_leaf = true;
            leaf2->key = (uint8_t *)malloc(keylen);
            if (!leaf2->key) {
                free(leaf2);
                free(inner->prefix);
                free(inner);
                return -1;
            }
            memcpy(leaf2->key, key, keylen);
            leaf2->keylen = keylen;
            leaf2->value = malloc(valuelen);
            if (!leaf2->value) {
                free(leaf2->key);
                free(leaf2);
                free(inner->prefix);
                free(inner);
                return -1;
            }
            memcpy(leaf2->value, value, valuelen);
            leaf2->valuelen = valuelen;

            /* 添加子节点（注意处理 old_key 已结束的情况）。 */
            if (depth + common_len < old_keylen) {
                /* old_key 还有剩余字符。 */
                uint8_t key1_suffix = old_key[depth + common_len];
                _art_add_child(inner, key1_suffix, (art_node_t *)leaf1);
            }
            if (depth + common_len < keylen) {
                /* new_key 还有剩余字符。 */
                uint8_t key2_suffix = key[depth + common_len];
                _art_add_child(inner, key2_suffix, (art_node_t *)leaf2);
            }
            inner->count = 2;

            /* 不释放 old_leaf，因为它的指针已经转移给 leaf1，成为内部节点的子节点 */

            *node = inner;
            tree->size++;
            return 0;
        } else {
            /* 没有公共前缀，创建内部节点。 */
            art_node_t *inner = _art_create_node(ART_N4);
            if (!inner) return -1;

            art_leaf_t *leaf1 = old_leaf;  /* 复用 old_leaf */
            art_leaf_t *leaf2 = (art_leaf_t *)calloc(1, sizeof(art_leaf_t));
            if (!leaf2) {
                free(inner);
                return -1;
            }

            leaf2->is_leaf = true;
            leaf2->key = (uint8_t *)malloc(keylen);
            if (!leaf2->key) {
                free(leaf2);
                free(inner);
                return -1;
            }
            memcpy(leaf2->key, key, keylen);
            leaf2->keylen = keylen;
            leaf2->value = malloc(valuelen);
            if (!leaf2->value) {
                free(leaf2->key);
                free(leaf2);
                free(inner);
                return -1;
            }
            memcpy(leaf2->value, value, valuelen);
            leaf2->valuelen = valuelen;

            /* 添加子节点（确保 depth < keylen）。 */
            if (depth < old_keylen) {
                uint8_t key1_first = old_key[depth];
                _art_add_child(inner, key1_first, (art_node_t *)leaf1);
            }
            if (depth < keylen) {
                uint8_t key2_first = key[depth];
                _art_add_child(inner, key2_first, (art_node_t *)leaf2);
            }
            inner->count = 2;

            /* 不释放 old_leaf，因为它的指针已经转移给 leaf1 */

            *node = inner;
            tree->size++;
            return 0;
        }
    }

    /* 检查前缀匹配。 */
    if (n->prefix_len > 0 && depth < keylen) {
        uint32_t i;
        for (i = 0; i < n->prefix_len && depth + i < keylen; i++) {
            if (n->prefix[i] != key[depth + i]) break;
        }
        depth += i;
    }

    if (depth >= keylen) {
        /* key 结束，创建叶子。 */
        art_leaf_t *new_leaf = (art_leaf_t *)calloc(1, sizeof(art_leaf_t));
        if (!new_leaf) return -1;
        new_leaf->is_leaf = true;

        new_leaf->key = (uint8_t *)malloc(keylen);
        memcpy(new_leaf->key, key, keylen);
        new_leaf->keylen = keylen;

        new_leaf->value = malloc(valuelen);
        memcpy(new_leaf->value, value, valuelen);
        new_leaf->valuelen = valuelen;

        *node = (art_node_t *)new_leaf;
        tree->size++;
        return 0;
    }

    k = key[depth];
    art_node_t *child = _art_find_child(n, k);

    if (!child) {
        /* 没有子节点，创建叶子。 */
        art_leaf_t *new_leaf = (art_leaf_t *)calloc(1, sizeof(art_leaf_t));
        if (!new_leaf) return -1;
        new_leaf->is_leaf = true;

        new_leaf->key = (uint8_t *)malloc(keylen);
        if (!new_leaf->key) {
            free(new_leaf);
            return -1;
        }
        memcpy(new_leaf->key, key, keylen);
        new_leaf->keylen = keylen;

        new_leaf->value = malloc(valuelen);
        if (!new_leaf->value) {
            free(new_leaf->key);
            free(new_leaf);
            return -1;
        }
        memcpy(new_leaf->value, value, valuelen);
        new_leaf->valuelen = valuelen;

        _art_add_child(n, k, (art_node_t *)new_leaf);
        n->count++;
        tree->size++;
        return 0;
    }

    /* 递归插入。 */
    return _art_insert_node(tree, &child, key, keylen, value, valuelen, depth + 1);
}

int art_insert(art_tree_t *tree,
               const uint8_t *key, uint32_t keylen,
               const void *value, uint32_t valuelen)
{
    if (!tree || !key || keylen == 0) return -1;
    return _art_insert_node(tree, &tree->root, key, keylen, value, valuelen, 0);
}
