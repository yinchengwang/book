#include "bptree_private.h"
#include "..\tree_page_private.h"

typedef struct bptree_file_meta {
    uint32_t tree_type;
    uint32_t root_page_id;
    uint32_t page_count;
    uint32_t order;
    uint32_t size;
    uint32_t leaf_count;
} bptree_file_meta_t;

typedef struct bptree_page_map_entry {
    const bptree_node_t *node;
    uint32_t page_id;
} bptree_page_map_entry_t;

static uint32_t _bptree_count_nodes(const bptree_node_t *node)
{
    uint32_t count = 1;
    uint32_t i;

    if (!node) {
        return 0;
    }
    if (node->is_leaf) {
        return 1;
    }
    for (i = 0; i <= node->nkeys; i++) {
        count += _bptree_count_nodes(node->children[i]);
    }
    return count;
}

static void _bptree_collect_nodes(const bptree_node_t *node,
                                  bptree_page_map_entry_t *entries,
                                  uint32_t *cursor)
{
    uint32_t i;

    entries[*cursor].node = node;
    entries[*cursor].page_id = *cursor + 1u;
    (*cursor)++;
    if (node->is_leaf) {
        return;
    }
    for (i = 0; i <= node->nkeys; i++) {
        _bptree_collect_nodes(node->children[i], entries, cursor);
    }
}

static uint32_t _bptree_lookup_page_id(const bptree_page_map_entry_t *entries,
                                       uint32_t count,
                                       const bptree_node_t *node)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (entries[i].node == node) {
            return entries[i].page_id;
        }
    }
    return TREE_INDEX_INVALID_PAGE_ID;
}

static const bptree_node_t *_bptree_leftmost_leaf_node(const bptree_node_t *node)
{
    const bptree_node_t *cursor = node;

    while (cursor && !cursor->is_leaf) {
        cursor = cursor->children[0];
    }
    return cursor;
}

static void _bptree_collect_leaf_links(const bptree_page_map_entry_t *entries,
                                       uint32_t count,
                                       const bptree_node_t **leaves,
                                       uint32_t *leaf_count)
{
    const bptree_node_t *leaf = _bptree_leftmost_leaf_node(entries[0].node);
    uint32_t cursor = 0;

    /* 叶子页的 prev/next_page_id 需要按 next_leaf 链重新编号。 */
    while (leaf) {
        leaves[cursor++] = leaf;
        leaf = leaf->next_leaf;
    }
    *leaf_count = cursor;
    (void)count;
}

static uint32_t _bptree_prev_leaf_page_id(const bptree_page_map_entry_t *entries,
                                          uint32_t entry_count,
                                          const bptree_node_t **leaves,
                                          uint32_t leaf_count,
                                          const bptree_node_t *target)
{
    uint32_t i;

    for (i = 0; i < leaf_count; i++) {
        if (leaves[i] == target) {
            if (i == 0) {
                return TREE_INDEX_INVALID_PAGE_ID;
            }
            return _bptree_lookup_page_id(entries, entry_count, leaves[i - 1u]);
        }
    }
    return TREE_INDEX_INVALID_PAGE_ID;
}

static uint32_t _bptree_next_leaf_page_id(const bptree_page_map_entry_t *entries,
                                          uint32_t entry_count,
                                          const bptree_node_t *leaf)
{
    if (!leaf || !leaf->next_leaf) {
        return TREE_INDEX_INVALID_PAGE_ID;
    }
    return _bptree_lookup_page_id(entries, entry_count, leaf->next_leaf);
}

