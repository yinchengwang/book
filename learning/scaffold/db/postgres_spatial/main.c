/*
 * main.c — R-Tree 空间索引演示
 *
 * 演示内容：
 *   1. MBR（最小包围矩形）结构
 *   2. R-Tree 节点：孩子 MBR 数组 + 孩子指针数组
 *   3. 插入：选择最小面积增长子节点，叶节点满则分裂
 *   4. 搜索：递归搜索与查询矩形相交的子树
 *   5. 范围查询演示 R-Tree 剪枝
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== MBR（最小包围矩形） ========== */
typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} mbr_t;

/* 计算矩形面积 */
static float mbr_area(const mbr_t *r) {
    return (r->max_x - r->min_x) * (r->max_y - r->min_y);
}

/* 两个矩形的并集 */
static mbr_t mbr_union(const mbr_t *a, const mbr_t *b) {
    mbr_t r;
    r.min_x = a->min_x < b->min_x ? a->min_x : b->min_x;
    r.min_y = a->min_y < b->min_y ? a->min_y : b->min_y;
    r.max_x = a->max_x > b->max_x ? a->max_x : b->max_x;
    r.max_y = a->max_y > b->max_y ? a->max_y : b->max_y;
    return r;
}

/* 判断两个矩形是否相交 */
static bool mbr_intersects(const mbr_t *a, const mbr_t *b) {
    return !(a->max_x < b->min_x || a->min_x > b->max_x ||
             a->max_y < b->min_y || a->min_y > b->max_y);
}

/* ========== R-Tree 节点 ========== */
#define MAX_ENTRIES 4  /* 每个节点最大条目数（教学用小值） */

typedef struct rtree_node {
    bool is_leaf;              /* 是否为叶节点 */
    int nentries;              /* 当前条目数 */
    mbr_t mbr;                 /* 本节点的 MBR */
    mbr_t child_mbrs[MAX_ENTRIES + 1];  /* 孩子 MBR 数组 */
    union {
        void *data[MAX_ENTRIES + 1];            /* 叶节点：数据指针 */
        struct rtree_node *children[MAX_ENTRIES + 1]; /* 内部节点：孩子指针 */
    } u;
} rtree_node_t;

/* R-Tree 结构 */
typedef struct {
    rtree_node_t *root;
    int size;
} rtree_t;

/* 创建节点 */
static rtree_node_t *node_create(bool is_leaf) {
    rtree_node_t *n = (rtree_node_t *)calloc(1, sizeof(rtree_node_t));
    n->is_leaf = is_leaf;
    return n;
}

