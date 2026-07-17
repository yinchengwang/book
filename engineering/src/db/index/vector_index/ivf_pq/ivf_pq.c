/*
 * ivf_pq.c
 *
 * IVF-PQ (Inverted File with Product Quantization) 索引实现
 */

#include <db/index/vector_index/ivf_pq/ivf_pq.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>
#include <algo-prod/Kmeans/kmeans.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

typedef struct ivf_pq_entry {
    int id;
    int cluster;
    uint8_t *pq_code;
} ivf_pq_entry_t;

typedef struct ivf_pq_list {
    ivf_pq_entry_t *entries;
    int n_entries;
    int capacity;
} ivf_pq_list_t;

struct ivf_pq_index {
    int nlist;
    int nprobe;
    int pq_m;
    int pq_bits;
    int pq_k;
    int dims;
    int code_size;
    float *centroids;
    ivf_pq_list_t *lists;
    quantizer_t *pq;
    int n_total;
    int trained;
};

static int _find_nearest_cluster(const ivf_pq_index_t *idx, const float *vec);
static float _compute_l2_dist(const float *a, const float *b, int dims);

int ivf_pq_train(ivf_pq_index_t *idx, int n, const float *vectors)
{
    if (!idx || !vectors || n <= 0) return -1;

    /* 转换为 double 以符合 KMeans API 要求 */
    double *vectors_d = (double *)malloc((size_t)n * idx->dims * sizeof(double));
    if (!vectors_d) return -1;
    for (int i = 0; i < n * idx->dims; i++) {
        vectors_d[i] = (double)vectors[i];
    }

    double *centroids_d = (double *)malloc((size_t)idx->nlist * idx->dims * sizeof(double));
    if (!centroids_d) {
        free(vectors_d);
        return -1;
    }

    KMeansParams params = {0};
    params.n_clusters = idx->nlist;
    params.n_init = 1;
    params.max_iter = 25;
    params.verbose = 0;
    params.n_samples = n;
    params.n_features = idx->dims;
    params.data = vectors_d;
    params.cluster_centers = centroids_d;

    Kmeans(&params);

    if (!params.success) {
        free(vectors_d);
        KmeansFree(&params);
        return -1;
    }

    /* 转回 float */
    for (int i = 0; i < idx->nlist * idx->dims; i++) {
        idx->centroids[i] = (float)centroids_d[i];
    }

    free(vectors_d);
    KmeansFree(&params);

    if (quantizer_train(idx->pq, n, vectors) != 0) return -1;

    idx->trained = 1;
    return 0;
}

int ivf_pq_add(ivf_pq_index_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !vectors || !ids || !idx->trained) return -1;

    int added = 0;
    for (int i = 0; i < n; i++) {
        const float *vec = &vectors[i * idx->dims];
        int cluster = _find_nearest_cluster(idx, vec);

        uint8_t *code = (uint8_t *)malloc(idx->code_size);
        if (!code) continue;

        if (quantizer_encode(idx->pq, vec, code) != 0) { free(code); continue; }

        ivf_pq_list_t *list = &idx->lists[cluster];

        if (list->n_entries >= list->capacity) {
            list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
            ivf_pq_entry_t *new_entries = (ivf_pq_entry_t *)realloc(list->entries, list->capacity * sizeof(ivf_pq_entry_t));
            if (!new_entries) { free(code); continue; }
            list->entries = new_entries;
        }

        ivf_pq_entry_t *entry = &list->entries[list->n_entries];
        entry->id = ids[i];
        entry->cluster = cluster;
        entry->pq_code = code;

        list->n_entries++;
        idx->n_total++;
        added++;
    }
    return added;
}

