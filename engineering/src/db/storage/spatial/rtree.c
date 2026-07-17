/**
 * @file rtree.c
 * @brief R-Tree 空间索引实现
 */
#include "db/storage/spatial/rtree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#define RTREE_DEFAULT_MAX_ENTRIES 16
#define RTREE_MIN_FILL_RATE 0.4

/* ========================================================================
 * 内存分配
 * ======================================================================== */

static rtree_node_t *rtree_node_create(rtree_t *tree, bool is_leaf) {
    rtree_node_t *node = (rtree_node_t *)calloc(1, sizeof(rtree_node_t));
    if (!node) return NULL;

    node->is_leaf = is_leaf;
    node->num_entries = 0;
    node->bbox.min_x = node->bbox.min_y = DBL_MAX;
    node->bbox.max_x = node->bbox.max_y = -DBL_MAX;

    if (is_leaf) {
        node->data.leaf.ids = (uint64_t *)calloc(tree->max_entries, sizeof(uint64_t));
        node->data.leaf.bboxes = (bbox_t *)calloc(tree->max_entries, sizeof(bbox_t));
        if (!node->data.leaf.ids || !node->data.leaf.bboxes) {
            free(node->data.leaf.ids);
            free(node->data.leaf.bboxes);
            free(node);
            return NULL;
        }
    } else {
        node->data.internal.children = (rtree_node_t **)calloc(tree->max_entries, sizeof(rtree_node_t *));
        if (!node->data.internal.children) {
            free(node);
            return NULL;
        }
    }

    return node;
}

static void rtree_node_free(rtree_node_t *node) {
    if (!node) return;

    if (node->is_leaf) {
        free(node->data.leaf.ids);
        free(node->data.leaf.bboxes);
    } else {
        for (uint32_t i = 0; i < node->num_entries; i++) {
            rtree_node_free(node->data.internal.children[i]);
        }
        free(node->data.internal.children);
    }
    free(node);
}

/* ========================================================================
 * 边界框操作
 * ======================================================================== */

bbox_t bbox_create(double min_x, double min_y, double max_x, double max_y) {
    bbox_t bbox;
    bbox.min_x = min_x;
    bbox.min_y = min_y;
    bbox.max_x = max_x;
    bbox.max_y = max_y;
    return bbox;
}

bool bbox_contains_point(const bbox_t *bbox, const point_t *point) {
    return (point->x >= bbox->min_x && point->x <= bbox->max_x &&
            point->y >= bbox->min_y && point->y <= bbox->max_y);
}

bool bbox_intersects(const bbox_t *a, const bbox_t *b) {
    return !(a->max_x < b->min_x || a->min_x > b->max_x ||
             a->max_y < b->min_y || a->min_y > b->max_y);
}

double bbox_area(const bbox_t *bbox) {
    if (bbox->max_x <= bbox->min_x || bbox->max_y <= bbox->min_y) {
        return 0.0;
    }
    return (bbox->max_x - bbox->min_x) * (bbox->max_y - bbox->min_y);
}

double bbox_area_enlarged(const bbox_t *bbox, const bbox_t *other) {
    bbox_t united = bbox_union(bbox, other);
    return bbox_area(&united);
}

double bbox_point_distance(const bbox_t *bbox, const point_t *point) {
    double dx = 0.0, dy = 0.0;

    if (point->x < bbox->min_x) {
        dx = bbox->min_x - point->x;
    } else if (point->x > bbox->max_x) {
        dx = point->x - bbox->max_x;
    }

    if (point->y < bbox->min_y) {
        dy = bbox->min_y - point->y;
    } else if (point->y > bbox->max_y) {
        dy = point->y - bbox->max_y;
    }

    return sqrt(dx * dx + dy * dy);
}

bbox_t bbox_union(const bbox_t *a, const bbox_t *b) {
    bbox_t result;
    result.min_x = (a->min_x < b->min_x) ? a->min_x : b->min_x;
    result.min_y = (a->min_y < b->min_y) ? a->min_y : b->min_y;
    result.max_x = (a->max_x > b->max_x) ? a->max_x : b->max_x;
    result.max_y = (a->max_y > b->max_y) ? a->max_y : b->max_y;
    return result;
}