/* ========== 节点分裂 ========== */
/* 选择分裂轴：按 x 轴或 y 轴排序后，枚举所有分裂点，选总面积最小的 */
static void node_split(rtree_node_t *node, rtree_node_t **new_node_out) {
    rtree_node_t *n2 = node_create(node->is_leaf);
    int total = node->nentries;
    int half = total / 2;

    /* 简化分裂：按 x 中心排序，前半留原节点，后半给新节点 */
    /* 构建索引数组并按 x 中心排序 */
    int idx[MAX_ENTRIES + 1];
    for (int i = 0; i < total; i++) idx[i] = i;
    for (int i = 0; i < total - 1; i++) {
        for (int j = i + 1; j < total; j++) {
            float ci = (node->child_mbrs[idx[i]].min_x + node->child_mbrs[idx[i]].max_x) / 2;
            float cj = (node->child_mbrs[idx[j]].min_x + node->child_mbrs[idx[j]].max_x) / 2;
            if (ci > cj) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
        }
    }

    /* 临时保存原条目 */
    mbr_t tmp_mbrs[MAX_ENTRIES + 1];
    void *tmp_data[MAX_ENTRIES + 1];
    rtree_node_t *tmp_children[MAX_ENTRIES + 1];
    memcpy(tmp_mbrs, node->child_mbrs, sizeof(tmp_mbrs));
    memcpy(tmp_data, node->u.data, sizeof(tmp_data));
    memcpy(tmp_children, node->u.children, sizeof(tmp_children));

    /* 重新分配到原节点 */
    node->nentries = 0;
    memset(&node->mbr, 0, sizeof(mbr_t));
    for (int i = 0; i < half; i++) {
        int k = idx[i];
        node->child_mbrs[node->nentries] = tmp_mbrs[k];
        if (node->is_leaf) node->u.data[node->nentries] = tmp_data[k];
        else node->u.children[node->nentries] = tmp_children[k];
        node->mbr = (i == 0) ? tmp_mbrs[k] : mbr_union(&node->mbr, &tmp_mbrs[k]);
        node->nentries++;
    }

    /* 分配到新节点 */
    n2->nentries = 0;
    memset(&n2->mbr, 0, sizeof(mbr_t));
    for (int i = half; i < total; i++) {
        int k = idx[i];
        n2->child_mbrs[n2->nentries] = tmp_mbrs[k];
        if (n2->is_leaf) n2->u.data[n2->nentries] = tmp_data[k];
        else n2->u.children[n2->nentries] = tmp_children[k];
        n2->mbr = (i == half) ? tmp_mbrs[k] : mbr_union(&n2->mbr, &tmp_mbrs[k]);
        n2->nentries++;
    }

    *new_node_out = n2;
}

/* ========== 插入 ========== */
static void rtree_insert_internal(rtree_t *tree, rtree_node_t *node,
                                  const mbr_t *rect, const void *data,
                                  rtree_node_t **split_out) {
    *split_out = NULL;

    if (node->is_leaf) {
        if (node->nentries < MAX_ENTRIES) {
            /* 叶节点未满，直接插入 */
            node->child_mbrs[node->nentries] = *rect;
            node->u.data[node->nentries] = (void *)data;
            node->nentries++;
            node->mbr = mbr_union(&node->mbr, rect);
            return;
        }
        /* 叶节点已满，添加后分裂 */
        node->child_mbrs[node->nentries] = *rect;
        node->u.data[node->nentries] = (void *)data;
        node->nentries++;
        node->mbr = mbr_union(&node->mbr, rect);
        node_split(node, split_out);
        return;
    }

    /* 内部节点：选择面积增长最小的子节点 */
    int best = 0;
    float best_inc = 1e30f;
    for (int i = 0; i < node->nentries; i++) {
        mbr_t u = mbr_union(&node->child_mbrs[i], rect);
        float inc = mbr_area(&u) - mbr_area(&node->child_mbrs[i]);
        if (inc < best_inc) { best_inc = inc; best = i; }
    }

    /* 递归插入 */
    rtree_node_t *new_child = NULL;
    rtree_insert_internal(tree, node->u.children[best], rect, data, &new_child);

    /* 更新本节点 MBR */
    node->child_mbrs[best] = node->u.children[best]->mbr;
    node->mbr = mbr_union(&node->mbr, &node->child_mbrs[best]);

    if (new_child) {
        /* 子节点分裂，需要将新节点加入本节点 */
        if (node->nentries < MAX_ENTRIES) {
            node->child_mbrs[node->nentries] = new_child->mbr;
            node->u.children[node->nentries] = new_child;
            node->nentries++;
            node->mbr = mbr_union(&node->mbr, &new_child->mbr);
        } else {
            /* 本节点也满了，先加入再分裂 */
            node->child_mbrs[node->nentries] = new_child->mbr;
            node->u.children[node->nentries] = new_child;
            node->nentries++;
            node_split(node, split_out);
        }
    }
}

