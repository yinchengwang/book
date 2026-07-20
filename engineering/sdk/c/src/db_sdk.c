/**
 * @file db_sdk.c
 * @brief 数据库统一 C SDK 实现（Task 3.9 + 3.10）
 *
 * 封装 PlannerContext + executor_run + vdb_api 管线。
 * 提供 SQL 查询和向量操作的统一 C API。
 */

#include "db_sdk.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "db/kv.h"
#include "db/parser/sql/sql.h"
#include "db/sql/sql_planner.h"
#include "db/sql/sql_executor.h"
#include "db/vdb_api.h"

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 集合内部包装 */
typedef struct db_sdk_collection_s {
    char name[128];
    vdb_collection_t *vdb_coll;    /**< 底层 vdb 集合句柄 */
} db_sdk_collection_t;

struct db_sdk_s {
    kv_t *kv;                      /**< KV 存储句柄 */
    PlannerContext *planner_ctx;   /**< 计划器上下文 */
    void *executor;                /**< 执行器 */
    vdb_handle_t *vdb;             /**< 向量数据库句柄 */
    db_sdk_collection_t **collections; /**< 集合句柄缓存 */
    int32_t n_collections;
    int32_t capacity;
    char error_msg[512];           /**< 错误消息 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

static void set_error(db_sdk_t *db, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(db->error_msg, sizeof(db->error_msg), fmt, args);
    va_end(args);
}

/* ========================================================================
 * 数据库生命周期
 * ======================================================================== */

db_sdk_t *db_sdk_open(const char *path)
{
    if (!path) return NULL;

    db_sdk_t *db = (db_sdk_t *)calloc(1, sizeof(db_sdk_t));
    if (!db) return NULL;

    db->kv = kv_open(path);
    if (!db->kv) {
        free(db);
        return NULL;
    }

    db->planner_ctx = planner_create();
    if (!db->planner_ctx) {
        kv_close(db->kv);
        free(db);
        return NULL;
    }

    db->executor = executor_create();
    if (!db->executor) {
        planner_destroy(db->planner_ctx);
        kv_close(db->kv);
        free(db);
        return NULL;
    }

    /* 打开向量引擎 */
    db->vdb = vdb_open(path, NULL);
    if (!db->vdb) {
        executor_destroy(db->executor);
        planner_destroy(db->planner_ctx);
        kv_close(db->kv);
        free(db);
        return NULL;
    }

    db->collections = NULL;
    db->n_collections = 0;
    db->capacity = 0;

    return db;
}

int db_sdk_close(db_sdk_t *db)
{
    if (!db) return -1;
    if (db->vdb) vdb_close(db->vdb);
    if (db->executor) executor_destroy(db->executor);
    if (db->planner_ctx) planner_destroy(db->planner_ctx);
    if (db->kv) kv_close(db->kv);
    if (db->collections) {
        for (int32_t i = 0; i < db->n_collections; i++) {
            free(db->collections[i]);
        }
        free(db->collections);
    }
    free(db);
    return 0;
}

/* ========================================================================
 * SQL 执行
 * ======================================================================== */

db_sdk_result_t *db_sdk_query(db_sdk_t *db, const char *sql)
{
    if (!db || !sql) return NULL;

    db_sdk_result_t *result = (db_sdk_result_t *)calloc(1, sizeof(db_sdk_result_t));
    if (!result) return NULL;

    /* 解析 SQL */
    sql_node_t *node = sql_parse_one(sql);
    if (!node) {
        set_error(db, "SQL 解析失败");
        result->error_msg = strdup(db->error_msg);
        return result;
    }

    /* DDL 直接返回成功 */
    if (node->type == SQL_NODE_CREATE_TABLE || node->type == SQL_NODE_DROP_TABLE) {
        result->num_rows = 1;
        sql_node_free(node);
        return result;
    }

    /* 生成物理计划 */
    PhysPlan *plan = planner_plan(db->planner_ctx, node);
    if (!plan) {
        set_error(db, "无法生成执行计划");
        result->error_msg = strdup(db->error_msg);
        sql_node_free(node);
        return result;
    }

    /* 提取列名 */
    if (plan->targetlist) {
        result->num_cols = (int)plan->targetlist->length;
        result->columns = (char **)calloc(result->num_cols, sizeof(char *));
        if (result->columns) {
            int col = 0;
            ListCell *lc = plan->targetlist->head;
            while (lc && col < result->num_cols) {
                TargetEntry *te = (TargetEntry *)lc->data;
                if (te && te->resname) {
                    result->columns[col] = strdup(te->resname);
                }
                col++;
                lc = lc->next;
            }
        }
    }

    /* 执行 */
    executor_init(db->executor, plan, NULL);
    long count = executor_run(db->executor, plan,
                             (ScanDirection)ForwardScanDirection, 1000, NULL);
    executor_finish(db->executor);
    result->num_rows = (int)count;

    planner_free_physical_plan(plan);
    sql_node_free(node);

    return result;
}

void db_sdk_free_result(db_sdk_result_t *result)
{
    if (!result) return;
    if (result->columns) {
        for (int i = 0; i < result->num_cols; i++) {
            free(result->columns[i]);
        }
        free(result->columns);
    }
    if (result->rows) {
        for (int i = 0; i < result->num_rows; i++) {
            if (result->rows[i]) {
                for (int j = 0; j < result->num_cols; j++) {
                    free(result->rows[i][j]);
                }
                free(result->rows[i]);
            }
        }
        free(result->rows);
    }
    free(result->error_msg);
    free(result);
}

const char *db_sdk_error(db_sdk_t *db)
{
    return db ? db->error_msg : "NULL db";
}

/* ========================================================================
 * 向量操作：辅助函数
 * ======================================================================== */

static void db_sdk_set_error(db_sdk_t *db, const char *fmt, ...)
{
    if (!db) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(db->error_msg, sizeof(db->error_msg), fmt, args);
    va_end(args);
}

static db_sdk_collection_t *db_sdk_find_cached(db_sdk_t *db, const char *name)
{
    for (int32_t i = 0; i < db->n_collections; i++) {
        if (strcmp(db->collections[i]->name, name) == 0) {
            return db->collections[i];
        }
    }
    return NULL;
}

static int db_sdk_cache_add(db_sdk_t *db, db_sdk_collection_t *coll)
{
    if (db->n_collections >= db->capacity) {
        int32_t new_cap = db->capacity == 0 ? 16 : db->capacity * 2;
        db_sdk_collection_t **new_arr = (db_sdk_collection_t **)realloc(
            db->collections, new_cap * sizeof(db_sdk_collection_t *));
        if (!new_arr) return DB_SDK_ERR_NO_MEMORY;
        db->collections = new_arr;
        db->capacity = new_cap;
    }
    db->collections[db->n_collections++] = coll;
    return DB_SDK_OK;
}

/* ========================================================================
 * 向量操作：集合管理
 * ======================================================================== */

int db_sdk_create_collection(db_sdk_t *db, const char *name,
                             const db_sdk_collection_config_t *config)
{
    if (!db || !name || !config) return DB_SDK_ERR_INVALID_PARAM;

    /* 映射 vdb 配置 */
    vdb_collection_config_t vdb_cfg;
    memset(&vdb_cfg, 0, sizeof(vdb_cfg));
    strncpy(vdb_cfg.name, name, sizeof(vdb_cfg.name) - 1);
    vdb_cfg.dimension = config->dimension;
    vdb_cfg.index_type = (vdb_index_type_t)config->index_type;
    vdb_cfg.metric_type = (vdb_metric_type_t)config->metric_type;
    vdb_cfg.index_ef_search = config->index_ef_search;
    vdb_cfg.index_m = config->index_m;
    vdb_cfg.index_ef_construction = config->index_ef_construction;

    int rc = vdb_create_collection(db->vdb, name, &vdb_cfg);
    if (rc != VDB_OK) {
        db_sdk_set_error(db, "创建集合失败: %s", vdb_error(db->vdb));
        return DB_SDK_ERR_INTERNAL;
    }

    /* 缓存集合句柄 */
    db_sdk_collection_t *coll = (db_sdk_collection_t *)calloc(1, sizeof(*coll));
    if (!coll) return DB_SDK_ERR_NO_MEMORY;
    strncpy(coll->name, name, sizeof(coll->name) - 1);
    coll->vdb_coll = vdb_get_collection(db->vdb, name);
    if (!coll->vdb_coll) {
        free(coll);
        return DB_SDK_ERR_INTERNAL;
    }
    int ret = db_sdk_cache_add(db, coll);
    if (ret != DB_SDK_OK) {
        free(coll);
        return ret;
    }

    return DB_SDK_OK;
}

int db_sdk_drop_collection(db_sdk_t *db, const char *name)
{
    if (!db || !name) return DB_SDK_ERR_INVALID_PARAM;

    int rc = vdb_drop_collection(db->vdb, name);
    if (rc != VDB_OK) return DB_SDK_ERR_NOT_FOUND;

    /* 从缓存移除 */
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
    return DB_SDK_OK;
}

int db_sdk_list_collections(db_sdk_t *db, char ***names, int32_t *count)
{
    if (!db || !names || !count) return DB_SDK_ERR_INVALID_PARAM;
    return vdb_list_collections(db->vdb, names, count);
}

void db_sdk_free_names(char **names, int32_t count)
{
    vdb_free_names(names, count);
}

db_sdk_collection_t *db_sdk_get_collection(db_sdk_t *db, const char *name)
{
    if (!db || !name) return NULL;

    /* 查缓存 */
    db_sdk_collection_t *coll = db_sdk_find_cached(db, name);
    if (coll) return coll;

    /* 从 vdb 加载 */
    vdb_collection_t *vdb_coll = vdb_get_collection(db->vdb, name);
    if (!vdb_coll) {
        db_sdk_set_error(db, "集合不存在: %s", name);
        return NULL;
    }

    coll = (db_sdk_collection_t *)calloc(1, sizeof(*coll));
    if (!coll) return NULL;
    strncpy(coll->name, name, sizeof(coll->name) - 1);
    coll->vdb_coll = vdb_coll;
    db_sdk_cache_add(db, coll);
    return coll;
}

int db_sdk_collection_info(db_sdk_collection_t *coll,
                           db_sdk_collection_config_t *config)
{
    if (!coll || !config) return DB_SDK_ERR_INVALID_PARAM;

    vdb_collection_config_t vdb_cfg;
    memset(&vdb_cfg, 0, sizeof(vdb_cfg));
    int rc = vdb_collection_info(coll->vdb_coll, &vdb_cfg);
    if (rc != VDB_OK) return DB_SDK_ERR_NOT_FOUND;

    strncpy(config->name, vdb_cfg.name, sizeof(config->name) - 1);
    config->dimension = vdb_cfg.dimension;
    config->index_type = (db_sdk_index_type_t)vdb_cfg.index_type;
    config->metric_type = (db_sdk_metric_type_t)vdb_cfg.metric_type;
    return DB_SDK_OK;
}

int32_t db_sdk_size(db_sdk_collection_t *coll)
{
    if (!coll) return DB_SDK_ERR_INVALID_PARAM;
    return vdb_size(coll->vdb_coll);
}

/* ========================================================================
 * 向量操作：增删查
 * ======================================================================== */

int64_t db_sdk_insert(db_sdk_collection_t *coll, const float *vector,
                      int32_t dim, const void *metadata, int32_t metadata_size)
{
    if (!coll || !vector || dim <= 0) return DB_SDK_ERR_INVALID_PARAM;
    return vdb_insert(coll->vdb_coll, vector, dim, metadata, metadata_size);
}

db_sdk_results_t *db_sdk_search(db_sdk_collection_t *coll, const float *query,
                                 int32_t dim, const db_sdk_search_params_t *params)
{
    if (!coll || !query || dim <= 0) return NULL;

    vdb_search_params_t sp;
    memset(&sp, 0, sizeof(sp));
    if (params) {
        sp.top_k = params->top_k;
        sp.radius = params->radius;
        sp.with_distance = params->with_distance;
        sp.with_metadata = params->with_metadata;
        sp.offset = params->offset;
        sp.limit = params->limit;
    } else {
        sp.top_k = 10;
        sp.with_distance = true;
    }

    vdb_results_t *vdb_res = vdb_search(coll->vdb_coll, query, dim, &sp);
    if (!vdb_res) return NULL;

    db_sdk_results_t *results = (db_sdk_results_t *)calloc(1, sizeof(*results));
    if (!results) {
        vdb_results_free(vdb_res);
        return NULL;
    }

    results->count = vdb_res->count;
    results->capacity = vdb_res->capacity;
    results->total_time_us = vdb_res->total_time_us;

    if (vdb_res->count > 0) {
        results->items = (db_sdk_result_item_t *)calloc(
            vdb_res->count, sizeof(db_sdk_result_item_t));
        if (!results->items) {
            free(results);
            vdb_results_free(vdb_res);
            return NULL;
        }
        for (int32_t i = 0; i < vdb_res->count; i++) {
            results->items[i].id = vdb_res->items[i].id;
            results->items[i].distance = vdb_res->items[i].distance;
            results->items[i].score = vdb_res->items[i].score;
            if (vdb_res->items[i].metadata && vdb_res->items[i].metadata_size > 0) {
                results->items[i].metadata = malloc(vdb_res->items[i].metadata_size);
                memcpy(results->items[i].metadata, vdb_res->items[i].metadata,
                       vdb_res->items[i].metadata_size);
                results->items[i].metadata_size = vdb_res->items[i].metadata_size;
            }
        }
    }

    vdb_results_free(vdb_res);
    return results;
}

int db_sdk_delete(db_sdk_collection_t *coll, int64_t id)
{
    if (!coll) return DB_SDK_ERR_INVALID_PARAM;
    return vdb_delete(coll->vdb_coll, id);
}

void db_sdk_results_free(db_sdk_results_t *results)
{
    if (!results) return;
    for (int32_t i = 0; i < results->count; i++) {
        free(results->items[i].metadata);
    }
    free(results->items);
    free(results);
}