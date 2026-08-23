/**
 * @file timeseries.c
 * @brief 时序数据 append / query
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk/mmdb.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/filter_parser.h"

#include <sqlite3.h>

/**
 * @brief 为时序查询生成 SQL 语句（内部声明）
 */
int mmdb_timeseries_build_query_sql(mmdb_collection_t* c,
                                     const mmdb_ts_query_t* q,
                                     char** out_sql, int* out_agg);

/* ------------------------------------------------------------------ */
/* 内部辅助：确保数据表存在                                              */
/* ------------------------------------------------------------------ */

static int ensure_ts_table(mmdb_collection_t* c) {
    if (!c || c->model != MMDB_MODEL_TIMESERIES) return MMDB_ERR_INVALID;

    /* 检查表是否已存在 */
    char check_sql[256];
    snprintf(check_sql, sizeof(check_sql),
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='mmdb_ts_%s';",
        c->name);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, check_sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    if (exists) return MMDB_OK;

    /* 创建时序数据表 */
    char ddl[256];
    snprintf(ddl, sizeof(ddl),
        "CREATE TABLE IF NOT EXISTS mmdb_ts_%s ("
        "  timestamp INTEGER NOT NULL,"
        "  value REAL NOT NULL,"
        "  tags TEXT,"
        "  PRIMARY KEY (timestamp)"
        ");",
        c->name);

    return mmdb_sqlite_exec(c->sdb, ddl);
}

/* ------------------------------------------------------------------ */
/* 公共 API                                                            */
/* ------------------------------------------------------------------ */

int mmdb_timeseries_append(mmdb_collection_t* c, const mmdb_datapoint_t* dp) {
    return mmdb_timeseries_append_batch(c, dp, 1);
}

int mmdb_timeseries_append_batch(mmdb_collection_t* c,
                                   const mmdb_datapoint_t* dps, size_t n) {
    if (!c || (!dps && n > 0)) return MMDB_ERR_INVALID;
    if (n == 0) return MMDB_OK;
    if (c->model != MMDB_MODEL_TIMESERIES) return MMDB_ERR_INVALID;

    /* 确保表存在 */
    int rc = ensure_ts_table(c);
    if (rc != MMDB_OK) return rc;

    /* 构建 INSERT SQL */
    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT OR REPLACE INTO mmdb_ts_%s (timestamp, value, tags) "
        "VALUES (?, ?, ?);",
        c->name);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    pthread_mutex_lock(c->coll_lock);
    int err = MMDB_OK;
    for (size_t i = 0; i < n; i++) {
        const mmdb_datapoint_t* dp = &dps[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        mmdb_sqlite_bind_int(stmt, 1, dp->timestamp);
        mmdb_sqlite_bind_double(stmt, 2, dp->value);
        if (dp->tags_json) {
            mmdb_sqlite_bind_text(stmt, 3, dp->tags_json);
        } else {
            sqlite3_bind_null(stmt, 3);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            err = MMDB_ERR_IO;
            break;
        }
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(c->coll_lock);
    return err;
}

int mmdb_timeseries_query(mmdb_collection_t* c, const mmdb_ts_query_t* q,
                           mmdb_result_t* out) {
    if (!c || !q || !out) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_TIMESERIES) return MMDB_ERR_INVALID;

    memset(out, 0, sizeof(*out));

    /* 确保表存在（空表也要能查询） */
    int rc = ensure_ts_table(c);
    if (rc != MMDB_OK) return rc;

    /* 构建 SQL */
    char* sql = NULL;
    int is_agg = 0;
    rc = mmdb_timeseries_build_query_sql(c, q, &sql, &is_agg);
    if (rc != MMDB_OK) return rc;

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    free(sql);
    if (!stmt) return MMDB_ERR_IO;

    /* 绑定参数 */
    mmdb_sqlite_bind_int(stmt, 1, q->start);
    mmdb_sqlite_bind_int(stmt, 2, q->end);

    /* 绑定 filter 参数（如果有） */
    /* TODO: 实现 filter 参数绑定 */

    pthread_mutex_lock(c->coll_lock);

    if (is_agg) {
        /* 聚合模式：返回单行结果 */
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out->count = 1;
            out->items = (mmdb_result_item_t*)calloc(1, sizeof(mmdb_result_item_t));
            if (!out->items) {
                sqlite3_finalize(stmt);
                pthread_mutex_unlock(c->coll_lock);
                return MMDB_ERR_NOMEM;
            }
            out->items[0].distance = (float)sqlite3_column_double(stmt, 0);
            /* id 用空字符串表示聚合结果 */
            out->items[0].id = (uint8_t*)strdup("agg");
            out->items[0].id_len = 3;
        }
    } else {
        /* 原始数据：先扫描再填充 */
        size_t cap = 16;
        out->items = (mmdb_result_item_t*)calloc(cap, sizeof(mmdb_result_item_t));
        if (!out->items) {
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(c->coll_lock);
            return MMDB_ERR_NOMEM;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (out->count >= cap) {
                cap *= 2;
                out->items = (mmdb_result_item_t*)realloc(
                    out->items, sizeof(mmdb_result_item_t) * cap);
                if (!out->items) {
                    sqlite3_finalize(stmt);
                    pthread_mutex_unlock(c->coll_lock);
                    return MMDB_ERR_NOMEM;
                }
            }

            mmdb_result_item_t* it = &out->items[out->count];
            /* 用 id 字段存储时间戳（int64 → BLOB） */
            int64_t ts = sqlite3_column_int64(stmt, 0);
            it->id = (uint8_t*)malloc(sizeof(int64_t));
            if (!it->id) {
                sqlite3_finalize(stmt);
                pthread_mutex_unlock(c->coll_lock);
                return MMDB_ERR_NOMEM;
            }
            memcpy(it->id, &ts, sizeof(int64_t));
            it->id_len = sizeof(int64_t);
            it->distance = (float)sqlite3_column_double(stmt, 1);
            const char* tags = (const char*)sqlite3_column_text(stmt, 2);
            it->metadata_json = tags ? strdup(tags) : NULL;
            it->text = NULL;
            out->count++;
        }
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(c->coll_lock);
    return MMDB_OK;
}