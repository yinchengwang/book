/**
 * @file handlers.c
 * @brief REST API 请求处理器实现
 */
#include <db/api/rest_api.h>
#include <db/api/vector_api.h>
#include <db/vdb_api.h>
#include <db/index/vector_index/BM25/bm25.h>
#include <db/log.h>
#include <cJSON.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 从 RequestContext 获取 RestServer 句柄
 */
static inline RestServer *ctx_server(RequestContext *ctx) {
    return (RestServer *)request_get_user_data(ctx);
}

/**
 * @brief 从 RequestContext 获取 VectorAPI 句柄
 */
static inline VectorAPI *ctx_vector_api(RequestContext *ctx) {
    return (VectorAPI *)rest_server_get_vector_api(ctx_server(ctx));
}

/**
 * @brief 从 RequestContext 获取 VDB 统一 API 句柄
 */
static inline vdb_handle_t *ctx_vdb(RequestContext *ctx) {
    return (vdb_handle_t *)rest_server_get_vdb(ctx_server(ctx));
}

/**
 * @brief 查找集合对应的 BM25 索引（不存在返回 NULL）
 */
static inline bm25_t *ctx_bm25_index(RequestContext *ctx, const char *name) {
    return (bm25_t *)rest_server_bm25_lookup(ctx_server(ctx), name);
}

/**
 * @brief 创建集合对应的 BM25 索引（已存在返回现有）
 */
static inline bm25_t *ctx_bm25_ensure(RequestContext *ctx, const char *name) {
    return (bm25_t *)rest_server_bm25_ensure(ctx_server(ctx), name);
}

/**
 * @brief 删除集合对应的 BM25 索引
 */
static inline void ctx_bm25_drop(RequestContext *ctx, const char *name) {
    rest_server_bm25_drop(ctx_server(ctx), name);
}

/**
 * @brief 将 VectorCollectionInfo 转为 cJSON 对象
 */
static cJSON *collection_info_to_json(const VectorCollectionInfo *info) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", info->name);
    cJSON_AddStringToObject(json, "name", info->name);
    cJSON_AddNumberToObject(json, "dimension", info->dimension);
    cJSON_AddNumberToObject(json, "vector_count", info->size);
    cJSON_AddNumberToObject(json, "index_type", (int)info->index_type);
    cJSON_AddNumberToObject(json, "metric_type", (int)info->metric_type);
    cJSON_AddNumberToObject(json, "created_at", (double)info->created_at);
    cJSON_AddStringToObject(json, "status", "ready");
    return json;
}

/* ========================================================================
 * 健康检查端点
 * ======================================================================== */

/**
 * @brief GET /health - 健康检查
 */
static int handle_health(RequestContext *ctx) {
    (void)ctx;
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "ok");
    cJSON_AddStringToObject(json, "version", "0.1.0");

    char *str = cJSON_Print(json);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(json);

    return 0;
}

/**
 * @brief GET /ready - 就绪检查
 */
static int handle_ready(RequestContext *ctx) {
    (void)ctx;
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "ready");
    cJSON_AddBoolToObject(json, "storage", true);
    cJSON_AddBoolToObject(json, "vector_index", true);

    char *str = cJSON_Print(json);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(json);

    return 0;
}

/**
 * @brief GET /live - 活跃检查
 */
static int handle_live(RequestContext *ctx) {
    response_set_json(ctx, "{\"status\":\"alive\"}");
    return 0;
}

/**
 * @brief GET /metrics - Prometheus 指标
 */
static int handle_metrics(RequestContext *ctx) {
    RestServer *server = (RestServer *)ctx;  // 通过某种方式获取 server
    (void)server;

    // 获取服务器指标
    const char *metrics = "minivecdb_up 1\nminivecdb_build_info{version=\"0.1.0\"} 1\n";
    response_set_text(ctx, metrics);
    return 0;
}

/* ========================================================================
 * 集合管理接口
 * ======================================================================== */

/**
 * @brief POST /collections - 创建集合
 */