static void bbox_update(bbox_t *dest, const bbox_t *src) {
    if (src->min_x < dest->min_x) dest->min_x = src->min_x;
    if (src->min_y < dest->min_y) dest->min_y = src->min_y;
    if (src->max_x > dest->max_x) dest->max_x = src->max_x;
    if (src->max_y > dest->max_y) dest->max_y = src->max_y;
}

/* ========================================================================
 * R-Tree 核心实现
 * ======================================================================== */

rtree_t *rtree_create(int max_entries) {
    if (max_entries <= 0) {
        max_entries = RTREE_DEFAULT_MAX_ENTRIES;
    }

    rtree_t *tree = (rtree_t *)calloc(1, sizeof(rtree_t));
    if (!tree) return NULL;

    tree->max_entries = (uint32_t)max_entries;
    tree->min_entries = (uint32_t)(max_entries * RTREE_MIN_FILL_RATE);
    if (tree->min_entries < 2) tree->min_entries = 2;

    tree->root = rtree_node_create(tree, true);
    if (!tree->root) {
        free(tree);
        return NULL;
    }

    tree->num_items = 0;
    tree->height = 1;

    return tree;
}

void rtree_free(rtree_t *tree) {
    if (!tree) return;
    rtree_node_free(tree->root);
    free(tree);
}

/**
 * @brief 计算节点边界框
 */
static void rtree_node_compute_bbox(rtree_node_t *node) {
    if (node->num_entries == 0) {
        node->bbox.min_x = node->bbox.min_y = DBL_MAX;
        node->bbox.max_x = node->bbox.max_y = -DBL_MAX;
        return;
    }

    if (node->is_leaf) {
        node->bbox = node->data.leaf.bboxes[0];
        for (uint32_t i = 1; i < node->num_entries; i++) {
            node->bbox = bbox_union(&node->bbox, &node->data.leaf.bboxes[i]);
        }
    } else {
        node->bbox = node->data.internal.children[0]->bbox;
        for (uint32_t i = 1; i < node->num_entries; i++) {
            node->bbox = bbox_union(&node->bbox, &node->data.internal.children[i]->bbox);
        }
    }
}

/**
 * @brief 查找最佳子节点（面积增量最小）
 */
static int rtree_pick_seeds(const rtree_node_t *node,
                            const bbox_t *bbox,
                            int *seed1, int *seed2) {
    /* 简单策略：选择距离最远的两个边界框 */
    double max_dist = -1.0;

    for (uint32_t i = 0; i < node->num_entries; i++) {
        for (uint32_t j = i + 1; j < node->num_entries; j++) {
            bbox_t a, b;
            if (node->is_leaf) {
                a = node->data.leaf.bboxes[i];
                b = node->data.leaf.bboxes[j];
            } else {
                a = node->data.internal.children[i]->bbox;
                b = node->data.internal.children[j]->bbox;
            }

            double dx = (a.min_x + a.max_x) - (b.min_x + b.max_x);
            double dy = (a.min_y + a.max_y) - (b.min_y + b.min_y);
            double dist = dx * dx + dy * dy;

            if (dist > max_dist) {
                max_dist = dist;
                *seed1 = (int)i;
                *seed2 = (int)j;
            }
        }
    }

    return 0;
}

/**
 * @brief 分配条目到组
 */
