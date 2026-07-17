/*
 * lsh.c
 *
 * LSH (Locality-Sensitive Hashing) 局部敏感哈希索引实现
 *
 * 支持的 LSH 类型：
 * - LSH_BITWISE: 比特采样 LSH，适合二值向量
 * - LSH_PSTABLE: p-stable LSH，适合 L2 距离的浮点向量
 * - LSH_SIMHASH: SimHash，适合 Cosine 相似度
 *
 * 设计要点：
 * 1. 使用多表 AND-amplification 提高召回率
 * 2. OR-amplification 通过多表联合查询实现
 * 3. 汉明距离通过位运算高效计算
 * 4. Phase 2: 向量引用改造 - 向量存储在 Heap 中，索引只存引用
 */

#include <db/index/vector_index/lsh/lsh.h>
#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>
#include <algo-prod/distance/distance.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

/* ── 内部宏定义 ── */
#define LSH_W 4.0f              /* p-stable LSH 的桶宽度 */
#define LSH_MAX_CANDIDATES 1000 /* 最大候选集大小 */

/* ── 内部结构 ── */

/* p-stable LSH 哈希函数参数 */
typedef struct lsh_pstable_func {
    float *a;                   /* 随机投影向量 [dims] */
    float b;                    /* 偏移量 [0, W) */
} lsh_pstable_func_t;

/* SimHash 哈希函数参数 */
typedef struct lsh_simhash_func {
    float *v;                   /* 随机超平面向量 [dims] */
} lsh_simhash_func_t;

/* 哈希函数联合结构 */
typedef struct lsh_hash_func {
    lsh_type_t type;
    union {
        struct {
            int *indices;       /* 采样的维度索引 */
            float *thresholds;  /* 阈值 */
            int n_samples;
        } bitwise;
        lsh_pstable_func_t pstable;
        lsh_simhash_func_t simhash;
    } params;
} lsh_hash_func_t;

/* 哈希桶条目 */
typedef struct lsh_bucket_entry {
    int id;                     /* 向量 ID */
    float *vector;              /* 向量副本（用于精确距离计算） */
    int is_binary;              /* 是否为二值向量 */
    struct lsh_bucket_entry *next;
} lsh_bucket_entry_t;

/* 哈希表 */
typedef struct lsh_table {
    lsh_bucket_entry_t **buckets;  /* 桶数组 */
    int n_buckets;                  /* 桶数量 */
    int *bucket_sizes;               /* 每个桶的大小 */
    int capacity;                    /* 初始桶容量 */
} lsh_table_t;

/* LSH 索引结构 */
struct lsh_index {
    lsh_type_t type;             /* LSH 类型 */
    int n_hash;                  /* 每个表的哈希函数数 */
    int n_tables;                /* 哈希表数量 */
    int dims;                    /* 向量维度 */
    int n_vectors;                /* 已添加向量数 */
    int max_vectors;              /* 最大向量数 */
    int signature_bits;           /* 签名位数 */

    lsh_hash_func_t *hash_funcs; /* 哈希函数 [n_tables][n_hash] */
    lsh_table_t *tables;          /* 哈希表 [n_tables] */

    /* 存储向量（用于精确距离计算） */
    float *vectors;              /* 浮点向量 [max_vectors, dims] */
    uint8_t *binary_vectors;     /* 二值向量 [max_vectors, binary_size] */
    int *vector_ids;              /* 向量 ID [max_vectors] */
    int vector_size;              /* 每向量字节数 */

    /* Phase 1 基础设施：存储后端与 Heap 向量存储 */
    storage_backend_t *storage;         /**< 存储后端 */
    heap_vector_store_t *heap_store;    /**< 向量主存储 */
    vector_ref_t *vector_refs;          /**< 向量引用数组 */
    uint32_t vector_refs_size;          /**< 引用数组大小 */
    persist_control_t *persist_ctrl;    /**< 持久化控制 */
};

/* ── 静态函数声明 ── */
static uint64_t _lsh_compute_signature(const lsh_index_t *idx, const float *vec,
                                       int table_idx);
static uint64_t _lsh_compute_binary_signature(const lsh_index_t *idx,
                                              const uint8_t *vec, int table_idx);
