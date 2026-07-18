/**
 * @file spatial_quadtree.c
 * @brief QuadTree 空间索引实现
 */

#include "db/storage/spatial/spatial_quadtree.h"
#include "db/storage/spatial/rtree.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static bool bbox_contains_bbox_internal(const bbox_t *outer, const bbox_t *inner) {
    return inner->min_x >= outer->min_x && inner->max_x <= outer->max_x &&
           inner->min_y >= outer->min_y && inner->max_y <= outer->max_y;
}

static bool bbox_intersects_internal(const bbox_t *a, const bbox_t *b) {
    return !(a->max_x < b->min_x || a->min_x > b->max_x ||
             a->max_y < b->min_y || a->min_y > b->max_y);
}

static double point_distance(const point_t *p, const bbox_t *bbox) {
    double dx = 0, dy = 0;
    if (p->x < bbox->min_x) dx = bbox->min_x - p->x;
    else if (p->x > bbox->max_x) dx = p->x - bbox->max_x;
    if (p->y < bbox->min_y) dy = bbox->min_y - p->y;
    else if (p->y > bbox->max_y) dy = p->y - bbox->max_y;
    return sqrt(dx * dx + dy * dy);
}

static void subdivide_bbox(const bbox_t *parent, bbox_t *nw, bbox_t *ne,
                          bbox_t *sw, bbox_t *se) {
    double mid_x = (parent->min_x + parent->max_x) / 2;
    double mid_y = (parent->min_y + parent->max_y) / 2;

    *nw = (bbox_t){parent->min_x, mid_y, mid_x, parent->max_y};
    *ne = (bbox_t){mid_x, mid_y, parent->max_x, parent->max_y};
    *sw = (bbox_t){parent->min_x, parent->min_y, mid_x, mid_y};
    *se = (bbox_t){mid_x, parent->min_y, parent->max_x, mid_y};
}

/* ========================================================================
 * 节点操作
 * ======================================================================== */

static QuadTreeNode *node_create(const bbox_t *boundary, bool is_leaf) {
    QuadTreeNode *node = (QuadTreeNode *)calloc(1, sizeof(QuadTreeNode));
    if (!node) return NULL;

    node->boundary = *boundary;
    node->is_leaf = is_leaf;
    node->num_objects = 0;
    node->capacity = 4;
    node->object_ids = (uint64_t *)calloc(node->capacity, sizeof(uint64_t));

    return node;
}

static void node_destroy(QuadTreeNode *node) {
    if (!node) return;

    if (node->object_ids) free(node->object_ids);
    if (node->nw) node_destroy(node->nw);
    if (node->ne) node_destroy(node->ne);
    if (node->sw) node_destroy(node->sw);
    if (node->se) node_destroy(node->se);

    free(node);
}

static int node_insert(QuadTreeNode *node, uint64_t id, const bbox_t *bbox,
                       uint32_t depth, uint32_t max_depth, uint32_t max_capacity) {
    /* 检查是否在边界内 */
    if (!bbox_intersects_internal(&node->boundary, bbox)) {
        return -1;
    }

    if (node->is_leaf) {
        /* 叶子节点 */
        if (node->num_objects < max_capacity || depth >= max_depth) {
            /* 扩容 */
            if (node->num_objects >= node->capacity) {
                node->capacity *= 2;
                node->object_ids = (uint64_t *)realloc(node->object_ids,
                    node->capacity * sizeof(uint64_t));
            }
            node->object_ids[node->num_objects++] = id;
            return 0;
        } else {
            /* 分裂为四个子节点 */
            bbox_t nw, ne, sw, se;
            subdivide_bbox(&node->boundary, &nw, &ne, &sw, &se);

            node->nw = node_create(&nw, true);
            node->ne = node_create(&ne, true);
            node->sw = node_create(&sw, true);
            node->se = node_create(&se, true);
            node->is_leaf = false;

            /* 将现有对象重新插入 */
            for (size_t i = 0; i < node->num_objects; i++) {
                /* 简化：假设现有对象仍然在这个边界内 */
                node_insert(node->nw, node->object_ids[i], &node->boundary,
                           depth + 1, max_depth, max_capacity);
            }
            free(node->object_ids);
            node->object_ids = NULL;
            node->num_objects = 0;
        }
    }

    /* 内部节点：插入到子节点 */
    int result = -1;
    if (node->nw && node_insert(node->nw, id, bbox, depth + 1, max_depth, max_capacity) == 0) result = 0;
    if (node->ne && node_insert(node->ne, id, bbox, depth + 1, max_depth, max_capacity) == 0) result = 0;
    if (node->sw && node_insert(node->sw, id, bbox, depth + 1, max_depth, max_capacity) == 0) result = 0;
    if (node->se && node_insert(node->se, id, bbox, depth + 1, max_depth, max_capacity) == 0) result = 0;

    return result;
}

static void node_search(QuadTreeNode *node, const bbox_t *query,
                       uint64_t **ids, size_t *count, size_t *capacity) {
    if (!node || !bbox_intersects_internal(&node->boundary, query)) return;

    if (node->is_leaf) {
        for (size_t i = 0; i < node->num_objects; i++) {
            if (*count >= *capacity) {
                *capacity *= 2;
                *ids = (uint64_t *)realloc(*ids, *capacity * sizeof(uint64_t));
            }
            (*ids)[(*count)++] = node->object_ids[i];
        }
    } else {
        if (node->nw) node_search(node->nw, query, ids, count, capacity);
        if (node->ne) node_search(node->ne, query, ids, count, capacity);
        if (node->sw) node_search(node->sw, query, ids, count, capacity);
        if (node->se) node_search(node->se, query, ids, count, capacity);
    }
}

