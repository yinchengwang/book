/*
 * rtree_private.h
 *
 * R-Tree 内部结构。
 */

#ifndef RTREE_PRIVATE_H
#define RTREE_PRIVATE_H

#include <db/index/rtree/rtree.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RTREE_DEFAULT_MAX_ENTRIES 9

/*
 * R-Tree 条目。
 */
typedef struct {
    rect_t rect;
    void *data;
} rtree_entry_t;

/*
 * R-Tree 节点。
 */
typedef struct rtree_node {
    bool is_leaf;
    uint32_t nentries;
    uint32_t max_entries;
    rect_t mbr;  /* 最小边界矩形 */
    union {
        rtree_entry_t *entries;           /* 叶子节点 */
        struct rtree_node **children;     /* 内部节点 */
    } u;
} rtree_node_t;

struct rtree {
    uint32_t max_entries;
    uint32_t size;
    rtree_node_t *root;
};

/* 辅助函数 */
rect_t _rect_union(const rect_t *a, const rect_t *b);
bool _rect_intersects(const rect_t *a, const rect_t *b);
rtree_node_t *_rtree_node_create(rtree_t *tree, bool is_leaf);
void _rtree_node_drop(rtree_node_t *node);
int _rtree_node_insert(rtree_node_t *node, rtree_t *tree,
                       const rect_t *rect, const void *data);
int _rtree_node_search(const rtree_node_t *node, const rect_t *query,
                       rtree_callback_fn callback, void *ctx);

#endif /* RTREE_PRIVATE_H */