static int _bptree_write_node_page(FILE *file,
                                   uint32_t page_size,
                                   const bptree_index_t *index,
                                   const bptree_page_map_entry_t *entries,
                                   uint32_t entry_count,
                                   const bptree_node_t **leaves,
                                   uint32_t leaf_count,
                                   uint32_t map_pos)
{
    const bptree_node_t *node = entries[map_pos].node;
    tree_page_builder_t builder = {0};
    uint16_t flags = node->is_leaf ? TREE_INDEX_PAGE_FLAG_LEAF : 0u;
    uint32_t i;
    int rc = -1;

    if (node == index->root) {
        flags |= TREE_INDEX_PAGE_FLAG_ROOT;
    }

    /* B+Tree 页除了 first_child_page_id 外，还需要记录叶子前后链。 */
    if (tree_page_init(&builder,
                       page_size,
                       entries[map_pos].page_id,
                       TREE_INDEX_PAGE_BPTREE_NODE,
                       flags,
                       node->is_leaf ? _bptree_prev_leaf_page_id(entries, entry_count, leaves, leaf_count, node)
                                     : TREE_INDEX_INVALID_PAGE_ID,
                       node->is_leaf ? _bptree_next_leaf_page_id(entries, entry_count, node)
                                     : TREE_INDEX_INVALID_PAGE_ID,
                       node->is_leaf ? TREE_INDEX_INVALID_PAGE_ID
                                     : _bptree_lookup_page_id(entries, entry_count, node->children[0]),
                       node->nkeys) != 0) {
        return -1;
    }

    for (i = 0; i < node->nkeys; i++) {
        uint16_t payload_len;
        uint8_t *payload;

        /* 叶子页存 key/value，内部页存 right_child_page + key。 */
        if (node->is_leaf) {
            uint32_t keylen = node->records[i].keylen;
            uint32_t valuelen = node->records[i].valuelen;

            payload_len = (uint16_t)(sizeof(uint32_t) * 2u + keylen + valuelen);
            payload = (uint8_t *)malloc(payload_len);
            if (!payload) {
                goto cleanup;
            }
            memcpy(payload, &keylen, sizeof(uint32_t));
            memcpy(payload + sizeof(uint32_t), &valuelen, sizeof(uint32_t));
            memcpy(payload + sizeof(uint32_t) * 2u, node->records[i].key, keylen);
            if (valuelen > 0) {
                memcpy(payload + sizeof(uint32_t) * 2u + keylen, node->records[i].value, valuelen);
            }
        } else {
            uint32_t right_child_page = _bptree_lookup_page_id(entries, entry_count, node->children[i + 1u]);
            uint32_t keylen = node->records[i].keylen;

            payload_len = (uint16_t)(sizeof(uint32_t) * 2u + keylen);
            payload = (uint8_t *)malloc(payload_len);
            if (!payload) {
                goto cleanup;
            }
            memcpy(payload, &right_child_page, sizeof(uint32_t));
            memcpy(payload + sizeof(uint32_t), &keylen, sizeof(uint32_t));
            memcpy(payload + sizeof(uint32_t) * 2u, node->records[i].key, keylen);
        }

        if (tree_page_add_slot(&builder, payload, payload_len) != 0) {
            free(payload);
            goto cleanup;
        }
        free(payload);
    }

    rc = tree_page_write(file, &builder);

cleanup:
    tree_page_destroy(&builder);
    return rc;
}

