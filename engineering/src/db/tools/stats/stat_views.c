/**
 * @file stat_views.c
 * @brief 统计视图实现
 *
 * 提供 SQL 可查询的统计视图生成功能，
 * 支持 pipe-separated 和 JSON 两种输出格式。
 */

#include "db/tools/stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────
 * stat_database 视图
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 生成 stat_database 视图的 SQL 结果
 * @param sc 统计收集器句柄
 * @param[out] result SQL 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_database(StatsCollector *sc, char **result)
{
    if (!sc || !result) return -1;

    StatDatabase db;
    if (stats_get_database(sc, &db) != 0) {
        return -1;
    }

    /* 计算缓冲命中率 */
    double hit_ratio = 0.0;
    if (db.blks_read + db.blks_hit > 0) {
        hit_ratio = (double)db.blks_hit / (db.blks_read + db.blks_hit);
    }

    /* 格式化输出 */
    char *buf = malloc(2048);
    if (!buf) return -1;

    snprintf(buf, 2048,
        "datid|datname|numbackends|xact_commit|xact_rollback|"
        "blks_read|blks_hit|hit_ratio|tup_returned|tup_fetched|"
        "tup_inserted|tup_updated|tup_deleted\n"
        "1|default_db|%lld|%lld|%lld|%lld|%lld|%.4f|%lld|%lld|%lld|%lld|%lld\n",
        (long long)db.numbackends, (long long)db.xact_commit,
        (long long)db.xact_rollback,
        (long long)db.blks_read, (long long)db.blks_hit, hit_ratio,
        (long long)db.tup_returned, (long long)db.tup_fetched,
        (long long)db.tup_inserted, (long long)db.tup_updated,
        (long long)db.tup_deleted);

    *result = buf;
    return 0;
}

/**
 * @brief 生成 stat_database 视图的 JSON 结果
 * @param sc 统计收集器句柄
 * @param[out] result JSON 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_database_json(StatsCollector *sc, char **result)
{
    if (!sc || !result) return -1;

    StatDatabase db;
    if (stats_get_database(sc, &db) != 0) {
        return -1;
    }

    double hit_ratio = 0.0;
    if (db.blks_read + db.blks_hit > 0) {
        hit_ratio = (double)db.blks_hit / (db.blks_read + db.blks_hit);
    }

    char *buf = malloc(2048);
    if (!buf) return -1;

    snprintf(buf, 2048,
        "{\n"
        "  \"datname\": \"default_db\",\n"
        "  \"numbackends\": %lld,\n"
        "  \"xact_commit\": %lld,\n"
        "  \"xact_rollback\": %lld,\n"
        "  \"blks_read\": %lld,\n"
        "  \"blks_hit\": %lld,\n"
        "  \"hit_ratio\": %.4f,\n"
        "  \"tup_returned\": %lld,\n"
        "  \"tup_fetched\": %lld,\n"
        "  \"tup_inserted\": %lld,\n"
        "  \"tup_updated\": %lld,\n"
        "  \"tup_deleted\": %lld\n"
        "}\n",
        (long long)db.numbackends, (long long)db.xact_commit,
        (long long)db.xact_rollback,
        (long long)db.blks_read, (long long)db.blks_hit, hit_ratio,
        (long long)db.tup_returned, (long long)db.tup_fetched,
        (long long)db.tup_inserted, (long long)db.tup_updated,
        (long long)db.tup_deleted);

    *result = buf;
    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * stat_user_tables 视图
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 生成 stat_user_tables 视图的 SQL 结果
 * @param sc 统计收集器句柄
 * @param[out] result SQL 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_tables(StatsCollector *sc, char **result)
{
    if (!sc || !result) return -1;

    StatTable tables[256];
    int count = 0;

    if (stats_get_tables(sc, NULL, tables, 256, &count) != 0) {
        return -1;
    }

    /* 计算缓冲区大小 */
    size_t buf_size = 1024 + count * 512;
    char *buf = malloc(buf_size);
    if (!buf) return -1;

    char *p = buf;
    p += snprintf(p, buf_size - (p - buf),
        "relid|relname|seq_scan|seq_tup_read|idx_scan|idx_tup_fetch|"
        "n_tup_ins|n_tup_upd|n_tup_del|n_live_tup|n_dead_tup|"
        "heap_blks_read|heap_blks_hit\n");

    for (int i = 0; i < count; i++) {
        p += snprintf(p, buf_size - (p - buf),
            "%d|%s|%lld|%lld|%lld|%lld|%lld|%lld|%lld|%lld|%lld|%lld|%lld\n",
            i + 1, tables[i].relname,
            (long long)tables[i].seq_scan, (long long)tables[i].seq_tup_read,
            (long long)tables[i].idx_scan, (long long)tables[i].idx_tup_fetch,
            (long long)tables[i].n_tup_ins, (long long)tables[i].n_tup_upd,
            (long long)tables[i].n_tup_del,
            (long long)tables[i].n_live_tup, (long long)tables[i].n_dead_tup,
            (long long)tables[i].heap_blks_read,
            (long long)tables[i].heap_blks_hit);
    }

    *result = buf;
    return 0;
}

/**
 * @brief 生成 stat_user_tables 视图的 JSON 结果
 * @param sc 统计收集器句柄
 * @param[out] result JSON 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_tables_json(StatsCollector *sc, char **result)
{
    if (!sc || !result) return -1;

    StatTable tables[256];
    int count = 0;

    if (stats_get_tables(sc, NULL, tables, 256, &count) != 0) {
        return -1;
    }

    size_t buf_size = 1024 + count * 1024;
    char *buf = malloc(buf_size);
    if (!buf) return -1;

    char *p = buf;
    p += snprintf(p, buf_size - (p - buf), "[\n");

    for (int i = 0; i < count; i++) {
        p += snprintf(p, buf_size - (p - buf),
            "  {\n"
            "    \"relname\": \"%s\",\n"
            "    \"seq_scan\": %lld,\n"
            "    \"seq_tup_read\": %lld,\n"
            "    \"idx_scan\": %lld,\n"
            "    \"idx_tup_fetch\": %lld,\n"
            "    \"n_tup_ins\": %lld,\n"
            "    \"n_tup_upd\": %lld,\n"
            "    \"n_tup_del\": %lld,\n"
            "    \"n_live_tup\": %lld,\n"
            "    \"n_dead_tup\": %lld\n"
            "  }%s\n",
            tables[i].relname,
            (long long)tables[i].seq_scan, (long long)tables[i].seq_tup_read,
            (long long)tables[i].idx_scan, (long long)tables[i].idx_tup_fetch,
            (long long)tables[i].n_tup_ins, (long long)tables[i].n_tup_upd,
            (long long)tables[i].n_tup_del,
            (long long)tables[i].n_live_tup, (long long)tables[i].n_dead_tup,
            (i < count - 1) ? "," : "");
    }

    p += snprintf(p, buf_size - (p - buf), "]\n");

    *result = buf;
    return 0;
}
