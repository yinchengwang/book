/*
 * ttree_lookup.c
 *
 * T-Tree 查找和范围查询操作。
 */

#include "ttree_private.h"

/* 查找 key 对应的 value。 */
int ttree_index_lookup(const ttree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void **value_out, uint32_t *valuelen_out)
{
    const ttree_record_t *rec;

    if (!index || !key || keylen == 0) {
        return -1;
    }

    rec = _ttree_find_record_const(index, index->root, key, keylen);
    if (!rec) {
        return -1;
    }

    if (value_out) *value_out = rec->value;
    if (valuelen_out) *valuelen_out = rec->valuelen;

    return 0;
}

/*
static uint32_t _ttree_foreach_in_node(ttree_node_t *node,
                                       ttree_range_callback_fn callback,
                                       void *ctx)
{
    uint32_t i;
    uint32_t count = 0;

    if (!node) return 0;

    for (i = 0; i < node->nkeys; i++) {
        if (callback(node->records[i].key, node->records[i].keylen,
                     node->records[i].value, node->records[i].valuelen,
                     ctx) == 0) {
            count++;
        } else {
            break;  // 回调返回非 0，停止遍历
        }
    }

    return count;
}
*/

/* 范围查询 [min_key, max_key]。 */
int ttree_index_range(const ttree_index_t *index,
                      const void *min_key, uint32_t min_keylen,
                      const void *max_key, uint32_t max_keylen,
                      ttree_range_callback_fn callback,
                      void *ctx)
{
    ttree_node_t *node;
    ttree_node_t *start_node = NULL;
    ttree_node_t *end_node = NULL;
    uint32_t pos, i;
    int cmp;
    uint32_t count = 0;

    if (!index || !callback) return -1;

    /* 找到起始叶子节点。 */
    node = index->root;
    while (node && !node->is_leaf) {
        if (node->nkeys == 0) {
            node = node->left;
        } else if (!min_key) {
            /* 负无穷，从最左叶子开始。 */
            node = node->left;
        } else {
            cmp = _ttree_compare(index, min_key, min_keylen,
                                  node->records[0].key, node->records[0].keylen);
            if (cmp <= 0) {
                node = node->left;
            } else {
                cmp = _ttree_compare(index, min_key, min_keylen,
                                      node->records[node->nkeys - 1].key,
                                      node->records[node->nkeys - 1].keylen);
                if (cmp >= 0) {
                    node = node->right;
                } else {
                    node = node->left;
                }
            }
        }
    }
    start_node = node;

    /* 找到结束叶子节点（可选）。 */
    if (max_key) {
        node = index->root;
        while (node && !node->is_leaf) {
            if (node->nkeys == 0) {
                node = node->left;
            } else {
                cmp = _ttree_compare(index, max_key, max_keylen,
                                      node->records[0].key, node->records[0].keylen);
                if (cmp < 0) {
                    node = node->left;
                } else {
                    cmp = _ttree_compare(index, max_key, max_keylen,
                                          node->records[node->nkeys - 1].key,
                                          node->records[node->nkeys - 1].keylen);
                    if (cmp >= 0) {
                        node = node->right;
                    } else {
                        node = node->right;
                    }
                }
            }
        }
        end_node = node;
    }

    /* 遍历叶子链表。 */
    node = start_node;
    while (node) {
        /* 确定起始位置。 */
        pos = 0;
        if (min_key) {
            bool matched;
            pos = _ttree_lower_bound(index, node, min_key, min_keylen, &matched);
        }

        /* 确定结束位置。 */
        for (i = pos; i < node->nkeys; i++) {
            if (max_key) {
                cmp = _ttree_compare(index, max_key, max_keylen,
                                      node->records[i].key, node->records[i].keylen);
                if (cmp < 0) {
                    /* 超过上界，停止。 */
                    return (int)count;
                }
            }

            if (callback(node->records[i].key, node->records[i].keylen,
                         node->records[i].value, node->records[i].valuelen,
                         ctx) != 0) {
                return (int)count;
            }
            count++;
        }

        /* 检查是否到达终点。 */
        if (end_node && node == end_node) {
            break;
        }

        node = node->next;
    }

    return (int)count;
}