static void rtree_quadratic_split(rtree_node_t *node,
                                  int start, int num,
                                  rtree_node_t *group_a,
                                  rtree_node_t *group_b) {
    bbox_t bbox_a, bbox_b;
    bool bbox_a_valid = false, bbox_b_valid = false;

    /* 初始化分组 */
    if (node->is_leaf) {
        group_a->data.leaf.ids[0] = node->data.leaf.ids[start];
        group_a->data.leaf.bboxes[0] = node->data.leaf.bboxes[start];
        group_a->num_entries = 1;
        bbox_a = node->data.leaf.bboxes[start];
        bbox_a_valid = true;

        group_b->data.leaf.ids[0] = node->data.leaf.ids[start + num - 1];
        group_b->data.leaf.bboxes[0] = node->data.leaf.bboxes[start + num - 1];
        group_b->num_entries = 1;
        bbox_b = node->data.leaf.bboxes[start + num - 1];
        bbox_b_valid = true;
    } else {
        group_a->data.internal.children[0] = node->data.internal.children[start];
        group_a->num_entries = 1;
        bbox_a = node->data.internal.children[start]->bbox;
        bbox_a_valid = true;

        group_b->data.internal.children[0] = node->data.internal.children[start + num - 1];
        group_b->num_entries = 1;
        bbox_b = node->data.internal.children[start + num - 1]->bbox;
        bbox_b_valid = true;
    }

    /* 分配剩余条目 */
    for (int i = 1; i < num - 1; i++) {
        int idx = start + i;
        bbox_t bbox_i = node->is_leaf ? node->data.leaf.bboxes[idx] : node->data.internal.children[idx]->bbox;

        double area_a = bbox_a_valid ? bbox_area(&bbox_a) : 0;
        double area_b = bbox_b_valid ? bbox_area(&bbox_b) : 0;
        double enlarge_a = bbox_area_enlarged(&bbox_a, &bbox_i) - area_a;
        double enlarge_b = bbox_area_enlarged(&bbox_b, &bbox_i) - area_b;

        /* 选择面积增量小的组 */
        if (enlarge_a < enlarge_b || (enlarge_a == enlarge_b && area_a <= area_b)) {
            if (node->is_leaf) {
                group_a->data.leaf.ids[group_a->num_entries] = node->data.leaf.ids[idx];
                group_a->data.leaf.bboxes[group_a->num_entries] = node->data.leaf.bboxes[idx];
            } else {
                group_a->data.internal.children[group_a->num_entries] = node->data.internal.children[idx];
            }
            group_a->num_entries++;
            if (bbox_a_valid) {
                bbox_a = bbox_union(&bbox_a, &bbox_i);
            }
        } else {
            if (node->is_leaf) {
                group_b->data.leaf.ids[group_b->num_entries] = node->data.leaf.ids[idx];
                group_b->data.leaf.bboxes[group_b->num_entries] = node->data.leaf.bboxes[idx];
            } else {
                group_b->data.internal.children[group_b->num_entries] = node->data.internal.children[idx];
            }
            group_b->num_entries++;
            if (bbox_b_valid) {
                bbox_b = bbox_union(&bbox_b, &bbox_i);
            }
        }
    }

    group_a->bbox = bbox_a;
    group_b->bbox = bbox_b;
}

/* 简化的插入函数 - 直接插入叶子节点 */
static bool rtree_insert_impl(rtree_t *tree, rtree_node_t *node,
                              uint64_t id, const bbox_t *bbox) {
    if (node->is_leaf) {
        /* 叶子节点，直接插入 */
        if (node->num_entries >= tree->max_entries) {
            return false;  /* 节点已满，需要分裂 */
        }

        node->data.leaf.ids[node->num_entries] = id;
        node->data.leaf.bboxes[node->num_entries] = *bbox;
        node->num_entries++;
        bbox_update(&node->bbox, bbox);
        tree->num_items++;
        return true;
    } else {
        /* 非叶子节点，选择最佳子节点 */
        int best_child = 0;
        double best_area = bbox_area(&node->data.internal.children[0]->bbox);
        double best_enlarged = bbox_area_enlarged(&node->data.internal.children[0]->bbox, bbox) - best_area;

        for (uint32_t i = 1; i < node->num_entries; i++) {
            bbox_t child_bbox = node->data.internal.children[i]->bbox;
            double area = bbox_area(&child_bbox);
            double enlarged = bbox_area_enlarged(&child_bbox, bbox) - area;

            if (enlarged < best_enlarged || (enlarged == best_enlarged && area < best_area)) {
                best_child = i;
                best_area = area;
                best_enlarged = enlarged;
            }
        }

        rtree_node_t *child = node->data.internal.children[best_child];
        if (rtree_insert_impl(tree, child, id, bbox)) {
            rtree_node_compute_bbox(node);
            return true;
        }

        /* 子节点已满，尝试分裂 */
        /* 简化实现：如果子节点已满，先分裂子节点 */
        /* 这里简化处理：直接添加到当前节点，不分裂 */
        if (node->num_entries < tree->max_entries) {
            rtree_node_t *new_child = rtree_node_create(tree, true);
            if (new_child) {
                new_child->data.leaf.ids[0] = id;
                new_child->data.leaf.bboxes[0] = *bbox;
                new_child->num_entries = 1;
                new_child->bbox = *bbox;
                node->data.internal.children[node->num_entries] = new_child;
                node->num_entries++;
                tree->num_items++;
                rtree_node_compute_bbox(node);
                return true;
            }
        }
    }

    return false;
}

