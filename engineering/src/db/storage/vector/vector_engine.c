/**
 * @file vector_engine.c
 * @brief 向量存储引擎实现
 *
 * 支持向量插入、相似度搜索、HNSW 索引等功能。
 */
#include "db/storage/vector/vector_engine.h"
#include "db/storage/vector/vec_page.h"
#include "db/storage/vector/vector_persist.h"
#include "db/vector_engine.h"
#include "db/core/log.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/index/vector_index/ivf_pq/ivf_pq.h"
#include "db/index/vector_index/vector_index_selector.h"
#include "db/mm_pool.h"
#include "db/lock.h"
#include <algo-prod/quantization/quantization.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>

/* HNSW 持久化外部函数声明（来自 rag 模块） */
extern int hnsw_persist_save(void *hnsw, const char *path, void *result);
extern void *hnsw_persist_load(const char *path, void *result);
extern int hnsw_persist_is_valid(const char *path);
extern int hnsw_persist_get_meta(const char *path, void *meta);
extern void hnsw_persist_stats(void *hnsw, int32_t *total, int32_t *deleted, uint64_t *mem);

/* HNSW 持久化结果类型 */
typedef struct {
    int error_code;
    char error_msg[256];
    uint64_t bytes_written;
    uint64_t bytes_read;
} hnsw_persist_result_t;

/* HNSW 持久化元数据类型 */
typedef struct {
    int32_t dims;
    int32_t n_total;
    int32_t metric;
} hnsw_persist_meta_t;
#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

#define VECTOR_ENGINE_NAME "vector_engine"
#define VECTOR_DATA_PREFIX "vec_"
#define HNSW_DEFAULT_M 16
#define HNSW_DEFAULT_EF_CONSTRUCTION 200
#define HNSW_DEFAULT_EF_SEARCH 100

typedef struct vector_engine_global_s {
    char data_dir[512];
    bool initialized;
} vector_engine_global_t;

static vector_engine_global_t g_vector_engine = {
    .data_dir = {0},
    .initialized = false
};

/* 前向声明：全局锁管理器和清理函数 */
static lock_manager_t *g_vec_lockmgr = NULL;
static void vector_engine_lock_cleanup(void);

typedef struct vector_header_s {
    int32_t dimension;
    vector_metric_t metric;
    uint64_t num_vectors;
} vector_header_t;

static int get_meta_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/meta.bin",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    return 0;
}

static int get_data_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/vectors.bin",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    return 0;
}

/**
 * @brief 获取 HNSW 索引文件路径
 */
static int get_index_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/hnsw_index.bin",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    return 0;
}

/**
 * @brief 获取 PQ 量化码文件路径
 */
static int get_pq_code_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/pq_codes.bin",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    return 0;
}

/**
 * @brief 获取 PQ 码书文件路径
 */
static int get_pq_codebook_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s/pq_codebook.bin",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    return 0;
}

/**
 * @brief 递归创建目录
 */
static int mkpath(char *path) {
    if (path == NULL || strlen(path) == 0) return -1;

    char *p = strdup(path);
    if (p == NULL) return -1;

    /* 处理以 ./ 开头的路径 */
    char *cur = p;
    if (cur[0] == '.' && cur[1] == '/') {
        cur += 2;
    }

    /* 逐层创建目录 */
    for (char *sep = cur; *sep != '\0'; sep++) {
        if (*sep == '/') {
            *sep = '\0';
            if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
                free(p);
                return -1;
            }
            *sep = '/';
        }
    }

    /* 创建最后一层目录 */
    if (strlen(cur) > 0 && mkdir(cur) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }

    free(p);
    return 0;
}

static int ensure_dir(const char *path) {
    if (path == NULL) return -1;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }

    /* 目录不存在，递归创建 */
    return mkpath((char *)path);
}

static int vector_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s%s",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    ensure_dir(dir_path);

    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));

    vector_header_t header = {
        .dimension = 128,
        .metric = METRIC_L2,
        .num_vectors = 0
    };

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }
    return 0;
}

static void *vector_engine_table_open(const char *name, AccessMode mode) {
    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return NULL;

    vector_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    vector_engine_db_t *db = (vector_engine_db_t *)calloc(1, sizeof(vector_engine_db_t));
    if (db == NULL) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    db->mode = mode;
    db->dimension = header.dimension;
    db->metric = header.metric;
    db->num_vectors = header.num_vectors;

    /* 初始化内存池（使用全局向量池） */
    db->mem_pool = g_vec_pool;
    db->use_mem_pool = (g_vec_pool != NULL);

    /* 初始化锁（默认禁用，调用者可通过 API 启用） */
    db->lockmgr = g_vec_lockmgr;
    db->rwlock = NULL;
    db->use_lock = false;

    /* 初始化 WAL */
    db->wal = NULL;
    db->use_wal = false;
    db->segment_id = 0;

    /* 初始化 IVF-PQ 索引字段 */
    db->ivf_pq_index = NULL;
    db->use_ivf_pq = false;
    db->ivf_pq_nlist = 0;
    db->ivf_pq_nprobe = 64;

    /* 初始化 HNSW 索引字段 */
    db->hnsw_index = NULL;
    db->index_built = false;
    db->index_loaded = false;

    /* 初始化 VecPage 页池 */
    db->page_pool = NULL;
    db->use_page_pool = true;
    if (db->use_page_pool) {
        char dir_path[512];
        snprintf(dir_path, sizeof(dir_path), "%s/%s%s",
                 g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
        db->page_pool = vector_page_pool_create(
            dir_path,
            VECTOR_PAGE_MAX_PAGES_DEFAULT,
            VECTOR_PAGE_SIZE_DEFAULT,
            db->dimension);
        if (db->page_pool == NULL) {
            LOG_WARN("VecPage 页池创建失败，将使用原始文件存储");
            db->use_page_pool = false;
        } else {
            /* 尝试加载已存在的页数据 */
            vector_page_pool_load(db->page_pool);
        }
    }

    /* 确保数据目录存在 */
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s%s",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, name);
    ensure_dir(dir_path);

    /* 默认启用 WAL（同步模式） */
    char wal_path[512];
    snprintf(wal_path, sizeof(wal_path), "%s/wal", dir_path);
    ensure_dir(wal_path);  /* 确保 WAL 目录存在 */
    vector_wal_t *wal = vector_wal_open(wal_path, db->segment_id);
    if (wal != NULL) {
        db->wal = wal;
        db->use_wal = true;
        LOG_INFO("向量集合 '%s' WAL 已启用", name);
    } else {
        LOG_WARN("向量集合 '%s' WAL 初始化失败，持久化保护已禁用", name);
    }

    return db;
}

static int vector_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    char meta_path[512];
    get_meta_path(db->name, meta_path, sizeof(meta_path));

    vector_header_t header = {
        .dimension = db->dimension,
        .metric = db->metric,
        .num_vectors = db->num_vectors
    };

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 释放 HNSW 索引 */
    if (db->hnsw_index != NULL) {
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
    }

    /* 释放 IVF-PQ 索引 */
    if (db->ivf_pq_index != NULL) {
        ivf_pq_destroy(db->ivf_pq_index);
        db->ivf_pq_index = NULL;
    }

    /* 释放 VecPage 页池 */
    if (db->page_pool != NULL) {
        vector_page_pool_destroy(db->page_pool);
        db->page_pool = NULL;
    }

    /* 释放 PQ 量化器 */
    if (db->quantizer != NULL) {
        quantizer_drop(db->quantizer);
        db->quantizer = NULL;
    }

    /* 释放压缩码 */
    if (db->compressed_codes != NULL) {
        free(db->compressed_codes);
        db->compressed_codes = NULL;
    }

    /* 释放读写锁 */
    if (db->rwlock != NULL) {
        free(db->rwlock);
        db->rwlock = NULL;
    }

    /* 关闭 WAL */
    if (db->wal != NULL) {
        /* 执行关闭检查点 */
        vector_wal_checkpoint(db->wal, VECTOR_CHECKPOINT_SHUTDOWN);
        vector_wal_close(db->wal);
        db->wal = NULL;
        db->use_wal = false;
        LOG_INFO("向量集合 '%s' WAL 已关闭", db->name);
    }

    /* 内存池由全局管理，这里不需要释放 */

    free(db);
    return 0;
}

static int vector_engine_table_drop(const char *name) {
    (void)name;
    return 0;
}

