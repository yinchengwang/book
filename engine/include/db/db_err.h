/*
 * db_err.h - 数据库错误码
 *
 * 支持两套错误码格式：
 * 1. SQLSTATE 格式（5字符，PostgreSQL 兼容，定义在 errors.h）
 * 2. DB_* 格式（易读格式，如 DB_ERROR_TABLE_NOT_FOUND，定义在此文件）
 *
 * SQLSTATE 标准错误码统一在 db/errors.h 中定义，本文件仅保留 DB_* 易读格式。
 * 新代码优先使用 db/errors.h 中的 ERRCODE_* 宏。
 *
 * 级别: WARN / ERROR / FATAL
 */
#ifndef DB_DB_ERR_H
#define DB_DB_ERR_H

#include <stdbool.h>
#include <stddef.h>
#include "common/base_err.h"
#include "db/errors.h"  /* SQLSTATE 标准错误码 */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 错误级别（兼容旧版）
// ============================================================

typedef enum {
    DB_ERR_LEVEL_SUCCESS = 0,
    DB_ERR_LEVEL_WARNING = 1,
    DB_ERR_LEVEL_ERROR   = 2,
    DB_ERR_LEVEL_FATAL   = 3,
} db_err_level_t;

/* 注意：SQLSTATE 标准错误码（如 DB_ERR_SUCCESSFUL_COMPLETION）已移至 db/errors.h
 * 此处仅保留 DB_* 格式的易读错误码。
 */

// ============================================================
// DB_* 格式错误码（易读格式）
// ============================================================

// --- 通用错误 ---
#define DB_ERROR_OUT_OF_MEMORY               "DB_ERROR_OUT_OF_MEMORY"
#define DB_ERROR_IO_FAILED                   "DB_ERROR_IO_FAILED"
#define DB_ERROR_INTERNAL                    "DB_ERROR_INTERNAL"
#define DB_ERROR_FEATURE_NOT_SUPPORTED       "DB_ERROR_FEATURE_NOT_SUPPORTED"

// --- Catalog 错误 ---
#define DB_ERROR_TABLE_NOT_FOUND             "DB_ERROR_TABLE_NOT_FOUND"
#define DB_ERROR_COLUMN_NOT_FOUND            "DB_ERROR_COLUMN_NOT_FOUND"
#define DB_ERROR_INDEX_NOT_FOUND             "DB_ERROR_INDEX_NOT_FOUND"
#define DB_ERROR_DATABASE_NOT_FOUND          "DB_ERROR_DATABASE_NOT_FOUND"
#define DB_ERROR_SCHEMA_NOT_FOUND            "DB_ERROR_SCHEMA_NOT_FOUND"
#define DB_ERROR_TABLE_EXISTS                "DB_ERROR_TABLE_EXISTS"
#define DB_ERROR_COLUMN_EXISTS               "DB_ERROR_COLUMN_EXISTS"
#define DB_ERROR_DUPLICATE_TABLE             "DB_ERROR_DUPLICATE_TABLE"
#define DB_ERROR_DUPLICATE_INDEX             "DB_ERROR_DUPLICATE_INDEX"

// --- 约束错误 ---
#define DB_ERROR_NOT_NULL_VIOLATION          "DB_ERROR_NOT_NULL_VIOLATION"
#define DB_ERROR_UNIQUE_VIOLATION            "DB_ERROR_UNIQUE_VIOLATION"
#define DB_ERROR_FOREIGN_KEY_VIOLATION       "DB_ERROR_FOREIGN_KEY_VIOLATION"
#define DB_ERROR_CHECK_VIOLATION             "DB_ERROR_CHECK_VIOLATION"

// --- 事务错误 ---
#define DB_ERROR_DEADLOCK_DETECTED           "DB_ERROR_DEADLOCK_DETECTED"
#define DB_ERROR_SERIALIZATION_FAILURE       "DB_ERROR_SERIALIZATION_FAILURE"
#define DB_ERROR_INVALID_TRANSACTION_STATE   "DB_ERROR_INVALID_TRANSACTION_STATE"
#define DB_ERROR_NO_ACTIVE_TRANSACTION       "DB_ERROR_NO_ACTIVE_TRANSACTION"
#define DB_ERROR_TRANSACTION_ROLLBACK        "DB_ERROR_TRANSACTION_ROLLBACK"

// --- 锁错误 ---
#define DB_ERROR_LOCK_NOT_AVAILABLE          "DB_ERROR_LOCK_NOT_AVAILABLE"
#define DB_ERROR_DEADLOCK                    "DB_ERROR_DEADLOCK"

// --- Buffer Pool 错误 ---
#define DB_ERROR_BUFFER_NOT_FOUND            "DB_ERROR_BUFFER_NOT_FOUND"
#define DB_ERROR_BUFFER_PINNED               "DB_ERROR_BUFFER_PINNED"
#define DB_ERROR_BUFFER_FULL                 "DB_ERROR_BUFFER_FULL"

