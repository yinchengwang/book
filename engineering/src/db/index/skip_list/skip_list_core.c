/*
 * skip_list_core.c
 *
 * Skip List 核心操作：创建、销毁、节点管理、查找。
 */

#include "skip_list_private.h"
#include <stdlib.h>

/* 默认字节序比较。 */
static int _skip_list_default_compare(const void *lhs, uint32_t lhs_len,
                                      const void *rhs, uint32_t rhs_len)
{
    uint32_t min_len = lhs_len < rhs_len ? lhs_len : rhs_len;
    int cmp;

    if (min_len == 0) {
        return lhs_len == rhs_len ? 0 : (lhs_len < rhs_len ? -1 : 1);
    }

    cmp = memcmp(lhs, rhs, min_len);
    if (cmp != 0) return cmp;
    return lhs_len == rhs_len ? 0 : (lhs_len < rhs_len ? -1 : 1);
}

int _skip_list_compare(const skip_list_t *list,
                       const void *lhs, uint32_t lhs_len,
                       const void *rhs, uint32_t rhs_len)
{
    if (list->compare) {
        return list->compare(lhs, lhs_len, rhs, rhs_len, list->compare_ctx);
    }
    return _skip_list_default_compare(lhs, lhs_len, rhs, rhs_len);
}

/* 随机生成层级，概率 P=0.5 晋升到更高层级。 */
uint32_t _skip_list_random_level(void)
{
    uint32_t level = 1;
    while ((rand() < (RAND_MAX / 2)) && (level < SKIP_LIST_MAX_LEVEL)) {
        level++;
    }
    return level;
}

skip_list_node_t *_skip_list_node_create(const skip_list_t *list,
                                         const void *key, uint32_t keylen,
                                         const void *value, uint32_t valuelen,
                                         uint32_t level)
{
    skip_list_node_t *node;

    (void)list;
    node = (skip_list_node_t *)calloc(1, sizeof(skip_list_node_t));
    if (!node) return NULL;

    node->forward = (skip_list_node_t **)calloc(level + 1, sizeof(skip_list_node_t *));
    if (!node->forward) {
        free(node);
        return NULL;
    }

    node->key = malloc(keylen);
    if (!node->key) {
        free(node->forward);
        free(node);
        return NULL;
    }
    memcpy(node->key, key, keylen);
    node->keylen = keylen;

    if (value && valuelen > 0) {
        node->value = malloc(valuelen);
        if (!node->value) {
            free(node->key);
            free(node->forward);
            free(node);
            return NULL;
        }
        memcpy(node->value, value, valuelen);
        node->valuelen = valuelen;
    }

    node->level = level;
    return node;
}

void _skip_list_node_drop(skip_list_node_t *node)
{
    if (!node) return;
    free(node->key);
    free(node->value);
    free(node->forward);  /* forward 大小是 node->level + 1 */
    free(node);
}

skip_list_t *skip_list_create(skip_list_compare_fn compare,
                               void *compare_ctx,
                               uint32_t max_level)
{
    skip_list_t *list;
    uint32_t i;

    if (max_level == 0) max_level = SKIP_LIST_DEFAULT_MAX_LEVEL;
    if (max_level > SKIP_LIST_MAX_LEVEL) max_level = SKIP_LIST_MAX_LEVEL;

    list = (skip_list_t *)calloc(1, sizeof(skip_list_t));
    if (!list) return NULL;

    list->compare = compare;
    list->compare_ctx = compare_ctx;
    list->level = 1;
    list->size = 0;

    /* 创建头节点。 */
    list->header = _skip_list_node_create(list, NULL, 0, NULL, 0, max_level);
    if (!list->header) {
        free(list);
        return NULL;
    }

    /* 头节点的 forward 全部初始化为 NULL。 */
    for (i = 0; i <= max_level; i++) {
        list->header->forward[i] = NULL;
    }

    return list;
}

void skip_list_drop(skip_list_t *list)
{
    skip_list_node_t *node, *next;

    if (!list) return;

    node = list->header;
    while (node) {
        next = node->forward[0];
        _skip_list_node_drop(node);
        node = next;
    }

    free(list);
}

uint32_t skip_list_size(const skip_list_t *list)
{
    return list ? list->size : 0;
}

uint32_t skip_list_level(const skip_list_t *list)
{
    return list ? list->level : 0;
}

/* 查找 key，更新数组保存每层最后一个小于 key 的节点。 */
skip_list_node_t **_skip_list_find(const skip_list_t *list,
                                   const void *key, uint32_t keylen)
{
    skip_list_node_t *current;
    skip_list_node_t **update;
    int cmp;
    uint32_t i;

    update = (skip_list_node_t **)calloc(list->level + 1, sizeof(skip_list_node_t *));
    if (!update) return NULL;

    current = list->header;
    for (i = list->level; i >= 1; i--) {
        while (current->forward[i] != NULL) {
            cmp = _skip_list_compare(list, key, keylen,
                                      current->forward[i]->key,
                                      current->forward[i]->keylen);
            if (cmp <= 0) {
                break;
            }
            current = current->forward[i];
        }
        update[i] = current;
    }

    /* 第 0 层：保存最后一个小于 key 的节点，然后 forward[0] 指向可能的匹配节点。 */
    while (current->forward[0] != NULL) {
        cmp = _skip_list_compare(list, key, keylen,
                                  current->forward[0]->key,
                                  current->forward[0]->keylen);
        if (cmp <= 0) {
            break;
        }
        current = current->forward[0];
    }
    update[0] = current;

    return update;
}