static int handle_create_collection(RequestContext *ctx) {
    const char *body = request_get_body(ctx);
    if (!body || strlen(body) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_REQUEST", "Empty request body");
        return -1;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        response_error(ctx, HTTP_BAD_REQUEST, "PARSE_ERROR", "Invalid JSON");
        return -1;
    }

    cJSON *name = cJSON_GetObjectItem(root, "name");
    cJSON *dimension = cJSON_GetObjectItem(root, "dimension");
    cJSON *index_type = cJSON_GetObjectItem(root, "index_type");
    cJSON *metric_type = cJSON_GetObjectItem(root, "metric_type");

    if (!name || !cJSON_IsString(name) || !dimension || !cJSON_IsNumber(dimension)) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                      "Missing required fields: name, dimension");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    /* 解析索引类型（缺省为 AUTO=99） */
    int idx_type_int = (index_type && cJSON_IsNumber(index_type))
                       ? index_type->valueint : 99;
    if (idx_type_int != 0 && idx_type_int != 1 && idx_type_int != 2 &&
        idx_type_int != 3 && idx_type_int != 99) {
        idx_type_int = 99;  /* 非法值回退到 AUTO */
    }

    /* 解析距离度量（缺省为 L2=0） */
    int metric_int = 0;
    if (metric_type && cJSON_IsString(metric_type)) {
        const char *m = metric_type->valuestring;
        if (strcmp(m, "cosine") == 0 || strcmp(m, "COSINE") == 0) metric_int = 1;
        else if (strcmp(m, "ip") == 0 || strcmp(m, "IP") == 0) metric_int = 2;
        else metric_int = 0;
    }

    VectorCreateParams params = {0};
    params.name = cJSON_GetStringValue(name);
    params.dimension = dimension->valueint;
    params.index_type = (VectorIndexType)idx_type_int;
    params.metric_type = (VectorMetricType)metric_int;
    params.index_ef_search = 50;
    params.index_m = 16;
    params.index_ef_construction = 100;

    int rc = vector_api_create_collection(api, &params);
    if (rc != 0) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_INTERNAL_ERROR, "CREATE_FAILED",
                      vector_api_error(api));
        return -1;
    }

    /* 初始化对应的空 BM25 索引 */
    ctx_bm25_ensure(ctx, params.name);

    /* 查询刚创建的集合信息（含真实 index_type / size） */
    VectorCollectionInfo info = {0};
    vector_api_get_collection(api, params.name, &info);

    cJSON *json = collection_info_to_json(&info);
    cJSON_AddStringToObject(json, "id", info.name);

    char *str = cJSON_Print(json);
    response_set_json(ctx, str);
    response_set_status(ctx, HTTP_CREATED);
    free(str);
    cJSON_Delete(json);
    cJSON_Delete(root);

    return 0;
}

/**
 * @brief GET /collections - 列出所有集合
 */
static int handle_list_collections(RequestContext *ctx) {
    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    char **names = NULL;
    int32_t count = 0;
    int rc = vector_api_list_collections(api, &names, &count);
    if (rc != 0) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "LIST_FAILED", vector_api_error(api));
        return -1;
    }

    cJSON *arr = cJSON_CreateArray();
    for (int32_t i = 0; i < count; i++) {
        VectorCollectionInfo info = {0};
        if (vector_api_get_collection(api, names[i], &info) == 0) {
            cJSON_AddItemToArray(arr, collection_info_to_json(&info));
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", names[i]);
        cJSON_AddItemToArray(arr, item);
    }
    vector_api_free_names(names, count);

    char *str = cJSON_Print(arr);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(arr);

    return 0;
}

/**
 * @brief GET /collections/:id - 获取集合详情
 */
