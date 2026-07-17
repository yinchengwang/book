/*
 * rtree_insert.c
 *
 * R-Tree 插入操作。
 */

#include "rtree_private.h"
#include <stdlib.h>
#include <string.h>

int _rtree_node_insert(rtree_node_t *node, rtree_t *tree,
                       const rect_t *rect, const void *data)
{
    uint32_t i;

    if (node->is_leaf) {
        if (node->nentries < node->max_entries) {
            node->u.entries[node->nentries].rect = *rect;
            node->u.entries[node->nentries].data = (void *)data;
            node->nentries++;
            node->mbr = _rect_union(&node->mbr, rect);
            return 0;
        }
        /* 节点满了，简化处理：直接添加（允许临时溢出）。 */
        node->u.entries[node->nentries].rect = *rect;
        node->u.entries[node->nentries].data = (void *)data;
        node->nentries++;
        node->mbr = _rect_union(&node->mbr, rect);
        return 0;
    } else {
        /* 内部节点：选择 MBR 增量最小的子节点。 */
        uint32_t best = 0;
        float best_increase = 0;
        rect_t best_mbr;

        for (i = 0; i < node->nentries; i++) {
            rect_t union_rect = _rect_union(&node->u.children[i]->mbr, rect);
            float increase = (union_rect.max_x - union_rect.min_x) *
                             (union_rect.max_y - union_rect.min_y) -
                             (node->u.children[i]->mbr.max_x - node->u.children[i]->mbr.min_x) *
                             (node->u.children[i]->mbr.max_y - node->u.children[i]->mbr.min_y);

            if (i == 0 || increase < best_increase) {
                best = i;
                best_increase = increase;
                best_mbr = union_rect;
            }
        }

        /* 更新父节点 MBR。 */
        node->u.children[best]->mbr = best_mbr;
        return _rtree_node_insert(node->u.children[best], tree, rect, data);
    }
}

int rtree_insert(rtree_t *tree, const rect_t *rect, const void *data)
{
    if (!tree || !rect) return -1;

    if (_rtree_node_insert(tree->root, tree, rect, data) == 0) {
        tree->size++;
        return 0;
    }
    return -1;
}