static bptree_node_t *_bptree_load_node(FILE *file,
                                        uint32_t page_size,
                                        uint32_t page_id,
                                        const bptree_index_t *index,
                                        bptree_node_t **cache,
                                        uint32_t cache_size)
{
    uint8_t *page;
    tree_index_page_header_t header;
    bptree_node_t *node;
    uint32_t i;

    if (page_id == TREE_INDEX_INVALID_PAGE_ID || page_id >= cache_size) {
        return NULL;
    }
    /* 用 cache 避免同一页被递归重复构造。 */
    if (cache[page_id]) {
        return cache[page_id];
    }

    page = (uint8_t *)malloc(page_size);
    if (!page) {
        return NULL;
    }
    if (tree_page_read(file, page_id, page, page_size, &header) != 0 ||
        header.page_type != TREE_INDEX_PAGE_BPTREE_NODE) {
        free(page);
        return NULL;
    }

    node = _bptree_node_create(index, (header.flags & TREE_INDEX_PAGE_FLAG_LEAF) != 0);
    if (!node) {
        free(page);
        return NULL;
    }
    cache[page_id] = node;
    node->nkeys = header.slot_count;

    if (!node->is_leaf && header.first_child_page_id != TREE_INDEX_INVALID_PAGE_ID) {
        node->children[0] = _bptree_load_node(file,
                                              page_size,
                                              header.first_child_page_id,
                                              index,
                                              cache,
                                              cache_size);
        if (!node->children[0]) {
            cache[page_id] = NULL;
            _bptree_node_drop(node);
            free(page);
            return NULL;
        }
    }

    for (i = 0; i < header.slot_count; i++) {
        const tree_index_page_slot_t *slot = tree_page_slot_at(page, &header, (uint16_t)i);
        const uint8_t *payload = tree_page_slot_payload(page, slot, page_size);

        if (!slot || !payload) {
            cache[page_id] = NULL;
            _bptree_node_drop(node);
            free(page);
            return NULL;
        }

        if (node->is_leaf) {
            uint32_t keylen;
            uint32_t valuelen;

            if (slot->length < sizeof(uint32_t) * 2u) {
                cache[page_id] = NULL;
                _bptree_node_drop(node);
                free(page);
                return NULL;
            }
            memcpy(&keylen, payload, sizeof(uint32_t));
            memcpy(&valuelen, payload + sizeof(uint32_t), sizeof(uint32_t));
            if (slot->length != sizeof(uint32_t) * 2u + keylen + valuelen ||
                _bptree_record_set(&node->records[i],
                                   payload + sizeof(uint32_t) * 2u,
                                   keylen,
                                   valuelen > 0 ? payload + sizeof(uint32_t) * 2u + keylen : NULL,
                                   valuelen) != 0) {
                cache[page_id] = NULL;
                _bptree_node_drop(node);
                free(page);
                return NULL;
            }
        } else {
            uint32_t right_child_page;
            uint32_t keylen;

            if (slot->length < sizeof(uint32_t) * 2u) {
                cache[page_id] = NULL;
                _bptree_node_drop(node);
                free(page);
                return NULL;
            }
            memcpy(&right_child_page, payload, sizeof(uint32_t));
            memcpy(&keylen, payload + sizeof(uint32_t), sizeof(uint32_t));
            if (slot->length != sizeof(uint32_t) * 2u + keylen ||
                _bptree_record_set(&node->records[i],
                                   payload + sizeof(uint32_t) * 2u,
                                   keylen,
                                   NULL,
                                   0) != 0) {
                cache[page_id] = NULL;
                _bptree_node_drop(node);
                free(page);
                return NULL;
            }
            node->children[i + 1u] = _bptree_load_node(file,
                                                       page_size,
                                                       right_child_page,
                                                       index,
                                                       cache,
                                                       cache_size);
            if (!node->children[i + 1u]) {
                cache[page_id] = NULL;
                _bptree_node_drop(node);
                free(page);
                return NULL;
            }
        }
    }

    free(page);
    return node;
}

int bptree_index_save(const bptree_index_t *index,
                      const char *path,
                      const tree_index_persist_options_t *options)
{
    uint32_t page_size;
    uint32_t node_count;
    bptree_page_map_entry_t *entries;
    const bptree_node_t **leaves;
    uint32_t cursor = 0;
    uint32_t leaf_count = 0;
    FILE *file;
    tree_page_builder_t meta_builder = {0};
    bptree_file_meta_t meta;
    uint32_t i;
    int rc = -1;

    if (!index || !path) {
        return -1;
    }

    page_size = (options && options->page_size > 0) ? options->page_size : TREE_INDEX_DEFAULT_PAGE_SIZE;
    if (page_size < TREE_INDEX_MIN_PAGE_SIZE) {
        return -1;
    }

    /* 第一步: 统计节点、分配 page_id，并单独提取叶子链顺序。 */
    node_count = _bptree_count_nodes(index->root);
    entries = (bptree_page_map_entry_t *)calloc(node_count, sizeof(bptree_page_map_entry_t));
    leaves = (const bptree_node_t **)calloc(index->leaf_count, sizeof(bptree_node_t *));
    if (!entries || !leaves) {
        free(entries);
        free(leaves);
        return -1;
    }
    _bptree_collect_nodes(index->root, entries, &cursor);
    _bptree_collect_leaf_links(entries, node_count, leaves, &leaf_count);

    file = fopen(path, "wb+");
    if (!file) {
        free(entries);
        free(leaves);
        return -1;
    }

    /* 第二步: 写 meta page。 */
    memset(&meta, 0, sizeof(meta));
    meta.tree_type = TREE_INDEX_PAGE_BPTREE_NODE;
    meta.root_page_id = 1u;
    meta.page_count = node_count;
    meta.order = index->order;
    meta.size = index->size;
    meta.leaf_count = index->leaf_count;

    if (tree_page_init(&meta_builder,
                       page_size,
                       0,
                       TREE_INDEX_PAGE_META,
                       TREE_INDEX_PAGE_FLAG_ROOT,
                       TREE_INDEX_INVALID_PAGE_ID,
                       TREE_INDEX_INVALID_PAGE_ID,
                       TREE_INDEX_INVALID_PAGE_ID,
                       1) != 0 ||
        tree_page_add_slot(&meta_builder, &meta, (uint16_t)sizeof(meta)) != 0 ||
        tree_page_write(file, &meta_builder) != 0) {
        goto cleanup;
    }

    /* 第三步: 逐页写出内部页和叶子页。 */
    for (i = 0; i < node_count; i++) {
        if (_bptree_write_node_page(file,
                                    page_size,
                                    index,
                                    entries,
                                    node_count,
                                    leaves,
                                    leaf_count,
                                    i) != 0) {
            goto cleanup;
        }
    }

    rc = 0;

cleanup:
    tree_page_destroy(&meta_builder);
    fclose(file);
    free(entries);
    free(leaves);
    return rc;
}

