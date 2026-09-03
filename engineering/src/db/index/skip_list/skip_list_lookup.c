/*
 * skip_list_lookup.c
 *
 * Skip List 查找和范围查询操作。
 */

#include "skip_list_private.h"

/* 查找 key。 */
int skip_list_search(const skip_list_t *list,
                    const void *key, uint32_t keylen,
                    const void **value_out, uint32_t *valuelen_out)
{
    skip_list_node_t **update;
    skip_list_node_t *current;
    int cmp;

    if (!list || !key || keylen == 0) return -1;

    update = _skip_list_find(list, key, keylen);
    if (!update) return -1;

    /* update[0] 是最后一个小于 key 的节点，forward[0] 才是可能的匹配节点。 */
    current = update[0]->forward[0];
    free(update);

    if (current == NULL || current->key == NULL) {
        return -1;
    }

    cmp = _skip_list_compare(list, key, keylen, current->key, current->keylen);
    if (cmp != 0) {
        return -1;
    }

    if (value_out) *value_out = current->value;
    if (valuelen_out) *valuelen_out = current->valuelen;

    return 0;
}

/* 范围查询 [min_key, max_key]。 */
int skip_list_range(skip_list_t *list,
                    const void *min_key, uint32_t min_keylen,
                    const void *max_key, uint32_t max_keylen,
                    skip_list_range_callback_fn callback,
                    void *ctx)
{
    skip_list_node_t *current;
    skip_list_node_t *target;
    int cmp;
    uint32_t count = 0;

    if (!list || !callback) return -1;

    /* 找到起始位置。 */
    current = list->header;
    if (min_key) {
        uint32_t i;
        for (i = list->level; i >= 1; i--) {
            while (current->forward[i] != NULL) {
                cmp = _skip_list_compare(list, min_key, min_keylen,
                                          current->forward[i]->key,
                                          current->forward[i]->keylen);
                if (cmp <= 0) {
                    break;
                }
                current = current->forward[i];
            }
        }
        /* 还需要在第 0 层找到第一个 >= min_key 的节点。 */
        while (current->forward[0] != NULL) {
            cmp = _skip_list_compare(list, min_key, min_keylen,
                                      current->forward[0]->key,
                                      current->forward[0]->keylen);
            if (cmp <= 0) {
                break;
            }
            current = current->forward[0];
        }
        current = current->forward[0];
    } else {
        current = current->forward[0];
    }

    target = current;

    /* 遍历直到超过上界。 */
    while (target != NULL) {
        if (max_key) {
            cmp = _skip_list_compare(list, max_key, max_keylen,
                                      target->key, target->keylen);
            if (cmp < 0) {
                /* 超过上界，停止。 */
                break;
            }
        }

        if (callback(target->key, target->keylen,
                     target->value, target->valuelen, ctx) != 0) {
            break;
        }
        count++;

        target = target->forward[0];
    }

    return (int)count;
}