static int _lsh_find_candidates(const lsh_index_t *idx, const float *query,
                                int *candidates, int *candidate_count);
static void _lsh_generate_random_projection(float *a, int dims);
static void _lsh_generate_random_hyperplane(float *v, int dims);

static lsh_bucket_entry_t *_lsh_bucket_entry_create(int id, const float *vec, int dims);
static lsh_bucket_entry_t *_lsh_binary_entry_create(int id, const uint8_t *vec, int size);
static void _lsh_bucket_entry_destroy(lsh_bucket_entry_t *entry);

/*
 * _lsh_store_vector - 将向量写入存储区
 *
 * Heap 模式: 调用 heap_vector_insert 写入并记录引用
 * 兼容模式: 追加到 idx->vectors 数组
 *
 * 返回 0 表示成功，非 0 表示失败。
 */
static int _lsh_store_vector(lsh_index_t *idx, const float *vector, int vec_id)
{
    if (idx == NULL || vector == NULL || vec_id < 0) {
        return -1;
    }

    if (idx->heap_store != NULL) {
        /* 扩展 vector_refs 数组 */
        uint32_t new_size = (uint32_t)(vec_id + 1);
        vector_ref_t *new_refs = (vector_ref_t *)realloc(
            idx->vector_refs,
            (size_t)new_size * sizeof(vector_ref_t));
        if (new_refs == NULL) {
            return -1;
        }
        idx->vector_refs = new_refs;
        idx->vector_refs_size = new_size;

        /* 写入 Heap 并记录引用 */
        vector_ref_t ref = heap_vector_insert(idx->heap_store, vector);
        if (!vector_ref_is_valid(&ref)) {
            return -1;
        }
        idx->vector_refs[vec_id] = ref;
        return 0;
    }

    /* 兼容路径: 直接存入 vectors 数组 */
    if (vec_id >= idx->max_vectors) {
        return -1;
    }
    memcpy(&idx->vectors[vec_id * idx->dims], vector,
           (size_t)idx->dims * sizeof(float));
    return 0;
}

/*
 * _lsh_load_vector - 从存储区读取向量
 *
 * Heap 模式: 通过 vector_refs[id] 从 Heap 获取向量
 * 兼容模式: 直接从 idx->vectors 数组读取
 *
 * 返回 0 表示成功，非 0 表示失败。
 */
static int _lsh_load_vector(const lsh_index_t *idx, int vec_id, float *out)
{
    if (idx == NULL || vec_id < 0 || out == NULL) {
        return -1;
    }

    if (idx->heap_store != NULL) {
        if ((uint32_t)vec_id >= idx->vector_refs_size) {
            return -1;
        }
        return heap_vector_get(idx->heap_store,
                               &idx->vector_refs[vec_id],
                               out);
    }

    /* 兼容路径: 直接从连续数组读取 */
    if (vec_id >= idx->n_vectors) {
        return -1;
    }
    memcpy(out, &idx->vectors[vec_id * idx->dims],
           (size_t)idx->dims * sizeof(float));
    return 0;
}

/* ── 核心 API 实现 ── */

