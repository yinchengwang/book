/*
 * btree_core.c
 *
 * B-Tree 核心操作。
 */

#include "btree_private.h"

static int _btree_default_compare(const void *lhs, uint32_t lhs_len,
                                  const void *rhs, uint32_t rhs_len)
{
    uint32_t min_len = lhs_len < rhs_len ? lhs_len : rhs_len;
    int cmp;

    if (min_len == 0) {
        if (lhs_len == rhs_len) {
            return 0;
        }
        return lhs_len < rhs_len ? -1 : 1;
    }

    cmp = memcmp(lhs, rhs, min_len);
    if (cmp != 0) {
        return cmp;
    }
    if (lhs_len == rhs_len) {
        return 0;
    }
    return lhs_len < rhs_len ? -1 : 1;
}

int _btree_compare(const btree_index_t *index,
                   const void *lhs, uint32_t lhs_len,
                   const void *rhs, uint32_t rhs_len)
{
    /* 用户提供 compare 时优先使用，否则回退到默认字节序比较。 */
    if (index->compare) {
        return index->compare(lhs, lhs_len, rhs, rhs_len, index->compare_ctx);
    }
    return _btree_default_compare(lhs, lhs_len, rhs, rhs_len);
}

void _btree_record_clear(btree_record_t *record)
{
    if (!record) {
        return;
    }
    free(record->key);
    free(record->value);
    record->key = NULL;
    record->keylen = 0;
    record->value = NULL;
    record->valuelen = 0;
}

void _btree_record_move(btree_record_t *dest, btree_record_t *src)
{
    *dest = *src;
    src->key = NULL;
    src->keylen = 0;
    src->value = NULL;
    src->valuelen = 0;
}

int _btree_record_set(btree_record_t *record,
                      const void *key, uint32_t keylen,
                      const void *value, uint32_t valuelen)
{
    void *key_copy = NULL;
    void *value_copy = NULL;

    if (!record || !key || keylen == 0) {
        return -1;
    }

    key_copy = malloc(keylen);
    if (!key_copy) {
        return -1;
    }
    memcpy(key_copy, key, keylen);

    if (value && valuelen > 0) {
        value_copy = malloc(valuelen);
        if (!value_copy) {
            free(key_copy);
            return -1;
        }
        memcpy(value_copy, value, valuelen);
    }

    record->key = key_copy;
    record->keylen = keylen;
    record->value = value_copy;
    record->valuelen = value && valuelen > 0 ? valuelen : 0;
    return 0;
}

int _btree_record_clone(btree_record_t *dest, const btree_record_t *src)
{
    if (!dest || !src || !src->key || src->keylen == 0) {
        return -1;
    }
    return _btree_record_set(dest,
                             src->key,
                             src->keylen,
                             src->value,
                             src->valuelen);
}

int _btree_record_replace_value(btree_record_t *record,
                                const void *value, uint32_t valuelen)
{
    void *value_copy = NULL;

    if (!record) {
        return -1;
    }

    if (value && valuelen > 0) {
        value_copy = malloc(valuelen);
        if (!value_copy) {
            return -1;
        }
        memcpy(value_copy, value, valuelen);
    }

    free(record->value);
    record->value = value_copy;
    record->valuelen = value && valuelen > 0 ? valuelen : 0;
    return 0;
}

btree_node_t *_btree_node_create(const btree_index_t *index, bool is_leaf)
{
    btree_node_t *node = (btree_node_t *)calloc(1, sizeof(btree_node_t));

    if (!node) {
        return NULL;
    }

    /* 节点自身和"记录数组/孩子数组"分开分配，便于 shallow free。 */
    node->records = (btree_record_t *)calloc(index->max_keys, sizeof(btree_record_t));
    node->children = (btree_node_t **)calloc(index->max_children, sizeof(btree_node_t *));
    if (!node->records || !node->children) {
        free(node->records);
        free(node->children);
        free(node);
        return NULL;
    }

    node->is_leaf = is_leaf;
    return node;
}

void _btree_node_drop(const btree_index_t *index, btree_node_t *node)
{
    uint32_t i;

    (void)index;
    if (!node) {
        return;
    }

    for (i = 0; i < node->nkeys; i++) {
        _btree_record_clear(&node->records[i]);
    }
    if (!node->is_leaf) {
        for (i = 0; i <= node->nkeys; i++) {
            _btree_node_drop(index, node->children[i]);
        }
    }

    free(node->records);
    free(node->children);
    free(node);
}

void _btree_node_free_shallow(btree_node_t *node)
{
    if (!node) {
        return;
    }
    free(node->records);
    free(node->children);
    free(node);
}

btree_index_t *btree_index_create(uint32_t min_degree,
                                  btree_compare_fn compare,
                                  void *compare_ctx)
{
    btree_index_t *index;

    if (min_degree == 0) {
        min_degree = BTREE_DEFAULT_MIN_DEGREE;
    }
    if (min_degree < BTREE_MIN_MIN_DEGREE) {
        min_degree = BTREE_MIN_MIN_DEGREE;
    }

    /* 第一步: 根据最小度推导单节点容量，并创建根节点。 */
    index = (btree_index_t *)calloc(1, sizeof(btree_index_t));
    if (!index) {
        return NULL;
    }

    index->min_degree = min_degree;
    index->max_keys = 2u * min_degree - 1u;
    index->max_children = 2u * min_degree;
    index->compare = compare;
    index->compare_ctx = compare_ctx;
    index->root = _btree_node_create(index, true);
    if (!index->root) {
        free(index);
        return NULL;
    }

    return index;
}

void btree_index_drop(btree_index_t *index)
{
    if (!index) {
        return;
    }

    /* 整棵树按后序递归释放。 */
    _btree_node_drop(index, index->root);
    free(index);
}

uint32_t _btree_compute_height(const btree_index_t *index)
{
    const btree_node_t *node;
    uint32_t height = 0;

    if (!index || !index->root) {
        return 0;
    }

    node = index->root;
    while (node) {
        height++;
        if (node->is_leaf) {
            break;
        }
        node = node->children[0];
    }

    return height;
}

uint32_t btree_index_size(const btree_index_t *index)
{
    return index ? index->size : 0;
}

uint32_t btree_index_height(const btree_index_t *index)
{
    return _btree_compute_height(index);
}
