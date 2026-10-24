/*
 * rtree_core.c
 *
 * R-Tree 核心操作。
 */

#include "rtree_private.h"
#include <stdlib.h>
#include <string.h>

rect_t _rect_union(const rect_t *a, const rect_t *b)
{
    rect_t r;
    r.min_x = a->min_x < b->min_x ? a->min_x : b->min_x;
    r.min_y = a->min_y < b->min_y ? a->min_y : b->min_y;
    r.max_x = a->max_x > b->max_x ? a->max_x : b->max_x;
    r.max_y = a->max_y > b->max_y ? a->max_y : b->max_y;
    return r;
}

bool _rect_intersects(const rect_t *a, const rect_t *b)
{
    return !(a->max_x < b->min_x || a->min_x > b->max_x ||
             a->max_y < b->min_y || a->min_y > b->max_y);
}

rtree_node_t *_rtree_node_create(rtree_t *tree, bool is_leaf)
{
    rtree_node_t *node = (rtree_node_t *)calloc(1, sizeof(rtree_node_t));
    if (!node) return NULL;

    node->is_leaf = is_leaf;
    node->nentries = 0;
    node->max_entries = tree->max_entries;

    if (is_leaf) {
        node->u.entries = (rtree_entry_t *)calloc(tree->max_entries,
                                                  sizeof(rtree_entry_t));
    } else {
        node->u.children = (rtree_node_t **)calloc(tree->max_entries + 1,
                                                   sizeof(rtree_node_t *));
    }

    node->mbr.min_x = node->mbr.min_y = 0;
    node->mbr.max_x = node->mbr.max_y = 0;

    return node;
}

void _rtree_node_drop(rtree_node_t *node)
{
    uint32_t i;

    if (!node) return;

    if (node->is_leaf) {
        free(node->u.entries);
    } else {
        for (i = 0; i < node->nentries; i++) {
            if (node->u.children[i]) {
                _rtree_node_drop(node->u.children[i]);
            }
        }
        free(node->u.children);
    }

    free(node);
}

rtree_t *rtree_create(uint32_t max_entries)
{
    rtree_t *tree = (rtree_t *)calloc(1, sizeof(rtree_t));
    if (!tree) return NULL;

    if (max_entries == 0) max_entries = RTREE_DEFAULT_MAX_ENTRIES;
    tree->max_entries = max_entries;
    tree->size = 0;
    tree->root = _rtree_node_create(tree, true);
    if (!tree->root) {
        free(tree);
        return NULL;
    }

    return tree;
}

void rtree_drop(rtree_t *tree)
{
    if (!tree) return;
    _rtree_node_drop(tree->root);
    free(tree);
}

uint32_t rtree_size(const rtree_t *tree)
{
    return tree ? tree->size : 0;
}