static int handle_get_collection(RequestContext *ctx) {
    const char *id = request_get_path_param(ctx, 0);
    if (!id || strlen(id) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Collection ID required");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    VectorCollectionInfo info = {0};
    int rc = vector_api_get_collection(api, id, &info);
    if (rc != 0) {
        response_error(ctx, HTTP_NOT_FOUND, "NOT_FOUND",
                      "Collection not found");
        return -1;
    }

    cJSON *json = collection_info_to_json(&info);
    char *str = cJSON_Print(json);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(json);

    return 0;
}

/**
 * @brief DELETE /collections/:id - 删除集合
 */
static int handle_delete_collection(RequestContext *ctx) {
    const char *id = request_get_path_param(ctx, 0);
    if (!id || strlen(id) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Collection ID required");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    int rc = vector_api_drop_collection(api, id);
    if (rc != 0) {
        response_error(ctx, HTTP_NOT_FOUND, "NOT_FOUND",
                      "Collection not found");
        return -1;
    }

    /* 同步删除 BM25 索引 */
    ctx_bm25_drop(ctx, id);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "message", "Collection deleted");
    cJSON_AddStringToObject(json, "id", id);

    char *str = cJSON_Print(json);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(json);

    return 0;
}

/* ========================================================================
 * 向量操作接口
 * ======================================================================== */

/**
 * @brief POST /collections/:id/vectors - 批量插入向量
 */
static int handle_insert_vectors(RequestContext *ctx) {
    const char *collection = request_get_path_param(ctx, 0);
    if (!collection || strlen(collection) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Collection ID required");
        return -1;
    }

    const char *body = request_get_body(ctx);
    if (!body || strlen(body) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_REQUEST", "Empty request body");
        return -1;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        response_error(ctx, HTTP_BAD_REQUEST, "PARSE_ERROR", "Invalid JSON");
        return -1;
    }

    cJSON *vectors = cJSON_GetObjectItem(root, "vectors");
    if (!vectors || !cJSON_IsArray(vectors)) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                      "Missing 'vectors' array field");
        return -1;
    }

    int n = cJSON_GetArraySize(vectors);
    if (n <= 0) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                      "vectors array is empty");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    /* 解析每个向量项：[{ "id": "doc1", "vector": [0.1, 0.2, ...] }, ...]
     * 也支持简写：[[0.1, 0.2, ...], ...] —— 无 ID 自动分配 */
    int64_t *ids = (int64_t *)calloc(n, sizeof(int64_t));
    float **vec_data = (float **)calloc(n, sizeof(float *));
    int32_t *vec_dims = (int32_t *)calloc(n, sizeof(int32_t));
    bool *has_id = (bool *)calloc(n, sizeof(bool));
    int valid_count = 0;

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(vectors, i);
        if (!item) continue;

        cJSON *id_node = cJSON_GetObjectItem(item, "id");
        cJSON *vec_node = cJSON_GetObjectItem(item, "vector");

        if (!vec_node || !cJSON_IsArray(vec_node)) continue;

        if (id_node && (cJSON_IsString(id_node) || cJSON_IsNumber(id_node))) {
            if (cJSON_IsString(id_node)) {
                /* 字符串 ID：暂时用哈希转 int64（VectorAPI 用 int64） */
                const char *s = id_node->valuestring;
                uint64_t h = 14695981039346656037ULL;
                for (const char *p = s; *p; p++) {
                    h ^= (uint8_t)*p;
                    h *= 1099511628211ULL;
                }
                ids[valid_count] = (int64_t)h;
            } else {
                ids[valid_count] = (int64_t)id_node->valuedouble;
            }
            has_id[valid_count] = true;
        }

        int dim = cJSON_GetArraySize(vec_node);
        vec_dims[valid_count] = dim;
        vec_data[valid_count] = (float *)malloc(dim * sizeof(float));
        for (int j = 0; j < dim; j++) {
            cJSON *v = cJSON_GetArrayItem(vec_node, j);
            vec_data[valid_count][j] = v && cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
        }
        valid_count++;
    }

    if (valid_count == 0) {
        free(ids); free(vec_data); free(vec_dims); free(has_id);
        cJSON_Delete(root);
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                      "No valid vectors in array");
        return -1;
    }

    /* 转换 has_id 为 NULL 数组（让 API 自动分配） */
    int64_t *ids_for_api = NULL;
    int64_t *auto_ids_buf = (int64_t *)calloc(valid_count, sizeof(int64_t));
    bool any_no_id = false;
    for (int i = 0; i < valid_count; i++) {
        if (!has_id[i]) { any_no_id = true; break; }
    }
    if (any_no_id) {
        ids_for_api = NULL;  /* VectorAPI 自动分配 */
    } else {
        ids_for_api = ids;
    }

    VectorInsertParams params = {0};
    params.collection = collection;
    params.n = valid_count;
    params.ids = ids_for_api;
    params.vectors = vec_data;
    params.metadata = NULL;
    params.metadata_sizes = NULL;

    int64_t *out_ids = auto_ids_buf;
    int inserted = vector_api_insert(api, &params, out_ids);

    /* 清理 */
    for (int i = 0; i < valid_count; i++) free(vec_data[i]);
    free(vec_data); free(vec_dims); free(has_id);
    free(ids); free(auto_ids_buf);

    if (inserted < 0) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_INTERNAL_ERROR, "INSERT_FAILED",
                      vector_api_error(api));
        return -1;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "inserted_count", inserted);
    cJSON_AddStringToObject(result, "status", "success");

    char *str = cJSON_Print(result);
    response_set_json(ctx, str);
    response_set_status(ctx, HTTP_CREATED);
    free(str);
    cJSON_Delete(result);
    cJSON_Delete(root);

    return 0;
}

