/*
 * kd_tree.c — KD-Tree 索引实现
 *
 * === 实现要点 ===
 * 1. 空间划分：选择方差最大的维度切分
 * 2. 中位数切分：使用快速选择算法
 * 3. kNN 搜索：优先队列 + 剪枝策略
 *
 * === 数据结构 ===
 * - 节点：存储切分维度、切分值、左右子树
 * - 叶节点：存储向量 ID
 *
 * === 参考实现 ===
 * Bentley 1975, "Multidimensional Binary Search Trees"
 */

#include <db/index/vector_index/kd_tree/kd_tree.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/* KD-Tree 节点 */
typedef struct kd_node {
    int split_dim;          /* 切分维度（-1 表示叶节点） */
    float split_val;        /* 切分值 */
    int id;                 /* 向量 ID（叶节点有效） */
    struct kd_node *left;   /* 左子树 */
    struct kd_node *right;  /* 右子树 */
} kd_node_t;

/* KD-Tree 索引结构 */
struct kd_tree {
    int dims;               /* 向量维度 */
    int n_items;            /* 向量数量 */
    int n_nodes;            /* 节点数量 */

    float *vectors;         /* 向量数据 [n_items * dims] */
    kd_node_t *root;        /* 树根 */
};

/* 优先队列项 */
typedef struct pq_item {
    float dist;
    int id;
} pq_item_t;

/* 堆下沉 */
static void heap_down(pq_item_t *heap, int size, int i)
{
    while (1) {
        int largest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < size && heap[left].dist > heap[largest].dist) {
            largest = left;
        }
        if (right < size && heap[right].dist > heap[largest].dist) {
            largest = right;
        }

        if (largest == i) break;

        pq_item_t tmp = heap[i];
        heap[i] = heap[largest];
        heap[largest] = tmp;

        i = largest;
    }
}

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/* 计算欧氏距离平方 */
static float euclidean_distance_sqr(const float *a, const float *b, int dims)
{
    float sum = 0;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sum;
}

/* 创建叶节点 */
static kd_node_t *create_leaf_node(int id)
{
    kd_node_t *node = (kd_node_t *)calloc(1, sizeof(kd_node_t));
    if (!node) return NULL;

    node->split_dim = -1;
    node->id = id;
    node->left = NULL;
    node->right = NULL;

    return node;
}

/* 创建内部节点 */
static kd_node_t *create_inner_node(int split_dim, float split_val)
{
    kd_node_t *node = (kd_node_t *)calloc(1, sizeof(kd_node_t));
    if (!node) return NULL;

    node->split_dim = split_dim;
    node->split_val = split_val;
    node->id = -1;
    node->left = NULL;
    node->right = NULL;

    return node;
}

/* 销毁节点 */
static void destroy_node(kd_node_t *node)
{
    if (!node) return;

    destroy_node(node->left);
    destroy_node(node->right);
    free(node);
}

/* 快速选择：找到第 k 小的元素 */
static void quickselect(float *arr, int *ids, int n, int k)
{
    int left = 0, right = n - 1;

    while (left < right) {
        float pivot = arr[k];
        arr[k] = arr[right];
        arr[right] = pivot;

        int id_tmp = ids[k];
        ids[k] = ids[right];
        ids[right] = id_tmp;

        int store = left;
        for (int i = left; i < right; i++) {
            if (arr[i] < pivot) {
                float tmp = arr[store];
                arr[store] = arr[i];
                arr[i] = tmp;

                int id_tmp2 = ids[store];
                ids[store] = ids[i];
                ids[i] = id_tmp2;

                store++;
            }
        }

        float tmp = arr[store];
        arr[store] = arr[right];
        arr[right] = tmp;

        int id_tmp3 = ids[store];
        ids[store] = ids[right];
        ids[right] = id_tmp3;

        if (store == k) break;
        if (store < k) {
            left = store + 1;
        } else {
            right = store - 1;
        }
    }
}

/* 计算方差 */
static float compute_variance(kd_tree_t *idx, int *ids, int n, int dim)
{
    float sum = 0, sum_sq = 0;

    for (int i = 0; i < n; i++) {
        float val = idx->vectors[ids[i] * idx->dims + dim];
        sum += val;
        sum_sq += val * val;
    }

    float mean = sum / n;
    return sum_sq / n - mean * mean;
}