bptree_index_t *bptree_index_load(const char *path,
                                  bptree_compare_fn compare,
                                  void *compare_ctx)
{
    FILE *file;
    uint8_t header_page[TREE_INDEX_MIN_PAGE_SIZE];
    tree_index_page_header_t header;
    const tree_index_page_slot_t *slot;
    const uint8_t *payload;
    bptree_file_meta_t meta;
    bptree_index_t *index;
    bptree_node_t **cache;
    bptree_node_t *root;
    uint32_t page_id;

    if (!path) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    /* 第一步: 读取 meta page 头并校验页格式。 */
    if (fread(header_page, sizeof(header_page), 1, file) != 1) {
        fclose(file);
        return NULL;
    }
    memcpy(&header, header_page, sizeof(header));
    if (header.magic != TREE_INDEX_PAGE_MAGIC ||
        header.page_type != TREE_INDEX_PAGE_META ||
        header.page_size < TREE_INDEX_MIN_PAGE_SIZE) {
        fclose(file);
        return NULL;
    }
    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    {
        uint8_t *meta_page = (uint8_t *)malloc(header.page_size);
        if (!meta_page) {
            fclose(file);
            return NULL;
        }
        if (tree_page_read(file, 0, meta_page, header.page_size, &header) != 0 ||
            header.slot_count != 1) {
            free(meta_page);
            fclose(file);
            return NULL;
        }
        slot = tree_page_slot_at(meta_page, &header, 0);
        payload = tree_page_slot_payload(meta_page, slot, header.page_size);
        if (!slot || !payload || slot->length != sizeof(meta)) {
            free(meta_page);
            fclose(file);
            return NULL;
        }
        memcpy(&meta, payload, sizeof(meta));
        free(meta_page);
    }

    /* 第二步: 按 meta 创建空索引，并准备页缓存。 */
    index = bptree_index_create(meta.order, compare, compare_ctx);
    if (!index) {
        fclose(file);
        return NULL;
    }

    cache = (bptree_node_t **)calloc(meta.page_count + 1u, sizeof(bptree_node_t *));
    if (!cache) {
        bptree_index_drop(index);
        fclose(file);
        return NULL;
    }

    root = _bptree_load_node(file,
                             header.page_size,
                             meta.root_page_id,
                             index,
                             cache,
                             meta.page_count + 1u);
    if (!root) {
        free(cache);
        bptree_index_drop(index);
        fclose(file);
        return NULL;
    }

    for (page_id = 1; page_id <= meta.page_count; page_id++) {
        uint8_t *page = (uint8_t *)malloc(header.page_size);
        tree_index_page_header_t node_header;

        if (!page) {
            free(cache);
            bptree_index_drop(index);
            fclose(file);
            return NULL;
        }
        if (tree_page_read(file, page_id, page, header.page_size, &node_header) != 0) {
            free(page);
            free(cache);
            bptree_index_drop(index);
            fclose(file);
            return NULL;
        }
        if ((node_header.flags & TREE_INDEX_PAGE_FLAG_LEAF) != 0 &&
            node_header.next_page_id != TREE_INDEX_INVALID_PAGE_ID &&
            node_header.next_page_id <= meta.page_count) {
            cache[page_id]->next_leaf = cache[node_header.next_page_id];
        }
        free(page);
    }

    fclose(file);
    _bptree_node_drop(index->root);
    index->root = root;
    index->size = meta.size;
    index->leaf_count = meta.leaf_count;
    free(cache);
    return index;
}