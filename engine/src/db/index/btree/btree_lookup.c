/*
 * btree_lookup.c
 *
 * B-Tree 查找操作。
 */

#include "btree_private.h"

uint32_t _btree_lower_bound(const btree_index_t *index,
                            const btree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *matched)
{
    uint32_t lo = 0;
    uint32_t hi = node->nkeys;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int cmp = _btree_compare(index,
                                 key,
                                 keylen,
                                 node->records[mid].key,
                                 node->records[mid].keylen);

        if (cmp > 0) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }

    if (matched) {
        *matched = lo < node->nkeys &&
                   _btree_compare(index,
                                  key,
                                  keylen,
                                  node->records[lo].key,
                                  node->records[lo].keylen) == 0;
    }

    return lo;
}

btree_record_t *_btree_find_record(const btree_index_t *index,
                                   btree_node_t *node,
                                   const void *key, uint32_t keylen)
{
    bool matched = false;
    uint32_t pos;

    if (!node) {
        return NULL;
    }

    pos = _btree_lower_bound(index, node, key, keylen, &matched);
    if (matched) {
        return &node->records[pos];
    }
    if (node->is_leaf) {
        return NULL;
    }

    return _btree_find_record(index, node->children[pos], key, keylen);
}

const btree_record_t *_btree_find_record_const(const btree_index_t *index,
                                               const btree_node_t *node,
                                               const void *key, uint32_t keylen)
{
    bool matched = false;
    uint32_t pos;

    if (!node) {
        return NULL;
    }

    pos = _btree_lower_bound(index, node, key, keylen, &matched);
    if (matched) {
        return &node->records[pos];
    }
    if (node->is_leaf) {
        return NULL;
    }

    return _btree_find_record_const(index, node->children[pos], key, keylen);
}

int btree_index_lookup(const btree_index_t *index,
                       const void *key, uint32_t keylen,
                       void **value_out, uint32_t *valuelen_out)
{
    const btree_record_t *record;

    if (!index || !key || keylen == 0) {
        return -1;
    }

    record = _btree_find_record_const(index, index->root, key, keylen);
    if (!record) {
        return -1;
    }

    if (value_out) {
        *value_out = record->value;
    }
    if (valuelen_out) {
        *valuelen_out = record->valuelen;
    }

    return 0;
}