int rtree_insert(rtree_t *tree, uint64_t id, const bbox_t *bbox) {
    if (!tree || !bbox) return -1;

    if (rtree_insert_impl(tree, tree->root, id, bbox)) {
        return 0;
    }

    /* 根节点已满，需要分裂根节点 */
    /* 简化实现：扩展树高度 */
    rtree_node_t *new_root = rtree_node_create(tree, false);
    if (!new_root) return -1;

    new_root->data.internal.children[0] = tree->root;
    new_root->num_entries = 1;
    new_root->bbox = tree->root->bbox;

    /* 添加新条目到新根节点 */
    rtree_node_t *new_child = rtree_node_create(tree, true);
    if (!new_child) {
        free(new_root);
        return -1;
    }

    new_child->data.leaf.ids[0] = id;
    new_child->data.leaf.bboxes[0] = *bbox;
    new_child->num_entries = 1;
    new_child->bbox = *bbox;

    new_root->data.internal.children[1] = new_child;
    new_root->num_entries = 2;
    new_root->bbox = bbox_union(&tree->root->bbox, bbox);

    tree->root = new_root;
    tree->height++;
    tree->num_items++;

    return 0;
}

int rtree_remove(rtree_t *tree, uint64_t id, const bbox_t *bbox) {
    (void)tree;
    (void)id;
    (void)bbox;
    /* 简化实现：暂不支持删除 */
    return -1;
}

/* ========================================================================
 * 搜索实现
 * ======================================================================== */

static int rtree_search_impl(const rtree_node_t *node, const bbox_t *query,
                             rtree_result_t *results, int max_results, int *count) {
    if (!node || *count >= max_results) return *count;

    if (node->is_leaf) {
        for (uint32_t i = 0; i < node->num_entries && *count < max_results; i++) {
            if (bbox_intersects(&node->data.leaf.bboxes[i], query)) {
                results[*count].id = node->data.leaf.ids[i];
                results[*count].bbox = node->data.leaf.bboxes[i];
                results[*count].distance = 0;
                (*count)++;
            }
        }
    } else {
        for (uint32_t i = 0; i < node->num_entries && *count < max_results; i++) {
            if (bbox_intersects(&node->data.internal.children[i]->bbox, query)) {
                rtree_search_impl(node->data.internal.children[i], query,
                                  results, max_results, count);
            }
        }
    }

    return *count;
}

int rtree_search(rtree_t *tree, const bbox_t *query,
                  rtree_result_t *results, int max_results) {
    if (!tree || !query || !results || max_results <= 0) return 0;

    int count = 0;
    rtree_search_impl(tree->root, query, results, max_results, &count);
    return count;
}

/* ========================================================================
 * KNN 实现
 * ======================================================================== */

typedef struct knn_entry_s {
    double distance;
    uint64_t id;
    bbox_t bbox;
} knn_entry_t;