lsh_index_t *lsh_create(lsh_type_t type, int n_hash, int n_tables, int dims)
{
    if (n_hash <= 0 || n_tables <= 0 || dims <= 0) {
        return NULL;
    }

    lsh_index_t *idx = (lsh_index_t *)calloc(1, sizeof(lsh_index_t));
    if (!idx) return NULL;

    idx->type = type;
    idx->n_hash = n_hash;
    idx->n_tables = n_tables;
    idx->dims = dims;
    idx->n_vectors = 0;
    idx->max_vectors = 100000;
    idx->signature_bits = n_hash;  /* 每表一位数 */

    /* 初始化随机种子 */
    srand((unsigned int)time(NULL));

    /* 分配哈希函数 */
    int total_funcs = n_hash * n_tables;
    idx->hash_funcs = (lsh_hash_func_t *)calloc(total_funcs, sizeof(lsh_hash_func_t));

    /* 初始化哈希函数参数 */
    for (int t = 0; t < n_tables; t++) {
        for (int h = 0; h < n_hash; h++) {
            lsh_hash_func_t *func = &idx->hash_funcs[t * n_hash + h];
            func->type = type;

            switch (type) {
                case LSH_BITWISE: {
                    func->params.bitwise.indices = (int *)malloc(n_hash * sizeof(int));
                    func->params.bitwise.thresholds = (float *)malloc(n_hash * sizeof(float));
                    func->params.bitwise.n_samples = n_hash;
                    /* 采样 n_hash 个维度 */
                    for (int i = 0; i < n_hash; i++) {
                        func->params.bitwise.indices[i] = rand() % dims;
                        func->params.bitwise.thresholds[i] = 0.5f;
                    }
                    break;
                }
                case LSH_PSTABLE:
                    func->params.pstable.a = (float *)malloc(dims * sizeof(float));
                    _lsh_generate_random_projection(func->params.pstable.a, dims);
                    func->params.pstable.b = (float)(rand() % 100) / 100.0f * LSH_W;
                    break;
                case LSH_SIMHASH:
                    func->params.simhash.v = (float *)malloc(dims * sizeof(float));
                    _lsh_generate_random_hyperplane(func->params.simhash.v, dims);
                    break;
            }
        }
    }

    /* 分配哈希表 */
    int n_buckets = 1 << 16;  /* 65536 个桶 */
    idx->tables = (lsh_table_t *)calloc(n_tables, sizeof(lsh_table_t));

    for (int t = 0; t < n_tables; t++) {
        idx->tables[t].n_buckets = n_buckets;
        idx->tables[t].buckets = (lsh_bucket_entry_t **)calloc(n_buckets, sizeof(lsh_bucket_entry_t *));
        idx->tables[t].bucket_sizes = (int *)calloc(n_buckets, sizeof(int));
        idx->tables[t].capacity = 16;
    }

    /* 分配向量存储 */
    idx->vectors = (float *)calloc(idx->max_vectors * dims, sizeof(float));
    idx->vector_ids = (int *)calloc(idx->max_vectors, sizeof(int));
    idx->vector_size = dims * sizeof(float);

    if (!idx->vectors || !idx->vector_ids) {
        lsh_destroy(idx);
        return NULL;
    }

    return idx;
}

int lsh_train(lsh_index_t *idx, int n, const float *vectors)
{
    /* LSH 通常不需要显式训练，这里仅验证参数 */
    (void)n;
    (void)vectors;
    if (!idx) return -1;

    /* 可以在这里根据数据分布调整参数，但简化起见省略 */
    return 0;
}

int lsh_add(lsh_index_t *idx, int n, const float *vectors, const int *ids)
{
    if (!idx || !vectors || !ids) return -1;

    int added = 0;
    for (int i = 0; i < n; i++) {
        /* Heap 模式: 不需要预先扩容 vectors 数组 */
        if (idx->heap_store == NULL) {
            /* 兼容路径: 检查并扩容 */
            if (idx->n_vectors >= idx->max_vectors) {
                int new_max = idx->max_vectors * 2;
                float *new_vectors = (float *)realloc(idx->vectors, (size_t)new_max * idx->dims * sizeof(float));
                int *new_ids = (int *)realloc(idx->vector_ids, (size_t)new_max * sizeof(int));

                if (!new_vectors || !new_ids) {
                    free(new_vectors);
                    free(new_ids);
                    break;
                }

                idx->vectors = new_vectors;
                idx->vector_ids = new_ids;
                idx->max_vectors = new_max;
            }
        }

        int vec_id = idx->n_vectors;

        /* 存储向量（Heap 或兼容模式） */
        if (_lsh_store_vector(idx, &vectors[i * idx->dims], vec_id) != 0) {
            break;
        }
        idx->vector_ids[vec_id] = ids[i];

        /* 插入到每个哈希表 */
        for (int t = 0; t < idx->n_tables; t++) {
            uint64_t sig = _lsh_compute_signature(idx, &vectors[i * idx->dims], t);
            int bucket_idx = sig % idx->tables[t].n_buckets;

            /* 创建桶条目（不存向量副本，只存 ID） */
            lsh_bucket_entry_t *entry = _lsh_bucket_entry_create(ids[i], NULL, 0);
            if (!entry) continue;

            /* 头插法 */
            entry->next = idx->tables[t].buckets[bucket_idx];
            idx->tables[t].buckets[bucket_idx] = entry;
            idx->tables[t].bucket_sizes[bucket_idx]++;
        }

        idx->n_vectors++;
        added++;
    }

    return added;
}

