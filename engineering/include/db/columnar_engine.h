/**
 * @file columnar_engine.h
 * @brief 列式存储引擎接口
 *
 * 提供列式存储、向量化聚合、压缩等能力。
 */
#ifndef DB_COLUMNAR_ENGINE_H
#define DB_COLUMNAR_ENGINE_H

#include "db/storage_engine.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 列存类型
 * ======================================================================== */

/** 压缩类型 */
typedef enum ColumnarCompressionType_e {
    COL_COMPRESS_NONE = 0,
    COL_COMPRESS_RLE,
    COL_COMPRESS_DICTIONARY,
    COL_COMPRESS_ZSTD,
} ColumnarCompressionType;

/** 列定义 */
typedef struct columnar_column_s {
    const char *name;           /**< 列名 */
    int32_t type_oid;           /**< 类型 OID */
    void *data;                 /**< 列数据 */
    size_t size;                /**< 数据大小 */
    size_t count;               /**< 行数 */
} columnar_column_t;

/** 列存表 */
typedef struct columnar_table_s {
    char name[256];             /**< 表名 */
    columnar_column_t *columns; /**< 列数组 */
    int32_t num_columns;        /**< 列数 */
    int64_t row_count;          /**< 行数 */
    void *internal;             /**< 内部实现 */
} columnar_table_t;

/* ========================================================================
 * 引擎操作接口
 * ======================================================================== */

/**
 * @brief 列式存储引擎操作表
 */
typedef struct columnar_ops_s {
    const char *name;                    /**< 引擎名称 */
    DataModel model;                     /**< MODEL_COLUMNAR */

    /* 生命周期 */
    int (*init)(const char *data_dir);
    int (*shutdown)(void);

    /* 表操作 */
    int (*table_create)(const char *name, const char **columns, int32_t num_columns);
    void *(*table_open)(const char *name);
    int (*table_close)(void *table);
    int (*table_drop)(const char *name);

    /* 列操作 */
    int (*column_append)(void *table, const char *col_name, const void *data, size_t len);
    int (*column_append_batch)(void *table, const char *col_name, const void **data, size_t *lens, size_t count);

    /* 向量化聚合 */
    int64_t (*agg_count)(void *table);
    int (*agg_sum_int64)(void *table, const char *col_name, int64_t *result);
    int (*agg_avg_double)(void *table, const char *col_name, double *result);

    /* 压缩 */
    int (*compress)(void *table, ColumnarCompressionType type);
    int (*get_stats)(const char *name, storage_stats_t *stats);
} columnar_ops_t;

/**
 * @brief 获取列式存储引擎操作表
 */
const storage_ops_t *columnar_engine_get_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_COLUMNAR_ENGINE_H */
