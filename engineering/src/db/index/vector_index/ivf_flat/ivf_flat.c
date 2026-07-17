/*
 * ivf_flat.c — IVF-Flat (Inverted File Flat) 索引实现
 *
 * === 实现要点 ===
 * 1. K-Means 训练：复用 algo-prod 的 kmeans_double API
 * 2. 倒排列表：每个簇一个动态数组，存储 (id, vector) 对
 * 3. 搜索：大顶堆维护 top-k，按距离排序返回
 * 4. 持久化：二进制格式（中心 + 倒排列表）
 */

#include <db/index/vector_index/ivf_flat/ivf_flat.h>

#include <algo-prod/Kmeans/kmeans.h>
#include <algo-prod/distance/distance.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/* 倒排列表中的向量项 */
typedef struct ivf_flat_entry {
    int32_t id;              /* 向量 ID */
    float *vector;           /* 向量数据 [dims] */
} ivf_flat_entry_t;

/* 倒排列表 */
typedef struct ivf_flat_list {
    ivf_flat_entry_t *entries;  /* 向量项数组 */
    int32_t size;               /* 当前元素数 */
    int32_t capacity;           /* 容量 */
} ivf_flat_list_t;

/* IVF-Flat 索引结构 */
struct ivf_flat_index {
    int32_t nlist;               /* 聚类中心数 */
    int32_t dims;                /* 向量维度 */
    int32_t nprobe;              /* 搜索探针数 */
    int32_t n_vectors;           /* 已添加向量总数 */
    bool trained;                /* 是否已训练 */

    float *centroids;            /* 聚类中心 [nlist * dims] */
    ivf_flat_list_t *invlists;   /* 倒排列表数组 [nlist] */
};

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/* 初始化倒排列表 */
static void invlist_init(ivf_flat_list_t *list)
{
    list->entries = NULL;
    list->size = 0;
    list->capacity = 0;
}

/* 释放倒排列表 */
static void invlist_destroy(ivf_flat_list_t *list)
{
    if (list->entries) {
        for (int32_t i = 0; i < list->size; i++) {
            free(list->entries[i].vector);
        }
        free(list->entries);
    }
    list->entries = NULL;
    list->size = 0;
    list->capacity = 0;
}

/* 添加向量到倒排列表 */
static int invlist_add(ivf_flat_list_t *list, int32_t id, const float *vector, int32_t dims)
{
    /* 扩容 */
    if (list->size >= list->capacity) {
        int32_t new_capacity = (list->capacity == 0) ? 16 : list->capacity * 2;
        ivf_flat_entry_t *new_entries = (ivf_flat_entry_t *)realloc(
            list->entries, new_capacity * sizeof(ivf_flat_entry_t));
        if (!new_entries) return -1;
        list->entries = new_entries;
        list->capacity = new_capacity;
    }

    /* 添加向量 */
    float *vec_copy = (float *)malloc(dims * sizeof(float));
    if (!vec_copy) return -1;
    memcpy(vec_copy, vector, dims * sizeof(float));

    list->entries[list->size].id = id;
    list->entries[list->size].vector = vec_copy;
    list->size++;

    return 0;
}

/* 找到距离向量最近的聚类中心 */
static int32_t find_nearest_centroid(const ivf_flat_index_t *idx, const float *vector)
{
    int32_t best_cluster = 0;
    float best_dist = FLT_MAX;

    for (int32_t i = 0; i < idx->nlist; i++) {
        const float *centroid = idx->centroids + i * idx->dims;
        float dist = distance_l2sqr(vector, centroid, idx->dims);
        if (dist < best_dist) {
            best_dist = dist;
            best_cluster = i;
        }
    }

    return best_cluster;
}

/* 大顶堆维护 top-k */
typedef struct heap_item {
    float dist;
    int32_t id;
} heap_item_t;

/* 堆比较器：大顶堆，距离大的在堆顶 */
static int heap_compare(const void *a, const void *b)
{
    const heap_item_t *ha = (const heap_item_t *)a;
    const heap_item_t *hb = (const heap_item_t *)b;
    if (ha->dist > hb->dist) return -1;
    if (ha->dist < hb->dist) return 1;
    return 0;
}

