/*
 * gist.c — GiST (Generalized Search Tree) 实现
 *
 * ============================================================
 * 核心原理
 * ============================================================
 * - GiST 是 B-Tree 的通用化版本，支持任意数据类型
 * - 用户通过实现操作符接口来定义具体行为
 * - 典型的操作符包括：Union、Consistent、Distance
 *
 * ============================================================
 * 设计要点
 * ============================================================
 * 1. 采用简化版的 R-Tree 结构（分裂策略简化）
 * 2. 叶子节点存储实际的 (bbox, value) 对
 * 3. 非叶子节点存储子节点的边界框并集
 */

#include <db/index/gist/gist.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* ── 内部宏定义 ── */
#define GIST_DEFAULT_FANOUT 16
#define GIST_MIN_FANOUT 4
#define GIST_MAX_FANOUT 64

/* ── 内部结构 ── */

/* GiST 条目 */
typedef struct gist_entry {
    void *bbox;           /* 边界框 */
    void *value;          /* 值（叶子节点）或子节点指针（非叶子节点） */
    int is_leaf;          /* 是否为叶子节点 */
} gist_entry_t;

/* GiST 节点 */
struct gist_node {
    gist_entry_t *entries;  /* 条目数组 */
    int n_entries;          /* 条目数量 */
    int capacity;           /* 容量 */
    void *bbox;             /* 节点边界框 */
    int is_leaf;            /* 是否为叶子节点 */
    struct gist_node *parent; /* 父节点 */
};

/* GiST 索引结构 */
struct gist_index {
    gist_ops_t ops;        /* 操作符集合 */
    int bbox_size;         /* 边界框大小 */
    float fill_factor;     /* 填充因子 */

    gist_node_t *root;     /* 根节点 */
    int height;            /* 树高度 */
    int n_entries;         /* 总条目数 */
};

/* ── 静态函数声明 ── */
static gist_node_t *_gist_node_create(gist_index_t *idx, int is_leaf);
static void _gist_node_destroy(gist_index_t *idx, gist_node_t *node);
static int _gist_node_insert(gist_index_t *idx, gist_node_t *node,
                              const void *bbox, const void *value, int is_leaf);
/* 暂未使用 _gist_node_search */
// static int _gist_node_search(gist_index_t *idx, gist_node_t *node,
//                               const void *query_bbox, void **results,
//                               int *count, int max_results);
static int _gist_node_knn(gist_index_t *idx, gist_node_t *node,
                           const void *point, int k, void **results,
                           float *distances, int *count);
static int _gist_update_bbox(gist_index_t *idx, gist_node_t *node);
static int _gist_pick_split(gist_index_t *idx, gist_node_t *node,
                             gist_node_t **node1, gist_node_t **node2);
static void _gist_search_recursive(gist_index_t *idx, gist_node_t *node,
                                    const void *query_bbox, void **results,
                                    int *count, int max_results);

static void _default_union(const void **children, int count, void *result);
static int _default_consistent(const void *bbox, const void *query);
static float _default_distance(const void *bbox, const void *point);
static void _default_free(void *ptr);
static void *_default_copy(const void *bbox);

/* ── 核心 API 实现 ── */

gist_index_t *gist_create(const gist_ops_t *ops, int bbox_size)
{
    if (!ops || bbox_size <= 0) return NULL;

    gist_index_t *idx = (gist_index_t *)calloc(1, sizeof(gist_index_t));
    if (!idx) return NULL;

    idx->ops = *ops;
    idx->bbox_size = bbox_size;
    idx->fill_factor = 0.7f;

    /* 设置默认操作符 */
    if (!idx->ops.union_fn) idx->ops.union_fn = _default_union;
    if (!idx->ops.consistent_fn) idx->ops.consistent_fn = _default_consistent;
    if (!idx->ops.distance_fn) idx->ops.distance_fn = _default_distance;
    if (!idx->ops.free_fn) idx->ops.free_fn = _default_free;
    if (!idx->ops.copy_fn) idx->ops.copy_fn = _default_copy;

    /* 创建根节点 */
    idx->root = _gist_node_create(idx, 1);
    if (!idx->root) {
        free(idx);
        return NULL;
    }

    idx->height = 1;
    idx->n_entries = 0;

    return idx;
}