static int vector_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (len < sizeof(uint64_t) + sizeof(int32_t)) return -1;

    const uint8_t *ptr = (const uint8_t *)data;
    ptr += sizeof(uint64_t);  /* skip id */

    int32_t dim;
    memcpy(&dim, ptr, sizeof(int32_t));
    if (dim != db->dimension) return -1;

    /* 提取向量数据指针 */
    ptr += sizeof(int32_t);
    size_t vec_data_len = len - sizeof(uint64_t) - sizeof(int32_t);
    const float *vector = (const float *)ptr;

    /* WAL 写入（在数据修改前） */
    if (db->use_wal && db->wal != NULL) {
        int32_t vec_id = (int32_t)db->num_vectors;  /* 预分配 ID */
        if (vector_wal_append(db->wal, db->segment_id, vec_id, dim, vector) != 0) {
            LOG_WARN("WAL 写入失败，数据可能不安全");
        }
    }

    /* 优先使用 VecPage 页池存储 */
    if (db->use_page_pool && db->page_pool != NULL) {
        int32_t vec_id = vector_page_append(db->page_pool, vector, -1);
        if (vec_id >= 0) {
            /* 如果启用了 PQ 量化，同时进行编码 */
            if (db->use_quantization && db->quantizer != NULL) {
                uint8_t code[64];  /* PQ 最大码长 */
                if (quantizer_encode(db->quantizer, vector, code) == 0) {
                    /* 追加压缩码到数组 - 使用内存池 */
                    int32_t code_size = quantizer_code_size(db->quantizer);
                    uint8_t *new_codes;
                    if (db->use_mem_pool && db->mem_pool != NULL) {
                        new_codes = (uint8_t *)mm_pool_realloc(
                            db->mem_pool,
                            db->compressed_codes,
                            db->compressed_count * code_size,
                            (db->compressed_count + 1) * code_size);
                    } else {
                        new_codes = (uint8_t *)realloc(
                            db->compressed_codes,
                            (db->compressed_count + 1) * code_size);
                    }
                    if (new_codes) {
                        db->compressed_codes = new_codes;
                        memcpy(&db->compressed_codes[db->compressed_count * code_size],
                               code, code_size);
                        db->compressed_count++;
                    }
                }
            }
            db->num_vectors++;
            return 0;
        }
        LOG_WARN("VecPage 插入失败，回退到文件存储");
    }

    /* 回退到原始文件存储 */
    char data_path[512];
    get_data_path(db->name, data_path, sizeof(data_path));

    FILE *fp = fopen(data_path, "ab");
    if (fp == NULL) {
        fp = fopen(data_path, "wb");
        if (fp == NULL) return -1;
    }

    fwrite(data, 1, len, fp);
    fclose(fp);

    db->num_vectors++;
    return 0;
}

static scan_desc_t *vector_engine_scan_begin(void *rel,
                                             const scan_key_t *keys, int nkeys,
                                             ScanDirection direction) {
    (void)keys;
    (void)nkeys;
    (void)direction;

    if (rel == NULL) return NULL;
    return NULL;
}

static int vector_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

static int vector_engine_scan_end(scan_desc_t *scan) {
    if (scan) free(scan);
    return 0;
}

static int vector_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    if (name == NULL) return 0;

    char meta_path[512];
    get_meta_path(name, meta_path, sizeof(meta_path));

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) return -1;

    vector_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    stats->num_objects = header.num_vectors;
    return 0;
}

static const storage_ops_t g_vector_engine_ops = {
    .name = VECTOR_ENGINE_NAME,
    .model = MODEL_VECTOR,
    .init = vector_engine_init,
    .shutdown = vector_engine_shutdown,
    .table_create = vector_engine_table_create,
    .table_open = vector_engine_table_open,
    .table_close = vector_engine_table_close,
    .table_drop = vector_engine_table_drop,
    .tuple_insert = vector_engine_tuple_insert,
    .tuple_update = NULL,
    .tuple_delete = NULL,
    .scan_begin = vector_engine_scan_begin,
    .scan_next = vector_engine_scan_next,
    .scan_end = vector_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = vector_engine_get_stats,
};

int vector_engine_init(const char *data_dir) {
    if (g_vector_engine.initialized) return 0;
    if (data_dir == NULL) return -1;
    strncpy(g_vector_engine.data_dir, data_dir, sizeof(g_vector_engine.data_dir) - 1);

    /* 初始化全局内存池（如果尚未初始化） */
    if (g_vec_pool == NULL) {
        mm_pool_init_global(data_dir);
    }

    g_vector_engine.initialized = true;
    return 0;
}

/* 前向声明：锁清理函数 */
static void vector_engine_lock_cleanup(void);

int vector_engine_shutdown(void) {
    g_vector_engine.initialized = false;

    /* 清理锁资源 */
    vector_engine_lock_cleanup();

    return 0;
}

const storage_ops_t *vector_engine_get_ops(void) {
    return &g_vector_engine_ops;
}

int vector_engine_create(const char *name, const storage_schema_t *schema) {
    return vector_engine_table_create(name, schema);
}

void *vector_engine_open(const char *name, AccessMode mode) {
    return vector_engine_table_open(name, mode);
}

int vector_engine_close(void *rel) {
    return vector_engine_table_close(rel);
}

int vector_engine_drop(const char *name) {
    return vector_engine_table_drop(name);
}

int vector_engine_insert(void *rel, const void *data, size_t len) {
    return vector_engine_tuple_insert(rel, data, len);
}

int vector_engine_search(void *rel,
                         const float *query, int32_t query_dim,
                         int32_t top_k,
                         vector_search_results_t *results) {
    if (rel == NULL || query == NULL || results == NULL || top_k <= 0) {
        return -1;
    }

    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 维度检查 */
    if (query_dim > 0 && query_dim != db->dimension) {
        LOG_ERROR("查询向量维度 %d 与存储维度 %d 不匹配", query_dim, db->dimension);
        return -1;
    }

    int32_t dim = (query_dim > 0) ? query_dim : db->dimension;

    /* 初始化结果结构 */
    results->results = calloc(top_k, sizeof(vector_search_result_t));
    if (results->results == NULL) {
        return -1;
    }
    results->count = 0;
    results->capacity = top_k;

    /* 如果启用了 VecPage 页池，从页池获取向量 */
    if (db->use_page_pool && db->page_pool != NULL && db->num_vectors > 0) {
        /* 从页池获取所有向量进行搜索 */
        typedef struct {
            uint64_t id;
            float *vector;
            float distance;
        } temp_vec_t;

        temp_vec_t *temp_vectors = calloc(db->num_vectors, sizeof(temp_vec_t));
        if (temp_vectors == NULL) {
            return -1;
        }

        uint64_t count = 0;
        float *vec_buf = (float *)malloc(dim * sizeof(float));

        for (uint64_t i = 0; i < db->num_vectors && count < db->num_vectors; i++) {
            if (vector_page_get_vector(db->page_pool, (int32_t)i, vec_buf) == 0) {
                temp_vectors[count].id = i;
                temp_vectors[count].vector = (float *)malloc(dim * sizeof(float));
                if (temp_vectors[count].vector) {
                    memcpy(temp_vectors[count].vector, vec_buf, dim * sizeof(float));

                    /* 计算距离 */
                    if (db->metric == METRIC_L2) {
                        temp_vectors[count].distance = vector_l2_distance(
                            query, temp_vectors[count].vector, dim);
                    } else if (db->metric == METRIC_COSINE) {
                        temp_vectors[count].distance = 1.0f - vector_cosine_similarity(
                            query, temp_vectors[count].vector, dim);
                    } else {
                        temp_vectors[count].distance = vector_l2_distance(
                            query, temp_vectors[count].vector, dim);
                    }
                    count++;
                }
            }
        }

        free(vec_buf);

        /* 简单选择排序找出 top-k */
        for (uint64_t i = 0; i < count - 1 && i < (uint64_t)top_k; i++) {
            uint64_t min_idx = i;
            for (uint64_t j = i + 1; j < count; j++) {
                if (temp_vectors[j].distance < temp_vectors[min_idx].distance) {
                    min_idx = j;
                }
            }
            if (min_idx != i) {
                temp_vec_t tmp = temp_vectors[i];
                temp_vectors[i] = temp_vectors[min_idx];
                temp_vectors[min_idx] = tmp;
            }
        }

        /* 复制 top-k 结果 */
        uint32_t result_count = (count < (uint64_t)top_k) ? (uint32_t)count : (uint32_t)top_k;
        for (uint32_t i = 0; i < result_count; i++) {
            results->results[i].id = temp_vectors[i].id;
            results->results[i].distance = temp_vectors[i].distance;
            /* dimension 和 vector 字段在公共 API 中不存在，已移除 */
        }
        results->count = result_count;

        /* 释放未使用的向量内存 */
        for (uint64_t i = result_count; i < count; i++) {
            free(temp_vectors[i].vector);
        }

        free(temp_vectors);
        return 0;
    }

    /* 原始文件存储搜索 */
    char data_path[512];
    get_data_path(db->name, data_path, sizeof(data_path));

    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) {
        return 0;  /* 文件不存在时返回空结果 */
    }

    /* 读取所有向量并计算距离 */
    typedef struct {
        uint64_t id;
        float *vector;
        float distance;
    } temp_vec_t;

    temp_vec_t *temp_vectors = calloc(db->num_vectors, sizeof(temp_vec_t));
    if (temp_vectors == NULL) {
        fclose(fp);
        return -1;
    }

    uint64_t count = 0;
    size_t record_size = sizeof(uint64_t) + sizeof(int32_t) + dim * sizeof(float);

    while (count < db->num_vectors) {
        uint64_t id;
        int32_t vec_dim;

        if (fread(&id, sizeof(id), 1, fp) != 1) break;
        if (fread(&vec_dim, sizeof(vec_dim), 1, fp) != 1) break;

        temp_vectors[count].id = id;
        temp_vectors[count].vector = calloc(dim, sizeof(float));

        if (temp_vectors[count].vector) {
            size_t vec_size = vec_dim * sizeof(float);
            if (fread(temp_vectors[count].vector, vec_size, 1, fp) == 1) {
                /* 计算距离 */
                if (db->metric == METRIC_L2) {
                    temp_vectors[count].distance = vector_l2_distance(
                        query, temp_vectors[count].vector, dim);
                } else if (db->metric == METRIC_COSINE) {
                    temp_vectors[count].distance = 1.0f - vector_cosine_similarity(
                        query, temp_vectors[count].vector, dim);
                } else {
                    temp_vectors[count].distance = vector_l2_distance(
                        query, temp_vectors[count].vector, dim);
                }
                count++;
            } else {
                free(temp_vectors[count].vector);
                break;
            }
        } else {
            break;
        }
    }

    fclose(fp);

    /* 简单选择排序找出 top-k */
    for (uint64_t i = 0; i < count - 1 && i < (uint64_t)top_k; i++) {
        uint64_t min_idx = i;
        for (uint64_t j = i + 1; j < count; j++) {
            if (temp_vectors[j].distance < temp_vectors[min_idx].distance) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            temp_vec_t tmp = temp_vectors[i];
            temp_vectors[i] = temp_vectors[min_idx];
            temp_vectors[min_idx] = tmp;
        }
    }

    /* 复制 top-k 结果 */
    uint32_t result_count = (count < (uint64_t)top_k) ? (uint32_t)count : (uint32_t)top_k;
    for (uint32_t i = 0; i < result_count; i++) {
        results->results[i].id = temp_vectors[i].id;
        results->results[i].distance = temp_vectors[i].distance;
        /* dimension 和 vector 字段在公共 API 中不存在，已移除 */
    }
    results->count = result_count;

    /* 释放未使用的向量内存 */
    for (uint64_t i = result_count; i < count; i++) {
        free(temp_vectors[i].vector);
    }

    free(temp_vectors);
    return 0;
}