/* 维持堆性质（大顶堆） */
static void heap_down(heap_item_t *heap, int32_t size, int32_t i)
{
    while (1) {
        int32_t largest = i;
        int32_t left = 2 * i + 1;
        int32_t right = 2 * i + 2;

        if (left < size && heap[left].dist > heap[largest].dist) {
            largest = left;
        }
        if (right < size && heap[right].dist > heap[largest].dist) {
            largest = right;
        }

        if (largest == i) break;

        heap_item_t tmp = heap[i];
        heap[i] = heap[largest];
        heap[largest] = tmp;

        i = largest;
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

ivf_flat_index_t *ivf_flat_create(int nlist, int dims)
{
    if (nlist <= 0 || dims <= 0) return NULL;

    ivf_flat_index_t *idx = (ivf_flat_index_t *)calloc(1, sizeof(ivf_flat_index_t));
    if (!idx) return NULL;

    idx->nlist = nlist;
    idx->dims = dims;
    idx->nprobe = 1;  /* 默认只探测 1 个簇 */
    idx->n_vectors = 0;
    idx->trained = false;

    idx->centroids = (float *)calloc(nlist * dims, sizeof(float));
    if (!idx->centroids) {
        free(idx);
        return NULL;
    }

    idx->invlists = (ivf_flat_list_t *)calloc(nlist, sizeof(ivf_flat_list_t));
    if (!idx->invlists) {
        free(idx->centroids);
        free(idx);
        return NULL;
    }

    for (int32_t i = 0; i < nlist; i++) {
        invlist_init(&idx->invlists[i]);
    }

    return idx;
}

void ivf_flat_destroy(ivf_flat_index_t *idx)
{
    if (!idx) return;

    if (idx->invlists) {
        for (int32_t i = 0; i < idx->nlist; i++) {
            invlist_destroy(&idx->invlists[i]);
        }
        free(idx->invlists);
    }

    free(idx->centroids);
    free(idx);
}

int ivf_flat_train(ivf_flat_index_t *idx, int n, const float *vectors)
{
    if (!idx || !vectors || n <= 0) return -1;
    if (n < idx->nlist) return -1;  /* 训练样本数不能少于簇数 */

    /* 转换为 double 以符合 KMeans API 要求 */
    double *vectors_d = (double *)malloc((size_t)n * idx->dims * sizeof(double));
    if (!vectors_d) return -1;

    for (int i = 0; i < n * idx->dims; i++) {
        vectors_d[i] = (double)vectors[i];
    }

    /* KMeans API 会释放并重新分配 cluster_centers。
     * 因此使用临时 double 缓冲区，KMeans 返回后拷贝回 float 数组。 */
    double *centroids_d = (double *)malloc((size_t)idx->nlist * idx->dims * sizeof(double));
    if (!centroids_d) {
        free(vectors_d);
        return -1;
    }

    KMeansParams params = {0};
    params.n_clusters = idx->nlist;
    params.n_samples = n;
    params.n_features = idx->dims;
    params.data = vectors_d;
    params.cluster_centers = centroids_d;
    params.labels = NULL;  /* 我们不需要 labels */
    params.n_init = 1;
    params.max_iter = 10;
    params.tol = 1e-4;
    params.verbose = 0;

    Kmeans(&params);

    if (!params.success) {
        free(vectors_d);
        /* KmeansFree 会释放 centroids_d 和 labels */
        KmeansFree(&params);
        return -1;
    }

    /* 转回 float 并写入 idx->centroids */
    for (int i = 0; i < idx->nlist * idx->dims; i++) {
        idx->centroids[i] = (float)centroids_d[i];
    }

    free(vectors_d);
    /* Kmeans 内部重新分配了 cluster_centers，使用 KmeansFree 释放 */
    KmeansFree(&params);
    idx->trained = true;
    return 0;
}

int ivf_flat_add(ivf_flat_index_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !vectors || !ids || n <= 0) return -1;
    if (!idx->trained) return -1;  /* 必须先训练 */

    int added = 0;
    for (int i = 0; i < n; i++) {
        const float *vec = vectors + i * idx->dims;
        int32_t cluster = find_nearest_centroid(idx, vec);

        if (invlist_add(&idx->invlists[cluster], ids[i], vec, idx->dims) == 0) {
            added++;
            idx->n_vectors++;
        }
    }

    return added;
}

int ivf_flat_search(const ivf_flat_index_t *idx, const float *query, int k,
                    int *result_ids, float *result_dists)
{
    if (!idx || !query || k <= 0 || !result_ids || !result_dists) return -1;
    if (!idx->trained) return -1;
    if (idx->n_vectors == 0) return 0;

    /* 找到最近的 nprobe 个簇 */
    int32_t *cluster_ids = (int32_t *)malloc(idx->nprobe * sizeof(int32_t));
    float *cluster_dists = (float *)malloc(idx->nprobe * sizeof(float));
    if (!cluster_ids || !cluster_dists) {
        free(cluster_ids);
        free(cluster_dists);
        return -1;
    }

    /* 计算所有簇的距离 */
    typedef struct cluster_dist {
        float dist;
        int32_t id;
    } cluster_dist_t;

    cluster_dist_t *all_clusters = (cluster_dist_t *)malloc(idx->nlist * sizeof(cluster_dist_t));
    if (!all_clusters) {
        free(cluster_ids);
        free(cluster_dists);
        return -1;
    }

    for (int32_t i = 0; i < idx->nlist; i++) {
        all_clusters[i].dist = distance_l2sqr(query, idx->centroids + i * idx->dims, idx->dims);
        all_clusters[i].id = i;
    }

    /* 按距离排序 */
    qsort(all_clusters, idx->nlist, sizeof(cluster_dist_t),
          (int (*)(const void *, const void *))heap_compare);

    /* 取前 nprobe 个 */
    for (int32_t i = 0; i < idx->nprobe && i < idx->nlist; i++) {
        cluster_ids[i] = all_clusters[i].id;
    }

    free(all_clusters);

    /* 大顶堆维护 top-k */
    heap_item_t *heap = (heap_item_t *)malloc((k + 1) * sizeof(heap_item_t));
    if (!heap) {
        free(cluster_ids);
        free(cluster_dists);
        return -1;
    }
    int32_t heap_size = 0;

    /* 扫描选中的簇 */
    for (int32_t c = 0; c < idx->nprobe; c++) {
        int32_t cluster_id = cluster_ids[c];
        ivf_flat_list_t *list = &idx->invlists[cluster_id];

        for (int32_t i = 0; i < list->size; i++) {
            float dist = distance_l2sqr(query, list->entries[i].vector, idx->dims);

            /* 堆未满或距离小于堆顶 */
            if (heap_size < k) {
                heap[heap_size].dist = dist;
                heap[heap_size].id = list->entries[i].id;
                heap_size++;

                /* 维持堆性质 */
                int32_t j = heap_size - 1;
                while (j > 0 && heap[(j - 1) / 2].dist < heap[j].dist) {
                    heap_item_t tmp = heap[j];
                    heap[j] = heap[(j - 1) / 2];
                    heap[(j - 1) / 2] = tmp;
                    j = (j - 1) / 2;
                }
            } else if (dist < heap[0].dist) {
                /* 替换堆顶 */
                heap[0].dist = dist;
                heap[0].id = list->entries[i].id;
                heap_down(heap, heap_size, 0);
            }
        }
    }

    /* 堆排序（从小到大） */
    for (int32_t i = heap_size - 1; i >= 0; i--) {
        result_ids[i] = heap[i].id;
        result_dists[i] = heap[i].dist;
    }

    free(heap);
    free(cluster_ids);
    free(cluster_dists);

    return (int)heap_size;
}

void ivf_flat_set_nprobe(ivf_flat_index_t *idx, int nprobe)
{
    if (!idx) return;
    idx->nprobe = (nprobe < 1) ? 1 : (nprobe > idx->nlist) ? idx->nlist : nprobe;
}

void ivf_flat_stats(const ivf_flat_index_t *idx, int *n_vectors, int *n_clusters)
{
    if (!idx) return;
    if (n_vectors) *n_vectors = idx->n_vectors;
    if (n_clusters) *n_clusters = idx->nlist;
}

int ivf_flat_save(const ivf_flat_index_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写头部：nlist, dims, n_vectors */
    fwrite(&idx->nlist, sizeof(int32_t), 1, fp);
    fwrite(&idx->dims, sizeof(int32_t), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int32_t), 1, fp);

    /* 写聚类中心 */
    fwrite(idx->centroids, sizeof(float), idx->nlist * idx->dims, fp);

    /* 写倒排列表 */
    for (int32_t i = 0; i < idx->nlist; i++) {
        ivf_flat_list_t *list = &idx->invlists[i];
        fwrite(&list->size, sizeof(int32_t), 1, fp);

        for (int32_t j = 0; j < list->size; j++) {
            fwrite(&list->entries[j].id, sizeof(int32_t), 1, fp);
            fwrite(list->entries[j].vector, sizeof(float), idx->dims, fp);
        }
    }

    fclose(fp);
    return 0;
}

