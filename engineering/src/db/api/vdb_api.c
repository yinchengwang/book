/**
 * @file vdb_api.c
 * @brief MiniVecDB 统一 C SDK 实现
 *
 * 整合 VectorAPI 与底层向量引擎，提供统一的数据库操作接口。
 */
#include <db/vdb_api.h>
#include <db/api/vector_api.h>
#include <db/core/vector_query.h>
#include <db/core/query_filter.h>
#include <db/core/query_fusion.h>
#include <db/storage/vector/vector_persist.h>
#include <db/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 集合内部包装 */
typedef struct vdb_collection_s {
    char name[128];              /**< 集合名称 */
    VectorAPI *api;              /**< 所属 VectorAPI */
    VectorCollectionInfo info;   /**< 集合元信息（缓存） */
} vdb_collection_t;

/** 数据库内部结构 */
typedef struct vdb_handle_s {
    char data_dir[512];          /**< 数据目录 */
    vdb_config_t config;         /**< 配置 */
    VectorAPI *vector_api;       /**< 底层 VectorAPI */
    vdb_collection_t **collections; /**< 集合句柄缓存 */
    int32_t n_collections;       /**< 集合数量 */
    int32_t capacity;            /**< 容量 */
    int64_t start_time;          /**< 启动时间 */
    char error_msg[256];         /**< 错误信息 */
} vdb_handle_t;

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/** 设置错误信息 */
static void set_error(vdb_handle_t *db, const char *fmt, ...) {
    if (!db) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(db->error_msg, sizeof(db->error_msg), fmt, args);
    va_end(args);
}

/** 获取当前时间（毫秒） */
static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/** 在缓存中查找集合 */
static vdb_collection_t *find_cached(vdb_handle_t *db, const char *name) {
    for (int32_t i = 0; i < db->n_collections; i++) {
        if (strcmp(db->collections[i]->name, name) == 0) {
            return db->collections[i];
        }
    }
    return NULL;
}

/** 将 VectorIndexType 映射为 vdb_index_type_t */
static vdb_index_type_t map_index_type(VectorIndexType t) {
    switch (t) {
        case VECTOR_INDEX_HNSW: return VDB_INDEX_HNSW;
        case VECTOR_INDEX_IVF: return VDB_INDEX_IVF;
        case VECTOR_INDEX_DISKANN: return VDB_INDEX_DISKANN;
        case VECTOR_INDEX_BRUTE_FORCE: return VDB_INDEX_BRUTE_FORCE;
        default: return VDB_INDEX_AUTO;
    }
}

/** 将 vdb_index_type_t 映射为 VectorIndexType */
static VectorIndexType map_index_type_rev(vdb_index_type_t t) {
    switch (t) {
        case VDB_INDEX_HNSW: return VECTOR_INDEX_HNSW;
        case VDB_INDEX_IVF: return VECTOR_INDEX_IVF;
        case VDB_INDEX_DISKANN: return VECTOR_INDEX_DISKANN;
        case VDB_INDEX_BRUTE_FORCE: return VECTOR_INDEX_BRUTE_FORCE;
        default: return VECTOR_INDEX_AUTO;
    }
}

/** 将 VectorMetricType 映射为 vdb_metric_type_t */
static vdb_metric_type_t map_metric_type(VectorMetricType t) {
    switch (t) {
        case VECTOR_METRIC_COSINE: return VDB_METRIC_COSINE;
        case VECTOR_METRIC_IP: return VDB_METRIC_IP;
        default: return VDB_METRIC_L2;
    }
}

/** 将 vdb_metric_type_t 映射为 VectorMetricType */
static VectorMetricType map_metric_type_rev(vdb_metric_type_t t) {
    switch (t) {
        case VDB_METRIC_COSINE: return VECTOR_METRIC_COSINE;
        case VDB_METRIC_IP: return VECTOR_METRIC_IP;
        default: return VECTOR_METRIC_L2;
    }
}

/* ========================================================================
 * 数据库生命周期
 * ======================================================================== */