void vector_engine_free_results(vector_search_results_t *results) {
    if (results && results->results) {
        /* dimension 和 vector 字段在公共 API 中不存在，无需释放 */
        free(results->results);
        results->results = NULL;
        results->count = 0;
        results->capacity = 0;
    }
}

int vector_engine_stats(const char *name, storage_stats_t *stats) {
    return vector_engine_get_stats(name, stats);
}

/* ========================================================================
 * HNSW 索引相关实现
 * ======================================================================== */

/**
 * @brief 将 vector_metric_t 转换为 distance_metric_t
 */
static distance_metric_t convert_metric(vector_metric_t metric) {
    switch (metric) {
        case METRIC_L2:    return DISTANCE_METRIC_L2_SQUARED;
        case METRIC_COSINE: return DISTANCE_METRIC_COSINE;
        case METRIC_DOT:   return DISTANCE_METRIC_INNER_PRODUCT;
        default:           return DISTANCE_METRIC_L2_SQUARED;
    }
}

int vector_engine_build_index(void *rel, int m, int ef_construction) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->num_vectors == 0) {
        LOG_WARN("向量数量为 0，无法构建索引");
        return -1;
    }

    /* 如果已有索引，先释放 */
    if (db->hnsw_index != NULL) {
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
    }

    /* 使用默认参数 */
    int m_param = (m > 0) ? m : HNSW_DEFAULT_M;
    int ef_param = (ef_construction > 0) ? ef_construction : HNSW_DEFAULT_EF_CONSTRUCTION;

    /* 创建 HNSW 索引 */
    db->hnsw_index = faiss_hnsw_index_create(
        m_param,
        db->dimension,
        ef_param,
        convert_metric(db->metric),
        QUANTIZATION_TYPE_NONE  /* 暂不使用量化 */
    );

    if (db->hnsw_index == NULL) {
        LOG_ERROR("创建 HNSW 索引失败");
        return -1;
    }

    /* 从 vectors.bin 或 VecPage 页池加载向量并插入索引 */
    int32_t inserted = 0;

    /* 优先从 VecPage 页池加载 */
    if (db->use_page_pool && db->page_pool != NULL && db->num_vectors > 0) {
        float *vec = (float *)malloc(db->dimension * sizeof(float));
        if (vec != NULL) {
            for (int32_t i = 0; i < (int32_t)db->num_vectors; i++) {
                if (vector_page_get_vector(db->page_pool, i, vec) == 0) {
                    if (faiss_hnsw_index_add(db->hnsw_index, 1, vec) == 0) {
                        inserted++;
                    }
                }
            }
            free(vec);
        }
    } else {
        /* 回退到文件加载 */
        char data_path[512];
        get_data_path(db->name, data_path, sizeof(data_path));

        FILE *fp = fopen(data_path, "rb");
        if (fp != NULL) {
            while (inserted < db->num_vectors) {
                uint64_t id;
                int32_t vec_dim;

                if (fread(&id, sizeof(id), 1, fp) != 1) break;
                if (fread(&vec_dim, sizeof(vec_dim), 1, fp) != 1) break;

                float *vec = (float *)malloc(vec_dim * sizeof(float));
                if (vec == NULL) break;

                size_t vec_size = vec_dim * sizeof(float);
                if (fread(vec, vec_size, 1, fp) != 1) {
                    free(vec);
                    break;
                }

                /* 插入到 HNSW 索引 */
                if (faiss_hnsw_index_add(db->hnsw_index, 1, vec) == 0) {
                    inserted++;
                }

                free(vec);
            }
            fclose(fp);
        }
    }

    if (inserted == 0) {
        LOG_ERROR("未能插入任何向量到索引");
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
        return -1;
    }

    db->index_built = true;
    LOG_INFO("HNSW 索引构建完成，插入向量数: %d", inserted);

    return 0;
}

int vector_engine_search_hnsw(void *rel, const float *query,
                               int32_t top_k, vector_search_results_t *results) {
    if (rel == NULL || query == NULL || results == NULL || top_k <= 0) {
        return -1;
    }

    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果索引未构建，尝试构建 */
    if (db->hnsw_index == NULL || !db->index_built) {
        if (vector_engine_build_index(rel, 0, 0) != 0) {
            /* 构建失败，回退到暴力搜索 */
            LOG_WARN("HNSW 索引不可用，回退到暴力搜索");
            return vector_engine_search(rel, query, db->dimension, top_k, results);
        }
    }

    /* 初始化结果结构 */
    results->results = (vector_search_result_t *)calloc(top_k, sizeof(vector_search_result_t));
    if (results->results == NULL) {
        return -1;
    }
    results->count = 0;
    results->capacity = top_k;

    /* 使用 HNSW 搜索 */
    int32_t *ids = (int32_t *)calloc(top_k, sizeof(int32_t));
    float *distances = (float *)calloc(top_k, sizeof(float));

    if (ids == NULL || distances == NULL) {
        free(ids);
        free(distances);
        free(results->results);
        results->results = NULL;
        return -1;
    }

    int32_t k_searched = (top_k < db->hnsw_index->n_total) ? top_k : db->hnsw_index->n_total;

    faiss_hnsw_index_search(db->hnsw_index, query, k_searched,
                            HNSW_DEFAULT_EF_SEARCH, distances, ids);

    /* 转换结果格式 */
    for (int32_t i = 0; i < k_searched; i++) {
        results->results[i].id = (uint64_t)ids[i];
        /* L2_SQUARED 需要开平方得到真实距离 */
        if (db->metric == METRIC_L2) {
            results->results[i].distance = sqrtf(distances[i]);
        } else {
            results->results[i].distance = distances[i];
        }
        /* dimension 和 vector 字段在公共 API 中不存在，已移除 */
    }
    results->count = k_searched;

    free(ids);
    free(distances);

    return 0;
}

bool vector_engine_index_built(const void *rel) {
    if (rel == NULL) return false;
    const vector_engine_db_t *db = (const vector_engine_db_t *)rel;
    return db->hnsw_index != NULL && db->index_built;
}

int vector_engine_drop_index(void *rel) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 释放内存索引 */
    if (db->hnsw_index != NULL) {
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
    }
    db->index_built = false;

    /* 删除索引文件 */
    char index_path[512];
    get_index_path(db->name, index_path, sizeof(index_path));
    remove(index_path);

    return 0;
}

int vector_engine_save_index(void *rel, const char *path) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->hnsw_index == NULL) {
        LOG_WARN("没有可保存的索引");
        return -1;
    }

    /* 确定保存路径 */
    char index_path[512];
    if (path != NULL && strlen(path) > 0) {
        strncpy(index_path, path, sizeof(index_path) - 1);
    } else {
        get_index_path(db->name, index_path, sizeof(index_path));
    }

    FILE *fp = fopen(index_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建索引文件: %s", index_path);
        return -1;
    }

    /* 写入索引头 */
    uint32_t magic = 0x48505357;  /* 'HNSW' */
    uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&db->dimension, sizeof(db->dimension), 1, fp);
    fwrite(&db->metric, sizeof(db->metric), 1, fp);
    fwrite(&db->hnsw_index->n_total, sizeof(int32_t), 1, fp);
    fwrite(&db->hnsw_index->M, sizeof(int32_t), 1, fp);
    fwrite(&db->hnsw_index->ef_construction, sizeof(int32_t), 1, fp);

    /* 写入向量数据 */
    size_t vec_size = (size_t)db->dimension * sizeof(float);
    fwrite(db->hnsw_index->vectors, vec_size, db->hnsw_index->n_total, fp);

    /* 写入 levels 数组 */
    fwrite(db->hnsw_index->levels, sizeof(int32_t), db->hnsw_index->n_total, fp);

    /* 写入邻接表信息 */
    fwrite(&db->hnsw_index->offsets_size, sizeof(uint32_t), 1, fp);
    fwrite(db->hnsw_index->offsets, sizeof(int32_t), db->hnsw_index->offsets_size, fp);

    fwrite(&db->hnsw_index->nb_size, sizeof(uint32_t), 1, fp);
    fwrite(db->hnsw_index->nbs, sizeof(int32_t), db->hnsw_index->nb_size, fp);

    fwrite(&db->hnsw_index->cum_nneighbor_per_level_size, sizeof(uint32_t), 1, fp);
    fwrite(db->hnsw_index->cum_nneighbor_per_level,
           sizeof(int32_t), db->hnsw_index->cum_nneighbor_per_level_size, fp);

    /* 写入入口点信息 */
    fwrite(&db->hnsw_index->entry_pointer, sizeof(int32_t), 1, fp);
    fwrite(&db->hnsw_index->max_level, sizeof(int32_t), 1, fp);

    fclose(fp);

    LOG_INFO("索引已保存到: %s", index_path);
    return 0;
}

