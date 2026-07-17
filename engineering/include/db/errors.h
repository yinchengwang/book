/**
 * @file errors.h
 * @brief 数据库错误码系统头文件
 *
 * @deprecated 请使用 db_err.h 代替
 * 此文件保留用于向后兼容，不再推荐新代码使用
 *
 * 注意：
 * - ERRCODE_* 宏已保留为 DB_ERR_* 别名
 * - db_error_t 等类型定义保持不变
 * - 新代码应直接 include "db/db_err.h"
 */
#ifndef DB_ERRORS_H
#define DB_ERRORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 成功与警告 (00*, 01*) */
#define ERRCODE_SUCCESSFUL_COMPLETION          "00000"
#define ERRCODE_WARNING                        "01000"
#define ERRCODE_NULL_VALUE_ELIMINATED_SET       "01003"
#define ERRCODE_STRING_DATA_RIGHT_TRUNCATION    "01004"

/* 连接异常 (08*) */
#define ERRCODE_CONNECTION_EXCEPTION            "08000"
#define ERRCODE_CONNECTION_DOES_NOT_EXIST       "08003"
#define ERRCODE_CONNECTION_FAILURE              "08006"

/* 功能不支持 (0A*) */
#define ERRCODE_FEATURE_NOT_SUPPORTED           "0A000"

/* 找不到标识符 (42*) */
#define ERRCODE_SYNTAX_ERROR_OR_ACCESS_RULE_VIOLATION "42000"
#define ERRCODE_SYNTAX_ERROR                    "42601"
#define ERRCODE_INVALID_NAME                    "42602"
#define ERRCODE_NAME_TOO_LONG                   "42622"
#define ERRCODE_UNDEFINED_COLUMN                "42703"
#define ERRCODE_UNDEFINED_OBJECT                "42704"
#define ERRCODE_UNDEFINED_TABLE                 "42P01"
#define ERRCODE_DUPLICATE_ALIAS                "42712"
#define ERRCODE_DUPLICATE_FUNCTION             "42723"
#define ERRCODE_DUPLICATE_SCHEMA               "42P06"
#define ERRCODE_DUPLICATE_TABLE                "42P07"

/* 数据异常 (22*) */
#define ERRCODE_DATA_EXCEPTION                  "22000"
#define ERRCODE_NULL_VALUE_NO_INDICATOR         "22002"
#define ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE      "22003"
#define ERRCODE_NULL_VALUE_NOT_ALLOWED          "22004"
#define ERRCODE_INVALID_DATETIME_FORMAT         "22007"
#define ERRCODE_DATETIME_FIELD_OVERFLOW         "22008"
#define ERRCODE_INVALID_TIME_ZONE               "22009"
#define ERRCODE_DIVISION_BY_ZERO                "22012"
#define ERRCODE_INVALID_CHARACTER_VALUE         "22018"
#define ERRCODE_INVALID_REGULAR_EXPRESSION      "2201B"
#define ERRCODE_CHARACTER_NOT_IN_REPERTOIRE     "22021"
#define ERRCODE_INVALID_PARAMETER_VALUE         "22023"

/* 完整性约束违规 (23*) */
#define ERRCODE_INTEGRITY_CONSTRAINT_VIOLATION  "23000"
#define ERRCODE_NOT_NULL_VIOLATION              "23502"
#define ERRCODE_FOREIGN_KEY_VIOLATION           "23503"
#define ERRCODE_UNIQUE_VIOLATION                "23505"
#define ERRCODE_CHECK_VIOLATION                 "23514"

/* 事务状态 (25*, 40*) */
#define ERRCODE_INVALID_TRANSACTION_STATE       "25000"
#define ERRCODE_NO_ACTIVE_SQL_TRANSACTION       "25P01"
#define ERRCODE_IN_FAILED_SQL_TRANSACTION       "25P02"
#define ERRCODE_IDLE_IN_TRANSACTION_TIMEOUT     "25P03"
#define ERRCODE_TRANSACTION_ROLLBACK            "40000"
#define ERRCODE_SERIALIZATION_FAILURE           "40001"
#define ERRCODE_DEADLOCK_DETECTED               "40P01"