vdb_handle_t *vdb_open(const char *path, const vdb_config_t *config) {
    if (!path) return NULL;

    vdb_handle_t *db = (vdb_handle_t *)calloc(1, sizeof(*db));
    if (!db) return NULL;

    strncpy(db->data_dir, path, sizeof(db->data_dir) - 1);

    /* 设置默认配置 */
    if (config) {
        db->config = *config;
    } else {
        strcpy(db->config.data_dir, path);
        db->config.enable_wal = true;
        db->config.max_collections = 64;
        db->config.cache_size_mb = 128;
    }

    db->capacity = 16;
    db->collections = (vdb_collection_t **)calloc(db->capacity, sizeof(vdb_collection_t *));
    if (!db->collections) {
        free(db);
        return NULL;
    }

    /* 创建底层 VectorAPI */
    db->vector_api = vector_api_create(path);
    if (!db->vector_api) {
        set_error(db, "创建 VectorAPI 失败");
        free(db->collections);
        free(db);
        return NULL;
    }

    /* 从磁盘加载已有集合 */
    vector_api_load(db->vector_api);

    /* 将已加载的集合缓存为 vdb_collection_t 句柄 */
    char **names = NULL;
    int32_t count = 0;
    if (vector_api_list_collections(db->vector_api, &names, &count) == 0) {
        for (int32_t i = 0; i < count; i++) {
            vdb_collection_t *coll = (vdb_collection_t *)calloc(1, sizeof(*coll));
            if (!coll) continue;
            strncpy(coll->name, names[i], sizeof(coll->name) - 1);
            coll->api = db->vector_api;
            vector_api_get_collection(db->vector_api, names[i], &coll->info);
            db->collections[db->n_collections++] = coll;
        }
        vector_api_free_names(names, count);
    }

    db->start_time = now_ms();

    LOG_INFO("VDB 打开成功: data_dir=%s, collections=%d",
             db->data_dir, db->n_collections);
    return db;
}

int vdb_close(vdb_handle_t *db) {
    if (!db) return VDB_ERR_INVALID_PARAM;

    /* 保存到磁盘 */
    vector_api_save(db->vector_api);

    /* 释放集合缓存 */
    for (int32_t i = 0; i < db->n_collections; i++) {
        free(db->collections[i]);
    }
    free(db->collections);

    /* 释放底层 VectorAPI */
    if (db->vector_api) {
        vector_api_destroy(db->vector_api);
    }

    LOG_INFO("VDB 关闭完成: data_dir=%s", db->data_dir);
    free(db);
    return VDB_OK;
}

const char *vdb_error(vdb_handle_t *db) {
    return db ? db->error_msg : "NULL handle";
}

/* ========================================================================
 * 集合管理
 * ======================================================================== */

int vdb_create_collection(vdb_handle_t *db, const char *name,
                          const vdb_collection_config_t *config) {
    if (!db || !name || !config) {
        if (db) set_error(db, "无效参数");
        return VDB_ERR_INVALID_PARAM;
    }

    /* 检查是否已存在 */
    if (find_cached(db, name)) {
        set_error(db, "集合已存在: %s", name);
        return VDB_ERR_EXISTS;
    }

    /* 构造 VectorCreateParams */
    VectorCreateParams params = {0};
    params.name = name;
    params.dimension = config->dimension;
    params.index_type = map_index_type_rev(config->index_type);
    params.metric_type = map_metric_type_rev(config->metric_type);
    params.index_ef_search = config->index_ef_search > 0 ? config->index_ef_search : 100;
    params.index_m = config->index_m > 0 ? config->index_m : 16;
    params.index_ef_construction = config->index_ef_construction > 0
                                   ? config->index_ef_construction : 200;

    int rc = vector_api_create_collection(db->vector_api, &params);
    if (rc != 0) {
        set_error(db, "创建集合失败: %s", vector_api_error(db->vector_api));
        return VDB_ERR_INTERNAL;
    }

    /* 创建缓存句柄 */
    vdb_collection_t *coll = (vdb_collection_t *)calloc(1, sizeof(*coll));
    if (!coll) return VDB_ERR_NO_MEMORY;
    strncpy(coll->name, name, sizeof(coll->name) - 1);
    coll->api = db->vector_api;
    vector_api_get_collection(db->vector_api, name, &coll->info);

    /* 添加到缓存 */
    if (db->n_collections >= db->capacity) {
        db->capacity *= 2;
        vdb_collection_t **new_arr = (vdb_collection_t **)realloc(
            db->collections, db->capacity * sizeof(vdb_collection_t *));
        if (!new_arr) {
            free(coll);
            return VDB_ERR_NO_MEMORY;
        }
        db->collections = new_arr;
    }
    db->collections[db->n_collections++] = coll;

    LOG_INFO("集合创建成功: %s, dim=%d", name, config->dimension);
    return VDB_OK;
}