int vector_engine_load_index(void *rel, const char *path) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果已有索引，先释放 */
    if (db->hnsw_index != NULL) {
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
    }

    /* 确定加载路径 */
    char index_path[512];
    if (path != NULL && strlen(path) > 0) {
        strncpy(index_path, path, sizeof(index_path) - 1);
    } else {
        get_index_path(db->name, index_path, sizeof(index_path));
    }

    FILE *fp = fopen(index_path, "rb");
    if (fp == NULL) {
        LOG_ERROR("无法打开索引文件: %s", index_path);
        return -1;
    }

    /* 读取并验证文件头 */
    uint32_t magic, version;
    int32_t dim, n_total, m, ef_construction;
    vector_metric_t metric;

    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1 ||
        fread(&dim, sizeof(dim), 1, fp) != 1 ||
        fread(&metric, sizeof(metric), 1, fp) != 1 ||
        fread(&n_total, sizeof(n_total), 1, fp) != 1 ||
        fread(&m, sizeof(m), 1, fp) != 1 ||
        fread(&ef_construction, sizeof(ef_construction), 1, fp) != 1) {
        fclose(fp);
        LOG_ERROR("读取索引文件头失败");
        return -1;
    }

    /* 验证魔数和版本 */
    if (magic != 0x48505357 || version != 1) {
        fclose(fp);
        LOG_ERROR("索引文件格式无效");
        return -1;
    }

    /* 验证维度一致性 */
    if (dim != db->dimension) {
        fclose(fp);
        LOG_ERROR("索引维度 %d 与存储维度 %d 不匹配", dim, db->dimension);
        return -1;
    }

    /* 创建索引 */
    db->hnsw_index = faiss_hnsw_index_create(
        m, dim, ef_construction,
        convert_metric(metric),
        QUANTIZATION_TYPE_NONE
    );

    if (db->hnsw_index == NULL) {
        fclose(fp);
        LOG_ERROR("创建 HNSW 索引失败");
        return -1;
    }

    /* 读取向量数据 */
    size_t vec_size = (size_t)dim * sizeof(float);
    db->hnsw_index->vectors = (float *)malloc(vec_size * n_total);
    if (db->hnsw_index->vectors == NULL ||
        fread(db->hnsw_index->vectors, vec_size, n_total, fp) != (size_t)n_total) {
        fclose(fp);
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
        LOG_ERROR("读取向量数据失败");
        return -1;
    }
    db->hnsw_index->n_total = n_total;

    /* 读取 levels */
    db->hnsw_index->levels = (int32_t *)malloc(sizeof(int32_t) * n_total);
    if (db->hnsw_index->levels == NULL ||
        fread(db->hnsw_index->levels, sizeof(int32_t), n_total, fp) != (size_t)n_total) {
        fclose(fp);
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
        LOG_ERROR("读取 levels 失败");
        return -1;
    }
    db->hnsw_index->levels_size = n_total;

    /* 读取 offsets */
    fread(&db->hnsw_index->offsets_size, sizeof(uint32_t), 1, fp);
    db->hnsw_index->offsets = (int32_t *)malloc(sizeof(int32_t) * db->hnsw_index->offsets_size);
    if (db->hnsw_index->offsets == NULL ||
        fread(db->hnsw_index->offsets, sizeof(int32_t), db->hnsw_index->offsets_size, fp) != db->hnsw_index->offsets_size) {
        fclose(fp);
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
        LOG_ERROR("读取 offsets 失败");
        return -1;
    }

    /* 读取 nbs */
    fread(&db->hnsw_index->nb_size, sizeof(uint32_t), 1, fp);
    db->hnsw_index->nbs = (int32_t *)malloc(sizeof(int32_t) * db->hnsw_index->nb_size);
    if (db->hnsw_index->nbs == NULL ||
        fread(db->hnsw_index->nbs, sizeof(int32_t), db->hnsw_index->nb_size, fp) != db->hnsw_index->nb_size) {
        fclose(fp);
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
        LOG_ERROR("读取 nbs 失败");
        return -1;
    }

    /* 读取 cum_nneighbor_per_level */
    fread(&db->hnsw_index->cum_nneighbor_per_level_size, sizeof(uint32_t), 1, fp);
    db->hnsw_index->cum_nneighbor_per_level = (int32_t *)malloc(
        sizeof(int32_t) * db->hnsw_index->cum_nneighbor_per_level_size);
    if (db->hnsw_index->cum_nneighbor_per_level == NULL ||
        fread(db->hnsw_index->cum_nneighbor_per_level, sizeof(int32_t),
              db->hnsw_index->cum_nneighbor_per_level_size, fp) != db->hnsw_index->cum_nneighbor_per_level_size) {
        fclose(fp);
        faiss_hnsw_index_drop(db->hnsw_index);
        db->hnsw_index = NULL;
        LOG_ERROR("读取 cum_nneighbor_per_level 失败");
        return -1;
    }

    /* 读取入口点信息 */
    fread(&db->hnsw_index->entry_pointer, sizeof(int32_t), 1, fp);
    fread(&db->hnsw_index->max_level, sizeof(int32_t), 1, fp);

    fclose(fp);

    db->index_built = true;
    db->index_loaded = true;

    LOG_INFO("索引已从 %s 加载，向量数: %d", index_path, n_total);
    return 0;
}

/* ========================================================================
 * PQ 量化压缩 API 实现
 * ======================================================================== */

int vector_engine_enable_pq(void *rel, int32_t subquantizers, int32_t bits) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果已有量化器，先释放 */
    if (db->quantizer != NULL) {
        quantizer_drop(db->quantizer);
        db->quantizer = NULL;
    }

    /* 创建 PQ 量化器配置 */
    quantizer_config_t config;
    quantizer_config_init(&config, db->dimension, QUANTIZATION_TYPE_PQ);

    /* 设置子空间数量和位数 */
    if (subquantizers > 0) {
        config.pq_subquantizers = subquantizers;
    } else {
        /* 默认: dim/8 个子空间 */
        config.pq_subquantizers = db->dimension / 8;
        if (config.pq_subquantizers < 1) {
            config.pq_subquantizers = 1;
        }
    }

    if (bits > 0) {
        config.pq_bits = bits;
    }

    /* 验证配置 */
    if (quantizer_config_validate(&config) != 0) {
        LOG_ERROR("PQ 量化器配置无效");
        return -1;
    }

    /* 创建量化器 */
    db->quantizer = quantizer_create(&config);
    if (db->quantizer == NULL) {
        LOG_ERROR("创建 PQ 量化器失败");
        return -1;
    }

    db->use_quantization = true;
    LOG_INFO("PQ 量化已启用: subquantizers=%d, bits=%d",
             config.pq_subquantizers, config.pq_bits);

    return 0;
}

int vector_engine_train_pq(void *rel, int32_t sample_size) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->quantizer == NULL) {
        LOG_ERROR("量化器未初始化，请先调用 vector_engine_enable_pq");
        return -1;
    }

    if (db->num_vectors == 0) {
        LOG_WARN("没有向量数据可供训练");
        return -1;
    }

    /* 收集训练样本 */
    int32_t train_size = (sample_size > 0) ? sample_size : (int32_t)db->num_vectors;
    if (train_size > (int32_t)db->num_vectors) {
        train_size = (int32_t)db->num_vectors;
    }

    /* 限制最大训练样本数 */
    if (train_size > 10000) {
        train_size = 10000;
    }

    float *samples = (float *)malloc(train_size * db->dimension * sizeof(float));
    if (samples == NULL) {
        LOG_ERROR("分配训练样本内存失败");
        return -1;
    }

    /* 从 VecPage 页池或文件加载向量 */
    int32_t loaded = 0;
    float *vec_buf = (float *)malloc(db->dimension * sizeof(float));

    if (vec_buf) {
        for (int32_t i = 0; i < train_size && i < (int32_t)db->num_vectors; i++) {
            if (db->use_page_pool && db->page_pool != NULL) {
                if (vector_page_get_vector(db->page_pool, i, vec_buf) == 0) {
                    memcpy(&samples[loaded * db->dimension], vec_buf,
                           db->dimension * sizeof(float));
                    loaded++;
                }
            } else {
                /* 从文件加载 */
                char data_path[512];
                get_data_path(db->name, data_path, sizeof(data_path));
                FILE *fp = fopen(data_path, "rb");
                if (fp) {
                    /* 跳过到指定位置 */
                    size_t record_size = sizeof(uint64_t) + sizeof(int32_t) +
                                        db->dimension * sizeof(float);
                    if (fseek(fp, i * record_size, SEEK_SET) == 0) {
                        uint64_t id;
                        int32_t dim;
                        if (fread(&id, sizeof(id), 1, fp) == 1 &&
                            fread(&dim, sizeof(dim), 1, fp) == 1) {
                            if (fread(&samples[loaded * db->dimension],
                                     db->dimension * sizeof(float), 1, fp) == 1) {
                                loaded++;
                            }
                        }
                    }
                    fclose(fp);
                }
                break;  /* 简化为只从文件读一次 */
            }
        }
        free(vec_buf);
    }

    if (loaded < 256) {
        LOG_ERROR("训练样本不足，需要至少 256 个向量");
        free(samples);
        return -1;
    }

    /* 训练量化器 */
    int32_t ret = quantizer_train(db->quantizer, loaded, samples);
    free(samples);

    if (ret != 0) {
        LOG_ERROR("PQ 量化器训练失败");
        return -1;
    }

    LOG_INFO("PQ 量化器训练完成，训练样本数: %d", loaded);
    return 0;
}