/* 授权 (28*) */
#define ERRCODE_INVALID_AUTHORIZATION           "28000"
#define ERRCODE_INSUFFICIENT_PRIVILEGE          "42501"
#define ERRCODE_ROLE_NOT_GRANTED                "42719"

/* 资源不足 (53*, 54*) */
#define ERRCODE_OUT_OF_MEMORY                   "53000"
#define ERRCODE_DISK_FULL                       "53100"
#define ERRCODE_OUT_OF_DISK                     "53200"
#define ERRCODE_TOO_MANY_CONNECTIONS           "53300"
#define ERRCODE_PROGRAM_LIMIT_EXCEEDED          "54000"
#define ERRCODE_STATEMENT_TOO_COMPLEX          "54001"
#define ERRCODE_TOO_MANY_COLUMNS               "54004"
#define ERRCODE_TOO_MANY_OPEN_CURSORS           "54011"
#define ERRCODE_TOO_MANY_INDEXES                "54012"
#define ERRCODE_TOO_MANY_TABLES                "54023"
#define ERRCODE_LOCK_NOT_AVAILABLE              "55P03"

/* 系统错误 (58*, XX*) */
#define ERRCODE_IO_ERROR                        "58030"
#define ERRCODE_INTERNAL_ERROR                  "XX000"
#define ERRCODE_FILE_SYSTEM_ERROR               "XX001"
#define ERRCODE_DATA_CORRUPTED_ERROR            "XX002"
#define ERRCODE_INDEX_CORRUPTED_ERROR           "XX003"

/* ========================================================================
 * 图数据库扩展错误码 (G*)
 * ======================================================================== */
#define ERRCODE_VERTEX_NOT_FOUND               "G0001"
#define ERRCODE_EDGE_NOT_FOUND                 "G0002"
#define ERRCODE_LABEL_NOT_FOUND                "G0003"
#define ERRCODE_CYCLE_DETECTED                 "G0004"
#define ERRCODE_INVALID_GRAPH_SCHEMA           "G0005"
#define ERRCODE_GRAPH_CONSTRAINT_VIOLATION     "G0006"

/* ========================================================================
 * 向量数据库扩展错误码 (V*)
 * ======================================================================== */
#define ERRCODE_VECTOR_DIMENSION_MISMATCH       "V0001"
#define ERRCODE_VECTOR_NOT_NORMALIZED          "V0002"
#define ERRCODE_INVALID_METRIC_TYPE            "V0003"
#define ERRCODE_INDEX_BUILD_FAILED             "V0004"
#define ERRCODE_INVALID_VECTOR_VALUE           "V0005"

/* ========================================================================
 * 时序数据库扩展错误码 (T*)
 * ======================================================================== */
#define ERRCODE_TIMESTAMP_OVERFLOW             "T0001"
#define ERRCODE_INVALID_TIME_WINDOW            "T0002"
#define ERRCODE_TIME_SERIES_NOT_FOUND          "T0003"
#define ERRCODE_DOWNSAMPLING_ERROR             "T0004"

/* ========================================================================
 * 文档数据库扩展错误码 (D*)
 * ======================================================================== */
#define ERRCODE_INVALID_JSON                   "D0001"
#define ERRCODE_JSON_PATH_ERROR                "D0002"
#define ERRCODE_DOCUMENT_TOO_LARGE             "D0003"
#define ERRCODE_SCHEMA_MISMATCH                "D0004"

/* ========================================================================
 * 空间数据库扩展错误码 (S*)
 * ======================================================================== */
#define ERRCODE_INVALID_GEOMETRY               "S0001"
#define ERRCODE_GEOMETRY_SRID_MISMATCH         "S0002"
#define ERRCODE_INVALID_GEOMETRY_TYPE          "S0003"
#define ERRCODE_GEOMETRY_NOT_FOUND             "S0004"

