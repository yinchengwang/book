/**
 * @file stats.h
 * @brief 统计信息收集器公共接口
 *
 * 提供数据库运行时统计信息的收集和查询功能，
 * 参考 PostgreSQL 的统计信息收集器 (pg_stat_*)。
 */
#ifndef DB_TOOLS_STATS_H
#define DB_TOOLS_STATS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 统计信息结构
 * ───────────────────────────────────────────────────────────────── */

/** 数据库级统计 */
typedef struct {
    int64_t numbackends;       /* 当前连接数 */
    int64_t xact_commit;       /* 提交的事务数 */
    int64_t xact_rollback;     /* 回滚的事务数 */
    int64_t blks_read;         /* 磁盘块读取数 */
    int64_t blks_hit;          /* 缓冲区命中数 */
    int64_t tup_returned;      /* 返回的元组数 */
    int64_t tup_fetched;       /* 抓取的元组数 */
    int64_t tup_inserted;      /* 插入的元组数 */
    int64_t tup_updated;       /* 更新的元组数 */
    int64_t tup_deleted;       /* 删除的元组数 */
    time_t  stats_reset;       /* 统计重置时间 */
} StatDatabase;

/** 表级统计 */
typedef struct {
    char    relname[64];       /* 表名 */
    int64_t seq_scan;          /* 顺序扫描次数 */
    int64_t seq_tup_read;      /* 顺序扫描读取的元组数 */
    int64_t idx_scan;          /* 索引扫描次数 */
    int64_t idx_tup_fetch;     /* 索引扫描抓取的元组数 */
    int64_t n_tup_ins;         /* 插入元组数 */
    int64_t n_tup_upd;         /* 更新元组数 */
    int64_t n_tup_del;         /* 删除元组数 */
    int64_t n_live_tup;        /* 活元组数 */
    int64_t n_dead_tup;        /* 死元组数 */
    int64_t heap_blks_read;    /* 堆块读取数 */
    int64_t heap_blks_hit;     /* 堆块命中数 */
} StatTable;

/** 索引级统计 */
typedef struct {
    char    indexrelname[64];  /* 索引名 */
    char    relname[64];       /* 表名 */
    int64_t idx_scan;          /* 索引扫描次数 */
    int64_t idx_tup_read;      /* 索引扫描读取的元组数 */
    int64_t idx_tup_fetch;     /* 索引扫描抓取的元组数 */
    int64_t idx_blks_read;     /* 索引块读取数 */
    int64_t idx_blks_hit;      /* 索引块命中数 */
} StatIndex;

/* ─────────────────────────────────────────────────────────────────
 * 统计收集器 API
 * ───────────────────────────────────────────────────────────────── */

/** 统计收集器句柄 */
typedef struct StatsCollector_s StatsCollector;

/**
 * @brief 初始化统计收集器
 * @return 统计收集器句柄，NULL 表示失败
 */
StatsCollector *stats_init(void);

/**
 * @brief 关闭统计收集器
 * @param sc 收集器句柄
 */
void stats_shutdown(StatsCollector *sc);

/**
 * @brief 获取数据库级统计
 * @param sc 收集器句柄
 * @param[out] stat 统计信息输出
 * @return 0 成功，非 0 失败
 */
int stats_get_database(StatsCollector *sc, StatDatabase *stat);

/**
 * @brief 获取表级统计
 * @param sc 收集器句柄
 * @param table_name 表名，NULL 获取所有
 * @param[out] stat 统计信息输出
 * @param max_count 数组大小
 * @param[out] count 实际获取的数量
 * @return 0 成功，非 0 失败
 */
int stats_get_tables(StatsCollector *sc, const char *table_name,
                      StatTable *stat, int max_count, int *count);

/**
 * @brief 获取索引级统计
 * @param sc 收集器句柄
 * @param index_name 索引名，NULL 获取所有
 * @param[out] stat 统计信息输出
 * @param max_count 数组大小
 * @param[out] count 实际获取的数量
 * @return 0 成功，非 0 失败
 */
int stats_get_indexes(StatsCollector *sc, const char *index_name,
                       StatIndex *stat, int max_count, int *count);

/**
 * @brief 重置统计信息
 * @param sc 收集器句柄
 */
void stats_reset(StatsCollector *sc);

/* ─────────────────────────────────────────────────────────────────
 * 统计更新 API（Task 3.4）
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 设置全局统计收集器
 * @param sc 收集器句柄
 */
void stats_set_collector(StatsCollector *sc);

/**
 * @brief 获取全局统计收集器
 * @return 收集器句柄，NULL 表示未设置
 */
StatsCollector *stats_get_collector(void);

/**
 * @brief 更新表插入计数
 * @param sc 收集器句柄
 * @param table_name 表名
 * @param delta 插入行数
 * @return 0 成功，-1 失败（表不存在于统计表）
 */
int stats_update_table_insert(StatsCollector *sc, const char *table_name, int64_t delta);

/**
 * @brief 更新表更新计数
 * @param sc 收集器句柄
 * @param table_name 表名
 * @param delta 更新行数
 * @return 0 成功，-1 失败
 */
int stats_update_table_update(StatsCollector *sc, const char *table_name, int64_t delta);

/**
 * @brief 更新表删除计数
 * @param sc 收集器句柄
 * @param table_name 表名
 * @param delta 删除行数
 * @return 0 成功，-1 失败
 */
int stats_update_table_delete(StatsCollector *sc, const char *table_name, int64_t delta);

/**
 * @brief 更新表元组数（活/死元组）
 * @param sc 收集器句柄
 * @param table_name 表名
 * @param n_live_tup 活元组数
 * @param n_dead_tup 死元组数
 * @return 0 成功，-1 失败
 */
int stats_update_table_tuples(StatsCollector *sc, const char *table_name,
                               int64_t n_live_tup, int64_t n_dead_tup);

/* ─────────────────────────────────────────────────────────────────
 * 统计视图 API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 生成 stat_database 视图的 SQL 结果
 * @param sc 统计收集器句柄
 * @param[out] result SQL 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_database(StatsCollector *sc, char **result);

/**
 * @brief 生成 stat_database 视图的 JSON 结果
 * @param sc 统计收集器句柄
 * @param[out] result JSON 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_database_json(StatsCollector *sc, char **result);

/**
 * @brief 生成 stat_user_tables 视图的 SQL 结果
 * @param sc 统计收集器句柄
 * @param[out] result SQL 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_tables(StatsCollector *sc, char **result);

/**
 * @brief 生成 stat_user_tables 视图的 JSON 结果
 * @param sc 统计收集器句柄
 * @param[out] result JSON 结果字符串（需调用方 free）
 * @return 0 成功，非 0 失败
 */
int stat_view_tables_json(StatsCollector *sc, char **result);

#ifdef __cplusplus
}
#endif

#endif /* DB_TOOLS_STATS_H */
