/**
 * @file vector_api.c
 * @brief 向量数据库 API 实现
 *
 * 实现向量集合管理、向量插入、搜索、删除等核心功能。
 */
#include <db/api/vector_api.h>
#include <db/core/vector_query.h>
#include <db/storage/vector/vector_persist.h>
#include <db/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 集合内部结构 */
typedef struct VectorCollectionImpl_s {
    char name[128];              /**< 集合名称 */
    int32_t dimension;           /**< 向量维度 */
    int32_t size;                /**< 当前向量数量 */
    int32_t capacity;            /**< 容量 */
    VectorIndexType index_type;  /**< 索引类型 */
    VectorMetricType metric_type;/**< 距离度量 */
    int64_t created_at;          /**< 创建时间 */
    int64_t updated_at;          /**< 更新时间 */

    /* 向量数据 */
    int64_t *ids;                /**< ID 数组 */
    float *vectors;              /**< 向量数据（扁平化） */
    void **metadata;             /**< 元数据数组 */
    int32_t *metadata_sizes;     /**< 元数据大小数组 */
    int64_t next_id;             /**< 下一个自动生成的 ID */

    /* 查询计划 */
    VectorQueryPlan *query_plan; /**< 查询计划 */
} VectorCollectionImpl;

/** API 内部结构 */
struct VectorAPI_s {
    char data_dir[512];          /**< 数据目录 */
    VectorCollectionImpl **collections; /**< 集合数组 */
    int32_t n_collections;       /**< 集合数量 */
    int32_t capacity;            /**< 容量 */
    char error_msg[256];         /**< 错误信息 */
};

