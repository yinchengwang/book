/*
 * ttree_core.c
 *
 * T-Tree 核心操作：创建、销毁、比较、节点管理。
 */

#include "ttree_private.h"

/* 默认字节序比较。 */
static int _ttree_default_compare(const void *lhs, uint32_t lhs_len,
                                  const void *rhs, uint32_t rhs_len)
{
    uint32_t min_len = lhs_len < rhs_len ? lhs_len : rhs_len;
    int cmp;

    if (min_len == 0) {
        return lhs_len == rhs_len ? 0 : (lhs_len < rhs_len ? -1 : 1);
    }

    cmp = memcmp(lhs, rhs, min_len);
    if (cmp != 0) return cmp;
    return lhs_len == rhs_len ? 0 : (lhs_len < rhs_len ? -1 : 1);
}

int _ttree_compare(const ttree_index_t *index,
                   const void *lhs, uint32_t lhs_len,
                   const void *rhs, uint32_t rhs_len)
{
    if (index->compare) {
        return index->compare(lhs, lhs_len, rhs, rhs_len, index->compare_ctx);
    }
    return _ttree_default_compare(lhs, lhs_len, rhs, rhs_len);
}

void _ttree_record_clear(ttree_record_t *record)
{
    if (!record) return;
    free(record->key);
    free(record->value);
    memset(record, 0, sizeof(*record));
}

int _ttree_record_set(ttree_record_t *record,
                      const void *key, uint32_t keylen,
                      const void *value, uint32_t valuelen)
{
    void *key_copy = NULL;
    void *value_copy = NULL;

    if (!record || !key || keylen == 0) return -1;

    key_copy = malloc(keylen);
    if (!key_copy) return -1;
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
    record->valuelen = value_copy ? valuelen : 0;
    return 0;
}

int _ttree_record_clone(ttree_record_t *dest, const ttree_record_t *src)
{
    if (!dest || !src || !src->key || src->keylen == 0) return -1;
    return _ttree_record_set(dest, src->key, src->keylen, src->value, src->valuelen);
}

ttree_node_t *_ttree_node_create(const ttree_index_t *index)
{
    ttree_node_t *node = (ttree_node_t *)calloc(1, sizeof(ttree_node_t));
    if (!node) return NULL;

    node->records = (ttree_record_t *)calloc(index->max_keys + 1, sizeof(ttree_record_t));
    if (!node->records) {
        free(node);
        return NULL;
    }

    node->is_leaf = true;
    return node;
}

void _ttree_node_drop(ttree_node_t *node)
{
    uint32_t i;

    if (!node) return;

    for (i = 0; i < node->nkeys; i++) {
        _ttree_record_clear(&node->records[i]);
    }

    if (!node->is_leaf) {
        if (node->left)  _ttree_node_drop(node->left);
        if (node->right) _ttree_node_drop(node->right);
    }

    free(node->records);
    free(node);
}

void _ttree_node_free_shallow(ttree_node_t *node)
{
    if (!node) return;
    free(node->records);
    free(node);
}

ttree_index_t *ttree_index_create(uint32_t min_keys,
                                  ttree_compare_fn compare,
                                  void *compare_ctx)
{
    ttree_index_t *index;

    if (min_keys == 0) min_keys = TTREE_DEFAULT_MIN_KEYS;
    if (min_keys < TTREE_MIN_MIN_KEYS) min_keys = TTREE_MIN_MIN_KEYS;

    index = (ttree_index_t *)calloc(1, sizeof(ttree_index_t));
    if (!index) return NULL;

    index->min_keys = min_keys;
    index->max_keys = 2 * min_keys - 1;
    index->compare = compare;
    index->compare_ctx = compare_ctx;
    index->root = _ttree_node_create(index);
    if (!index->root) {
        free(index);
        return NULL;
    }

    return index;
}

void ttree_index_drop(ttree_index_t *index)
{
    if (!index) return;
    _ttree_node_drop(index->root);
    free(index);
}

uint32_t _ttree_compute_height(const ttree_index_t *index)
{
    const ttree_node_t *node;
    uint32_t height = 0;

    if (!index || !index->root) return 0;

    node = index->root;
    while (node) {
        height++;
        if (node->is_leaf) break;
        node = node->left;
    }
    return height;
}

uint32_t ttree_index_size(const ttree_index_t *index)
{
    return index ? index->size : 0;
}

uint32_t ttree_index_height(const ttree_index_t *index)
{
    return _ttree_compute_height(index);
}

/* 二分查找，返回第一个 >= key 的位置。 */
uint32_t _ttree_lower_bound(const ttree_index_t *index,
                            const ttree_node_t *node,
                            const void *key, uint32_t keylen,
                            bool *matched)
{
    uint32_t lo = 0, hi = node->nkeys;

    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        int cmp = _ttree_compare(index, key, keylen,
                                  node->records[mid].key, node->records[mid].keylen);
        if (cmp <= 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    if (matched) {
        *matched = (lo < node->nkeys &&
                    _ttree_compare(index, key, keylen,
                                   node->records[lo].key, node->records[lo].keylen) == 0);
    }

    return lo;
}

int _ttree_find_in_node(const ttree_index_t *index,
                        const ttree_node_t *node,
                        const void *key, uint32_t keylen)
{
    bool matched;
    uint32_t pos = _ttree_lower_bound(index, node, key, keylen, &matched);
    (void)pos;
    return matched ? (int)pos : -1;
}

const ttree_record_t *_ttree_find_record_const(const ttree_index_t *index,
                                               const ttree_node_t *node,
                                               const void *key, uint32_t keylen)
{
    int pos;

    if (!node) return NULL;

    pos = _ttree_find_in_node(index, node, key, keylen);
    if (pos >= 0) return &node->records[pos];

    if (node->is_leaf) return NULL;

    /* 根据 key 大小决定往哪棵子树找。 */
    if (node->nkeys == 0) {
        return _ttree_find_record_const(index, node->left, key, keylen);
    }

    int cmp = _ttree_compare(index, key, keylen,
                              node->records[0].key, node->records[0].keylen);
    if (cmp < 0) {
        return _ttree_find_record_const(index, node->left, key, keylen);
    }

    cmp = _ttree_compare(index, key, keylen,
                          node->records[node->nkeys - 1].key,
                          node->records[node->nkeys - 1].keylen);
    if (cmp >= 0) {
        return _ttree_find_record_const(index, node->right, key, keylen);
    }

    /* key 在中间，继续往左子树找。 */
    return _ttree_find_record_const(index, node->left, key, keylen);
}

bool _ttree_node_underflow(const ttree_index_t *index, const ttree_node_t *node)
{
    return node->nkeys < index->min_keys;
}