int vdb_drop_collection(vdb_handle_t *db, const char *name) {
    if (!db || !name) return VDB_ERR_INVALID_PARAM;

    int rc = vector_api_drop_collection(db->vector_api, name);
    if (rc != 0) {
        set_error(db, "删除集合失败: %s", vector_api_error(db->vector_api));
        return VDB_ERR_NOT_FOUND;
    }

    /* 从缓存中移除 */
    for (int32_t i = 0; i < db->n_collections; i++) {
        if (strcmp(db->collections[i]->name, name) == 0) {
            free(db->collections[i]);
            for (int32_t j = i; j < db->n_collections - 1; j++) {
                db->collections[j] = db->collections[j + 1];
            }
            db->n_collections--;
            break;
        }
    }

    LOG_INFO("集合删除成功: %s", name);
    return VDB_OK;
}

vdb_collection_t *vdb_get_collection(vdb_handle_t *db, const char *name) {
    if (!db || !name) return NULL;

    vdb_collection_t *coll = find_cached(db, name);
    if (!coll) {
        set_error(db, "集合不存在: %s", name);
        return NULL;
    }

    /* 刷新缓存信息 */
    vector_api_get_collection(db->vector_api, name, &coll->info);
    return coll;
}

int vdb_list_collections(vdb_handle_t *db, char ***names, int32_t *count) {
    if (!db || !names || !count) return VDB_ERR_INVALID_PARAM;
    return vector_api_list_collections(db->vector_api, names, count);
}

void vdb_free_names(char **names, int32_t count) {
    vector_api_free_names(names, count);
}

/* ========================================================================
 * 向量操作
 * ======================================================================== */

int64_t vdb_insert(vdb_collection_t *coll, const float *vector, int32_t dim,
                   const void *metadata, int32_t metadata_size) {
    if (!coll || !vector || dim <= 0) return VDB_ERR_INVALID_PARAM;

    /* 构造单条插入参数 */
    float *vecs[1] = {(float *)vector};
    void *metas[1] = {(void *)metadata};
    int32_t meta_sizes[1] = {metadata_size};

    VectorInsertParams params = {0};
    params.collection = coll->name;
    params.n = 1;
    params.vectors = vecs;
    params.ids = NULL;  /* 自动分配 ID */
    params.metadata = metadata ? metas : NULL;
    params.metadata_sizes = metadata ? meta_sizes : NULL;

    int64_t out_id = 0;
    int rc = vector_api_insert(coll->api, &params, &out_id);
    if (rc < 0) return rc;

    return out_id;
}

int32_t vdb_insert_batch(vdb_collection_t *coll, const float *vectors,
                         int32_t dim, int32_t n, int64_t *ids) {
    if (!coll || !vectors || dim <= 0 || n <= 0) return VDB_ERR_INVALID_PARAM;

    /* 构造向量指针数组 */
    float **vec_ptrs = (float **)malloc(n * sizeof(float *));
    if (!vec_ptrs) return VDB_ERR_NO_MEMORY;
    for (int32_t i = 0; i < n; i++) {
        vec_ptrs[i] = (float *)(vectors + i * dim);
    }

    VectorInsertParams params = {0};
    params.collection = coll->name;
    params.n = n;
    params.vectors = vec_ptrs;
    params.ids = NULL;  /* 自动分配 ID */

    int rc = vector_api_insert(coll->api, &params, ids);
    free(vec_ptrs);
    return rc;
}

int vdb_delete(vdb_collection_t *coll, int64_t id) {
    if (!coll) return VDB_ERR_INVALID_PARAM;
    int rc = vector_api_delete(coll->api, coll->name, &id, 1);
    return rc < 0 ? rc : VDB_OK;
}

int32_t vdb_delete_batch(vdb_collection_t *coll, const int64_t *ids, int32_t n) {
    if (!coll || !ids || n <= 0) return VDB_ERR_INVALID_PARAM;
    return vector_api_delete(coll->api, coll->name, ids, n);
}