void gist_set_fill_factor(gist_index_t *idx, float fill_factor)
{
    if (!idx || fill_factor <= 0 || fill_factor > 1.0f) return;
    idx->fill_factor = fill_factor;
}

int gist_insert(gist_index_t *idx, const void *bbox, const void *value)
{
    if (!idx || !bbox || !value) return -1;

    if (!idx->root) {
        idx->root = _gist_node_create(idx, 1);
        if (!idx->root) return -1;
    }

    int result = _gist_node_insert(idx, idx->root, bbox, value, 1);
    if (result == 0) {
        idx->n_entries++;
    }

    return result;
}

int gist_range_query(gist_index_t *idx, const void *query_bbox,
                    void **results, int *count, int max_results)
{
    if (!idx || !query_bbox || !results || !count) return -1;

    *count = 0;

    if (!idx->root) return 0;

    _gist_search_recursive(idx, idx->root, query_bbox, results, count, max_results);

    return 0;
}

int gist_knn_query(gist_index_t *idx, const void *point, int k,
                  void **results, float *distances)
{
    if (!idx || !point || !results || !distances) return -1;

    if (!idx->root || !idx->ops.distance_fn) return -1;

    int count = 0;
    _gist_node_knn(idx, idx->root, point, k, results, distances, &count);

    return 0;
}

int gist_delete(gist_index_t *idx, const void *bbox, const void *value)
{
    if (!idx || !bbox) return -1;
    /* 简化实现：删除功能需要更复杂的树重构 */
    (void)value;
    return -1;  /* 暂未实现 */
}

void gist_stats(const gist_index_t *idx, int *out_n_nodes, int *out_height, int *out_n_entries)
{
    if (!idx) {
        if (out_n_nodes) *out_n_nodes = 0;
        if (out_height) *out_height = 0;
        if (out_n_entries) *out_n_entries = 0;
        return;
    }

    if (out_n_nodes) {
        /* 简化计算 */
        *out_n_nodes = idx->n_entries > 0 ? idx->n_entries / GIST_DEFAULT_FANOUT + 1 : 1;
    }
    if (out_height) *out_height = idx->height;
    if (out_n_entries) *out_n_entries = idx->n_entries;
}

void gist_destroy(gist_index_t *idx)
{
    if (!idx) return;

    if (idx->root) {
        _gist_node_destroy(idx, idx->root);
    }

    free(idx);
}

/* ── 内部函数实现 ── */

static gist_node_t *_gist_node_create(gist_index_t *idx, int is_leaf)
{
    gist_node_t *node = (gist_node_t *)calloc(1, sizeof(gist_node_t));
    if (!node) return NULL;

    node->entries = (gist_entry_t *)calloc(GIST_DEFAULT_FANOUT, sizeof(gist_entry_t));
    if (!node->entries) {
        free(node);
        return NULL;
    }

    node->n_entries = 0;
    node->capacity = GIST_DEFAULT_FANOUT;
    node->bbox = calloc(1, idx->bbox_size);
    node->is_leaf = is_leaf;
    node->parent = NULL;

    return node;
}

static void _gist_node_destroy(gist_index_t *idx, gist_node_t *node)
{
    if (!node) return;

    for (int i = 0; i < node->n_entries; i++) {
        if (node->entries[i].bbox && idx->ops.free_fn) {
            idx->ops.free_fn(node->entries[i].bbox);
        }
        if (!node->is_leaf) {
            gist_node_t *child = (gist_node_t *)node->entries[i].value;
            _gist_node_destroy(idx, child);
        }
    }

    free(node->entries);
    free(node->bbox);
    free(node);
}

static int _gist_node_insert(gist_index_t *idx, gist_node_t *node,
                              const void *bbox, const void *value, int is_leaf)
{
    /* 复制边界框 */
    void *bbox_copy = malloc(idx->bbox_size);
    if (!bbox_copy) return -1;
    memcpy(bbox_copy, bbox, idx->bbox_size);

    if (node->n_entries >= node->capacity) {
        /* 节点已满，需要分裂 */
        gist_node_t *node1 = NULL, *node2 = NULL;
        if (_gist_pick_split(idx, node, &node1, &node2) != 0) {
            free(bbox_copy);
            return -1;
        }

        /* 简化：直接添加，忽略分裂 */
        /* 实际实现中应该处理分裂和向上传播 */
        (void)node1;
        (void)node2;
    }

    /* 添加条目 */
    gist_entry_t *entry = &node->entries[node->n_entries];
    entry->bbox = bbox_copy;
    entry->value = (void *)value;
    entry->is_leaf = is_leaf;
    node->n_entries++;

    /* 更新节点边界框 */
    _gist_update_bbox(idx, node);

    return 0;
}

