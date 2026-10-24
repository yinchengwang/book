/*
 * annoy.c — Annoy 索引实现
 *
 * === 实现要点 ===
 * 1. 随机投影树：随机选择超平面递归划分空间
 * 2. 多树搜索：构建多棵树提高召回率
 * 3. 优先队列：使用大顶堆维护 top-k
 *
 * === 数据结构 ===
 * - 树节点：存储分割超平面（法向量 + 阈值）
 * - 叶节点：存储向量 ID 列表
 * - 多棵树：提高召回率
 *
 * === 参考实现 ===
 * Spotify Annoy: https://github.com/spotify/annoy
 */

#include <db/index/vector_index/annoy/annoy.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/* 树节点类型 */
typedef enum node_type {
    NODE_INNER = 0,  /* 内部节点（分割节点） */
    NODE_LEAF = 1    /* 叶节点 */
} node_type_t;

/* 树节点 */
typedef struct annoy_node {
    node_type_t type;
    union {
        struct {
            float *hyperplane;  /* 分割超平面法向量 [dims] */
            float threshold;    /* 分割阈值 */
            struct annoy_node *left;   /* 左子树 */
            struct annoy_node *right;  /* 右子树 */
        } inner;
        struct {
            int *ids;           /* 向量 ID 数组 */
            int n_ids;          /* ID 数量 */
        } leaf;
    } u;
} annoy_node_t;

/* 一棵树 */
typedef struct annoy_tree {
    annoy_node_t *root;  /* 树根 */
    int n_nodes;         /* 节点数 */
} annoy_tree_t;

/* Annoy 索引结构 */
struct annoy_index {
    int dims;                /* 向量维度 */
    int n_trees;             /* 树的数量 */
    int n_items;             /* 向量数量 */
    bool built;              /* 是否已构建 */

    float *vectors;          /* 向量数据 [n_items * dims] */
    int vectors_capacity;    /* 向量容量 */

    annoy_tree_t *trees;     /* 树数组 [n_trees] */

    int metric;              /* 距离度量：0=angular, 1=euclidean */
};

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/* 生成随机浮点数 [-1, 1] */
static float random_uniform(void)
{
    return 2.0f * ((float)rand() / RAND_MAX) - 1.0f;
}