/* 对外插入接口 */
static void rtree_insert(rtree_t *tree, const mbr_t *rect, const void *data) {
    rtree_node_t *new_root_sibling = NULL;
    rtree_insert_internal(tree, tree->root, rect, data, &new_root_sibling);

    if (new_root_sibling) {
        /* 根节点分裂，创建新根 */
        rtree_node_t *new_root = node_create(false);
        new_root->child_mbrs[0] = tree->root->mbr;
        new_root->u.children[0] = tree->root;
        new_root->child_mbrs[1] = new_root_sibling->mbr;
        new_root->u.children[1] = new_root_sibling;
        new_root->nentries = 2;
        new_root->mbr = mbr_union(&tree->root->mbr, &new_root_sibling->mbr);
        tree->root = new_root;
    }
    tree->size++;
}

/* ========== 搜索 ========== */
static int rtree_search(const rtree_node_t *node, const mbr_t *query,
                        mbr_t *results, int *found, int max_results) {
    if (!node || *found >= max_results) return 0;
    /* MBR 不相交则剪枝 */
    if (!mbr_intersects(&node->mbr, query)) return 0;

    int count = 0;
    if (node->is_leaf) {
        for (int i = 0; i < node->nentries; i++) {
            if (mbr_intersects(&node->child_mbrs[i], query)) {
                if (*found < max_results) results[(*found)++] = node->child_mbrs[i];
                count++;
            }
        }
    } else {
        for (int i = 0; i < node->nentries; i++) {
            if (mbr_intersects(&node->child_mbrs[i], query)) {
                count += rtree_search(node->u.children[i], query, results, found, max_results);
            }
        }
    }
    return count;
}

/* ========== 主函数 ========== */
int main(void) {
    rtree_t tree = { .root = node_create(true), .size = 0 };

    /* 插入 8 个矩形（模拟空间对象） */
    mbr_t rects[] = {
        {0, 0, 2, 2},    /* A: 左下角小方块 */
        {3, 3, 5, 5},    /* B: 右上角小方块 */
        {1, 1, 4, 4},    /* C: 中心区域 */
        {6, 0, 8, 2},    /* D: 右下角 */
        {0, 6, 2, 8},    /* E: 左上角 */
        {5, 5, 9, 9},    /* F: 右上大区域 */
        {7, 3, 9, 5},    /* G: 右侧中部 */
        {2, 7, 5, 9},    /* H: 上方中部 */
    };
    const char *names[] = {"A", "B", "C", "D", "E", "F", "G", "H"};

    printf("=== R-Tree 空间索引演示 ===\n\n");
    printf("插入 %d 个矩形：\n", 8);
    for (int i = 0; i < 8; i++) {
        rtree_insert(&tree, &rects[i], names[i]);
        printf("  [%s] (%.0f,%.0f)-(%.0f,%.0f)\n", names[i],
               rects[i].min_x, rects[i].min_y, rects[i].max_x, rects[i].max_y);
    }
    printf("树中共 %d 个条目\n\n", tree.size);

    /* 范围查询：查找与 (2,2)-(6,6) 相交的矩形 */
    mbr_t query = {2, 2, 6, 6};
    mbr_t results[16];
    int found = 0;
    int count = rtree_search(tree.root, &query, results, &found, 16);

    printf("范围查询 (%.0f,%.0f)-(%.0f,%.0f)：\n",
           query.min_x, query.min_y, query.max_x, query.max_y);
    printf("  命中 %d 个矩形：\n", count);
    for (int i = 0; i < found; i++) {
        printf("    (%.0f,%.0f)-(%.0f,%.0f)\n",
               results[i].min_x, results[i].min_y,
               results[i].max_x, results[i].max_y);
    }

    /* 演示剪枝：查询不相交区域 */
    mbr_t miss_query = {10, 10, 12, 12};
    found = 0;
    int miss_count = rtree_search(tree.root, &miss_query, results, &found, 16);
    printf("\n不相交查询 (%.0f,%.0f)-(%.0f,%.0f)：命中 %d 个（剪枝生效）\n",
           miss_query.min_x, miss_query.min_y,
           miss_query.max_x, miss_query.max_y, miss_count);

    return 0;
}