static void _gist_search_recursive(gist_index_t *idx, gist_node_t *node,
                                    const void *query_bbox, void **results,
                                    int *count, int max_results)
{
    if (!node || *count >= max_results) return;

    for (int i = 0; i < node->n_entries; i++) {
        gist_entry_t *entry = &node->entries[i];

        /* 检查是否与查询边界框一致 */
        if (idx->ops.consistent_fn(entry->bbox, query_bbox)) {
            if (node->is_leaf) {
                results[*count] = entry->value;
                (*count)++;
            } else {
                gist_node_t *child = (gist_node_t *)entry->value;
                _gist_search_recursive(idx, child, query_bbox, results, count, max_results);
            }
        }
    }
}

static int _gist_node_knn(gist_index_t *idx, gist_node_t *node,
                           const void *point, int k, void **results,
                           float *distances, int *count)
{
    if (!node || *count >= k) return 0;

    for (int i = 0; i < node->n_entries; i++) {
        gist_entry_t *entry = &node->entries[i];
        float dist = idx->ops.distance_fn(entry->bbox, point);

        if (node->is_leaf) {
            /* 找到更近的结果，插入排序 */
            int pos = *count;
            for (int j = 0; j < *count; j++) {
                if (dist < distances[j]) {
                    pos = j;
                    break;
                }
            }

            if (pos < k) {
                /* 移动元素腾出位置 */
                for (int j = *count; j > pos; j--) {
                    if (j < k) {
                        results[j] = results[j - 1];
                        distances[j] = distances[j - 1];
                    }
                }
                results[pos] = entry->value;
                distances[pos] = dist;
                if (*count < k) (*count)++;
            }
        } else {
            gist_node_t *child = (gist_node_t *)entry->value;
            _gist_node_knn(idx, child, point, k, results, distances, count);
        }
    }

    return 0;
}

static int _gist_update_bbox(gist_index_t *idx, gist_node_t *node)
{
    if (!node || node->n_entries == 0) return -1;

    /* 收集所有子边界框 */
    const void **bboxes = (const void **)malloc(node->n_entries * sizeof(void *));
    if (!bboxes) return -1;

    for (int i = 0; i < node->n_entries; i++) {
        bboxes[i] = node->entries[i].bbox;
    }

    /* 计算并集 */
    idx->ops.union_fn(bboxes, node->n_entries, node->bbox);

    free(bboxes);
    return 0;
}

static int _gist_pick_split(gist_index_t *idx, gist_node_t *node,
                             gist_node_t **out_node1, gist_node_t **out_node2)
{
    /* 简化分裂策略：创建两个新节点 */
    (void)idx;
    (void)node;

    *out_node1 = NULL;
    *out_node2 = NULL;

    return -1;  /* 暂未实现复杂分裂 */
}

/* ── 默认操作符实现 ── */

/* 简单的字节拷贝并集（适用于固定大小结构如矩形） */
static void _default_union(const void **children, int count, void *result)
{
    if (!children || count == 0 || !result) return;

    /* 简化：复制第一个孩子 */
    memcpy(result, children[0], sizeof(void *));
    (void)count;
}

/* 简单的指针比较一致函数 */
static int _default_consistent(const void *bbox, const void *query)
{
    /* 简化：总是返回真 */
    (void)bbox;
    (void)query;
    return 1;
}

/* 简单的欧氏距离（假设 bbox 是 float 数组） */
static float _default_distance(const void *bbox, const void *point)
{
    /* 简化：返回 0 */
    (void)bbox;
    (void)point;
    return 0.0f;
}

static void _default_free(void *ptr)
{
    free(ptr);
}

static void *_default_copy(const void *bbox)
{
    /* 简化实现：假设 bbox 是已知大小 */
    return (void *)bbox;
}