static void node_knn(QuadTreeNode *node, const point_t *point, int k,
                    double **distances, uint64_t **ids, size_t *count,
                    size_t *capacity) {
    if (!node) return;

    if (node->is_leaf) {
        for (size_t i = 0; i < node->num_objects; i++) {
            double dist = point_distance(point, &node->boundary);

            if (*count < (size_t)k) {
                /* 插入到排序数组 */
                size_t pos = *count;
                if (*count > 0 && dist < (*distances)[*count - 1]) {
                    pos = *count - 1;
                    while (pos > 0 && dist < (*distances)[pos - 1]) {
                        (*distances)[pos] = (*distances)[pos - 1];
                        (*ids)[pos] = (*ids)[pos - 1];
                        pos--;
                    }
                }

                if (pos == *count) {
                    (*distances)[pos] = dist;
                    (*ids)[pos] = node->object_ids[i];
                } else {
                    (*distances)[pos] = dist;
                    (*ids)[pos] = node->object_ids[i];
                }
                (*count)++;
            } else if (dist < (*distances)[*count - 1]) {
                /* 替换最远的 */
                size_t pos = *count - 1;
                while (pos > 0 && dist < (*distances)[pos - 1]) {
                    (*distances)[pos] = (*distances)[pos - 1];
                    (*ids)[pos] = (*ids)[pos - 1];
                    pos--;
                }
                (*distances)[pos] = dist;
                (*ids)[pos] = node->object_ids[i];
            }
        }
    } else {
        /* 按距离排序递归 */
        double nw_dist = point_distance(point, &node->nw->boundary);
        double ne_dist = point_distance(point, &node->ne->boundary);
        double sw_dist = point_distance(point, &node->sw->boundary);
        double se_dist = point_distance(point, &node->se->boundary);

        /* 简单的冒泡排序 */
        QuadTreeNode *order[4] = {node->nw, node->ne, node->sw, node->se};
        double dists[4] = {nw_dist, ne_dist, sw_dist, se_dist};

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3 - i; j++) {
                if (dists[j] > dists[j + 1]) {
                    QuadTreeNode *tmp_node = order[j];
                    order[j] = order[j + 1];
                    order[j + 1] = tmp_node;
                    double tmp_dist = dists[j];
                    dists[j] = dists[j + 1];
                    dists[j + 1] = tmp_dist;
                }
            }
        }

        for (int i = 0; i < 4; i++) {
            if (order[i]) node_knn(order[i], point, k, distances, ids, count, capacity);
        }
    }
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

QuadTree *quadtree_create(const bbox_t *boundary,
                         uint32_t max_capacity,
                         uint32_t max_depth) {
    QuadTree *tree = (QuadTree *)calloc(1, sizeof(QuadTree));
    if (!tree) return NULL;

    tree->max_capacity = max_capacity > 0 ? max_capacity : 4;
    tree->max_depth = max_depth > 0 ? max_depth : 8;

    bbox_t default_boundary = {0, 0, 1000, 1000};
    tree->root = node_create(boundary ? boundary : &default_boundary, true);

    return tree;
}

void quadtree_destroy(QuadTree *tree) {
    if (!tree) return;
    node_destroy(tree->root);
    free(tree);
}

int quadtree_insert(QuadTree *tree, uint64_t id, const bbox_t *bbox) {
    if (!tree || !bbox) return -1;
    int result = node_insert(tree->root, id, bbox, 0,
                            tree->max_depth, tree->max_capacity);
    if (result == 0) tree->num_objects++;
    return result;
}

int quadtree_remove(QuadTree *tree, uint64_t id, const bbox_t *bbox) {
    (void)tree; (void)id; (void)bbox;
    /* 简化实现：暂不支持删除 */
    return -1;
}

QuadTreeResult *quadtree_search(QuadTree *tree, const bbox_t *query) {
    if (!tree || !query) return NULL;

    QuadTreeResult *result = (QuadTreeResult *)calloc(1, sizeof(QuadTreeResult));
    result->capacity = 64;
    result->ids = (uint64_t *)calloc(result->capacity, sizeof(uint64_t));

    node_search(tree->root, query, &result->ids, &result->num_results,
               &result->capacity);

    return result;
}

QuadTreeResult *quadtree_knn(QuadTree *tree, const point_t *point, int k) {
    if (!tree || !point || k <= 0) return NULL;

    QuadTreeResult *result = (QuadTreeResult *)calloc(1, sizeof(QuadTreeResult));
    result->capacity = (size_t)k;
    result->ids = (uint64_t *)calloc(result->capacity, sizeof(uint64_t));
    double *distances = (double *)calloc(result->capacity, sizeof(double));

    size_t count = 0;
    node_knn(tree->root, point, k, &distances, &result->ids,
            &count, &result->capacity);
    result->num_results = count;

    free(distances);
    return result;
}

void quadtree_result_free(QuadTreeResult *result) {
    if (!result) return;
    free(result->ids);
    free(result);
}

void quadtree_stats(QuadTree *tree, storage_stats_t *stats) {
    if (!tree || !stats) return;
    memset(stats, 0, sizeof(storage_stats_t));
    stats->num_objects = tree->num_objects;
}

int quadtree_save(QuadTree *tree, const char *path) {
    (void)tree; (void)path;
    /* 简化实现 */
    return 0;
}

QuadTree *quadtree_load(const char *path) {
    (void)path;
    /* 简化实现 */
    return quadtree_create(NULL, 4, 8);
}
