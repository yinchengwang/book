#include "bptree_private.h"

static int _bptree_default_compare(const void *lhs, uint32_t lhs_len,
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

int _bptree_compare(const bptree_index_t *index,
                    const void *lhs, uint32_t lhs_len,
                    const void *rhs, uint32_t rhs_len)
{
    /* 用户 compare 优先，否则回退到默认字节序比较。 */
    if (index->compare) {
        return index->compare(lhs, lhs_len, rhs, rhs_len, index->compare_ctx);
    }
    return _bptree_default_compare(lhs, lhs_len, rhs, rhs_len);
}

void _bptree_record_clear(bptree_record_t *record)
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

void _bptree_record_move(bptree_record_t *dest, bptree_record_t *src)
{
    *dest = *src;
    src->key = NULL;
    src->keylen = 0;
    src->value = NULL;
    src->valuelen = 0;
}

int _bptree_record_set(bptree_record_t *record,
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

int _bptree_record_clone(bptree_record_t *dest, const bptree_record_t *src)
{
    if (!dest || !src || !src->key || src->keylen == 0) {
        return -1;
    }
    return _bptree_record_set(dest,
                              src->key,
                              src->keylen,
                              src->value,
                              src->valuelen);
}

int _bptree_record_replace_value(bptree_record_t *record,
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

bptree_node_t *_bptree_node_create(const bptree_index_t *index, bool is_leaf)
{
    bptree_node_t *node = (bptree_node_t *)calloc(1, sizeof(bptree_node_t));

    if (!node) {
        return NULL;
    }

    /* +1 预留给分裂前的临时溢出插入。 */
    node->records = (bptree_record_t *)calloc(index->max_keys + 1u, sizeof(bptree_record_t));
    node->children = (bptree_node_t **)calloc(index->max_children + 1u, sizeof(bptree_node_t *));
    if (!node->records || !node->children) {
        free(node->records);
        free(node->children);
        free(node);
        return NULL;
    }

    node->is_leaf = is_leaf;
    return node;
}

void _bptree_node_drop(bptree_node_t *node)
{
    uint32_t i;

    if (!node) {
        return;
    }

    for (i = 0; i < node->nkeys; i++) {
        _bptree_record_clear(&node->records[i]);
    }

    if (!node->is_leaf) {
        for (i = 0; i <= node->nkeys; i++) {
            _bptree_node_drop(node->children[i]);
        }
    }

    free(node->records);
    free(node->children);
    free(node);
}

void _bptree_node_free_shallow(bptree_node_t *node)
{
    if (!node) {
        return;
    }
    free(node->records);
    free(node->children);
    free(node);
}

bptree_index_t *bptree_index_create(uint32_t order,
                                    bptree_compare_fn compare,
                                    void *compare_ctx)
{
    bptree_index_t *index;

    if (order == 0) {
        order = BPTREE_DEFAULT_ORDER;
    }
    if (order < BPTREE_MIN_ORDER) {
        order = BPTREE_MIN_ORDER;
    }

    /* 第一步: 根据树阶推导节点容量，并创建初始叶子根。 */
    index = (bptree_index_t *)calloc(1, sizeof(bptree_index_t));
    if (!index) {
        return NULL;
    }

    index->order = order;
    index->max_children = order;
    index->max_keys = order - 1u;
    index->compare = compare;
    index->compare_ctx = compare_ctx;
    index->root = _bptree_node_create(index, true);
    if (!index->root) {
        free(index);
        return NULL;
    }
    index->leaf_count = 1u;

    return index;
}

void bptree_index_drop(bptree_index_t *index)
{
    if (!index) {
        return;
    }

    /* B+Tree 同样按后序释放整棵树。 */
    _bptree_node_drop(index->root);
    free(index);
}

uint32_t _bptree_compute_height(const bptree_index_t *index)
{
    const bptree_node_t *node;
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

uint32_t bptree_index_size(const bptree_index_t *index)
{
    return index ? index->size : 0;
}

uint32_t bptree_index_height(const bptree_index_t *index)
{
    return _bptree_compute_height(index);
}

uint32_t bptree_index_leaf_count(const bptree_index_t *index)
{
    return index ? index->leaf_count : 0;
}

uint32_t _bptree_min_leaf_keys(const bptree_index_t *index)
{
    return index ? ((index->max_keys + 1u) / 2u) : 0;
}

uint32_t _bptree_min_internal_keys(const bptree_index_t *index)
{
    return index ? (((index->order + 1u) / 2u) - 1u) : 0;
}