int lsh_add_binary(lsh_index_t *idx, int n, const uint8_t *vectors, const int *ids)
{
    if (!idx || !vectors || !ids) return -1;

    int binary_size = (idx->dims + 7) / 8;

    int added = 0;
    for (int i = 0; i < n; i++) {
        if (idx->n_vectors >= idx->max_vectors) break;

        int vec_id = idx->n_vectors;

        /* 存储二值向量 */
        if (!idx->binary_vectors) {
            idx->binary_vectors = (uint8_t *)calloc(idx->max_vectors * binary_size, sizeof(uint8_t));
        }
        memcpy(&idx->binary_vectors[vec_id * binary_size], &vectors[i * binary_size],
               binary_size * sizeof(uint8_t));
        idx->vector_ids[vec_id] = ids[i];

        /* 插入到哈希表 */
        for (int t = 0; t < idx->n_tables; t++) {
            uint64_t sig = _lsh_compute_binary_signature(idx, &vectors[i * binary_size], t);
            int bucket_idx = sig % idx->tables[t].n_buckets;

            lsh_bucket_entry_t *entry = _lsh_binary_entry_create(ids[i],
                &vectors[i * binary_size], binary_size);
            if (!entry) continue;

            entry->next = idx->tables[t].buckets[bucket_idx];
            idx->tables[t].buckets[bucket_idx] = entry;
            idx->tables[t].bucket_sizes[bucket_idx]++;
        }

        idx->n_vectors++;
        added++;
    }

    return added;
}

int lsh_search(const lsh_index_t *idx, const float *query, int k, int *out_ids, float *out_dists)
{
    float *temp_vec = NULL;

    if (!idx || !query || !out_ids || !out_dists) return -1;

    /* 收集候选向量 */
    int candidates[LSH_MAX_CANDIDATES];
    int candidate_count = 0;

    _lsh_find_candidates(idx, query, candidates, &candidate_count);

    if (candidate_count == 0) {
        return 0;
    }

    /* 为 Heap 模式分配临时向量缓冲区 */
    if (idx->heap_store != NULL) {
        temp_vec = (float *)malloc((size_t)idx->dims * sizeof(float));
        if (temp_vec == NULL) {
            return -1;
        }
    }

    /* 计算精确距离并排序 */
    typedef struct {
        int id;
        float dist;
    } result_t;

    result_t *results = (result_t *)malloc(candidate_count * sizeof(result_t));
    if (results == NULL) {
        free(temp_vec);
        return -1;
    }

    for (int i = 0; i < candidate_count; i++) {
        int vec_id = candidates[i];
        const float *vec;

        /* 两阶段搜索：Heap 模式从 Heap 获取，兼容模式直接读取 */
        if (idx->heap_store != NULL) {
            if (_lsh_load_vector(idx, vec_id, temp_vec) != 0) {
                /* 加载失败，跳过该候选 */
                results[i].id = vec_id;
                results[i].dist = FLT_MAX;
                continue;
            }
            vec = temp_vec;
        } else {
            vec = &idx->vectors[vec_id * idx->dims];
        }

        float dist = distance_l2sqr(vec, query, idx->dims);
        results[i].id = vec_id;
        results[i].dist = dist;
    }

    /* 简单选择排序取 top-k */
    for (int i = 0; i < k && i < candidate_count; i++) {
        int best_idx = i;
        for (int j = i + 1; j < candidate_count; j++) {
            if (results[j].dist < results[best_idx].dist) {
                best_idx = j;
            }
        }
        if (best_idx != i) {
            result_t tmp = results[i];
            results[i] = results[best_idx];
            results[best_idx] = tmp;
        }
        out_ids[i] = idx->vector_ids[results[i].id];
        out_dists[i] = results[i].dist;
    }

    free(results);
    free(temp_vec);
    return k < candidate_count ? k : candidate_count;
}

int lsh_search_batch(const lsh_index_t *idx, int nq, const float *queries,
                     int k, int *ids, float *distances)
{
    if (!idx || !queries || !ids || !distances) return -1;

    for (int i = 0; i < nq; i++) {
        lsh_search(idx, &queries[i * idx->dims], k,
                   &ids[i * k], &distances[i * k]);
    }

    return 0;
}

