/*
 * skip_list_insert.c
 *
 * Skip List 插入操作。
 */

#include "skip_list_private.h"
#include <stdlib.h>

int skip_list_insert(skip_list_t *list,
                     const void *key, uint32_t keylen,
                     const void *value, uint32_t valuelen)
{
    skip_list_node_t **update;
    skip_list_node_t *current;
    skip_list_node_t *new_node;
    int cmp;
    uint32_t i;
    uint32_t level;
    uint32_t old_level;

    if (!list || !key || keylen == 0) return -1;

    /* 随机生成层级（先确定）。 */
    level = _skip_list_random_level();

    /* 查找插入位置，update 数组大小至少为 level+1。 */
    old_level = list->level;
    if (level > old_level) {
        /* 需要扩展 update 数组。 */
        update = (skip_list_node_t **)calloc(level + 1, sizeof(skip_list_node_t *));
        if (!update) return -1;

        /* 先用旧层级查找。 */
        skip_list_node_t *cur = list->header;
        uint32_t j;
        for (j = old_level; j >= 1; j--) {
            while (cur->forward[j] != NULL) {
                cmp = _skip_list_compare(list, key, keylen,
                                          cur->forward[j]->key,
                                          cur->forward[j]->keylen);
                if (cmp <= 0) {
                    break;
                }
                cur = cur->forward[j];
            }
            update[j] = cur;
        }
        /* 第 0 层。 */
        while (cur->forward[0] != NULL) {
            cmp = _skip_list_compare(list, key, keylen,
                                      cur->forward[0]->key,
                                      cur->forward[0]->keylen);
            if (cmp <= 0) {
                break;
            }
            cur = cur->forward[0];
        }
        update[0] = cur;

        /* 新层级指向 header。 */
        for (j = old_level + 1; j <= level; j++) {
            update[j] = list->header;
        }
    } else {
        /* 使用普通查找，但分配足够大的数组。 */
        update = (skip_list_node_t **)calloc(level + 1, sizeof(skip_list_node_t *));
        if (!update) return -1;

        /* 手动查找。 */
        skip_list_node_t *cur = list->header;
        uint32_t j;
        for (j = old_level; j >= 1; j--) {
            while (cur->forward[j] != NULL) {
                cmp = _skip_list_compare(list, key, keylen,
                                          cur->forward[j]->key,
                                          cur->forward[j]->keylen);
                if (cmp <= 0) {
                    break;
                }
                cur = cur->forward[j];
            }
            if (j <= level) {
                update[j] = cur;
            }
        }
        /* 第 0 层。 */
        while (cur->forward[0] != NULL) {
            cmp = _skip_list_compare(list, key, keylen,
                                      cur->forward[0]->key,
                                      cur->forward[0]->keylen);
            if (cmp <= 0) {
                break;
            }
            cur = cur->forward[0];
        }
        update[0] = cur;
    }

    /* update[0] 是最后一个小于 key 的节点，forward[0] 才是可能的匹配节点。 */
    current = update[0]->forward[0];

    /* 检查是否已存在。 */
    if (current != NULL && current->key != NULL) {
        cmp = _skip_list_compare(list, key, keylen, current->key, current->keylen);
        if (cmp == 0) {
            /* key 已存在，更新 value。 */
            free(current->value);
            if (value && valuelen > 0) {
                current->value = malloc(valuelen);
                if (current->value) {
                    memcpy(current->value, value, valuelen);
                    current->valuelen = valuelen;
                } else {
                    current->valuelen = 0;
                }
            } else {
                current->value = NULL;
                current->valuelen = 0;
            }
            free(update);
            return 0;
        }
    }

    /* 更新列表层级。 */
    if (level > old_level) {
        list->level = level;
    }

    /* 创建新节点。 */
    new_node = _skip_list_node_create(list, key, keylen, value, valuelen, level);
    if (!new_node) {
        free(update);
        return -1;
    }

    /* 在各层级插入新节点。 */
    for (i = 1; i <= level; i++) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    /* 第 0 层也要插入！ */
    new_node->forward[0] = update[0]->forward[0];
    update[0]->forward[0] = new_node;

    list->size++;
    free(update);
    return 0;
}