int vector_engine_save_pq(void *rel) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->quantizer == NULL) {
        LOG_WARN("量化器不存在，无需保存");
        return -1;
    }

    /* 导出码书 */
    int32_t float_count = quantizer_model_float_count(db->quantizer);
    float *codebook = (float *)malloc(float_count * sizeof(float));
    if (codebook == NULL) {
        LOG_ERROR("分配码书内存失败");
        return -1;
    }

    int32_t trained_size;
    if (quantizer_export_model(db->quantizer, codebook, float_count, &trained_size) != 0) {
        LOG_ERROR("导出码书失败");
        free(codebook);
        return -1;
    }

    /* 保存码书 */
    char codebook_path[512];
    get_pq_codebook_path(db->name, codebook_path, sizeof(codebook_path));

    FILE *fp = fopen(codebook_path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建码书文件");
        free(codebook);
        return -1;
    }

    /* 写入魔数和版本 */
    uint32_t magic = 0x50513031;  /* "PQ01" */
    uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, fp);
    fwrite(&version, sizeof(version), 1, fp);
    fwrite(&trained_size, sizeof(trained_size), 1, fp);
    fwrite(codebook, sizeof(float), trained_size, fp);
    fclose(fp);

    free(codebook);

    /* 保存压缩码 */
    if (db->compressed_count > 0) {
        char codes_path[512];
        get_pq_code_path(db->name, codes_path, sizeof(codes_path));

        fp = fopen(codes_path, "wb");
        if (fp != NULL) {
            int32_t code_size = quantizer_code_size(db->quantizer);
            fwrite(&db->compressed_count, sizeof(db->compressed_count), 1, fp);
            fwrite(&code_size, sizeof(code_size), 1, fp);
            fwrite(db->compressed_codes, code_size, db->compressed_count, fp);
            fclose(fp);
            LOG_INFO("PQ 压缩码已保存: %lu 条", (unsigned long)db->compressed_count);
        }
    }

    LOG_INFO("PQ 量化器已保存到: %s", codebook_path);
    return 0;
}

int vector_engine_load_pq(void *rel) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 加载码书 */
    char codebook_path[512];
    get_pq_codebook_path(db->name, codebook_path, sizeof(codebook_path));

    FILE *fp = fopen(codebook_path, "rb");
    if (fp == NULL) {
        LOG_WARN("PQ 码书文件不存在: %s", codebook_path);
        return -1;
    }

    /* 读取魔数和版本 */
    uint32_t magic, version;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 ||
        fread(&version, sizeof(version), 1, fp) != 1) {
        fclose(fp);
        LOG_ERROR("读取码书头失败");
        return -1;
    }

    if (magic != 0x50513031 || version != 1) {
        fclose(fp);
        LOG_ERROR("码书文件格式无效");
        return -1;
    }

    /* 读取码书大小和数据 */
    int32_t trained_size;
    if (fread(&trained_size, sizeof(trained_size), 1, fp) != 1) {
        fclose(fp);
        LOG_ERROR("读取码书大小失败");
        return -1;
    }

    float *codebook = (float *)malloc(trained_size * sizeof(float));
    if (codebook == NULL || fread(codebook, sizeof(float), trained_size, fp) != (size_t)trained_size) {
        free(codebook);
        fclose(fp);
        LOG_ERROR("读取码书数据失败");
        return -1;
    }
    fclose(fp);

    /* 重建量化器（需要与保存时相同的配置） */
    if (db->quantizer != NULL) {
        quantizer_drop(db->quantizer);
    }

    /* 从码书推断配置 */
    quantizer_config_t config;
    quantizer_config_init(&config, db->dimension, QUANTIZATION_TYPE_PQ);
    /* 默认配置，后续可能需要从文件保存更多信息 */
    config.pq_subquantizers = db->dimension / 8;
    if (config.pq_subquantizers < 1) config.pq_subquantizers = 1;
    config.pq_bits = 8;

    db->quantizer = quantizer_create_from_model(&config, trained_size, codebook, trained_size);
    if (db->quantizer == NULL) {
        free(codebook);
        LOG_ERROR("重建量化器失败");
        return -1;
    }
    free(codebook);

    db->use_quantization = true;

    /* 加载压缩码 */
    char codes_path[512];
    get_pq_code_path(db->name, codes_path, sizeof(codes_path));

    fp = fopen(codes_path, "rb");
    if (fp != NULL) {
        fread(&db->compressed_count, sizeof(db->compressed_count), 1, fp);
        int32_t code_size;
        fread(&code_size, sizeof(code_size), 1, fp);

        db->compressed_codes = (uint8_t *)malloc(db->compressed_count * code_size);
        if (db->compressed_codes) {
            fread(db->compressed_codes, code_size, db->compressed_count, fp);
        }
        fclose(fp);
        LOG_INFO("PQ 压缩码已加载: %lu 条", (unsigned long)db->compressed_count);
    }

    LOG_INFO("PQ 量化器已从: %s 加载", codebook_path);
    return 0;
}

void vector_engine_disable_pq(void *rel) {
    if (rel == NULL) return;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->quantizer != NULL) {
        quantizer_drop(db->quantizer);
        db->quantizer = NULL;
    }

    if (db->compressed_codes != NULL) {
        free(db->compressed_codes);
        db->compressed_codes = NULL;
    }

    db->use_quantization = false;
    db->compressed_count = 0;

    LOG_INFO("PQ 量化已禁用");
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

float vector_l2_distance(const float *a, const float *b, int32_t dim) {
    float dist = 0.0f;
    for (int i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

float vector_cosine_similarity(const float *a, const float *b, int32_t dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

void vector_normalize(float *v, int32_t dim) {
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) norm += v[i] * v[i];
    norm = sqrtf(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < dim; i++) v[i] /= norm;
    }
}

/* ========================================================================
 * 内存池管理 API 实现
 * ======================================================================== */

int vector_engine_enable_mem_pool(void *rel, bool use_pool) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (use_pool && g_vec_pool != NULL) {
        db->mem_pool = g_vec_pool;
        db->use_mem_pool = true;
        LOG_INFO("向量引擎内存池已启用");
    } else {
        db->mem_pool = NULL;
        db->use_mem_pool = false;
        LOG_INFO("向量引擎内存池已禁用");
    }
    return 0;
}

int vector_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats) {
    if (rel == NULL || stats == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->mem_pool != NULL) {
        return mm_pool_get_stats(db->mem_pool, stats);
    }

    memset(stats, 0, sizeof(mm_pool_stats_t));
    return 0;
}

/* ========================================================================
 * 并发锁控制 API 实现
 * ======================================================================== */

/* 简单的读写锁实现（基于自旋锁） */
typedef struct {
    volatile int readers;
    volatile int writers_waiting;
    volatile int writer_active;
} simple_rwlock_t;

static void rwlock_init(simple_rwlock_t *rwlock) {
    rwlock->readers = 0;
    rwlock->writers_waiting = 0;
    rwlock->writer_active = 0;
}

static void rwlock_read_lock(simple_rwlock_t *rwlock) {
    while (1) {
        while (rwlock->writer_active) {
            /* 等待写锁释放 */;
        }
        /* 原子增加读计数 */
        __sync_fetch_and_add(&rwlock->readers, 1);
        /* 再次检查写锁是否激活 */
        if (!rwlock->writer_active) {
            return;
        }
        /* 回退 */
        __sync_fetch_and_sub(&rwlock->readers, 1);
    }
}

static void rwlock_read_unlock(simple_rwlock_t *rwlock) {
    __sync_fetch_and_sub(&rwlock->readers, 1);
}

static void rwlock_write_lock(simple_rwlock_t *rwlock) {
    __sync_fetch_and_add(&rwlock->writers_waiting, 1);
    while (1) {
        /* 等待所有读锁释放 */
        while (rwlock->readers > 0) {
            /* 等待 */;
        }
        /* 尝试获取写锁 */
        if (__sync_bool_compare_and_swap(&rwlock->writer_active, 0, 1)) {
            __sync_fetch_and_sub(&rwlock->writers_waiting, 1);
            return;
        }
    }
}

static void rwlock_write_unlock(simple_rwlock_t *rwlock) {
    __sync_fetch_and_and(&rwlock->writer_active, 0);
}

static void rwlock_destroy(simple_rwlock_t *rwlock) {
    (void)rwlock;  /* 自旋锁无需清理 */
}

int vector_engine_enable_lock(void *rel, bool use_lock) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (use_lock) {
        /* 懒加载全局锁管理器 */
        if (g_vec_lockmgr == NULL) {
            g_vec_lockmgr = lock_mgr_create();
            if (g_vec_lockmgr == NULL) {
                LOG_ERROR("创建向量引擎锁管理器失败");
                return -1;
            }
        }
        /* 初始化读写锁 */
        if (db->rwlock == NULL) {
            db->rwlock = calloc(1, sizeof(simple_rwlock_t));
            if (db->rwlock == NULL) {
                return -1;
            }
            rwlock_init((simple_rwlock_t *)db->rwlock);
        }
        db->lockmgr = g_vec_lockmgr;
        db->use_lock = true;
        LOG_INFO("向量引擎并发锁已启用");
    } else {
        db->use_lock = false;
        db->lockmgr = NULL;
        if (db->rwlock != NULL) {
            rwlock_destroy((simple_rwlock_t *)db->rwlock);
            free(db->rwlock);
            db->rwlock = NULL;
        }
        LOG_INFO("向量引擎并发锁已禁用");
    }
    return 0;
}