/* ========================================================================
 * 查询接口
 * ======================================================================== */

vdb_results_t *vdb_search(vdb_collection_t *coll, const float *query, int32_t dim,
                          const vdb_search_params_t *params) {
    if (!coll || !query || dim <= 0) return NULL;

    int32_t top_k = params && params->top_k > 0 ? params->top_k : 10;

    /* 构造 VectorSearchParams */
    VectorSearchParams sp = {0};
    sp.collection = coll->name;
    sp.query = query;
    sp.top_k = top_k;
    sp.radius = params ? params->radius : 0;
    sp.with_distance = params ? params->with_distance : true;
    sp.with_metadata = params ? params->with_metadata : false;
    sp.filter = NULL;

    /* 执行搜索 */
    int64_t t0 = now_ms();
    VectorSearchResults *sr = vector_api_search(coll->api, &sp);
    int64_t took_us = (now_ms() - t0) * 1000;

    if (!sr) return NULL;

    /* 转换结果 */
    int32_t count = vector_api_results_count(sr);
    vdb_results_t *results = (vdb_results_t *)calloc(1, sizeof(*results));
    if (!results) {
        vector_api_results_free(sr);
        return NULL;
    }

    results->items = (vdb_result_item_t *)calloc(count, sizeof(vdb_result_item_t));
    results->count = count;
    results->capacity = count;
    results->total_time_us = took_us;

    for (int32_t i = 0; i < count; i++) {
        const VectorSearchResult *r = vector_api_results_get(sr, i);
        if (!r) continue;
        results->items[i].id = r->id;
        results->items[i].distance = r->distance;
        results->items[i].score = 1.0f / (1.0f + r->distance); /* 距离转分数 */
        if (r->metadata && r->metadata_size > 0) {
            results->items[i].metadata = malloc(r->metadata_size);
            memcpy(results->items[i].metadata, r->metadata, r->metadata_size);
            results->items[i].metadata_size = r->metadata_size;
        }
    }

    vector_api_results_free(sr);
    return results;
}

int32_t vdb_size(vdb_collection_t *coll) {
    if (!coll) return VDB_ERR_INVALID_PARAM;
    return vector_api_size(coll->api, coll->name);
}

int32_t vdb_dimension(vdb_collection_t *coll) {
    if (!coll) return VDB_ERR_INVALID_PARAM;
    VectorCollectionInfo info = {0};
    if (vector_api_get_collection(coll->api, coll->name, &info) != 0) {
        return VDB_ERR_NOT_FOUND;
    }
    return info.dimension;
}

int vdb_collection_info(vdb_collection_t *coll, vdb_collection_config_t *config) {
    if (!coll || !config) return VDB_ERR_INVALID_PARAM;

    VectorCollectionInfo info = {0};
    int rc = vector_api_get_collection(coll->api, coll->name, &info);
    if (rc != 0) return VDB_ERR_NOT_FOUND;

    strncpy(config->name, info.name, sizeof(config->name) - 1);
    config->dimension = info.dimension;
    config->index_type = map_index_type(info.index_type);
    config->metric_type = map_metric_type(info.metric_type);

    return VDB_OK;
}

/* ========================================================================
 * 结果集管理
 * ======================================================================== */

void vdb_results_free(vdb_results_t *results) {
    if (!results) return;
    for (int32_t i = 0; i < results->count; i++) {
        if (results->items[i].metadata) {
            free(results->items[i].metadata);
        }
    }
    free(results->items);
    free(results);
}

/* ========================================================================
 * 持久化
 * ======================================================================== */

int vdb_save(vdb_handle_t *db) {
    if (!db) return VDB_ERR_INVALID_PARAM;
    return vector_api_save(db->vector_api);
}

int vdb_load(vdb_handle_t *db) {
    if (!db) return VDB_ERR_INVALID_PARAM;
    return vector_api_load(db->vector_api);
}

/* ========================================================================
 * 多模态查询接口
 * ======================================================================== */

const char *vdb_filter_error(vdb_handle_t *db) {
    return db ? db->error_msg : "NULL handle";
}