/* 递归构建 KD-Tree */
static kd_node_t *build_tree_recursive(kd_tree_t *idx, int *ids, int n, int depth)
{
    /* 叶节点条件 */
    if (n <= 1) {
        return create_leaf_node(ids[0]);
    }

    /* 选择切分维度：选择方差最大的维度 */
    int split_dim = depth % idx->dims;
    float max_var = -1;

    for (int d = 0; d < idx->dims; d++) {
        float var = compute_variance(idx, ids, n, d);
        if (var > max_var) {
            max_var = var;
            split_dim = d;
        }
    }

    /* 提取该维度的值 */
    float *values = (float *)malloc(n * sizeof(float));
    if (!values) return create_leaf_node(ids[0]);

    for (int i = 0; i < n; i++) {
        values[i] = idx->vectors[ids[i] * idx->dims + split_dim];
    }

    /* 找中位数 */
    int median = n / 2;
    quickselect(values, ids, n, median);

    float split_val = values[median];
    free(values);

    /* 创建内部节点 */
    kd_node_t *node = create_inner_node(split_dim, split_val);
    if (!node) return NULL;

    /* 递归构建子树 */
    node->left = build_tree_recursive(idx, ids, median, depth + 1);
    node->right = build_tree_recursive(idx, ids + median + 1, n - median - 1, depth + 1);

    if (!node->left || !node->right) {
        destroy_node(node);
        return NULL;
    }

    return node;
}

