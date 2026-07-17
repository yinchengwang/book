/*
 * faiss_ivf_compat.c
 *
 * Faiss IVF 兼容层实现
 *
 * 兼容 Faiss IVF 接口：
 * - is_trained
 * - add
 * - search
 * - train
 */

#include <db/index/vector_index/faiss_ivf_compat/faiss_ivf_compat.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部结构 ── */

typedef struct {
    int *vector_indices;
    int n_vectors;
    int capacity;
} inv_list_t;

struct faiss_ivf_compat {
    int dims;
    int nlist;
    int n_vectors;
    int trained;

    float *centroids;
    inv_list_t *inv_lists;

    float *vectors;
    int *vector_ids;
    int max_vectors;
};

/* ── 内部辅助 ── */

static float l2_dist(const float *v1, const float *v2, int dims)
{
    float d = 0.0f;
    for (int i = 0; i < dims; i++) {
        float diff = v1[i] - v2[i];
        d += diff * diff;
    }
    return sqrtf(d);
}

static int nearest_cluster(const faiss_ivf_compat_t *idx, const float *v)
{
    int best = 0;
    float best_d = l2_dist(v, idx->centroids, idx->dims);
    for (int c = 1; c < idx->nlist; c++) {
        float d = l2_dist(v, idx->centroids + c * idx->dims, idx->dims);
        if (d < best_d) { best_d = d; best = c; }
    }
    return best;
}

/* ── 公共 API ── */

faiss_ivf_compat_t *faiss_ivf_compat_create(int dims, int nlist)
{
    if (dims <= 0 || nlist <= 0) return NULL;

    faiss_ivf_compat_t *idx = calloc(1, sizeof(faiss_ivf_compat_t));
    if (!idx) return NULL;

    idx->dims = dims;
    idx->nlist = nlist;
    idx->n_vectors = 0;
    idx->trained = 0;
    idx->max_vectors = 10000;

    idx->centroids = calloc(nlist * dims, sizeof(float));
    idx->inv_lists = calloc(nlist, sizeof(inv_list_t));
    for (int c = 0; c < nlist; c++) {
        idx->inv_lists[c].capacity = 16;
        idx->inv_lists[c].vector_indices = malloc(16 * sizeof(int));
    }

    idx->vectors = malloc(idx->max_vectors * dims * sizeof(float));
    idx->vector_ids = malloc(idx->max_vectors * sizeof(int));

    if (!idx->centroids || !idx->vectors || !idx->vector_ids) {
        faiss_ivf_compat_destroy(idx);
        return NULL;
    }

    return idx;
}

int faiss_ivf_compat_train(faiss_ivf_compat_t *idx, int n, const float *vectors)
{
    if (!idx || n <= 0 || !vectors) return -1;

    srand((unsigned)time(NULL));

    /* k-means++ */
    int *counts = calloc(idx->nlist, sizeof(int));

    /* 第一个中心 */
    int first = rand() % n;
    memcpy(idx->centroids, vectors + first * idx->dims, idx->dims * sizeof(float));

    /* 剩余中心 */
    for (int c = 1; c < idx->nlist; c++) {
        float max_min_d = -1.0f;
        const float *best = NULL;

        for (int i = 0; i < n; i++) {
            float min_d = FLT_MAX;
            for (int j = 0; j < c; j++) {
                float d = l2_dist(vectors + i * idx->dims,
                                 idx->centroids + j * idx->dims, idx->dims);
                if (d < min_d) min_d = d;
            }
            if (min_d > max_min_d) { max_min_d = min_d; best = vectors + i * idx->dims; }
        }

        if (best) memcpy(idx->centroids + c * idx->dims, best, idx->dims * sizeof(float));
    }

    /* k-means 迭代 */
    for (int iter = 0; iter < 20; iter++) {
        memset(counts, 0, idx->nlist * sizeof(int));
        memset(idx->centroids, 0, idx->nlist * idx->dims * sizeof(float));

        for (int i = 0; i < n; i++) {
            int cluster = nearest_cluster(idx, vectors + i * idx->dims);
            counts[cluster]++;
            for (int d = 0; d < idx->dims; d++) {
                idx->centroids[cluster * idx->dims + d] += vectors[i * idx->dims + d];
            }
        }

        for (int c = 0; c < idx->nlist; c++) {
            if (counts[c] > 0) {
                float inv = 1.0f / counts[c];
                for (int d = 0; d < idx->dims; d++) {
                    idx->centroids[c * idx->dims + d] *= inv;
                }
            }
        }
    }

    free(counts);
    idx->trained = 1;
    return 0;
}

int faiss_ivf_compat_add(faiss_ivf_compat_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !idx->trained || n <= 0 || !vectors || !ids) return 0;

    int added = 0;
    for (int i = 0; i < n; i++) {
        if (idx->n_vectors >= idx->max_vectors) {
            int new_max = idx->max_vectors * 2;
            float *new_v = realloc(idx->vectors, new_max * idx->dims * sizeof(float));
            int *new_ids = realloc(idx->vector_ids, new_max * sizeof(int));
            if (!new_v || !new_ids) break;
            idx->vectors = new_v;
            idx->vector_ids = new_ids;
            idx->max_vectors = new_max;
        }

        int vid = idx->n_vectors++;
        memcpy(&idx->vectors[vid * idx->dims], vectors + i * idx->dims, idx->dims * sizeof(float));
        idx->vector_ids[vid] = ids[i];

        int cluster = nearest_cluster(idx, vectors + i * idx->dims);
        inv_list_t *list = &idx->inv_lists[cluster];

        if (list->n_vectors >= list->capacity) {
            list->capacity *= 2;
            list->vector_indices = realloc(list->vector_indices, list->capacity * sizeof(int));
        }

        list->vector_indices[list->n_vectors++] = vid;
        added++;
    }
    return added;
}

