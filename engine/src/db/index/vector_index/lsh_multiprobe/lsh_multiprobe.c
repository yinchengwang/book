/*
 * lsh_multiprobe.c
 *
 * Multi-Probe LSH (多探测 LSH) 实现
 *
 * 核心思想：
 * 标准 LSH 只探测查询点落入的主桶，Multi-Probe 通过探测相邻桶提高召回率。
 *
 * 探测序列生成算法：
 * 1. 计算查询点到各哈希函数分割边界的距离
 * 2. 对距离排序，生成探测序列
 * 3. 按序列依次探测桶
 *
 * 参考文献：
 * - "Multi-Probe LSH: Efficient Indexing for High-Dimensional Similarity Search"
 *   Qin Lv, William Josephson, Zhe Wang, Moses Charikar, Kai Li (VLDB 2007)
 */

#include <db/index/vector_index/lsh_multiprobe/lsh_multiprobe.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define LSH_W 4.0f              /* p-stable LSH 的桶宽度 */
#define LSH_MAX_CANDIDATES 1000 /* 最大候选集大小 */
#define LSH_MAX_PROBES 100      /* 最大探测数量 */

/* ── 内部结构 ── */

/* p-stable LSH 哈希函数参数 */
typedef struct {
    float *a;       /* 随机投影向量 [dims] */
    float b;        /* 偏移量 [0, W) */
} hash_func_t;

/* 探测序列条目 */
typedef struct {
    int table_idx;  /* 哈希表索引 */
    int bucket_idx; /* 桶索引（偏移后的桶） */
    float score;    /* 探测得分（距离主桶的估计距离） */
} probe_entry_t;

/* 哈希桶条目 */
typedef struct bucket_entry {
    int id;                     /* 向量 ID */
    float *vector;              /* 向量副本 */
    struct bucket_entry *next;  /* 链表下一个 */
} bucket_entry_t;

/* 哈希表 */
typedef struct {
    bucket_entry_t **buckets;   /* 桶数组 */
    int n_buckets;              /* 桶数量（2^n_hash） */
} hash_table_t;

/* Multi-Probe LSH 索引结构 */
struct lsh_multiprobe {
    int n_hash;                 /* 每个表的哈希函数数量 */
    int n_tables;               /* 哈希表数量 */
    int dims;                   /* 向量维度 */
    int n_vectors;              /* 已添加向量数 */
    int max_vectors;            /* 最大向量数 */

    hash_func_t *hash_funcs;    /* 哈希函数 [n_tables][n_hash] */
    hash_table_t *tables;       /* 哈希表 [n_tables] */

    float *vectors;             /* 向量存储 [max_vectors, dims] */
    int *vector_ids;            /* 向量 ID [max_vectors] */
};

/* ── 内部辅助函数 ── */

/**
 * 生成随机高斯向量
 */
static void generate_gaussian_vector(float *v, int dims)
{
    /* Box-Muller 变换生成正态分布随机数 */
    for (int i = 0; i < dims; i += 2) {
        float u1 = (float)rand() / RAND_MAX;
        float u2 = (float)rand() / RAND_MAX;
        if (u1 < 1e-10f) u1 = 1e-10f;

        float r = sqrtf(-2.0f * logf(u1));
        float theta = 2.0f * (float)M_PI * u2;

        v[i] = r * cosf(theta);
        if (i + 1 < dims) {
            v[i + 1] = r * sinf(theta);
        }
    }
}

/**
 * 计算单个哈希值
 */
static int compute_single_hash(const hash_func_t *func, const float *vector, int dims)
{
    /* 计算 h(v) = floor((a·v + b) / W) */
    float dot = 0.0f;
    for (int i = 0; i < dims; i++) {
        dot += func->a[i] * vector[i];
    }

    return (int)floorf((dot + func->b) / LSH_W);
}

/**
 * 计算哈希表的主桶索引
 */
static int compute_bucket_index(const lsh_multiprobe_t *idx, int table_idx, const float *vector)
{
    /* 组合 n_hash 个哈希值 */
    int bucket = 0;
    for (int i = 0; i < idx->n_hash; i++) {
        int h = compute_single_hash(&idx->hash_funcs[table_idx * idx->n_hash + i], vector, idx->dims);
        /* 使用简单的组合哈希 */
        bucket = bucket * 31 + h;
    }

    /* 取模到桶数量 */
    return abs(bucket) % idx->tables[table_idx].n_buckets;
}

/**
 * 计算探测序列
 * 生成 n_probes 个最可能包含最近邻的桶索引
 */