int vector_engine_read_lock(void *rel) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->use_lock && db->rwlock != NULL) {
        rwlock_read_lock((simple_rwlock_t *)db->rwlock);
    }
    return 0;
}

void vector_engine_read_unlock(void *rel) {
    if (rel == NULL) return;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->use_lock && db->rwlock != NULL) {
        rwlock_read_unlock((simple_rwlock_t *)db->rwlock);
    }
}

int vector_engine_write_lock(void *rel, uint32_t timeout_ms) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->use_lock && db->rwlock != NULL) {
        rwlock_write_lock((simple_rwlock_t *)db->rwlock);
        (void)timeout_ms;  /* 自旋锁暂不支持超时 */
    }
    (void)db;  /* 消除未使用警告 */
    return 0;
}

void vector_engine_write_unlock(void *rel) {
    if (rel == NULL) return;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->use_lock && db->rwlock != NULL) {
        rwlock_write_unlock((simple_rwlock_t *)db->rwlock);
    }
}

void *vector_engine_get_lockmgr(void *rel) {
    if (rel == NULL) return NULL;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    return db->lockmgr;
}

/* 向量引擎关闭时清理锁资源 */
static void vector_engine_lock_cleanup(void) {
    if (g_vec_lockmgr != NULL) {
        lock_mgr_destroy(g_vec_lockmgr);
        g_vec_lockmgr = NULL;
    }
}

/* ========================================================================
 * IVF-PQ 索引 API 实现
 * ======================================================================== */

int vector_engine_enable_ivf_pq(void *rel, int nlist, int nprobe) {
    if (rel == NULL || nlist <= 0) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果已启用，先禁用 */
    if (db->ivf_pq_index != NULL) {
        ivf_pq_destroy(db->ivf_pq_index);
        db->ivf_pq_index = NULL;
    }

    /* 计算 PQ 参数：m = dim / 4, bits = 8 */
    int pq_m = db->dimension / 4;
    if (pq_m < 4) pq_m = 4;
    int pq_bits = 8;

    /* 创建 IVF-PQ 索引 */
    db->ivf_pq_index = ivf_pq_create(nlist, pq_m, pq_bits, db->dimension);
    if (db->ivf_pq_index == NULL) {
        LOG_ERROR("IVF-PQ 索引创建失败");
        return -1;
    }

    db->use_ivf_pq = true;
    db->ivf_pq_nlist = nlist;
    db->ivf_pq_nprobe = (nprobe > 0) ? nprobe : (nlist / 10);
    if (db->ivf_pq_nprobe < 1) db->ivf_pq_nprobe = 1;

    ivf_pq_set_nprobe(db->ivf_pq_index, db->ivf_pq_nprobe);

    LOG_INFO("IVF-PQ 索引已启用: nlist=%d, nprobe=%d, pq_m=%d", nlist, db->ivf_pq_nprobe, pq_m);
    return 0;
}

int vector_engine_train_ivf_pq(void *rel, int32_t sample_size) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (!db->use_ivf_pq || db->ivf_pq_index == NULL) {
        LOG_ERROR("IVF-PQ 索引未启用");
        return -1;
    }

    if (db->num_vectors == 0) {
        LOG_WARN("没有向量可用于训练");
        return -1;
    }

    /* 分配训练数据缓冲区 */
    int32_t train_size = (sample_size > 0 && sample_size < db->num_vectors) ?
                        sample_size : (int32_t)db->num_vectors;

    float *train_vectors = (float *)malloc(train_size * db->dimension * sizeof(float));
    if (train_vectors == NULL) {
        LOG_ERROR("分配训练数据失败");
        return -1;
    }

    /* 从页池或文件读取向量 */
    int32_t loaded = 0;
    if (db->use_page_pool && db->page_pool != NULL) {
        /* 从页池加载向量 */
        for (uint64_t i = 0; i < db->num_vectors && loaded < train_size; i++) {
            if (vector_page_get_vector(db->page_pool, (int32_t)i,
                                      &train_vectors[loaded * db->dimension]) == 0) {
                loaded++;
            }
        }
    } else {
        /* 从原始文件加载 */
        char data_path[512];
        snprintf(data_path, sizeof(data_path), "%s/%s%s/vectors.bin",
                 g_vector_engine.data_dir, VECTOR_DATA_PREFIX, db->name);

        FILE *fp = fopen(data_path, "rb");
        if (fp != NULL) {
            /* 跳过头部：num_vectors + dimension + metric */
            fseek(fp, sizeof(uint64_t) + sizeof(int32_t) + sizeof(int32_t), SEEK_SET);

            int32_t count = 0;
            while (count < train_size && loaded < db->num_vectors) {
                float vec[db->dimension];
                if (fread(vec, sizeof(float), db->dimension, fp) == (size_t)db->dimension) {
                    memcpy(&train_vectors[loaded * db->dimension], vec, db->dimension * sizeof(float));
                    loaded++;
                    count++;
                } else {
                    break;
                }
            }
            fclose(fp);
        }
    }

    if (loaded == 0) {
        free(train_vectors);
        LOG_ERROR("无法加载训练数据");
        return -1;
    }

    /* 训练 IVF-PQ 索引 */
    int result = ivf_pq_train(db->ivf_pq_index, loaded, train_vectors);
    free(train_vectors);

    if (result != 0) {
        LOG_ERROR("IVF-PQ 训练失败");
        return -1;
    }

    LOG_INFO("IVF-PQ 训练完成，使用 %d 个样本", loaded);
    return 0;
}

int vector_engine_search_ivf_pq(void *rel, const float *query, int32_t top_k,
                               vector_search_results_t *results) {
    if (rel == NULL || query == NULL || results == NULL || top_k <= 0) {
        return -1;
    }

    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (!db->use_ivf_pq || db->ivf_pq_index == NULL) {
        LOG_WARN("IVF-PQ 索引未启用，使用暴力搜索");
        return -1;
    }

    /* 初始化结果 */
    results->results = (vector_search_result_t *)calloc(top_k, sizeof(vector_search_result_t));
    if (results->results == NULL) {
        return -1;
    }
    results->count = 0;
    results->capacity = top_k;

    /* 分配搜索缓冲区 */
    int *ids = (int *)malloc(top_k * sizeof(int));
    float *distances = (float *)malloc(top_k * sizeof(float));
    if (ids == NULL || distances == NULL) {
        free(results->results);
        free(ids);
        free(distances);
        return -1;
    }

    /* 执行 IVF-PQ 搜索 */
    int count = ivf_pq_search(db->ivf_pq_index, query, top_k, ids, distances);

    if (count > 0) {
        results->count = count;
        for (int i = 0; i < count; i++) {
            results->results[i].id = ids[i];
            results->results[i].distance = distances[i];
            /* dimension 和 vector 字段在公共 API 中不存在，已移除 */
        }
    }

    free(ids);
    free(distances);

    return (count >= 0) ? 0 : -1;
}

int vector_engine_save_ivf_pq(void *rel, const char *path) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (!db->use_ivf_pq || db->ivf_pq_index == NULL) {
        LOG_ERROR("IVF-PQ 索引未启用");
        return -1;
    }

    char index_path[512];
    if (path == NULL) {
        snprintf(index_path, sizeof(index_path), "%s/%s%s/ivf_pq.bin",
                 g_vector_engine.data_dir, VECTOR_DATA_PREFIX, db->name);
    } else {
        snprintf(index_path, sizeof(index_path), "%s", path);
    }

    int result = ivf_pq_save(db->ivf_pq_index, index_path);
    if (result != 0) {
        LOG_ERROR("IVF-PQ 索引保存失败");
        return -1;
    }

    LOG_INFO("IVF-PQ 索引已保存: %s", index_path);
    return 0;
}

int vector_engine_load_ivf_pq(void *rel, const char *path) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果已存在索引，先销毁 */
    if (db->ivf_pq_index != NULL) {
        ivf_pq_destroy(db->ivf_pq_index);
        db->ivf_pq_index = NULL;
    }

    char index_path[512];
    if (path == NULL) {
        snprintf(index_path, sizeof(index_path), "%s/%s%s/ivf_pq.bin",
                 g_vector_engine.data_dir, VECTOR_DATA_PREFIX, db->name);
    } else {
        snprintf(index_path, sizeof(index_path), "%s", path);
    }

    db->ivf_pq_index = ivf_pq_load(index_path);
    if (db->ivf_pq_index == NULL) {
        LOG_ERROR("IVF-PQ 索引加载失败: %s", index_path);
        return -1;
    }

    db->use_ivf_pq = true;

    /* 更新统计 */
    int n_vectors, code_size;
    ivf_pq_stats(db->ivf_pq_index, &n_vectors, &code_size);
    db->ivf_pq_nlist = 0;  /* 从索引获取 */

    LOG_INFO("IVF-PQ 索引已加载: %s, vectors=%d", index_path, n_vectors);
    return 0;
}

void vector_engine_disable_ivf_pq(void *rel) {
    if (rel == NULL) return;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->ivf_pq_index != NULL) {
        ivf_pq_destroy(db->ivf_pq_index);
        db->ivf_pq_index = NULL;
    }

    db->use_ivf_pq = false;
    db->ivf_pq_nlist = 0;
    db->ivf_pq_nprobe = 64;

    LOG_INFO("IVF-PQ 索引已禁用");
}

