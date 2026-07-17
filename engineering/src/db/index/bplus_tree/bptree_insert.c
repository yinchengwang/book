#include "bptree_private.h"

static void _bptree_shift_records_right(bptree_node_t *node, uint32_t from, uint32_t to)
{
    uint32_t i;

    for (i = to; i > from; i--) {
        _bptree_record_move(&node->records[i], &node->records[i - 1u]);
    }
}

static void _bptree_shift_children_right(bptree_node_t *node, uint32_t from, uint32_t to)
{
    uint32_t i;

    for (i = to; i > from; i--) {
        node->children[i] = node->children[i - 1u];
    }
}

static int _bptree_split_leaf(bptree_index_t *index,
                              bptree_node_t *leaf,
                              bptree_record_t *promoted,
                              bptree_node_t **right_child)
{
    bptree_node_t *right;
    uint32_t split_at = leaf->nkeys / 2u;
    uint32_t i;

    if (_bptree_record_set(promoted,
                           leaf->records[split_at].key,
                           leaf->records[split_at].keylen,
                           NULL,
                           0) != 0) {
        return -1;
    }

    right = _bptree_node_create(index, true);
    if (!right) {
        _bptree_record_clear(promoted);
        return -1;
    }

    /* 叶子分裂后，右叶子的首 key 会提升为父节点中的分隔键。 */
    right->nkeys = leaf->nkeys - split_at;
    for (i = 0; i < right->nkeys; i++) {
        _bptree_record_move(&right->records[i], &leaf->records[split_at + i]);
    }
    leaf->nkeys = split_at;

    /* 维护叶子链表，保证 range scan 仍可顺序遍历。 */
    right->next_leaf = leaf->next_leaf;
    leaf->next_leaf = right;
    index->leaf_count++;

    *right_child = right;
    return 0;
}

static int _bptree_split_internal(bptree_index_t *index,
                                  bptree_node_t *node,
                                  bptree_record_t *promoted,
                                  bptree_node_t **right_child)
{
    bptree_node_t *right;
    uint32_t mid = node->nkeys / 2u;
    uint32_t i;

    if (_bptree_record_set(promoted,
                           node->records[mid].key,
                           node->records[mid].keylen,
                           NULL,
                           0) != 0) {
        return -1;
    }

    right = _bptree_node_create(index, false);
    if (!right) {
        _bptree_record_clear(promoted);
        return -1;
    }

    /* 内部节点分裂时，中间 key 上升到父节点，两侧孩子留在左右子树。 */
    right->nkeys = node->nkeys - mid - 1u;
    for (i = 0; i < right->nkeys; i++) {
        _bptree_record_move(&right->records[i], &node->records[mid + 1u + i]);
    }
    for (i = 0; i <= right->nkeys; i++) {
        right->children[i] = node->children[mid + 1u + i];
        node->children[mid + 1u + i] = NULL;
    }

    _bptree_record_clear(&node->records[mid]);
    node->nkeys = mid;
    *right_child = right;
    return 0;
}

