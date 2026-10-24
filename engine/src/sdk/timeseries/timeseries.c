/**
 * @file timeseries.c
 * @brief 时序数据 append / query / 滑动窗口聚合
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
/* 滑动窗口聚合内部实现                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief 执行单个聚合表达式的滑动窗口计算
 */
static int execute_single_agg(mmdb_collection_t* c,
                               const mmdb_ts_agg_expr_t* agg,
                               uint64_t start_time, uint64_t end_time,
                               bool fill_empty,
                               mmdb_aggregate_result_t** out_results,
                               size_t* out_count) {
    uint64_t window_ms = agg->window_ms;
    uint64_t slide_ms = agg->slide_ms;

    /* 默认步长等于窗口大小（不滑动） */
    if (slide_ms == 0) slide_ms = window_ms;

    /* 计算窗口数量 */
    uint64_t total_range = end_time - start_time;
    size_t max_windows = (size_t)((total_range + slide_ms - 1) / slide_ms);

    mmdb_aggregate_result_t* results = (mmdb_aggregate_result_t*)calloc(
        max_windows, sizeof(mmdb_aggregate_result_t));
    if (!results) return MMDB_ERR_NOMEM;

    size_t window_idx = 0;
    uint64_t win_start = start_time;

    while (win_start < end_time && window_idx < max_windows) {
        uint64_t win_end = win_start + window_ms;
        if (win_end > end_time) win_end = end_time;

        /* 构建 SQL 查询当前窗口 */
        char sql[512];
        const char* agg_func;
        switch (agg->type) {
            case MMDB_AGG_COUNT: agg_func = "COUNT(*)"; break;
            case MMDB_AGG_SUM:   agg_func = "SUM(value)"; break;
            case MMDB_AGG_AVG:   agg_func = "AVG(value)"; break;
            case MMDB_AGG_MIN:   agg_func = "MIN(value)"; break;
            case MMDB_AGG_MAX:   agg_func = "MAX(value)"; break;
            default: agg_func = "COUNT(*)"; break;
        }

        snprintf(sql, sizeof(sql),
            "SELECT %s FROM mmdb_ts_%s WHERE timestamp >= ? AND timestamp < ?;",
            agg_func, c->name);

        sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
        if (!stmt) {
            free(results);
            return MMDB_ERR_IO;
        }

        mmdb_sqlite_bind_int(stmt, 1, (int64_t)win_start);
        mmdb_sqlite_bind_int(stmt, 2, (int64_t)win_end);

        mmdb_aggregate_result_t* r = &results[window_idx];
        r->count = 1;  /* 每个窗口一行结果 */

        int has_data = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            /* SQLite 的聚合查询即使无匹配行也会返回一行；通过 NULL/COUNT=0 区分空窗口。 */
            if (agg->type == MMDB_AGG_COUNT) {
                r->count = (uint64_t)sqlite3_column_int64(stmt, 0);
                has_data = (r->count > 0);
            } else {
                has_data = (sqlite3_column_type(stmt, 0) != SQLITE_NULL);
                if (has_data) {
                    switch (agg->type) {
                        case MMDB_AGG_SUM:
                        case MMDB_AGG_AVG:
                        case MMDB_AGG_MIN:
                        case MMDB_AGG_MAX:
                            r->sum = sqlite3_column_double(stmt, 0);
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        if (!has_data && fill_empty) {
            /* 空窗口补零 */
            r->count = 0;
            r->sum = 0.0;
        } else if (!has_data) {
            /* 不补零，跳过此窗口 */
            sqlite3_finalize(stmt);
            win_start += slide_ms;
            continue;
        }

        sqlite3_finalize(stmt);

        /* 设置窗口时间范围描述 */
        snprintf(r->key, sizeof(r->key), "[%llu,%llu)",
                 (unsigned long long)win_start, (unsigned long long)win_end);

        window_idx++;
        win_start += slide_ms;
    }

    *out_results = results;
    *out_count = window_idx;
    return MMDB_OK;
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

    /* 写操作：获取写锁 */
    mmdb_rwlock_wrlock(c->coll_lock);
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
    mmdb_rwlock_unlock(c->coll_lock, 1);
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

    /* 读操作：获取读锁（支持并发读） */
    mmdb_rwlock_rdlock(c->coll_lock);

    if (is_agg) {
        /* 聚合模式：返回单行结果 */
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out->count = 1;
            out->items = (mmdb_result_item_t*)calloc(1, sizeof(mmdb_result_item_t));
            if (!out->items) {
                sqlite3_finalize(stmt);
                mmdb_rwlock_unlock(c->coll_lock, 0);
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
            mmdb_rwlock_unlock(c->coll_lock, 0);
            return MMDB_ERR_NOMEM;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (out->count >= cap) {
                cap *= 2;
                out->items = (mmdb_result_item_t*)realloc(
                    out->items, sizeof(mmdb_result_item_t) * cap);
                if (!out->items) {
                    sqlite3_finalize(stmt);
                    mmdb_rwlock_unlock(c->coll_lock, 0);
                    return MMDB_ERR_NOMEM;
                }
            }

            mmdb_result_item_t* it = &out->items[out->count];
            /* 用 id 字段存储时间戳（int64 → BLOB） */
            int64_t ts = sqlite3_column_int64(stmt, 0);
            it->id = (uint8_t*)malloc(sizeof(int64_t));
            if (!it->id) {
                sqlite3_finalize(stmt);
                mmdb_rwlock_unlock(c->coll_lock, 0);
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
    mmdb_rwlock_unlock(c->coll_lock, 0);
    return MMDB_OK;
}

int mmdb_ts_aggregate(mmdb_collection_t* c, const mmdb_ts_aggregate_query_t* query,
                      mmdb_aggregate_result_set_t** result) {
    if (!c || !query || !result) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_TIMESERIES) return MMDB_ERR_INVALID;
    if (query->agg_count == 0 || query->agg_count > 4) return MMDB_ERR_INVALID;

    /* 确保表存在 */
    int rc = ensure_ts_table(c);
    if (rc != MMDB_OK) return rc;

    /* 分配结果集 */
    mmdb_aggregate_result_set_t* rs = (mmdb_aggregate_result_set_t*)calloc(
        1, sizeof(mmdb_aggregate_result_set_t));
    if (!rs) return MMDB_ERR_NOMEM;

    /* 合并所有聚合表达式的结果 */
    size_t total_groups = 0;
    mmdb_aggregate_result_t* all_groups = NULL;

    for (size_t i = 0; i < query->agg_count; i++) {
        const mmdb_ts_agg_expr_t* agg = &query->aggs[i];

        mmdb_aggregate_result_t* groups = NULL;
        size_t count = 0;

        rc = execute_single_agg(c, agg, query->start_time, query->end_time,
                                query->fill_empty, &groups, &count);
        if (rc != MMDB_OK) {
            free(rs);
            return rc;
        }

        /* 合并到总结果 */
        mmdb_aggregate_result_t* new_groups = (mmdb_aggregate_result_t*)realloc(
            all_groups, (total_groups + count) * sizeof(mmdb_aggregate_result_t));
        if (!new_groups) {
            free(groups);
            free(all_groups);
            free(rs);
            return MMDB_ERR_NOMEM;
        }

        all_groups = new_groups;
        memcpy(all_groups + total_groups, groups, count * sizeof(mmdb_aggregate_result_t));
        total_groups += count;
        free(groups);
    }

    rs->groups = all_groups;
    rs->group_count = total_groups;
    rs->total_count = total_groups;
    rs->has_more = false;

    *result = rs;
    return MMDB_OK;
}