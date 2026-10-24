/*
 * ball_tree.c
 *
 * Ball-Tree 索引实现
 *
 * Ball-Tree 使用超球来划分空间：
 * - 每个节点由中心点 + 半径定义
 * - 递归地将点集分成两部分
 * - 搜索时利用三角不等式剪枝
 *
 * 构建算法：
 * 1. 选择两个距离最远的点作为初始球心
 * 2. 将所有点分配到最近的球心
 * 3. 计算每个子球的中心和半径
 * 4. 递归处理子球
 */

#include <db/index/vector_index/ball_tree/ball_tree.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define BALL_TREE_LEAF_SIZE 10  /* 叶节点最大点数 */

/* ── 内部结构 ── */

/* Ball-Tree 节点 */
typedef struct ball_node {
    int is_leaf;            /* 是否为叶节点 */
    int n_points;           /* 点数量 */

    /* 球定义 */
    float *center;          /* 中心点 [dims] */
    float radius;           /* 半径 */

    /* 叶节点 */
    int *point_ids;         /* 点 ID 数组 */
    int *vector_indices;    /* 向量索引数组 */

    /* 内部节点 */
    struct ball_node *left;
    struct ball_node *right;
} ball_node_t;

/* Ball-Tree 结构 */
struct ball_tree {
    int dims;               /* 维度 */
    int n_vectors;          /* 向量数量 */
    int n_nodes;            /* 节点数量 */

    ball_node_t *root;      /* 根节点 */

    float *vectors;         /* 向量存储 [n_vectors, dims] */
    int *vector_ids;        /* 向量 ID [n_vectors] */
    int max_vectors;        /* 最大向量数 */
};

/* ── 内部辅助函数 ── */

/**
 * 计算欧氏距离
 */
static float compute_l2_distance(const float *v1, const float *v2, int dims)
{
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float diff = v1[i] - v2[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * 计算两个向量的距离
 */
static float distance_between(const float *v1, const float *v2, int dims)
{
    return compute_l2_distance(v1, v2, dims);
}

/**
 * 找到距离最远的两个点
 */
static void find_farthest_points(const float *vectors, const int *indices, int n, int dims,
                                 int *p1, int *p2)
{
    float max_dist = -1.0f;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dist = distance_between(vectors + indices[i] * dims,
                                         vectors + indices[j] * dims, dims);
            if (dist > max_dist) {
                max_dist = dist;
                *p1 = indices[i];
                *p2 = indices[j];
            }
        }
    }
}

/**
 * 计算球的中心和半径
 */
static void compute_ball(const float *vectors, const int *indices, int n, int dims,
                         float *center, float *radius)
{
    /* 中心 = 均值 */
    memset(center, 0, dims * sizeof(float));
    for (int i = 0; i < n; i++) {
        for (int d = 0; d < dims; d++) {
            center[d] += vectors[indices[i] * dims + d];
        }
    }

    float inv_n = 1.0f / n;
    for (int d = 0; d < dims; d++) {
        center[d] *= inv_n;
    }

    /* 半径 = 到最远点的距离 */
    *radius = 0.0f;
    for (int i = 0; i < n; i++) {
        float dist = distance_between(vectors + indices[i] * dims, center, dims);
        if (dist > *radius) {
            *radius = dist;
        }
    }
}

/**
 * 递归构建 Ball-Tree
 */
