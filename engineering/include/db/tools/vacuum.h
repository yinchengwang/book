/**
 * @file vacuum.h
 * @brief 空间回收和统计更新工具公共接口
 *
 * 提供 VACUUM、VACUUM FULL、ANALYZE 等功能，
 * 参考 PostgreSQL 的 vacuum 实现。
 */
#ifndef DB_TOOLS_VACUUM_H
#define DB_TOOLS_VACUUM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * VACUUM 选项
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief VACUUM 命令选项
 */
typedef struct VacuumOptions {
    bool    full;           /* VACUUM FULL - 重建表 */
    bool    analyze;        /* 同时更新统计信息 */
    bool    verbose;        /* 详细输出 */
    bool    freeze;         /* 冻结事务 ID */
    bool    disable_page_skipping;  /* 禁用页面跳过优化 */
    int     index_cleanup;  /* 索引清理：0=auto, 1=on, 2=off */
    int     truncate;       /* 截断：0=auto, 1=on, 2=off */
    int     parallel_workers; /* 并行工作线程数 */
} VacuumOptions;

/**
 * @brief ANALYZE 命令选项
 */
typedef struct AnalyzeOptions {
    bool    verbose;        /* 详细输出 */
} AnalyzeOptions;

/* ─────────────────────────────────────────────────────────────────
 * VACUUM 统计
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief VACUUM 执行统计
 */
typedef struct VacuumStats {
    int64_t num_pages;          /* 扫描的页面数 */
    int64_t num_tuples;         /* 扫描的元组数 */
    int64_t num_dead_tuples;    /* 死元组数 */
    int64_t pages_removed;      /* 移除的页面数 */
    int64_t tuples_removed;     /* 移除的元组数 */
    int64_t pages_frozen;       /* 冻结的页面数 */
    int64_t index_pages_removed; /* 索引移除的页面数 */
    double  execution_time_ms;  /* 执行时间（毫秒） */
} VacuumStats;

/**
 * @brief ANALYZE 执行统计
 */
typedef struct AnalyzeStats {
    int64_t num_pages;          /* 采样的页面数 */
    int64_t num_tuples;         /* 采样的元组数 */
    int64_t num_columns;        /* 分析的列数 */
    double  execution_time_ms;  /* 执行时间（毫秒） */
} AnalyzeStats;

/* ─────────────────────────────────────────────────────────────────
 * VACUUM 上下文
 * ───────────────────────────────────────────────────────────────── */

/** VACUUM 执行上下文 */
typedef struct VacuumContext_s VacuumContext;

/* ─────────────────────────────────────────────────────────────────
 * VACUUM API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取默认 VACUUM 选项
 */
VacuumOptions vacuum_default_options(void);

/**
 * @brief 创建 VACUUM 上下文
 * @param options VACUUM 选项
 * @return 上下文句柄，NULL 表示失败
 */
VacuumContext *vacuum_create(VacuumOptions *options);

/**
 * @brief 销毁 VACUUM 上下文
 * @param ctx 上下文句柄
 */
void vacuum_destroy(VacuumContext *ctx);

/**
 * @brief 执行 VACUUM
 * @param ctx 上下文
 * @param table_name 表名（NULL 表示所有表）
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int vacuum_execute(VacuumContext *ctx, const char *table_name, VacuumStats *stats);

/**
 * @brief 执行 VACUUM FULL
 * @param ctx 上下文
 * @param table_name 表名
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int vacuum_full_execute(VacuumContext *ctx, const char *table_name, VacuumStats *stats);

/**
 * @brief 获取 VACUUM 错误信息
 * @param ctx 上下文
 * @return 错误消息
 */
const char *vacuum_get_error(VacuumContext *ctx);

/* ─────────────────────────────────────────────────────────────────
 * ANALYZE API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取默认 ANALYZE 选项
 */
AnalyzeOptions analyze_default_options(void);

/**
 * @brief 执行 ANALYZE
 * @param table_name 表名（NULL 表示所有表）
 * @param column_names 列名数组（NULL 表示所有列）
 * @param num_columns 列数
 * @param[out] stats 执行统计
 * @return 0 成功，非 0 失败
 */
int analyze_execute(const char *table_name, const char **column_names,
                    int num_columns, AnalyzeStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_TOOLS_VACUUM_H */
