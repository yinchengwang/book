/**
 * @file aggregate.c
 * @brief 通用聚合框架实现
 *
 * 基于 SQLite 构建 GROUP BY + 聚合函数查询，解析结果填充结果集。
 */
#include "sdk/mmdb_aggregate.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

/* ======================================================================== */
/* 内部工具函数                                                             */
/* ======================================================================== */

/**
 * @brief 为聚合查询构建 SQL 语句
 *
 * 根据聚合表达式生成 SELECT ... GROUP BY ... 语句。
 * HISTOGRAM 用 CASE WHEN 实现 bucket 分配。
 */
static int build_agg_sql(const mmdb_collection_t* c,
                         const mmdb_aggregate_query_t* query,
                         const char* filter,
                         char* sql_buf, size_t buf_len,
                         char* count_sql, size_t count_buf_len) {
    char select_clause[2048] = {0};
    char group_clause[256] = {0};
    char where_clause[1024] = {0};
    char having_clause[512] = {0};
    size_t pos = 0;

    /* 构建 WHERE 子句 */
    if (filter && filter[0] != '\0') {
        snprintf(where_clause, sizeof(where_clause), " WHERE %s", filter);
    }

    /* 构建 SELECT 子句和 GROUP BY 子句 */
    if (query->group_by && query->group_by[0] != '\0') {
        pos += snprintf(select_clause + pos, sizeof(select_clause) - pos,
                        "SELECT \"%s\" AS _group_key", query->group_by);
        snprintf(group_clause, sizeof(group_clause), " GROUP BY \"%s\"",
                 query->group_by);
    } else {
        pos += snprintf(select_clause + pos, sizeof(select_clause) - pos,
                        "SELECT 'ALL' AS _group_key");
    }

    /* 逐个处理聚合表达式 */
    for (size_t i = 0; i < query->agg_count; i++) {
        const mmdb_agg_expr_t* expr = &query->aggs[i];
        const char* alias = expr->alias ? expr->alias : "agg_val";

        switch (expr->type) {
            case MMDB_AGG_COUNT:
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                ", COUNT(*) AS \"%s\"", alias);
                break;

            case MMDB_AGG_SUM:
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                ", SUM(\"%s\") AS \"%s\"",
                                expr->field, alias);
                break;

            case MMDB_AGG_AVG:
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                ", AVG(\"%s\") AS \"%s\"",
                                expr->field, alias);
                break;

            case MMDB_AGG_MIN:
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                ", MIN(\"%s\") AS \"%s\"",
                                expr->field, alias);
                break;

            case MMDB_AGG_MAX:
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                ", MAX(\"%s\") AS \"%s\"",
                                expr->field, alias);
                break;

            case MMDB_AGG_HISTOGRAM: {
                /* 用 CASE WHEN 实现直方图 bucket 分配 */
                double range = expr->bucket_max - expr->bucket_min;
                uint32_t bc = expr->bucket_count > 0 ? expr->bucket_count : 10;
                double bucket_size = (range > 0.0) ? (range / bc) : 1.0;
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                ", CASE");
                for (uint32_t b = 0; b < bc; b++) {
                    double lo = expr->bucket_min + b * bucket_size;
                    double hi = lo + bucket_size;
                    pos += snprintf(select_clause + pos,
                                    sizeof(select_clause) - pos,
                                    " WHEN \"%s\" >= %.6f AND \"%s\" < %.6f THEN %u",
                                    expr->field, lo, expr->field, hi, b);
                }
                /* 最后一个 bucket 包含上界 */
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                " WHEN \"%s\" >= %.6f AND \"%s\" <= %.6f THEN %u",
                                expr->field,
                                expr->bucket_min + (bc - 1) * bucket_size,
                                expr->field,
                                expr->bucket_max,
                                bc - 1);
                pos += snprintf(select_clause + pos,
                                sizeof(select_clause) - pos,
                                " ELSE -1 END AS \"%s\"", alias);
                break;
            }
        }
    }

    /* 计算总数 SQL（用于分页） */
    snprintf(count_sql, count_buf_len,
             "SELECT COUNT(*) FROM (SELECT 1 FROM \"%s\"%s%s)",
             c->name, where_clause, group_clause);

    /* 组合完整查询 */
    snprintf(sql_buf, buf_len,
             "%s FROM \"%s\"%s%s",
             select_clause, c->name, where_clause, group_clause);

    return 0;
}

/**
 * @brief 查找聚合表达式的索引（按别名匹配）
 */