int ivf_pq_search(const ivf_pq_index_t *idx, const float *query, int k, int *out_ids, float *out_distances)
{
    if (!idx || !query || !out_ids || !out_distances || !idx->trained) return -1;

    typedef struct { int id; float dist; } result_t;
    result_t *cluster_dists = (result_t *)malloc(idx->nlist * sizeof(result_t));

    for (int c = 0; c < idx->nlist; c++) {
        cluster_dists[c].id = c;
        cluster_dists[c].dist = _compute_l2_dist(query, &idx->centroids[c * idx->dims], idx->dims);
    }

    for (int i = 0; i < idx->nprobe && i < idx->nlist; i++) {
        int best = i;
        for (int j = i + 1; j < idx->nlist; j++) {
            if (cluster_dists[j].dist < cluster_dists[best].dist) best = j;
        }
        if (best != i) {
            result_t tmp = cluster_dists[i]; cluster_dists[i] = cluster_dists[best]; cluster_dists[best] = tmp;
        }
    }

    float *distance_table = (float *)malloc(idx->pq_m * idx->pq_k * sizeof(float));
    quantizer_compute_distance_table(idx->pq, DISTANCE_METRIC_L2_SQUARED, query, distance_table);

    typedef struct { int id; float dist; } cand_t;
    cand_t *candidates = (cand_t *)malloc(k * idx->nprobe * sizeof(cand_t));
    int n_candidates = 0;

    for (int i = 0; i < idx->nprobe && i < idx->nlist; i++) {
        int cluster = cluster_dists[i].id;
        ivf_pq_list_t *list = &idx->lists[cluster];

        for (int j = 0; j < list->n_entries; j++) {
            float dist = quantizer_adc_distance(idx->pq, list->entries[j].pq_code, distance_table);
            candidates[n_candidates].id = list->entries[j].id;
            candidates[n_candidates].dist = dist;
            n_candidates++;
        }
    }

    int count = n_candidates < k ? n_candidates : k;
    for (int i = 0; i < count; i++) {
        int best = i;
        for (int j = i + 1; j < n_candidates; j++) {
            if (candidates[j].dist < candidates[best].dist) best = j;
        }
        if (best != i) {
            cand_t tmp = candidates[i]; candidates[i] = candidates[best]; candidates[best] = tmp;
        }
        out_ids[i] = candidates[i].id;
        out_distances[i] = candidates[i].dist;
    }

    free(cluster_dists);
    free(distance_table);
    free(candidates);

    return count;
}

static int _find_nearest_cluster(const ivf_pq_index_t *idx, const float *vec)
{
    int best = 0;
    float best_dist = FLT_MAX;
    for (int c = 0; c < idx->nlist; c++) {
        float dist = _compute_l2_dist(vec, &idx->centroids[c * idx->dims], idx->dims);
        if (dist < best_dist) { best_dist = dist; best = c; }
    }
    return best;
}

static float _compute_l2_dist(const float *a, const float *b, int dims)
{
    return distance_l2sqr(a, b, dims);
}

ivf_pq_index_t *ivf_pq_create(int nlist, int pq_m, int pq_bits, int dims)
{
    if (nlist <= 0 || pq_m <= 0 || pq_bits <= 0 || dims <= 0) return NULL;

    ivf_pq_index_t *idx = (ivf_pq_index_t *)calloc(1, sizeof(ivf_pq_index_t));
    if (!idx) return NULL;

    idx->nlist = nlist;
    idx->nprobe = 64;
    idx->pq_m = pq_m;
    idx->pq_bits = pq_bits;
    idx->pq_k = 1 << pq_bits;
    idx->dims = dims;
    idx->code_size = pq_m;
    idx->n_total = 0;
    idx->trained = 0;

    idx->centroids = (float *)calloc(nlist * dims, sizeof(float));
    idx->lists = (ivf_pq_list_t *)calloc(nlist, sizeof(ivf_pq_list_t));

    quantizer_config_t config;
    quantizer_config_init(&config, dims, QUANTIZATION_TYPE_PQ);
    config.pq_subquantizers = pq_m;
    config.pq_bits = pq_bits;
    idx->pq = quantizer_create(&config);

    if (!idx->centroids || !idx->lists || !idx->pq) {
        free(idx->centroids); free(idx->lists);
        if (idx->pq) quantizer_drop(idx->pq);
        free(idx); return NULL;
    }
    return idx;
}

void ivf_pq_set_nprobe(ivf_pq_index_t *idx, int nprobe)
{
    if (!idx || nprobe <= 0) return;
    idx->nprobe = nprobe > idx->nlist ? idx->nlist : nprobe;
}

int ivf_pq_rerank(ivf_pq_index_t *idx, int n, const float *vectors, int k, int *ids, float *distances)
{
    (void)idx; (void)vectors;
    for (int i = 0; i < n && i < k; i++) { ids[i] = i; distances[i] = 0; }
    return n < k ? n : k;
}

/* IVF-PQ 文件魔数和版本 */
#define IVF_PQ_MAGIC 0x4956508510A50001ULL  /* "IVFPQ" + version */
#define IVF_PQ_VERSION 1

/* IVF-PQ 文件头 */
typedef struct {
    uint64_t magic;
    uint32_t version;
    int nlist;
    int nprobe;
    int pq_m;
    int pq_bits;
    int pq_k;
    int dims;
    int code_size;
    int n_total;
    int trained;
} ivf_pq_header_t;