int faiss_ivf_compat_search(const faiss_ivf_compat_t *idx, const float *query,
                            int k, int *ids, float *distances, int nprobe)
{
    if (!idx || !idx->trained || !query || k <= 0 || !ids || nprobe <= 0) return -1;
    if (idx->n_vectors == 0) return 0;

    /* 找最近簇 */
    typedef struct { int c; float d; } cd_t;
    cd_t *cds = malloc(idx->nlist * sizeof(cd_t));
    for (int c = 0; c < idx->nlist; c++) {
        cds[c].c = c;
        cds[c].d = l2_dist(query, idx->centroids + c * idx->dims, idx->dims);
    }

    /* 排序 */
    for (int i = 0; i < idx->nlist - 1; i++) {
        for (int j = i + 1; j < idx->nlist; j++) {
            if (cds[j].d < cds[i].d) {
                cd_t tmp = cds[i]; cds[i] = cds[j]; cds[j] = tmp;
            }
        }
    }

    int np = nprobe < idx->nlist ? nprobe : idx->nlist;

    /* 收集候选 */
    int *cands = malloc(idx->n_vectors * sizeof(int));
    float *cand_ds = malloc(idx->n_vectors * sizeof(float));
    int nc = 0;

    for (int p = 0; p < np; p++) {
        inv_list_t *list = &idx->inv_lists[cds[p].c];
        for (int i = 0; i < list->n_vectors; i++) {
            int vid = list->vector_indices[i];
            cands[nc] = vid;
            cand_ds[nc] = l2_dist(query, idx->vectors + vid * idx->dims, idx->dims);
            nc++;
        }
    }

    free(cds);

    /* top-k */
    int rc = k < nc ? k : nc;
    for (int i = 0; i < rc; i++) {
        int min_i = i;
        for (int j = i + 1; j < nc; j++) {
            if (cand_ds[j] < cand_ds[min_i]) min_i = j;
        }
        if (min_i != i) {
            int tmp = cands[i]; cands[i] = cands[min_i]; cands[min_i] = tmp;
            float tmpd = cand_ds[i]; cand_ds[i] = cand_ds[min_i]; cand_ds[min_i] = tmpd;
        }
        ids[i] = idx->vector_ids[cands[i]];
        if (distances) distances[i] = cand_ds[i];
    }

    free(cands);
    free(cand_ds);
    return rc;
}

int faiss_ivf_compat_search_batch(const faiss_ivf_compat_t *idx, int nq, const float *queries,
                                 int k, int *ids, float *distances, int nprobe)
{
    if (!idx || nq <= 0 || !queries || k <= 0 || !ids) return -1;
    for (int q = 0; q < nq; q++) {
        int r = faiss_ivf_compat_search(idx, queries + q * idx->dims, k,
                                        ids + q * k, distances ? distances + q * k : NULL, nprobe);
        if (r < 0) return -1;
    }
    return 0;
}

void faiss_ivf_compat_stats(const faiss_ivf_compat_t *idx,
                            int *out_n_vectors, int *out_dims, int *out_nlist)
{
    if (!idx) return;
    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_dims) *out_dims = idx->dims;
    if (out_nlist) *out_nlist = idx->nlist;
}

int faiss_ivf_compat_save(const faiss_ivf_compat_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->nlist, sizeof(int), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int), 1, fp);
    fwrite(&idx->trained, sizeof(int), 1, fp);

    fwrite(idx->centroids, sizeof(float), idx->nlist * idx->dims, fp);
    fwrite(idx->vectors, sizeof(float), idx->n_vectors * idx->dims, fp);
    fwrite(idx->vector_ids, sizeof(int), idx->n_vectors, fp);

    for (int c = 0; c < idx->nlist; c++) {
        fwrite(&idx->inv_lists[c].n_vectors, sizeof(int), 1, fp);
        fwrite(idx->inv_lists[c].vector_indices, sizeof(int),
               idx->inv_lists[c].n_vectors, fp);
    }

    fclose(fp);
    return 0;
}

faiss_ivf_compat_t *faiss_ivf_compat_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    int dims, nlist, n_vectors, trained;
    if (fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&nlist, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1 ||
        fread(&trained, sizeof(int), 1, fp) != 1) {
        fclose(fp); return NULL;
    }

    faiss_ivf_compat_t *idx = faiss_ivf_compat_create(dims, nlist);
    if (!idx) { fclose(fp); return NULL; }

    idx->trained = trained;
    idx->n_vectors = n_vectors;

    fread(idx->centroids, sizeof(float), nlist * dims, fp);
    fread(idx->vectors, sizeof(float), n_vectors * dims, fp);
    fread(idx->vector_ids, sizeof(int), n_vectors, fp);

    for (int c = 0; c < nlist; c++) {
        fread(&idx->inv_lists[c].n_vectors, sizeof(int), 1, fp);
        fread(idx->inv_lists[c].vector_indices, sizeof(int),
               idx->inv_lists[c].n_vectors, fp);
    }

    fclose(fp);
    return idx;
}

void faiss_ivf_compat_destroy(faiss_ivf_compat_t *idx)
{
    if (!idx) return;

    if (idx->inv_lists) {
        for (int c = 0; c < idx->nlist; c++) free(idx->inv_lists[c].vector_indices);
        free(idx->inv_lists);
    }

    free(idx->centroids);
    free(idx->vectors);
    free(idx->vector_ids);
    free(idx);
}