static ball_node_t *build_recursive(ball_tree_t *tree, const int *indices, int n)
{
    if (n == 0) return NULL;

    ball_node_t *node = (ball_node_t *)calloc(1, sizeof(ball_node_t));
    if (!node) return NULL;

    node->center = (float *)malloc(tree->dims * sizeof(float));
    if (!node->center) {
        free(node);
        return NULL;
    }

    node->n_points = n;
    tree->n_nodes++;

    /* 计算球 */
    compute_ball(tree->vectors, indices, n, tree->dims, node->center, &node->radius);

    /* 叶节点 */
    if (n <= BALL_TREE_LEAF_SIZE) {
        node->is_leaf = 1;
        node->point_ids = (int *)malloc(n * sizeof(int));
        node->vector_indices = (int *)malloc(n * sizeof(int));

        for (int i = 0; i < n; i++) {
            node->point_ids[i] = tree->vector_ids[indices[i]];
            node->vector_indices[i] = indices[i];
        }

        node->left = NULL;
        node->right = NULL;

        return node;
    }

    /* 内部节点：划分 */
    node->is_leaf = 0;
    node->point_ids = NULL;
    node->vector_indices = NULL;

    /* 找到两个种子点 */
    int seed1, seed2;
    find_farthest_points(tree->vectors, indices, n, tree->dims, &seed1, &seed2);

    /* 分配点到两个子球 */
    int *left_indices = (int *)malloc(n * sizeof(int));
    int *right_indices = (int *)malloc(n * sizeof(int));
    int n_left = 0, n_right = 0;

    for (int i = 0; i < n; i++) {
        float dist1 = distance_between(tree->vectors + indices[i] * tree->dims,
                                      tree->vectors + seed1 * tree->dims, tree->dims);
        float dist2 = distance_between(tree->vectors + indices[i] * tree->dims,
                                      tree->vectors + seed2 * tree->dims, tree->dims);

        if (dist1 <= dist2) {
            left_indices[n_left++] = indices[i];
        } else {
            right_indices[n_right++] = indices[i];
        }
    }

    /* 递归构建 */
    node->left = build_recursive(tree, left_indices, n_left);
    node->right = build_recursive(tree, right_indices, n_right);

    free(left_indices);
    free(right_indices);

    return node;
}

/**
 * 递归搜索
 */
static void search_recursive(const ball_node_t *node, const ball_tree_t *tree,
                            const float *query, int k, int *result_ids,
                            float *result_dists, int *result_count)
{
    if (!node) return;

    /* 计算查询点到球心的距离 */
    float dist_to_center = distance_between(query, node->center, tree->dims);

    /* 剪枝：如果查询点距离球心太远 */
    if (dist_to_center - node->radius > result_dists[0]) {
        return;  /* 不可能在这个子树下找到更近的点 */
    }

    if (node->is_leaf) {
        /* 叶节点：检查所有点 */
        for (int i = 0; i < node->n_points; i++) {
            float dist = distance_between(query,
                                        tree->vectors + node->vector_indices[i] * tree->dims,
                                        tree->dims);

            /* 插入结果 */
            if (dist < result_dists[0]) {
                /* 找到更近的点，插入 */
                int insert_pos = 0;
                for (int j = *result_count - 1; j >= 0; j--) {
                    if (dist > result_dists[j]) {
                        insert_pos = j + 1;
                        break;
                    }
                    if (j == 0) insert_pos = 0;
                }

                /* 移动后面的元素 */
                for (int j = *result_count - 1; j > insert_pos; j--) {
                    result_ids[j] = result_ids[j - 1];
                    result_dists[j] = result_dists[j - 1];
                }

                result_ids[insert_pos] = node->point_ids[i];
                result_dists[insert_pos] = dist;
            }
        }
    } else {
        /* 内部节点：先搜索更近的子节点 */
        float dist_left = node->left ?
            distance_between(query, node->left->center, tree->dims) : FLT_MAX;
        float dist_right = node->right ?
            distance_between(query, node->right->center, tree->dims) : FLT_MAX;

        /* 先搜索更近的 */
        if (dist_left < dist_right) {
            search_recursive(node->left, tree, query, k, result_ids, result_dists, result_count);
            search_recursive(node->right, tree, query, k, result_ids, result_dists, result_count);
        } else {
            search_recursive(node->right, tree, query, k, result_ids, result_dists, result_count);
            search_recursive(node->left, tree, query, k, result_ids, result_dists, result_count);
        }
    }
}

/* ── 公共 API 实现 ── */

ball_tree_t *ball_tree_create(int dims)
{
    if (dims <= 0) return NULL;

    ball_tree_t *tree = (ball_tree_t *)calloc(1, sizeof(ball_tree_t));
    if (!tree) return NULL;

    tree->dims = dims;
    tree->n_vectors = 0;
    tree->n_nodes = 0;
    tree->max_vectors = 10000;

    tree->vectors = (float *)malloc(tree->max_vectors * dims * sizeof(float));
    tree->vector_ids = (int *)malloc(tree->max_vectors * sizeof(int));

    if (!tree->vectors || !tree->vector_ids) {
        ball_tree_destroy(tree);
        return NULL;
    }

    tree->root = NULL;

    return tree;
}

