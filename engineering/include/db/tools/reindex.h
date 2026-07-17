/**
 * @file reindex.h
 * @brief 索引重建工具公共接口
 *
 * 提供 REINDEX 命令的索引重建功能，
 * 参考 PostgreSQL 的 REINDEX 实现。
 */
#ifndef DB_TOOLS_REINDEX_H
#define DB_TOOLS_REINDEX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * REINDEX 选项
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief REINDEX 命令选项
 */
typedef struct ReindexOptions {
    bool    verbose;        /* 详细输出 */
    bool    concurrently;   /* 并发重建（不阻塞写操作） */
    int     parallel_workers; /* 并行工作线程数 */
} ReindexOptions;

/* ─────────────────────────────────────────────────────────────────
 * REINDEX 范围类型
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief REINDEX 作用范围
 */
typedef enum ReindexScope {
    REINDEX_SCOPE_INDEX,     /* 重建单个索引 */
    REINDEX_SCOPE_TABLE,     /* 重建表的所有索引 */
    REINDEX_SCOPE_DATABASE,  /* 重建数据库的所有索引 */
    REINDEX_SCOPE_SYSTEM     /* 重建系统表索引 */
} ReindexScope;

/* ─────────────────────────────────────────────────────────────────
 * REINDEX 统计
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief REINDEX 执行统计
 */
typedef struct ReindexStats {
    int64_t num_indexes;        /* 处理的索引数 */
    int64_t num_pages;          /* 处理的页面数 */
    int64_t num_tuples;         /* 处理的元组数 */
    double  execution_time_ms;  /* 执行时间（毫秒） */
} ReindexStats;

/* ─────────────────────────────────────────────────────────────────
 * REINDEX API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取默认 REINDEX 选项
 */
ReindexOptions reindex_default_options(void);

/**
 * @brief 重建单个索引
 * @param index_name 索引名
 * @param options 选项
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int reindex_index(const char *index_name, ReindexOptions *options, ReindexStats *stats);

/**
 * @brief 重建表的所有索引
 * @param table_name 表名
 * @param options 选项
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int reindex_table(const char *table_name, ReindexOptions *options, ReindexStats *stats);

/**
 * @brief 重建数据库的所有索引
 * @param database_name 数据库名（NULL 表示当前数据库）
 * @param options 选项
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int reindex_database(const char *database_name, ReindexOptions *options, ReindexStats *stats);

/**
 * @brief 重建系统表索引
 * @param options 选项
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int reindex_system(ReindexOptions *options, ReindexStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_TOOLS_REINDEX_H */