/* ========================================================================
 * 错误级别定义
 * ======================================================================== */

/**
 * @brief 错误严重级别枚举
 */
typedef enum {
    DB_ERROR_LEVEL_SUCCESS = 0,   /**< 成功 */
    DB_ERROR_LEVEL_WARNING = 1,    /**< 警告 */
    DB_ERROR_LEVEL_ERROR = 2,      /**< 错误 */
    DB_ERROR_LEVEL_FATAL = 3,      /**< 致命错误 */
    DB_ERROR_LEVEL_PANIC = 4,     /**< 紧急错误 */
} db_error_level_t;

/* ========================================================================
 * 数据结构定义
 * ======================================================================== */

/**
 * @brief 错误码结构
 */
typedef struct db_errcode_s {
    char code[6];  /**< 5字符错误码 + '\0' */
} db_errcode_t;

/**
 * @brief 错误上下文结构
 */
typedef struct db_error_s {
    char            sqlstate[6];      /**< SQLSTATE 错误码 */
    db_error_level_t level;           /**< 错误级别 */
    char           *message;          /**< 错误消息 */
    char           *detail;           /**< 详细错误信息 */
    char           *hint;             /**< 提示信息 */
    char           *schema_name;      /**< Schema 名称 */
    char           *table_name;       /**< 表名称 */
    char           *column_name;      /**< 列名称 */
    char           *constraint_name; /**< 约束名称 */
    const char     *file;             /**< 错误发生文件 */
    int             line;             /**< 错误发生行号 */
    const char     *func;             /**< 错误发生函数 */
} db_error_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/* --- 生命周期 --- */

/**
 * @brief 初始化错误系统
 * @return 成功返回 0，失败返回 -1
 */
int db_error_init(void);

/**
 * @brief 关闭错误系统，释放资源
 */
void db_error_shutdown(void);

/* --- 错误创建 --- */

/**
 * @brief 创建错误对象
 * @param sqlstate SQLSTATE 错误码
 * @param level 错误级别
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @return 错误对象指针，失败返回 NULL
 */
db_error_t *db_error_create(const char *sqlstate, db_error_level_t level,
                            const char *fmt, ...);

/**
 * @brief 创建带详细信息的错误对象
 * @param sqlstate SQLSTATE 错误码
 * @param level 错误级别
 * @param message 错误消息
 * @param detail 详细信息
 * @param hint 提示信息
 * @return 错误对象指针，失败返回 NULL
 */
db_error_t *db_error_create_detailed(const char *sqlstate, db_error_level_t level,
                                     const char *message, const char *detail,
                                     const char *hint, ...);

/**
 * @brief 释放错误对象
 * @param err 错误对象指针
 */
void db_error_free(db_error_t *err);

/**
 * @brief 复制错误对象
 * @param err 原始错误对象
 * @return 错误对象副本，失败返回 NULL
 */
db_error_t *db_error_copy(const db_error_t *err);

/* --- 线程本地错误存储 --- */

/**
 * @brief 获取当前线程的错误对象
 * @return 当前线程的错误对象，如果没有则返回 NULL
 */
db_error_t *db_error_get(void);

/**
 * @brief 设置当前线程的错误对象
 * @param err 错误对象（会被复制）
 */
void db_error_set(db_error_t *err);

/**
 * @brief 清除当前线程的错误
 */
void db_error_clear(void);

/**
 * @brief 检查当前线程是否有错误
 * @return 有错误返回 true，否则返回 false
 */
bool db_error_has_error(void);

/* --- 错误抛出 --- */

/**
 * @brief 抛出错误并存储到线程本地
 * @param sqlstate SQLSTATE 错误码
 * @param level 错误级别
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @return 错误对象指针
 */
db_error_t *db_error_raise(const char *sqlstate, db_error_level_t level,
                           const char *fmt, ...);

/* --- 错误匹配 --- */