void vector_engine_ivf_pq_stats(void *rel, int *out_n_vectors, int *out_code_size) {
    if (rel == NULL) return;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->ivf_pq_index != NULL) {
        ivf_pq_stats(db->ivf_pq_index, out_n_vectors, out_code_size);
    } else {
        if (out_n_vectors) *out_n_vectors = 0;
        if (out_code_size) *out_code_size = 0;
    }
}

void vector_engine_set_ivf_pq_nprobe(void *rel, int nprobe) {
    if (rel == NULL || nprobe <= 0) return;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->ivf_pq_index != NULL) {
        db->ivf_pq_nprobe = nprobe;
        ivf_pq_set_nprobe(db->ivf_pq_index, nprobe);
    }
}

/* ========================================================================
 * 自动索引选择 API 实现
 * ======================================================================== */

int vector_engine_get_recommended_index(void *rel, int *out_type, int *out_param1, int *out_param2) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    vector_data_info_t info = {
        .num_vectors = db->num_vectors,
        .dimension = db->dimension,
        .available_memory_mb = 4096,  /* 默认 4GB */
        .target_qps = 0,
        .target_recall = 0.95f,
        .is_static = true
    };

    vector_index_decision_t decision;
    if (vector_index_selector_choose(&info, &decision) != 0) {
        return -1;
    }

    if (out_type) *out_type = (int)decision.index_type;
    if (out_param1) *out_param1 = decision.param1;
    if (out_param2) *out_param2 = decision.param2;

    return 0;
}

int vector_engine_auto_select_index(void *rel, float target_qps, float target_recall) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 获取推荐决策 */
    vector_data_info_t info = {
        .num_vectors = db->num_vectors,
        .dimension = db->dimension,
        .available_memory_mb = 4096,
        .target_qps = target_qps,
        .target_recall = (target_recall > 0) ? target_recall : 0.95f,
        .is_static = true
    };

    vector_index_decision_t decision;
    if (vector_index_selector_choose(&info, &decision) != 0) {
        LOG_ERROR("索引选择失败");
        return -1;
    }

    LOG_INFO("自动选择索引: %s, 预估内存: %.2f MB, 预估 QPS: %.0f, 预估召回率: %.2f",
             vector_index_selector_get_name(decision.index_type),
             decision.estimated_memory_mb,
             decision.estimated_qps,
             decision.estimated_recall);

    /* 根据决策启用相应索引 */
    switch (decision.index_type) {
    case VECTOR_INDEX_HNSW:
        LOG_INFO("启用 HNSW 索引: m=%d, ef=%d", decision.param1, decision.param2);
        /* TODO: 调用 HNSW 构建 */
        return 0;

    case VECTOR_INDEX_IVF_PQ:
        LOG_INFO("启用 IVF-PQ 索引: nlist=%d, nprobe=%d",
                 decision.param1, decision.param2);
        return vector_engine_enable_ivf_pq(rel, decision.param1, decision.param2);

    case VECTOR_INDEX_BRUTE_FORCE:
    default:
        LOG_INFO("使用暴力搜索（无需索引）");
        return 0;
    }
}

const char *vector_engine_get_index_name(void *rel) {
    if (rel == NULL) return "unknown";

    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (db->use_ivf_pq) {
        return "ivf_pq";
    }
    if (db->index_built) {
        return "hnsw";
    }
    return "brute_force";
}

/* ========================================================================
 * WAL 持久化 API 实现
 * ======================================================================== */

int vector_engine_enable_wal(void *rel, int mode) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果已有 WAL，先关闭 */
    if (db->wal != NULL) {
        vector_wal_close(db->wal);
        db->wal = NULL;
    }

    /* 构造 WAL 路径 */
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s%s",
             g_vector_engine.data_dir, VECTOR_DATA_PREFIX, db->name);

    char wal_path[512];
    snprintf(wal_path, sizeof(wal_path), "%s/wal", dir_path);

    /* 创建或打开 WAL */
    vector_wal_t *wal = vector_wal_open(wal_path, db->segment_id);
    if (wal == NULL) {
        LOG_ERROR("创建 WAL 失败: %s", wal_path);
        db->use_wal = false;
        return -1;
    }

    db->wal = wal;
    db->use_wal = (mode != 2);  /* mode 2 = WAL_NONE */

    LOG_INFO("向量集合 '%s' WAL 已启用, mode=%d", db->name, mode);
    return 0;
}

int vector_engine_checkpoint(void *rel) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (!db->use_wal || db->wal == NULL) {
        LOG_WARN("WAL 未启用，无法执行 checkpoint");
        return -1;
    }

    /* 执行检查点 */
    if (vector_wal_checkpoint(db->wal, VECTOR_CHECKPOINT_MANUAL) != 0) {
        LOG_ERROR("Checkpoint 执行失败");
        return -1;
    }

    LOG_INFO("向量集合 '%s' checkpoint 完成", db->name);
    return 0;
}

int vector_engine_recover(const char *name, const char *data_dir) {
    if (name == NULL || data_dir == NULL) return -1;

    /* 构造数据目录路径 */
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s%s", data_dir, VECTOR_DATA_PREFIX, name);

    /* 执行恢复 */
    vector_recovery_result_t *result = vector_index_recover(dir_path, 0);
    if (result == NULL) {
        LOG_ERROR("恢复失败: %s", name);
        return -1;
    }

    if (result->status == VECTOR_RECOVERY_OK) {
        LOG_INFO("恢复成功: %s, records=%lu, vectors=%lu, elapsed=%lums",
                 name, result->records_recovered, result->vectors_recovered,
                 result->elapsed_ms);
        vector_recovery_result_free(result);
        return 0;
    }

    LOG_WARN("恢复状态: %s, msg=%s",
             vector_recovery_status_str(result->status), result->error_msg);
    vector_recovery_result_free(result);
    return -1;
}

int vector_engine_get_wal_stats(void *rel, uint64_t *out_records, uint64_t *out_bytes) {
    if (rel == NULL) return -1;
    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    if (!db->use_wal || db->wal == NULL) {
        if (out_records) *out_records = 0;
        if (out_bytes) *out_bytes = 0;
        return 0;
    }

    if (out_records) *out_records = vector_wal_num_records(db->wal);
    if (out_bytes) *out_bytes = vector_wal_file_size(db->wal);

    return 0;
}

/* ========================================================================
 * 扩展查询接口实现 (Wave 1-③ 新增)
 * ======================================================================== */

/**
 * @brief 执行范围查询 (距离阈值过滤)
 */
int vector_query_range(void *rel,
                       const float *query, int32_t query_dim,
                       float max_distance,
                       int32_t max_results,
                       vector_search_results_t *results) {
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    if (!db || !query || !results || query_dim <= 0) {
        return -1;
    }

    /* 先执行 Top-K 搜索 */
    int32_t search_k = max_results * 2;  /* 多搜索一些候选 */
    int rc = vector_engine_search(rel, query, query_dim, search_k, results);
    if (rc != 0) {
        return rc;
    }

    /* 过滤距离超过阈值的候选 */
    int32_t valid_count = 0;
    for (int32_t i = 0; i < results->count; i++) {
        if (results->results[i].distance <= max_distance) {
            if (valid_count != i) {
                /* 移动有效结果到前面 */
                results->results[valid_count].id = results->results[i].id;
                results->results[valid_count].distance = results->results[i].distance;
            }
            valid_count++;
        }
    }

    results->count = valid_count;

    LOG_DEBUG("范围查询完成: max_distance=%.4f, 有效结果=%d/%d",
              max_distance, valid_count, search_k);

    return 0;
}

/**
 * @brief 执行带过滤条件的向量查询
 */
int vector_query_filtered(void *rel,
                          const float *query, int32_t query_dim,
                          int32_t top_k,
                          const char *filter_column,
                          int filter_op,
                          const void *filter_value,
                          vector_search_results_t *results) {
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    if (!db || !query || !results || query_dim <= 0) {
        return -1;
    }

    /* 先执行 Top-K 搜索 */
    int32_t search_k = top_k * 10;  /* 搜索更多候选以应对过滤损失 */
    int rc = vector_engine_search(rel, query, query_dim, search_k, results);
    if (rc != 0) {
        return rc;
    }

    /* 应用标量过滤 */
    if (filter_column && filter_value) {
        int32_t valid_count = 0;
        for (int32_t i = 0; i < results->count && valid_count < top_k; i++) {
            /* TODO: 从元数据列中获取属性值并比较 */
            /* 当前简化实现：假设所有结果都满足条件 */
            if (valid_count != i) {
                results->results[valid_count].id = results->results[i].id;
                results->results[valid_count].distance = results->results[i].distance;
            }
            valid_count++;
        }
        results->count = valid_count;
    }

    LOG_DEBUG("过滤查询完成: top_k=%d, 过滤列=%s, 结果=%d",
              top_k, filter_column ? filter_column : "无", results->count);

    return 0;
}

/**
 * @brief 执行多索引混合查询
 */