int ivf_pq_save(const ivf_pq_index_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入文件头 */
    ivf_pq_header_t header = {
        .magic = IVF_PQ_MAGIC,
        .version = IVF_PQ_VERSION,
        .nlist = idx->nlist,
        .nprobe = idx->nprobe,
        .pq_m = idx->pq_m,
        .pq_bits = idx->pq_bits,
        .pq_k = idx->pq_k,
        .dims = idx->dims,
        .code_size = idx->code_size,
        .n_total = idx->n_total,
        .trained = idx->trained
    };

    fwrite(&header, sizeof(header), 1, fp);

    /* 写入聚类中心 */
    fwrite(idx->centroids, sizeof(float), idx->nlist * idx->dims, fp);

    /* 写入每个 list 的 PQ 码 */
    for (int i = 0; i < idx->nlist; i++) {
        /* 写入 list 中的条目数 */
        int n_entries = idx->lists[i].n_entries;
        fwrite(&n_entries, sizeof(int), 1, fp);

        /* 写入每个条目的 ID 和 PQ 码 */
        for (int j = 0; j < n_entries; j++) {
            fwrite(&idx->lists[i].entries[j].id, sizeof(int), 1, fp);
            fwrite(&idx->lists[i].entries[j].cluster, sizeof(int), 1, fp);
            fwrite(idx->lists[i].entries[j].pq_code, 1, idx->code_size, fp);
        }
    }

    fclose(fp);
    return 0;
}

static ivf_pq_list_t *_create_empty_list(void) {
    ivf_pq_list_t *list = (ivf_pq_list_t *)calloc(1, sizeof(ivf_pq_list_t));
    return list;
}

ivf_pq_index_t *ivf_pq_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取文件头 */
    ivf_pq_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 验证魔数和版本 */
    if (header.magic != IVF_PQ_MAGIC || header.version != IVF_PQ_VERSION) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引结构 */
    ivf_pq_index_t *idx = (ivf_pq_index_t *)calloc(1, sizeof(ivf_pq_index_t));
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    idx->nlist = header.nlist;
    idx->nprobe = header.nprobe;
    idx->pq_m = header.pq_m;
    idx->pq_bits = header.pq_bits;
    idx->pq_k = header.pq_k;
    idx->dims = header.dims;
    idx->code_size = header.code_size;
    idx->n_total = header.n_total;
    idx->trained = header.trained;

    /* 分配内存 */
    idx->centroids = (float *)malloc(header.nlist * header.dims * sizeof(float));
    idx->lists = (ivf_pq_list_t *)calloc(header.nlist, sizeof(ivf_pq_list_t));

    if (!idx->centroids || !idx->lists) {
        free(idx->centroids);
        free(idx->lists);
        free(idx);
        fclose(fp);
        return NULL;
    }

    /* 读取聚类中心 */
    fread(idx->centroids, sizeof(float), header.nlist * header.dims, fp);

    /* 读取每个 list */
    for (int i = 0; i < header.nlist; i++) {
        int n_entries;
        fread(&n_entries, sizeof(int), 1, fp);

        if (n_entries > 0) {
            idx->lists[i].entries = (ivf_pq_entry_t *)malloc(n_entries * sizeof(ivf_pq_entry_t));
            idx->lists[i].capacity = n_entries;
            idx->lists[i].n_entries = n_entries;

            for (int j = 0; j < n_entries; j++) {
                idx->lists[i].entries[j].pq_code = (uint8_t *)malloc(header.code_size);
                fread(&idx->lists[i].entries[j].id, sizeof(int), 1, fp);
                fread(&idx->lists[i].entries[j].cluster, sizeof(int), 1, fp);
                fread(idx->lists[i].entries[j].pq_code, 1, header.code_size, fp);
            }
        }
    }

    fclose(fp);
    return idx;
}

void ivf_pq_stats(const ivf_pq_index_t *idx, int *out_n_vectors, int *out_code_size)
{
    if (!idx) {
        if (out_n_vectors) *out_n_vectors = 0;
        if (out_code_size) *out_code_size = 0;
        return;
    }
    if (out_n_vectors) *out_n_vectors = idx->n_total;
    if (out_code_size) *out_code_size = idx->code_size;
}

void ivf_pq_destroy(ivf_pq_index_t *idx)
{
    if (!idx) return;
    for (int i = 0; i < idx->nlist; i++) {
        for (int j = 0; j < idx->lists[i].n_entries; j++) free(idx->lists[i].entries[j].pq_code);
        free(idx->lists[i].entries);
    }
    free(idx->lists);
    free(idx->centroids);
    if (idx->pq) quantizer_drop(idx->pq);
    free(idx);
}