void lsh_stats(const lsh_index_t *idx, int *out_n_vectors, int *out_n_tables, int *out_dims)
{
    if (!idx) {
        if (out_n_vectors) *out_n_vectors = 0;
        if (out_n_tables) *out_n_tables = 0;
        if (out_dims) *out_dims = 0;
        return;
    }

    if (out_n_vectors) *out_n_vectors = idx->n_vectors;
    if (out_n_tables) *out_n_tables = idx->n_tables;
    if (out_dims) *out_dims = idx->dims;
}

int lsh_save(const lsh_index_t *idx, const char *path)
{
    /* 简化实现：保存到文件 */
    /* 后续可扩展为完整序列化 */
    (void)idx;
    (void)path;
    return -1;  /* 暂未实现 */
}

lsh_index_t *lsh_load(const char *path)
{
    (void)path;
    return NULL;  /* 暂未实现 */
}

void lsh_destroy(lsh_index_t *idx)
{
    if (!idx) return;

    /* 释放哈希函数 */
    int total_funcs = idx->n_hash * idx->n_tables;
    for (int i = 0; i < total_funcs; i++) {
        switch (idx->hash_funcs[i].type) {
            case LSH_BITWISE:
                free(idx->hash_funcs[i].params.bitwise.indices);
                free(idx->hash_funcs[i].params.bitwise.thresholds);
                break;
            case LSH_PSTABLE:
                free(idx->hash_funcs[i].params.pstable.a);
                break;
            case LSH_SIMHASH:
                free(idx->hash_funcs[i].params.simhash.v);
                break;
        }
    }
    free(idx->hash_funcs);

    /* 释放哈希表 */
    for (int t = 0; t < idx->n_tables; t++) {
        for (int b = 0; b < idx->tables[t].n_buckets; b++) {
            lsh_bucket_entry_t *entry = idx->tables[t].buckets[b];
            while (entry) {
                lsh_bucket_entry_t *next = entry->next;
                _lsh_bucket_entry_destroy(entry);
                entry = next;
            }
        }
        free(idx->tables[t].buckets);
        free(idx->tables[t].bucket_sizes);
    }
    free(idx->tables);

    /* 释放向量存储（兼容模式） */
    free(idx->vectors);
    free(idx->binary_vectors);
    free(idx->vector_ids);

    /* Heap 模式下释放引用数组，不释放 Heap 本身（由上层管理） */
    free(idx->vector_refs);

    free(idx);
}

/* ── 内部函数实现 ── */

static uint64_t _lsh_compute_signature(const lsh_index_t *idx, const float *vec, int table_idx)
{
    uint64_t sig = 0;

    for (int h = 0; h < idx->n_hash; h++) {
        lsh_hash_func_t *func = &idx->hash_funcs[table_idx * idx->n_hash + h];
        int bit = 0;

        switch (func->type) {
            case LSH_BITWISE: {
                /* 取指定维度的值与阈值比较 */
                int dim = func->params.bitwise.indices[h % func->params.bitwise.n_samples];
                float thresh = func->params.bitwise.thresholds[h % func->params.bitwise.n_samples];
                bit = (vec[dim] > thresh) ? 1 : 0;
                break;
            }
            case LSH_PSTABLE: {
                /* 计算投影 + 偏移，然后取整 */
                float proj = func->params.pstable.b;
                for (int d = 0; d < idx->dims; d++) {
                    proj += vec[d] * func->params.pstable.a[d];
                }
                bit = ((int)(proj / LSH_W)) & 1;
                break;
            }
            case LSH_SIMHASH: {
                /* 计算点积 */
                float dot = 0;
                for (int d = 0; d < idx->dims; d++) {
                    dot += vec[d] * func->params.simhash.v[d];
                }
                bit = (dot > 0) ? 1 : 0;
                break;
            }
        }

        sig = (sig << 1) | (uint64_t)bit;
    }

    return sig;
}