/**
 * @brief 检查错误是否匹配指定 SQLSTATE
 * @param err 错误对象
 * @param sqlstate 要匹配的 SQLSTATE
 * @return 匹配返回 true，否则返回 false
 */
bool db_error_matches(const db_error_t *err, const char *sqlstate);

/* --- 辅助函数 --- */

/**
 * @brief 获取 SQLSTATE 对应的默认消息
 * @param sqlstate SQLSTATE 错误码
 * @return 对应的消息字符串
 */
const char *db_errcode_to_message(const char *sqlstate);

/**
 * @brief 格式化错误消息
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 * @param err 错误对象
 */
void db_error_format_message(char *buf, size_t size, const db_error_t *err);

/**
 * @brief 获取错误级别的名称
 * @param level 错误级别
 * @return 级别名称字符串
 */
const char *db_error_level_name(db_error_level_t level);

/* ========================================================================
 * 便捷宏定义
 * ======================================================================== */

/**
 * @brief 抛出错误并设置到线程本地
 */
#define DB_ERROR(sqlstate, fmt, ...) \
    db_error_raise(sqlstate, DB_ERROR_LEVEL_ERROR, fmt, ##__VA_ARGS__)

/**
 * @brief 抛出警告并设置到线程本地
 */
#define DB_WARNING(sqlstate, fmt, ...) \
    db_error_raise(sqlstate, DB_ERROR_LEVEL_WARNING, fmt, ##__VA_ARGS__)

/**
 * @brief 抛出致命错误并设置到线程本地
 */
#define DB_FATAL(sqlstate, fmt, ...) \
    db_error_raise(sqlstate, DB_ERROR_LEVEL_FATAL, fmt, ##__VA_ARGS__)

/* --- 常见错误便捷宏 --- */

/** 表不存在 */
#define DB_ERR_TABLE_NOT_FOUND(table) \
    db_error_raise(ERRCODE_UNDEFINED_TABLE, DB_ERROR_LEVEL_ERROR, \
                   "表 \"%s\" 不存在", table)

/** 列不存在 */
#define DB_ERR_COLUMN_NOT_FOUND(col) \
    db_error_raise(ERRCODE_UNDEFINED_COLUMN, DB_ERROR_LEVEL_ERROR, \
                   "列 \"%s\" 不存在", col)

/** 唯一约束违规 */
#define DB_ERR_UNIQUE_VIOLATION(table, key) \
    db_error_raise(ERRCODE_UNIQUE_VIOLATION, DB_ERROR_LEVEL_ERROR, \
                   "重复的键值违反唯一约束 \"%s\"", table)

/** 非空约束违规 */
#define DB_ERR_NOT_NULL_VIOLATION(col) \
    db_error_raise(ERRCODE_NOT_NULL_VIOLATION, DB_ERROR_LEVEL_ERROR, \
                   "列 \"%s\" 不能为空", col)

/** 外键约束违规 */
#define DB_ERR_FOREIGN_KEY_VIOLATION(child, parent) \
    db_error_raise(ERRCODE_FOREIGN_KEY_VIOLATION, DB_ERROR_LEVEL_ERROR, \
                   "外键约束违规: 表 \"%s\" 引用表 \"%s\"", child, parent)

/** 检测到死锁 */
#define DB_ERR_DEADLOCK() \
    db_error_raise(ERRCODE_DEADLOCK_DETECTED, DB_ERROR_LEVEL_ERROR, \
                   "检测到事务死锁")

/** 内存耗尽 */
#define DB_ERR_OUT_OF_MEMORY() \
    db_error_raise(ERRCODE_OUT_OF_MEMORY, DB_ERROR_LEVEL_FATAL, \
                   "内存耗尽")

/** 磁盘空间不足 */
#define DB_ERR_DISK_FULL(path) \
    db_error_raise(ERRCODE_DISK_FULL, DB_ERROR_LEVEL_ERROR, \
                   "磁盘空间不足: %s", path)

#ifdef __cplusplus
}
#endif

#endif /* DB_ERRORS_H */
