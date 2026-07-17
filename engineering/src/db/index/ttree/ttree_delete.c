/*
 * ttree_delete.c
 *
 * T-Tree 删除操作。
 */

#include "ttree_private.h"

/* 从右兄弟借一个 key。 */
int _ttree_borrow_from_right(ttree_index_t *index,
                             ttree_node_t *node,
                             ttree_node_t *right)
{
    uint32_t i;

    if (!right || right->nkeys <= index->min_keys) {
        return -1;  /* 右兄弟没有多余的 key */
    }

    if (node->is_leaf) {
        /* 叶子节点：从右兄弟借最小的 key。 */
        ttree_record_t *rec = &right->records[0];

        /* 移动节点记录的最后一个 key 到 node。 */
        _ttree_record_clone(&node->records[node->nkeys], &node->records[node->nkeys - 1]);
        /* 把右兄弟最小的 key 复制到 node 末尾。 */
        _ttree_record_clone(&node->records[node->nkeys], rec);

        /* 更新链表。 */
        node->nkeys++;
        right->prev = right->next;
        if (right->next) {
            right->next->prev = right;
        }

        /* 从右兄弟删除借出的 key。 */
        for (i = 0; i < right->nkeys - 1; i++) {
            _ttree_record_clear(&right->records[i]);
            _ttree_record_clone(&right->records[i], &right->records[i + 1]);
        }
        _ttree_record_clear(&right->records[right->nkeys - 1]);
        right->nkeys--;

        return 0;
    } else {
        /* 内部节点：需要借一个分隔 key。 */
        /* 把右兄弟最小的 key 和它的左孩子移到当前节点。 */
        /* 简化处理：这里采用与叶子类似的逻辑。 */
        _ttree_record_clone(&node->records[node->nkeys], &right->records[0]);
        node->nkeys++;

        for (i = 0; i < right->nkeys - 1; i++) {
            _ttree_record_clear(&right->records[i]);
            _ttree_record_clone(&right->records[i], &right->records[i + 1]);
        }
        _ttree_record_clear(&right->records[right->nkeys - 1]);
        right->nkeys--;

        return 0;
    }
}

/* 从左兄弟借一个 key。 */
int _ttree_borrow_from_left(ttree_index_t *index,
                            ttree_node_t *node,
                            ttree_node_t *left)
{
    uint32_t i;

    if (!left || left->nkeys <= index->min_keys) {
        return -1;
    }

    if (node->is_leaf) {
        /* 叶子节点：从左兄弟借最大的 key。 */
        ttree_record_t *rec = &left->records[left->nkeys - 1];

        /* 将节点所有记录右移一位。 */
        for (i = node->nkeys; i > 0; i--) {
            _ttree_record_clear(&node->records[i]);
            _ttree_record_clone(&node->records[i], &node->records[i - 1]);
        }

        /* 从左兄弟借最大的 key 到节点第一个位置。 */
        _ttree_record_clear(&node->records[0]);
        _ttree_record_clone(&node->records[0], rec);

        /* 更新链表。 */
        node->nkeys++;
        left->nkeys--;
        left->next = node;

        return 0;
    } else {
        /* 内部节点。 */
        /* 节点所有记录右移。 */
        for (i = node->nkeys; i > 0; i--) {
            _ttree_record_clear(&node->records[i]);
            _ttree_record_clone(&node->records[i], &node->records[i - 1]);
        }

        /* 从左兄弟借最大的 key。 */
        _ttree_record_clear(&node->records[0]);
        _ttree_record_clone(&node->records[0], &left->records[left->nkeys - 1]);
        node->nkeys++;
        left->nkeys--;

        return 0;
    }
}