static int find_agg_index(const mmdb_aggregate_query_t* query, const char* alias) {
    for (size_t i = 0; i < query->agg_count; i++) {
        if (query->aggs[i].alias && strcmp(query->aggs[i].alias, alias) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* ======================================================================== */
/* 公共 API                                                                  */
/* ======================================================================== */

int mmdb_aggregate(mmdb_collection_t* c, const mmdb_aggregate_query_t* query,
                   const char* filter, mmdb_aggregate_result_set_t** result) {
    if (!c || !query || !result) {
        return MMDB_ERR_INVALID;
    }

    if (query->agg_count == 0 || query->agg_count > 8) {
        return MMDB_ERR_INVALID;
    }

    *result = NULL;

    /* 构建 SQL */
    char sql[4096] = {0};
    char count_sql[1024] = {0};
    int rc = build_agg_sql(c, query, filter, sql, sizeof(sql),
                           count_sql, sizeof(count_sql));
    if (rc != 0) return rc;

    /* 查询总组数（用于分页） */
    sqlite3_stmt* count_stmt = mmdb_sqlite_prepare(c->sdb, count_sql,
                                                   NULL, 0);
    uint64_t total_count = 0;
    if (count_stmt) {
        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
            total_count = (uint64_t)sqlite3_column_int64(count_stmt, 0);
        }
        sqlite3_finalize(count_stmt);
    }

    /* 添加分页 LIMIT/OFFSET */
    char final_sql[5120] = {0};
    if (query->limit > 0) {
        snprintf(final_sql, sizeof(final_sql), "%s LIMIT %u OFFSET %u",
                 sql, query->limit, query->offset);
    } else {
        snprintf(final_sql, sizeof(final_sql), "%s", sql);
    }

    /* 执行查询 */
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, final_sql, NULL, 0);
    if (!stmt) {
        return MMDB_ERR_INTERNAL;
    }

    /* 分配结果集 */
    mmdb_aggregate_result_set_t* rs = (mmdb_aggregate_result_set_t*)calloc(
        1, sizeof(mmdb_aggregate_result_set_t));
    if (!rs) {
        sqlite3_finalize(stmt);
        return MMDB_ERR_NOMEM;
    }

    /* 预分配 groups 数组（初始 64，可增长） */
    size_t capacity = 64;
    rs->groups = (mmdb_aggregate_result_t*)calloc(
        capacity, sizeof(mmdb_aggregate_result_t));
    if (!rs->groups) {
        sqlite3_finalize(stmt);
        free(rs);
        return MMDB_ERR_NOMEM;
    }

    /* 逐行读取结果 */
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        /* 扩容检查 */
        if (rs->group_count >= capacity) {
            capacity *= 2;
            mmdb_aggregate_result_t* new_groups =
                (mmdb_aggregate_result_t*)realloc(
                    rs->groups, capacity * sizeof(mmdb_aggregate_result_t));
            if (!new_groups) {
                sqlite3_finalize(stmt);
                mmdb_aggregate_result_free(rs);
                return MMDB_ERR_NOMEM;
            }
            rs->groups = new_groups;
            memset(rs->groups + rs->group_count, 0,
                   (capacity - rs->group_count) * sizeof(mmdb_aggregate_result_t));
        }

        mmdb_aggregate_result_t* grp = &rs->groups[rs->group_count];
        memset(grp, 0, sizeof(*grp));

        /* 提取 _group_key */
        const char* key = (const char*)sqlite3_column_text(stmt, 0);
        if (key) {
            snprintf(grp->key, sizeof(grp->key), "%s", key);
        }

        /* 提取各聚合值 */
        for (size_t i = 0; i < query->agg_count; i++) {
            const mmdb_agg_expr_t* expr = &query->aggs[i];
            const char* alias = expr->alias ? expr->alias : "agg_val";
            int col_idx = find_agg_index(query, alias);
            if (col_idx < 0) continue;
            /* SQLite 列索引 = col_idx + 1（跳过 _group_key） */
            int sqlite_col = col_idx + 1;

            switch (expr->type) {
                case MMDB_AGG_COUNT:
                    grp->count = (uint64_t)sqlite3_column_int64(stmt, sqlite_col);
                    break;
                case MMDB_AGG_SUM:
                    grp->sum = sqlite3_column_double(stmt, sqlite_col);
                    break;
                case MMDB_AGG_AVG:
                    grp->avg = sqlite3_column_double(stmt, sqlite_col);
                    break;
                case MMDB_AGG_MIN:
                    grp->min = sqlite3_column_double(stmt, sqlite_col);
                    break;
                case MMDB_AGG_MAX:
                    grp->max = sqlite3_column_double(stmt, sqlite_col);
                    break;
                case MMDB_AGG_HISTOGRAM: {
                    uint32_t bc = expr->bucket_count > 0 ? expr->bucket_count : 10;
                    if (!grp->histogram_buckets) {
                        grp->histogram_buckets = (uint32_t*)calloc(bc, sizeof(uint32_t));
                        grp->histogram_bucket_count = bc;
                    }
                    int bucket_idx = sqlite3_column_int(stmt, sqlite_col);
                    if (bucket_idx >= 0 && (uint32_t)bucket_idx < bc) {
                        grp->histogram_buckets[bucket_idx]++;
                    }
                    break;
                }
            }
        }

        rs->group_count++;
    }

    sqlite3_finalize(stmt);

    /* 填充分页元信息 */
    rs->total_count = total_count;
    rs->has_more = (query->limit > 0 &&
                    (query->offset + query->limit) < total_count);

    *result = rs;
    return MMDB_OK;
}

void mmdb_aggregate_result_free(mmdb_aggregate_result_set_t* result) {
    if (!result) return;
    for (size_t i = 0; i < result->group_count; i++) {
        free(result->groups[i].histogram_buckets);
    }
    free(result->groups);
    free(result);
}