static int knn_compare(const void *a, const void *b) {
    double da = ((knn_entry_t *)a)->distance;
    double db = ((knn_entry_t *)b)->distance;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

int rtree_knn(rtree_t *tree, const point_t *point, int k,
               rtree_result_t *results, int max_results) {
    if (!tree || !point || !results || k <= 0) return 0;

    /* 简化实现：收集所有项，计算距离，排序取前 k 个 */
    int capacity = (int)tree->num_items;
    if (capacity <= 0) capacity = 256;

    knn_entry_t *entries = (knn_entry_t *)calloc(capacity, sizeof(knn_entry_t));
    if (!entries) return 0;

    int count = 0;

    /* 深度优先收集所有项 */
    typedef struct {
        const rtree_node_t *node;
        size_t idx;
    } stack_t;

    stack_t *stack = (stack_t *)calloc(tree->height * 100, sizeof(stack_t));
    if (!stack) {
        free(entries);
        return 0;
    }

    size_t stack_top = 0;
    stack[stack_top++] = (stack_t){tree->root, 0};

    while (stack_top > 0) {
        stack_t current = stack[--stack_top];
        const rtree_node_t *node = current.node;
        size_t idx = current.idx;

        if (node->is_leaf) {
            for (uint32_t i = 0; i < node->num_entries && count < capacity; i++) {
                entries[count].id = node->data.leaf.ids[i];
                entries[count].bbox = node->data.leaf.bboxes[i];
                entries[count].distance = bbox_point_distance(&node->data.leaf.bboxes[i], point);
                count++;
            }
        } else {
            if (idx < node->num_entries) {
                /* 压入当前节点的后续和子节点 */
                stack[stack_top++] = (stack_t){node, idx + 1};
                stack[stack_top++] = (stack_t){node->data.internal.children[idx], 0};
            }
        }
    }

    free(stack);

    /* 排序 */
    qsort(entries, count, sizeof(knn_entry_t), knn_compare);

    /* 取前 k 个 */
    int result_count = count < k ? count : k;
    for (int i = 0; i < result_count; i++) {
        results[i].id = entries[i].id;
        results[i].bbox = entries[i].bbox;
        results[i].distance = entries[i].distance;
    }

    free(entries);
    return result_count;
}

/* ========================================================================
 * 持久化
 * ======================================================================== */

int rtree_save(rtree_t *tree, const char *path) {
    if (!tree || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头 */
    uint32_t magic = 0x52545245;  /* 'RTRE' */
    uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&tree->num_items, sizeof(tree->num_items), 1, fp);
    fwrite(&tree->height, sizeof(tree->height), 1, fp);

    /* 递归写入节点（简化：只保存叶子节点数据） */
    typedef struct {
        const rtree_node_t *node;
        uint32_t parent_idx;
        uint32_t idx_in_parent;
    } node_stack_t;

    node_stack_t *stack = (node_stack_t *)calloc(10000, sizeof(node_stack_t));
    uint32_t node_count = 0;
    size_t stack_top = 0;

    stack[stack_top++] = (node_stack_t){tree->root, 0, 0};

    while (stack_top > 0) {
        node_stack_t current = stack[--stack_top];

        for (uint32_t i = 0; i < current.node->num_entries; i++) {
            if (current.node->is_leaf) {
                fwrite(&current.node->data.leaf.ids[i], sizeof(uint64_t), 1, fp);
                fwrite(&current.node->data.leaf.bboxes[i], sizeof(bbox_t), 1, fp);
                node_count++;
            } else {
                stack[stack_top++] = (node_stack_t){
                    current.node->data.internal.children[i], 0, i
                };
            }
        }
    }

    free(stack);
    fclose(fp);
    return 0;
}

rtree_t *rtree_load(const char *path) {
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头 */
    uint32_t magic, version;
    uint32_t num_items, height;

    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        fread(&num_items, sizeof(num_items), 1, fp) != 1 ||
        fread(&height, sizeof(height), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (magic != 0x52545245 || version != 1) {
        fclose(fp);
        return NULL;
    }

    rtree_t *tree = rtree_create(0);
    if (!tree) {
        fclose(fp);
        return NULL;
    }

    tree->num_items = num_items;
    tree->height = height;

    /* 读取所有条目 */
    for (uint32_t i = 0; i < num_items; i++) {
        uint64_t id;
        bbox_t bbox;

        if (fread(&id, sizeof(id), 1, fp) != 1 ||
            fread(&bbox, sizeof(bbox), 1, fp) != 1) {
            rtree_free(tree);
            fclose(fp);
            return NULL;
        }

        rtree_insert(tree, id, &bbox);
    }

    fclose(fp);
    return tree;
}

void rtree_stats(rtree_t *tree, rtree_stats_t *stats) {
    if (!tree || !stats) return;

    stats->num_items = tree->num_items;
    stats->height = tree->height;
    stats->max_fill = tree->max_entries;

    /* 统计节点数 */
    uint32_t node_count = 0;
    typedef struct {
        const rtree_node_t *node;
    } stack_t;

    stack_t *stack = (stack_t *)calloc(10000, sizeof(stack_t));
    size_t stack_top = 0;
    stack[stack_top++] = (stack_t){tree->root};

    while (stack_top > 0) {
        stack_t current = stack[--stack_top];
        node_count++;

        if (!current.node->is_leaf) {
            for (uint32_t i = 0; i < current.node->num_entries; i++) {
                stack[stack_top++] = (stack_t){current.node->data.internal.children[i]};
            }
        }
    }

    free(stack);
    stats->num_nodes = node_count;
}