/** 搜索结果内部结构 */
struct VectorSearchResults_s {
    VectorSearchResult *results; /**< 结果数组 */
    int32_t count;               /**< 结果数量 */
    int32_t capacity;            /**< 容量 */
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 设置错误信息
 */
static void api_set_error(VectorAPI *api, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(api->error_msg, sizeof(api->error_msg), fmt, args);
    va_end(args);
    LOG_ERROR("VectorAPI: %s", api->error_msg);
}

/**
 * @brief 查找集合
 */
static VectorCollectionImpl *find_collection(VectorAPI *api, const char *name) {
    for (int32_t i = 0; i < api->n_collections; i++) {
        if (strcmp(api->collections[i]->name, name) == 0) {
            return api->collections[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取当前时间（毫秒）
 */
static int64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ========================================================================
 * 集合管理
 * ======================================================================== */

/**
 * @brief 创建集合实现
 */
static VectorCollectionImpl *collection_create(const VectorCreateParams *params) {
    VectorCollectionImpl *coll = (VectorCollectionImpl *)calloc(1, sizeof(*coll));
    if (!coll) return NULL;

    strncpy(coll->name, params->name, sizeof(coll->name) - 1);
    coll->dimension = params->dimension;
    coll->index_type = params->index_type;
    coll->metric_type = params->metric_type;
    coll->created_at = get_current_time_ms();
    coll->updated_at = coll->created_at;
    coll->next_id = 1;
    coll->capacity = 1024;
    coll->size = 0;

    /* 分配初始空间 */
    coll->ids = (int64_t *)malloc(coll->capacity * sizeof(int64_t));
    coll->vectors = (float *)malloc(coll->capacity * coll->dimension * sizeof(float));
    coll->metadata = (void **)calloc(coll->capacity, sizeof(void *));
    coll->metadata_sizes = (int32_t *)calloc(coll->capacity, sizeof(int32_t));

    if (!coll->ids || !coll->vectors) {
        free(coll->ids);
        free(coll->vectors);
        free(coll->metadata);
        free(coll->metadata_sizes);
        free(coll);
        return NULL;
    }

    /* 创建查询计划 */
    coll->query_plan = vector_query_plan_create();
    if (coll->query_plan) {
        VectorIndexType vtype = params->index_type;
        if (vtype == VECTOR_INDEX_AUTO) {
            vtype = VECTOR_INDEX_HNSW;
        }
        vector_query_plan_set_coarse(coll->query_plan, vtype,
                                     params->index_ef_search > 0 ? params->index_ef_search : 100,
                                     params->index_m > 0 ? params->index_m * 10 : 100);
    }

    LOG_INFO("创建集合: name=%s, dim=%d, type=%d", coll->name, coll->dimension, coll->index_type);
    return coll;
}

/**
 * @brief 销毁集合实现
 */
static void collection_destroy(VectorCollectionImpl *coll) {
    if (!coll) return;

    /* 销毁查询计划 */
    if (coll->query_plan != NULL) {
        vector_query_plan_destroy(coll->query_plan);
        coll->query_plan = NULL;
    }

    /* 释放元数据 */
    for (int32_t i = 0; i < coll->size; i++) {
        if (coll->metadata && coll->metadata[i]) {
            free(coll->metadata[i]);
            coll->metadata[i] = NULL;
        }
    }

    /* 释放数据数组 */
    if (coll->ids) { free(coll->ids); coll->ids = NULL; }
    if (coll->vectors) { free(coll->vectors); coll->vectors = NULL; }
    if (coll->metadata) { free(coll->metadata); coll->metadata = NULL; }
    if (coll->metadata_sizes) { free(coll->metadata_sizes); coll->metadata_sizes = NULL; }
    free(coll);
}

/**
 * @brief 扩展集合容量
 */
static int collection_expand(VectorCollectionImpl *coll) {
    if (coll->size >= coll->capacity) {
        int32_t new_capacity = coll->capacity * 2;

        /* 逐个 realloc，失败时恢复原指针 */
        int64_t *new_ids = (int64_t *)realloc(coll->ids, new_capacity * sizeof(int64_t));
        if (!new_ids) return -1;
        coll->ids = new_ids;

        float *new_vectors = (float *)realloc(coll->vectors, new_capacity * coll->dimension * sizeof(float));
        if (!new_vectors) return -1;
        coll->vectors = new_vectors;

        void **new_metadata = (void **)realloc(coll->metadata, new_capacity * sizeof(void *));
        if (!new_metadata) return -1;
        coll->metadata = new_metadata;

        int32_t *new_sizes = (int32_t *)realloc(coll->metadata_sizes, new_capacity * sizeof(int32_t));
        if (!new_sizes) return -1;
        coll->metadata_sizes = new_sizes;

        coll->capacity = new_capacity;
    }
    return 0;
}

/* ========================================================================
 * API 生命周期
 * ======================================================================== */

VectorAPI *vector_api_create(const char *data_dir) {
    VectorAPI *api = (VectorAPI *)calloc(1, sizeof(*api));
    if (!api) return NULL;

    if (data_dir) {
        strncpy(api->data_dir, data_dir, sizeof(api->data_dir) - 1);
    } else {
        strcpy(api->data_dir, "./vector_data");
    }

    api->capacity = 16;
    api->collections = (VectorCollectionImpl **)calloc(api->capacity, sizeof(VectorCollectionImpl *));
    if (!api->collections) {
        free(api);
        return NULL;
    }

    /* 初始化向量查询执行器 */
    vector_query_init();

    LOG_INFO("VectorAPI 创建成功: data_dir=%s", api->data_dir);
    return api;
}

void vector_api_destroy(VectorAPI *api) {
    if (!api) return;

    for (int32_t i = 0; i < api->n_collections; i++) {
        collection_destroy(api->collections[i]);
    }
    free(api->collections);

    vector_query_shutdown();
    free(api);
    LOG_INFO("VectorAPI 销毁完成");
}

const char *vector_api_error(VectorAPI *api) {
    return api ? api->error_msg : "NULL API";
}

/* ========================================================================
 * 集合管理 API
 * ======================================================================== */

int vector_api_create_collection(VectorAPI *api, const VectorCreateParams *params) {
    if (!api || !params || !params->name) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    /* 检查是否已存在 */
    if (find_collection(api, params->name)) {
        api_set_error(api, "集合已存在: %s", params->name);
        return VECTOR_API_ERR_EXISTS;
    }

    /* 扩展容量 */
    if (api->n_collections >= api->capacity) {
        int32_t new_cap = api->capacity * 2;
        VectorCollectionImpl **new_arr = (VectorCollectionImpl **)realloc(
            api->collections, new_cap * sizeof(VectorCollectionImpl *));
        if (!new_arr) {
            api_set_error(api, "内存分配失败");
            return VECTOR_API_ERR_NO_MEMORY;
        }
        api->collections = new_arr;
        api->capacity = new_cap;
    }

    /* 创建集合 */
    VectorCollectionImpl *coll = collection_create(params);
    if (!coll) {
        api_set_error(api, "创建集合失败");
        return VECTOR_API_ERR_NO_MEMORY;
    }

    api->collections[api->n_collections++] = coll;
    LOG_INFO("集合创建成功: %s", params->name);
    return VECTOR_API_OK;
}

int vector_api_drop_collection(VectorAPI *api, const char *name) {
    if (!api || !name) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    for (int32_t i = 0; i < api->n_collections; i++) {
        if (strcmp(api->collections[i]->name, name) == 0) {
            collection_destroy(api->collections[i]);

            /* 移动数组元素 */
            for (int32_t j = i; j < api->n_collections - 1; j++) {
                api->collections[j] = api->collections[j + 1];
            }
            api->n_collections--;

            LOG_INFO("集合删除成功: %s", name);
            return VECTOR_API_OK;
        }
    }

    api_set_error(api, "集合不存在: %s", name);
    return VECTOR_API_ERR_NOT_FOUND;
}

int vector_api_get_collection(VectorAPI *api, const char *name, VectorCollectionInfo *info) {
    if (!api || !name || !info) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    VectorCollectionImpl *coll = find_collection(api, name);
    if (!coll) {
        api_set_error(api, "集合不存在: %s", name);
        return VECTOR_API_ERR_NOT_FOUND;
    }

    memset(info, 0, sizeof(*info));
    strncpy(info->name, coll->name, sizeof(info->name) - 1);
    info->dimension = coll->dimension;
    info->size = coll->size;
    info->index_type = coll->index_type;
    info->metric_type = coll->metric_type;
    info->created_at = coll->created_at;
    info->updated_at = coll->updated_at;

    return VECTOR_API_OK;
}

int vector_api_list_collections(VectorAPI *api, char ***names, int32_t *n) {
    if (!api || !names || !n) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    *names = (char **)malloc(api->n_collections * sizeof(char *));
    if (!*names) {
        api_set_error(api, "内存分配失败");
        return VECTOR_API_ERR_NO_MEMORY;
    }

    for (int32_t i = 0; i < api->n_collections; i++) {
        (*names)[i] = strdup(api->collections[i]->name);
    }
    *n = api->n_collections;

    return VECTOR_API_OK;
}

void vector_api_free_names(char **names, int32_t n) {
    if (!names) return;
    for (int32_t i = 0; i < n; i++) {
        free(names[i]);
    }
    free(names);
}

/* ========================================================================
 * 向量操作 API
 * ======================================================================== */

int vector_api_insert(VectorAPI *api, const VectorInsertParams *params, int64_t *out_ids) {
    if (!api || !params || !params->collection || !params->vectors) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    VectorCollectionImpl *coll = find_collection(api, params->collection);
    if (!coll) {
        api_set_error(api, "集合不存在: %s", params->collection);
        return VECTOR_API_ERR_NOT_FOUND;
    }

    int32_t inserted = 0;
    for (int32_t i = 0; i < params->n; i++) {
        if (collection_expand(coll) != 0) break;

        int64_t id = params->ids ? params->ids[i] : coll->next_id++;
        if (!params->ids) {
            if (out_ids) out_ids[i] = id;
        }

        coll->ids[coll->size] = id;
        memcpy(coll->vectors + coll->size * coll->dimension,
               params->vectors[i],
               coll->dimension * sizeof(float));

        if (params->metadata && params->metadata[i]) {
            coll->metadata[coll->size] = malloc(params->metadata_sizes[i]);
            memcpy(coll->metadata[coll->size], params->metadata[i], params->metadata_sizes[i]);
            coll->metadata_sizes[coll->size] = params->metadata_sizes[i];
        }

        coll->size++;
        inserted++;
    }

    coll->updated_at = get_current_time_ms();
    LOG_INFO("插入向量成功: collection=%s, count=%d", params->collection, inserted);
    return inserted;
}

VectorSearchResults *vector_api_search(VectorAPI *api, const VectorSearchParams *params) {
    if (!api || !params || !params->collection || !params->query) {
        if (api) api_set_error(api, "无效参数");
        return NULL;
    }

    VectorCollectionImpl *coll = find_collection(api, params->collection);
    if (!coll) {
        api_set_error(api, "集合不存在: %s", params->collection);
        return NULL;
    }

    if (coll->size == 0) {
        /* 返回空结果 */
        VectorSearchResults *results = (VectorSearchResults *)calloc(1, sizeof(*results));
        return results;
    }

    /* 暴力搜索（简化实现） */
    VectorSearchResults *results = (VectorSearchResults *)calloc(1, sizeof(*results));
    results->results = (VectorSearchResult *)malloc(params->top_k * sizeof(VectorSearchResult));
    results->capacity = params->top_k;

    /* 简单暴力搜索 + 排序 */
    for (int32_t i = 0; i < coll->size; i++) {
        float dist = 0;
        for (int32_t d = 0; d < coll->dimension; d++) {
            float diff = params->query[d] - coll->vectors[i * coll->dimension + d];
            dist += diff * diff;
        }

        if (params->radius <= 0 || dist <= params->radius * params->radius) {
            /* 插入排序 */
            int32_t pos = results->count;
            for (int32_t j = 0; j < results->count; j++) {
                if (dist < results->results[j].distance) {
                    pos = j;
                    break;
                }
            }

            if (pos < params->top_k) {
                /* 移动元素 */
                for (int32_t j = results->count; j > pos && j > 0; j--) {
                    results->results[j] = results->results[j - 1];
                }
                results->results[pos].id = coll->ids[i];
                results->results[pos].distance = dist;
                if (params->with_metadata && coll->metadata[i]) {
                    results->results[pos].metadata = coll->metadata[i];
                    results->results[pos].metadata_size = coll->metadata_sizes[i];
                }
                if (results->count < params->top_k) {
                    results->count++;
                }
            }
        }
    }

    return results;
}

int32_t vector_api_results_count(const VectorSearchResults *results) {
    return results ? results->count : 0;
}

const VectorSearchResult *vector_api_results_get(const VectorSearchResults *results, int32_t index) {
    if (!results || index < 0 || index >= results->count) {
        return NULL;
    }
    return &results->results[index];
}

void vector_api_results_free(VectorSearchResults *results) {
    if (!results) return;
    free(results->results);
    free(results);
}

int vector_api_delete(VectorAPI *api, const char *collection, const int64_t *ids, int32_t n) {
    if (!api || !collection || !ids || n <= 0) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    VectorCollectionImpl *coll = find_collection(api, collection);
    if (!coll) {
        api_set_error(api, "集合不存在: %s", collection);
        return VECTOR_API_ERR_NOT_FOUND;
    }

    int32_t deleted = 0;
    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < coll->size; j++) {
            if (coll->ids[j] == ids[i]) {
                /* 释放元数据 */
                if (coll->metadata[j]) {
                    free(coll->metadata[j]);
                }

                /* 移动数组元素 */
                for (int32_t k = j; k < coll->size - 1; k++) {
                    coll->ids[k] = coll->ids[k + 1];
                    memcpy(coll->vectors + k * coll->dimension,
                           coll->vectors + (k + 1) * coll->dimension,
                           coll->dimension * sizeof(float));
                    coll->metadata[k] = coll->metadata[k + 1];
                    coll->metadata_sizes[k] = coll->metadata_sizes[k + 1];
                }
                coll->size--;
                deleted++;
                break;
            }
        }
    }

    coll->updated_at = get_current_time_ms();
    LOG_INFO("删除向量成功: collection=%s, count=%d", collection, deleted);
    return deleted;
}

int32_t vector_api_size(VectorAPI *api, const char *collection) {
    if (!api || !collection) {
        if (api) api_set_error(api, "无效参数");
        return VECTOR_API_ERR_INVALID_PARAM;
    }

    VectorCollectionImpl *coll = find_collection(api, collection);
    if (!coll) {
        api_set_error(api, "集合不存在: %s", collection);
        return VECTOR_API_ERR_NOT_FOUND;
    }

    return coll->size;
}

/* ========================================================================
 * 持久化
 * ======================================================================== */

int vector_api_save(VectorAPI *api) {
    if (!api) return VECTOR_API_ERR_INVALID_PARAM;

    /* 确保数据目录存在 */
#ifdef _WIN32
    _mkdir(api->data_dir);
#else
    mkdir(api->data_dir, 0755);
#endif

    char path[1024];
    snprintf(path, sizeof(path), "%s/collections.meta", api->data_dir);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        api_set_error(api, "无法打开文件: %s", path);
        return VECTOR_API_ERR_IO;
    }

    /* 写入集合数量 */
    fwrite(&api->n_collections, sizeof(int32_t), 1, fp);

    for (int32_t i = 0; i < api->n_collections; i++) {
        VectorCollectionImpl *coll = api->collections[i];

        /* 写入集合元数据 */
        fwrite(coll->name, sizeof(coll->name), 1, fp);
        fwrite(&coll->dimension, sizeof(int32_t), 1, fp);
        fwrite(&coll->size, sizeof(int32_t), 1, fp);
        fwrite(&coll->index_type, sizeof(int32_t), 1, fp);
        fwrite(&coll->metric_type, sizeof(int32_t), 1, fp);
        fwrite(&coll->created_at, sizeof(int64_t), 1, fp);
        fwrite(&coll->updated_at, sizeof(int64_t), 1, fp);
        fwrite(&coll->next_id, sizeof(int64_t), 1, fp);

        /* 写入向量数据 */
        size_t vectors_size = coll->size * coll->dimension * sizeof(float);
        fwrite(&vectors_size, sizeof(size_t), 1, fp);
        if (vectors_size > 0) {
            fwrite(coll->vectors, vectors_size, 1, fp);
        }

        /* 写入 ID 数组 */
        if (coll->size > 0) {
            fwrite(coll->ids, sizeof(int64_t), coll->size, fp);
        }

        /* 写入元数据 */
        for (int32_t j = 0; j < coll->size; j++) {
            fwrite(&coll->metadata_sizes[j], sizeof(int32_t), 1, fp);
            if (coll->metadata_sizes[j] > 0 && coll->metadata[j]) {
                fwrite(coll->metadata[j], coll->metadata_sizes[j], 1, fp);
            }
        }
    }

    fclose(fp);
    LOG_INFO("保存集合成功: %s, count=%d", path, api->n_collections);
    return VECTOR_API_OK;
}

int vector_api_load(VectorAPI *api) {
    if (!api) return VECTOR_API_ERR_INVALID_PARAM;

    char path[1024];
    snprintf(path, sizeof(path), "%s/collections.meta", api->data_dir);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        /* 文件不存在不是错误 */
        LOG_INFO("无持久化数据文件: %s", path);
        return VECTOR_API_OK;
    }

    /* 读取集合数量 */
    int32_t n_collections;
    if (fread(&n_collections, sizeof(int32_t), 1, fp) != 1) {
        fclose(fp);
        api_set_error(api, "读取集合数量失败");
        return VECTOR_API_ERR_IO;
    }

    for (int32_t i = 0; i < n_collections; i++) {
        VectorCollectionImpl *coll = (VectorCollectionImpl *)calloc(1, sizeof(*coll));
        if (!coll) break;

        /* 读取集合元数据 */
        fread(coll->name, sizeof(coll->name), 1, fp);
        fread(&coll->dimension, sizeof(int32_t), 1, fp);
        fread(&coll->size, sizeof(int32_t), 1, fp);
        fread(&coll->index_type, sizeof(int32_t), 1, fp);
        fread(&coll->metric_type, sizeof(int32_t), 1, fp);
        fread(&coll->created_at, sizeof(int64_t), 1, fp);
        fread(&coll->updated_at, sizeof(int64_t), 1, fp);
        fread(&coll->next_id, sizeof(int64_t), 1, fp);

        /* 读取向量数据 */
        size_t vectors_size;
        fread(&vectors_size, sizeof(size_t), 1, fp);
        if (vectors_size > 0) {
            /* 从文件加载：capacity 根据实际数据大小计算 */
            coll->capacity = (int32_t)(vectors_size / (coll->dimension * sizeof(float)));
            coll->vectors = (float *)malloc(vectors_size);
            fread(coll->vectors, vectors_size, 1, fp);
        } else {
            /* 空集合：分配默认容量 */
            coll->capacity = 1024;
            coll->vectors = (float *)malloc(coll->capacity * coll->dimension * sizeof(float));
        }

        /* 读取 ID 数组 */
        if (coll->size > 0) {
            coll->ids = (int64_t *)malloc(coll->capacity * sizeof(int64_t));
            fread(coll->ids, sizeof(int64_t), coll->size, fp);
        } else {
            coll->ids = (int64_t *)malloc(coll->capacity * sizeof(int64_t));
        }

        /* 读取元数据 */
        coll->metadata = (void **)calloc(coll->capacity, sizeof(void *));
        coll->metadata_sizes = (int32_t *)calloc(coll->capacity, sizeof(int32_t));

        for (int32_t j = 0; j < coll->size; j++) {
            fread(&coll->metadata_sizes[j], sizeof(int32_t), 1, fp);
            if (coll->metadata_sizes[j] > 0) {
                coll->metadata[j] = malloc(coll->metadata_sizes[j]);
                fread(coll->metadata[j], coll->metadata_sizes[j], 1, fp);
            }
        }

        /* 创建查询计划 */
        coll->query_plan = vector_query_plan_create();

        api->collections[api->n_collections++] = coll;
    }

    fclose(fp);
    LOG_INFO("加载集合成功: count=%d", api->n_collections);
    return VECTOR_API_OK;
}
