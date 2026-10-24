#include "bptree_private.h"

static void _bptree_shift_records_left(bptree_node_t *node, uint32_t from)
{
    uint32_t i;

    for (i = from; i + 1u < node->nkeys; i++) {
        _bptree_record_move(&node->records[i], &node->records[i + 1u]);
    }
}

static void _bptree_shift_children_left(bptree_node_t *node, uint32_t from)
{
    uint32_t i;

    for (i = from; i + 1u <= node->nkeys; i++) {
        node->children[i] = node->children[i + 1u];
    }
}

const bptree_record_t *_bptree_leftmost_record(const bptree_node_t *node)
{
    const bptree_node_t *cursor = node;

    while (cursor && !cursor->is_leaf) {
        cursor = cursor->children[0];
    }
    if (!cursor || cursor->nkeys == 0) {
        return NULL;
    }
    return &cursor->records[0];
}

int _bptree_refresh_separator_from_child(bptree_node_t *parent,
                                         uint32_t child_pos,
                                         const bptree_index_t *index)
{
    const bptree_record_t *record;

    (void)index;

    if (!parent || child_pos == 0 || child_pos > parent->nkeys) {
        return 0;
    }

    /* 父节点的分隔键始终跟随右子树最左 key。 */
    record = _bptree_leftmost_record(parent->children[child_pos]);
    if (!record) {
        return -1;
    }

    _bptree_record_clear(&parent->records[child_pos - 1u]);
    return _bptree_record_set(&parent->records[child_pos - 1u],
                              record->key,
                              record->keylen,
                              NULL,
                              0);
}

static void _bptree_borrow_leaf_from_left(bptree_node_t *parent, uint32_t child_pos)
{
    bptree_node_t *child = parent->children[child_pos];
    bptree_node_t *left = parent->children[child_pos - 1u];
    uint32_t i;

    /* 叶子借位只移动记录，不涉及 value/child 语义变化。 */
    for (i = child->nkeys; i > 0; i--) {
        _bptree_record_move(&child->records[i], &child->records[i - 1u]);
    }
    _bptree_record_move(&child->records[0], &left->records[left->nkeys - 1u]);
    left->nkeys--;
    child->nkeys++;
}

static void _bptree_borrow_leaf_from_right(bptree_node_t *parent, uint32_t child_pos)
{
    bptree_node_t *child = parent->children[child_pos];
    bptree_node_t *right = parent->children[child_pos + 1u];

    _bptree_record_move(&child->records[child->nkeys], &right->records[0]);
    child->nkeys++;
    _bptree_shift_records_left(right, 0);
    right->nkeys--;
}

static void _bptree_borrow_internal_from_left(bptree_node_t *parent,
                                              uint32_t child_pos,
                                              const bptree_index_t *index)
{
    bptree_node_t *child = parent->children[child_pos];
    bptree_node_t *left = parent->children[child_pos - 1u];
    uint32_t i;

    for (i = child->nkeys; i > 0; i--) {
        _bptree_record_move(&child->records[i], &child->records[i - 1u]);
    }
    for (i = child->nkeys + 1u; i > 0; i--) {
        child->children[i] = child->children[i - 1u];
    }

    child->children[0] = left->children[left->nkeys];
    left->children[left->nkeys] = NULL;
    _bptree_record_move(&child->records[0], &parent->records[child_pos - 1u]);
    child->nkeys++;
    left->nkeys--;
    (void)_bptree_refresh_separator_from_child(parent, child_pos, index);
}

static void _bptree_borrow_internal_from_right(bptree_node_t *parent,
                                               uint32_t child_pos,
                                               const bptree_index_t *index)
{
    bptree_node_t *child = parent->children[child_pos];
    bptree_node_t *right = parent->children[child_pos + 1u];

    _bptree_record_move(&child->records[child->nkeys], &parent->records[child_pos]);
    child->children[child->nkeys + 1u] = right->children[0];
    child->nkeys++;

    _bptree_shift_children_left(right, 0);
    right->children[right->nkeys] = NULL;
    _bptree_shift_records_left(right, 0);
    right->nkeys--;
    (void)_bptree_refresh_separator_from_child(parent, child_pos + 1u, index);
}

static void _bptree_remove_parent_entry(bptree_node_t *parent, uint32_t pos)
{
    _bptree_record_clear(&parent->records[pos]);
    _bptree_shift_records_left(parent, pos);
    _bptree_shift_children_left(parent, pos + 1u);
    parent->nkeys--;
    parent->children[parent->nkeys + 1u] = NULL;
}

static void _bptree_merge_leaf_children(bptree_index_t *index,
                                        bptree_node_t *parent,
                                        uint32_t left_pos)
{
    bptree_node_t *left = parent->children[left_pos];
    bptree_node_t *right = parent->children[left_pos + 1u];
    uint32_t i;

    /* 合并叶子时同时维护 next_leaf，保证范围扫描链不断裂。 */
    for (i = 0; i < right->nkeys; i++) {
        _bptree_record_move(&left->records[left->nkeys + i], &right->records[i]);
    }
    left->nkeys += right->nkeys;
    left->next_leaf = right->next_leaf;
    _bptree_remove_parent_entry(parent, left_pos);
    index->leaf_count--;
    _bptree_node_free_shallow(right);
}