/**
 * @brief GET /collections/:id/vectors/:vid - 获取向量
 *
 * 当前简化实现：返回向量元信息与维度，不返回完整向量数据（VectorAPI
 * 当前未提供按 ID 获取完整 float[] 的接口）。
 */
static int handle_get_vector(RequestContext *ctx) {
    const char *vid = request_get_path_param(ctx, 1);
    if (!vid || strlen(vid) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Vector ID required");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    /* 计算字符串 ID 对应的 int64（与 insert 时一致） */
    int64_t numeric_id;
    if (sscanf(vid, "%lld", (long long *)&numeric_id) != 1) {
        uint64_t h = 14695981039346656037ULL;
        for (const char *p = vid; *p; p++) {
            h ^= (uint8_t)*p;
            h *= 1099511628211ULL;
        }
        numeric_id = (int64_t)h;
    }

    const char *collection = request_get_path_param(ctx, 0);
    int32_t size = vector_api_size(api, collection);
    if (size < 0) {
        response_error(ctx, HTTP_NOT_FOUND, "NOT_FOUND", "Collection not found");
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", vid);
    cJSON_AddNumberToObject(json, "numeric_id", (double)numeric_id);
    cJSON_AddStringToObject(json, "collection", collection);
    cJSON_AddStringToObject(json, "status", "ok");

    char *str = cJSON_Print(json);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(json);

    return 0;
}

/**
 * @brief PUT /collections/:id/vectors/:vid - 更新向量
 *
 * 当前简化实现：VectorAPI 暂无 update 接口，返回 NOT_IMPLEMENTED 提示。
 */
static int handle_update_vector(RequestContext *ctx) {
    const char *vid = request_get_path_param(ctx, 1);
    if (!vid || strlen(vid) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Vector ID required");
        return -1;
    }

    response_error(ctx, HTTP_NOT_FOUND, "NOT_IMPLEMENTED",
                  "vector_api_update not yet exposed");
    return -1;
}

/**
 * @brief DELETE /collections/:id/vectors/:vid - 删除向量
 */
static int handle_delete_vector(RequestContext *ctx) {
    const char *vid = request_get_path_param(ctx, 1);
    if (!vid || strlen(vid) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Vector ID required");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    /* 字符串 ID → int64（与 insert 时的 FNV-1a 哈希一致） */
    int64_t numeric_id;
    if (sscanf(vid, "%lld", (long long *)&numeric_id) != 1) {
        uint64_t h = 14695981039346656037ULL;
        for (const char *p = vid; *p; p++) {
            h ^= (uint8_t)*p;
            h *= 1099511628211ULL;
        }
        numeric_id = (int64_t)h;
    }

    const char *collection = request_get_path_param(ctx, 0);
    int deleted = vector_api_delete(api, collection, &numeric_id, 1);
    if (deleted < 0) {
        response_error(ctx, HTTP_INTERNAL_ERROR, "DELETE_FAILED",
                      vector_api_error(api));
        return -1;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "message", "Vector deleted");
    cJSON_AddStringToObject(result, "id", vid);
    cJSON_AddNumberToObject(result, "deleted_count", deleted);

    char *str = cJSON_Print(result);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(result);

    return 0;
}

/* ========================================================================
 * BM25 文本搜索接口
 *
 * POST /collections/:id/text-search
 *   body: { "action": "add", "id": "doc1", "text": "..." }
 *   body: { "action": "delete", "id": "doc1" }
 *   body: { "query": "...", "top_k": 5 }
 * ======================================================================== */

static int handle_text_search(RequestContext *ctx) {
    const char *collection = request_get_path_param(ctx, 0);
    if (!collection || strlen(collection) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Collection ID required");
        return -1;
    }

    const char *body = request_get_body(ctx);
    if (!body || strlen(body) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_REQUEST", "Empty request body");
        return -1;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        response_error(ctx, HTTP_BAD_REQUEST, "PARSE_ERROR", "Invalid JSON");
        return -1;
    }

    bm25_t *idx = ctx_bm25_ensure(ctx, collection);

    cJSON *action = cJSON_GetObjectItem(root, "action");
    cJSON *query = cJSON_GetObjectItem(root, "query");
    cJSON *id_node = cJSON_GetObjectItem(root, "id");
    cJSON *text_node = cJSON_GetObjectItem(root, "text");
    cJSON *top_k = cJSON_GetObjectItem(root, "top_k");

    int k = top_k && cJSON_IsNumber(top_k) ? top_k->valueint : 5;
    if (k <= 0) k = 5;

    cJSON *response = cJSON_CreateObject();
    int rc = 0;

    /* ===== action=add ===== */
    if (action && cJSON_IsString(action) &&
        strcmp(action->valuestring, "add") == 0) {
        if (!id_node || !text_node || !cJSON_IsString(text_node)) {
            response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                          "add requires 'id' and 'text'");
            cJSON_Delete(response);
            cJSON_Delete(root);
            return -1;
        }
        const char *text = text_node->valuestring;
        int32_t added = bm25_index_add_text(idx, text);
        if (added < 0) {
            response_error(ctx, HTTP_INTERNAL_ERROR, "ADD_FAILED",
                          "bm25_index_add_text failed");
            cJSON_Delete(response);
            cJSON_Delete(root);
            return -1;
        }
        cJSON_AddStringToObject(response, "status", "added");
        cJSON_AddStringToObject(response, "id", id_node->valuestring);
        cJSON_AddNumberToObject(response, "doc_id", added);
    }
    /* ===== action=delete ===== */
    else if (action && cJSON_IsString(action) &&
             strcmp(action->valuestring, "delete") == 0) {
        if (!id_node || !cJSON_IsString(id_node)) {
            response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                          "delete requires 'id'");
            cJSON_Delete(response);
            cJSON_Delete(root);
            return -1;
        }
        /* 用 BM25 doc_id 的字符串哈希转 int32（与 insert 时一致） */
        int32_t doc_id = 0;
        for (const char *p = id_node->valuestring; *p; p++) {
            doc_id = doc_id * 31 + (uint8_t)*p;
        }
        int32_t deleted = bm25_delete_document(idx, doc_id);
        cJSON_AddStringToObject(response, "status", "deleted");
        cJSON_AddStringToObject(response, "id", id_node->valuestring);
        cJSON_AddNumberToObject(response, "deleted_count", deleted);
    }
    /* ===== query 搜索 ===== */
    else if (query && cJSON_IsString(query)) {
        const char *qstr = query->valuestring;
        if (strlen(qstr) == 0) {
            response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                          "query is empty");
            cJSON_Delete(response);
            cJSON_Delete(root);
            return -1;
        }

        float *scores = (float *)calloc(k, sizeof(float));
        int32_t *doc_ids = (int32_t *)calloc(k, sizeof(int32_t));
        int32_t found = bm25_index_search_text(idx, qstr, k, scores, doc_ids);
        cJSON *results_arr = cJSON_CreateArray();
        for (int i = 0; i < found; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", (double)doc_ids[i]);
            cJSON_AddNumberToObject(item, "doc_id", (double)doc_ids[i]);
            cJSON_AddNumberToObject(item, "score", scores[i]);
            cJSON_AddStringToObject(item, "text", "");
            cJSON_AddItemToArray(results_arr, item);
        }
        free(scores);
        free(doc_ids);

        cJSON_AddItemToObject(response, "results", results_arr);
        cJSON_AddNumberToObject(response, "count", found);
        cJSON_AddStringToObject(response, "status", "ok");
    }
    else {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                      "Need 'action' (add|delete) or 'query'");
        cJSON_Delete(response);
        cJSON_Delete(root);
        return -1;
    }

    char *str = cJSON_Print(response);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    (void)rc;
    return 0;
}

