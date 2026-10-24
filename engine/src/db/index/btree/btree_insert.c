/*
 * btree_insert.c
 *
 * B-Tree 插入操作。
 */

#include "btree_private.h"

static void _btree_shift_records_right(btree_node_t *node, uint32_t from, uint32_t to)
{
    uint32_t i;

    for (i = to; i > from; i--) {
        _btree_record_move(&node->records[i], &node->records[i - 1u]);
    }
}

static void _btree_shift_children_right(btree_node_t *node, uint32_t from, uint32_t to)
{
    uint32_t i;

    for (i = to; i > from; i--) {
        node->children[i] = node->children[i - 1u];
    }
}

int _btree_split_child(btree_index_t *index,
                       btree_node_t *parent,
                       uint32_t child_pos)
{
    btree_node_t *child = parent->children[child_pos];
    btree_node_t *right;
    uint32_t t = index->min_degree;
    uint32_t i;

    right = _btree_node_create(index, child->is_leaf);
    if (!right) {
        return -1;
    }

    /* 满孩子按中位数一分为二，右半部分搬到新节点。 */
    right->nkeys = t - 1u;
    for (i = 0; i < t - 1u; i++) {
        _btree_record_move(&right->records[i], &child->records[i + t]);
    }

    if (!child->is_leaf) {
        for (i = 0; i < t; i++) {
            right->children[i] = child->children[i + t];
            child->children[i + t] = NULL;
        }
    }

    child->nkeys = t - 1u;

    /* 把中位 key 提升到父节点，并把新右孩子挂到 parent 上。 */
    _btree_shift_children_right(parent, child_pos + 1u, parent->nkeys + 1u);
    parent->children[child_pos + 1u] = right;

    _btree_shift_records_right(parent, child_pos, parent->nkeys);
    _btree_record_move(&parent->records[child_pos], &child->records[t - 1u]);
    parent->nkeys++;
    return 0;
}

int _btree_insert_nonfull(btree_index_t *index,
                          btree_node_t *node,
                          const void *key, uint32_t keylen,
                          const void *value, uint32_t valuelen)
{
    bool matched = false;
    uint32_t pos = _btree_lower_bound(index, node, key, keylen, &matched);

    /* 叶子节点直接插入到有序数组中。 */
    if (node->is_leaf) {
        if (matched) {
            return 1;
        }

        _btree_shift_records_right(node, pos, node->nkeys);
        if (_btree_record_set(&node->records[pos], key, keylen, value, valuelen) != 0) {
            return -1;
        }
        node->nkeys++;
        return 0;
    }

    /* 下探前先保证目标孩子未满，这是经典 B-Tree top-down 插入策略。 */
    if (node->children[pos]->nkeys == index->max_keys) {
        if (_btree_split_child(index, node, pos) != 0) {
            return -1;
        }

        if (_btree_compare(index,
                           key,
                           keylen,
                           node->records[pos].key,
                           node->records[pos].keylen) > 0) {
            pos++;
        } else if (_btree_compare(index,
                                  key,
                                  keylen,
                                  node->records[pos].key,
                                  node->records[pos].keylen) == 0) {
            return 1;
        }
    }

    return _btree_insert_nonfull(index, node->children[pos], key, keylen, value, valuelen);
}

static int _btree_insert_internal(btree_index_t *index,
                                  const void *key, uint32_t keylen,
                                  const void *value, uint32_t valuelen)
{
    btree_node_t *root;

    if (_btree_find_record(index, index->root, key, keylen)) {
        return 1;
    }

    /* 根已满时先长高一层，再把旧根分裂到新根下面。 */
    root = index->root;
    if (root->nkeys == index->max_keys) {
        btree_node_t *new_root = _btree_node_create(index, false);

        if (!new_root) {
            return -1;
        }
        new_root->children[0] = root;

        if (_btree_split_child(index, new_root, 0) != 0) {
            free(new_root->records);
            free(new_root->children);
            free(new_root);
            return -1;
        }

        index->root = new_root;
        root = new_root;
    }

    return _btree_insert_nonfull(index, root, key, keylen, value, valuelen);
}

int btree_index_insert(btree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen)
{
    int rc;

    if (!index || !key || keylen == 0) {
        return -1;
    }

    rc = _btree_insert_internal(index, key, keylen, value, valuelen);
    if (rc == 0) {
        index->size++;
    }
    return rc;
}

int btree_index_upsert(btree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen)
{
    btree_record_t *record;
    int rc;

    if (!index || !key || keylen == 0) {
        return -1;
    }

    record = _btree_find_record(index, index->root, key, keylen);
    if (record) {
        return _btree_record_replace_value(record, value, valuelen);
    }

    rc = _btree_insert_internal(index, key, keylen, value, valuelen);
    if (rc == 0) {
        index->size++;
    }
    return rc;
}
