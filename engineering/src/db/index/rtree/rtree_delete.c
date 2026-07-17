/*
 * rtree_delete.c
 *
 * R-Tree 删除操作（简化版）。
 */

#include "rtree_private.h"
#include <stdlib.h>

/* 简化删除：只检查第一个匹配的矩形。 */
static int _rtree_node_delete(rtree_node_t *node, const rect_t *rect, const void *data)
{
    uint32_t i;

    (void)data;

    if (node->is_leaf) {
        for (i = 0; i < node->nentries; i++) {
            if (node->u.entries[i].rect.min_x == rect->min_x &&
                node->u.entries[i].rect.min_y == rect->min_y &&
                node->u.entries[i].rect.max_x == rect->max_x &&
                node->u.entries[i].rect.max_y == rect->max_y) {
                /* 找到，删除。 */
                for (; i < node->nentries - 1; i++) {
                    node->u.entries[i] = node->u.entries[i + 1];
                }
                node->nentries--;
                return 0;
            }
        }
        return -1;
    } else {
        /* 递归删除。 */
        for (i = 0; i < node->nentries; i++) {
            if (_rect_intersects(&node->u.children[i]->mbr, rect)) {
                if (_rtree_node_delete(node->u.children[i], rect, data) == 0) {
                    return 0;
                }
            }
        }
        return -1;
    }
}

int rtree_delete(rtree_t *tree, const rect_t *rect, const void *data)
{
    if (!tree || !rect) return -1;

    if (_rtree_node_delete(tree->root, rect, data) == 0) {
        tree->size--;
        return 0;
    }
    return -1;
}
