/*
 * ttree_insert.c
 *
 * T-Tree 插入操作。
 */

#include "ttree_private.h"
#include <stdio.h>

/* 在节点中插入 key-value，保持有序。 */
int _ttree_insert_into_node(ttree_index_t *index,
                            ttree_node_t *node,
                            const void *key, uint32_t keylen,
                            const void *value, uint32_t valuelen)
{
    bool matched;
    uint32_t pos = _ttree_lower_bound(index, node, key, keylen, &matched);
    uint32_t i;

    if (matched) {
        _ttree_record_clear(&node->records[pos]);
        return _ttree_record_set(&node->records[pos], key, keylen, value, valuelen);
    }

    /* 移动后面的记录腾出位置。 */
    for (i = node->nkeys; i > pos; i--) {
        _ttree_record_clear(&node->records[i]);
        _ttree_record_clone(&node->records[i], &node->records[i - 1]);
    }
    memset(&node->records[pos], 0, sizeof(ttree_record_t));

    node->nkeys++;
    return _ttree_record_set(&node->records[pos], key, keylen, value, valuelen);
}

/* 分裂叶子节点 left，将其后半移到新兄弟节点 right。 */
int _ttree_split_leaf(ttree_index_t *index,
                      ttree_node_t *left,
                      ttree_node_t **right_out)
{
    ttree_node_t *right;
    uint32_t mid = left->nkeys / 2;
    uint32_t i;

    (void)index;
    right = _ttree_node_create(index);
    if (!right) return -1;

    right->is_leaf = true;

    /* 将后半部分移到新节点。 */
    for (i = mid; i < left->nkeys; i++) {
        _ttree_record_clone(&right->records[right->nkeys], &left->records[i]);
        right->nkeys++;
        _ttree_record_clear(&left->records[i]);
    }
    left->nkeys = mid;

    /* 更新兄弟链表。 */
    right->next = left->next;
    right->prev = left;
    if (left->next) {
        left->next->prev = right;
    }
    left->next = right;

    *right_out = right;
    return 0;
}

/* 分裂内部节点。 */
int _ttree_split_internal(ttree_index_t *index,
                          ttree_node_t *node,
                          ttree_record_t *up_record,
                          ttree_node_t **right_out)
{
    ttree_node_t *right;
    uint32_t mid = node->nkeys / 2;
    uint32_t i;

    (void)index;
    right = _ttree_node_create(index);
    if (!right) return -1;

    right->is_leaf = false;

    /* 中位 key 提升到父节点。 */
    _ttree_record_clone(up_record, &node->records[mid]);
    _ttree_record_clear(&node->records[mid]);

    /* 后半部分的 keys 和 right 子树移到新节点。 */
    for (i = mid + 1; i < node->nkeys; i++) {
        _ttree_record_clone(&right->records[right->nkeys], &node->records[i]);
        right->nkeys++;
        _ttree_record_clear(&node->records[i]);
    }

    /* 移动 right 子树。 */
    right->right = node->right;
    node->right = NULL;

    node->nkeys = mid;

    *right_out = right;
    return 0;
}

/* 查找要插入的叶子节点。 */
ttree_node_t *_ttree_find_leaf_for_insert(ttree_index_t *index,
                                          ttree_node_t *node,
                                          const void *key, uint32_t keylen)
{
    if (node->is_leaf) return node;

    if (node->nkeys == 0) {
        return _ttree_find_leaf_for_insert(index, node->left, key, keylen);
    }

    int cmp = _ttree_compare(index, key, keylen,
                              node->records[0].key, node->records[0].keylen);

    if (cmp < 0) {
        return _ttree_find_leaf_for_insert(index, node->left, key, keylen);
    }

    cmp = _ttree_compare(index, key, keylen,
                          node->records[node->nkeys - 1].key,
                          node->records[node->nkeys - 1].keylen);

    if (cmp >= 0) {
        return _ttree_find_leaf_for_insert(index, node->right, key, keylen);
    }

    return _ttree_find_leaf_for_insert(index, node->left, key, keylen);
}

/* 主入口：插入 key-value。 */
int ttree_index_insert(ttree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen)
{
    ttree_node_t *leaf;
    bool matched;
    int cmp;

    if (!index || !key || keylen == 0) return -1;

    /* 查找叶子节点。 */
    leaf = _ttree_find_leaf_for_insert(index, index->root, key, keylen);

    /* 检查是否已存在。 */
    _ttree_lower_bound(index, leaf, key, keylen, &matched);

    if (matched) {
        /* key 已存在，更新 value。 */
        int pos = (int)_ttree_find_in_node(index, leaf, key, keylen);
        if (pos >= 0) {
            _ttree_record_clear(&leaf->records[pos]);
            return _ttree_record_set(&leaf->records[pos], key, keylen, value, valuelen);
        }
    }

    if (leaf->nkeys < index->max_keys) {
        /* 有空间，直接插入。 */
        int ret = _ttree_insert_into_node(index, leaf, key, keylen, value, valuelen);
        if (ret == 0) index->size++;
        return ret;
    }

    /* 满了，分裂。 */
    ttree_node_t *right = NULL;
    if (_ttree_split_leaf(index, leaf, &right) != 0) return -1;

    cmp = _ttree_compare(index, key, keylen,
                          leaf->records[leaf->nkeys - 1].key,
                          leaf->records[leaf->nkeys - 1].keylen);

    if (cmp > 0) {
        int ret = _ttree_insert_into_node(index, right, key, keylen, value, valuelen);
        if (ret == 0) index->size++;
        return ret;
    } else {
        int ret = _ttree_insert_into_node(index, leaf, key, keylen, value, valuelen);
        if (ret == 0) index->size++;
        return ret;
    }
}