/* ========================================================================
 * 搜索接口
 * ======================================================================== */

/**
 * @brief POST /collections/:id/search - 向量搜索
 */
static int handle_search(RequestContext *ctx) {
    const char *collection = request_get_path_param(ctx, 0);
    if (!collection || strlen(collection) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_ID", "Collection ID required");
        return -1;
    }

    const char *body = request_get_body(ctx);
    if (!body || strlen(body) == 0) {
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_REQUEST", "Empty request body");
        return -1;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        response_error(ctx, HTTP_BAD_REQUEST, "PARSE_ERROR", "Invalid JSON");
        return -1;
    }

    cJSON *vector = cJSON_GetObjectItem(root, "vector");
    cJSON *top_k = cJSON_GetObjectItem(root, "top_k");

    int k = top_k && cJSON_IsNumber(top_k) ? top_k->valueint : 10;
    if (k <= 0) k = 10;

    if (!vector || !cJSON_IsArray(vector)) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_BAD_REQUEST, "INVALID_PARAMS",
                      "Missing 'vector' array field");
        return -1;
    }

    VectorAPI *api = ctx_vector_api(ctx);
    if (!api) {
        cJSON_Delete(root);
        response_error(ctx, HTTP_INTERNAL_ERROR, "NO_API", "VectorAPI not configured");
        return -1;
    }

    int dim = cJSON_GetArraySize(vector);
    float *query = (float *)malloc(dim * sizeof(float));
    for (int i = 0; i < dim; i++) {
        cJSON *v = cJSON_GetArrayItem(vector, i);
        query[i] = v && cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    }

    VectorSearchParams params = {0};
    params.collection = collection;
    params.query = query;
    params.top_k = k;
    params.radius = 0;
    params.with_distance = true;
    params.with_metadata = false;
    params.filter = NULL;

    int64_t t0 = (int64_t)time(NULL) * 1000;
    VectorSearchResults *results = vector_api_search(api, &params);
    int64_t took_ms = (int64_t)time(NULL) * 1000 - t0;
    free(query);

    cJSON *response = cJSON_CreateObject();
    cJSON *results_arr = cJSON_CreateArray();

    if (results) {
        int32_t cnt = vector_api_results_count(results);
        for (int32_t i = 0; i < cnt; i++) {
            const VectorSearchResult *r = vector_api_results_get(results, i);
            if (!r) continue;
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", (double)r->id);
            cJSON_AddNumberToObject(item, "score", r->distance);
            cJSON_AddNumberToObject(item, "distance", r->distance);
            cJSON_AddItemToArray(results_arr, item);
        }
        vector_api_results_free(results);
    }

    cJSON_AddItemToObject(response, "results", results_arr);
    cJSON_AddNumberToObject(response, "took_ms", took_ms);

    char *str = cJSON_Print(response);
    response_set_json(ctx, str);
    free(str);
    cJSON_Delete(response);
    cJSON_Delete(root);

    return 0;
}