static int generate_probe_sequence(const lsh_multiprobe_t *idx, const float *query,
                                    probe_entry_t *probes, int n_probes)
{
    int total_probes = 0;

    /* 为每个表生成探测序列 */
    for (int t = 0; t < idx->n_tables && total_probes < n_probes; t++) {
        /* 计算主桶 */
        int main_bucket = compute_bucket_index(idx, t, query);

        /* 首先添加主桶 */
        probes[total_probes].table_idx = t;
        probes[total_probes].bucket_idx = main_bucket;
        probes[total_probes].score = 0.0f;  /* 主桶得分最高 */
        total_probes++;

        /* 计算到各哈希函数边界的距离 */
        /* 简化实现：探测相邻桶 (+1, -1, +2, -2, ...) */
        int offset = 1;
        while (total_probes < n_probes && offset <= 10) {
            /* 向上偏移 */
            int bucket_up = (main_bucket + offset) % idx->tables[t].n_buckets;
            if (bucket_up < 0) bucket_up += idx->tables[t].n_buckets;

            probes[total_probes].table_idx = t;
            probes[total_probes].bucket_idx = bucket_up;
            probes[total_probes].score = (float)offset;  /* 得分与偏移成比例 */
            total_probes++;

            if (total_probes >= n_probes) break;

            /* 向下偏移 */
            int bucket_down = (main_bucket - offset) % idx->tables[t].n_buckets;
            if (bucket_down < 0) bucket_down += idx->tables[t].n_buckets;

            probes[total_probes].table_idx = t;
            probes[total_probes].bucket_idx = bucket_down;
            probes[total_probes].score = (float)offset;
            total_probes++;

            offset++;
        }
    }

    /* 按得分排序（得分低的优先探测） */
    for (int i = 0; i < total_probes - 1; i++) {
        for (int j = i + 1; j < total_probes; j++) {
            if (probes[j].score < probes[i].score) {
                probe_entry_t tmp = probes[i];
                probes[i] = probes[j];
                probes[j] = tmp;
            }
        }
    }

    return total_probes;
}

/**
 * 计算 L2 距离
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

/* ── 公共 API 实现 ── */

lsh_multiprobe_t *lsh_multiprobe_create(int n_hash, int n_tables, int dims)
{
    if (n_hash <= 0 || n_tables <= 0 || dims <= 0) {
        return NULL;
    }

    lsh_multiprobe_t *idx = (lsh_multiprobe_t *)calloc(1, sizeof(lsh_multiprobe_t));
    if (!idx) return NULL;

    idx->n_hash = n_hash;
    idx->n_tables = n_tables;
    idx->dims = dims;
    idx->n_vectors = 0;
    idx->max_vectors = 10000;

    /* 分配哈希函数 */
    idx->hash_funcs = (hash_func_t *)calloc(n_tables * n_hash, sizeof(hash_func_t));
    if (!idx->hash_funcs) {
        free(idx);
        return NULL;
    }

    /* 分配哈希表 */
    idx->tables = (hash_table_t *)calloc(n_tables, sizeof(hash_table_t));
    if (!idx->tables) {
        free(idx->hash_funcs);
        free(idx);
        return NULL;
    }

    /* 初始化每个表 */
    int n_buckets = 1 << n_hash;  /* 2^n_hash 个桶 */
    for (int t = 0; t < n_tables; t++) {
        idx->tables[t].n_buckets = n_buckets;
        idx->tables[t].buckets = (bucket_entry_t **)calloc(n_buckets, sizeof(bucket_entry_t *));
        if (!idx->tables[t].buckets) {
            /* 回滚 */
            for (int i = 0; i < t; i++) {
                free(idx->tables[i].buckets);
            }
            free(idx->tables);
            free(idx->hash_funcs);
            free(idx);
            return NULL;
        }
    }

    /* 分配向量存储 */
    idx->vectors = (float *)malloc(idx->max_vectors * dims * sizeof(float));
    idx->vector_ids = (int *)malloc(idx->max_vectors * sizeof(int));
    if (!idx->vectors || !idx->vector_ids) {
        lsh_multiprobe_destroy(idx);
        return NULL;
    }

    return idx;
}

int lsh_multiprobe_train(lsh_multiprobe_t *idx, int n, const float *vectors)
{
    if (!idx || n <= 0 || !vectors) {
        return -1;
    }

    srand((unsigned int)time(NULL));

    /* 生成随机哈希函数 */
    for (int t = 0; t < idx->n_tables; t++) {
        for (int h = 0; h < idx->n_hash; h++) {
            hash_func_t *func = &idx->hash_funcs[t * idx->n_hash + h];

            func->a = (float *)malloc(idx->dims * sizeof(float));
            if (!func->a) return -1;

            /* 随机高斯向量 */
            generate_gaussian_vector(func->a, idx->dims);

            /* 随机偏移 [0, W) */
            func->b = ((float)rand() / RAND_MAX) * LSH_W;
        }
    }

    return 0;
}