/* 合并 node 和右兄弟 right。合并后 key 总数 < 2*min_keys。 */
int _ttree_merge_with_right(ttree_index_t *index,
                            ttree_node_t *node,
                            ttree_node_t *right)
{
    uint32_t i;

    (void)index;

    /* 将右兄弟的所有 key 移到当前节点。 */
    for (i = 0; i < right->nkeys; i++) {
        _ttree_record_clone(&node->records[node->nkeys], &right->records[i]);
        node->nkeys++;
        _ttree_record_clear(&right->records[i]);
    }

    if (node->is_leaf) {
        /* 更新链表。 */
        node->next = right->next;
        if (right->next) {
            right->next->prev = node;
        }
    } else {
        /* 内部节点还需要处理子树。 */
        /* 简化：这里不做复杂的子树合并。 */
        if (node->right == right) {
            node->right = NULL;
        }
    }

    /* 释放右兄弟。 */
    free(right->records);
    free(right);

    return 0;
}

/* 递归删除 key。 */
int _ttree_delete_recursive(ttree_index_t *index,
                            ttree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *deleted)
{
    int pos;
    ttree_node_t *target;
    int cmp;

    if (!node) return -1;

    /* 在当前节点查找。 */
    pos = _ttree_find_in_node(index, node, key, keylen);

    if (pos >= 0) {
        /* 在当前节点找到了。 */
        if (node->is_leaf) {
            /* 叶子节点：直接删除。 */
            uint32_t i;
            for (i = pos; i < node->nkeys - 1; i++) {
                _ttree_record_clear(&node->records[i]);
                _ttree_record_clone(&node->records[i], &node->records[i + 1]);
            }
            _ttree_record_clear(&node->records[node->nkeys - 1]);
            node->nkeys--;
            *deleted = true;
            index->size--;
            return 0;
        } else {
            /* 内部节点：用后继 key 替换。 */
            /* 简化：用后继叶子节点的最小 key 替换。 */
            ttree_node_t *leaf = node->right;
            while (leaf && !leaf->is_leaf) {
                leaf = leaf->left;
            }
            if (leaf && leaf->nkeys > 0) {
                _ttree_record_clear(&node->records[pos]);
                _ttree_record_clone(&node->records[pos], &leaf->records[0]);
                key = leaf->records[0].key;
                keylen = leaf->records[0].keylen;
                node = leaf;
                pos = 0;
            } else {
                return -1;
            }
        }
    }

    /* 如果是叶子节点且没找到，返回错误。 */
    if (node->is_leaf) {
        *deleted = false;
        return 0;
    }

    /* 在子树中查找要删除的 key。 */
    if (node->nkeys == 0) {
        target = node->left;
    } else {
        cmp = _ttree_compare(index, key, keylen,
                              node->records[0].key, node->records[0].keylen);
        if (cmp < 0) {
            target = node->left;
        } else {
            cmp = _ttree_compare(index, key, keylen,
                                  node->records[node->nkeys - 1].key,
                                  node->records[node->nkeys - 1].keylen);
            if (cmp >= 0) {
                target = node->right;
            } else {
                target = node->left;
            }
        }
    }

    /* 递归删除。 */
    if (_ttree_delete_recursive(index, target, key, keylen, deleted) != 0) {
        return -1;
    }

    /* 检查是否需要再平衡。 */
    if (*deleted && _ttree_node_underflow(index, target)) {
        /* 尝试借或合并。简化处理：合并到左兄弟。 */
        if (node->left && node->left->nkeys > index->min_keys) {
            _ttree_borrow_from_left(index, target, node->left);
        } else if (node->right && node->right->nkeys > index->min_keys) {
            _ttree_borrow_from_right(index, target, node->right);
        } else if (node->left) {
            _ttree_merge_with_right(index, node->left, target);
        } else if (node->right) {
            _ttree_merge_with_right(index, target, node->right);
        }
    }

    return 0;
}

/* 主入口：删除 key。 */
int ttree_index_delete(ttree_index_t *index,
                       const void *key, uint32_t keylen)
{
    bool deleted = false;

    if (!index || !key || keylen == 0) return -1;

    if (index->size == 0) return -1;

    if (_ttree_delete_recursive(index, index->root, key, keylen, &deleted) != 0) {
        return -1;
    }

    return deleted ? 0 : -1;
}