ivf_flat_index_t *ivf_flat_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读头部 */
    int32_t nlist, dims, n_vectors;
    if (fread(&nlist, sizeof(int32_t), 1, fp) != 1 ||
        fread(&dims, sizeof(int32_t), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int32_t), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    ivf_flat_index_t *idx = ivf_flat_create(nlist, dims);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读聚类中心 */
    if (fread(idx->centroids, sizeof(float), nlist * dims, fp) != (size_t)(nlist * dims)) {
        ivf_flat_destroy(idx);
        fclose(fp);
        return NULL;
    }
    idx->trained = true;

    /* 读倒排列表 */
    for (int32_t i = 0; i < nlist; i++) {
        int32_t size;
        if (fread(&size, sizeof(int32_t), 1, fp) != 1) {
            ivf_flat_destroy(idx);
            fclose(fp);
            return NULL;
        }

        for (int32_t j = 0; j < size; j++) {
            int32_t id;
            float *vec = (float *)malloc(dims * sizeof(float));
            if (!vec ||
                fread(&id, sizeof(int32_t), 1, fp) != 1 ||
                fread(vec, sizeof(float), dims, fp) != (size_t)dims) {
                free(vec);
                ivf_flat_destroy(idx);
                fclose(fp);
                return NULL;
            }

            invlist_add(&idx->invlists[i], id, vec, dims);
            free(vec);
        }
    }

    idx->n_vectors = n_vectors;
    fclose(fp);
    return idx;
}