/* 生成随机高斯分布 */
static float random_gaussian(void)
{
    /* Box-Muller 变换 */
    static bool has_spare = false;
    static float spare;

    if (has_spare) {
        has_spare = false;
        return spare;
    }

    float u, v, s;
    do {
        u = random_uniform();
        v = random_uniform();
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    has_spare = true;

    return u * s;
}

/* 计算点积 */
static float dot_product(const float *a, const float *b, int dims)
{
    float sum = 0;
    for (int i = 0; i < dims; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

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

/* 计算角度距离 */
static float angular_distance(const float *a, const float *b, int dims)
{
    float dot = dot_product(a, b, dims);
    float norm_a = sqrtf(dot_product(a, a, dims));
    float norm_b = sqrtf(dot_product(b, b, dims));

    if (norm_a == 0 || norm_b == 0) return 0;

    float cos_theta = dot / (norm_a * norm_b);
    /* 限制在 [-1, 1] 范围内 */
    if (cos_theta > 1.0f) cos_theta = 1.0f;
    if (cos_theta < -1.0f) cos_theta = -1.0f;

    return acosf(cos_theta);
}

/* 计算距离 */
static float compute_distance(const annoy_index_t *idx, const float *a, const float *b)
{
    if (idx->metric == 0) {
        return angular_distance(a, b, idx->dims);
    } else {
        return sqrtf(euclidean_distance_sqr(a, b, idx->dims));
    }
}

/* 创建内部节点 */
static annoy_node_t *create_inner_node(float *hyperplane, float threshold)
{
    annoy_node_t *node = (annoy_node_t *)calloc(1, sizeof(annoy_node_t));
    if (!node) return NULL;

    node->type = NODE_INNER;
    node->u.inner.hyperplane = hyperplane;
    node->u.inner.threshold = threshold;
    node->u.inner.left = NULL;
    node->u.inner.right = NULL;

    return node;
}

/* 创建叶节点 */
static annoy_node_t *create_leaf_node(int *ids, int n_ids)
{
    annoy_node_t *node = (annoy_node_t *)calloc(1, sizeof(annoy_node_t));
    if (!node) return NULL;

    node->type = NODE_LEAF;
    node->u.leaf.ids = ids;
    node->u.leaf.n_ids = n_ids;

    return node;
}

/* 销毁节点 */
static void destroy_node(annoy_node_t *node)
{
    if (!node) return;

    if (node->type == NODE_INNER) {
        free(node->u.inner.hyperplane);
        destroy_node(node->u.inner.left);
        destroy_node(node->u.inner.right);
    } else {
        free(node->u.leaf.ids);
    }

    free(node);
}

/* 递归构建树 */
static annoy_node_t *build_tree_recursive(annoy_index_t *idx, int *ids, int n_ids, int depth)
{
    /* 叶节点条件：点数太少或深度太大 */
    if (n_ids <= 10 || depth > 20) {
        int *ids_copy = (int *)malloc(n_ids * sizeof(int));
        if (!ids_copy) return NULL;
        memcpy(ids_copy, ids, n_ids * sizeof(int));
        return create_leaf_node(ids_copy, n_ids);
    }

    /* 随机选择两个点 */
    int i1 = rand() % n_ids;
    int i2 = rand() % n_ids;
    while (i2 == i1 && n_ids > 1) {
        i2 = rand() % n_ids;
    }

    const float *v1 = idx->vectors + ids[i1] * idx->dims;
    const float *v2 = idx->vectors + ids[i2] * idx->dims;

    /* 构造分割超平面：垂直于 v1-v2，通过中点 */
    float *hyperplane = (float *)malloc(idx->dims * sizeof(float));
    if (!hyperplane) return NULL;

    /* 超平面法向量 = v2 - v1 */
    for (int i = 0; i < idx->dims; i++) {
        hyperplane[i] = v2[i] - v1[i];
    }

    /* 归一化 */
    float norm = sqrtf(dot_product(hyperplane, hyperplane, idx->dims));
    if (norm > 0) {
        for (int i = 0; i < idx->dims; i++) {
            hyperplane[i] /= norm;
        }
    }

    /* 阈值 = 中点在超平面上的投影 */
    float threshold = 0;
    for (int i = 0; i < idx->dims; i++) {
        threshold += hyperplane[i] * ((v1[i] + v2[i]) / 2.0f);
    }

    /* 分割点 */
    int *left_ids = (int *)malloc(n_ids * sizeof(int));
    int *right_ids = (int *)malloc(n_ids * sizeof(int));
    if (!left_ids || !right_ids) {
        free(hyperplane);
        free(left_ids);
        free(right_ids);
        return NULL;
    }

    int n_left = 0, n_right = 0;
    for (int i = 0; i < n_ids; i++) {
        const float *v = idx->vectors + ids[i] * idx->dims;
        float proj = dot_product(v, hyperplane, idx->dims);

        if (proj < threshold) {
            left_ids[n_left++] = ids[i];
        } else {
            right_ids[n_right++] = ids[i];
        }
    }

    /* 防止退化（所有点都在一侧） */
    if (n_left == 0 || n_right == 0) {
        free(hyperplane);
        free(left_ids);
        free(right_ids);

        /* 作为叶节点处理 */
        int *ids_copy = (int *)malloc(n_ids * sizeof(int));
        if (!ids_copy) return NULL;
        memcpy(ids_copy, ids, n_ids * sizeof(int));
        return create_leaf_node(ids_copy, n_ids);
    }

    /* 递归构建子树 */
    annoy_node_t *left_child = build_tree_recursive(idx, left_ids, n_left, depth + 1);
    annoy_node_t *right_child = build_tree_recursive(idx, right_ids, n_right, depth + 1);

    free(left_ids);
    free(right_ids);

    if (!left_child || !right_child) {
        destroy_node(left_child);
        destroy_node(right_child);
        free(hyperplane);
        return NULL;
    }

    /* 创建内部节点 */
    annoy_node_t *node = create_inner_node(hyperplane, threshold);
    if (!node) {
        destroy_node(left_child);
        destroy_node(right_child);
        free(hyperplane);
        return NULL;
    }

    node->u.inner.left = left_child;
    node->u.inner.right = right_child;

    return node;
}

/* 优先队列项 */
typedef struct pq_item {
    float dist;
    int id;
} pq_item_t;

/* 大顶堆比较器 */
static int pq_compare(const void *a, const void *b)
{
    const pq_item_t *pa = (const pq_item_t *)a;
    const pq_item_t *pb = (const pq_item_t *)b;
    if (pa->dist > pb->dist) return -1;
    if (pa->dist < pb->dist) return 1;
    return 0;
}

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

/* 在单棵树中搜索 */
static void search_tree(const annoy_index_t *idx, const annoy_node_t *node,
                       const float *query, pq_item_t *heap, int *heap_size, int k)
{
    if (!node) return;

    if (node->type == NODE_LEAF) {
        /* 叶节点：计算所有候选点的距离 */
        for (int i = 0; i < node->u.leaf.n_ids; i++) {
            int id = node->u.leaf.ids[i];
            const float *v = idx->vectors + id * idx->dims;
            float dist = compute_distance(idx, query, v);

            /* 堆未满或距离小于堆顶 */
            if (*heap_size < k) {
                heap[*heap_size].dist = dist;
                heap[*heap_size].id = id;
                (*heap_size)++;

                /* 维持堆性质 */
                int j = *heap_size - 1;
                while (j > 0 && heap[(j - 1) / 2].dist < heap[j].dist) {
                    pq_item_t tmp = heap[j];
                    heap[j] = heap[(j - 1) / 2];
                    heap[(j - 1) / 2] = tmp;
                    j = (j - 1) / 2;
                }
            } else if (dist < heap[0].dist) {
                /* 替换堆顶 */
                heap[0].dist = dist;
                heap[0].id = id;
                heap_down(heap, *heap_size, 0);
            }
        }
    } else {
        /* 内部节点：根据分割超平面决定搜索方向 */
        float proj = dot_product(query, node->u.inner.hyperplane, idx->dims);

        annoy_node_t *near, *far;
        if (proj < node->u.inner.threshold) {
            near = node->u.inner.left;
            far = node->u.inner.right;
        } else {
            near = node->u.inner.right;
            far = node->u.inner.left;
        }

        /* 先搜索近侧 */
        search_tree(idx, near, query, heap, heap_size, k);

        /* 如果需要，搜索远侧 */
        if (*heap_size < k) {
            search_tree(idx, far, query, heap, heap_size, k);
        }
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

annoy_index_t *annoy_create(int dims, const char *metric)
{
    if (dims <= 0) return NULL;

    annoy_index_t *idx = (annoy_index_t *)calloc(1, sizeof(annoy_index_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->n_trees = 0;
    idx->n_items = 0;
    idx->built = false;
    idx->vectors = NULL;
    idx->vectors_capacity = 0;
    idx->trees = NULL;

    /* 设置距离度量 */
    if (metric && strcmp(metric, "euclidean") == 0) {
        idx->metric = 1;
    } else {
        idx->metric = 0;  /* 默认 angular */
    }

    return idx;
}

void annoy_destroy(annoy_index_t *idx)
{
    if (!idx) return;

    /* 销毁所有树 */
    if (idx->trees) {
        for (int i = 0; i < idx->n_trees; i++) {
            destroy_node(idx->trees[i].root);
        }
        free(idx->trees);
    }

    free(idx->vectors);
    free(idx);
}

int annoy_add_item(annoy_index_t *idx, int id, const float *vector)
{
    if (!idx || !vector) return -1;
    if (idx->built) return -1;  /* 构建后不能再添加 */

    /* 扩容 */
    if (idx->n_items >= idx->vectors_capacity) {
        int new_capacity = (idx->vectors_capacity == 0) ? 1024 : idx->vectors_capacity * 2;
        float *new_vectors = (float *)realloc(idx->vectors, new_capacity * idx->dims * sizeof(float));
        if (!new_vectors) return -1;
        idx->vectors = new_vectors;
        idx->vectors_capacity = new_capacity;
    }

    /* 存储向量 */
    memcpy(idx->vectors + id * idx->dims, vector, idx->dims * sizeof(float));
    if (id >= idx->n_items) {
        idx->n_items = id + 1;
    }

    return 0;
}

int annoy_build(annoy_index_t *idx, int n_trees)
{
    if (!idx || n_trees <= 0) return -1;
    if (idx->n_items == 0) return -1;

    idx->trees = (annoy_tree_t *)calloc(n_trees, sizeof(annoy_tree_t));
    if (!idx->trees) return -1;

    idx->n_trees = n_trees;

    /* 准备 ID 列表 */
    int *ids = (int *)malloc(idx->n_items * sizeof(int));
    if (!ids) {
        free(idx->trees);
        idx->trees = NULL;
        return -1;
    }

    for (int i = 0; i < idx->n_items; i++) {
        ids[i] = i;
    }

    /* 构建每棵树 */
    for (int t = 0; t < n_trees; t++) {
        idx->trees[t].root = build_tree_recursive(idx, ids, idx->n_items, 0);
        idx->trees[t].n_nodes = idx->n_items;  /* 估算 */
    }

    free(ids);
    idx->built = true;

    return 0;
}

int annoy_search(const annoy_index_t *idx, const float *query, int k,
                 int *result_ids, float *result_dists, int search_k)
{
    if (!idx || !query || k <= 0 || !result_ids) return -1;
    if (!idx->built) return -1;

    /* 默认 search_k = n_trees * k */
    if (search_k < 0) {
        search_k = idx->n_trees * k;
    }

    /* 大顶堆维护 top-k */
    pq_item_t *heap = (pq_item_t *)malloc((k + 1) * sizeof(pq_item_t));
    if (!heap) return -1;

    int heap_size = 0;

    /* 在所有树中搜索 */
    for (int t = 0; t < idx->n_trees; t++) {
        search_tree(idx, idx->trees[t].root, query, heap, &heap_size, k);
    }

    /* 堆排序（从小到大） */
    for (int i = heap_size - 1; i >= 0; i--) {
        result_ids[i] = heap[i].id;
        if (result_dists) {
            result_dists[i] = heap[i].dist;
        }
    }

    free(heap);
    return heap_size;
}

int annoy_save(const annoy_index_t *idx, const char *path)
{
    if (!idx || !path) return -1;
    if (!idx->built) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写头部 */
    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->n_trees, sizeof(int), 1, fp);
    fwrite(&idx->n_items, sizeof(int), 1, fp);
    fwrite(&idx->metric, sizeof(int), 1, fp);

    /* 写向量数据 */
    fwrite(idx->vectors, sizeof(float), idx->n_items * idx->dims, fp);

    /* 写树结构（简化版：只保存向量数据，加载时重建树） */
    fclose(fp);
    return 0;
}

annoy_index_t *annoy_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读头部 */
    int dims, n_trees, n_items, metric;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_trees, sizeof(int), 1, fp) != 1 ||
        fread(&n_items, sizeof(int), 1, fp) != 1 ||
        fread(&metric, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    annoy_index_t *idx = annoy_create(dims, metric == 1 ? "euclidean" : "angular");
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读向量数据 */
    idx->vectors = (float *)malloc(n_items * dims * sizeof(float));
    if (!idx->vectors) {
        annoy_destroy(idx);
        fclose(fp);
        return NULL;
    }

    if (fread(idx->vectors, sizeof(float), n_items * dims, fp) != (size_t)(n_items * dims)) {
        annoy_destroy(idx);
        fclose(fp);
        return NULL;
    }

    idx->n_items = n_items;
    idx->vectors_capacity = n_items;

    /* 重建树 */
    idx->trees = (annoy_tree_t *)calloc(n_trees, sizeof(annoy_tree_t));
    if (!idx->trees) {
        annoy_destroy(idx);
        fclose(fp);
        return NULL;
    }

    int *ids = (int *)malloc(n_items * sizeof(int));
    if (!ids) {
        annoy_destroy(idx);
        fclose(fp);
        return NULL;
    }

    for (int i = 0; i < n_items; i++) {
        ids[i] = i;
    }

    idx->n_trees = n_trees;
    for (int t = 0; t < n_trees; t++) {
        idx->trees[t].root = build_tree_recursive(idx, ids, n_items, 0);
    }

    free(ids);
    idx->built = true;

    fclose(fp);
    return idx;
}

int annoy_get_n_items(const annoy_index_t *idx)
{
    return idx ? idx->n_items : 0;
}

const float *annoy_get_item(const annoy_index_t *idx, int id)
{
    if (!idx || id < 0 || id >= idx->n_items) return NULL;
    return idx->vectors + id * idx->dims;
}

int annoy_get_n_trees(const annoy_index_t *idx)
{
    return idx ? idx->n_trees : 0;
}