int lsh_multiprobe_add(lsh_multiprobe_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || n <= 0 || !vectors || !ids) {
        return 0;
    }

    /* 检查是否已训练 */
    if (!idx->hash_funcs || !idx->hash_funcs[0].a) {
        return 0;  /* 未训练，不能添加 */
    }

    int added = 0;

    for (int i = 0; i < n; i++) {
        if (idx->n_vectors >= idx->max_vectors) {
            /* 扩容 */
            int new_max = idx->max_vectors * 2;
            float *new_vectors = (float *)realloc(idx->vectors, new_max * idx->dims * sizeof(float));
            int *new_ids = (int *)realloc(idx->vector_ids, new_max * sizeof(int));
            if (!new_vectors || !new_ids) break;

            idx->vectors = new_vectors;
            idx->vector_ids = new_ids;
            idx->max_vectors = new_max;
        }

        /* 存储向量 */
        int vid = idx->n_vectors;
        memcpy(&idx->vectors[vid * idx->dims], vectors + i * idx->dims, idx->dims * sizeof(float));
        idx->vector_ids[vid] = ids[i];
        idx->n_vectors++;

        /* 添加到每个哈希表 */
        for (int t = 0; t < idx->n_tables; t++) {
            int bucket = compute_bucket_index(idx, t, vectors + i * idx->dims);

            bucket_entry_t *entry = (bucket_entry_t *)malloc(sizeof(bucket_entry_t));
            if (!entry) continue;

            entry->id = ids[i];
            entry->vector = &idx->vectors[vid * idx->dims];
            entry->next = idx->tables[t].buckets[bucket];
            idx->tables[t].buckets[bucket] = entry;
        }

        added++;
    }

    return added;
}

int lsh_multiprobe_search(const lsh_multiprobe_t *idx, const float *query,
                          int k, int *ids, float *distances, int n_probes)
{
    if (!idx || !query || k <= 0 || !ids || n_probes <= 0) {
        return -1;
    }

    if (idx->n_vectors == 0) {
        return 0;
    }

    /* 生成探测序列 */
    probe_entry_t *probes = (probe_entry_t *)malloc(n_probes * sizeof(probe_entry_t));
    if (!probes) return -1;

    int total_probes = generate_probe_sequence(idx, query, probes, n_probes);

    /* 收集候选集 */
    int *candidates = (int *)malloc(LSH_MAX_CANDIDATES * sizeof(int));
    float *candidate_dists = (float *)malloc(LSH_MAX_CANDIDATES * sizeof(float));
    int n_candidates = 0;

    for (int p = 0; p < total_probes && n_candidates < LSH_MAX_CANDIDATES; p++) {
        int t = probes[p].table_idx;
        int bucket = probes[p].bucket_idx;

        bucket_entry_t *entry = idx->tables[t].buckets[bucket];
        while (entry && n_candidates < LSH_MAX_CANDIDATES) {
            /* 检查是否已存在 */
            int exists = 0;
            for (int i = 0; i < n_candidates; i++) {
                if (candidates[i] == entry->id) {
                    exists = 1;
                    break;
                }
            }

            if (!exists) {
                candidates[n_candidates] = entry->id;
                candidate_dists[n_candidates] = compute_l2_distance(query, entry->vector, idx->dims);
                n_candidates++;
            }

            entry = entry->next;
        }
    }

    /* 选择 top-k */
    int result_count = (n_candidates < k) ? n_candidates : k;

    /* 简单选择排序（小数据集） */
    for (int i = 0; i < result_count; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n_candidates; j++) {
            if (candidate_dists[j] < candidate_dists[min_idx]) {
                min_idx = j;
            }
        }

        if (min_idx != i) {
            int tmp_id = candidates[i];
            candidates[i] = candidates[min_idx];
            candidates[min_idx] = tmp_id;

            float tmp_dist = candidate_dists[i];
            candidate_dists[i] = candidate_dists[min_idx];
            candidate_dists[min_idx] = tmp_dist;
        }

        ids[i] = candidates[i];
        if (distances) distances[i] = candidate_dists[i];
    }

    free(probes);
    free(candidates);
    free(candidate_dists);

    return result_count;
}

int lsh_multiprobe_search_batch(const lsh_multiprobe_t *idx, int nq, const float *queries,
                                 int k, int *ids, float *distances, int n_probes)
{
    if (!idx || nq <= 0 || !queries || k <= 0 || !ids) {
        return -1;
    }

    for (int i = 0; i < nq; i++) {
        int result = lsh_multiprobe_search(idx, queries + i * idx->dims, k,
                                           ids + i * k,
                                           distances ? distances + i * k : NULL,
                                           n_probes);
        if (result < 0) {
            return -1;
        }
    }

    return 0;
}