/* kNN 搜索辅助函数 */
static void knn_search_recursive(const kd_tree_t *idx, const kd_node_t *node,
                                  const float *query, pq_item_t *heap, int *heap_size, int k)
{
    if (!node) return;

    if (node->split_dim == -1) {
        /* 叶节点 */
        const float *v = idx->vectors + node->id * idx->dims;
        float dist = sqrtf(euclidean_distance_sqr(query, v, idx->dims));

        if (*heap_size < k) {
            heap[*heap_size].dist = dist;
            heap[*heap_size].id = node->id;
            (*heap_size)++;

            int j = *heap_size - 1;
            while (j > 0 && heap[(j - 1) / 2].dist < heap[j].dist) {
                pq_item_t tmp = heap[j];
                heap[j] = heap[(j - 1) / 2];
                heap[(j - 1) / 2] = tmp;
                j = (j - 1) / 2;
            }
        } else if (dist < heap[0].dist) {
            heap[0].dist = dist;
            heap[0].id = node->id;
            heap_down(heap, *heap_size, 0);
        }

        return;
    }

    /* 内部节点 */
    int split_dim = node->split_dim;
    float split_val = node->split_val;
    float diff = query[split_dim] - split_val;

    kd_node_t *near, *far;
    if (diff < 0) {
        near = node->left;
        far = node->right;
    } else {
        near = node->right;
        far = node->left;
    }

    /* 先搜索近侧 */
    knn_search_recursive(idx, near, query, heap, heap_size, k);

    /* 检查是否需要搜索远侧 */
    if (*heap_size < k || fabsf(diff) < heap[0].dist) {
        knn_search_recursive(idx, far, query, heap, heap_size, k);
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

kd_tree_t *kd_tree_create(int dims)
{
    if (dims <= 0) return NULL;

    kd_tree_t *idx = (kd_tree_t *)calloc(1, sizeof(kd_tree_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->n_items = 0;
    idx->n_nodes = 0;
    idx->vectors = NULL;
    idx->root = NULL;

    return idx;
}

void kd_tree_destroy(kd_tree_t *idx)
{
    if (!idx) return;

    destroy_node(idx->root);
    free(idx->vectors);
    free(idx);
}

int kd_tree_insert(kd_tree_t *idx, int id, const float *vector)
{
    if (!idx || !vector) return -1;

    /* 扩容 */
    int new_capacity = (idx->n_items == 0) ? 1024 : idx->n_items * 2;
    float *new_vectors = (float *)realloc(idx->vectors, new_capacity * idx->dims * sizeof(float));
    if (!new_vectors) return -1;
    idx->vectors = new_vectors;

    /* 存储向量 */
    memcpy(idx->vectors + id * idx->dims, vector, idx->dims * sizeof(float));
    idx->n_items++;

    return 0;
}

int kd_tree_build(kd_tree_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !vectors || !ids || n <= 0) return -1;

    /* 分配向量存储 */
    idx->vectors = (float *)malloc(n * idx->dims * sizeof(float));
    if (!idx->vectors) return -1;

    memcpy(idx->vectors, vectors, n * idx->dims * sizeof(float));
    idx->n_items = n;

    /* 准备 ID 列表 */
    int *id_list = (int *)malloc(n * sizeof(int));
    if (!id_list) return -1;

    memcpy(id_list, ids, n * sizeof(int));

    /* 递归构建 */
    idx->root = build_tree_recursive(idx, id_list, n, 0);

    free(id_list);

    if (!idx->root) return -1;

    idx->n_nodes = n;

    return 0;
}

int kd_tree_search(const kd_tree_t *idx, const float *query, int k,
                   int *result_ids, float *result_dists)
{
    if (!idx || !query || k <= 0 || !result_ids) return -1;
    if (!idx->root) return -1;

    pq_item_t *heap = (pq_item_t *)malloc((k + 1) * sizeof(pq_item_t));
    if (!heap) return -1;

    int heap_size = 0;

    knn_search_recursive(idx, idx->root, query, heap, &heap_size, k);

    /* 堆排序（从小到大，使最近的在前面） */
    for (int i = heap_size - 1; i > 0; i--) {
        pq_item_t tmp = heap[0];
        heap[0] = heap[i];
        heap[i] = tmp;
        heap_down(heap, i, 0);
    }
    for (int i = 0; i < heap_size; i++) {
        result_ids[i] = heap[i].id;
        if (result_dists) {
            result_dists[i] = heap[i].dist;
        }
    }

    free(heap);
    return heap_size;
}

int kd_tree_range_search(const kd_tree_t *idx, const float *query, float radius,
                         int *result_ids, int max_results)
{
    if (!idx || !query || radius <= 0 || !result_ids) return -1;
    if (!idx->root) return -1;

    /* 简化实现：使用 kNN 搜索并过滤 */
    int count = 0;

    /* 遍历所有向量（简化实现） */
    for (int i = 0; i < idx->n_items && count < max_results; i++) {
        const float *v = idx->vectors + i * idx->dims;
        float dist = sqrtf(euclidean_distance_sqr(query, v, idx->dims));

        if (dist <= radius) {
            result_ids[count++] = i;
        }
    }

    return count;
}

int kd_tree_save(const kd_tree_t *idx, const char *path)
{
    if (!idx || !path) return -1;
    if (!idx->root) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写头部 */
    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->n_items, sizeof(int), 1, fp);
    fwrite(&idx->n_nodes, sizeof(int), 1, fp);

    /* 写向量数据 */
    fwrite(idx->vectors, sizeof(float), idx->n_items * idx->dims, fp);

    fclose(fp);
    return 0;
}

kd_tree_t *kd_tree_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读头部 */
    int dims, n_items, n_nodes;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_items, sizeof(int), 1, fp) != 1 ||
        fread(&n_nodes, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    kd_tree_t *idx = kd_tree_create(dims);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读向量数据 */
    idx->vectors = (float *)malloc(n_items * dims * sizeof(float));
    if (!idx->vectors) {
        kd_tree_destroy(idx);
        fclose(fp);
        return NULL;
    }

    if (fread(idx->vectors, sizeof(float), n_items * dims, fp) != (size_t)(n_items * dims)) {
        kd_tree_destroy(idx);
        fclose(fp);
        return NULL;
    }

    idx->n_items = n_items;
    idx->n_nodes = n_nodes;

    /* 重建树 */
    int *ids = (int *)malloc(n_items * sizeof(int));
    if (!ids) {
        kd_tree_destroy(idx);
        fclose(fp);
        return NULL;
    }

    for (int i = 0; i < n_items; i++) {
        ids[i] = i;
    }

    idx->root = build_tree_recursive(idx, ids, n_items, 0);

    free(ids);
    fclose(fp);

    return idx;
}

int kd_tree_get_n_items(const kd_tree_t *idx)
{
    return idx ? idx->n_items : 0;
}

int kd_tree_get_n_nodes(const kd_tree_t *idx)
{
    return idx ? idx->n_nodes : 0;
}