vdb_results_t *vdb_search_filtered(vdb_collection_t *coll, const float *query, int32_t dim,
                                    const vdb_filtered_search_params_t *params) {
    if (!coll || !query || dim <= 0) return NULL;

    /* 解析过滤表达式 */
    FilterNode *filter_ast = NULL;
    if (params && params->filter_expr && strlen(params->filter_expr) > 0) {
        FilterParseResult *parse_result = filter_parse(params->filter_expr);
        if (!parse_result) {
            set_error(NULL, "过滤表达式解析失败");
            return NULL;
        }
        if (parse_result->error != FILTER_PARSE_OK) {
            set_error(NULL, "过滤表达式错误: %s", parse_result->error_msg);
            filter_parse_result_free(parse_result);
            return NULL;
        }
        filter_ast = parse_result->ast;
        filter_parse_result_free(parse_result);
    }

    int32_t top_k = (params && params->base.top_k > 0) ? params->base.top_k : 10;

    /* 构造搜索参数 */
    VectorSearchParams sp = {0};
    sp.collection = coll->name;
    sp.query = query;
    sp.top_k = top_k;
    sp.radius = (params && params->base.radius > 0) ? params->base.radius : 0;
    sp.with_distance = params ? params->base.with_distance : true;
    sp.with_metadata = params ? params->base.with_metadata : false;
    sp.filter = NULL;  /* 暂时不支持 filter 下推，后续集成 */

    /* 执行搜索 */
    int64_t t0 = now_ms();
    VectorSearchResults *sr = vector_api_search(coll->api, &sp);
    int64_t took_us = (now_ms() - t0) * 1000;

    if (!sr) {
        if (filter_ast) filter_ast_free(filter_ast);
        return NULL;
    }

    int32_t count = vector_api_results_count(sr);

    /* 如果有过滤表达式，应用过滤 */
    if (filter_ast && count > 0) {
        /* 创建临时记录上下文用于过滤评估 */
        /* 注意：这里需要实际的元数据解析，实际使用时需要扩展 */
        /* 目前先返回所有结果，后续集成元数据过滤 */
    }

    /* 转换结果 */
    vdb_results_t *results = (vdb_results_t *)calloc(1, sizeof(*results));
    if (!results) {
        vector_api_results_free(sr);
        if (filter_ast) filter_ast_free(filter_ast);
        return NULL;
    }

    results->items = (vdb_result_item_t *)calloc(count, sizeof(vdb_result_item_t));
    results->count = count;
    results->capacity = count;
    results->total_time_us = took_us;

    for (int32_t i = 0; i < count; i++) {
        const VectorSearchResult *r = vector_api_results_get(sr, i);
        if (!r) continue;
        results->items[i].id = r->id;
        results->items[i].distance = r->distance;
        results->items[i].score = 1.0f / (1.0f + r->distance);
        if (r->metadata && r->metadata_size > 0) {
            results->items[i].metadata = malloc(r->metadata_size);
            memcpy(results->items[i].metadata, r->metadata, r->metadata_size);
            results->items[i].metadata_size = r->metadata_size;
        }
    }

    vector_api_results_free(sr);
    if (filter_ast) filter_ast_free(filter_ast);

    return results;
}

