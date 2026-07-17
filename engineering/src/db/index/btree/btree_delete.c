/*
 * btree_delete.c
 *
 * B-Tree 删除操作。
 */

#include "btree_private.h"

static void _btree_shift_records_left(btree_node_t *node, uint32_t from)
{
    uint32_t i;

    for (i = from; i + 1u < node->nkeys; i++) {
        _btree_record_move(&node->records[i], &node->records[i + 1u]);
    }
}

static void _btree_shift_children_left(btree_node_t *node, uint32_t from)
{
    uint32_t i;

    for (i = from; i + 1u <= node->nkeys; i++) {
        node->children[i] = node->children[i + 1u];
    }
}

static void _btree_rotate_from_left(btree_node_t *parent, uint32_t child_pos)
{
    btree_node_t *child = parent->children[child_pos];
    btree_node_t *left = parent->children[child_pos - 1u];
    uint32_t i;

    /* 从左兄弟借一个 key，父节点中的分隔 key 下沉到当前孩子。 */
    for (i = child->nkeys; i > 0; i--) {
        _btree_record_move(&child->records[i], &child->records[i - 1u]);
    }
    if (!child->is_leaf) {
        for (i = child->nkeys + 1u; i > 0; i--) {
            child->children[i] = child->children[i - 1u];
        }
        child->children[0] = left->children[left->nkeys];
        left->children[left->nkeys] = NULL;
    }

    _btree_record_move(&child->records[0], &parent->records[child_pos - 1u]);
    _btree_record_move(&parent->records[child_pos - 1u], &left->records[left->nkeys - 1u]);
    left->nkeys--;
    child->nkeys++;
}

static void _btree_rotate_from_right(btree_node_t *parent, uint32_t child_pos)
{
    btree_node_t *child = parent->children[child_pos];
    btree_node_t *right = parent->children[child_pos + 1u];

    /* 从右兄弟借一个 key，父节点中的分隔 key 下沉到当前孩子。 */
    _btree_record_move(&child->records[child->nkeys], &parent->records[child_pos]);
    if (!child->is_leaf) {
        child->children[child->nkeys + 1u] = right->children[0];
        _btree_shift_children_left(right, 0);
        right->children[right->nkeys] = NULL;
    }
    _btree_record_move(&parent->records[child_pos], &right->records[0]);
    _btree_shift_records_left(right, 0);
    right->nkeys--;
    child->nkeys++;
}

static void _btree_merge_children(btree_index_t *index,
                                  btree_node_t *parent,
                                  uint32_t child_pos)
{
    btree_node_t *left = parent->children[child_pos];
    btree_node_t *right = parent->children[child_pos + 1u];
    uint32_t i;

    /* 把父节点分隔 key 下沉，然后将右兄弟整体并入左兄弟。 */
    _btree_record_move(&left->records[left->nkeys], &parent->records[child_pos]);
    left->nkeys++;

    for (i = 0; i < right->nkeys; i++) {
        _btree_record_move(&left->records[left->nkeys + i], &right->records[i]);
    }
    if (!left->is_leaf) {
        for (i = 0; i <= right->nkeys; i++) {
            left->children[left->nkeys + i] = right->children[i];
            right->children[i] = NULL;
        }
    }
    left->nkeys += right->nkeys;

    _btree_shift_records_left(parent, child_pos);
    _btree_shift_children_left(parent, child_pos + 1u);
    parent->nkeys--;
    parent->children[parent->nkeys + 1u] = NULL;
    _btree_node_free_shallow(right);
    (void)index;
}

static btree_record_t *_btree_descend_max(btree_node_t *node)
{
    while (!node->is_leaf) {
        node = node->children[node->nkeys];
    }
    return &node->records[node->nkeys - 1u];
}

static btree_record_t *_btree_descend_min(btree_node_t *node)
{
    while (!node->is_leaf) {
        node = node->children[0];
    }
    return &node->records[0];
}

static int _btree_delete_at_leaf(btree_node_t *node, uint32_t pos)
{
    _btree_record_clear(&node->records[pos]);
    _btree_shift_records_left(node, pos);
    node->nkeys--;
    return 0;
}

int _btree_delete_from_node(btree_index_t *index,
                            btree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *deleted)
{
    bool matched = false;
    uint32_t pos = _btree_lower_bound(index, node, key, keylen, &matched);
    uint32_t min_keys = index->min_degree - 1u;

    /* 情况 1: 当前节点就命中 key。 */
    if (matched) {
        if (node->is_leaf) {
            if (deleted) {
                *deleted = true;
            }
            return _btree_delete_at_leaf(node, pos);
        }

        /* 优先从左子树找前驱替换，其次从右子树找后继替换。 */
        if (node->children[pos]->nkeys >= index->min_degree) {
            btree_record_t replacement = {0};
            btree_record_t *pred = _btree_descend_max(node->children[pos]);

            if (_btree_record_clone(&replacement, pred) != 0) {
                return -1;
            }
            _btree_record_clear(&node->records[pos]);
            _btree_record_move(&node->records[pos], &replacement);
            return _btree_delete_from_node(index,
                                           node->children[pos],
                                           node->records[pos].key,
                                           node->records[pos].keylen,
                                           deleted);
        }

        if (node->children[pos + 1u]->nkeys >= index->min_degree) {
            btree_record_t replacement = {0};
            btree_record_t *succ = _btree_descend_min(node->children[pos + 1u]);

            if (_btree_record_clone(&replacement, succ) != 0) {
                return -1;
            }
            _btree_record_clear(&node->records[pos]);
            _btree_record_move(&node->records[pos], &replacement);
            return _btree_delete_from_node(index,
                                           node->children[pos + 1u],
                                           node->records[pos].key,
                                           node->records[pos].keylen,
                                           deleted);
        }

        /* 左右孩子都太小，只能先合并，再继续递归删除。 */
        _btree_merge_children(index, node, pos);
        return _btree_delete_from_node(index,
                                       node->children[pos],
                                       key,
                                       keylen,
                                       deleted);
    }

    if (node->is_leaf) {
        if (deleted) {
            *deleted = false;
        }
        return -1;
    }

    /* 情况 2: 需要下探时，先保证目标孩子不是最小占用。 */
    if (node->children[pos]->nkeys == min_keys) {
        if (pos > 0 && node->children[pos - 1u]->nkeys >= index->min_degree) {
            _btree_rotate_from_left(node, pos);
        } else if (pos < node->nkeys && node->children[pos + 1u]->nkeys >= index->min_degree) {
            _btree_rotate_from_right(node, pos);
        } else if (pos < node->nkeys) {
            _btree_merge_children(index, node, pos);
        } else {
            _btree_merge_children(index, node, pos - 1u);
            pos--;
        }
    }

    return _btree_delete_from_node(index,
                                   node->children[pos],
                                   key,
                                   keylen,
                                   deleted);
}

int btree_index_delete(btree_index_t *index,
                       const void *key, uint32_t keylen)
{
    bool deleted = false;
    int rc;

    if (!index || !key || keylen == 0 || !index->root) {
        return -1;
    }

    rc = _btree_delete_from_node(index, index->root, key, keylen, &deleted);
    if (rc != 0 || !deleted) {
        return -1;
    }

    index->size--;
    /* 删除后根节点若空且不是叶子，就把树高缩短一层。 */
    if (!index->root->is_leaf && index->root->nkeys == 0) {
        btree_node_t *old_root = index->root;

        index->root = old_root->children[0];
        old_root->children[0] = NULL;
        _btree_node_free_shallow(old_root);
    }
    return 0;
}