/* ========================================================================
 * 处理器注册函数
 * ======================================================================== */

void rest_api_register_handlers(RestServer *server) {
    // 健康检查
    rest_server_register_handler(server, HTTP_GET, "/health", handle_health);
    rest_server_register_handler(server, HTTP_GET, "/ready", handle_ready);
    rest_server_register_handler(server, HTTP_GET, "/live", handle_live);
    rest_server_register_handler(server, HTTP_GET, "/metrics", handle_metrics);

    // 集合管理
    rest_server_register_handler(server, HTTP_POST, "/collections", handle_create_collection);
    rest_server_register_handler(server, HTTP_GET, "/collections", handle_list_collections);
    rest_server_register_handler(server, HTTP_GET, "/collections/:id", handle_get_collection);
    rest_server_register_handler(server, HTTP_DELETE, "/collections/:id", handle_delete_collection);

    // 向量操作
    rest_server_register_handler(server, HTTP_POST, "/collections/:id/vectors", handle_insert_vectors);
    rest_server_register_handler(server, HTTP_GET, "/collections/:id/vectors/:vid", handle_get_vector);
    rest_server_register_handler(server, HTTP_PUT, "/collections/:id/vectors/:vid", handle_update_vector);
    rest_server_register_handler(server, HTTP_DELETE, "/collections/:id/vectors/:vid", handle_delete_vector);

    // 搜索
    rest_server_register_handler(server, HTTP_POST, "/collections/:id/search", handle_search);

    // BM25 文本搜索
    rest_server_register_handler(server, HTTP_POST, "/collections/:id/text-search", handle_text_search);

    // VDB 统一 API 端点（/vdb/ 前缀）
    rest_server_register_handler(server, HTTP_POST, "/vdb/collections", handle_create_collection);
    rest_server_register_handler(server, HTTP_GET, "/vdb/collections", handle_list_collections);
    rest_server_register_handler(server, HTTP_DELETE, "/vdb/collections/:name", handle_delete_collection);
    rest_server_register_handler(server, HTTP_POST, "/vdb/collections/:name/insert", handle_insert_vectors);
    rest_server_register_handler(server, HTTP_POST, "/vdb/collections/:name/query", handle_search);
    rest_server_register_handler(server, HTTP_DELETE, "/vdb/collections/:name/:id", handle_delete_vector);
}
