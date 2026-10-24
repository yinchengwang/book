/*
 * skip_list_delete.c
 *
 * Skip List 删除操作。
 */

#include "skip_list_private.h"
#include <stdlib.h>
#include <stdio.h>

int skip_list_delete(skip_list_t *list,
                    const void *key, uint32_t keylen)
{
    skip_list_node_t **update;
    skip_list_node_t *current;
    int cmp;
    uint32_t i;
    uint32_t max_level;

    if (!list || !key || keylen == 0) return -1;
    if (list->size == 0) return -1;

    /* 查找要删除的节点。 */
    update = _skip_list_find(list, key, keylen);
    if (!update) return -1;

    /* update[0] 是最后一个小于 key 的节点。 */
    current = update[0]->forward[0];

    /* 检查是否找到。 */
    if (current == NULL || current->key == NULL) {
        free(update);
        return -1;
    }

    cmp = _skip_list_compare(list, key, keylen, current->key, current->keylen);
    if (cmp != 0) {
        free(update);
        return -1;
    }

    /* 计算要删除的层级上限（不能超过 update 数组大小）。 */
    max_level = (current->level < list->level) ? current->level : list->level;

    /* 从各层级删除节点。 */
    for (i = 1; i <= max_level; i++) {
        if (update[i] && update[i]->forward[i] == current) {
            update[i]->forward[i] = current->forward[i];
        }
    }

    /* 也需要更新第 0 层的 forward！ */
    if (update[0] && update[0]->forward[0] == current) {
        update[0]->forward[0] = current->forward[0];
    }

    /* 释放节点的所有资源。 */
    _skip_list_node_drop(current);

    list->size--;

    /* 降低层级的空层。 */
    while (list->level > 1 && list->header->forward[list->level] == NULL) {
        list->level--;
    }

    free(update);
    return 0;
}
