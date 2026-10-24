/**
 * @file timeseries_sql.c
 * @brief 时序 SQL 构造
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk/mmdb.h"
#include "sdk/impl/filter_parser.h"

#define TS_SQL_BUF 2048

/**
 * @brief 为时序查询生成 SQL 语句
 * @param c collection 指针
 * @param q 查询参数
 * @param out_sql 输出的 SQL 字符串（调用方需 free）
 * @param out_agg 输出标志：1 表示聚合查询，0 表示原始数据查询
 * @return MMDB_OK 成功，其他表示错误
 */
int mmdb_timeseries_build_query_sql(mmdb_collection_t* c,
                                     const mmdb_ts_query_t* q,
                                     char** out_sql, int* out_agg) {
    if (!c || !q || !out_sql || !out_agg) return MMDB_ERR_INVALID;

    *out_sql = NULL;
    *out_agg = 0;

    /* 构建过滤条件 */
    char filter[1024] = "";
    if (q->filter_json && q->filter_json[0]) {
        mmdb_filter_params_t fp;
        char* fsql = mmdb_filter_compile(q->filter_json, &fp);
        if (fsql && fsql[0]) {
            snprintf(filter, sizeof(filter), " AND %s", fsql);
        }
        if (fsql) free(fsql);
        mmdb_filter_params_free(&fp);
    }

    char* sql = (char*)malloc(TS_SQL_BUF);
    if (!sql) return MMDB_ERR_NOMEM;

    if (q->agg && q->agg[0]) {
        /* 聚合查询 */
        *out_agg = 1;
        const char* sqlfn = "AVG";
        if (strcmp(q->agg, "avg") == 0) sqlfn = "AVG";
        else if (strcmp(q->agg, "sum") == 0) sqlfn = "SUM";
        else if (strcmp(q->agg, "min") == 0) sqlfn = "MIN";
        else if (strcmp(q->agg, "max") == 0) sqlfn = "MAX";
        else if (strcmp(q->agg, "count") == 0) sqlfn = "COUNT";

        snprintf(sql, TS_SQL_BUF,
            "SELECT %s(value) AS agg FROM mmdb_ts_%s "
            "WHERE timestamp >= ? AND timestamp <= ?%s",
            sqlfn, c->name, filter);
    } else {
        /* 原始数据查询 */
        snprintf(sql, TS_SQL_BUF,
            "SELECT timestamp, value, tags FROM mmdb_ts_%s "
            "WHERE timestamp >= ? AND timestamp <= ?%s "
            "ORDER BY timestamp ASC",
            c->name, filter);
    }

    *out_sql = sql;
    return MMDB_OK;
}