vdb_results_t *vdb_hybrid_search(vdb_collection_t *coll, const float *query, int32_t dim,
                                  const vdb_hybrid_search_params_t *params) {
    if (!coll || !query || dim <= 0) return NULL;

    /* 解析过滤表达式（如果有） */
    FilterNode *filter_ast = NULL;
    if (params && params->filter_expr && strlen(params->filter_expr) > 0) {
        FilterParseResult *parse_result = filter_parse(params->filter_expr);
        if (parse_result && parse_result->error == FILTER_PARSE_OK) {
            filter_ast = parse_result->ast;
        }
        if (parse_result) filter_parse_result_free(parse_result);
    }

    /* 获取搜索参数 */
    int32_t top_k = (params && params->base.top_k > 0) ? params->base.top_k : 10;
    vdb_fusion_strategy_t fusion = (params && params->fusion >= 0) ? params->fusion : VDB_FUSION_RRF;
    float vector_weight = (params && params->vector_weight >= 0) ? params->vector_weight : 0.5f;
    float bm25_weight = (params && params->bm25_weight >= 0) ? params->bm25_weight : 0.5f;
    int rrf_k = (params && params->rrf_k > 0) ? params->rrf_k : 60;

    /* 执行向量搜索 */
    int64_t t0 = now_ms();

    VectorSearchParams sp = {0};
    sp.collection = coll->name;
    sp.query = query;
    sp.top_k = top_k * 2;  /* 多取一些结果用于融合 */
    sp.with_distance = true;
    sp.with_metadata = true;

    VectorSearchResults *vector_results = vector_api_search(coll->api, &sp);

    /* TODO: 执行 BM25 搜索（需要集成 BM25 索引） */
    /* 目前仅使用向量搜索结果 */
    QueryResultItem *fusion_vector_items = NULL;
    QueryResultItem *fusion_bm25_items = NULL;
    int num_vector = 0;
    int num_bm25 = 0;

    if (vector_results) {
        num_vector = vector_api_results_count(vector_results);
        fusion_vector_items = (QueryResultItem *)calloc(num_vector, sizeof(QueryResultItem));
        if (fusion_vector_items) {
            for (int32_t i = 0; i < num_vector; i++) {
                const VectorSearchResult *r = vector_api_results_get(vector_results, i);
                if (r) {
                    fusion_vector_items[i].id = r->id;
                    fusion_vector_items[i].vector_score = 1.0f / (1.0f + r->distance);
                    fusion_vector_items[i].bm25_score = 0;
                    fusion_vector_items[i].vector_rank = i + 1;
                    fusion_vector_items[i].bm25_rank = 0;
                }
            }
        }
        vector_api_results_free(vector_results);
    }

    /* 执行融合 */
    FusionConfig *fusion_config = NULL;
    if (fusion == VDB_FUSION_RRF) {
        fusion_config = fusion_config_create_rrf(rrf_k, top_k);
    } else {
        fusion_config = fusion_config_create_weighted(vector_weight, bm25_weight, top_k);
    }

    QueryResultList *fused = NULL;
    if (fusion_config) {
        fused = fusion_execute(fusion_vector_items, num_vector,
                               fusion_bm25_items, num_bm25, fusion_config);
        fusion_config_free(fusion_config);
    }

    /* 转换结果 */
    vdb_results_t *results = (vdb_results_t *)calloc(1, sizeof(*results));
    if (!results) {
        free(fusion_vector_items);
        if (fused) query_result_list_free(fused);
        if (filter_ast) filter_ast_free(filter_ast);
        return NULL;
    }

    int32_t result_count = fused ? fused->count : num_vector;
    results->items = (vdb_result_item_t *)calloc(result_count, sizeof(vdb_result_item_t));
    results->capacity = result_count;
    results->total_time_us = (now_ms() - t0) * 1000;

    if (fused && fused->count > 0) {
        results->count = fused->count;
        for (int32_t i = 0; i < fused->count; i++) {
            const QueryResultItem *item = query_result_list_get(fused, i);
            if (item) {
                results->items[i].id = item->id;
                results->items[i].distance = 1.0f - item->combined_score;
                results->items[i].score = item->combined_score;
            }
        }
    } else if (fusion_vector_items) {
        results->count = num_vector;
        for (int32_t i = 0; i < num_vector; i++) {
            results->items[i].id = fusion_vector_items[i].id;
            results->items[i].distance = 1.0f - fusion_vector_items[i].vector_score;
            results->items[i].score = fusion_vector_items[i].vector_score;
        }
    }

    /* 清理 */
    free(fusion_vector_items);
    free(fusion_bm25_items);
    if (fused) query_result_list_free(fused);
    if (filter_ast) filter_ast_free(filter_ast);

    return results;
}

/* ========================================================================
 * 统计
 * ======================================================================== */

int vdb_stats(vdb_handle_t *db, vdb_stats_t *stats) {
    if (!db || !stats) return VDB_ERR_INVALID_PARAM;

    stats->collection_count = db->n_collections;

    int64_t total = 0;
    for (int32_t i = 0; i < db->n_collections; i++) {
        int32_t sz = vector_api_size(db->vector_api, db->collections[i]->name);
        if (sz > 0) total += sz;
    }
    stats->total_vectors = total;
    stats->total_memory_bytes = 0;  /* 暂不计算 */
    stats->uptime_ms = now_ms() - db->start_time;

    return VDB_OK;
}