// --- BTree 索引错误 ---
#define DB_ERROR_INDEX_CORRUPTED             "DB_ERROR_INDEX_CORRUPTED"
#define DB_ERROR_INDEX_BUILD_FAILED          "DB_ERROR_INDEX_BUILD_FAILED"
#define DB_ERROR_DUPLICATE_KEY               "DB_ERROR_DUPLICATE_KEY"

// --- SQL 执行错误 ---
#define DB_ERROR_SYNTAX_ERROR                "DB_ERROR_SYNTAX_ERROR"
#define DB_ERROR_INVALID_NAME                "DB_ERROR_INVALID_NAME"
#define DB_ERROR_INVALID_PARAMETER           "DB_ERROR_INVALID_PARAMETER"
#define DB_ERROR_DIVISION_BY_ZERO            "DB_ERROR_DIVISION_BY_ZERO"
#define DB_ERROR_NUMERIC_OVERFLOW            "DB_ERROR_NUMERIC_OVERFLOW"

// --- 权限错误 ---
#define DB_ERROR_INSUFFICIENT_PRIVILEGE      "DB_ERROR_INSUFFICIENT_PRIVILEGE"
#define DB_ERROR_UNAUTHORIZED                "DB_ERROR_UNAUTHORIZED"

// ============================================================
// 多模态扩展错误码（DB_* 格式）
// ============================================================

// --- 图数据库扩展 ---
#define DB_ERROR_VERTEX_NOT_FOUND            "DB_ERROR_VERTEX_NOT_FOUND"
#define DB_ERROR_EDGE_NOT_FOUND              "DB_ERROR_EDGE_NOT_FOUND"
#define DB_ERROR_LABEL_NOT_FOUND             "DB_ERROR_LABEL_NOT_FOUND"
#define DB_ERROR_CYCLE_DETECTED              "DB_ERROR_CYCLE_DETECTED"
#define DB_ERROR_INVALID_GRAPH_SCHEMA        "DB_ERROR_INVALID_GRAPH_SCHEMA"
#define DB_ERROR_GRAPH_CONSTRAINT_VIOLATION  "DB_ERROR_GRAPH_CONSTRAINT_VIOLATION"

// --- 向量数据库扩展 ---
#define DB_ERROR_VECTOR_DIM_MISMATCH         "DB_ERROR_VECTOR_DIM_MISMATCH"
#define DB_ERROR_VECTOR_NOT_NORMALIZED       "DB_ERROR_VECTOR_NOT_NORMALIZED"
#define DB_ERROR_INVALID_METRIC_TYPE         "DB_ERROR_INVALID_METRIC_TYPE"
#define DB_ERROR_INDEX_BUILD_FAILED_VEC      "DB_ERROR_INDEX_BUILD_FAILED_VEC"
#define DB_ERROR_INVALID_VECTOR_VALUE        "DB_ERROR_INVALID_VECTOR_VALUE"

// --- 时序数据库扩展 ---
#define DB_ERROR_TIMESTAMP_OVERFLOW          "DB_ERROR_TIMESTAMP_OVERFLOW"
#define DB_ERROR_INVALID_TIME_WINDOW         "DB_ERROR_INVALID_TIME_WINDOW"
#define DB_ERROR_TIME_SERIES_NOT_FOUND       "DB_ERROR_TIME_SERIES_NOT_FOUND"
#define DB_ERROR_DOWNSAMPLING_ERROR          "DB_ERROR_DOWNSAMPLING_ERROR"

// --- 文档数据库扩展 ---
#define DB_ERROR_INVALID_JSON                "DB_ERROR_INVALID_JSON"
#define DB_ERROR_JSON_PATH_ERROR             "DB_ERROR_JSON_PATH_ERROR"
#define DB_ERROR_DOCUMENT_TOO_LARGE          "DB_ERROR_DOCUMENT_TOO_LARGE"
#define DB_ERROR_SCHEMA_MISMATCH             "DB_ERROR_SCHEMA_MISMATCH"

// --- 空间数据库扩展 ---
#define DB_ERROR_INVALID_GEOMETRY            "DB_ERROR_INVALID_GEOMETRY"
#define DB_ERROR_GEOMETRY_SRID_MISMATCH      "DB_ERROR_GEOMETRY_SRID_MISMATCH"
#define DB_ERROR_INVALID_GEOMETRY_TYPE       "DB_ERROR_INVALID_GEOMETRY_TYPE"
#define DB_ERROR_GEOMETRY_NOT_FOUND          "DB_ERROR_GEOMETRY_NOT_FOUND"

// ============================================================
// 警告级别错误码
// ============================================================

#define DB_WARN_NULL_VALUE_ELIMINATED        "DB_WARN_NULL_VALUE_ELIMINATED"
#define DB_WARN_STRING_RIGHT_TRUNCATED       "DB_WARN_STRING_RIGHT_TRUNCATED"
#define DB_WARN_FEATURE_DEPRECATED           "DB_WARN_FEATURE_DEPRECATED"
#define DB_WARN_SLOW_QUERY                   "DB_WARN_SLOW_QUERY"
#define DB_WARN_IDLE_IN_TRANSACTION          "DB_WARN_IDLE_IN_TRANSACTION"

#ifdef __cplusplus
}
#endif

#endif // DB_DB_ERR_H