static uint64_t _lsh_compute_binary_signature(const lsh_index_t *idx,
                                              const uint8_t *vec, int table_idx)
{
    uint64_t sig = 0;
    (void)(idx->dims);  /* 避免未使用警告 */

    for (int h = 0; h < idx->n_hash; h++) {
        lsh_hash_func_t *func = &idx->hash_funcs[table_idx * idx->n_hash + h];
        int bit = 0;

        if (func->type == LSH_BITWISE) {
            int dim = func->params.bitwise.indices[h % func->params.bitwise.n_samples];
            int byte_idx = dim / 8;
            int bit_idx = dim % 8;
            bit = (vec[byte_idx] >> bit_idx) & 1;
        }

        sig = (sig << 1) | (uint64_t)bit;
    }

    return sig;
}

static int _lsh_find_candidates(const lsh_index_t *idx, const float *query,
                                int *candidates, int *candidate_count)
{
    *candidate_count = 0;

    /* 使用临时数组跟踪已访问的向量 */
    int *visited = (int *)calloc(idx->n_vectors, sizeof(int));
    int visited_count = 0;

    for (int t = 0; t < idx->n_tables; t++) {
        uint64_t sig = _lsh_compute_signature(idx, query, t);
        int bucket_idx = sig % idx->tables[t].n_buckets;

        lsh_bucket_entry_t *entry = idx->tables[t].buckets[bucket_idx];
        while (entry) {
            if (visited_count >= LSH_MAX_CANDIDATES) break;

            /* 检查是否已访问 */
            int is_visited = 0;
            for (int i = 0; i < visited_count; i++) {
                if (visited[i] == entry->id) {
                    is_visited = 1;
                    break;
                }
            }

            if (!is_visited) {
                candidates[visited_count++] = entry->id;
                visited[visited_count - 1] = entry->id;
            }

            entry = entry->next;
        }

        if (visited_count >= LSH_MAX_CANDIDATES) break;
    }

    free(visited);
    *candidate_count = visited_count;

    return 0;
}


static void _lsh_generate_random_projection(float *a, int dims)
{
    /* 生成正态分布的随机向量（简化：使用均匀分布近似） */
    for (int i = 0; i < dims; i++) {
        a[i] = (float)(rand() % 1000 - 500) / 500.0f;
    }
}

static void _lsh_generate_random_hyperplane(float *v, int dims)
{
    /* 生成随机超平面向量 */
    for (int i = 0; i < dims; i++) {
        v[i] = (float)(rand() % 1000 - 500) / 500.0f;
    }

    /* 归一化 */
    float norm = 0;
    for (int i = 0; i < dims; i++) {
        norm += v[i] * v[i];
    }
    norm = sqrtf(norm);
    if (norm > 0) {
        for (int i = 0; i < dims; i++) {
            v[i] /= norm;
        }
    }
}

static lsh_bucket_entry_t *_lsh_bucket_entry_create(int id, const float *vec, int dims)
{
    lsh_bucket_entry_t *entry = (lsh_bucket_entry_t *)malloc(sizeof(lsh_bucket_entry_t));
    if (!entry) return NULL;

    entry->id = id;

    /* Phase 2: 向量引用改造后，桶条目不再存储向量副本 */
    /* 若 dims == 0 或 vec == NULL，表示使用 Heap 模式，向量通过引用获取 */
    if (vec != NULL && dims > 0) {
        entry->vector = (float *)malloc((size_t)dims * sizeof(float));
        if (!entry->vector) {
            free(entry);
            return NULL;
        }
        memcpy(entry->vector, vec, (size_t)dims * sizeof(float));
        entry->is_binary = 0;
    } else {
        entry->vector = NULL;
        entry->is_binary = 0;
    }

    entry->next = NULL;
    return entry;
}

static lsh_bucket_entry_t *_lsh_binary_entry_create(int id, const uint8_t *vec, int size)
{
    (void)size;
    lsh_bucket_entry_t *entry = (lsh_bucket_entry_t *)malloc(sizeof(lsh_bucket_entry_t));
    if (!entry) return NULL;

    entry->id = id;
    entry->vector = (float *)vec;  /* 强制转换，调用者需注意生命周期 */
    entry->is_binary = 1;
    entry->next = NULL;

    return entry;
}

static void _lsh_bucket_entry_destroy(lsh_bucket_entry_t *entry)
{
    if (!entry) return;
    if (!entry->is_binary) {
        free(entry->vector);
    }
    free(entry);
}