int _bptree_insert_recursive(bptree_index_t *index,
                             bptree_node_t *node,
                             const void *key, uint32_t keylen,
                             const void *value, uint32_t valuelen,
                             bptree_record_t *promoted,
                             bptree_node_t **right_child,
                             bool *inserted_new_key)
{
    /* 叶子节点负责真正存放 value。 */
    if (node->is_leaf) {
        bool matched = false;
        uint32_t pos = _bptree_leaf_lower_bound(index, node, key, keylen, &matched);

        if (matched) {
            if (inserted_new_key) {
                *inserted_new_key = false;
            }
            return 1;
        }

        _bptree_shift_records_right(node, pos, node->nkeys);
        if (_bptree_record_set(&node->records[pos], key, keylen, value, valuelen) != 0) {
            return -1;
        }
        node->nkeys++;
        if (inserted_new_key) {
            *inserted_new_key = true;
        }

        if (node->nkeys <= index->max_keys) {
            return 0;
        }
        return _bptree_split_leaf(index, node, promoted, right_child);
    }

    {
        /* 内部节点先递归下探，只有子树分裂时才在本层插入新的分隔键。 */
        uint32_t child_pos = _bptree_internal_child_pos(index, node, key, keylen);
        bptree_record_t child_promoted = {0};
        bptree_node_t *child_right = NULL;
        bool child_inserted = false;
        int rc;

        rc = _bptree_insert_recursive(index,
                                      node->children[child_pos],
                                      key,
                                      keylen,
                                      value,
                                      valuelen,
                                      &child_promoted,
                                      &child_right,
                                      &child_inserted);
        if (rc == -1 || rc == 1) {
            return rc;
        }
        if (inserted_new_key) {
            *inserted_new_key = child_inserted;
        }

        if (!child_right) {
            return 0;
        }

        _bptree_shift_records_right(node, child_pos, node->nkeys);
        _bptree_record_move(&node->records[child_pos], &child_promoted);
        _bptree_shift_children_right(node, child_pos + 1u, node->nkeys + 1u);
        node->children[child_pos + 1u] = child_right;
        node->nkeys++;

        if (node->nkeys <= index->max_keys) {
            return 0;
        }
        return _bptree_split_internal(index, node, promoted, right_child);
    }
}

static int _bptree_insert_internal(bptree_index_t *index,
                                   const void *key, uint32_t keylen,
                                   const void *value, uint32_t valuelen,
                                   bool allow_replace)
{
    bptree_node_t *root;
    bptree_node_t *leaf = NULL;
    uint32_t pos = 0;
    const bptree_record_t *record;

    record = _bptree_find_record(index, key, keylen, &leaf, &pos);
    if (record) {
        if (!allow_replace) {
            return 1;
        }
        return _bptree_record_replace_value(&leaf->records[pos], value, valuelen);
    }

    /* 根满时先分裂根，再从新的根继续插入。 */
    root = index->root;
    if (root->nkeys == index->max_keys) {
        bptree_node_t *new_root = _bptree_node_create(index, false);
        bptree_record_t promoted = {0};
        bptree_node_t *right_child = NULL;
        int rc;

        if (!new_root) {
            return -1;
        }

        new_root->children[0] = root;
        if (root->is_leaf) {
            rc = _bptree_split_leaf(index, root, &promoted, &right_child);
        } else {
            rc = _bptree_split_internal(index, root, &promoted, &right_child);
        }

        if (rc != 0) {
            free(new_root->records);
            free(new_root->children);
            free(new_root);
            return -1;
        }

        new_root->children[1] = right_child;
        _bptree_record_move(&new_root->records[0], &promoted);
        new_root->nkeys = 1;
        index->root = new_root;
    }

    {
        bptree_record_t promoted = {0};
        bptree_node_t *right_child = NULL;
        bool inserted_new_key = false;
        int rc = _bptree_insert_recursive(index,
                                          index->root,
                                          key,
                                          keylen,
                                          value,
                                          valuelen,
                                          &promoted,
                                          &right_child,
                                          &inserted_new_key);
        if (rc != 0) {
            _bptree_record_clear(&promoted);
            return rc;
        }

        if (right_child) {
            _bptree_record_clear(&promoted);
            return -1;
        }

        if (inserted_new_key) {
            index->size++;
        }
        return 0;
    }
}

int bptree_index_insert(bptree_index_t *index,
                        const void *key, uint32_t keylen,
                        const void *value, uint32_t valuelen)
{
    if (!index || !key || keylen == 0) {
        return -1;
    }
    return _bptree_insert_internal(index, key, keylen, value, valuelen, false);
}

int bptree_index_upsert(bptree_index_t *index,
                        const void *key, uint32_t keylen,
                        const void *value, uint32_t valuelen)
{
    if (!index || !key || keylen == 0) {
        return -1;
    }
    return _bptree_insert_internal(index, key, keylen, value, valuelen, true);
}