int ball_tree_build(ball_tree_t *tree, int n, const float *vectors, const int *ids)
{
    if (!tree || n <= 0 || !vectors || !ids) return -1;

    /* 扩容 */
    if (n > tree->max_vectors) {
        float *new_vectors = (float *)realloc(tree->vectors, n * tree->dims * sizeof(float));
        int *new_ids = (int *)realloc(tree->vector_ids, n * sizeof(int));
        if (!new_vectors || !new_ids) return -1;

        tree->vectors = new_vectors;
        tree->vector_ids = new_ids;
        tree->max_vectors = n;
    }

    /* 复制向量 */
    memcpy(tree->vectors, vectors, n * tree->dims * sizeof(float));
    memcpy(tree->vector_ids, ids, n * sizeof(int));
    tree->n_vectors = n;

    /* 构建索引 */
    int *indices = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) indices[i] = i;

    tree->root = build_recursive(tree, indices, n);

    free(indices);

    return (tree->root != NULL) ? 0 : -1;
}

int ball_tree_search(const ball_tree_t *tree, const float *query,
                    int k, int *ids, float *distances)
{
    if (!tree || !tree->root || !query || k <= 0 || !ids) return -1;

    /* 初始化结果 */
    for (int i = 0; i < k; i++) {
        ids[i] = -1;
        distances[i] = FLT_MAX;
    }

    int result_count = 0;

    /* 搜索 */
    search_recursive(tree->root, tree, query, k, ids, distances, &result_count);

    /* 填充剩余位置 */
    for (int i = 0; i < k; i++) {
        if (ids[i] == -1) {
            ids[i] = 0;
            distances[i] = 0.0f;
        }
    }

    return result_count;
}

int ball_tree_range_search(const ball_tree_t *tree, const float *query,
                          float radius, int *result_ids, int max_results)
{
    if (!tree || !tree->root || !query || !result_ids || max_results <= 0) return -1;

    /* 简化实现：使用 kNN 搜索 */
    int *temp_ids = (int *)malloc(tree->n_vectors * sizeof(int));
    float *temp_dists = (float *)malloc(tree->n_vectors * sizeof(float));

    ball_tree_search(tree, query, tree->n_vectors, temp_ids, temp_dists);

    int count = 0;
    for (int i = 0; i < tree->n_vectors && count < max_results; i++) {
        if (temp_dists[i] <= radius) {
            result_ids[count++] = temp_ids[i];
        }
    }

    free(temp_ids);
    free(temp_dists);

    return count;
}

void ball_tree_stats(const ball_tree_t *tree, int *out_n_vectors, int *out_n_nodes)
{
    if (!tree) return;

    if (out_n_vectors) *out_n_vectors = tree->n_vectors;
    if (out_n_nodes) *out_n_nodes = tree->n_nodes;
}

int ball_tree_save(const ball_tree_t *tree, const char *path)
{
    if (!tree || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头部 */
    fwrite(&tree->dims, sizeof(int), 1, fp);
    fwrite(&tree->n_vectors, sizeof(int), 1, fp);

    /* 写入向量 */
    fwrite(tree->vectors, sizeof(float), tree->n_vectors * tree->dims, fp);
    fwrite(tree->vector_ids, sizeof(int), tree->n_vectors, fp);

    fclose(fp);
    return 0;
}

ball_tree_t *ball_tree_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int dims, n_vectors;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建树 */
    ball_tree_t *tree = ball_tree_create(dims);
    if (!tree) {
        fclose(fp);
        return NULL;
    }

    /* 读取向量 */
    tree->n_vectors = n_vectors;
    fread(tree->vectors, sizeof(float), n_vectors * dims, fp);
    fread(tree->vector_ids, sizeof(int), n_vectors, fp);

    /* 重建索引 */
    int *indices = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) indices[i] = i;
    tree->root = build_recursive(tree, indices, n_vectors);
    free(indices);

    fclose(fp);
    return tree;
}

/* 释放节点 */
static void free_node(ball_node_t *node)
{
    if (!node) return;

    if (node->is_leaf) {
        free(node->point_ids);
        free(node->vector_indices);
    } else {
        free_node(node->left);
        free_node(node->right);
    }

    free(node->center);
    free(node);
}

void ball_tree_destroy(ball_tree_t *tree)
{
    if (!tree) return;

    free_node(tree->root);
    free(tree->vectors);
    free(tree->vector_ids);
    free(tree);
}