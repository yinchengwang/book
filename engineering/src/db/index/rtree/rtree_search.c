/*
 * rtree_search.c
 *
 * R-Tree 搜索操作。
 */

#include "rtree_private.h"

int _rtree_node_search(const rtree_node_t *node, const rect_t *query,
                       rtree_callback_fn callback, void *ctx)
{
    uint32_t i;
    int count = 0;

    if (!node) return 0;

    /* 如果 MBR 不相交，跳过。 */
    if (!_rect_intersects(&node->mbr, query)) {
        return 0;
    }

    if (node->is_leaf) {
        for (i = 0; i < node->nentries; i++) {
            if (_rect_intersects(&node->u.entries[i].rect, query)) {
                if (callback(&node->u.entries[i].rect,
                            node->u.entries[i].data, ctx) != 0) {
                    return count;
                }
                count++;
            }
        }
    } else {
        for (i = 0; i < node->nentries; i++) {
            if (_rect_intersects(&node->u.children[i]->mbr, query)) {
                count += _rtree_node_search(node->u.children[i], query,
                                            callback, ctx);
            }
        }
    }

    return count;
}

int rtree_search(rtree_t *tree, const rect_t *query_rect,
                 rtree_callback_fn callback, void *ctx)
{
    if (!tree || !query_rect || !callback) return -1;
    return _rtree_node_search(tree->root, query_rect, callback, ctx);
}