void lsh_multiprobe_stats(const lsh_multiprobe_t *idx,
                           int *out_n_vectors, int *out_n_tables, int *out_dims)
{
    if (!idx) return;

    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_n_tables) *out_n_tables = idx->n_tables;
    if (out_dims) *out_dims = idx->dims;
}

int lsh_multiprobe_save(const lsh_multiprobe_t *idx, const char *path)
{
    if (!idx || !path) return -1;

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    /* 写入头部 */
    fwrite(&idx->n_hash, sizeof(int), 1, fp);
    fwrite(&idx->n_tables, sizeof(int), 1, fp);
    fwrite(&idx->dims, sizeof(int), 1, fp);
    fwrite(&idx->n_vectors, sizeof(int), 1, fp);

    /* 写入哈希函数 */
    for (int t = 0; t < idx->n_tables; t++) {
        for (int h = 0; h < idx->n_hash; h++) {
            hash_func_t *func = &idx->hash_funcs[t * idx->n_hash + h];
            fwrite(func->a, sizeof(float), idx->dims, fp);
            fwrite(&func->b, sizeof(float), 1, fp);
        }
    }

    /* 写入向量 */
    fwrite(idx->vectors, sizeof(float), idx->n_vectors * idx->dims, fp);
    fwrite(idx->vector_ids, sizeof(int), idx->n_vectors, fp);

    fclose(fp);
    return 0;
}

lsh_multiprobe_t *lsh_multiprobe_load(const char *path)
{
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    /* 读取头部 */
    int n_hash, n_tables, dims, n_vectors;
    if (fread(&n_hash, sizeof(int), 1, fp) != 1 ||
        fread(&n_tables, sizeof(int), 1, fp) != 1 ||
        fread(&dims, sizeof(int), 1, fp) != 1 ||
        fread(&n_vectors, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    /* 创建索引 */
    lsh_multiprobe_t *idx = lsh_multiprobe_create(n_hash, n_tables, dims);
    if (!idx) {
        fclose(fp);
        return NULL;
    }

    /* 读取哈希函数 */
    for (int t = 0; t < n_tables; t++) {
        for (int h = 0; h < n_hash; h++) {
            hash_func_t *func = &idx->hash_funcs[t * n_hash + h];
            func->a = (float *)malloc(dims * sizeof(float));
            if (!func->a) {
                fclose(fp);
                lsh_multiprobe_destroy(idx);
                return NULL;
            }
            fread(func->a, sizeof(float), dims, fp);
            fread(&func->b, sizeof(float), 1, fp);
        }
    }

    /* 读取向量 */
    idx->n_vectors = n_vectors;
    fread(idx->vectors, sizeof(float), n_vectors * dims, fp);
    fread(idx->vector_ids, sizeof(int), n_vectors, fp);

    /* 重建哈希表 */
    for (int i = 0; i < n_vectors; i++) {
        for (int t = 0; t < n_tables; t++) {
            int bucket = compute_bucket_index(idx, t, &idx->vectors[i * dims]);

            bucket_entry_t *entry = (bucket_entry_t *)malloc(sizeof(bucket_entry_t));
            if (!entry) continue;

            entry->id = idx->vector_ids[i];
            entry->vector = &idx->vectors[i * dims];
            entry->next = idx->tables[t].buckets[bucket];
            idx->tables[t].buckets[bucket] = entry;
        }
    }

    fclose(fp);
    return idx;
}

void lsh_multiprobe_destroy(lsh_multiprobe_t *idx)
{
    if (!idx) return;

    /* 释放哈希函数 */
    if (idx->hash_funcs) {
        for (int t = 0; t < idx->n_tables; t++) {
            for (int h = 0; h < idx->n_hash; h++) {
                free(idx->hash_funcs[t * idx->n_hash + h].a);
            }
        }
        free(idx->hash_funcs);
    }

    /* 释放哈希表 */
    if (idx->tables) {
        for (int t = 0; t < idx->n_tables; t++) {
            if (idx->tables[t].buckets) {
                for (int b = 0; b < idx->tables[t].n_buckets; b++) {
                    bucket_entry_t *entry = idx->tables[t].buckets[b];
                    while (entry) {
                        bucket_entry_t *next = entry->next;
                        free(entry);
                        entry = next;
                    }
                }
                free(idx->tables[t].buckets);
            }
        }
        free(idx->tables);
    }

    free(idx->vectors);
    free(idx->vector_ids);
    free(idx);
}