static void _bptree_merge_internal_children(bptree_node_t *parent,
                                            uint32_t left_pos)
{
    bptree_node_t *left = parent->children[left_pos];
    bptree_node_t *right = parent->children[left_pos + 1u];
    uint32_t i;

    _bptree_record_move(&left->records[left->nkeys], &parent->records[left_pos]);
    left->nkeys++;
    for (i = 0; i < right->nkeys; i++) {
        _bptree_record_move(&left->records[left->nkeys + i], &right->records[i]);
    }
    for (i = 0; i <= right->nkeys; i++) {
        left->children[left->nkeys + i] = right->children[i];
        right->children[i] = NULL;
    }
    left->nkeys += right->nkeys;
    _bptree_remove_parent_entry(parent, left_pos);
    _bptree_node_free_shallow(right);
}

static int _bptree_delete_from_leaf(bptree_node_t *node,
                                    const bptree_index_t *index,
                                    const void *key,
                                    uint32_t keylen,
                                    bool *deleted)
{
    bool matched = false;
    uint32_t pos = _bptree_leaf_lower_bound(index, node, key, keylen, &matched);

    if (!matched) {
        if (deleted) {
            *deleted = false;
        }
        return -1;
    }

    _bptree_record_clear(&node->records[pos]);
    _bptree_shift_records_left(node, pos);
    node->nkeys--;
    if (deleted) {
        *deleted = true;
    }
    return 0;
}

int _bptree_delete_recursive(bptree_index_t *index,
                             bptree_node_t *node,
                             const void *key, uint32_t keylen,
                             bool *deleted)
{
    /* 情况 1: 到达叶子，直接删记录。 */
    if (node->is_leaf) {
        return _bptree_delete_from_leaf(node, index, key, keylen, deleted);
    }

    {
        uint32_t child_pos = _bptree_internal_child_pos(index, node, key, keylen);
        bptree_node_t *child = node->children[child_pos];
        int rc = _bptree_delete_recursive(index, child, key, keylen, deleted);
        uint32_t min_keys = child->is_leaf ? _bptree_min_leaf_keys(index) : _bptree_min_internal_keys(index);

        if (rc != 0 || !deleted || !*deleted) {
            return -1;
        }

        if (child_pos > 0 && child->nkeys > 0) {
            if (_bptree_refresh_separator_from_child(node, child_pos, index) != 0) {
                return -1;
            }
        }

        /* 递归返回后，如果子节点仍满足最小占用，就无需进一步修复。 */
        if (child->nkeys >= min_keys || child == index->root) {
            return 0;
        }

        /* 否则按“借位优先，合并兜底”的顺序修复。 */
        if (child->is_leaf) {
            if (child_pos > 0 && node->children[child_pos - 1u]->nkeys > min_keys) {
                _bptree_borrow_leaf_from_left(node, child_pos);
                if (_bptree_refresh_separator_from_child(node, child_pos, index) != 0) {
                    return -1;
                }
            } else if (child_pos < node->nkeys && node->children[child_pos + 1u]->nkeys > min_keys) {
                _bptree_borrow_leaf_from_right(node, child_pos);
                if (_bptree_refresh_separator_from_child(node, child_pos + 1u, index) != 0) {
                    return -1;
                }
            } else if (child_pos < node->nkeys) {
                _bptree_merge_leaf_children(index, node, child_pos);
            } else {
                _bptree_merge_leaf_children(index, node, child_pos - 1u);
            }
        } else {
            if (child_pos > 0 && node->children[child_pos - 1u]->nkeys > min_keys) {
                _bptree_borrow_internal_from_left(node, child_pos, index);
            } else if (child_pos < node->nkeys && node->children[child_pos + 1u]->nkeys > min_keys) {
                _bptree_borrow_internal_from_right(node, child_pos, index);
            } else if (child_pos < node->nkeys) {
                _bptree_merge_internal_children(node, child_pos);
            } else {
                _bptree_merge_internal_children(node, child_pos - 1u);
            }
        }

        return 0;
    }
}

int bptree_index_delete(bptree_index_t *index,
                        const void *key, uint32_t keylen)
{
    bool deleted = false;

    if (!index || !key || keylen == 0 || !index->root) {
        return -1;
    }
    if (_bptree_delete_recursive(index, index->root, key, keylen, &deleted) != 0 || !deleted) {
        return -1;
    }

    index->size--;
    /* 根内部节点被删空时，把唯一孩子提升为新根。 */
    if (!index->root->is_leaf && index->root->nkeys == 0) {
        bptree_node_t *old_root = index->root;

        index->root = old_root->children[0];
        old_root->children[0] = NULL;
        _bptree_node_free_shallow(old_root);
    }
    /* 空树退化为单叶根。 */
    if (index->root->is_leaf && index->root->nkeys == 0) {
        index->leaf_count = 1u;
    }
    return 0;
}