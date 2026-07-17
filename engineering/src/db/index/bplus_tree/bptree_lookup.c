#include "bptree_private.h"

uint32_t _bptree_leaf_lower_bound(const bptree_index_t *index,
                                  const bptree_node_t *node,
                                  const void *key, uint32_t keylen,
                                  bool *matched)
{
    uint32_t lo = 0;
    uint32_t hi = node->nkeys;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int cmp = _bptree_compare(index,
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
                   _bptree_compare(index,
                                   key,
                                   keylen,
                                   node->records[lo].key,
                                   node->records[lo].keylen) == 0;
    }
    return lo;
}

uint32_t _bptree_internal_child_pos(const bptree_index_t *index,
                                    const bptree_node_t *node,
                                    const void *key, uint32_t keylen)
{
    uint32_t pos = 0;

    while (pos < node->nkeys &&
           _bptree_compare(index,
                           key,
                           keylen,
                           node->records[pos].key,
                           node->records[pos].keylen) >= 0) {
        pos++;
    }
    return pos;
}

bptree_node_t *_bptree_find_leaf(const bptree_index_t *index,
                                 const void *key, uint32_t keylen)
{
    bptree_node_t *node = index->root;

    while (node && !node->is_leaf) {
        uint32_t pos = _bptree_internal_child_pos(index, node, key, keylen);
        node = node->children[pos];
    }
    return node;
}

const bptree_record_t *_bptree_find_record(const bptree_index_t *index,
                                           const void *key, uint32_t keylen,
                                           bptree_node_t **leaf_out,
                                           uint32_t *pos_out)
{
    bptree_node_t *leaf;
    bool matched = false;
    uint32_t pos;

    leaf = _bptree_find_leaf(index, key, keylen);
    if (!leaf) {
        return NULL;
    }

    pos = _bptree_leaf_lower_bound(index, leaf, key, keylen, &matched);
    if (leaf_out) {
        *leaf_out = leaf;
    }
    if (pos_out) {
        *pos_out = pos;
    }
    if (!matched) {
        return NULL;
    }
    return &leaf->records[pos];
}

int bptree_index_lookup(const bptree_index_t *index,
                        const void *key, uint32_t keylen,
                        void **value_out, uint32_t *valuelen_out)
{
    const bptree_record_t *record;

    if (!index || !key || keylen == 0) {
        return -1;
    }

    record = _bptree_find_record(index, key, keylen, NULL, NULL);
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

int bptree_index_range_scan(const bptree_index_t *index,
                            const void *lower_key, uint32_t lower_keylen,
                            bool lower_inclusive,
                            const void *upper_key, uint32_t upper_keylen,
                            bool upper_inclusive,
                            bptree_scan_visit_fn visitor,
                            void *visitor_ctx)
{
    bptree_node_t *leaf;
    uint32_t pos = 0;

    if (!index || !visitor) {
        return -1;
    }

    if (lower_key && lower_keylen > 0) {
        bool matched = false;

        leaf = _bptree_find_leaf(index, lower_key, lower_keylen);
        if (!leaf) {
            return 0;
        }
        pos = _bptree_leaf_lower_bound(index, leaf, lower_key, lower_keylen, &matched);
        if (matched && !lower_inclusive) {
            pos++;
        }
    } else {
        leaf = index->root;
        while (leaf && !leaf->is_leaf) {
            leaf = leaf->children[0];
        }
    }

    while (leaf) {
        while (pos < leaf->nkeys) {
            int upper_cmp = -1;

            if (upper_key && upper_keylen > 0) {
                upper_cmp = _bptree_compare(index,
                                            leaf->records[pos].key,
                                            leaf->records[pos].keylen,
                                            upper_key,
                                            upper_keylen);
                if (upper_cmp > 0 || (upper_cmp == 0 && !upper_inclusive)) {
                    return 0;
                }
            }

            if (visitor(leaf->records[pos].key,
                        leaf->records[pos].keylen,
                        leaf->records[pos].value,
                        leaf->records[pos].valuelen,
                        visitor_ctx) != 0) {
                return 1;
            }
            pos++;
        }

        leaf = leaf->next_leaf;
        pos = 0;
    }

    return 0;
}