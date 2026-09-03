/**
 * @file sql_compat.h
 * @brief SQL 方言兼容层
 *
 * 提供多 SQL 方言之间的查询翻译与兼容功能，
 * 支持 PostgreSQL、MySQL、DuckDB、ClickHouse 等方言。
 */
#ifndef DB_SQL_COMPAT_H
#define DB_SQL_COMPAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * SQL 方言类型
 * ============================================================ */

typedef enum {
    SQL_DIALECT_POSTGRESQL = 0,  /**< PostgreSQL 方言 */
    SQL_DIALECT_MYSQL,           /**< MySQL 方言 */
    SQL_DIALECT_DUCKDB,          /**< DuckDB 方言 */
    SQL_DIALECT_CLICKHOUSE,      /**< ClickHouse 方言 */
    SQL_DIALECT_SQLITE,          /**< SQLite 方言 */
    SQL_DIALECT_INTERNAL,        /**< 内部表示（不对外） */
    SQL_DIALECT_COUNT,           /**< 方言总数 */
} sql_dialect_t;

/* ============================================================
 * 兼容性标志
 * ============================================================ */

/** 方言能力标志（位掩码） */
#define SQL_DIALECT_FLAG_CROSS_JOIN          (1 << 0)  /**< 支持 CROSS JOIN */
#define SQL_DIALECT_FLAG_FULL_OUTER_JOIN     (1 << 1)  /**< 支持 FULL OUTER JOIN */
#define SQL_DIALECT_FLAG_LATERAL_JOIN        (1 << 2)  /**< 支持 LATERAL JOIN */
#define SQL_DIALECT_FLAG_ARRAY_TYPE          (1 << 3)  /**< 支持数组类型 */
#define SQL_DIALECT_FLAG_JSON_TYPE           (1 << 4)  /**< 支持 JSON 类型 */
#define SQL_DIALECT_FLAG_WINDOW_FUNCTION     (1 << 5)  /**< 支持窗口函数 */
#define SQL_DIALECT_FLAG_CTE                 (1 << 6)  /**< 支持 WITH 子句（CTE） */
#define SQL_DIALECT_FLAG_RECURSIVE_CTE       (1 << 7)  /**< 支持递归 CTE */
#define SQL_DIALECT_FLAG_UPSERT              (1 << 8)  /**< 支持 INSERT ... ON CONFLICT */
#define SQL_DIALECT_FLAG_BOOL_TYPE           (1 << 9)  /**< 支持 BOOL 类型 */
#define SQL_DIALECT_FLAG_ILIKE               (1 << 10) /**< 支持 ILIKE */
#define SQL_DIALECT_FLAG_REGEXP               (1 << 11) /**< 支持正则表达式 */
#define SQL_DIALECT_FLAG_LIMIT_OFFSET        (1 << 12) /**< 支持 LIMIT ... OFFSET */
#define SQL_DIALECT_FLAG_TOP_N               (1 << 13) /**< 支持 TOP N 语法 */

/* ============================================================
 * 类型映射
 * ============================================================ */

/** 类型映射条目 */
typedef struct {
    const char *source_type;   /**< 源方言类型名 */
    const char *target_type;   /**< 目标方言类型名 */
    int         source_oid;    /**< 源类型 OID（0 表示不使用） */
    int         target_oid;    /**< 目标类型 OID */
    sql_dialect_t source_dialect; /**< 源方言 */
    sql_dialect_t target_dialect; /**< 目标方言 */
} sql_type_mapping_t;

/* ============================================================
 * 兼容层配置
 * ============================================================ */

typedef struct sql_compat_config_s {
    sql_dialect_t dialect;              /**< 默认方言 */
    bool          allow_cross_join;     /**< 允许交叉连接 */
    bool          standard_conforming_strings; /**< 标准符合字符串 */
    int           client_encoding;      /**< 客户端编码 */
    bool          case_sensitive;       /**< 标识符大小写敏感 */
    bool          quoted_identifiers;   /**< 引用标识符 */
    bool          supports_window_func; /**< 支持窗口函数 */
    bool          supports_recursive_cte; /**< 支持递归 CTE */
    bool          supports_lateral_join; /**< 支持 LATERAL JOIN */
} sql_compat_config_t;

/* ============================================================
 * 函数名映射
 * ============================================================ */

/** 函数映射条目 */
typedef struct {
    const char *source_func;    /**< 源函数名 */
    const char *target_func;    /**< 目标函数名 */
    sql_dialect_t source_dialect; /**< 源方言 */
    sql_dialect_t target_dialect; /**< 目标方言 */
} sql_func_mapping_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief 获取指定方言的默认配置
 * @param dialect 方言类型
 * @return 默认配置
 */
sql_compat_config_t sql_compat_config_default(sql_dialect_t dialect);

/**
 * @brief 获取方言能力标志
 * @param dialect 方言类型
 * @return 能力标志位掩码
 */
int sql_dialect_flags(sql_dialect_t dialect);

/**
 * @brief 检查方言是否支持指定能力
 * @param dialect 方言类型
 * @param flag    能力标志
 * @return true 支持，false 不支持
 */
bool sql_dialect_supports(sql_dialect_t dialect, uint32_t flag);

/**
 * @brief 查询翻译（核心函数）
 * @param query 源 SQL 查询
 * @param from  源方言
 * @param to    目标方言
 * @return 翻译后的查询字符串（调用者需 free），失败返回 NULL
 */
char* sql_translate_query(const char *query, sql_dialect_t from, sql_dialect_t to);

/**
 * @brief 获取方言名称
 * @param dialect 方言类型
 * @return 方言名称字符串
 */
const char* sql_dialect_name(sql_dialect_t dialect);

/**
 * @brief 根据名称解析方言类型
 * @param name 方言名称
 * @return 方言类型，-1 表示未知
 */
sql_dialect_t sql_dialect_parse(const char *name);

/**
 * @brief 获取类型映射
 * @param source_type 源类型名
 * @param from 源方言
 * @param to   目标方言
 * @return 目标类型名，未找到返回 NULL
 */
const char* sql_type_map(const char *source_type, sql_dialect_t from, sql_dialect_t to);

/**
 * @brief 获取函数映射
 * @param source_func 源函数名
 * @param from 源方言
 * @param to   目标方言
 * @return 目标函数名，未找到返回 NULL
 */
const char* sql_func_map(const char *source_func, sql_dialect_t from, sql_dialect_t to);

/**
 * @brief 字符串字面量转义
 * @param input 输入字符串
 * @param from  源方言
 * @param to    目标方言
 * @return 转义后的字符串（调用者需 free），失败返回 NULL
 */
char* sql_escape_string(const char *input, sql_dialect_t from, sql_dialect_t to);

/**
 * @brief 标识符引用
 * @param identifier 标识符
 * @param dialect    方言
 * @return 引用后的标识符（调用者需 free），失败返回 NULL
 */
char* sql_quote_identifier(const char *identifier, sql_dialect_t dialect);

/**
 * @brief 解析 LIMIT/OFFSET 子句
 * @param query SQL 查询
 * @param limit 输出：限制行数（-1 表示无限制）
 * @param offset 输出：偏移量（0 表示无偏移）
 * @return 0 成功解析，-1 无 LIMIT 子句
 */
int sql_parse_limit_offset(const char *query, int64_t *limit, int64_t *offset);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_COMPAT_H */
