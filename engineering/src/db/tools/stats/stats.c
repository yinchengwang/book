/**
 * @file stats.c
 * @brief 统计收集器实现
 */

#include "db/tools/stats.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─────────────────────────────────────────────────────────────────
 * 内部结构
 * ───────────────────────────────────────────────────────────────── */

/** 最大跟踪的表数 */
#define MAX_TABLES 256

/** 最大跟踪的索引数 */
#define MAX_INDEXES 512

/** 跟踪的表信息 */
typedef struct TableEntry {
    bool        active;
    char        name[64];
    StatTable   stats;
} TableEntry;

/** 跟踪的索引信息 */
typedef struct IndexEntry {
    bool        active;
    char        name[64];
    StatIndex   stats;
} IndexEntry;

/** 统计收集器结构 */
struct StatsCollector_s {
    StatDatabase db_stats;           /* 数据库级统计 */
    TableEntry  tables[MAX_TABLES];  /* 表级统计 */
    int         table_count;         /* 实际表数 */
    IndexEntry  indexes[MAX_INDEXES]; /* 索引级统计 */
    int         index_count;         /* 实际索引数 */
    bool        initialized;
};

/* ─────────────────────────────────────────────────────────────────
 * API 实现
 * ───────────────────────────────────────────────────────────────── */

StatsCollector *stats_init(void)
{
    StatsCollector *sc = calloc(1, sizeof(StatsCollector));
    if (!sc) return NULL;

    sc->initialized = true;
    sc->db_stats.stats_reset = time(NULL);

    return sc;
}

void stats_shutdown(StatsCollector *sc)
{
    if (sc) free(sc);
}

int stats_get_database(StatsCollector *sc, StatDatabase *stat)
{
    if (!sc || !stat) return -1;
    *stat = sc->db_stats;
    return 0;
}

int stats_get_tables(StatsCollector *sc, const char *table_name,
                      StatTable *stat, int max_count, int *count)
{
    if (!sc || !stat || !count) return -1;

    *count = 0;

    for (int i = 0; i < sc->table_count && *count < max_count; i++) {
        if (!sc->tables[i].active) continue;

        if (table_name == NULL || strcmp(sc->tables[i].name, table_name) == 0) {
            stat[*count] = sc->tables[i].stats;
            (*count)++;
        }
    }

    return 0;
}

int stats_get_indexes(StatsCollector *sc, const char *index_name,
                       StatIndex *stat, int max_count, int *count)
{
    if (!sc || !stat || !count) return -1;

    *count = 0;

    for (int i = 0; i < sc->index_count && *count < max_count; i++) {
        if (!sc->indexes[i].active) continue;

        if (index_name == NULL || strcmp(sc->indexes[i].name, index_name) == 0) {
            stat[*count] = sc->indexes[i].stats;
            (*count)++;
        }
    }

    return 0;
}

void stats_reset(StatsCollector *sc)
{
    if (!sc) return;

    memset(&sc->db_stats, 0, sizeof(sc->db_stats));
    sc->db_stats.stats_reset = time(NULL);

    for (int i = 0; i < MAX_TABLES; i++) {
        sc->tables[i].active = false;
    }
    sc->table_count = 0;

    for (int i = 0; i < MAX_INDEXES; i++) {
        sc->indexes[i].active = false;
    }
    sc->index_count = 0;
}

/* ================================================================
 * 全局收集器（Task 3.4）
 * ================================================================ */

/** 全局统计收集器句柄 */
static StatsCollector *g_stats_collector = NULL;

void stats_set_collector(StatsCollector *sc)
{
    g_stats_collector = sc;
}

StatsCollector *stats_get_collector(void)
{
    return g_stats_collector;
}

/* ================================================================
 * 统计更新 API（Task 3.4）
 * ================================================================ */

/** 根据表名查找或创建表条目 */
static TableEntry *stats_find_or_create_table(StatsCollector *sc, const char *table_name)
{
    if (!sc || !table_name) return NULL;

    /* 查找现有条目 */
    for (int i = 0; i < sc->table_count; i++) {
        if (sc->tables[i].active && strcmp(sc->tables[i].name, table_name) == 0) {
            return &sc->tables[i];
        }
    }

    /* 创建新条目 */
    if (sc->table_count >= MAX_TABLES) return NULL;

    TableEntry *entry = &sc->tables[sc->table_count];
    entry->active = true;
    strncpy(entry->name, table_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    memset(&entry->stats, 0, sizeof(entry->stats));
    strncpy(entry->stats.relname, table_name, sizeof(entry->stats.relname) - 1);
    sc->table_count++;

    return entry;
}

int stats_update_table_insert(StatsCollector *sc, const char *table_name, int64_t delta)
{
    TableEntry *entry = stats_find_or_create_table(sc, table_name);
    if (!entry) return -1;

    entry->stats.n_tup_ins += delta;
    sc->db_stats.tup_inserted += delta;
    return 0;
}

int stats_update_table_update(StatsCollector *sc, const char *table_name, int64_t delta)
{
    TableEntry *entry = stats_find_or_create_table(sc, table_name);
    if (!entry) return -1;

    entry->stats.n_tup_upd += delta;
    sc->db_stats.tup_updated += delta;
    return 0;
}

int stats_update_table_delete(StatsCollector *sc, const char *table_name, int64_t delta)
{
    TableEntry *entry = stats_find_or_create_table(sc, table_name);
    if (!entry) return -1;

    entry->stats.n_tup_del += delta;
    sc->db_stats.tup_deleted += delta;
    return 0;
}

int stats_update_table_tuples(StatsCollector *sc, const char *table_name,
                               int64_t n_live_tup, int64_t n_dead_tup)
{
    TableEntry *entry = stats_find_or_create_table(sc, table_name);
    if (!entry) return -1;

    entry->stats.n_live_tup = n_live_tup;
    entry->stats.n_dead_tup = n_dead_tup;
    return 0;
}