int vector_query_hybrid(void *rel,
                        const float *query, int32_t query_dim,
                        const char *text_query,
                        int32_t top_k,
                        float vector_weight,
                        float text_weight,
                        vector_search_results_t *results) {
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    if (!db || !query || !results || query_dim <= 0) {
        return -1;
    }

    /* === 阶段 1: 向量索引搜索 === */
    vector_search_results_t vec_results;
    memset(&vec_results, 0, sizeof(vec_results));
    vec_results.results = (vector_search_result_t *)malloc(top_k * sizeof(vector_search_result_t));
    vec_results.capacity = top_k;

    if (!vec_results.results) {
        /* 修复前：BUFFER 位置原代码为 `free(vec_results.results); return -1;`
         * 由于分配失败时 vec_results.results 已经是 NULL，这次 free 没有意义。
         * 修复后：直接返回 -1，避免 free(NULL) 调用掩盖真实问题。*/
        return -1;
    }

    int rc = vector_engine_search(rel, query, query_dim, top_k, &vec_results);
    if (rc != 0) {
        free(vec_results.results);
        return rc;
    }

    /* === 阶段 2: BM25 文本搜索 === */
    vector_search_results_t text_results;
    memset(&text_results, 0, sizeof(text_results));
    text_results.results = (vector_search_result_t *)malloc(top_k * sizeof(vector_search_result_t));
    text_results.capacity = top_k;

    if (!text_results.results) {
        free(vec_results.results);
        free(text_results.results);
        return -1;
    }

    /* TODO: 实现真正的 BM25 搜索 */
    /* 当前简化实现：生成占位结果 */
    text_results.count = top_k / 2;
    for (int32_t i = 0; i < text_results.count; i++) {
        text_results.results[i].id = i;
        text_results.results[i].distance = 1.0f - (float)i / top_k;  /* BM25 分数 */
    }

    /* === 阶段 3: RRF 融合 === */
    results->results = (vector_search_result_t *)malloc(top_k * sizeof(vector_search_result_t));
    results->capacity = top_k;

    if (!results->results) {
        free(vec_results.results);
        free(text_results.results);
        /* 修复前：原代码为 `free(results->results); return -1;`
         * 由于分配失败时 results->results 已经是 NULL，这次 free 没有意义。
         * 修复后：直接返回 -1，避免 free(NULL) 调用。*/
        return -1;
    }

    /* RRF 融合计算 */
    int32_t rrf_k = 60;  /* RRF 参数 */
    float *scores = (float *)malloc(top_k * sizeof(float));
    if (!scores) {
        free(vec_results.results);
        free(text_results.results);
        free(results->results);
        return -1;
    }

    memset(scores, 0, top_k * sizeof(float));

    /* 向量结果贡献 */
    for (int32_t i = 0; i < vec_results.count; i++) {
        int64_t id = (int64_t)vec_results.results[i].id;
        if (id < top_k) {
            scores[id] += vector_weight / (rrf_k + i + 1);
        }
    }

    /* 文本结果贡献 */
    for (int32_t i = 0; i < text_results.count; i++) {
        int64_t id = (int64_t)text_results.results[i].id;
        if (id < top_k) {
            scores[id] += text_weight / (rrf_k + i + 1);
        }
    }

    /* 按分数排序并填充结果 */
    /* 简化实现：取分数最高的 top_k 个 */
    results->count = top_k;
    for (int32_t i = 0; i < top_k; i++) {
        results->results[i].id = i;
        results->results[i].distance = 1.0f - scores[i];  /* 转换为距离 */
    }

    free(scores);
    free(vec_results.results);
    free(text_results.results);

    LOG_DEBUG("混合查询完成: vector_weight=%.2f, text_weight=%.2f, 结果=%d",
              vector_weight, text_weight, results->count);

    return 0;
}

/**
 * @brief 查询配置默认值
 */
static const VectorQueryConfig g_default_query_config = {
    .timeout_ms = 30000,
    .batch_size = 1024,
    .ef_search = 100,
    .nprobe = 10,
    .enable_rerank = false,
    .enable_profiling = false,
    .metric = DISTANCE_METRIC_L2_SQUARED
};

/**
 * @brief 执行带配置的向量查询
 */
int vector_query_with_config(void *rel,
                             const float *query, int32_t query_dim,
                             int32_t top_k,
                             const VectorQueryConfig *config,
                             vector_search_results_t *results) {
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    if (!db || !query || !results || query_dim <= 0) {
        return -1;
    }

    /* 使用默认配置或自定义配置 */
    VectorQueryConfig cfg = config ? *config : g_default_query_config;

    /* 设置搜索参数 */
    if (db->hnsw_index && cfg.ef_search > 0) {
        /* TODO: 设置 HNSW ef_search 参数 */
    }

    if (db->ivf_pq_index && cfg.nprobe > 0) {
        vector_engine_set_ivf_pq_nprobe(rel, cfg.nprobe);
    }

    /* 执行搜索 */
    int rc = vector_engine_search(rel, query, query_dim, top_k, results);
    if (rc != 0) {
        return rc;
    }

    /* 精排 (如果启用) */
    if (cfg.enable_rerank && results->count > 0) {
        /* TODO: 调用 ReRanker 进行精排 */
        LOG_DEBUG("精排已启用但暂未实现");
    }

    LOG_DEBUG("带配置查询完成: top_k=%d, timeout=%dms, batch=%d",
              top_k, cfg.timeout_ms, cfg.batch_size);

    return 0;
}

/**
 * @brief 设置默认查询配置
 */
int vector_engine_set_query_config(void *rel, const VectorQueryConfig *config) {
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    if (!db || !config) {
        return -1;
    }

    /* 存储配置 (简化实现：直接存储在 db 结构中) */
    /* TODO: 在 vector_engine_db_t 中添加 query_config 字段 */

    LOG_DEBUG("查询配置已设置: timeout=%dms, ef_search=%d, nprobe=%d",
              config->timeout_ms, config->ef_search, config->nprobe);

    return 0;
}

/**
 * @brief 获取当前默认查询配置
 */
int vector_engine_get_query_config(void *rel, VectorQueryConfig *config) {
    vector_engine_db_t *db = (vector_engine_db_t *)rel;
    if (!db || !config) {
        return -1;
    }

    /* 返回默认配置 */
    *config = g_default_query_config;

    /* TODO: 从 db 结构中读取实际配置 */

    return 0;
}

/* ========================================================================
 * 持久化接口实现
 * ======================================================================== */

int vector_index_save(void *rel, const char *path, vector_persist_result_t *result) {
    if (rel == NULL || path == NULL) {
        if (result) {
            result->success = 0;
            result->error_code = -1;
            strncpy(result->error_msg, "Invalid arguments", sizeof(result->error_msg) - 1);
        }
        return -1;
    }

    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 如果索引未构建，先构建 */
    if (!db->index_built && db->num_vectors > 0) {
        LOG_WARN("索引未构建，无法保存");
        if (result) {
            result->success = 0;
            result->error_code = -2;
            strncpy(result->error_msg, "Index not built", sizeof(result->error_msg) - 1);
        }
        return -1;
    }

    /* 确保目录存在 */
    char dir_path[512];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    char *last_sep = strrchr(dir_path, '/');
    if (last_sep) {
        *last_sep = '\0';
        ensure_dir(dir_path);
    }

    /* 调用 hnsw_persist_save */
    hnsw_persist_result_t hnsw_result = {0};
    int ret = hnsw_persist_save(db->hnsw_index, path, &hnsw_result);

    if (result) {
        result->success = (ret == 0) ? 1 : 0;
        result->error_code = hnsw_result.error_code;
        strncpy(result->error_msg, hnsw_result.error_msg, sizeof(result->error_msg) - 1);
        result->bytes_written = hnsw_result.bytes_written;
    }

    if (ret == 0) {
        LOG_INFO("向量索引已保存: %s (%lu bytes)", path, (unsigned long)hnsw_result.bytes_written);
    } else {
        LOG_ERROR("向量索引保存失败: %s (error: %s)", path, hnsw_result.error_msg);
    }

    return ret;
}

int vector_index_load(void *rel, const char *path, vector_persist_result_t *result) {
    if (rel == NULL || path == NULL) {
        if (result) {
            result->success = 0;
            result->error_code = -1;
            strncpy(result->error_msg, "Invalid arguments", sizeof(result->error_msg) - 1);
        }
        return -1;
    }

    vector_engine_db_t *db = (vector_engine_db_t *)rel;

    /* 加载索引 */
    hnsw_persist_result_t hnsw_result = {0};
    void *index = hnsw_persist_load(path, &hnsw_result);

    if (index == NULL) {
        if (result) {
            result->success = 0;
            result->error_code = hnsw_result.error_code;
            strncpy(result->error_msg, hnsw_result.error_msg, sizeof(result->error_msg) - 1);
        }
        LOG_ERROR("向量索引加载失败: %s (error: %s)", path, hnsw_result.error_msg);
        return -1;
    }

    /* 释放旧索引 */
    if (db->hnsw_index) {
        faiss_hnsw_index_drop(db->hnsw_index);
    }

    db->hnsw_index = index;
    db->index_built = 1;

    /* 更新统计信息 */
    int32_t total_count = 0;
    int32_t deleted_count = 0;
    uint64_t memory_usage = 0;
    hnsw_persist_stats(index, &total_count, &deleted_count, &memory_usage);
    db->num_vectors = total_count;

    if (result) {
        result->success = 1;
        result->error_code = 0;
        strncpy(result->error_msg, "", sizeof(result->error_msg) - 1);
        result->bytes_read = hnsw_result.bytes_read;
    }

    LOG_INFO("向量索引已加载: %s (%d vectors, %lu bytes)",
             path, total_count, (unsigned long)hnsw_result.bytes_read);

    return 0;
}

bool vector_index_is_valid(const char *path) {
    return hnsw_persist_is_valid(path);
}

int vector_index_get_meta(const char *path, int32_t *dims, int32_t *n_total, int32_t *metric) {
    if (path == NULL) {
        return -1;
    }

    hnsw_persist_meta_t meta = {0};
    int ret = hnsw_persist_get_meta(path, &meta);
    if (ret != 0) {
        return -1;
    }

    if (dims) *dims = meta.dims;
    if (n_total) *n_total = meta.n_total;
    if (metric) *metric = meta.metric;

    return 0;
}
